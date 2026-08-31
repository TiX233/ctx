#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# Copyright (c) 2026, realTiX
# SPDX-License-Identifier: Apache-2.0

"""
C 无栈协程 _async 源到源翻译器
2026年 6月27日: V0.1
2026年 6月30日: V0.3, 修改协程收尾操作
2026年 7月 6日: V0.4, 补充 _await_static 的设置子协程为 NULL 操作
2026年 8月30日: V0.5, 修复循环体对变量作用域误判的问题; 修复异步等待结果时遗留变量类型的问题，并且优化相关变量作用域判断

2026年 8月30日: V0.6, 转用 V2 版本翻译模板作为翻译规则
2026年 8月31日: V0.7, 变更变量作用域判断方法，使用起止行号与异步关键字行号决定是否提升变量生命周期
2026年 8月31日: V0.8, 增加 _var_frame/_var_local 关键字的识别，支持手动声明生命周期；增加局部变量翻译结果检查输出能力

用法: python coro_translater.py <input.c> [--line]
输出：
  - <input.c>.coro.h   （结构体定义）
  - <input.c>.coro     （其余翻译代码）
若无 _async 函数则不会生成任何文件。
"""

import sys
import re
import os
import json

# ================================================
# 词法分析器
# ================================================
def tokenize_c(source: str):
    i = 0
    n = len(source)
    while i < n:
        ch = source[i]
        if ch.isspace():
            j = i
            while j < n and source[j].isspace():
                j += 1
            yield ('whitespace', source[i:j], i, j)
            i = j
            continue
        if ch == '/' and i + 1 < n and source[i + 1] == '/':
            j = i + 2
            while j < n and source[j] != '\n':
                j += 1
            yield ('comment_line', source[i:j], i, j)
            i = j
            continue
        if ch == '/' and i + 1 < n and source[i + 1] == '*':
            j = i + 2
            while j < n and not (source[j] == '*' and j + 1 < n and source[j + 1] == '/'):
                j += 1
            if j < n:
                j += 2
            yield ('comment_block', source[i:j], i, j)
            i = j
            continue
        if ch == '"':
            j = i + 1
            while j < n and source[j] != '"':
                if source[j] == '\\' and j + 1 < n:
                    j += 2
                else:
                    j += 1
            if j < n:
                j += 1
            yield ('string', source[i:j], i, j)
            i = j
            continue
        if ch == "'":
            j = i + 1
            while j < n and source[j] != "'":
                if source[j] == '\\' and j + 1 < n:
                    j += 2
                else:
                    j += 1
            if j < n:
                j += 1
            yield ('char', source[i:j], i, j)
            i = j
            continue
        if re.match(r'[a-zA-Z_]', ch):
            j = i
            while j < n and re.match(r'[a-zA-Z0-9_]', source[j]):
                j += 1
            word = source[i:j]
            if word == '_async':
                yield ('keyword_async', word, i, j)
            else:
                yield ('identifier', word, i, j)
            i = j
            continue
        if ch in '(){},;*[]':
            yield ('punctuation', ch, i, i + 1)
            i += 1
            continue
        yield ('other', ch, i, i + 1)
        i += 1

# ================================================
# 基础工具
# ================================================
C_KEYWORDS = {
    'auto', 'break', 'case', 'char', 'const', 'continue', 'default', 'do',
    'double', 'else', 'enum', 'extern', 'float', 'for', 'goto', 'if',
    'inline', 'int', 'long', 'register', 'restrict', 'return', 'short',
    'signed', 'sizeof', 'static', 'struct', 'switch', 'typedef', 'union',
    'unsigned', 'void', 'volatile', 'while', '_Bool', '_Complex', '_Imaginary',
}
CONTROL_KW = {'if', 'for', 'while', 'switch', 'do'}

def skip_balanced(tokens, idx, end_idx, open_ch, close_ch):
    depth = 1
    idx += 1
    while idx <= end_idx and depth > 0:
        t = tokens[idx]
        if t[0] in ('string', 'char', 'comment_line', 'comment_block'):
            pass
        elif t[1] == open_ch:
            depth += 1
        elif t[1] == close_ch:
            depth -= 1
        idx += 1
    return idx

def find_matching_brace(tokens, open_idx, end_idx):
    return skip_balanced(tokens, open_idx, end_idx, '{', '}') - 1

def skip_whitespace(tokens, idx, end_idx):
    while idx <= end_idx and tokens[idx][0] in ('whitespace', 'comment_line', 'comment_block'):
        idx += 1
    return idx

def skip_preprocessor_line(tokens, idx, end_idx):
    if idx > end_idx or tokens[idx][1] != '#':
        return idx
    idx += 1
    while idx <= end_idx:
        t = tokens[idx]
        if t[0] == 'whitespace' and '\n' in t[1]:
            idx += 1
            break
        idx += 1
    return idx

# ================================================
# 提取 _async 函数
# ================================================
def extract_async_functions(source: str):
    tokens = list(tokenize_c(source))
    total = len(tokens)
    funcs = []
    idx = 0
    while idx < total:
        if tokens[idx][0] == 'keyword_async':
            idx += 1
            while idx < total and tokens[idx][0] in ('whitespace', 'comment_line', 'comment_block'):
                idx += 1

            header = []
            lparen = None
            while idx < total:
                t = tokens[idx]
                if t[0] == 'punctuation' and t[1] == '(':
                    lparen = idx
                    break
                if t[0] in ('comment_line', 'comment_block', 'whitespace'):
                    header.append(t)
                else:
                    header.append(t)
                idx += 1
            if lparen is None:
                continue

            func_name = None
            name_idx = -1
            for i in range(len(header)-1, -1, -1):
                if header[i][0] == 'identifier':
                    func_name = header[i][1]
                    name_idx = i
                    break
            if not func_name:
                continue

            if name_idx == 0:
                ret_type = ""
            else:
                ret_type = source[header[0][2] : header[name_idx][2]].rstrip()

            idx = lparen + 1
            param_start = tokens[idx][2]
            paren_depth = 1
            while idx < total and paren_depth > 0:
                if tokens[idx][0] == 'punctuation':
                    if tokens[idx][1] == '(': paren_depth += 1
                    elif tokens[idx][1] == ')': paren_depth -= 1
                idx += 1
            param_str = source[param_start : tokens[idx-1][2]].strip()

            idx = skip_whitespace(tokens, idx, total)
            if idx >= total or tokens[idx][1] != '{':
                continue
            lbrace = idx
            idx += 1
            brace_depth = 1
            while idx < total and brace_depth > 0:
                t = tokens[idx]
                if t[0] in ('string', 'char', 'comment_line', 'comment_block'):
                    pass
                elif t[0] == 'punctuation':
                    if t[1] == '{': brace_depth += 1
                    elif t[1] == '}': brace_depth -= 1
                idx += 1
            rbrace = idx - 1

            funcs.append({
                "name": func_name,
                "return_type": ret_type.strip(),
                "params": param_str,
                "lbrace": lbrace,
                "rbrace": rbrace,
            })
            continue
        idx += 1
    return funcs, tokens

