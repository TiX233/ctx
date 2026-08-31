#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path


def run_translator(input_c: str):
    script = Path(__file__).resolve().parent.parent / 'ctx' / 'coro_translater.py'
    out = subprocess.run(
        [sys.executable, str(script), input_c, '--debug-scope-text', '-'],
        capture_output=True,
        text=True,
        encoding='utf-8',
        check=False,
    )
    if out.returncode != 0:
        raise RuntimeError(f"translator failed for {input_c}: {out.stderr.strip() or out.stdout.strip()}")
    return out.stdout.strip() + '\n'


def normalize_var_token(value: str):
    text = value.strip()
    if not text or text == '(无)':
        return ''
    tokens = text.split()
    return tokens[-1] if tokens else text


def parse_txt_report(text: str):
    lines = text.splitlines()
    result = {}
    current_func = None
    current_section = None
    current_var = None
    current_var_map = None

    def flush_var():
        nonlocal current_var, current_var_map
        if current_var is not None and current_var_map is not None and current_func is not None:
            result.setdefault(current_func, {}).setdefault(current_section, {})[current_var] = current_var_map
        current_var = None
        current_var_map = None

    for raw in lines:
        line = raw.rstrip()
        if not line.strip():
            continue
        if line.endswith(':') and not line.startswith('    '):
            current_func = line[:-1]
            result.setdefault(current_func, {})
            current_section = None
            flush_var()
            continue
        if line.strip().startswith('#'):
            current_section = None
            continue
        if line.startswith('    all_local_var:'):
            current_section = 'all_local_var'
            continue
        if line.startswith('    frame_var:'):
            current_section = 'frame_var'
            flush_var()
            continue
        if line.startswith('        ') and line.strip().endswith(':'):
            flush_var()
            name = line.strip()[:-1]
            current_var = name
            current_var_map = {}
            continue
        if line.startswith('            ') and current_var is not None:
            if line.strip().startswith('作用范围='):
                current_var_map['scope'] = line.strip().split('=', 1)[1]
            elif line.strip().startswith('跨越出让点='):
                current_var_map['cross_await'] = line.strip().split('=', 1)[1]
            continue
        if line.startswith('        ') and current_section == 'frame_var':
            text_value = line.strip()
            if text_value and text_value != '(无)':
                result.setdefault(current_func, {}).setdefault(current_section, {})[normalize_var_token(text_value)] = {'frame': 'yes'}
            continue

    flush_var()
    return result


def canonicalize_report(text: str):
    data = parse_txt_report(text)
    output = {}
    for func_name, sections in data.items():
        out_sections = {}
        for sec_name, items in sections.items():
            if sec_name == 'all_local_var':
                out_sections[sec_name] = {k: v for k, v in sorted(items.items())}
            else:
                out_sections[sec_name] = sorted(items.keys())
        output[func_name] = out_sections
    return output


def load_expected(path: Path):
    text = path.read_text(encoding='utf-8')
    return canonicalize_report(text)


def main():
    parser = argparse.ArgumentParser(description='Compare the local variable lifetime report with the golden txt oracle file.')
    parser.add_argument('source', help='Input C source file to analyze (example: tests/input1.c)')
    parser.add_argument('oracle', help='Golden text oracle file (example: tests/input1_local_var_check.txt)')
    args = parser.parse_args()

    actual = canonicalize_report(run_translator(args.source))
    expected = load_expected(Path(args.oracle))

    if actual == expected:
        print(f"PASS: {args.source} matches {args.oracle}")
        return 0

    print('FAIL: report mismatch')
    print('--- expected ---')
    print(json.dumps(expected, ensure_ascii=False, indent=2, sort_keys=True))
    print('--- actual ---')
    print(json.dumps(actual, ensure_ascii=False, indent=2, sort_keys=True))
    return 1


if __name__ == '__main__':
    raise SystemExit(main())
