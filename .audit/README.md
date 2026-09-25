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
| `report/` | The audit report PDF pipeline: data in, WAX-branded PDF out. Template, theme, fonts and non-sensitive metadata only — see [`report/README.md`](report/README.md). |

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

## Deployed-contract provenance (WCAP I7, WBP-1991)

`verify-hashes.sh` compares the *current* source with what is deployed. When they differ,
`reproduce-deployed.sh <ref> <image>` finds which tag, built in which toolchain, matches —
so the audit can say for every account which source it reviewed and whether that source is
the one on chain. Result of the 2026-09 forensics (mainnet `last_code_update` in the
second column):

| account | deployed | reproduced from | toolchain image | match |
|---|---|---|---|---|
| `eosio` | 2026-09-25 | `wax-3.3.2` (`d12f261`) — the 2026-09 review's remediations; replaced `wax-3.3.0` (`715ddba`, deployed 2026-02-17), which was reproduced the same way | `waxteam/waxdev:v5.0.3wax02-v4.1.0` (cdt 4.1.0) | **yes**, bit for bit (`f5b832cd…`) |
| `eosio.token` | 2019-12-10 | source identical to `wax-1.7.0-2.0.0` (`318dc57`, tagged the day after the deployment) | `waxteam/dev:wax-1.6.1-1.2.1` (eosio.cdt 1.6.1) | **yes**, bit for bit |
| `eosio.wrap` | 2019-12-10 | source identical to `wax-1.7.0-2.0.0` (`318dc57`) | `waxteam/dev:wax-1.6.1-1.2.1` (eosio.cdt 1.6.1) | **yes**, bit for bit |
| `eosio.msig` | 2022-12-08 | `EOSIO/eosio.contracts` branch `1.8.3-oob-patch` (`301c901`, 2020-09-14) | official `eosio.cdt` 1.6.3 package on Ubuntu 18.04, via the CDT cmake toolchain | **yes**, bit for bit |

The three in-repo rows are re-runnable with `reproduce-deployed.sh <ref> <image>` using the
ref and image columns as written (the older image also needs `-Dcdt_DIR=…`, see the script
header); the `eosio.msig` row uses the recipe below. The source for
`eosio.token` and `eosio.wrap` in this repository has changed since the deployed tag only
cosmetically (doc comments, a redundant `sym.is_valid()` removed because `asset::is_valid()`
already checks the symbol, and two error messages added to `get()` lookups); the code on
chain is therefore older than, but not weaker than, the code reviewed.

`eosio.msig` is the one contract not built from a commit of this repository. It is EOSIO's
out-of-band 1.8.3 patch (inline `exec`, `earliest_exec_time`), which this repository ported
onto the CDT 3 tree in October 2022 (`c3db26f`…`83985e6`). The port is the same code: the
only differences are the patch's hand-rolled `eosio_msig_binary_extension` class, replaced
by the library's `eosio::binary_extension`, and `std::optional` assignments replaced by
`emplace()`. Two details mattered for the reproduction and are easy to get wrong:

- the compiler generation is visible in the deployed ABI's `version` field (`eosio::abi/1.1`
  is eosio.cdt 1.6–1.7; `eosio::abi/1.2` is 1.8 and cdt 3+), which rules toolchains in or
  out before any build;
- `eosio-cpp` on the command line and `add_contract()` through the CDT cmake toolchain do
  not produce the same bytes (1.6.3 gives 36,489 vs 36,978), so reproduce the way the
  release was built.

Recipe for the msig row, from a stock container, no WAX image needed:

```bash
# source: EOSIO/eosio.contracts @ 301c901, contracts/eosio.msig; toolchain: eosio.cdt 1.6.3
gh release download v1.6.3 --repo EOSIO/eosio.cdt --pattern '*ubuntu-18.04_amd64.deb'
docker run --rm -v "$PWD":/w ubuntu:18.04 bash -c '
  apt-get update -qq && apt-get install -y -qq cmake make /w/eosio.cdt_1.6.3-1-ubuntu-18.04_amd64.deb
  mkdir /b && cd /b && cmake -DCMAKE_TOOLCHAIN_FILE=/usr/opt/eosio.cdt/1.6.3/lib/cmake/eosio.cdt/EosioWasmToolchain.cmake /w/eosio.msig
  make && sha256sum eosio.msig.wasm'      # 055f1cd3bffc3262ccd40a0acd665a0a62e4a7cc48f34f6fc54aa74928cbac13
```

where `/w/eosio.msig` holds that branch's `src/`, `include/` and a `CMakeLists.txt` of
`find_package(eosio.cdt)` + `add_contract(eosio.msig eosio.msig src/eosio.msig.cpp)` with
`target_include_directories(eosio.msig.wasm PUBLIC include)`.

What this buys the audit: every finding against `eosio.system` applies to live code with no
version caveat, and for the three older contracts the reviewer knows exactly which source
is live and that the source in this repository is the same code, modulo the cosmetic drift
listed above. Re-run the table after every deployment (WCAP Phase 7).

## Reproducible builds

`verify-hashes.sh` only means something because the build is reproducible: building this
repo inside the image pinned in the `Makefile` produces WASM whose SHA-256 can be compared
directly against `get_code_hash` on mainnet. That property is worth protecting — if a
toolchain change breaks it, the script stops being able to distinguish real drift from
build noise.

Run it quarterly, and whenever a system contract is deployed.