# ================================================
# 声明解析
# ================================================
def try_parse_declaration(tokens, idx, end_idx, source):
    save = idx
    idx = skip_whitespace(tokens, idx, end_idx)
    if idx > end_idx:
        return None, save

    if tokens[idx][0] == 'identifier' and tokens[idx][1] == 'static':
        while idx <= end_idx and tokens[idx][1] != ';':
            if tokens[idx][1] == '{':
                idx = skip_balanced(tokens, idx, end_idx, '{', '}')
            else:
                idx += 1
        if idx <= end_idx and tokens[idx][1] == ';':
            idx += 1
        return None, idx

    force_frame = False
    force_local = False
    type_tokens = []
    while idx <= end_idx:
        idx = skip_whitespace(tokens, idx, end_idx)
        if idx > end_idx:
            break
        ct = tokens[idx]
        if ct[0] == 'identifier' and ct[1] in ('_var_frame', '_var_local'):
            if ct[1] == '_var_frame':
                force_frame = True
            else:
                force_local = True
            idx += 1
            continue
        if ct[0] == 'identifier' and ct[1] in ('const','volatile','restrict','inline','_Atomic'):
            type_tokens.append(ct)
            idx += 1
            continue
        if ct[1] == '*':
            # `*z`/`*ptr` in expressions are not declarations. A declaration must start with a
            # real type token or qualifier before a pointer modifier, not a bare pointer dereference.
            if not type_tokens:
                return None, save + 1
            type_tokens.append(ct)
            idx += 1
            continue
        if ct[0] == 'identifier':
            type_tokens.append(ct)
            idx += 1
            if ct[1] in ('struct','union','enum'):
                idx = skip_whitespace(tokens, idx, end_idx)
                if idx <= end_idx and tokens[idx][0] == 'identifier':
                    type_tokens.append(tokens[idx])
                    idx += 1
                    idx = skip_whitespace(tokens, idx, end_idx)
                    if idx <= end_idx and tokens[idx][1] == '{':
                        start_def = idx
                        idx = skip_balanced(tokens, idx, end_idx, '{', '}')
                        for i in range(start_def, idx):
                            type_tokens.append(tokens[i])
            continue
        break

    if not type_tokens:
        return None, save + 1

    var_name = None
    var_idx = -1
    for i in range(len(type_tokens)-1, -1, -1):
        if type_tokens[i][0] == 'identifier' and type_tokens[i][1] not in C_KEYWORDS:
            var_name = type_tokens[i][1]
            var_idx = i
            break
    if var_name is None:
        return None, save + 1

    type_part = type_tokens[:var_idx]
    if not type_part:
        return None, save + 1

    type_str = source[type_part[0][2] : type_part[-1][3]].strip()
    type_str = ' '.join(type_str.split())

    la = skip_whitespace(tokens, idx, end_idx)
    if la > end_idx:
        return None, save + 1
    if tokens[la][1] == '(':
        idx = la + 1
        idx = skip_balanced(tokens, idx-1, end_idx, '(', ')')
        idx = skip_whitespace(tokens, idx, end_idx)
        if idx <= end_idx and tokens[idx][1] == ';':
            idx += 1
        return None, idx

    decl_start = save
    var_list = []
    first_name_idx = type_tokens[var_idx][2]

    while True:
        cur_name = None
        cur_name_idx = None
        init_expr = None

        if not var_list:
            cur_name = var_name
            cur_name_idx = first_name_idx
        else:
            if idx > end_idx or tokens[idx][0] != 'identifier':
                break
            cur_name = tokens[idx][1]
            cur_name_idx = tokens[idx][2]
            idx += 1
            idx = skip_whitespace(tokens, idx, end_idx)

        if idx <= end_idx and tokens[idx][1] == '=':
            idx += 1
            start_init = idx
            paren_depth = 0
            while idx <= end_idx:
                t = tokens[idx]
                if t[1] in (',', ';') and paren_depth == 0:
                    break
                if t[1] == '(': paren_depth += 1
                elif t[1] == ')': paren_depth -= 1
                idx += 1
            init_expr = ''.join(tok[1] for tok in tokens[start_init:idx]).strip()
        elif idx <= end_idx and tokens[idx][1] == '[':
            idx = skip_balanced(tokens, idx, end_idx, '[', ']')
            idx = skip_whitespace(tokens, idx, end_idx)

        var_list.append({
            "type": type_str,
            "name": cur_name,
            "decl_start": decl_start,
            "name_token_idx": cur_name_idx,
            "init": init_expr,
            "force_frame": force_frame,
            "force_local": force_local,
        })

        if idx > end_idx:
            break
        if tokens[idx][1] == ';':
            idx += 1
            break
        elif tokens[idx][1] == ',':
            idx += 1
            idx = skip_whitespace(tokens, idx, end_idx)
            continue
        else:
            break

    decl_end = idx - 1
    for v in var_list:
        v['decl_end'] = decl_end
    return var_list, idx

# ================================================
# 变量信息类
# ================================================
class VarInfo:
    def __init__(self, vtype, name, decl_start, decl_end, scope_end, name_token_idx, init=None):
        self.type = vtype
        self.name = name
        self.decl_start = decl_start
        self.decl_end = decl_end
        self.scope_end = scope_end
        self.name_token_idx = name_token_idx
        self.init = init
        self.refs = []
        self.inside_loop = False
        self.in_for_header = False
        self.addr_taken = False
        self.last_use_line = None
        self.force_frame = False
        self.force_local = False
        self.value_param_await_line = None

