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

Classes C6 (message/check mismatch), C6-sibling (duplicated validation that can drift),
C6-setter (privileged numeric parameter stored unvalidated), C3 (asset parameter never
validated) and C2 (64-bit product narrowed into uint32_t) were added under WBP-1998 after
the 2026-09 audit found that four of its first seven findings had one shape: an action
whose validation does not do what its error message claims. Self-test:
    python3 .audit/rules/test_wcap_check.py
Tickets: WBP-1985, WBP-1998
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


# ----------------------------------------------------------------------------- WBP-1998
# The validation/message defect class. Four of the first seven 2026-09 findings shared
# one shape - an action whose check() does not enforce what its message promises - and a
# fifth came from two sibling actions validating the same input differently. These checks
# make that class mechanical. They are deliberately narrow: each sub-pattern below was
# calibrated against 715ddba (fires on every known instance) and against the remediated
# tree (goes quiet on every fixed one) before the false-positive shapes seen on the way
# were excluded by rule, not by baseline.

MODAL_POS = re.compile(r"\b(must|should|needs? to|has to|have to|required to|is required to)\b", re.I)
MODAL_NEG = re.compile(r"\b(cannot|can't|can not|may not|must not|should not|shouldn't|mustn't)\b", re.I)
STRICT_WORDS = re.compile(r"\b(greater than|more than|longer than|larger than|bigger than|above|over|"
                          r"exceed(?:s|ed)?|less than|fewer than|shorter than|smaller than|lower than|"
                          r"below|under)\b", re.I)
NONSTRICT_WORDS = re.compile(r"\b(at least|at most|or more|or less|or fewer|or equal|equal to or|"
                             r"no more than|no less than|no fewer than|up to|(?:a )?(?:minimum|maximum) of)\b", re.I)
# "cannot claim until the chain is activated (at least 15% ...)": what follows until/unless
# is the passing condition even though the sentence is negated.
UNTIL = re.compile(r"\b(until|unless)\b", re.I)
SIMPLE_CMP = re.compile(r'^\s*([A-Za-z_][A-Za-z0-9_.()>-]*)\s*(>=|<=|>|<)\s*(-?\d+)\s*$')
NUM_LITERAL = re.compile(r'(?<![\w.])-?\d+(?![\w.])')
# A number followed by a time unit or a percent sign is a conversion of the bound, not the
# bound itself ("at least 600 (10 minutes)", "more than 100%"), so it does not count.
CONVERTED_NUM = re.compile(r'-?\d+\s*(?:%|percent|second|minute|hour|day|week|month|year)s?\b', re.I)


def _block_end(text, start):
    """Index of the brace closing the block opened at text[start], skipping string and
    char literals so a brace inside a message cannot merge two bodies."""
    depth, j = 0, start
    while j < len(text):
        c = text[j]
        if c in ('"', "'"):
            q, j = c, j + 1
            while j < len(text) and text[j] != q:
                j += 2 if text[j] == '\\' else 1
        elif c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                return j
        j += 1
    return j


def split_args(s):
    """Split an argument list on top-level commas only."""
    out, depth, cur = [], 0, ''
    for c in s:
        if c in '(<[':
            depth += 1
        elif c in ')>]':
            depth -= 1
        if c == ',' and depth == 0:
            out.append(cur.strip()); cur = ''
        else:
            cur += c
    if cur.strip():
        out.append(cur.strip())
    return out


def fn_bodies(text):
    """(name, params, body, first_line) for every `<type> <class>::<fn>(...) {`.

    The parameter list may contain one level of nested parentheses (a std::function
    type, a default argument); the body walk skips string literals.
    """
    out = []
    for m in re.finditer(r'\w[\w:<>]*\s+\w+::([a-z0-9_]+)\s*\(((?:[^()]|\([^()]*\))*)\)\s*(?:const\s*)?\{', text):
        start = m.end() - 1
        out.append((m.group(1), m.group(2), text[start:_block_end(text, start)],
                    text[:m.start()].count('\n') + 1))
    return out


def strip_comments(text):
    """Blank out comments but keep newlines, so line numbers survive."""
    text = re.sub(r'/\*.*?\*/', lambda m: re.sub(r'[^\n]', ' ', m.group(0)), text, flags=re.S)
    return re.sub(r'//[^\n]*', '', text)


def checks_in(body, first_line):
    """(line, condition, message) for each check(cond, "msg" ...) in a function body.

    The message is the first string literal after the condition, so a concatenated
    message (`"cannot exceed " + std::to_string(x)`) still yields its literal prefix.
    """
    out = []
    for m in re.finditer(r'\bcheck\s*\(([^;"]*?),\s*"((?:[^"\\]|\\.)*)"', body, re.S):
        cond = ' '.join(m.group(1).split())
        if cond.count('(') != cond.count(')'):
            continue                                   # the comma was inside a nested call
        out.append((first_line + body[:m.start()].count('\n'), cond, m.group(2)))
    return out


