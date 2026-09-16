#!/usr/bin/env python3
"""
WCAP v1 — type-aware Antelope checks.

The semgrep ruleset in antelope.yaml covers the classes that regex can decide on its
own. The checks here need something regex cannot do: resolve a variable's declared type,
or correlate a declaration in a header with its definition in a .cpp. Trying to express
them as regex rules produced a 100% false-positive rate on A1 and 6/9 on C5, so they
live here instead.

A ruleset earns its place by being precise. A noisy one gets muted, and a muted rule
protects nothing.

Usage:
    .audit/rules/wcap-check.py [contracts_dir]
    .audit/rules/wcap-check.py [contracts_dir] --baseline .audit/rules/baseline.txt
    .audit/rules/wcap-check.py [contracts_dir] --write-baseline .audit/rules/baseline.txt

Exit 1 if any finding is reported. With --baseline, exit 1 only for findings NOT in the
baseline, so CI stays green on a known backlog while catching anything new. That ratchet
matters: a gate that fails on every build from day one gets switched off, and then it
protects nothing.

The baseline keys on (file, class, symbol) rather than line number, so it survives
unrelated edits that shift lines around.
Ticket: WBP-1985
"""

import re
import sys
import pathlib

UNSIGNED = re.compile(r'\b(uint\d+_t|unsigned|size_t|name)\b')
SIGNED = re.compile(r'\b(int\d+_t|double|float|\bint\b|asset|time_point)\b')


def read(p):
    return p.read_text(errors='replace')


def params_of(text, fn_name):
    """Return the parameter list string of a system_contract::<fn> definition."""
    m = re.search(r'::' + re.escape(fn_name) + r'\s*\(([^)]*)\)', text)
    return m.group(1) if m else None


def param_type(params, var):
    """Declared type of `var` within a parameter list, or None."""
    if not params:
        return None
    for part in params.split(','):
        if re.search(r'\b' + re.escape(var) + r'\s*$', part.strip()):
            return part.strip()
    return None


def enclosing_fn(lines, idx):
    """Walk back to the nearest system_contract::<name>( definition."""
    for i in range(idx, -1, -1):
        m = re.search(r'system_contract::([a-z0-9_]+)\s*\(', lines[i])
        if m:
            return m.group(1), i
    return None, None


def check_c5(path, text):
    """C5 — dead lower-bound comparison on an unsigned type."""
    out = []
    lines = text.splitlines()
    for i, line in enumerate(lines):
        if re.match(r'\s*//', line):
            continue                                   # commented-out code is not a check
        m = re.search(r'check\s*\(\s*([A-Za-z_][A-Za-z0-9_.]*)\s*>=\s*0\b(?!\.)', line)
        if not m:
            continue
        var = m.group(1).split('.')[-1]
        fn, fidx = enclosing_fn(lines, i)
        decl = param_type(params_of(text, fn), var) if fn else None
        if decl is None:
            decl = ''
            for j in range(max(0, i - 40), i):
                d = re.search(r'\b((?:uint\d+_t|int\d+_t|size_t|unsigned|double|float)[\w\s:<>]*)\b' +
                              re.escape(var) + r'\b\s*[=;]', lines[j])
                if d:
                    decl = d.group(1)
        if decl and UNSIGNED.search(decl) and not SIGNED.search(decl):
            out.append((i + 1, 'C5',
                        f'`check({m.group(1)} >= 0)` is always true: {var} is `{decl.strip()}`. '
                        f'The lower bound is unenforced and the message misleads.'))
    return out


def has_auth_transitive(action, defs, depth=2, seen=None):
    """
    True if the action's body checks auth, or delegates to a helper that does.

    Many actions are thin wrappers - buyrambytes calls buyram, claimrewards calls
    claim_producer_rewards, delegatebw calls changebw - and the auth check lives one
    level down. Without following that edge the rule reports a false positive on every
    such wrapper, which is what the first version of this check did.
    """
    if seen is None:
        seen = set()
    if action in seen or depth < 0:
        return False
    seen.add(action)
    body = defs.get(action)
    if body is None:
        return False
    if re.search(r'require_auth|has_auth', body):
        return True
    for callee in set(re.findall(r'\b([a-z0-9_]{3,})\s*\(', body)):
        if callee in defs and callee != action:
            if has_auth_transitive(callee, defs, depth - 1, seen):
                return True
    return False