# ================================================
# 递归作用域分析
# ================================================
def analyze_scope(tokens, lbrace, rbrace, source, in_loop=False, parent_vars=None):
    variables = [] if parent_vars is None else list(parent_vars)
    yields = []
    await_returns = {}

    idx = lbrace + 1
    while idx < rbrace:
        idx = skip_whitespace(tokens, idx, rbrace)
        if idx >= rbrace:
            break
        t = tokens[idx]

        if t[0] == 'other' and t[1] == '#':
            idx = skip_preprocessor_line(tokens, idx, rbrace)
            continue

        if t[0] == 'identifier' and t[1] in ('_yield', '_await', '_await_static'):
            if t[1] == '_await':
                stmt_start = idx
                while stmt_start > lbrace and tokens[stmt_start - 1][1] not in (';', '{', '}'):
                    stmt_start -= 1
                stmt_segment = source[tokens[stmt_start][2] : tokens[idx][3]] if stmt_start <= idx else source
                m = re.search(r'(?:(?:[A-Za-z_][A-Za-z0-9_\s\*\[\]]*?)\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*=\s*$', stmt_segment)
                if m:
                    await_returns[m.group(1)] = idx
            yields.append(idx)
            idx += 1
            continue

        if t[1] == '{':
            close = find_matching_brace(tokens, idx, rbrace)
            inner_vars, inner_yields, inner_await_returns = analyze_scope(tokens, idx, close, source, in_loop, variables)
            for iv in inner_vars:
                existing = next((v for v in variables if v.name == iv.name and v.decl_start == iv.decl_start), None)
                if existing is None:
                    variables.append(iv)
                else:
                    existing.refs = list(iv.refs)
                    existing.addr_taken = iv.addr_taken or existing.addr_taken
                    existing.inside_loop = existing.inside_loop or iv.inside_loop
                    existing.scope_end = max(existing.scope_end, iv.scope_end)
            yields.extend(inner_yields)
            await_returns.update(inner_await_returns)
            idx = close + 1
            continue

        if t[0] == 'identifier' and t[1] in CONTROL_KW:
            kw = t[1]
            idx += 1
            idx = skip_whitespace(tokens, idx, rbrace)
            for_head_vars = []
            if kw == 'for' and idx <= rbrace and tokens[idx][1] == '(':
                lpar = idx
                idx += 1
                vlist, nxt = try_parse_declaration(tokens, idx, rbrace, source)
                if vlist:
                    for v in vlist:
                        vi = VarInfo(v['type'], v['name'], v['decl_start'], v['decl_end'], v['decl_end'], v['name_token_idx'], v.get('init'))
                        vi.in_for_header = True
                        vi.force_frame = bool(v.get('force_frame', False))
                        vi.force_local = bool(v.get('force_local', False))
                        for_head_vars.append(vi)
                    idx = nxt
                idx = skip_balanced(tokens, lpar, rbrace, '(', ')')
            elif kw in ('if','while','switch'):
                if idx <= rbrace and tokens[idx][1] == '(':
                    idx = skip_balanced(tokens, idx, rbrace, '(', ')')

            idx = skip_whitespace(tokens, idx, rbrace)
            if idx <= rbrace and tokens[idx][1] == '{':
                close = find_matching_brace(tokens, idx, rbrace)
                is_loop = kw in ('for', 'while', 'do')
                inner_in_loop = in_loop or is_loop
                inner_vars, inner_yields, inner_await_returns = analyze_scope(tokens, idx, close, source, inner_in_loop, variables)
                for iv in inner_vars:
                    existing = next((v for v in variables if v.name == iv.name and v.decl_start == iv.decl_start), None)
                    if existing is None:
                        variables.append(iv)
                    else:
                        existing.refs = list(iv.refs)
                        existing.addr_taken = iv.addr_taken or existing.addr_taken
                        existing.inside_loop = existing.inside_loop or iv.inside_loop
                        existing.scope_end = max(existing.scope_end, iv.scope_end)
                    last_ref_line = None
                    if iv.refs:
                        ref_lines = [source.count('\n', 0, tokens[r][2]) + 1 for r in iv.refs if r < len(tokens)]
                        if ref_lines:
                            last_ref_line = max(ref_lines)
                    if last_ref_line is not None:
                        block_start_line = source.count('\n', 0, tokens[idx][2]) + 1
                        block_end_line = source.count('\n', 0, tokens[close][2]) + 1
                        if block_start_line <= last_ref_line <= block_end_line:
                            target = existing if existing is not None else iv
                            target.scope_end = close
                yields.extend(inner_yields)
                await_returns.update(inner_await_returns)
                for vi in for_head_vars:
                    vi.scope_end = close
                    vi.inside_loop = True
                    existing = next((v for v in variables if v.name == vi.name and v.decl_start == vi.decl_start), None)
                    if existing is None:
                        variables.append(vi)
                    else:
                        existing.refs = list(vi.refs)
                        existing.addr_taken = vi.addr_taken or existing.addr_taken
                        existing.inside_loop = existing.inside_loop or vi.inside_loop
                        existing.scope_end = max(existing.scope_end, vi.scope_end)
                idx = close + 1
                if kw == 'do':
                    idx = skip_whitespace(tokens, idx, rbrace)
                    if idx <= rbrace and tokens[idx][1] == 'while':
                        idx += 1
                        idx = skip_whitespace(tokens, idx, rbrace)
                        if idx <= rbrace and tokens[idx][1] == '(':
                            idx = skip_balanced(tokens, idx, rbrace, '(', ')')
                        idx = skip_whitespace(tokens, idx, rbrace)
                        if idx <= rbrace and tokens[idx][1] == ';':
                            idx += 1
            else:
                while idx <= rbrace and tokens[idx][1] not in (';', '}'):
                    if tokens[idx][1] == '{':
                        idx = skip_balanced(tokens, idx, rbrace, '{', '}')
                    else:
                        idx += 1
                if idx <= rbrace and tokens[idx][1] == ';':
                    idx += 1
            continue

        stmt_start = idx
        while stmt_start > lbrace and tokens[stmt_start - 1][1] not in (';', '{', '}'):
            stmt_start -= 1
        stmt_end = idx
        while stmt_end < rbrace and tokens[stmt_end][1] not in (';', '{', '}'):
            stmt_end += 1
        stmt_text = source[tokens[stmt_start][2]:tokens[stmt_end][2]] if stmt_end <= rbrace and stmt_start <= stmt_end else ''
        for j in range(stmt_start, min(stmt_end, rbrace)):
            if tokens[j][0] == 'identifier' and tokens[j][1] in ('_yield', '_await', '_await_static'):
                yields.append(j)
                if tokens[j][1] in ('_await', '_await_static'):
                    m = re.search(r'(?:(?:[A-Za-z_][A-Za-z0-9_\s\*\[\]]*?)\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*=\s*_await(?:_static)?\s*', stmt_text)
                    if m:
                        await_returns[m.group(1)] = j

        decl_start = idx
        vlist, idx = try_parse_declaration(tokens, idx, rbrace, source)
        if vlist:
            decl_end = vlist[0]['decl_end']
            for v in vlist:
                vi = VarInfo(v['type'], v['name'], decl_start, decl_end, decl_end, v['name_token_idx'], v.get('init'))
                vi.inside_loop = in_loop
                vi.force_frame = bool(v.get('force_frame', False))
                vi.force_local = bool(v.get('force_local', False))
                if vi.init is not None and '_await' in vi.init:
                    await_returns[vi.name] = idx
                variables = [v for v in variables if v.name != vi.name]
                variables.append(vi)
        else:
            idx += 1

    for i in range(lbrace+1, rbrace):
        tok = tokens[i]
        if tok[0] == 'identifier':
            name = tok[1]
            for v in reversed(variables):
                if v.name == name:
                    if i == v.name_token_idx:
                        break
                    v.refs.append(i)
                    v.scope_end = max(v.scope_end, i)
                    line_no = source.count('\n', 0, tok[2]) + 1
                    if v.last_use_line is None or line_no > v.last_use_line:
                        v.last_use_line = line_no
                    if in_loop:
                        v.inside_loop = True
                    break
        elif tok[0] == 'other' and tok[1] == '&':
            j = i + 1
            j = skip_whitespace(tokens, j, rbrace)
            if j <= rbrace and tokens[j][0] == 'identifier':
                for v in reversed(variables):
                    if v.name == tokens[j][1]:
                        v.refs.append(j)
                        v.scope_end = max(v.scope_end, j)
                        line_no = source.count('\n', 0, tokens[j][2]) + 1
                        if v.last_use_line is None or line_no > v.last_use_line:
                            v.last_use_line = line_no
                        v.addr_taken = True
                        break

    for v in variables:
        if v.force_local:
            continue
        if v.name in await_returns:
            continue
        for idx in range(lbrace + 1, rbrace):
            if tokens[idx][0] == 'identifier' and tokens[idx][1] in ('_await', '_await_static'):
                stmt_start = idx
                while stmt_start > lbrace and tokens[stmt_start - 1][1] not in (';', '{', '}'):
                    stmt_start -= 1
                stmt_end = idx
                while stmt_end < rbrace and tokens[stmt_end][1] not in (';', '{', '}'):
                    stmt_end += 1
                stmt_text = source[tokens[stmt_start][2]: tokens[stmt_end][2]] if stmt_end <= rbrace and stmt_start <= stmt_end else ''
                if '_await' not in stmt_text and '_await_static' not in stmt_text:
                    continue
                if re.search(r'\b' + re.escape(v.name) + r'\s*=\s*_await(?:_static)?\b', stmt_text):
                    continue
                if not re.search(r'(?<!&)\b' + re.escape(v.name) + r'\b', stmt_text):
                    continue
                if re.search(r'&\s*' + re.escape(v.name) + r'\b', stmt_text):
                    continue
                v.value_param_await_line = source.count('\n', 0, tokens[idx][2]) + 1
                break

    return variables, yields, await_returns

