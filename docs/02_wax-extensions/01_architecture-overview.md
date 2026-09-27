---
content_title: WAX extensions to eosio.system — architecture overview
link_text: WAX extensions to eosio.system — architecture overview
---

This page describes the parts of `eosio.system` that WAX added on top of the upstream Antelope system
contracts (the base is the leap 3.1.0 generation of `eos-system-contracts`; `wax-3.x` is WAX's own
numbering). The upstream mechanics — RAM market, staking, NET/CPU, plain voting, multisig — are
covered by the [key concepts](../01_key-concepts/01_system.md) pages and are not repeated here. What
is WAX-specific is how producers are **ranked**, who gets **paid** and when, the **worker proposal
system**, and the knobs that govern all three. Every statement below names the source it was read
from, at tag `wax-3.3.2`; where a public test pins the behaviour, it is named too.

Paths are relative to `contracts/eosio.system/`: `src/` for `.cpp`, `include/eosio.system/` for headers.

## The shape of it

Four subsystems share one clock and one pot:

```
                 every block                          when someone claims
              ┌──────────────┐                     ┌────────────────────────┐
   nodeos ──► │   onblock    │                     │ claimrewards           │
              │ • blockinfo  │                     │ voterclaim             │
              │ • unpaid blk │      ┌────────────┐ │ claimstandby           │
              │ • ~1/min:    │      │  election  │ │ setsbratio / setsbslot │
              │   election ──┼────► │  ranking   │ └───────────┬────────────┘
              │ • name bids  │      └─────┬──────┘             │ fill_buckets()
              └──────────────┘            │                    ▼
                                          │   inflation 4.879% cont. (≈5%/yr) on supply, fees first
                                          │   ├─ RNG carve-out (rng_rate, allow-listed orng.wax)
                                          │   └─ rest: 3/10 producers · 4/10 voters · 3/10 eosio.saving
                                          │          └─ producers split active : standby by slot weight
                                          ▼
                              active schedule + standby list
                                          ▲
                              guilds.oig score × votes (fail-open to plain votes)
```

Two facts about this picture matter more than any other:

- **`onblock` never mints.** Inflation is settled lazily, by `fill_buckets()`
  (`src/producer_pay.cpp:76`), which runs only inside `claimrewards`/`claimgbmprod`, `voterclaim`/`claimgbmvote`,
  `claimstandby`, `setsbratio` and `setsbslot`. Between claims the buckets simply fall behind and catch up on the
  next call; the arithmetic is 128-bit so a long gap cannot wrap it (`wcap_014_*` tests in `tests/eosio.system_tests.cpp`).
- **`onblock` must not throw.** If it did, nodeos would log and skip it rather than halt, and the election, standby
  rotation, unpaid-block accounting and name-bid closes would freeze on their last state until a contract fix
  shipped by multisig. A CI ratchet (`.audit/rules/wcap-check.py` in this repository, class D3, run by
  `.github/workflows/wcap-static.yml`) keeps the `onblock` call graph free of `check()` and throwing table reads.

## 1. Producer election and weighting

**When.** `onblock` (`src/producer_pay.cpp:10`) calls `update_elected_producers` once the schedule is older than
120 half-second slots — about once a minute — and only after the chain has activated (see §6).

**Candidates** (`src/voting.cpp:114-217`). The producer table is walked in raw-vote order. The walk stops at the
first producer that is inactive, whose `total_votes` is not above `min_producer_vote_threshold`, or once
`max_considered_producers` have been seen. Weighting therefore only ever *reorders* producers already inside
the top-N-by-votes; it cannot pull a producer in from below the threshold or the cap.

**Weighting.** Each candidate's weighted vote is `total_votes × get_bp_weight_multiplier(owner)`
(`src/voting.cpp:633-657`), sorted descending, ties by name. The multiplier is

```
score / bp_score_scaling_factor      where score = min(guilds.score, max_bp_score), or bp_default_score if the
                                     producer has no row in the guilds contract's `guilds` table
```