def param_names(params):
    return [p.strip().split()[-1].lstrip('&*') for p in params.split(',') if p.strip()]


def check_c6(path, text):
    """C6 - the error message does not describe what the check enforces.

    Three sub-patterns, all on a single relational check(cond, "msg"):
      (a) strictness: the message's wording is strict ("greater than") but the operator is
          not (>=), or the reverse. A requirement-form message ("x must be ...", "... until
          x is ...") describes the passing side; a negated one ("x cannot exceed ...")
          describes the failing side, whose strictness is the complement of the operator's.
          Messages that say both ("less than or equal to"), neither, or that merely
          describe the failure ("memo has more than 256 bytes") are left alone.
      (b) literal: the condition compares against a number and the message names a
          different number and not that one ("< 400" / "shorter than 256").
      (c) unit: the message counts "characters" but the container is a vector.
    """
    out = []
    for fn, params, body, first in fn_bodies(text):
        types = {n: (param_type(params, n) or '') for n in param_names(params)}
        for line, cond, msg in checks_in(body, first):
            ops = re.findall(r'(>=|<=|>|<)(?!=)', re.sub(r'->|[=!]=|<<|>>', ' ', cond))
            if len(ops) != 1 or '&&' in cond or '||' in cond:
                continue
            op = ops[0]
            strict_op = op in ('>', '<')
            strict_w, nonstrict_w = STRICT_WORDS.search(msg), NONSTRICT_WORDS.search(msg)

            simple = SIMPLE_CMP.match(cond)
            if simple:
                lit = int(simple.group(3))
                msg_nums = {int(n) for n in NUM_LITERAL.findall(CONVERTED_NUM.sub(' ', msg))}
                # "> 0" / "at least 1" and "< 256" / "at most 255" say the same thing.
                consistent = {lit, lit + 1} if op == '>' else {lit, lit - 1} if op == '<' else {lit}
                if msg_nums and not msg_nums & consistent:
                    out.append((line, 'C6',
                                f'`{fn}`: `check({cond}, "{msg}")` — the message names '
                                f'{sorted(msg_nums)} but the check compares against {lit}.'))
                    continue
                if msg_nums and lit not in msg_nums:
                    continue    # the message restated the bound in the other form ("> 0" as
                                # "at least 1"); its wording is judged against that number

            if strict_w and not nonstrict_w or nonstrict_w and not strict_w:
                wording_strict = bool(strict_w)
                if UNTIL.search(msg):
                    describes_pass = True
                elif MODAL_NEG.search(msg):
                    describes_pass = False
                elif MODAL_POS.search(msg):
                    describes_pass = True
                else:
                    continue    # "memo has more than 256 bytes": a loose failure description,
                                # not a stated bound. Calibration showed these are never the
                                # defect and flagging them would cost the rule its precision.
                expected_strict = strict_op if describes_pass else not strict_op
                if wording_strict != expected_strict:
                    word = (strict_w or nonstrict_w).group(0)
                    side = 'passing' if describes_pass else 'failing'
                    out.append((line, 'C6',
                                f'`{fn}`: `check({cond}, "{msg}")` — the message says "{word}" '
                                f'of the {side} side but the operator is {op}, so the bound '
                                f'enforced is not the bound promised.'))
                    continue

            m = re.search(r'\b([a-z_][a-z0-9_]*)\.size\(\)', cond)
            if m and re.search(r'\bcharacters?\b', msg) and 'vector' in types.get(m.group(1), ''):
                out.append((line, 'C6',
                            f'`{fn}`: `check({cond}, "{msg}")` — the message counts '
                            f'"characters" but {m.group(1)} is a vector, so the bound is on entries.'))
    return out