def get_scope_debug_records(variables, yields, await_returns, tokens, source):
    records = []
    async_lines = sorted({source.count('\n', 0, tokens[idx][2]) + 1 for idx in yields})
    for v in variables:
        decl_line = source[:tokens[v.decl_start][2]].count('\n') + 1 if v.decl_start < len(tokens) else 1
        decl_end_line = source[:tokens[v.decl_end][3]].count('\n') + 1 if v.decl_end < len(tokens) else decl_line
        if v.value_param_await_line is not None:
            scope_end_line = v.value_param_await_line
            end_bracket = ')'
        else:
            scope_end_line = source[:tokens[v.scope_end][3]].count('\n') + 1 if v.scope_end < len(tokens) else decl_end_line
            end_bracket = ']'
        receiver_pos = await_returns.get(v.name)
        if v.force_local:
            has_async_span = False
        elif v.value_param_await_line is not None:
            has_async_span = any(decl_line < line < v.value_param_await_line for line in async_lines)
        else:
            has_async_span = any(decl_line < line <= scope_end_line for line in async_lines)
        records.append({
            'name': v.name,
            'type': v.type,
            'decl_start': v.decl_start,
            'decl_end': v.decl_end,
            'scope_end': v.scope_end,
            'decl_line': decl_line,
            'decl_end_line': decl_end_line,
            'scope_end_line': scope_end_line,
            'scope_end_bracket': end_bracket,
            'addr_taken': getattr(v, 'addr_taken', False),
            'inside_loop': v.inside_loop,
            'await_receiver_at': receiver_pos,
            'refs': list(v.refs),
            'async_keyword_lines': async_lines,
            'has_async_span': has_async_span,
            'same_statement_await': receiver_pos is not None and receiver_pos <= v.decl_end,
            'force_frame': bool(getattr(v, 'force_frame', False)),
            'force_local': bool(getattr(v, 'force_local', False)),
        })
    return records


def determine_promoted(variables, yields, await_returns=None, tokens=None, source=None):
    if await_returns is None:
        await_returns = {}
    if tokens is None or source is None:
        return {}
    async_lines = sorted({source.count('\n', 0, tokens[idx][2]) + 1 for idx in yields})
    promoted = {}

    for v in variables:
        if v.force_frame:
            promoted[v.name] = v
            continue
        if v.force_local:
            continue
        if getattr(v, 'addr_taken', False):
            promoted[v.name] = v
            continue

        decl_line = source[:tokens[v.decl_start][2]].count('\n') + 1 if v.decl_start < len(tokens) else 1
        if v.value_param_await_line is not None:
            scope_end_line = v.value_param_await_line
            has_async_span = any(decl_line < line < v.value_param_await_line for line in async_lines)
        else:
            scope_end_line = source[:tokens[v.scope_end][3]].count('\n') + 1 if v.scope_end < len(tokens) else decl_line
            has_async_span = any(decl_line < line <= scope_end_line for line in async_lines)
        if has_async_span:
            promoted[v.name] = v

    return promoted

# ================================================
# 变量提升
# ================================================
def variable_hoisting_body(func_info, tokens, source, param_names):
    lbrace = func_info['lbrace']
    rbrace = func_info['rbrace']
    variables, yields, await_returns = analyze_scope(tokens, lbrace, rbrace, source, in_loop=False)
    promoted = determine_promoted(variables, yields, await_returns, tokens=tokens, source=source)

    replace_names = set(promoted.keys()) | set(param_names)

    decl_ranges = set()
    for v in promoted.values():
        for i in range(v.decl_start, v.decl_end+1):
            decl_ranges.add(i)

    result_parts = []
    i = lbrace
    while i <= rbrace:
        t = tokens[i]
        if t[0] in ('whitespace', 'comment_line', 'comment_block'):
            result_parts.append(t[1])
            i += 1
            continue

        if i in decl_ranges:
            decl_start_cur = None
            for v in promoted.values():
                if v.decl_start <= i <= v.decl_end:
                    decl_start_cur = v.decl_start
                    break
            if decl_start_cur is not None:
                vars_in_decl = [v for v in promoted.values() if v.decl_start == decl_start_cur]
                if vars_in_decl[0].in_for_header:
                    inits = []
                    for var in vars_in_decl:
                        if var.init is not None:
                            inits.append(f"_prv_data->{var.name} = {var.init}")
                    if inits:
                        result_parts.append(", ".join(inits))
                    result_parts.append(";")
                else:
                    for var in vars_in_decl:
                        if var.init is not None:
                            result_parts.append(f"_prv_data->{var.name} = {var.init};")
                i = vars_in_decl[0].decl_end + 1
                continue

        if t[0] == 'identifier' and t[1] in replace_names:
            result_parts.append(f"_prv_data->{t[1]}")
            i += 1
            continue

        result_parts.append(t[1])
        i += 1

    return ''.join(result_parts), promoted

# ================================================
# 状态机生成（支持 #line，无变化，不再重复）
# ================================================
def get_line_indent(text, pos):
    line_start = text.rfind('\n', 0, pos) + 1
    line = text[line_start:pos]
    return line[:len(line) - len(line.lstrip())]

