---
content_title: Runbook — RNG payouts
link_text: Runbook — RNG payouts
---

# Runbook: changing the RNG treasury carve-out

Actions: `setrngrate(rng_rate, max_pool_rng)` and the allow-list `setornghash(hashes)`,
`addornghash(hash)`, `rmornghash(hash)` — all `eosio` msig. Tables: `global5` (rate and cap),
`global.b` (hash allow-list), and, read-only from the ORNG side, the `treasury` singleton in
`orng.wax`. Read the [overview](09_runbook-tokenomics-parameters.md) first.

## The fields

| Field | Where | Meaning | Live (2026-10-29) |
|---|---|---|---|
| `rng_rate` | `global5` | Basis points of each fill's `distribute` offered to the ORNG treasury **before** the producer/voter/savings split. `100` = 1%. Bound `< 10000`. | 100 |
| `max_pool_rng` | `global5` | Ceiling on the treasury's `pool_balance`, in base units (10⁻⁸ WAX). The deposit per fill is `min(rng share, max_pool_rng − pool_balance)`: a **top-up to a level**, not a budget. `0` disables deposits. | 60000000000000 (600,000 WAX) |
| `orng_code_hashes` | `global.b` | Code hashes of `orng.wax` builds that may receive deposits. Checked on **every** fill. Empty list = no deposits. | `[fa4bee5f…]` |
| `pool_balance` | `orng.wax` `treasury` | What the ORNG contract says its pool holds. This, not the `eosio.token` balance of `orng.wax`, is what the system contract compares with `max_pool_rng`. | 600,000 WAX, at the cap (token balance of `orng.wax`: 779,466 WAX; the difference is oracle stakes, pending unstakes and unclaimed fees) |
| live code hash of `orng.wax` | chain | `cleos get code orng.wax` | `fa4bee5f…` (wax-orng v3.4.0, since 2026-10-29 01:16 UTC) |

## How it enters the formula

```
rng_amount  = distribute × rng_rate / 10000
rng_deposit = verify_orng_contract() ? min(rng_amount, max_pool_rng − pool_balance) : 0
rest        = distribute − rng_deposit          → split producers/voters/savings
```

`verify_orng_contract()` (`src/voting.cpp`) is true only when the allow-list is non-empty,
`orng.wax` exists and has code, and its deployed code hash is in the list. Any failure is
**silent**: no error, no log, the carve-out simply stays in `rest` and is split like everything
else. The same happens when the pool is at or above `max_pool_rng`.

### State of mainnet today

**The carve-out is live and saturated.** Both gates are open: the allow-list holds the live hash,
and the pool is topped back up to `max_pool_rng` by the first fill after any oracle claim. The
steady state is therefore:

- Most fills find `pool_balance == max_pool_rng`, deposit 0, and split the whole `distribute`
  producers/voters/savings. The exact equality is the signature of the clamp, not a fault.
- Each oracle `claim` (`rng1.wax`, `rng2.wax`, `rng3.wax`; at most once a day each) pays that
  oracle's accrued USD stipend in WAX out of `pool_balance` (`stipendmonth` = $300/month per
  oracle, priced through `delphioracle` `waxpusd`) plus its per-call fees. Only the stipend
  lowers `pool_balance`; the fees come from the oracle's own `balances` row.
- The next fills deposit `min(1% of the fill, headroom)` with memo `treasury`, and the ORNG
  contract's `on_notify` credits `pool_balance`. At ~$0.005/WAX the three stipends accrue about
  6,400 WAX a day in total, below the ~7,500 WAX a day the 1% would supply, so the pool is back at
  the cap within hours of every claim. The realised carve-out therefore equals what the oracles
  claim, not 1% of issuance.
- **Claims are manual.** `claim` needs the oracle account's key; nothing in the oracle process
  calls it. The stipend keeps accruing unclaimed (and is capped by the pool at claim time).

