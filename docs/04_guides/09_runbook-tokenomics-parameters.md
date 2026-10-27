---
content_title: Runbook — tokenomics parameters (overview)
link_text: Runbook — tokenomics parameters (overview)
---

# Adjusting the tokenomics parameters of `eosio.system`

This is the entry page for four operator runbooks. Each one changes a parameter of the WAX inflation
and producer-pay machinery by multisig, without a contract upgrade:

| Runbook | Actions | What it moves |
|---|---|---|
| [Inflation rate and split](10_runbook-inflation.md) | `setinflation` | how much is issued per year and how it divides between block pay, voters and `eosio.saving` |
| [RNG payouts](11_runbook-rng-payouts.md) | `setrngrate`, `setornghash` / `addornghash` / `rmornghash` | the carve-out that funds the ORNG treasury before the split |
| [Standby producers](12_runbook-standbys.md) | `setsbslot`, `setsbratio`, `disallowsb` / `allowsb` | how many standbys are seated and how a standby seat is paid relative to an active one |
| [Active producer count](13_runbook-producer-count.md) | `setprodcnt`, `setminbpvote`, `setprodctrl` | the size of the block-producing schedule and the voting requirement for voter pay |

Read this page once; it holds what the four have in common: the single formula every parameter
enters, the live values, the interaction matrix, and the multisig procedure. The mechanics are
described in depth in the
[architecture overview](../02_wax-extensions/01_architecture-overview.md); this page only repeats
what an operator needs at the keyboard. Source references are to tag `wax-3.4.0`
(deployed on mainnet `eosio` on 2026-10-26), paths relative to `contracts/eosio.system/`.

## One formula

Nothing is minted per block. Every claim (`claimrewards`, `voterclaim`, `claimstandby`) and every
parameter setter first calls `fill_buckets()` (`src/producer_pay.cpp`), which settles the time since
the previous fill under the parameters **in force at that moment**, and only then is a new value
stored. A change therefore applies from the instant it executes and never re-prices time already
served.

```
distribute = continuous_rate × supply × Δt / year          continuous_rate = ln(1 + annual_rate_bps/10000), truncated to 5 decimals
                                                          year = 364 days (52 × 7 × 24 × 3600 s)
   paid from eosio.fees first (PowerUp fees), the shortfall is issued, leftover fees are burned

rng      = distribute × rng_rate/10000                    deposited to orng.wax ONLY IF its code hash is allow-listed in global.b
                                                          AND capped at max_pool_rng − treasury pool_balance; otherwise rng = 0
rest     = distribute − rng

producers = rest × producers_bps/10000   ─┬─ active   = producers × A/(A+S)     A = active_producer_count × 10000
                                           └─ standby  = producers × S/(A+S)     S = standby_slot_weight × num_standby_slots
voters    = rest × voters_bps/10000                                                   (configured slots, not seated ones)
savings   = rest − producers − voters     (savings_bps is stored for the sum check; rounding dust lands here)
```

Downstream of the buckets:

- an active producer claims `perblock_bucket × its unpaid_blocks / total_unpaid_blocks`, once a day;
- a standby claims `standby_bucket × its standby_share / total_standby_share`, once a day, where share
  is **time seated**, in microseconds;
- a voter claims `voters_bucket × its unpaid_voteshare / total_unpaid_voteshare`, once a day, and
  accrues share only while voting for at least `min_bps_voting_reward` producers or through a proxy.

## Where the parameters live, and the live values

All setters `require_auth(eosio)`; `eosio@active` is `admin.wax@active`, a 2-of-3 of
`admin1.wax`, `admin2.wax`, `admin3.wax`, so every change is an `eosio.msig` proposal.
Live values read from mainnet on 2026-10-26 23:10 UTC:

| Field | Table | Setter | Bound (refusal text) | Live |
|---|---|---|---|---|
| `annual_rate_bps` | `global.c` | `setinflation` | `0 … 1000` — "annual_rate_bps must be between 0 and 1000" | 500 (5%/yr → 0.04879 continuous) |
| `producers_bps` / `voters_bps` / `savings_bps` | `global.c` | `setinflation` | sum `== 10000` — "producers_bps + voters_bps + savings_bps must equal 10000" | 3000 / 4000 / 3000 |
| `rng_rate` | `global5` | `setrngrate` | `< 10000` — "rng_rate must be less than 10000" | 100 (1%) — **inert, see below** |
| `max_pool_rng` | `global5` | `setrngrate` | none (base units, 10⁻⁸ WAX) | 60000000000000 (6,000,000 WAX) |
| `orng_code_hashes` | `global.b` | `setornghash`, `addornghash`, `rmornghash` | — | `4e516c80…` — **does not match `orng.wax` live code `c913ee95…`** |
| `num_standby_slots` | `global4` | `setsbslot` | `≤ 21` — "num_slots cannot exceed 21"; 0 allowed | 3 |
| `standby_slot_weight` | `global4` | `setsbratio` | `≤ 10000` — "ratio cannot exceed PAY_SPLIT_SCALE (10000)" | 4444 (a seat earns 44.44% of an active seat) |
| `sbdisallow` rows | `sbdisallow` | `disallowsb` / `allowsb` | no duplicate / must exist | empty |
| `active_producer_count` | `global4` | `setprodcnt` | `1 … 21`, exactly ±1 per call, cooldown — "must change by exactly ±1", "cool-down not elapsed" | 7 (last change 2026-10-13 15:24 UTC) |
| `min_cooldown_secs` | `global4` | `setprodctrl` | `≥ 600` — "cooldown must be at least 600 (10 minutes)" | 86400 (1 day) |
| `min_bps_voting_reward` | `global4` | `setminbpvote` | `1 … active_producer_count` — "min_bps_voting_reward must be less than or equal to active_producer_count" | 5 |
| `min_producer_vote_threshold` | `global.a` | `setminvote` | finite, `≥ 0` | 1000.0 |
| `max_considered_producers` | `global.a` | `setmaxprod` | `21 … 500` | 100 |

Other live facts the examples below use: supply 4,707,350,510.919 WAX; schedule of 7
(`eosiodetroit`, `ivote4waxusa`, `nation.wax`, `waxswedenorg`, `waxhiveguild`, `eosphereiobp`,
`bp.alcor`); seated standbys `alohaeosprod`, `bountyblokbp`, `eosdacserver`; `perblock_bucket`
3,221,797 WAX; `standby_bucket` 3,738,858 WAX; `voters_bucket` 2,495,101 WAX; `eosio.saving`
22,164,749 WAX. At these values one day issues **630,966 WAX**, split (RNG inert):

| destination | share | WAX / day |
|---|---|---|
| active producers (`eosio.bpay`, `perblock_bucket`) | 25.2% | 159,006 (22,715 per producer) |
| standbys (`eosio.bpay`, `standby_bucket`) | 4.8% | 30,284 (10,095 per seat) |
| voters (`eosio.voters`) | 40.0% | 252,386 |
| `eosio.saving` | 30.0% | 189,290 |

## Interaction matrix — what else moves when you change one thing

Every parameter is a term of the same formula, so none of them is independent. Read the row for
the parameter you are changing **before** you draft the proposal.