def apply_state_machine(hoisted_body, fname, ret_type, param_names,
                        body_start_line, enable_line, source_file,
                        promoted_names=None, callback_mode=False):
    if promoted_names is None:
        promoted_names = set()
    body = hoisted_body
    lbrace = body.find('{')
    rbrace = body.rfind('}')
    if lbrace == -1 or rbrace == -1:
        return body
    inner = body[lbrace + 1 : rbrace]

    pattern_await = re.compile(r'_await\s+(?P<func>\w+)\s*\((?P<args>[^)]*)\)\s*;')
    pattern_await_static = re.compile(r'_await_static\s*\((?P<obj>[^)]+)\)\s+(?P<func>\w+)\s*\((?P<args>[^)]*)\)\s*;')
    pattern_yield = re.compile(r'_yield\s*\(\s*\)\s*;')
    pattern_return = re.compile(r'return\s*(.*?)\s*;')

    def statement_start_for_pos(text, pos):
        line_start = text.rfind('\n', 0, pos) + 1
        line_text = text[line_start:pos]
        # An awaited call is usually the right-hand side of its owning statement on the
        # same line, e.g. "uint8_t x = _await foo();". In that case, the statement boundary
        # must start at the beginning of the line, otherwise earlier comment/assignment text
        # from the same block is incorrectly carried into the generated prefix.
        if '=' in line_text:
            return line_start
        i = pos
        while i > 0 and text[i - 1] not in ';{}':
            i -= 1
        return i

    def rewrite_promoted_expr(expr, promoted_names):
        if expr is None:
            return expr
        out = []
        i = 0
        while i < len(expr):
            ch = expr[i]
            if ch.isalpha() or ch == '_':
                j = i + 1
                while j < len(expr) and (expr[j].isalnum() or expr[j] == '_'):
                    j += 1
                word = expr[i:j]

                k = i - 1
                while k >= 0 and expr[k].isspace():
                    k -= 1
                prev_sig = expr[k] if k >= 0 else ''

                if word == '_prv_data':
                    out.append(word)
                elif word in promoted_names and prev_sig not in ('>', '.', '_'):
                    out.append(f"_prv_data->{word}")
                else:
                    out.append(word)
                i = j
                continue
            out.append(ch)
            i += 1
        return ''.join(out)

    def extract_lhs_before_await(text, pos):
        stmt_start = statement_start_for_pos(text, pos)
        stmt_text = text[stmt_start:pos]
        if '=' not in stmt_text:
            return '', ''
        left = stmt_text.rsplit('=', 1)[0].strip()
        if not left:
            return '', ''
        if '->' in left:
            base = left.rsplit('->', 1)[0].strip()
            name = left.split('->')[-1].strip()
            return base + '->', name
        toks = left.split()
        if len(toks) >= 2:
            return ' '.join(toks[:-1]), toks[-1]
        return '', toks[0] if toks else ''

    def strip_trailing_await_assignment(prefix, recv_var):
        if not prefix or not recv_var:
            return prefix
        if not prefix.strip():
            return prefix
        lines = prefix.splitlines(keepends=True)
        if not lines:
            return prefix
        last = lines[-1]
        if re.search(rf'\b{re.escape(recv_var)}\s*=', last):
            lines.pop()
            return ''.join(lines)
        if re.search(rf'\b{re.escape(recv_var)}\s*=', prefix):
            prefix = re.sub(rf'\s*\b{re.escape(recv_var)}\s*=\s*[^\n]*$', '', prefix)
        return prefix

    matches = []
    for m in pattern_await.finditer(inner):
        decl_prefix, recv_var = extract_lhs_before_await(inner, m.start())
        matches.append(('await', m, decl_prefix, recv_var))
    for m in pattern_await_static.finditer(inner):
        decl_prefix, recv_var = extract_lhs_before_await(inner, m.start())
        matches.append(('await_static', m, decl_prefix, recv_var))
    for m in pattern_yield.finditer(inner):
        matches.append(('yield', m, '', ''))
    for m in pattern_return.finditer(inner):
        matches.append(('return', m, '', ''))
    matches.sort(key=lambda x: x[1].start())

    label_prefix = "_colabel_" if callback_mode else f"_colable_{fname}_"
    step = 1
    case_lines = []
    for typ, m, _, _ in matches:
        if typ in ('await', 'await_static', 'yield'):
            case_lines.append(f"case {step}: goto {label_prefix}{step};")
            step += 1

    out = []
    indent = "    "
    if callback_mode:
        out.append(f"    struct _coval_{fname} *_prv_data = (struct _coval_{fname} *)co->prv_data;\n")
        out.append("\n")
        out.append("    switch(co->step){\n")
        for cl in case_lines:
            out.append("        " + cl + "\n")
        out.append("    }\n")
        out.append("\n")
    else:
        out.append("{\n")
        out.append(indent + f"struct _coval_{fname} *_prv_data;\n")
        out.append("\n")
        out.append(indent + "// 如果传进来的对象是空，那么代表外界期望动态创建这个协程的对象\n")
        out.append(indent + "if(co == NULL){\n")
        out.append(indent + "    // 动态分配\n")
        out.append(indent + "    co = (struct coro_stu *)ctx_mem_alloc(sizeof(struct coro_stu));\n")
        out.append(indent + "    if(co == NULL){\n")
        out.append(indent + "        return ;\n")
        out.append(indent + "    }\n")
        out.append(indent + f"    co->prv_data = (struct _coval_{fname} *)ctx_mem_data_alloc(sizeof(struct _coval_{fname}));\n")
        out.append(indent + "    if(co->prv_data == NULL){\n")
        out.append(indent + "        ctx_mem_free(co);\n")
        out.append(indent + "        return ;\n")
        out.append(indent + "    }\n")
        out.append(indent + "    co->step = 0;\n")
        out.append(indent + "}\n")
        out.append(indent + f"_prv_data = (struct _coval_{fname} *)co->prv_data;\n")
        out.append("\n")
        out.append(indent + "switch(co->step){\n")
        for cl in case_lines:
            out.append(indent + "    " + cl + "\n")
        out.append(indent + "}\n")
        out.append("\n")
        out.append(indent + "// 步骤 0 用于初始化\n")
        out.append(f"{label_prefix}0:\n")
        out.append(indent + "// 初始化协程对象\n")
        out.append(indent + "co->father = father;\n")
        out.append(indent + "if(father != NULL) father->son = co;\n")
        out.append(indent + f"// 配置状态机回调\n")
        out.append(indent + f"ctx_coro_init(co, _cocb_{fname});\n")
        out.append("\n")
        out.append(indent + "/* BEGIN: 根据实际情况生成不同的初始化参数变量内容 */\n")
        if param_names:
            for pn in param_names:
                out.append(indent + f"_prv_data->{pn} = {pn};\n")
        else:
            out.append(indent + "/* 无参数需要初始化 */\n")
        out.append(indent + "/* END: 根据实际情况生成不同的初始化参数和局部变量内容 */\n")
        out.append("\n")

    if enable_line and not callback_mode:
        out.append(f'#line {body_start_line} "{source_file}"\n')

    last_end = 0
    step = 1
    for typ, m, decl_prefix, recv_var in matches:
        stmt_start = statement_start_for_pos(inner, m.start())
        stmt_end = m.end()
        prefix = inner[last_end:stmt_start]
        prefix = strip_trailing_await_assignment(prefix, recv_var)
        if prefix and prefix.strip() and not prefix.endswith('\n') and not prefix.endswith('\r'):
            prefix += '\n'
        out.append(prefix)

        orig_indent = get_line_indent(inner, m.start())
        line_offset = inner[:m.start()].count('\n')
        line_num = body_start_line + line_offset

        if enable_line:
            out.append(f'#line {line_num} "{source_file}"\n')

        if typ == 'yield':
            out.append("\n" + orig_indent + "/* BEGIN: 检测到 _yield 关键字，替换 */\n")
            out.append(orig_indent + f"co->step = {step};\n")
            out.append(orig_indent + "ctx_coro_wake(co, 0); // 0 代表 0 tick 后唤醒，也就是告诉调度器尽快唤醒\n")
            out.append(orig_indent + "return ; // 出让\n")
            out.append(f"{label_prefix}{step}:\n")
            out.append(orig_indent + "/* END: 检测到 _yield 关键字，替换 */\n")
            if enable_line:
                out.append(f'#line {line_num + 1} "{source_file}"\n')
            step += 1

        elif typ == 'await':
            func = m.group('func')
            args = (m.group('args') or '').strip()
            rewritten_args = rewrite_promoted_expr(args, promoted_names)
            call_args = rewritten_args if rewritten_args else ""
            if call_args:
                call_str = f"_co_{func}(co, NULL, {call_args})"
            else:
                call_str = f"_co_{func}(co, NULL)"
            out.append("\n\n" + orig_indent + "/* BEGIN: 检测到 _await 关键字，替换 */\n")
            out.append(orig_indent + f"co->step = {step};\n")
            out.append(orig_indent + call_str + ";\n")
            out.append(orig_indent + "return ; // 出让\n")
            out.append(f"{label_prefix}{step}:\n")
            if recv_var:
                if recv_var in promoted_names:
                    out.append(orig_indent + f"_prv_data->{recv_var} = ((struct _coval_{func} *)(co->son->prv_data))->_coretval_;\n")
                elif decl_prefix:
                    out.append(orig_indent + ";\n")
                    out.append(orig_indent + f"{decl_prefix} {recv_var} = ((struct _coval_{func} *)(co->son->prv_data))->_coretval_;\n")
                else:
                    out.append(orig_indent + f"{recv_var} = ((struct _coval_{func} *)(co->son->prv_data))->_coretval_;\n")
            else:
                out.append(orig_indent + "/* 用户未接收返回值 */\n")
            out.append(orig_indent + "// free 子协程对象\n")
            out.append(orig_indent + "ctx_mem_data_free(co->son->prv_data);\n")
            out.append(orig_indent + "ctx_mem_free(co->son);\n")
            out.append(orig_indent + "co->son = NULL;\n")
            out.append(orig_indent + "/* END: 检测到 _await 关键字，替换 */\n")
            if enable_line:
                out.append(f'#line {line_num + 1} "{source_file}"\n')
            step += 1

        elif typ == 'await_static':
            obj_ptr = (m.group('obj') or '').strip()
            func = m.group('func')
            args = (m.group('args') or '').strip()
            rewritten_obj = rewrite_promoted_expr(obj_ptr, promoted_names)
            rewritten_args = rewrite_promoted_expr(args, promoted_names)
            if rewritten_args:
                call_args = f"{rewritten_obj}, {rewritten_args}"
            else:
                call_args = rewritten_obj
            out.append("\n\n" + orig_indent + "/* BEGIN: 检测到 _await_static 关键字，替换 */\n")
            out.append(orig_indent + f"co->step = {step};\n")
            out.append(orig_indent + f"_co_{func}(co, {call_args});\n")
            out.append(orig_indent + "return ; // 出让\n")
            out.append(f"{label_prefix}{step}:\n")
            if recv_var:
                if recv_var in promoted_names:
                    out.append(orig_indent + f"_prv_data->{recv_var} = ((struct _coval_{func} *)(co->son->prv_data))->_coretval_;\n")
                elif decl_prefix:
                    out.append(orig_indent + ";\n")
                    out.append(orig_indent + f"{decl_prefix} {recv_var} = ((struct _coval_{func} *)(co->son->prv_data))->_coretval_;\n")
                else:
                    out.append(orig_indent + f"{recv_var} = ((struct _coval_{func} *)(co->son->prv_data))->_coretval_;\n")
            else:
                out.append(orig_indent + "/* 用户未接收返回值 */\n")
            out.append(orig_indent + "co->son = NULL;\n")
            out.append(orig_indent + "/* END: 检测到 _await_static 关键字，替换 */\n")
            if enable_line:
                out.append(f'#line {line_num + 1} "{source_file}"\n')
            step += 1

        elif typ == 'return':
            expr = m.group(1).strip()
            if ret_type == 'void' or ret_type == '':
                out.append("\n" + orig_indent + "/* BEGIN: 检测到用户 return，替换 */\n")
                out.append(orig_indent + f"goto {label_prefix}end;\n")
                out.append(orig_indent + "/* END: 检测到用户 return，替换 */\n")
            else:
                assign = f"_prv_data->_coretval_ = {expr}; " if expr else ""
                out.append("\n" + orig_indent + "/* BEGIN: 检测到用户 return xxx; 替换 */\n")
                out.append(orig_indent + f"{assign}goto {label_prefix}end;\n")
                out.append(orig_indent + "/* END: 检测到用户 return xxx; 替换 */\n")
            if enable_line:
                out.append(f'#line {line_num + 1} "{source_file}"\n')

        last_end = stmt_end

    out.append(inner[last_end:])
    out.append(f"{label_prefix}end:\n")
    out.append(indent + "// co->step = 0; // 复位状态机\n")
    if callback_mode:
        out.append(indent + "if(co->father == NULL){ // 没有父协程则自己 free 自己\n")
        out.append(indent + indent + "ctx_mem_data_free(co->prv_data);\n")
        out.append(indent + indent + "ctx_mem_free(co);\n")
        out.append(indent + "}else {\n")
        out.append(indent + indent + "// 唤醒父协程\n")
        out.append(indent + indent + "ctx_coro_wake(co->father, 0); // 0 代表 0 tick 后唤醒\n")
        out.append(indent + "}\n")
    else:
        out.append(indent + "if(father == NULL){ // 没有父协程则自己 free 自己\n")
        out.append(indent + indent + "ctx_mem_data_free(co->prv_data);\n")
        out.append(indent + indent + "ctx_mem_free(co);\n")
        out.append(indent + "}else {\n")
        out.append(indent + indent + "// 唤醒父协程\n")
        out.append(indent + indent + "ctx_coro_wake(father, 0); // 0 代表 0 tick 后唤醒\n")
        out.append(indent + "}\n")

    return ''.join(out)