Observed after the restore (2026-10-28): the three claims at 15:27–15:33 UTC paid 3,659.85 /
3,667.74 / 3,667.44 WAX each (almost all accumulated per-call fees; the stipend rows had been
created minutes earlier) and lowered `pool_balance` by 106.69 WAX in total; five deposits between
15:34:47 and 15:59:01 refilled exactly that. The first of them, trx `e2e31ffb…`, was 6.97578842 WAX =
1% of the 697.57884290 WAX distributed for a 95.5 s gap, with the 30/40/30 split of the remainder
(the policy at the time) matching to the unit. No claim and therefore no deposit since.

Consequence for the other runbooks: on a fill with headroom every other destination is 1% below
what its bps suggest (today 156,702 / 29,845 / 124,390 / 435,252 WAX a day for active / standby /
voters / savings); on a fill at the cap it is exactly the bps (158,285 / 30,146 / 125,646 / 439,648).
Nothing on `eosio` decides which case applies.

### History — why the carve-out was dead from 2026-06-03 to 2026-10-28

Kept because every failure mode below can recur on the next `orng.wax` deploy.

1. **Hash drift.** The gate shipped in `wax-3.2.0` (WBP-1917, January 2026) with the list set to
   `4e516c80…`, a pre-tag build. On 2026-06-03 the ORNG team deployed `wax-orng` v3.2.1
   (`c913ee95…`, WBP-1955) without an `addornghash`; `verify_orng_contract()` failed silently
   from then on. Nobody noticed because the symptom is the absence of a transfer.
2. **Pool counter at the cap, and overstated.** `pool_balance` read 6,000,000 WAX against a token
   balance of 790,355 WAX: the counter had only ever been incremented by deposits and decremented
   by stipends, so every other outflow left it overstated, and the system contract follows the
   counter. Headroom was 0 even with a matching hash, so fixing the hash alone restored the
   mechanism but not the flow.
3. **Drain blocked on the ORNG side.** `config.a` had no `stipendmonth` / `minclaimint` entry
   (`configv3` never called) and the stipend table `ostip.a` was empty, so `claim` failed with
   `not an oracle` and nothing had decremented the counter since 2026-03-06.

The restore (WBP-2056, 2026-10-27/28): `reconcile` action in wax-orng v3.3.0 (WBP-2058) set
`pool_balance` to the WAX actually held for the treasury; `setornghash` pointed the list at
v3.3.0 (`f9f1e880…`); `configv3` + `setstipend` re-created the stipend rows; the three oracles
claimed; the first deposit landed on 2026-10-28. On 2026-10-29 v3.4.0 (`fa4bee5f…`, WBP-2064)
was deployed with the add-then-remove sequence of the
[allow-listed contract upgrade runbook](14_runbook-allowlisted-contract-upgrade.md), so funding
did not stop this time.

## Things that are easy to miss

