# Threat model — <scope> @ <commit> (<yyyy-mm>)

Written in WCAP Phase 1, before any line-by-line review. Every fact about the chain below is
measured and dated, not assumed. This file is private to the audit repository.

## Assets

Numbered; each finding names the asset it threatens.

1. <asset> — <what holds it, which actions move it>
2. …

## Actors

| Actor | Capability | Trust assumed |
|---|---|---|
| Anonymous account | any action not gated by `require_auth`; RAM-payer griefing; sybil accounts | none |
| <role> | <actions> | <what the design trusts them with> |
| The contract's own account (`<account>`, held by <authority, measured on <date>>) | everything | the governance trust root |
| External contract `<name>` | <what of its state enters this contract's paths> | <how it is gated: allow-list, hash check, none> |

## Trust boundaries

Each is a mandatory review target in Phase 3.

- `<contract>` ⇄ `<contract>` — <inline actions, table reads, notifications crossing it; how it is gated>
- contract ⇄ host functions — <privileged intrinsics used>
- upgrade path — <who can `setcode`, through what quorum, verified how>

## Invariants

State each so that it can be tested; name the owner test or the reason it cannot be tested.

| | Invariant | Test |
|---|---|---|
| I1 | Conservation — … | `tests/…::…` |
| I2 | No double claim — … | |
| I3 | Schedule determinism — … | |
| I4 | Solvency — … | |
| I5 | Authority monotonicity — … | |
| I6 | Bounded cost — … | |
| I7 | Upgrade integrity — … | `.audit/verify-hashes.sh` |

## Chain facts this model depends on

Measured, with the query and the date, so a later reader can re-measure.

| Fact | Value | Measured | How |
|---|---|---|---|
| <e.g. active producer count> | | <date> | `get_producer_schedule` |
| <e.g. authority of the trust root> | | <date> | `get_account` |

## Out of scope, stated

- <what the audit will not look at, and why>
