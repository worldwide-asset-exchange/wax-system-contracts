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

| Field | Where | Meaning | Live (2026-10-26) |
|---|---|---|---|
| `rng_rate` | `global5` | Basis points of each fill's `distribute` offered to the ORNG treasury **before** the producer/voter/savings split. `100` = 1%. Bound `< 10000`. | 100 |
| `max_pool_rng` | `global5` | Ceiling on the treasury's `pool_balance`, in base units (10⁻⁸ WAX). The deposit per fill is `min(rng share, max_pool_rng − pool_balance)`: a **top-up to a level**, not a budget. `0` disables deposits. | 60000000000000 (6,000,000 WAX) |
| `orng_code_hashes` | `global.b` | Code hashes of `orng.wax` builds that may receive deposits. Checked on **every** fill. Empty list = no deposits. | `[4e516c80…]` |
| `pool_balance` | `orng.wax` `treasury` | What the ORNG contract says its pool holds. This, not the `eosio.token` balance of `orng.wax`, is what the system contract compares with `max_pool_rng`. | 6,000,000 WAX (token balance of `orng.wax`: 790,355 WAX) |
| live code hash of `orng.wax` | chain | `cleos get code orng.wax` | `c913ee95…` |

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

Both gates are closed, so **`rng_rate = 100` currently moves nothing**:

1. The allow-listed hash `4e516c80…` is not the live `orng.wax` code `c913ee95…`. Provenance,
   established 2026-10-26: the live code is bit-for-bit the reproducible build of `wax-orng`
   **v3.2.1** (WBP-1955, `migrateundlv` removed) in `waxteam/waxdev:ce-v1.0.3wax01-v4.1.1wax01`,
   and `orng.wax` `last_code_update` is **2026-06-03 14:41 UTC**, the day v3.2.1 was tagged. The
   gate itself shipped in `wax-3.2.0` (WBP-1917, deployed January 2026) and the list was set then.
   The allow-listed hash matches none of the tagged releases v3.0.0 – v3.2.1 built the same way,
   so it was a pre-tag or locally built wasm that the chain ran before June. The June deploy went
   out with no `addornghash` (WBP-1955 is still "Selected for Development" in Jira and has no
   deploy note), and the gate has failed silently since.
2. `pool_balance` is exactly `max_pool_rng`, so headroom is 0 even with a matching hash. That
   exact equality is the signature of the system contract's top-up clamp: deposits **did** run
   under the old hash until the pool reached the 6,000,000 WAX cap. On the ORNG side
   `pool_balance` is incremented by every WAX transfer into `orng.wax` and decremented only by
   oracle stipend claims (`wax-orng` `src/orng.cpp`, `_treasury_deposit` / `claim`), so any other
   outflow leaves it overstated; the account holds 790,355 WAX against a recorded 6,000,000.

Consequence for the other runbooks: the live split is an exact 30/40/30 of the full issuance
(189,290 / 252,386 / 189,290 WAX a day), not the 29.7/39.6/29.7 the parameters suggest. If either
gate re-opens, every other destination drops by 1% relative, with no action on `eosio`.

## Things that are easy to miss