# ================================================
# 参数分割
# ================================================
def split_params(params_str):
    if not params_str or params_str.strip() == 'void':
        return []
    parts = []
    cur = []
    depth = 0
    for ch in params_str:
        if ch in '({[':
            depth += 1
            cur.append(ch)
        elif ch in ')}]':
            depth -= 1
            cur.append(ch)
        elif ch == ',' and depth == 0:
            parts.append(''.join(cur).strip())
            cur = []
        else:
            cur.append(ch)
    if cur:
        parts.append(''.join(cur).strip())

    res = []
    for p in parts:
        if not p:
            continue
        m = re.search(r'([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^\]]*\])?\s*$', p)
        if not m:
            continue
        name = m.group(1)
        type_part = p[:m.start(1)].rstrip()
        if type_part.endswith('*') or type_part.endswith('&'):
            type_part = type_part.rstrip() + ' '
        res.append((type_part.strip(), name))
    return res

# ================================================
# 生成输出文件
# ================================================
def generate_output(input_file, enable_line):
    with open(input_file, 'r', encoding='utf-8') as f:
        source = f.read()
    funcs, tokens = extract_async_functions(source)

    if not funcs:
        # print("No _async functions found. No output generated.")
        return

    base = os.path.splitext(input_file)[0]
    h_file = base + '.c.coro.h'
    c_file = base + '.c.coro'
    src_basename = os.path.basename(input_file)

    # 收集所有结构体定义
    struct_defs = []
    for fn in funcs:
        param_pairs = split_params(fn['params'])
        param_names = [n for _, n in param_pairs]

        # 分析变量提升
        _, tokens = extract_async_functions(source)  # 重新获取 tokens (简单方式)
        # 但 tokens 已在上方提取，这里重复调用会重新解析，但更快的方式是直接重用上面的 tokens。
        # 我们重新组织：在外部只做一次提取，然后循环处理。但代码略长，为清晰我们在这里单独分析。
        # 实际上上面的 funcs, tokens 已经包含所有函数，我们直接使用。
        # 重新获取 tokens 因为上面循环中 tokens 变量被覆盖了？我们保存一个副本。
        # 在这里简单重新获取 tokens，确保正确。
    # 我们重构 process_file 逻辑，直接循环 funcs，但 tokens 需保留。上面的提取已得到 tokens，我们在循环内直接使用外部 tokens。
    # 采用新方案：外部一次性提取，然后内部遍历 funcs。

    # 调整：直接使用上面提取的 funcs 和 tokens（但 tokens 是局部变量，需在函数内传递）
    # 我们把整体逻辑写在一个主函数中。

