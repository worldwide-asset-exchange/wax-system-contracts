---
content_title: Runbook — active producer count
link_text: Runbook — active producer count
---

# Runbook: changing the number of active block producers

Actions: `setprodcnt(count)`, `setminbpvote(min)`, `setprodctrl(cooldown_secs)` — all `eosio`
msig. Table: `global4`. Read the [overview](09_runbook-tokenomics-parameters.md) first.

## The fields

| Field | Setter | Meaning | Bound | Live |
|---|---|---|---|---|
| `active_producer_count` | `setprodcnt(count)` | Size of the block-producing schedule proposed at each election. | `1 … 21`; **exactly ±1 per call**; `min_cooldown_secs` since the last change | 7 (last change 2026-10-13 15:24 UTC) |
| `last_change_time` | written by `setprodcnt` | Unix seconds of the last count change; the cooldown is measured from it. | — | 1791905058 |
| `min_cooldown_secs` | `setprodctrl(cooldown_secs)` | Minimum seconds between two `setprodcnt` calls. | `≥ 600` | 86400 (one day) |
| `min_bps_voting_reward` | `setminbpvote(min)` | Producers a voter must vote for to accrue voter pay (proxy voters are exempt). | `1 … active_producer_count` | 5 |

The "one producer per day" rule is therefore **enforced on chain** at today's settings: a proposal
that steps by two, or steps again within 24 hours, is refused at `exec` (`must change by exactly ±1`,
`cool-down not elapsed`). Keep `min_cooldown_secs` at 86400; the contract minimum of 600 s exists
for emergencies and testnets, not for routine use.

## What a count change does

1. **Election.** Within about a minute `update_elected_producers` proposes a schedule of the new
   size: the top `count` producers by weighted votes (guild score × votes), skipping no one.
   The schedule becomes pending when its proposing block is irreversible and active one
   irreversibility interval later: a few minutes at today's chain speed. Follow it with
   `cleos get schedule` (active / pending).
2. **Who moves.** Going 7 → 6 drops the 7th-ranked producer; it becomes standby #1 and the former
   standby #3 is pushed off the list (deactivated, keeps its share). Going 7 → 8 seats the current
   standby #1 as the 8th producer and the next eligible candidate becomes standby #3. Check the
   weighted ranking before proposing so the change is not a surprise to the producer concerned —
   that is what the standby system exists for: the next producer in is one that has been paid to
   stay ready.
3. **Pay.** `A = count × 10000` changes, so the producers share re-splits between active and
   standby, and the active part is divided among one more or one fewer producer:

   | count | active bucket / day | per producer / day | standby per seat / day |
   |---|---|---|---|
   | 5 | 147,277 | 29,455 | 13,090 |
   | 6 | 152,632 | 25,439 | 11,305 |
   | **7 (live)** | **156,702** | **22,386** | **9,948** |
   | 8 | 159,900 | 19,987 | 8,882 |
   | 9 | 162,479 | 18,053 | 8,023 |

   (today's supply, 600 / 2500 / 1667 / 5833 live since 2026-10-29, RNG carve-out live, 3 × 4444
   standbys; voters and `eosio.saving` are unaffected; 1% higher on a fill that finds the ORNG pool
   at its cap)

4. **`min_bps_voting_reward` is clamped down only.** If `min > count` after a cut, `setprodcnt`
   lowers `min` to `count`. It never raises it, and a cut that leaves `min ≤ count` leaves it alone.
   Today `min = 5` at `count = 7`; 7 → 6 keeps 5, 6 → 5 keeps 5, 5 → 4 clamps to 4. A rise never
   changes it. **Decide the new minimum yourself and set it in the same proposal.**
5. **Finality.** Classic DPoS tolerates `⌊(count − 1)/3⌋` failed producers before last-irreversible
   stalls: 2 at 7, 8 and 9; 1 at 4, 5 and 6; **0 at 3 or fewer**. Do not take the count below 4
   under any circumstances, and treat 4–6 as an incident configuration. The contract's floor of 1
   is not a safe floor.

## `min_bps_voting_reward` — things that are easy to miss

- **A voter's eligibility is re-evaluated only when they vote.** `update_voter_votepay_share` runs
  inside `voteproducer`, nowhere else — not on stake changes, not on claims. After
  `setminbpvote 6`, a voter with 5 producers keeps accruing at their old rate until they next call
  `voteproducer` (any `voteproducer`, even with the same list, re-evaluates). Likewise a voter with
  5 producers who was below an old minimum of 6 gains nothing from `setminbpvote 5` until they
  re-vote. **Announce the change and ask voters to re-cast.** Wallets that re-vote on every stake
  change pick it up quickly; dormant voters never do.
  *Severity: low.* It is a fairness lag, not a loss: nobody is paid from a bucket they did not
  accrue into, and the clamp-down-only behaviour means a cut can never leave `min > count`. The
  only way to make it immediate is a contract change (re-evaluate at `voterclaim`), which is not
  planned.