read from the external `guilds.oig` contract (`include/eosio.system/guilds.oig.hpp`). `max_bp_score` is
100,000,000, a hard ceiling on how much any score can amplify a vote; a score of 0 sorts the producer last but
does not remove it.

**Fail-open.** `verify_guilds_contract()` (`src/voting.cpp:570-602`) returns false — and every multiplier becomes
1.0, i.e. plain vote order — when weighted voting is disabled (`setenablewv`), when the allow-list of guilds
code hashes is empty, when the configured guilds account does not exist or has no code, or when its deployed code
hash is not on the allow-list. **Consequence for operators:** deploying a new `guilds.oig` build without first
`addguildhash`-ing its hash does not fail; it silently switches the chain to unweighted voting until the hash is
listed. Pinned by `test_guild_hash_empty_disabled`, `test_admin_disabled_guild_weight`, `test_verify_hash_not_match`
in `tests/eosio.weighted_producer_tests.cpp`.

**Active set and standbys.** The first `active_producer_count` weighted producers become the schedule (re-sorted
by location, then name, before `set_proposed_producers`). The next weighted positions fill up to
`num_standby_slots` standby seats, skipping anyone on the `sbdisallow` list without consuming a slot. The standby
list is updated on every election, even when it is empty, so lowering the slot count deactivates the surplus
rows (`wcap_013_setsbslot_zero_deactivates_standbys`).

**Knobs** (all `require_auth(eosio)`, `src/eosio.system.cpp:522-600` and `src/voting.cpp:659-690`):

| Action | Bounds | Stored in |
| --- | --- | --- |
| `setguildcont(contract)` | must be an account | `global.a` |
| `setbpscale(factor)` | > 0 (default 1000) | `global.a` |
| `setbpdefscore(score)` | ≤ `max_bp_score` (default 1000) | `global.a` |
| `setmaxprod(n)` | 21 ≤ n ≤ 500 (default 100) — the candidate cap | `global.a` |
| `setminvote(votes)` | ≥ 0 — the candidate threshold | `global.a` |
| `setenablewv(bool)` | kill switch for weighting | `global.a` |
| `addguildhash` / `rmguildhash` | no duplicates / must exist | `global.a` |
| `setprodcnt(count)` | 1 ≤ count ≤ 21, **±1 per call**, cooldown `min_cooldown_secs` (default 86,400 s); clamps `min_bps_voting_reward` down to the new count | `global4` |
| `setprodctrl(secs)` | ≥ 600 — the cooldown itself | `global4` |
| `setminbpvote(m)` | 0 < m ≤ `active_producer_count` — producers a voter must pick to earn voter pay | `global4` |

WAX mainnet runs a small active set by governance choice (single digits in 2026). The ±1 rule with a one-day
cooldown makes each step an explicit, observable action; the finality arithmetic for small sets is the
inherited classic-DPoS ⌊(N−1)/3⌋ tolerance.

## 2. Standby pay

Standby producers are paid for **time spent on the standby list**, not for blocks (`src/standby.cpp`).

- `update_standby_producers` (called by each election, `:91-146`) marks rows active or inactive and accrues
  `standby_share` in microseconds of active time; `update_standby_share` (`:67-89`) brings every active row and
  `total_standby_share` up to date.
- `claimstandby(owner)` (`:148-184`, `require_auth(owner)`) pays `standby_bucket × share / total_share` in
  128-bit integer arithmetic, once per day, and refuses rather than pays if the row has no share, the share
  exceeds the total, or the amount would exceed the bucket (fail closed; `wcap_006_bucket_solvency_property`).
  Paid from `eosio.bpay`. A deactivated row keeps its accrued share and may still claim it.
- The standby bucket's size is set at fill time (§3) by `standby_slot_weight` (`setsbratio`, ≤ 10,000 =
  `PAY_SPLIT_SCALE`) and `num_standby_slots` (`setsbslot`, ≤ 21). Both setters settle the buckets first so a
  change never re-prices already-accrued pay.