def build_scope_debug_payload(source, funcs, tokens):
    debug_records = []
    for fn in funcs:
        variables, yields, await_returns = analyze_scope(tokens, fn['lbrace'], fn['rbrace'], source, in_loop=False)
        debug_records.append({
            'func': fn['name'],
            'start_line': source[:tokens[fn['lbrace']][2]].count('\n') + 1,
            'end_line': source[:tokens[fn['rbrace']][3]].count('\n') + 1,
            'variables': get_scope_debug_records(variables, yields, await_returns, tokens, source),
        })
    return {'functions': debug_records}


def render_scope_debug_text(payload):
    lines = []
    for fn in payload.get('functions', []):
        lines.append(f"{fn['func']}:")
        lines.append("    # 所有局部变量，不包括 const、static、_var_frame、_var_local 标注的变量")
        lines.append("    all_local_var:")
        all_vars = []
        for v in fn.get('variables', []):
            if v.get('force_local') or v.get('force_frame'):
                continue
            all_vars.append(v)
        for v in all_vars:
            lines.append(f"        {v['type']} {v['name']}:")
            lines.append(f"            作用范围=({v['decl_line']}, {v['scope_end_line']}{v.get('scope_end_bracket', ']')}")
            lines.append(f"            跨越出让点={str(v['has_async_span']).capitalize()}")
        lines.append("    # 需要提升的变量")
        lines.append("    frame_var:")
        frame_vars = []
        for v in fn.get('variables', []):
            if v.get('force_frame') or v.get('has_async_span'):
                if not v.get('force_local'):
                    frame_vars.append(f"{v['type']} {v['name']}")
        if not frame_vars:
            lines.append("        (无)")
        else:
            for entry in frame_vars:
                lines.append(f"        {entry}")
        lines.append("")
    return '\n'.join(lines).rstrip() + '\n'


