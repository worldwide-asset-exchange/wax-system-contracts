---
content_title: Runbook — inflation rate and split
link_text: Runbook — inflation rate and split
---

# Runbook: changing the inflation rate and the producer / voter / savings split

Action: `setinflation(annual_rate_bps, producers_bps, voters_bps, savings_bps)` — `eosio` msig.
Table: `global.c`. Added in `wax-3.4.0` (WBP-2035). Read the
[overview](09_runbook-tokenomics-parameters.md) first for the formula, the live values and the
msig procedure; this page covers only what is specific to this action.

## The fields

| Field | Type | Meaning | Default / live |
|---|---|---|---|
| `annual_rate_bps` | int64 | Annual issuance rate in basis points of supply. `500` = 5.00%/yr. `0` stops issuance. Stored as given; the contract derives the continuous rate `ln(1 + r)` truncated to five decimals and applies it to the current supply for the elapsed time. | 500 |
| `producers_bps` | uint16 | Share of post-RNG inflation sent to `eosio.bpay` for block pay. Divided between active producers and standbys by `A/(A+S)` (see overview). | 3000 |
| `voters_bps` | uint16 | Share sent to `eosio.voters`; paid out by `voterclaim`. | 4000 |
| `savings_bps` | uint16 | Share sent to `eosio.saving` (funds WPS and treasury spending). Stored so the proposal states the whole split; the transfer itself is the remainder, so any rounding dust lands here. | 3000 |

All four must be given on every call; there is no partial update. Bounds (`validate_inflation_policy`):
`0 ≤ annual_rate_bps ≤ 1000` and `producers_bps + voters_bps + savings_bps == 10000`. Anything
above 10%/yr is a tokenomics redesign and needs a contract upgrade, by design.

### Continuous rate and what one day issues

The rate is applied continuously, and the contract's year is 364 days (52 weeks). At today's supply
(4,707,350,511 WAX):

| `annual_rate_bps` | continuous rate stored | WAX issued per day | WAX issued per year |
|---|---|---|---|
| 0 | 0.00000 | 0 | 0 |
| 100 | 0.00995 | 128,676 | 46.8 M |
| 200 | 0.01980 | 256,059 | 93.2 M |
| 300 | 0.02955 | 382,149 | 139.1 M |
| 400 | 0.03922 | 507,204 | 184.6 M |
| **500 (live)** | **0.04879** | **630,966** | **229.7 M** |
| 600 | 0.05826 | 753,435 | 274.3 M |
| 700 | 0.06765 | 874,869 | 318.5 M |
| 800 | 0.07696 | 995,268 | 362.3 M |
| 1000 (ceiling) | 0.09531 | 1,232,576 | 448.7 M |

The truncation to five decimals under-issues by at most 0.00001 absolute: negligible at 500 bps
(0.02%), 1% at 10 bps. Below ~100 bps, read the stored continuous rate rather than the nominal bps.

**Fees come first.** Before issuing, `fill_buckets` spends the `eosio.fees` balance (PowerUp fees)
toward `distribute` and issues only the shortfall; whatever fee balance exceeds `distribute` is
**burned**. So net issuance is `distribute − fees`, and a lower rate means more of the fee revenue
is burned rather than less being minted. At `annual_rate_bps = 0` every fee is burned.

## How a change propagates — things that are easy to miss

1. **It applies from the moment of execution, not retroactively.** `setinflation` calls
   `fill_buckets()` before storing, so the interval since the last claim is settled under the old
   policy. No claimer gains or loses accrued pay.