- `allowsb` / `disallowsb` maintain the `sbdisallow` table (`require_auth(eosio)`).

State: table `standbys` (`owner, standby_share, last_standby_share_update, last_claim_time, is_active`) and
singleton `global4` (`standby_bucket, total_standby_share, standby_slot_weight, num_standby_slots,
last_standby_state_update` plus the producer-count controls above).

## 3. Producer pay and the inflation split

`fill_buckets()` (`src/producer_pay.cpp:76-155`), on every claim:

1. `distribute = continuous_rate × supply × Δt / year`, with `continuous_rate = 0.04879` (5% annual).
   The `eosio.fees` balance (PowerUp fees) is spent first and only the shortfall is issued; any fee balance left
   over is retired.
2. **RNG carve-out:** `rng_rate / 10,000` of `distribute` is deposited to the ORNG treasury (`orng.wax`) — only if
   that account's code hash is on the `global.b` allow-list (`addornghash`/`rmornghash`/`setornghash`) and capped
   at `max_pool_rng` minus the treasury's current pool. If verification fails, the carve-out stays with the
   producer split. `setrngrate(rng_rate, max_pool_rng)` requires `rng_rate < 10,000`.
3. **The split** of what remains: `3/10` per-block producer pay, `4/10` voters, and the remainder (3/10) to
   `eosio.saving`, transferred to `eosio.bpay`, `eosio.voters` and `eosio.saving` respectively.
4. **Producers : standbys.** The per-block share is divided by weight: active producers weigh
   `active_producer_count × 10,000`, standbys `standby_slot_weight × num_standby_slots` (the *configured*
   slot count, not the number actually seated). The producer part goes to `perblock_bucket`, the rest to
   `standby_bucket`.

`claimrewards(owner)` and `claimgbmprod(owner)` (`:158-227`; `require_auth(owner)`, active producer, chain
activated, once per day) pay `perblock_bucket × unpaid_blocks / total_unpaid_blocks` from `eosio.bpay`. **There is
no per-vote producer pay on WAX**; the upstream `pervote_bucket` and vote-pay constants are unused, and
`claimgbmprod` is an alias of `claimrewards` (`claimgbmprod_is_claimrewards_after_gbm`).

**Voter pay.** `voterclaim(owner)` / `claimgbmvote(owner)` (`src/voting.cpp:430-479`) pay
`voters_bucket × unpaid_voteshare / total_unpaid_voteshare` from `eosio.voters`, once per day, after the chain
has activated. Vote share accrues at the voter's **own** vote weight (proxied weight excluded) and only while the
voter picks at least `min_bps_voting_reward` producers or delegates to a proxy
(`update_voter_votepay_share`, `:481-507`; `min_bps_voting_reward_case`, `voter_pay_gstate_consistency`).

**Genesis balances (GBM).** `claimgenesis(claimer)` (`src/delegate_bandwidth.cpp:479-510`) vests a locked
genesis balance linearly over 1 July 2019 – 1 July 2022 and pays from `genesis.wax`; the vesting window is
closed, so the action now only releases whatever remains unclaimed, and `undelegatebw` burns the unvested part
of any withdrawn genesis stake (`change_genesis`, `:519-580`). No action awards new genesis balances.

**Conservation.** Every unit issued by `fill_buckets` lands in exactly one of `eosio.saving`, `eosio.voters`,
`eosio.bpay` (producer or standby bucket) or the RNG treasury; the split is exact to the base unit
(`producer_pay`, `adjust_chain_inflation`, `votepay_share_invariant` in `tests/eosio.system_tests.cpp`).

## 4. Worker proposal system (WPS)

`src/wps.cpp` funds community proposals from `eosio.saving` after a stake-weighted vote. One proposal per
proposer at a time (the `proposals` table is keyed by proposer).

**Roles and who signs.**

