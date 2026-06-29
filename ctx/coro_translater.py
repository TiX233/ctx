#!/usr/bin/env python3
"""
C 无栈协程 _async 源到源翻译器
2026年6月27日

用法：python coro_translater.py <input.c> [--line]
输出：
  - <input>.c.coro.h   （结构体定义）
  - <input>.c.coro     （其余翻译代码）
若无 _async 函数则不会生成任何文件。
"""

import sys
import re
import os

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

    type_tokens = []
    while idx <= end_idx:
        idx = skip_whitespace(tokens, idx, end_idx)
        if idx > end_idx:
            break
        ct = tokens[idx]
        if ct[0] == 'identifier' and ct[1] in ('const','volatile','restrict','inline','_Atomic'):
            type_tokens.append(ct)
            idx += 1
            continue
        if ct[1] == '*':
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
            "init": init_expr
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

# ================================================
# 递归作用域分析
# ================================================
def analyze_scope(tokens, lbrace, rbrace, source, in_loop=False):
    variables = []
    yields = []

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
            yields.append(idx)
            idx += 1
            continue

        if t[1] == '{':
            close = find_matching_brace(tokens, idx, rbrace)
            inner_vars, inner_yields = analyze_scope(tokens, idx, close, source, in_loop)
            for iv in inner_vars:
                iv.inside_loop = in_loop
                variables = [v for v in variables if v.name != iv.name]
                variables.append(iv)
            yields.extend(inner_yields)
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
                        vi = VarInfo(v['type'], v['name'], v['decl_start'], v['decl_end'], 0, v['name_token_idx'], v.get('init'))
                        vi.in_for_header = True
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
                inner_vars, inner_yields = analyze_scope(tokens, idx, close, source, inner_in_loop)
                for iv in inner_vars:
                    iv.inside_loop = inner_in_loop
                    variables = [v for v in variables if v.name != iv.name]
                    variables.append(iv)
                yields.extend(inner_yields)
                for vi in for_head_vars:
                    vi.scope_end = close
                    vi.inside_loop = True
                    variables = [v for v in variables if v.name != vi.name]
                    variables.append(vi)
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

        decl_start = idx
        vlist, idx = try_parse_declaration(tokens, idx, rbrace, source)
        if vlist:
            decl_end = vlist[0]['decl_end']
            for v in vlist:
                vi = VarInfo(v['type'], v['name'], decl_start, decl_end, rbrace, v['name_token_idx'], v.get('init'))
                vi.inside_loop = in_loop
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
                    if i <= v.scope_end:
                        v.refs.append(i)
                    break

    return variables, yields

def determine_promoted(variables, yields):
    promoted = {}
    for v in variables:
        if not v.refs:
            continue
        relevant_yields = [y for y in yields if v.decl_start < y <= v.scope_end]
        if not relevant_yields:
            continue
        if v.inside_loop:
            promoted[v.name] = v
        else:
            for y in relevant_yields:
                if any(ref > y for ref in v.refs):
                    promoted[v.name] = v
                    break
    return promoted

