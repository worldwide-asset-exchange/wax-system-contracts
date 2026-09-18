#!/usr/bin/env python3
"""
WCAP test-coverage measurement for wax-system-contracts.

Line and branch coverage of the WASM cannot be instrumented in the pinned toolchain:
cdt-cpp 4.1.0 rejects -fprofile-instr-generate, -fcoverage-mapping and --coverage, and the
EOS VM has no host function through which an instrumented contract could write a profile
(see WBP-2001). What CAN be measured mechanically, without instrumentation, are two things
that matter for an audit:

  1. ACTION coverage - which ABI actions the test sources reference (`"name"_n` or
     `N(name)`), fixture wrappers included. "Referenced" is the honest word: a wrapper
     nobody calls still counts. Comments are stripped first.
  2. GUARD coverage  - which `check(cond, "message")` failure branches a test asserts.
     A guard counts as asserted only when its message appears in an assertion form:
     wasm_assert_msg("..."), eosio_assert_message_is("...") or
     "assertion failure with message: ...". Comments are stripped from both sides;
     commented-out check() calls are not guards.

Actions nodeos implements (declared in native.hpp) and the block hook are excluded from
the contract-side denominator, as are the empty log* receipt emitters. Everything else
that is declared is counted, tested or not.

Usage (from anywhere; paths resolve from the repository root):
    .audit/rules/test-coverage.py                       # summary for eosio.system
    .audit/rules/test-coverage.py --list                # also list what is uncovered
    .audit/rules/test-coverage.py --abi path/to/eosio.system.abi
    .audit/rules/test-coverage.py --tests tests --tests ../other/tests

Exit code is always 0: this is a report, not a gate. Ticket: WBP-2001.
"""

import argparse
import glob
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CONTRACT_SRC = ["contracts/eosio.system/src/*.cpp", "contracts/eosio.system/include/eosio.system/*.hpp"]
NATIVE_HPP   = "contracts/eosio.system/include/eosio.system/native.hpp"
DEFAULT_ABI  = "build/contracts/eosio.system/eosio.system.abi"
MIN_PREFIX   = 12   # a concatenated message counts by its literal prefix only if the prefix is distinctive


def strip_comments(text):
    text = re.sub(r'/\*.*?\*/', ' ', text, flags=re.S)
    return re.sub(r'//[^\n]*', '', text)


def read_all(patterns):
    text = ""
    for pat in patterns:
        for path in sorted(glob.glob(os.path.join(ROOT, pat))):
            with open(path, encoding="utf-8", errors="replace") as fh:
                text += fh.read() + "\n"
    return strip_comments(text)


def abi_actions(path):
    if not os.path.isabs(path):
        path = os.path.join(ROOT, path)
    if not os.path.exists(path):
        return None
    with open(path) as fh:
        return sorted(a["name"] for a in json.load(fh)["actions"])


def native_actions():
    """Actions declared in native.hpp are implemented by nodeos, not by the contract."""
    try:
        with open(os.path.join(ROOT, NATIVE_HPP), encoding="utf-8") as fh:
            src = strip_comments(fh.read())
    except OSError:
        return set()
    return set(re.findall(r'\[\[eosio::action\]\]\s*(?:void|[\w:<>]+)\s+(\w+)\s*\(', src)) | {"onblock"}


def guard_messages(src):
    """Every message a check() can fail with, taken from the last top-level argument.
    A message built from a constant (`"prefix " + std::to_string(x)`) counts by its
    literal prefix when the prefix is distinctive enough to match on."""
    msgs = {}
    for m in re.finditer(r'\bcheck\s*\(', src):
        i = m.end(); depth = 1; start = i
        while i < len(src) and depth:
            c = src[i]
            if c == '"':
                i += 1
                while i < len(src) and src[i] != '"':
                    i += 2 if src[i] == '\\' else 1
            elif c == '(':
                depth += 1
            elif c == ')':
                depth -= 1
            i += 1
        args = src[start:i - 1]
        # last top-level comma
        depth = 0; last = -1; j = 0
        while j < len(args):
            c = args[j]
            if c == '"':
                j += 1
                while j < len(args) and args[j] != '"':
                    j += 2 if args[j] == '\\' else 1
            elif c in '([':
                depth += 1
            elif c in ')]':
                depth -= 1
            elif c == ',' and depth == 0:
                last = j
            j += 1
        if last < 0:
            continue
        msg_expr = args[last + 1:].strip().lstrip('(').strip()
        lit = re.match(r'"((?:[^"\\]|\\.)*)"\s*(\+)?', msg_expr)
        if not lit:
            continue
        text, concatenated = lit.group(1), lit.group(2) is not None
        if concatenated and len(text) < MIN_PREFIX:
            continue
        msgs[text] = concatenated
    return msgs


def asserted(msg, prefix_only, tests):
    q = re.escape(msg)
    tail = '' if prefix_only else '"'
    pat = r'(?:wasm_assert_msg|eosio_assert_message_is)\s*\(\s*"%s%s|assertion failure with message: %s%s' % (q, tail, q, tail)
    return re.search(pat, tests) is not None


def pct(n, d):
    return "%5.1f %%" % (100.0 * n / d) if d else "  n/a"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--abi", default=DEFAULT_ABI)
    ap.add_argument("--tests", action="append", default=None, help="test directory, relative to the repo root (repeatable)")
    ap.add_argument("--list", action="store_true", help="list uncovered actions and guards")
    args = ap.parse_args()

    test_dirs = args.tests or ["tests"]
    tests = read_all([os.path.join(d, "*.cpp") for d in test_dirs] + [os.path.join(d, "*.hpp") for d in test_dirs])
    src = read_all(CONTRACT_SRC)
    if not src.strip() or not tests.strip():
        print(f"no contract or test sources found under {ROOT}", file=sys.stderr)
        return 0
    actions = abi_actions(args.abi)
    if actions is None:
        print(f"ABI not found at {args.abi}; build the contract first (make) or pass --abi", file=sys.stderr)
        return 0

    native = native_actions() & set(actions)
    logs = [a for a in actions if a.startswith("log")]
    contract_side = [a for a in actions if a not in native and a not in logs]

    def referenced(a):
        return re.search(r'"%s"_n\b|\bN\(%s\)' % (re.escape(a), re.escape(a)), tests) is not None

    ref_all = [a for a in actions if referenced(a)]
    ref_cs  = [a for a in contract_side if referenced(a)]
    msgs = guard_messages(src)
    hit = sorted(m for m, pre in msgs.items() if asserted(m, pre, tests))

    print("WCAP test coverage - eosio.system")
    print("  method: action references in tests/ (fixture wrappers included) and check() guards asserted")
    print("  by message; NOT line or branch coverage, which this toolchain cannot instrument (WBP-2001)")
    print(f"  actions in ABI                              {len(actions):4d}")
    print(f"  actions referenced by a test                {len(ref_all):4d}  ({pct(len(ref_all), len(actions))})")
    print(f"  contract-side actions (excl. {len(native)} native/onblock, {len(logs)} log*) {len(contract_side):4d}")
    print(f"  contract-side actions referenced            {len(ref_cs):4d}  ({pct(len(ref_cs), len(contract_side))})")
    print(f"  guard messages (check() failure branches)   {len(msgs):4d}")
    print(f"  guards asserted by a test                   {len(hit):4d}  ({pct(len(hit), len(msgs))})")
    if args.list:
        print("\n  contract-side actions with no test reference:")
        for a in contract_side:
            if a not in ref_cs:
                print(f"    {a}")
        print("\n  guards no test asserts:")
        for m in sorted(set(msgs) - set(hit)):
            print(f"    {m}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