def check_c6_sibling(path, text):
    """C6-sibling - a reg*/edit* pair validates the same inputs with two inline copies.

    Two copies of the same checks are two places for the next edit to miss one. This is
    the shape of WCAP-SYS-2026-003 (editproposal lost the floor regproposal kept) and the
    sibling-comparison step in WCAP Phase 3. Reported at three levels: the copies already
    disagree on a check; they agree on the checks but a message has drifted; or they
    still agree - which is a warning, not a finding, and belongs in the baseline until the
    pair shares one validate_* helper.
    """
    out = []
    fns = {fn: (params, body, first) for fn, params, body, first in fn_bodies(text)}

    def param_checks(name):
        params, body, first = fns[name]
        names = set(param_names(params))
        rows = set()
        for _, cond, msg in checks_in(body, first):
            idents = set(re.findall(r'[A-Za-z_][A-Za-z0-9_]*', cond))
            if idents & names and not re.search(r'\.end\(\)|\bitr\b|_itr\b', cond):
                rows.add((cond, msg))
        return rows

    for fn in fns:
        m = re.match(r'reg([a-z0-9_]+)$', fn)
        if not m:
            continue
        sib = next((s for s in (f'edit{m.group(1)}', f'ed{m.group(1)}', f'upd{m.group(1)}',
                                f'update{m.group(1)}') if s in fns), None)
        if not sib:
            continue
        a, b = param_checks(fn), param_checks(sib)
        if len(a) < 3 and len(b) < 3:
            continue
        conds_a, conds_b = {c for c, _ in a}, {c for c, _ in b}
        only = sorted(conds_a ^ conds_b)
        msg_drift = sorted(f'{c}: "{ma}" vs "{mb}"' for c, ma in a for c2, mb in b
                           if c == c2 and ma != mb)
        first = fns[fn][2]
        if only:
            out.append((first, 'C6-sibling',
                        f'`{fn}`/`{sib}` validate the same inputs but one side lacks: '
                        f'{"; ".join(only)[:240]}. The weaker copy is usually the finding.'))
        elif msg_drift:
            out.append((first, 'C6-sibling',
                        f'`{fn}`/`{sib}` carry {len(a)} duplicated checks and a message has '
                        f'already drifted: {"; ".join(msg_drift)[:240]}. Share one helper.'))
        else:
            out.append((first, 'C6-sibling',
                        f'`{fn}`/`{sib}` carry {len(a)} duplicated inline checks. They agree today; '
                        f'move them into one validate_* helper so they cannot drift.'))
    return out


def check_c6_setter(path, text):
    """C6-setter - a set* action stores a numeric parameter that nothing bounds.

    WCAP-SYS-2026-005: setbpdefscore stored an arbitrary uint32 that reweighted the
    election, while its sibling setbpscale validated. A privileged setter without a
    check() on the value is not a vulnerability by itself - the msig can still be
    trusted - but it is the shape, and the message that would have caught it is the
    check that is missing.
    """
    out = []
    for fn, params, body, first in fn_bodies(text):
        if not fn.startswith('set'):
            continue
        for p in (x.strip() for x in params.split(',') if x.strip()):
            if not re.search(r'\b(u?int\d+_t|double|float)\b', p):
                continue
            name = p.split()[-1].lstrip('&*')
            if any(re.search(r'\b' + re.escape(name) + r'\b', cond) for _, cond, _ in checks_in(body, first)):
                continue
            if not re.search(r'(?<![=!<>])=\s*[^;=]*\b' + re.escape(name) + r'\b[^;]*;', body):
                continue
            out.append((first, 'C6-setter',
                        f'`{fn}` stores `{name}` (`{p}`) into state with no check() on it. '
                        f'Bound it, or record why any value is safe.'))
    return out


def _validates_asset(body, name, defs_index, depth=1):
    """True if body validates asset `name` itself or hands it to a helper that does."""
    if re.search(re.escape(name) + r'\.(is_valid\(\)|amount\s*[<>=!])', body):
        return True
    if re.search(r'\b' + re.escape(name) + r'\s*(>|<|>=|<=)\s*(zero|asset\s*\(\s*0)', body):
        return True
    if depth <= 0:
        return False
    for call in re.finditer(r'\b([a-z0-9_]{3,})\s*\(([^;{]*?)\)\s*;', body, re.S):
        callee, args = call.group(1), split_args(call.group(2))
        if callee not in defs_index or name not in args:
            continue
        cparams, cbody = defs_index[callee]
        cnames = param_names(cparams)
        pos = args.index(name)
        if pos < len(cnames) and _validates_asset(cbody, cnames[pos], defs_index, depth - 1):
            return True
    return False