| Actions | Authority |
| --- | --- |
| `regcommittee`, `edcommittee`, `rmvcommittee`, `setwpsenv`, `setwpsstate` | `eosio` |
| `regreviewer`, `editreviewer`, `rmvreviewer` | the committee account |
| `rejectfund` | a committee account: the proposal's own, or any committee registered with `is_oversight` |
| `acceptprop`, `rejectprop`, `approve`, `rmvreject`, `rmvcompleted`, `cleanvotes` | a reviewer of the proposal's committee |
| `regproposer`, `editproposer`, `rmvproposer`, `regproposal`, `editproposal`, `claimfunds` | the proposer |
| `voteproposal` | the voter |

**Lifecycle** (status values in `include/eosio.system/eosio.system.hpp:395-402`):

```
regproposal ──► PENDING ──acceptprop──► ON_VOTE ──(tally passes total_voting_percent)──► FINISHED_VOTING ──approve──► APPROVED ──claimfunds × N──► COMPLETED
                  │                        │                                                  │                          │
                  └── rejectprop ──────────┴──────────────────────────────────────────────────┘        rejectfund (proposal's committee or any
                  (also ON_VOTE → REJECTED when duration_of_voting elapses, evaluated lazily on the next vote)        oversight committee) ──► REJECTED
```

- **Admission bounds** (`validate_proposal_fields`, `:190-236`, shared by `regproposal` and `editproposal`):
  duration 30 days to `max_duration_of_funding` and never above the contract's own ceiling `max_wps_duration_days`
  (36,500); 1–99 iterations; `funding_goal` in the core symbol at its precision, positive, and at least one
  minimum unit per iteration so no instalment truncates to zero; string-length and member-count limits.
  Every message is asserted by `tests/eosio.wps_tests.cpp`.
- **Voting** (`update_wps_votes`, `:678-792`): at most 30 distinct proposals; the voter must have a stake row and
  the chain must be activated; proxies may not vote on proposals; weight is `stake2vote(staked)` (§6). Only
  proposals actually tallied (ON_VOTE) are recorded against the voter, and every stake change re-runs the
  voter's proposal votes (`src/delegate_bandwidth.cpp:455-458`). `wps_state.total_stake` — the denominator of the
  threshold — moves with every stake delta (`:426`) and can be reset by `setwpsstate`.
- **Funding cadence** (`claimfunds`, `:126-184`): instalment *i* of `total_iterations` is claimable at
  `fund_start_time + i × duration / total_iterations` (64-bit), each paying `funding_goal / total_iterations` by an
  inline `eosio.token::transfer` from `eosio.saving`. After the last instalment the proposal is COMPLETED.
- **Environment** (`setwpsenv`): `total_voting_percent` (> 0), `duration_of_voting` (1–36,500 days),
  `max_duration_of_funding` (30–36,500 days), `total_iteration_of_funding` (> 0; stored for ABI compatibility,
  read by no code path).

There is no `onblock` involvement: voting windows expire only when the next vote arrives, and nothing is paid
without a claim.

## 5. RAM supply growth

- `setram(max_ram_size)` (`src/eosio.system.cpp:83-101`): increase only, below 1 PiB, above what is reserved; the
  delta is added to the Bancor market's base so the price moves smoothly.
- `setramrate(bytes_per_block)` (`:124-131`): at most `max_new_ram_per_block` = 8,192 bytes per block (about
  1.4 GB per day); 0 disables growth. `update_ram_supply()` (`:103-122`) accrues `slots × rate` in 64-bit
  arithmetic and runs from `buyram`, `sellram`, `ramtransfer` and `ramburn` — not from `onblock` — so accrual is
  lazy like the buckets (`ram_inflation`, `wcap_017_setramrate_ceiling_and_supply_wrap`).

## 6. Vote weight, proxies and activation

- `stake2vote(staked) = staked × 2^(⌊weeks since 2000-01-01⌋ / 13)` (`src/voting.cpp:219-223`): a stake's vote
  weight doubles every 13 weeks, four times faster than upstream's yearly doubling, so recent stake outweighs
  old stake in both producer and WPS votes. (The header comment still says "per year"; the code is /13.)