| You change | Also changes, without any other action | What you should do about it |
|---|---|---|
| `annual_rate_bps` | Every bucket scales by the same factor: block pay, standby pay, voter pay, `eosio.saving` (hence WPS funding) and the RNG carve-out. Fee burn: fees offset issuance first, so at a lower rate more PowerUp fee revenue is burned. | Nothing structural. Announce; check `eosio.saving` budget assumptions. |
| `producers_bps` | Both active **and** standby pay (the standby bucket is a slice of the producers share). | If you only want active pay to move, also adjust `standby_slot_weight`. |
| `voters_bps` | Voter yield; nothing else. | — |
| `savings_bps` | `eosio.saving` inflow, i.e. what WPS and treasury spending draw on. | — |
| `rng_rate` | Reduces every other destination by `(1 − rng_rate/10000)`; a full pool or a non-allow-listed `orng.wax` hash silently returns the carve-out to the split. | Confirm the hash is allow-listed and the pool has headroom, or the change does nothing. |
| `num_standby_slots` | Active-producer pay moves the other way (same producers share, different `A/(A+S)`). Seats are reconciled at the next election; a standby dropped by a cut keeps its accrued share and may still claim it. Configured empty seats still divert pay into the standby bucket. | Check there are enough eligible candidates for the new count; see the standby runbook. |
| `standby_slot_weight` | Active-producer pay, inversely. | — |
| `active_producer_count` | Who produces (next election, within ~1 min, active a few minutes later); per-producer pay (`1/count` of a changed bucket); standby pay moves inversely (`S/(A+S)`); the ranking boundary moves, so the former #N+1 becomes/stops being a standby and the last standby is pushed out/in; **`min_bps_voting_reward` is clamped down only** — it is never raised. | Decide `min_bps_voting_reward` for the new count and set it in the same proposal (`setprodcnt` first). One step of ±1 per day, no more. |
| `min_bps_voting_reward` | Voter-pay eligibility — but a voter's accrual is only re-evaluated on their **next `voteproducer`**; nobody gains or loses accrual until they re-vote. Proxied voters are exempt. | Announce the new minimum and tell voters to re-cast their votes. |
| `min_cooldown_secs` | How fast the schedule can be resized. | Leave at 86400 unless an incident requires otherwise. |
| `min_producer_vote_threshold`, `max_considered_producers` | Which producers are even candidates for active **or standby** seats. | Not covered by these runbooks; see the architecture overview §1. |

Things that look like parameters but are not: the `pervote_bucket` (always 0 on WAX — no
per-vote producer pay), the 30-producer cap on a vote, the 24-hour claim interval, and the
`unpaid_blocks` accounting (see "Known state quirks" below).

## Procedure common to all four runbooks

### 0. Pre-flight reads

```bash
API=https://wax.greymass.com
cleos -u $API get table eosio eosio global.c          # inflation policy
cleos -u $API get table eosio eosio global5           # rng_rate, max_pool_rng
cleos -u $API get table eosio eosio global.b          # ORNG hash allow-list
cleos -u $API get code orng.wax                       # compare with global.b
cleos -u $API get table eosio eosio global4           # standby + producer-count controls
cleos -u $API get table eosio eosio global | jq '.rows[0] | {perblock_bucket, voters_bucket, total_unpaid_blocks, last_pervote_bucket_fill, last_producer_schedule_size}'
cleos -u $API get schedule                            # active + pending schedule
cleos -u $API get table eosio eosio standbys --limit 50
cleos -u $API multisig propose --help >/dev/null      # sanity: cleos present
```

Expect the validator to refuse **every** setter if any stored term is already out of bounds
(`validate_inflation_policy()` checks the stored state, not just the argument). Mainnet is inside
every bound as of 2026-10-26; if a refusal names a term you did not touch, fix that term first.

### 1. Propose

Any account with free RAM may propose (recent deploys used `4tioi.waa`; the deploy script's default
is `admin2.wax`). The requested approvals are the three `admin*.wax@active` keys; two must approve.

```bash
REQ='[{"actor":"admin1.wax","permission":"active"},{"actor":"admin2.wax","permission":"active"},{"actor":"admin3.wax","permission":"active"}]'
AUTH='[{"actor":"eosio","permission":"active"}]'
PROPOSER=4tioi.waa
cleos -u $API multisig propose <proposal_name> "$REQ" "$AUTH" eosio <action> '<json args>' $PROPOSER 168 -p $PROPOSER@active
```

For a proposal that carries **more than one action** (for example `setprodcnt` followed by
`setminbpvote`), build an unsigned transaction and use `propose_trx`:

```bash
cleos -u $API push action eosio setprodcnt '{"count":6}' -p eosio@active -s -d -j -x 604800 > /tmp/trx.json   # -s -d -j = unsigned, don't broadcast, print JSON
# edit /tmp/trx.json: append the second action to .actions[] in the order they must run
cleos -u $API multisig propose_trx <proposal_name> "$REQ" /tmp/trx.json $PROPOSER -p $PROPOSER@active
```

### 2. Review and approve (two of the three admins)

```bash
cleos -u $API multisig review $PROPOSER <proposal_name>          # each approver reads the decoded actions
cleos -u $API multisig approve $PROPOSER <proposal_name> '{"actor":"admin1.wax","permission":"active"}' -p admin1.wax@active
cleos -u $API multisig approve $PROPOSER <proposal_name> '{"actor":"admin2.wax","permission":"active"}' -p admin2.wax@active
```

### 3. Execute, then verify

```bash
cleos -u $API multisig exec $PROPOSER <proposal_name> -p $PROPOSER@active
cleos -u $API get table eosio eosio <table>           # the row shows the new value immediately
```

The setter itself settled the buckets, so the transaction's traces also show the transfers of the
final interval under the **old** policy (`fund bpay bucket`, `fund voters bucket`,
`unallocated inflation`). The first claim after the change settles under the new one.

### 4. Undo

Every setter is its own undo: propose the previous value. The exception is `setprodcnt`, which
can only step back by one and only after the cooldown has elapsed again.

## A small calculator

Paste the live `global4`, `global5` and `global.c` values in; it prints the daily split under any
candidate policy, so the proposal text can state what will change in WAX per day.

```python
import math
supply = 4_707_350_510.919          # cleos get currency stats eosio.token WAX
rate_bps, p_bps, v_bps = 500, 3000, 4000     # global.c (savings = remainder)
rng_bps, rng_live = 100, False                # global5 rng_rate; rng_live = hash allow-listed AND pool below max
apc, slots, weight = 7, 3, 4444               # global4
cont = math.floor(math.log1p(rate_bps / 10000) * 1e5) / 1e5
day = supply * cont / 364
rng = day * rng_bps / 10000 if rng_live else 0
rest = day - rng
prod = rest * p_bps / 10000; A = apc * 10000; S = weight * slots
active, standby = prod * A / (A + S), prod * S / (A + S)
voters = rest * v_bps / 10000; savings = rest - prod - voters
print(f"day {day:,.0f}  rng {rng:,.0f}  active {active:,.0f} ({active/apc:,.0f} each)  "
      f"standby {standby:,.0f} ({standby/slots if slots else 0:,.0f} per seat)  voters {voters:,.0f}  savings {savings:,.0f}")
```

## Known state quirks the examples assume

- **RNG carve-out is inert on mainnet today, for two independent reasons.** `global.b` allow-lists
  `4e516c80…` but `orng.wax` runs `c913ee95…`, so `verify_orng_contract()` fails; and the `orng.wax`
  `treasury` singleton reports `pool_balance` = 6,000,000 WAX, exactly `max_pool_rng`, so even a
  verified hash would deposit 0. The 1% falls through to the split. The RNG runbook covers
  re-enabling it and the alternative of setting the rate to 0 so the state says what it does.
- **`perblock_bucket` carries ~3.0 M unpaid blocks from the 2019 genesis producers** (`producera` …
  `producert`, inactive since 2019-06-24, never claimed). They cannot claim (`claimrewards` requires
  an active key), so about 3.2 M WAX sits in `eosio.bpay` permanently and every producer's daily
  claim is `perblock_bucket × own / total`. This is steady-state: the bucket holds ~20 days of
  inflow instead of ~1, and daily pay still equals daily inflow. Removing it needs a contract change.
- **Standby pay is claimed lazily.** `eosdacserver` has never claimed and holds 44.6% of
  `total_standby_share` (≈1.67 M WAX of today's bucket). Cutting slots or weight does not touch
  accrued shares or the bucket.