- **Order matters when enabling.** Allow-list the hash first, then set the rate (or set the rate
  while the hash is wrong and nothing happens until the hash is fixed, at which point deposits begin
  on the next claim with no further proposal — a surprise if the rate was left high "because it was
  inert").
- **`setornghash` replaces the whole list;** `addornghash` appends (duplicate refused);
  `rmornghash` removes one. Use `addornghash` + `rmornghash` across a redeploy of `orng.wax`
  (both hashes allowed during the switch), exactly as the guilds allow-list is handled.
- **An ORNG redeploy turns funding off.** Whoever deploys `orng.wax` must coordinate an
  `addornghash` before the deploy, or the treasury stops being funded silently. This is exactly
  what happened on 2026-06-03. Monitoring `get code orng.wax` against `global.b` is the only way
  to notice (the escrow-balance-monitor `CodeHashMonitor` alert does this since 2026-10-29). The
  deploy order is in the [allow-listed contract upgrade runbook](14_runbook-allowlisted-contract-upgrade.md).
- **`max_pool_rng` is a level, not a flow.** Raising `rng_rate` does not increase deposits if the pool
  is at the cap; raising `max_pool_rng` does. Deposits resume automatically as the ORNG contract
  spends its pool down.
- **Units.** `max_pool_rng` is in 10⁻⁸ WAX; `600000.00000000 WAX` is `60000000000000`. A units
  mistake here (writing 600000) sets the cap to 0.006 WAX and silently disables deposits. Earlier
  revisions of these runbooks mis-read the live value as 6,000,000 WAX; it is 600,000.
- **Rate is bounded only by `< 10000`.** There is no sanity ceiling; a proposal with `rng_rate = 5000`
  would send half of all issuance to the treasury until the cap is hit. Reviewers must check the
  number.
- **Settles first.** `setrngrate` calls `fill_buckets()` before storing (since `wax-3.3.x`), so a
  change never applies to time already served.
- **`pool_balance` is the ORNG contract's word.** It rises with every WAX transfer carrying the
  memo `treasury` and falls only when an oracle `claim` pays a stipend. Since v3.3.0 a
  contract-authority `reconcile` action can reset it to the WAX actually held for the treasury
  (token balance − stakes − pending unstakes − unclaimed fees); use it if the counter and the
  balance drift apart again. The system contract follows the counter regardless.

## Procedures

### A. Re-point the allow-list across an `orng.wax` deploy

The full checklist is runbook 14. In short: `addornghash(new)` → owner deploys → verify
`get code orng.wax` is listed → `rmornghash(old)`. Never `setornghash` while the chain is still
running the hash you would drop. Record of the two runs so far:

| Date (UTC) | Action | Proposal | Exec trx |
|---|---|---|---|
| 2026-10-27 | `setornghash([f9f1e880…])` (v3.3.0; replaced the stale `4e516c80…`) | `admin2.wax` | see WBP-2056 |
| 2026-10-29 15:33 | `addornghash(fa4bee5f…)` (v3.4.0) | `admin2.wax/haadrl` | `ffd49311…` |
| 2026-10-29 16:14 | `rmornghash(f9f1e880…)` | `admin2.wax/qdylsz` | `f3bbdce4…` |

### B. Switch funding off honestly

```bash
cleos -u $API multisig propose rng.off "$REQ" "$AUTH" eosio setrngrate \
  '{"rng_rate":0,"max_pool_rng":"60000000000000"}' $PROPOSER 168 -p $PROPOSER@active
```

The ORNG pool then drains at the stipend rate with no refill; at ~6,400 WAX a day (three oracles
at $300/month, ~$0.005/WAX) 600,000 WAX lasts about three months. Prefer this to removing the hash: the parameters then describe the
behaviour, and a later hash change cannot restart a drain by accident.

### C. Change the rate or the cap while funding is live

Raising the cap is the only way to get a larger one-off deposit; raising the rate only changes how
fast headroom is refilled (at 1% a day's fills already exceed a day's stipend accrual). Run the calculator
with `rng_live = True` for the per-fill effect on the other buckets:

| `rng_rate` | to `orng.wax` / day while headroom exists | active producers | standbys | voters | savings |
|---|---|---|---|---|---|
| 0 | 0 | 158,285 | 30,146 | 125,646 | 439,648 |
| 100 (1%, live) | 7,537 | 156,702 | 29,845 | 124,390 | 435,252 |
| 200 (2%) | 15,075 | 155,119 | 29,544 | 123,133 | 430,855 |
| 500 (5%) | 37,686 | 150,371 | 28,639 | 119,364 | 417,666 |

(today's supply, 600 / 2500 / 1667 / 5833, pool below cap, hash verified)

```bash
cleos -u $API multisig propose rng.cap "$REQ" "$AUTH" eosio setrngrate \
  '{"rng_rate":100,"max_pool_rng":"80000000000000"}' $PROPOSER 168 -p $PROPOSER@active      # 800,000 WAX cap
```

## Verification

```bash
cleos -u $API get table eosio eosio global5
cleos -u $API get table eosio eosio global.b
cleos -u $API get code orng.wax
cleos -u $API get table orng.wax orng.wax treasury
cleos -u $API get currency balance eosio.token orng.wax WAX
```

Expect: hash in list == live hash; `pool_balance == max_pool_rng` between claims and
`< max_pool_rng` right after an oracle claim; a `treasury`-memo transfer `eosio → orng.wax` in the
first fill trace after that claim. A hash mismatch or a pool pinned below the cap for more than a
day means the loop is broken again.
