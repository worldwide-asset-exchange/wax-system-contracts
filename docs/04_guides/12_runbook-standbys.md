---
content_title: Runbook — standby producers
link_text: Runbook — standby producers
---

# Runbook: changing the number and pay of standby producers

Actions: `setsbslot(num_slots)`, `setsbratio(ratio)`, `disallowsb(account)` / `allowsb(account)`
— all `eosio` msig. Tables: `global4` (controls and bucket), `standbys` (per-producer share),
`sbdisallow`. Read the [overview](09_runbook-tokenomics-parameters.md) first.

## The fields

| Field | Where | Meaning | Live |
|---|---|---|---|
| `num_standby_slots` | `global4` | How many producers behind the active schedule are seated as standbys at each election. `0` disables standbys. Bound `≤ 21`. | 3 |
| `standby_slot_weight` | `global4` | Pay weight of one standby seat relative to one active seat, out of `PAY_SPLIT_SCALE = 10000`. `4444` = a seat earns 44.44% of what an active producer earns for the same period. Bound `≤ 10000`. | 4444 |
| `standby_bucket`, `total_standby_share` | `global4` | The pool and the sum of all accrued shares. Not settable. | 3,738,858 WAX; 44.94 M seat-seconds |
| `standbys` rows | `standbys` | `owner`, `standby_share` (microseconds seated, unclaimed), `is_active` (seated now), `last_claim_time`. Rows are never deleted; a deactivated row keeps its share. | 3 active of 21 rows |
| `sbdisallow` rows | `sbdisallow` | Accounts that are skipped when filling standby seats (the next ranked producer takes the seat; no slot is consumed). | none |

## How it enters the formula

```
A = active_producer_count × 10000          S = standby_slot_weight × num_standby_slots
producers share → active bucket  × A/(A+S)
                → standby bucket × S/(A+S)
```

Today: `A = 70,000`, `S = 13,332`, so standbys receive 16.0% of the producers share
(4.0% of all issuance, 29,845 WAX a day, 9,948 per seat; an active producer gets 22,386 — at the
600 / 2500 / 1667 / 5833 policy live since 2026-10-29, RNG carve-out live).

Who is seated: at every election (about once a minute) the producers ranked `active_producer_count + 1`
onward by **weighted** votes (guild score × votes, see overview §1 of the architecture page) fill
the slots, skipping `sbdisallow` accounts. A candidate must also pass the election filters:
registered and active, `total_votes > min_producer_vote_threshold` (1000.0), and within the first
`max_considered_producers` (100) by raw votes. Share accrues per microsecond seated; `claimstandby`
pays `standby_bucket × share / total_share`, once a day, from `eosio.bpay`.

## Things that are easy to miss

- **Configured slots are paid whether or not they are filled.** `S` uses `num_standby_slots`, not
  the number of seated standbys. If only two eligible candidates exist for three slots, the third
  seat's pay still flows into the standby bucket and is shared by whoever has share, i.e. the two
  seated standbys are over-paid by 50%. Check the candidate pool before raising slots.
  *Severity: low today.* Mainnet has 54 active producers with votes above the threshold against
  3 slots, so every seat is filled; it only bites if slots are raised faster than the candidate
  pool, or if `disallowsb` empties a seat. Paying by seated count instead would be a contract
  change, which is not planned.
- **Changing slots or weight changes active-producer pay.** Same producers share, different
  `A/(A+S)`. Cutting slots 3 → 2 raises each active producer from 22,386 to 23,647 WAX a day.
- **Changing the producer count changes standby pay** without touching anything here: `A` shrinks
  when `setprodcnt` lowers the count, so standbys' share of the producers bucket rises
  (7 → 6 producers: per seat 9,948 → 11,305). See the
  [producer-count runbook](13_runbook-producer-count.md).
- **A cut deactivates at the next election, not immediately.** `setsbslot` stores the value and
  settles the buckets; the surplus standby rows are set `is_active = false` within about a minute
  (`update_standby_producers` runs on every election, even with an empty list).