- **Proxy voters are exempt.** `voter.proxy` set → accrues regardless of how many producers the proxy
  votes for. Only direct voters are gated.
- **The upper bound is the live count.** `setminbpvote 8` at `count = 7` is refused
  (`min_bps_voting_reward must be less than or equal to active_producer_count`). So when **raising**
  the count, order the actions `setprodcnt` then `setminbpvote` in the same transaction.
- **The lower bound is 1**, not 0; there is no way to disable the requirement other than `1`.
- **A vote may name up to 30 producers**, so the minimum is always satisfiable. But every producer
  on a vote dilutes the voter's weight across candidates, and the top-N ranking is by weighted votes:
  a high minimum pushes voters to spread votes to non-schedule producers, which flattens the
  ranking's margins. Keep the ratio `min / count` stable (today 5/7 ≈ 0.71) unless there is a reason
  to move it.

## Procedure — one step of ±1

Day N (repeat on day N+1, N+2, … until the target is reached; one proposal per day):

1. Pre-flight: `global4` (confirm `active_producer_count`, `last_change_time + 86400 ≤ now`,
   `min_bps_voting_reward`); `get schedule`; the weighted ranking around position `count` and
   `count ± 1`; active standbys. Confirm no other `setprodcnt` proposal is open
   (`cleos multisig ...` review of pending proposals for `eosio`).
2. Decide `min_bps_voting_reward` for the new count. Build **one transaction, two actions**,
   in this order:

   ```bash
   # cut 7 -> 6, minimum 5 -> 4
   cleos -u $API push action eosio setprodcnt   '{"count":6}' -p eosio@active -s -d -j -x 604800 > /tmp/trx.json
   cleos -u $API push action eosio setminbpvote '{"min":4}'   -p eosio@active -s -d -j -x 604800 > /tmp/trx2.json
   jq --slurpfile b /tmp/trx2.json '.actions += $b[0].actions' /tmp/trx.json > /tmp/prodcnt.trx.json
   cleos -u $API multisig propose_trx prodcnt.<date> "$REQ" /tmp/prodcnt.trx.json $PROPOSER -p $PROPOSER@active
   ```

   For a rise (`7 → 8`, minimum `5 → 6`) the same two actions in the same order; `setminbpvote`
   would be refused if it ran first.
3. Reviewers check: `count` differs from live by exactly 1; the cooldown has elapsed; `min ≤ count`;
   the producer being added/dropped is the one the ranking says. Approve 2 of 3, `exec`.
4. Watch for the rest of the day:
   - `cleos get schedule`: pending appears within ~1 min, becomes active within a few minutes.
   - Missed blocks: `cleos get table eosio eosio producers` → `unpaid_blocks` of each scheduled
     producer should grow by ~`172800 / count` per day (24,686 at 7; 28,800 at 6). A producer whose
     count stalls is not producing.
   - `standbys` table: `is_active` flags match the new boundary.
   - Head block / LIB gap unchanged (block explorers, or `cleos get info` twice).
5. Communicate: the producer that joined or left, the new `min_bps_voting_reward`, and "re-cast your
   vote to keep earning voter rewards" to the community.
6. Next step not before `last_change_time + 86400`. If anything looked wrong, the undo is the same
   procedure in reverse, and it also has to wait for the cooldown.

## Worked example — 7 → 5 over two days

| Day | Actions in the proposal | Result |
|---|---|---|
| 0 | read: count 7, min 5, standby #1 `alohaeosprod` | ranking shows `bp.alcor` as #7 |
| 1 | `setprodcnt 6`, `setminbpvote 4` | schedule of 6 within minutes; `bp.alcor` → standby #1; `eosdacserver` (standby #3) deactivated, keeps 45.1% share; per-producer pay 22,386 → 25,439 WAX/day; per-seat standby pay 9,948 → 11,305 |
| 2 | `setprodcnt 5`, `setminbpvote 4` (or 3) | schedule of 5; former #6 → standby #1; tolerates 1 failure (was 2) — note the finality drop |

Refusals you can hit: `count must be between 1 and 21`; `must change by exactly ±1`;
`cool-down not elapsed`; `min_bps_voting_reward must be greater than 0`;
`min_bps_voting_reward must be less than or equal to active_producer_count`;
`cooldown must be at least 600 (10 minutes)`.
