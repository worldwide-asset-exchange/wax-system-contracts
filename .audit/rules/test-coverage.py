#!/usr/bin/env python3
"""
WCAP test-coverage measurement for wax-system-contracts.

Line and branch coverage of the WASM cannot be instrumented in the pinned toolchain:
cdt-cpp 4.1.0 rejects -fprofile-instr-generate, -fcoverage-mapping and --coverage, and the
EOS VM has no host function through which an instrumented contract could write a profile
(see WBP-2001). What CAN be measured mechanically, without instrumentation, are two things
that matter for an audit:

  1. ACTION coverage - which ABI actions any test invokes at all.
  2. GUARD coverage  - which `check(cond, "message")` failure branches any test asserts,
     matched by the message string tests pass to wasm_assert_msg(). Every check() is a
     branch; the validation branches are where WCAP finds most of its defects.

Both are reproducible from the sources alone and are reported per contract. The method is
stated with the numbers so nobody mistakes them for line coverage.

Usage:
    .audit/rules/test-coverage.py                       # summary for eosio.system
    .audit/rules/test-coverage.py --list                # also list what is uncovered
    .audit/rules/test-coverage.py --abi build/contracts/eosio.system/eosio.system.abi
    .audit/rules/test-coverage.py --tests tests --tests ../other/tests

Exit code is always 0: this is a report, not a gate. Ticket: WBP-2001.
"""

import argparse
import glob
import json
import os
import re
import sys

CONTRACT_SRC = ["contracts/eosio.system/src/*.cpp", "contracts/eosio.system/include/eosio.system/*.hpp"]
DEFAULT_ABI  = "build/contracts/eosio.system/eosio.system.abi"

# Actions the contract declares but nodeos implements (native), the block hook, and the
# empty receipt emitters. They have no contract-side branch a test could reach.
NATIVE = {"newaccount", "setcode", "setabi", "canceldelay", "onerror", "updateauth", "deleteauth",
          "linkauth", "unlinkauth", "setparams", "activate", "wasmcfg", "onblock"}


def read_all(patterns):
    text = ""
    for pat in patterns:
        for path in sorted(glob.glob(pat)):
            with open(path, encoding="utf-8", errors="replace") as fh:
                text += fh.read() + "\n"
    return text


def abi_actions(path):
    if not os.path.exists(path):
        return None
    with open(path) as fh:
        return sorted(a["name"] for a in json.load(fh)["actions"])


def guard_messages(src):
    """Every literal message a check() can fail with. Messages built from a constant
    (`"prefix " + std::to_string(x)`) count by their literal prefix."""
    msgs = set()
    for m in re.finditer(r'check\s*\((?:[^;])*?,\s*"((?:[^"\\]|\\.)*)"\s*(?:\)|\+)', src, flags=re.S):
        msgs.add(m.group(1))
    return msgs


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--abi", default=DEFAULT_ABI)
    ap.add_argument("--tests", action="append", default=None, help="test directory (repeatable)")
    ap.add_argument("--list", action="store_true", help="list uncovered actions and guards")
    args = ap.parse_args()

    test_dirs = args.tests or ["tests"]
    tests = read_all([os.path.join(d, "*.cpp") for d in test_dirs] + [os.path.join(d, "*.hpp") for d in test_dirs])
    src = read_all(CONTRACT_SRC)

    actions = abi_actions(args.abi)
    if actions is None:
        print(f"ABI not found at {args.abi}; build the contract first (make) or pass --abi", file=sys.stderr)
        return 0

    def referenced(a):
        return re.search(r'"%s"_n\b|\bN\(%s\)' % (re.escape(a), re.escape(a)), tests) is not None

    logs = [a for a in actions if a.startswith("log")]
    contract_side = [a for a in actions if a not in NATIVE and a not in logs]
    ref_all = [a for a in actions if referenced(a)]
    ref_cs  = [a for a in contract_side if referenced(a)]

    msgs = guard_messages(src)
    asserted = sorted(m for m in msgs if m in tests)

    print("WCAP test coverage - eosio.system (method: action references and asserted guard messages; NOT line/branch coverage)")
    print(f"  actions in ABI                         {len(actions):4d}")
    print(f"  actions invoked by a test              {len(ref_all):4d}  ({100.0*len(ref_all)/len(actions):5.1f} %)")
    print(f"  contract-side actions (excl. {len(NATIVE & set(actions))} native/onblock, {len(logs)} log*)  {len(contract_side):4d}")
    print(f"  contract-side actions invoked          {len(ref_cs):4d}  ({100.0*len(ref_cs)/len(contract_side):5.1f} %)")
    print(f"  guard messages (check() branches)      {len(msgs):4d}")
    print(f"  guard messages asserted by a test      {len(asserted):4d}  ({100.0*len(asserted)/len(msgs):5.1f} %)")
    if args.list:
        print("\n  contract-side actions with no test reference:")
        for a in contract_side:
            if a not in ref_cs:
                print(f"    {a}")
        print("\n  guard messages no test asserts:")
        for m in sorted(msgs - set(asserted)):
            print(f"    {m}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