- **Deactivated standbys keep their accrued share and can still claim it.** The bucket is not
  redistributed. Today `eosdacserver` holds 45.1% of `total_standby_share` and has never claimed
  (≈1.67 M WAX of the bucket is theirs on demand). Reducing slots to 0 stops new pay but leaves the
  bucket claimable until drained.
- **`disallowsb` is not a pay switch for an already-seated standby.** It removes the account from
  future elections' standby lists (deactivated at the next one), but accrued share stays claimable.
- **`setsbratio` has no floor.** `0` is accepted: standbys are seated, accrue share, and the bucket
  receives nothing, so `claimstandby` fails with `no standby reward to claim`. Use `setsbslot 0`
  rather than `setsbratio 0` if the intent is "no standbys".
- **Settles first.** Both setters call `fill_buckets()` before storing.
- **No cooldown, no ±1 rule.** Unlike `setprodcnt`, slot and weight changes can jump arbitrarily
  within bounds. Keep the same discipline voluntarily: one step at a time, and not on the same day
  as a `setprodcnt`, so the effect of each is observable.

## Procedures

### Change the slot count

```bash
cleos -u $API multisig propose sb.slots "$REQ" "$AUTH" eosio setsbslot '{"num_slots":2}' $PROPOSER 168 -p $PROPOSER@active
```

Before proposing: `cleos get table eosio eosio standbys --limit 50 | jq '.rows[] | select(.is_active)'`
and the weighted ranking (producers ranked `count+1 … count+slots`) to know who will be dropped or
added. After exec: wait two minutes, re-read the table, confirm `is_active` flags match the new count.

### Change the seat weight

```bash
cleos -u $API multisig propose sb.ratio "$REQ" "$AUTH" eosio setsbratio '{"ratio":5000}' $PROPOSER 168 -p $PROPOSER@active
```

### Exclude / re-admit an account

```bash
cleos -u $API multisig propose sb.block "$REQ" "$AUTH" eosio disallowsb '{"account":"someguild111"}' $PROPOSER 168 -p $PROPOSER@active
cleos -u $API multisig propose sb.allow "$REQ" "$AUTH" eosio allowsb    '{"account":"someguild111"}' $PROPOSER 168 -p $PROPOSER@active
```

Refusals: `account already in standby disallow list`, `account not exist in standby disallow list`.

## Worked examples (today's supply, 7 active producers, 600 / 2500 / 1667 / 5833, RNG live)

| Change | standbys / day (per seat) | active producers / day (each) | note |
|---|---|---|---|
| **3 slots × 4444 (live)** | 29,845 (9,948) | 156,702 (22,386) | seat = 44.4% of an active producer |
| 2 slots × 4444 | 21,018 (10,509) | 165,530 (23,647) | former standby #3 deactivated at next election, keeps share |
| 4 slots × 4444 | 37,779 (9,445) | 148,768 (21,253) | needs a 4th eligible candidate or the seat is paid to the other three |
| 3 slots × 5000 | 32,920 (10,973) | 153,627 (21,947) | seat = 50% of an active producer |
| 3 slots × 10000 | 55,964 (18,655) | 130,583 (18,655) | a standby seat paid like an active one (the ceiling) |
| 0 slots | 0 | 186,547 (26,650) | all producers share to active; bucket remains claimable |
| 3 × 4444 but 6 active producers (`setprodcnt`) | 33,915 (11,305) | 152,632 (25,439) | no standby action taken |

On a fill that finds the ORNG pool at its cap every figure is 1% higher.

## Verification

```bash
cleos -u $API get table eosio eosio global4 | jq '.rows[0] | {num_standby_slots, standby_slot_weight, standby_bucket, total_standby_share}'
cleos -u $API get table eosio eosio standbys --limit 50 | jq '[.rows[] | select(.is_active) | .owner]'
```