def process(input_file, enable_line, debug_scope=False, debug_path=None, debug_scope_text=None):
    with open(input_file, 'r', encoding='utf-8') as f:
        source = f.read()
    funcs, tokens = extract_async_functions(source)

    if not funcs:
        # print("No _async functions detected. No files generated.")
        return

    payload = build_scope_debug_payload(source, funcs, tokens)
    if debug_scope or debug_path:
        if debug_path:
            with open(debug_path, 'w', encoding='utf-8') as f:
                json.dump(payload, f, ensure_ascii=False, indent=2)
            print(f"Scope debug JSON written to: {debug_path}")
        else:
            sys.stdout.buffer.write((json.dumps(payload, ensure_ascii=False, indent=2) + '\n').encode('utf-8'))

    if debug_scope_text is not None:
        text_out = render_scope_debug_text(payload)
        if debug_scope_text == '-':
            sys.stdout.buffer.write(text_out.encode('utf-8'))
        else:
            with open(debug_scope_text, 'w', encoding='utf-8') as f:
                f.write(text_out)
            print(f"Scope debug text written to: {debug_scope_text}")

    base = os.path.splitext(input_file)[0]
    h_file = base + '.c.coro.h'
    c_file = base + '.c.coro'
    src_basename = os.path.basename(input_file)

    # 准备头文件内容
    h_guard = re.sub(r'[^a-zA-Z0-9_]', '_', os.path.basename(base).upper()) + '_CORO_H_'
    h_lines = []
    h_lines.append(f"#ifndef {h_guard}")
    h_lines.append(f"#define {h_guard}")
    h_lines.append("")
    h_lines.append("// Auto-generated private data structures for async coroutines")
    h_lines.append("")

    # 准备实现文件内容
    c_lines = []
    c_lines.append(f'#include "{os.path.basename(h_file)}"')
    c_lines.append("")

    for fn in funcs:
        param_pairs = split_params(fn['params'])
        param_names = [n for _, n in param_pairs]

        # 重新计算 tokens 区间？使用外部 tokens 即可。
        # 计算起始行号
        body_start_line = source[:tokens[fn['lbrace']][2]].count('\n') + 1
        end_line = source[:tokens[fn['rbrace']][3]].count('\n')

        # 变量提升
        hoisted_body, promoted_dict = variable_hoisting_body(fn, tokens, source, param_names)
        promoted_vars = list(promoted_dict.values())
        promoted_names = set(promoted_dict.keys())

        # 生成结构体定义
        struct_name = f"_coval_{fn['name']}"
        struct_def = []
        struct_def.append(f"struct {struct_name} {{")
        struct_def.append("    // 参数")
        if param_pairs:
            for t, n in param_pairs:
                struct_def.append(f"    {t} {n};")
        else:
            struct_def.append("    // (无)")
        struct_def.append("")
        struct_def.append("    // 需要持久化的局部变量")
        if promoted_vars:
            for v in promoted_vars:
                struct_def.append(f"    {v.type} {v.name};")
        else:
            struct_def.append("    // (无)")
        struct_def.append("")
        ret_type = fn['return_type']
        if ret_type == 'void' or ret_type == '':
            core_t = 'int'
        else:
            core_t = ret_type
        struct_def.append(f"    // 返回值")
        struct_def.append(f"    {core_t} _coretval_;")
        struct_def.append("};")
        h_lines.extend(struct_def)
        h_lines.append("")

        # 生成实现部分（注释、回调声明、翻译体、回调实现）
        impl_lines = []
        impl_lines.append("/**")
        impl_lines.append(" * 识别到 _async 关键字")
        impl_lines.append(f" * 函数名称: {fn['name']}")
        impl_lines.append(f" * 返回类型: {fn['return_type']}")
        impl_lines.append(f" * 起始行号: {body_start_line}")
        impl_lines.append(f" * 终止行号: {end_line}")
        impl_lines.append(" * ")
        impl_lines.append(" * 参数: ")
        if param_pairs:
            for t, n in param_pairs:
                impl_lines.append(f" *       {t} {n}")
        else:
            impl_lines.append(" *       (无)")
        impl_lines.append(" * ")
        impl_lines.append(" * 需要持久化的局部变量: ")
        if promoted_vars:
            for v in promoted_vars:
                impl_lines.append(f" *           {v.type} {v.name}")
        else:
            impl_lines.append(" *           (无)")
        impl_lines.append(" * ")
        impl_lines.append(" * 局部变量作用域详情: ")
        variables, yields, await_returns = analyze_scope(tokens, fn['lbrace'], fn['rbrace'], source, in_loop=False)
        async_lines = sorted({source.count('\n', 0, tokens[idx][2]) + 1 for idx in yields})
        impl_lines.append(f" *   异步关键字捕获行号: {', '.join(map(str, async_lines))}")
        debug_vars = get_scope_debug_records(variables, yields, await_returns, tokens, source)
        for d in debug_vars:
            end_symbol = d.get('scope_end_bracket', ']')
            impl_lines.append(
                f" *   {d['type']} {d['name']}: 作用范围=({d['decl_line']}, {d['scope_end_line']}{end_symbol}, 跨越出让点={str(d['has_async_span']).capitalize()}"
            )
        impl_lines.append(" */")
        impl_lines.append("")

        impl_lines.append("// ======== 翻译后的函数体 ========")
        param_str = fn['params']
        if param_str and param_str.strip() != 'void':
            new_params = f"struct coro_stu *father, struct coro_stu *co, {param_str}"
        else:
            new_params = "struct coro_stu *father, struct coro_stu *co"

        callback_body = apply_state_machine(
            hoisted_body, fn['name'], fn['return_type'], param_names,
            body_start_line, enable_line, src_basename,
            promoted_names, callback_mode=True
        )
        impl_lines.append(f"void _cocb_{fn['name']}(struct coro_stu *co) {{")
        impl_lines.extend(callback_body.splitlines())
        impl_lines.append("}")
        impl_lines.append("")

        impl_lines.append(f"struct coro_stu* _co_{fn['name']}({new_params}) {{")
        # 函数声明
        h_lines.append(f"struct coro_stu* _co_{fn['name']}({new_params});\n")

        impl_lines.append(f"    struct _coval_{fn['name']} *_prv_data;")
        impl_lines.append("")
        impl_lines.append("    // 如果传进来的对象是空，那么代表外界期望动态创建这个协程的对象")
        impl_lines.append("    if(co == NULL){")
        impl_lines.append("        // 动态分配")
        impl_lines.append("        co = (struct coro_stu *)ctx_mem_alloc(sizeof(struct coro_stu));")
        impl_lines.append("        if(co == NULL){")
        impl_lines.append("            return NULL;")
        impl_lines.append("        }")
        impl_lines.append(f"        co->prv_data = (struct _coval_{fn['name']} *)ctx_mem_data_alloc(sizeof(struct _coval_{fn['name']}));")
        impl_lines.append("        if(co->prv_data == NULL){")
        impl_lines.append("            ctx_mem_free(co);")
        impl_lines.append("            return NULL;")
        impl_lines.append("        }")
        impl_lines.append("    }")
        impl_lines.append(f"    _prv_data = (struct _coval_{fn['name']} *)co->prv_data;")
        impl_lines.append("")
        impl_lines.append("    // 初始化协程对象")
        impl_lines.append("    co->father = father;")
        impl_lines.append("    // 配置状态机回调")
        impl_lines.append(f"    ctx_coro_init(co, _cocb_{fn['name']});")
        impl_lines.append("")
        impl_lines.append("    /* BEGIN: 根据实际情况生成不同的初始化参数变量内容 */")
        if param_pairs:
            for t, n in param_pairs:
                impl_lines.append(f"    _prv_data->{n} = {n};")
        else:
            impl_lines.append("    /* 无参数需要初始化 */")
        impl_lines.append("    /* END: 根据实际情况生成不同的初始化参数和局部变量内容 */")
        impl_lines.append("")
        impl_lines.append("    // 运行/启动该任务")
        impl_lines.append("    co->step = 0;")
        impl_lines.append("    if(father != NULL){")
        impl_lines.append("        father->son = co;")
        impl_lines.append("        // 如果是 _async 函数使用 _await/_await_static 调用，那么就地运行到出让点，不必等调度器调度，减少调度切换次数")
        impl_lines.append(f"        _cocb_{fn['name']}(co);")
        impl_lines.append("    }else {")
        impl_lines.append("        // 如果是非 _async 函数调用 _start_async 创建异步任务，那么不立即执行内容，而是等调度器调度，利于业务启停控制")
        impl_lines.append("        ctx_coro_wake(co, 0); // 0 代表 0 tick 后唤醒，也就是告诉调度器尽快唤醒")
        impl_lines.append("    }")
        impl_lines.append("")
        impl_lines.append("    return co;")
        impl_lines.append("}")
        impl_lines.append("")

        c_lines.extend(impl_lines)
        c_lines.append("")

    # 头文件结尾
    h_lines.append(f"#endif // {h_guard}\n")

    # 写入文件
    with open(h_file, 'w', encoding='utf-8') as f:
        f.write('\n'.join(h_lines))
    with open(c_file, 'w', encoding='utf-8') as f:
        f.write('\n'.join(c_lines))

    print(f"Generated: {h_file}")
    print(f"Generated: {c_file}")

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description='Translate C _async coroutine functions into coroutine state-machine code.')
    parser.add_argument('input', help='Input C source file')
    parser.add_argument('--line', action='store_true', help='Emit #line directives for debug mapping')
    parser.add_argument('--debug-scope', action='store_true', help='Print scope/lifetime debug JSON')
    parser.add_argument('--debug-scope-json', metavar='PATH', help='Write debug scope JSON to a file')
    parser.add_argument('--debug-scope-text', nargs='?', const='-', metavar='PATH', help='Print the human-readable local-variable lifetime report. If PATH is provided, write it to that file instead of stdout.')
    args = parser.parse_args()
    process(args.input, args.line, debug_scope=args.debug_scope, debug_path=args.debug_scope_json, debug_scope_text=args.debug_scope_text)