# ================================================
# 变量提升
# ================================================
def variable_hoisting_body(func_info, tokens, source, param_names):
    lbrace = func_info['lbrace']
    rbrace = func_info['rbrace']
    variables, yields = analyze_scope(tokens, lbrace, rbrace, source, in_loop=False)
    promoted = determine_promoted(variables, yields)

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
                        body_start_line, enable_line, source_file):
    body = hoisted_body
    lbrace = body.find('{')
    rbrace = body.rfind('}')
    if lbrace == -1 or rbrace == -1:
        return body
    inner = body[lbrace+1 : rbrace]

    pattern_await = re.compile(
        r'(?:(\S+(?:\s*->\s*\w+)*)\s*=\s*)?'   # 可选的左值 =
        r'_await\s+(\w+)\s*\(([^)]*)\)\s*;'
    )
    pattern_await_static = re.compile(
        r'(?:(\S+(?:\s*->\s*\w+)*)\s*=\s*)?'   # 可选的左值 =
        r'_await_static\s*\(([^)]+)\)\s+(\w+)\s*\(([^)]*)\)\s*;'
    )
    pattern_yield = re.compile(r'_yield\s*\(\s*\)\s*;')
    pattern_return = re.compile(r'return\s*(.*?)\s*;')

    matches = []
    for m in pattern_await.finditer(inner):
        matches.append(('await', m))
    for m in pattern_await_static.finditer(inner):
        matches.append(('await_static', m))
    for m in pattern_yield.finditer(inner):
        matches.append(('yield', m))
    for m in pattern_return.finditer(inner):
        matches.append(('return', m))
    matches.sort(key=lambda x: x[1].start())

    step = 1
    case_lines = [f"case 0: goto _colable_{fname}_0;"]
    for typ, m in matches:
        if typ in ('await', 'await_static', 'yield'):
            case_lines.append(f"case {step}: goto _colable_{fname}_{step};")
            step += 1

    out = []
    indent = "    "
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
    out.append(f"_colable_{fname}_0:\n")
    out.append(indent + "// 初始化协程对象\n")
    out.append(indent + "co->father = father;\n")
    out.append(indent + "if(father != NULL) co->father->son = co;\n")
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

    if enable_line:
        out.append(f'#line {body_start_line} "{source_file}"\n')

    last_end = 0
    step = 1
    for typ, m in matches:
        start, end = m.start(), m.end()
        out.append(inner[last_end:start])

        orig_indent = get_line_indent(inner, start)
        line_offset = inner[:start].count('\n')
        line_num = body_start_line + line_offset

        if enable_line:
            out.append(f'#line {line_num} "{source_file}"\n')

        if typ == 'yield':
            out.append("\n" + orig_indent + "/* BEGIN: 检测到 _yield 关键字，替换 */\n")
            out.append(orig_indent + "ctx_coro_wake(co, 0); // 0 代表 0 tick 后唤醒，也就是告诉调度器尽快唤醒\n")
            out.append(orig_indent + f"co->step = {step};\n")
            out.append(orig_indent + "return ; // 出让\n")
            out.append(f"_colable_{fname}_{step}:\n")
            out.append(orig_indent + "/* END: 检测到 _yield 关键字，替换 */\n")
            if enable_line:
                out.append(f'#line {line_num + 1} "{source_file}"\n')
            step += 1

        elif typ == 'await':
            left_expr = m.group(1)
            func = m.group(2)
            args = m.group(3)
            recv_var = None
            if left_expr:
                if '->' in left_expr:
                    recv_var = left_expr.split('->')[-1].strip().rstrip('=').strip()
                else:
                    recv_var = left_expr.strip().rstrip('=').strip()
            call_args = args if args else ""
            if call_args:
                call_str = f"_co_{func}(co, NULL, {call_args})"
            else:
                call_str = f"_co_{func}(co, NULL)"
            out.append("\n" + orig_indent + "/* BEGIN: 检测到 _await 关键字，替换 */\n")
            out.append(orig_indent + call_str + ";\n")
            out.append(orig_indent + f"co->step = {step};\n")
            out.append(orig_indent + "return ; // 出让\n")
            out.append(f"_colable_{fname}_{step}:\n")
            if recv_var:
                out.append(orig_indent + f"_prv_data->{recv_var} = ((struct _coval_{func} *)(co->son->prv_data))->_coretval_;\n")
            else:
                out.append(orig_indent + "/* 用户未接收返回值 */\n")
            out.append(orig_indent + "// free 子协程对象\n")
            out.append(orig_indent + "ctx_mem_data_free(co->son->prv_data);\n")
            out.append(orig_indent + "ctx_mem_free(co->son);\n")
            out.append(orig_indent + "// co->son = NULL;\n")
            out.append(orig_indent + "/* END: 检测到 _await 关键字，替换 */\n")
            if enable_line:
                out.append(f'#line {line_num + 1} "{source_file}"\n')
            step += 1

        elif typ == 'await_static':
            left_expr = m.group(1)
            obj_ptr = m.group(2).strip()
            func = m.group(3)
            args = m.group(4)
            recv_var = None
            if left_expr:
                if '->' in left_expr:
                    recv_var = left_expr.split('->')[-1].strip().rstrip('=').strip()
                else:
                    recv_var = left_expr.strip().rstrip('=').strip()
            if args:
                call_args = f"{obj_ptr}, {args}"
            else:
                call_args = obj_ptr
            out.append("\n" + orig_indent + "/* BEGIN: 检测到 _await_static 关键字，替换 */\n")
            out.append(orig_indent + f"_co_{func}(co, {call_args});\n")
            out.append(orig_indent + f"co->step = {step};\n")
            out.append(orig_indent + "return ; // 出让\n")
            out.append(f"_colable_{fname}_{step}:\n")
            if recv_var:
                out.append(orig_indent + f"_prv_data->{recv_var} = ((struct _coval_{func} *)(co->son->prv_data))->_coretval_;\n")
            else:
                out.append(orig_indent + "/* 用户未接收返回值 */\n")
            out.append(orig_indent + "/* END: 检测到 _await_static 关键字，替换 */\n")
            if enable_line:
                out.append(f'#line {line_num + 1} "{source_file}"\n')
            step += 1

        elif typ == 'return':
            expr = m.group(1).strip()
            if ret_type == 'void' or ret_type == '':
                out.append("\n" + orig_indent + "/* BEGIN: 检测到用户 return，替换 */\n")
                out.append(orig_indent + f"goto _colable_{fname}_end;\n")
                out.append(orig_indent + "/* END: 检测到用户 return，替换 */\n")
            else:
                assign = f"_prv_data->_coretval_ = {expr}; " if expr else ""
                out.append("\n" + orig_indent + "/* BEGIN: 检测到用户 return xxx; 替换 */\n")
                out.append(orig_indent + f"{assign}goto _colable_{fname}_end;\n")
                out.append(orig_indent + "/* END: 检测到用户 return xxx; 替换 */\n")
            if enable_line:
                out.append(f'#line {line_num + 1} "{source_file}"\n')

        last_end = end

    out.append(inner[last_end:])
    out.append(f"_colable_{fname}_end:\n")
    out.append(indent + "// 唤醒父协程\n")
    out.append(indent + "ctx_coro_wake(father, 0); // 0 代表 0 tick 后唤醒\n")
    out.append(indent + "co->step = 0; // 复位状态机\n")
    out.append(indent + "// free 操作交给父协程\n")
    out.append("}\n")

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
        toks = p.split()
        if len(toks) >= 2:
            res.append((' '.join(toks[:-1]), toks[-1]))
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

