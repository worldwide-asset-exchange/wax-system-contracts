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

| Field | Type | Meaning | Default | Live (since 2026-10-29) |
|---|---|---|---|---|
| `annual_rate_bps` | int64 | Annual issuance rate in basis points of supply. `500` = 5.00%/yr. `0` stops issuance. Stored as given; the contract derives the continuous rate `ln(1 + r)` truncated to five decimals and applies it to the current supply for the elapsed time. | 500 | **600** |
| `producers_bps` | uint16 | Share of post-RNG inflation sent to `eosio.bpay` for block pay. Divided between active producers and standbys by `A/(A+S)` (see overview). | 3000 | **2500** |
| `voters_bps` | uint16 | Share sent to `eosio.voters`; paid out by `voterclaim`. | 4000 | **1667** |
| `savings_bps` | uint16 | Share sent to `eosio.saving` (funds WPS and treasury spending). Stored so the proposal states the whole split; the transfer itself is the remainder, so any rounding dust lands here. | 3000 | **5833** |

All four must be given on every call; there is no partial update. Bounds (`validate_inflation_policy`):
`0 ≤ annual_rate_bps ≤ 1000` and `producers_bps + voters_bps + savings_bps == 10000`. Anything
above 10%/yr is a tokenomics redesign and needs a contract upgrade, by design.

### Continuous rate and what one day issues

The rate is applied continuously, and the contract's year is 364 days (52 weeks). At today's supply
(4,709,168,913 WAX):

| `annual_rate_bps` | continuous rate stored | WAX issued per day | WAX issued per year |
|---|---|---|---|
| 0 | 0.00000 | 0 | 0 |
| 100 | 0.00995 | 128,726 | 46.9 M |
| 200 | 0.01980 | 256,158 | 93.2 M |
| 300 | 0.02955 | 382,297 | 139.2 M |
| 400 | 0.03922 | 507,400 | 184.7 M |
| 500 (until 2026-10-29) | 0.04879 | 631,210 | 229.8 M |
| **600 (live)** | **0.05826** | **753,726** | **274.4 M** |
| 700 | 0.06765 | 875,207 | 318.6 M |
| 800 | 0.07696 | 995,653 | 362.4 M |
| 1000 (ceiling) | 0.09531 | 1,233,052 | 448.8 M |

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
   live RNG rate scales all three shares down by `(1 − rng_rate/10000)`. The carve-out has been live
   since 2026-10-28 (see the [RNG runbook](11_runbook-rng-payouts.md)): every share is 1% lower than
   its bps suggest on a fill that finds headroom in the ORNG pool, and exactly its bps on a fill that
   finds the pool at its cap. Nothing on `eosio` decides which; the ORNG stipend claims do.
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
     '{"annual_rate_bps":600,"producers_bps":2500,"voters_bps":1667,"savings_bps":5833}' $PROPOSER 168 -p $PROPOSER@active
   ```

   The values shown are the live policy, so copying them unchanged is a harmless no-op.

4. Reviewers check: the four numbers; that the three shares sum to 10000; that the rate is ≤ 1000.
   A proposal that fails either check will be refused at `exec`, not at `propose`.
5. Approve (2 of 3), `exec`, then `cleos get table eosio eosio global.c`.
6. Verify the first claim after the change: in its trace the transfer memos `fund bpay bucket`,
   `fund voters bucket`, `unallocated inflation` should split in the new proportions. Watch
   `eosio.token` supply (`cleos get currency stats eosio.token WAX`) over the next day.

Undo: propose the previous four values.

## Worked examples (today's supply, RNG live at 1%, 7 producers, 3 × 4444 standbys)

| Policy | daily total | active producers (each) | standbys (per seat) | voters | `eosio.saving` | `orng.wax` |
|---|---|---|---|---|---|---|
| **600 / 2500 / 1667 / 5833 (live since 2026-10-29)** | 753,726 | 156,702 (22,386) | 29,845 (9,948) | 124,390 | 435,252 | 7,537 |
| same, on a fill that finds the ORNG pool at its cap | 753,726 | 158,285 (22,612) | 30,146 (10,049) | 125,646 | 439,648 | 0 |
| 500 / 3000 / 4000 / 3000 — the policy before 2026-10-29 | 631,210 (−16.3%) | 157,477 (22,497) | 29,993 (9,998) | 249,959 | 187,469 | 6,312 |
| 500 / 2500 / 1667 / 5833 — undo only the rate | 631,210 (−16.3%) | 131,231 (18,747) −16.3% | 24,994 (8,331) | 104,170 | 364,503 | 6,312 |
| 700 / 2500 / 1667 / 5833 — raise the rate, keep the split | 875,207 (+16.1%) | 181,958 (25,994) +16.1% | 34,655 (11,552) | 144,438 | 505,403 | 8,752 |
| 600 / 3000 / 1167 / 5833 — shift 5 points from voters to producers | 753,726 | 188,043 (26,863) +20% | 35,814 (11,938) +20% | 87,080 −30% | 435,252 | 7,537 |
| 0 / any valid split | 0 | 0 | 0 | 0 | 0 — and all PowerUp fees burned | 0 |

The live row is the 2026-10-29 change itself: it raised the rate from 5% to 6% while cutting the
producers share from 30% to 25%, so block pay stayed within 0.5% of the previous policy in WAX
(22,497 → 22,386 per producer), voters were halved (249,959 → 124,390) and the rest went to savings
(187,469 → 435,252). Holding a bucket flat across a rate change means scaling its share by the
ratio of the two continuous rates (0.04879 / 0.05826 = 0.8375 here); the share was rounded to
2500 rather than the exact 2512 for readability.

Example proposal JSON for the live row:

```json
{"annual_rate_bps":600,"producers_bps":2500,"voters_bps":1667,"savings_bps":5833}
```

## Record

| Date (UTC) | Change | Proposal | Exec trx |
|---|---|---|---|
| 2026-10-26 | `wax-3.4.0` deployed; `global.c` written with the defaults 500 / 3000 / 4000 / 3000 (no `setinflation` call) | — | — |
| 2026-10-29 15:25:53 | `setinflation(600, 2500, 1667, 5833)` | `admin2.wax/infoct2eight` (approved admin2.wax, admin3.wax) | `6d27bdb89a2e8d191dd3eae3b022224bc815263ce047c6fdc9feba738742d8bb` |

Verified after the 2026-10-29 exec: the settlement inside the exec split 0.30000 / 0.40000 / 0.30000
(old policy, as designed); the first fill after it (15:32:53, trx `80e097c6…`) split
0.25000 / 0.16670 / 0.58330 and its total matched the 0.05826 continuous rate over the elapsed
419.5 s to four decimals.

Refusals you can hit: `annual_rate_bps must be between 0 and 1000`;
`producers_bps + voters_bps + savings_bps must equal 10000`; or a refusal naming `rng_rate`,
`ratio` or `num_slots` — those mean another table is already out of bounds and must be fixed first.