- **Order matters when enabling.** Allow-list the hash first, then set the rate (or set the rate
  while the hash is wrong and nothing happens until the hash is fixed, at which point deposits begin
  on the next claim with no further proposal — a surprise if the rate was left high "because it was
  inert").
- **`setornghash` replaces the whole list;** `addornghash` appends; `rmornghash` removes one.
  Use `addornghash` + `rmornghash` across a redeploy of `orng.wax` (both hashes allowed during the
  switch), exactly as the guilds allow-list is handled.
- **An ORNG redeploy turns funding off.** Whoever deploys `orng.wax` must coordinate an
  `addornghash` before the deploy, or the treasury stops being funded silently. This is exactly
  what happened on 2026-06-03. Monitoring `get code orng.wax` against `global.b` is the only way
  to notice. The deploy order is in the
  [allow-listed contract upgrade runbook](14_runbook-allowlisted-contract-upgrade.md).
- **`max_pool_rng` is a level, not a flow.** Raising `rng_rate` does not increase deposits if the pool
  is at the cap; raising `max_pool_rng` does. Deposits resume automatically as the ORNG contract
  spends its pool down.
- **Units.** `max_pool_rng` is in 10⁻⁸ WAX; `6000000.00000000 WAX` is `60000000000000`. A units
  mistake here (writing 6,000,000) sets the cap to 0.06 WAX and silently disables deposits.
- **Rate is bounded only by `< 10000`.** There is no sanity ceiling; a proposal with `rng_rate = 5000`
  would send half of all issuance to the treasury until the cap is hit. Reviewers must check the
  number.
- **Settles first.** `setrngrate` calls `fill_buckets()` before storing (since `wax-3.3.x`), so a
  change never applies to time already served.
- **`pool_balance` is the ORNG contract's word.** If that singleton is wrong (today it reports
  6,000,000 WAX while the account holds 790,355 WAX), the system contract follows it. Resolve that on
  the ORNG side before reasoning about headroom.

## Procedures

### A. Re-enable funding (hash fix, then confirm headroom)

1. `c913ee95…` is the reproducible build of `wax-orng` v3.2.1 (see above); confirm with the ORNG
   owners that v3.2.1 is what they intend to run, then allow-list that hash. Never allow-list a
   hash nobody can reproduce.
2. Propose `addornghash` (keeps the old hash too; harmless):

   ```bash
   cleos -u $API multisig propose orng.hash "$REQ" "$AUTH" eosio addornghash \
     '{"hash":"c913ee95943be5d02a321069238eb57816d7319acd763d174b7ca494b7c021ea"}' $PROPOSER 168 -p $PROPOSER@active
   ```

   Approve, exec, then `cleos get table eosio eosio global.b` shows both hashes. Optionally
   `rmornghash` the stale `4e516c80…` afterwards.
3. Check headroom: `cleos get table orng.wax orng.wax treasury`. If `pool_balance ≥ max_pool_rng`
   deposits stay at 0; either accept that (deposits resume as ORNG spends) or raise the cap:

   ```bash
   cleos -u $API multisig propose rng.cap "$REQ" "$AUTH" eosio setrngrate \
     '{"rng_rate":100,"max_pool_rng":"80000000000000"}' $PROPOSER 168 -p $PROPOSER@active      # 8,000,000 WAX cap
   ```

4. Verify on the next claim: the trace carries a transfer `eosio → orng.wax` with memo `treasury`.

### B. Make the state honest instead (RNG funding is intentionally off)

```bash
cleos -u $API multisig propose rng.off "$REQ" "$AUTH" eosio setrngrate \
  '{"rng_rate":0,"max_pool_rng":"60000000000000"}' $PROPOSER 168 -p $PROPOSER@active
```

Nothing changes in what anyone is paid; the parameters now describe the live behaviour, and a
future ORNG hash fix cannot start a 1% drain by accident.

### C. Change the rate while funding is live

Run the calculator with `rng_live = True`. Note that the *other* buckets move:

| `rng_rate` | to `orng.wax` / day | active producers | standbys | voters | savings |
|---|---|---|---|---|---|
| 0 | 0 | 159,006 | 30,284 | 252,386 | 189,290 |
| 100 (1%) | 6,310 | 157,416 | 29,981 | 249,863 | 187,397 |
| 200 (2%) | 12,619 | 155,826 | 29,678 | 247,339 | 185,504 |
| 500 (5%) | 31,548 | 151,056 | 28,770 | 239,767 | 179,825 |

(at today's supply and rate, pool below cap, hash verified)

## Verification

```bash
cleos -u $API get table eosio eosio global5
cleos -u $API get table eosio eosio global.b
cleos -u $API get code orng.wax
cleos -u $API get table orng.wax orng.wax treasury
cleos -u $API get currency balance eosio.token orng.wax WAX
```

Expect: hash in list == live hash, `pool_balance < max_pool_rng` if deposits are intended, and a
`treasury`-memo transfer in the first claim trace after the change.