2. **`producers_bps` moves standby pay too.** The standby bucket is a slice of the producers share
   (`S/(A+S)`, 16% of it at today's 7 × 10000 : 3 × 4444). Raising producers from 3000 to 3500
   raises a standby seat's pay by the same 16.7% as an active producer's. To move only active pay,
   pair it with `setsbratio`.
3. **`savings_bps` is the WPS budget.** `eosio.saving` is what `wps.cpp` and treasury operations
   draw on. Changing it changes what the chain can fund, not just a bucket.
4. **The RNG rate is nested.** `distribute` is reduced by the RNG carve-out before the split, so a
   live RNG rate scales all three shares down by `(1 − rng_rate/10000)`. Today the carve-out is inert
   (see the [RNG runbook](11_runbook-rng-payouts.md)); if it is re-enabled later, every share drops 1%
   relative without any `setinflation` call.
5. **A zero share is legal and fails closed.** With `voters_bps = 0`, `voterclaim` fails with
   `no rewards available.` once the bucket is empty; with `producers_bps = 0`, `claimrewards`
   succeeds but transfers nothing and `claimstandby` fails with `no standby reward to claim`.
   Existing bucket balances remain claimable until drained.
6. **Rate 0 stops every bucket at once**, including standby and voter pay. There is no per-bucket
   "off" other than a zero share.
7. **Nothing changes who is elected.** The split never enters the ranking; the producer count,
   standby count and vote thresholds are separate parameters.

## Procedure

1. Pre-flight reads (overview §0). Confirm `global.c` shows the values you believe are live.
2. Run the calculator with the candidate policy; put the "WAX per day" before/after table in the
   proposal text.
3. Propose, in one action:

   ```bash
   cleos -u $API multisig propose infl.<date> "$REQ" "$AUTH" eosio setinflation \
     '{"annual_rate_bps":500,"producers_bps":3000,"voters_bps":4000,"savings_bps":3000}' $PROPOSER 168 -p $PROPOSER@active
   ```

4. Reviewers check: the four numbers; that the three shares sum to 10000; that the rate is ≤ 1000.
   A proposal that fails either check will be refused at `exec`, not at `propose`.
5. Approve (2 of 3), `exec`, then `cleos get table eosio eosio global.c`.
6. Verify the first claim after the change: in its trace the transfer memos `fund bpay bucket`,
   `fund voters bucket`, `unallocated inflation` should split in the new proportions. Watch
   `eosio.token` supply (`cleos get currency stats eosio.token WAX`) over the next day.

Undo: propose the previous four values.

## Worked examples (today's supply, RNG inert, 7 producers, 3 × 4444 standbys)

| Policy | daily total | active producers (each) | standbys (per seat) | voters | `eosio.saving` |
|---|---|---|---|---|---|
| **500 / 3000 / 4000 / 3000 (live)** | 630,966 | 159,006 (22,715) | 30,284 (10,095) | 252,386 | 189,290 |
| 600 / 3000 / 4000 / 3000 — raise the rate, keep the split | 753,435 (+19.4%) | 189,869 (27,124) | 36,162 (12,054) | 301,374 | 226,030 |
| 500 / 3500 / 3500 / 3000 — shift 5 points from voters to producers | 630,966 | 185,507 (26,501) +16.7% | 35,331 (11,777) +16.7% | 220,838 −12.5% | 189,290 |
| 400 / 3000 / 4000 / 3000 — lower the rate | 507,204 (−19.6%) | 127,817 (18,260) | 24,344 (8,115) | 202,882 | 152,161 |
| 500 / 3000 / 4000 / 3000 with RNG re-enabled (1%) | 630,966 | 157,416 (22,488) | 29,981 (9,994) | 249,863 | 187,397 (+6,310 to `orng.wax`) |
| 0 / any valid split | 0 | 0 | 0 | 0 | 0 — and all PowerUp fees burned |

Example proposal JSON for the second row:

```json
{"annual_rate_bps":600,"producers_bps":3000,"voters_bps":4000,"savings_bps":3000}
```

Refusals you can hit: `annual_rate_bps must be between 0 and 1000`;
`producers_bps + voters_bps + savings_bps must equal 10000`; or a refusal naming `rng_rate`,
`ratio` or `num_slots` — those mean another table is already out of bounds and must be fixed first.
