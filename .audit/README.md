# `.audit/` — contract security audit tooling

Reusable tooling for auditing the WAX system contracts under the **WAX Contract Audit
Protocol (WCAP)**. Everything here is designed to run repeatedly, in CI, on every change —
not once at audit time.

## What is here

| Path | Purpose |
|---|---|
| `verify-hashes.sh` | Compares the WASM produced by a reproducible build against the code hash live on WAX mainnet. |
| `rules/antelope.yaml` | semgrep ruleset for Antelope/C++ contract idioms. |
| `rules/wcap-check.py` | Type-aware checks that regex cannot decide. |
| `rules/run-sweep.sh` | Runs both engines. |
| `rules/test-coverage.py` | Reports action-reference and guard-branch test coverage (the toolchain cannot instrument line/branch coverage). Advisory: always exits 0. |

## What is deliberately *not* here

Audit findings, threat models and triage records are **not** kept in this repository.
Publishing unremediated findings about the contract securing mainnet would be a disclosure
problem. They live in Jira (`WBP` project) and Confluence, and this repo references
security debt by tracker ID only.

`.gitignore` is configured so those artefacts cannot be committed here by accident.

## Why two analysis engines

They cover different things, and the split was forced by measurement rather than taste.

The first version of the ruleset was semgrep alone. Against this tree it produced **120
hits for the "action without an auth check" rule, none of them true**, and **9 hits for
the "dead unsigned comparison" rule, of which 6 were false**. Regex cannot tell a
`uint32_t` from an `int64_t`, and it cannot follow a header declaration to the `.cpp`
definition where `require_auth()` actually lives.

A ruleset that noisy gets muted within a week, and a muted rule protects nothing. So the
type-dependent classes moved into `wcap-check.py`, which resolves declared types and
follows one level of delegation from a wrapper action to its helper (`buyrambytes` →
`buyram`, `claimrewards` → `claim_producer_rewards`):

| check | before | after |
|---|---|---|
| action without auth | 120 hits, 0 true | 13 hits |
| dead unsigned bound | 9 hits, 3 true | 3 hits, 3 true |
| float in a sort comparator | 0 hits (missed the real one) | 1 hit, the real one |

**Precision is the whole point.** A rule that cannot be made precise does not belong in
the ruleset; write it up as a manual review step instead.

## Checks

Each carries a class ID used in audit reports.

**semgrep** (`rules/antelope.yaml`)

- **B1** — `on_notify` handler that never calls `get_first_receiver()`, so a forged
  notification from an arbitrary contract is indistinguishable from a real transfer.
- **B2** — transfer handler that does not test `to == get_self()`.
- **C2** — a rate applied as `x * rate / DENOMINATOR`, which can overflow the intermediate
  even when the result would fit.
- **D1** — unbounded iteration over a multi_index. In an action reached from `onblock`,
  this is a chain-halt risk once the table grows.
- **D2** — `emplace()` billing RAM to an account other than `get_self()`.

**type-aware** (`rules/wcap-check.py`)

- **A1** — action whose definition, and anything it delegates to, contains no
  `require_auth()`/`has_auth()`.
- **C4** — a sort comparator ordering on a `double`/`float` member. If that ordering
  reaches consensus, determinism depends on every node agreeing on floating-point
  evaluation.
- **C5** — `check(x >= 0)` where `x` is unsigned: always true, so the bound is unenforced
  and the error message misleads.
- **C4b** — a `check()` that has been commented out. A disabled assertion is a statement
  that the invariant does not hold.

The next five were added under WBP-1998 after the 2026-09 audit found that four of its first
seven findings had one shape — an action whose validation does not do what its error message
claims — and a fifth came from two sibling actions validating the same input differently.

- **C6** — the error message does not describe what the check enforces: strict wording on a
  non-strict operator (`"must be greater than 0"` on `>= 0`), a number in the message that is
  not the number in the comparison (`< 400` / `"shorter than 256"`), or "characters" counted
  on a vector.
- **C6-sibling** — a `reg*`/`edit*` pair carries two inline copies of the same validation.
  Reported at three levels: the copies already disagree (the weaker one is usually the
  finding), a message has drifted, or they still agree and should share one `validate_*`
  helper before they stop agreeing.
- **C6-setter** — a `set*` action stores a numeric parameter that no `check()` bounds.
- **C3** — an action takes an `asset` it never validates (`is_valid()` / sign of `.amount`),
  in its own body or in a helper it hands the asset to.
- **C2** (narrowing) — a product with a 64-bit operand assigned into a `uint32_t`.

Each of these was calibrated on both trees before it was kept: it fires on every known
instance at the audit base and goes quiet on every fixed one. `rules/test_wcap_check.py`
pins that behaviour with a positive and a negative case per check, and runs in CI.

## Usage

```bash
# full sweep (needs docker for semgrep)
.audit/rules/run-sweep.sh

# type-aware checks only, no docker needed
python3 .audit/rules/wcap-check.py contracts

# self-test of the type-aware checks (positive and negative case per check)
python3 .audit/rules/test_wcap_check.py

# deployed-vs-source verification, after `make dev-docker-all`
.audit/verify-hashes.sh build
```

`wcap-check.py` and `run-sweep.sh` exit non-zero when they report anything, so both work
as CI gates.

## Reproducible builds

`verify-hashes.sh` only means something because the build is reproducible: building this
repo inside the image pinned in the `Makefile` produces WASM whose SHA-256 can be compared
directly against `get_code_hash` on mainnet. That property is worth protecting — if a
toolchain change breaks it, the script stops being able to distinguish real drift from
build noise.

Run it quarterly, and whenever a system contract is deployed.