- A voter picks up to 30 producers, sorted and unique, **or** a proxy — never both; a proxy cannot use a proxy.
  Proxied weight propagates with `propagate_weight_change` and is excluded from the proxy's own voter-pay
  accrual (§3).
- **Activation.** The chain "activates" the first time `total_activated_stake` reaches `min_activated_stake`
  (1.5 × 10¹² base units) as votes are cast; until then `onblock` does not run elections, `undelegatebw` and
  WPS voting are refused, and rewards cannot be claimed. Mainnet crossed this at launch; the tester crosses it
  with `cross_15_percent_threshold()`.
- `global.total_producer_vote_weight` is a running accumulator clamped at zero, kept for off-chain readers;
  nothing in the contract reads it (`vote_weight_global_*` tests).

## 7. Invariants the tests hold

| | Property | Where it is pinned |
| --- | --- | --- |
| I1 | **Conservation** — every unit `fill_buckets` issues is in exactly one bucket or account; the 3/10 : 4/10 : 3/10 split is exact | `producer_pay`, `adjust_chain_inflation`, `votepay_share_invariant` (`tests/eosio.system_tests.cpp`); the no-wrap companion is `wcap_014_*` |
| I2 | **No double claim** — every claim action fails closed and enforces its one-day guard | `claim_genesis_*`, `standby_claims`, `voter_pay`, `claimgbmprod_is_claimrewards_after_gbm` |
| I3 | **Schedule determinism** — the election depends only on chain state and the deployed bytecode (WASM `f64` is deterministic; the code hash is consensus-enforced) | `test_producer_sorting_with_votes_and_scores`, `elect_producers`, the pinned vote-weight value in `tests/eosio.weighted_producer_tests.cpp` |
| I4 | **Solvency** — no claim can pay more than its bucket holds; standby share ≤ total share | `wcap_006_bucket_solvency_property`, `wcap_006_fractional_standby_reward_is_refused_cleanly` |
| I5 | **Authority monotonicity** — `limitauthchg` opt-ins are enforced on every authority-changing action | `tests/eosio.limitauth_tests.cpp` |
| I6 | **Bounded cost** — election work scales with the candidate set (cap ≤ 500, threshold), and nothing reachable from `onblock` can throw | `test_max_considered_producers_filters_standby_bps`, `test_min_vote_threshold_filters_*`, D3 ratchet in `.audit/rules/wcap-check.py` |
| I7 | **Upgrade integrity** — the deployed `eosio` code reproduces bit-for-bit from the tagged source in the pinned image | `.audit/verify-hashes.sh`, `deploy/system-contract.sh verify <tag>` |

## 8. Where the state lives

| Singleton (table name) | Holds |
| --- | --- |
| `global` | upstream fields plus `voters_bucket`, `total_voteshare_change_rate`, `total_unpaid_voteshare`, `total_unpaid_voteshare_last_updated` |
| `global2` | `new_ram_per_block`, `last_ram_increase`, `total_producer_votepay_share`, `revision` |
| `global3` | `last_vpay_state_update`, `total_vpay_share_change_rate` |
| `global4` | standby bucket and slots; `active_producer_count`, `min_cooldown_secs`, `last_change_time`, `min_bps_voting_reward` |
| `global5` | `rng_rate`, `max_pool_rng` |
| `global.a` | weighted voting: guilds contract, scaling factor, default score, candidate cap and threshold, enable flag, guilds code-hash allow-list |
| `global.b` | ORNG code-hash allow-list |
| `wpsglobal`, `wpsstate` | WPS environment; `total_stake` |

Related material: `tokenomics/README.md` is the 2023 design note for the PowerUp / inflation change and
predates the 3/10 : 4/10 : 3/10 split and lazy settlement described here; `SECURITY.md` lists the supported
versions and the disclosure policy; `.audit/README.md` describes the static checks and the reproduction recipes
for the deployed contracts.

_Security review performed internally using Claude (Anthropic) — Opus 5 for audit phases 0–5, Fable 5.1 for remediation and reporting._