def check_c3(path, text, actions, defs_index):
    # a helper defined in this file shadows a same-named one elsewhere (setparams, exec, ...)
    defs_index = {**defs_index, **{fn: (params, body) for fn, params, body, _ in fn_bodies(text)}}
    """C3 - an action takes an asset it never validates.

    WCAP-SYS-2026-004: removerefund accepted a negative asset and every guard downstream
    passed in the wrong direction. An action-level asset parameter must be checked for
    is_valid() and for the sign of .amount, in the action or in a helper it hands the
    asset to. Receipt emitters (log*) are excluded: they record, they do not decide.
    """
    out = []
    for fn, params, body, first in fn_bodies(text):
        if fn not in actions or fn.startswith('log'):
            continue
        for p in (x.strip() for x in params.split(',') if x.strip()):
            if not re.search(r'\basset\b', p):
                continue
            name = p.split()[-1].lstrip('&*')
            if not re.search(r'\b' + re.escape(name) + r'\b', body):
                continue
            if _validates_asset(body, name, defs_index):
                continue
            out.append((first, 'C3',
                        f'`{fn}` uses asset `{name}` without checking is_valid() or the sign of '
                        f'.amount, here or in a helper it passes it to. A negative quantity '
                        f'inverts every comparison downstream.'))
    return out


def check_c2_narrowing(path, text, wide_members):
    """C2 - a product with a 64-bit operand is assigned into a uint32_t.

    WCAP-SYS-2026-010: claimfunds computed `duration * seconds_per_day` (uint64 * const)
    into a uint32_t, and the truncated value set the payout cadence. The narrowing is
    silent in C++ and invisible to a reviewer who reads the right-hand side alone.
    """
    out = []
    for i, line in enumerate(text.splitlines(), 1):
        m = re.match(r'\s*uint32_t\s+([a-z_][a-z0-9_]*)\s*=\s*([^;]*[\w)]\s*\*\s*[\w(][^;]*);', line)
        if not m:
            continue
        operands = re.findall(r'[A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z_][A-Za-z0-9_]*)*', m.group(2))
        wide = [o for o in operands if o.split('.')[-1] in wide_members]
        if wide:
            out.append((i, 'C2',
                        f'`{m.group(1)}` is uint32_t but `{m.group(2).strip()}` multiplies a '
                        f'64-bit operand ({", ".join(wide)}); the product is truncated silently.'))
    return out


def build_hpp_index(root):
    """(declared action names, member names declared ONLY with a 64-bit type) across every .hpp.

    Member types are resolved by name, not by struct, so a name that is 64-bit in one
    struct and narrower in another is dropped rather than guessed.
    """
    actions, wide, narrow = set(), set(), set()
    for hpp in pathlib.Path(root).rglob('*.hpp'):
        if 'test_contracts' in str(hpp):
            continue
        text = read(hpp)
        lines = text.splitlines()
        for i, line in enumerate(lines):
            if '[[eosio::action]]' in line:
                m = re.search(r'\b([a-z0-9_]+)\s*\(', ' '.join(lines[i + 1:i + 5]))
                if m:
                    actions.add(m.group(1))
        wide |= set(re.findall(r'\b(?:u?int64_t|u?int128_t)\s+([a-z_][a-z0-9_]*)\s*[;=]', text))
        narrow |= set(re.findall(r'\b(?:u?int(?:8|16|32)_t|bool|float)\s+([a-z_][a-z0-9_]*)\s*[;=]', text))
    return actions, wide - narrow


def build_defs_with_params(root):
    """function name -> (params, body) for every definition under root."""
    index = {}
    for cpp in pathlib.Path(root).rglob('*.cpp'):
        if 'test_contracts' in str(cpp):
            continue
        for fn, params, body, _ in fn_bodies(strip_comments(read(cpp))):
            index[fn] = (params, body)
    return index


# Classes whose message names two things: the function and the specific check, parameter,
# asset or expression. Keying on the function alone would let a second defect in the same
# function hide behind the first one's baseline line.
TWO_PART_KEY = {'C6', 'C6-sibling', 'C6-setter', 'C3', 'C2'}


def key_of(path, cls, msg):
    """Stable identity for a finding: file + class + the symbol(s) it names."""
    parts = re.findall(r'`([^`]+)`', msg)
    n = 2 if cls in TWO_PART_KEY else 1
    return f'{path}|{cls}|' + '/'.join(parts[:n])


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
    actions, wide_members = build_hpp_index(root)
    defs_with_params = build_defs_with_params(root)
    findings = []
    for path in sorted(pathlib.Path(root).rglob('*')):
        if path.suffix not in ('.cpp', '.hpp') or 'test_contracts' in str(path):
            continue
        text = read(path)
        rows = check_c5(path, text) + check_c4(path, text) + check_c4b(path, text)
        if path.suffix == '.hpp':
            rows += check_a1(path, text, defs)
        if path.suffix == '.cpp':
            clean = strip_comments(text)               # C4b needs comments; these do not
            rows += (check_c6(path, clean) + check_c6_sibling(path, clean)
                     + check_c6_setter(path, clean) + check_c3(path, clean, actions, defs_with_params)
                     + check_c2_narrowing(path, clean, wide_members))
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