def process(input_file, enable_line):
    with open(input_file, 'r', encoding='utf-8') as f:
        source = f.read()
    funcs, tokens = extract_async_functions(source)

    if not funcs:
        # print("No _async functions detected. No files generated.")
        return

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
        impl_lines.append(" */")
        impl_lines.append("")

        impl_lines.append(f"void _cocb_{fn['name']}(struct coro_stu *co);")
        impl_lines.append("")

        impl_lines.append("// ======== 翻译后的函数体 ========")
        ret = 'void'
        param_str = fn['params']
        if param_str and param_str.strip() != 'void':
            new_params = f"struct coro_stu *father, struct coro_stu *co, {param_str}"
        else:
            new_params = "struct coro_stu *father, struct coro_stu *co"
        impl_lines.append(f"{ret} _co_{fn['name']}({new_params})")
        # 函数声明
        h_lines.append(f"void _co_{fn['name']}({new_params});\n")

        final_body = apply_state_machine(
            hoisted_body, fn['name'], fn['return_type'], param_names,
            body_start_line, enable_line, src_basename
        )
        impl_lines.append(final_body)
        impl_lines.append("")

        impl_lines.append(f"void _cocb_{fn['name']}(struct coro_stu *co) {{")
        if param_pairs:
            args = ', '.join([f"((struct {struct_name} *)co->prv_data)->{n}" for _, n in param_pairs])
            impl_lines.append(f"    _co_{fn['name']}(co->father, co, {args});")
        else:
            impl_lines.append(f"    _co_{fn['name']}(co->father, co);")
        impl_lines.append("}")

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
    if len(sys.argv) < 2:
        print("Usage: python async_extractor.py <input.c> [--line]")
        sys.exit(1)

    input_file = sys.argv[1]
    enable_line = '--line' in sys.argv
    process(input_file, enable_line)