def check_a1(path, text, defs_index):
    """A1 - declared action whose definition (or its delegate) contains no auth check."""
    out = []
    lines = text.splitlines()
    for i, line in enumerate(lines):
        if '[[eosio::action]]' not in line:
            continue
        sig = ' '.join(lines[i + 1:i + 5])
        m = re.search(r'\b([a-z0-9_]+)\s*\(', sig)
        if not m:
            continue
        action = m.group(1)
        if action not in defs_index:
            out.append((i + 1, 'A1-native',
                        f'action `{action}` is declared with no contract-side definition '
                        f'(implemented by nodeos). Auth is enforced by the chain, not here '
                        f'- confirm that is intended.'))
        elif not has_auth_transitive(action, defs_index):
            out.append((i + 1, 'A1',
                        f'action `{action}` has no require_auth()/has_auth() in its '
                        f'definition or in anything it delegates to. If it mutates state, '
                        f'anyone can call it.'))
    return out


def check_c4(path, text):
    """C4 — a sort comparator ordering on a member declared double/float."""
    out = []
    fp_members = set(re.findall(r'\b(?:double|float)\s+([a-z0-9_]+)\s*[;=]', text))
    if not fp_members:
        return out
    for m in re.finditer(r'std::sort\s*\(', text):
        window = text[m.start():m.start() + 700]
        for member in fp_members:
            if re.search(r'\.\s*' + re.escape(member) + r'\b', window):
                line = text[:m.start()].count('\n') + 1
                out.append((line, 'C4',
                            f'sort comparator orders on `{member}`, declared floating-point. '
                            f'If this ordering reaches consensus, determinism depends on every '
                            f'node agreeing on FP evaluation (invariant I3).'))
                break
    return out


def check_c4b(path, text):
    """C4b — an invariant check on a float that has been commented out."""
    out = []
    for i, line in enumerate(text.splitlines()):
        if re.match(r'\s*//\s*check\s*\(', line):
            out.append((i + 1, 'C4b',
                        f'a check() has been commented out: {line.strip()[:90]}. '
                        f'A disabled invariant assertion is a statement that the invariant '
                        f'does not hold — confirm why.'))
    return out


def build_defs_index(root):
    """action name -> definition body, across every .cpp under root."""
    index = {}
    for cpp in pathlib.Path(root).rglob('*.cpp'):
        text = read(cpp)
        for m in re.finditer(r'\w[\w:<>]*\s+\w+::([a-z0-9_]+)\s*\([^)]*\)\s*\{', text):
            start = m.end() - 1
            depth, j = 0, start
            while j < len(text):
                if text[j] == '{':
                    depth += 1
                elif text[j] == '}':
                    depth -= 1
                    if depth == 0:
                        break
                j += 1
            index[m.group(1)] = text[start:j]
    return index


def key_of(path, cls, msg):
    """Stable identity for a finding: file + class + the symbol it names."""
    m = re.search(r'`([^`]+)`', msg)
    return f'{path}|{cls}|{m.group(1) if m else ""}'


def load_baseline(path):
    try:
        return {ln.split('#')[0].strip()
                for ln in pathlib.Path(path).read_text().splitlines()
                if ln.split('#')[0].strip()}
    except FileNotFoundError:
        return set()


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    flags = {sys.argv[i]: sys.argv[i + 1]
             for i, a in enumerate(sys.argv)
             if a.startswith('--') and i + 1 < len(sys.argv)}
    root = args[0] if args else 'contracts'
    defs = build_defs_index(root)
    findings = []
    for path in sorted(pathlib.Path(root).rglob('*')):
        if path.suffix not in ('.cpp', '.hpp') or 'test_contracts' in str(path):
            continue
        text = read(path)
        rows = check_c5(path, text) + check_c4(path, text) + check_c4b(path, text)
        if path.suffix == '.hpp':
            rows += check_a1(path, text, defs)
        findings += [(str(path), ln, cls, msg) for ln, cls, msg in rows]

    if '--write-baseline' in flags:
        target = flags['--write-baseline']
        lines = ['# WCAP baseline - findings accepted at the time this was written.',
                 '# Keyed on file|class|symbol so it survives line-number churn.',
                 '# Shrink this file; never grow it. Each removal is a fixed finding.',
                 '']
        lines += sorted({key_of(p, c, m) for p, _, c, m in findings})
        pathlib.Path(target).write_text('\n'.join(lines) + '\n')
        print(f'wrote {len(findings)} entries to {target}', file=sys.stderr)
        return 0

    baseline = load_baseline(flags['--baseline']) if '--baseline' in flags else set()
    new = [f for f in findings if key_of(f[0], f[2], f[3]) not in baseline]

    for path, ln, cls, msg in sorted(new if baseline else findings):
        print(f'{path}:{ln}: [WCAP {cls}] {msg}')

    if baseline:
        print(f'\n{len(findings)} finding(s), {len(findings) - len(new)} baselined, '
              f'{len(new)} new.', file=sys.stderr)
        return 1 if new else 0

    print(f'\n{len(findings)} finding(s).', file=sys.stderr)
    return 1 if findings else 0


if __name__ == '__main__':
    sys.exit(main())
