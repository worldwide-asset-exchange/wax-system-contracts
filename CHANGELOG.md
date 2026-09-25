# Changelog

## Pending

BREAKING CHANGES:

FEATURES:

IMPROVEMENTS:

BUG FIXES:

- `setramrate` refuses more than `max_new_ram_per_block` (8,192 bytes per block, about 1.4 GB
  per day; mainnet runs 0) with a message that names the bound (WCAP-SYS-2026-017). Previously
  any `uint16_t` was stored, and the maximum, 65,535, would have grown RAM supply by about 11 GB
  per day - 3.6% of today's pool - irreversibly, since `setram` only ever increases.
  `update_ram_supply` also computes `slots * rate` in 64 bits: the 32-bit product wrapped once
  a gap with no RAM-market action exceeded 2^32 / rate slots (9 hours at the old maximum),
  under-crediting the supply.

## wax-3.3.1

Security review performed internally using Claude (Anthropic) — Opus 5 for audit phases 0–5,
Fable 5.1 for remediation and reporting. Every entry below names its WCAP finding or ticket.

BREAKING CHANGES:

- The msig-only `removerefund` action is removed (WBP-2010; closes WCAP-SYS-2026-004 and
  -012). It has had no in-contract caller since GBM was removed in 2023 (`a1461f7`), no
  ricardian clause and no test. ABI change: after `setcode`, a transaction still naming it
  executes as a silent no-op (the dispatcher ignores an unimplemented action name), so any
  `eosio.msig` proposal carrying it must be cancelled before deploy and, if needed,
  re-proposed as a new reviewed action; decoding historical traces of the action needs the
  pre-removal ABI.

FEATURES:

IMPROVEMENTS:

- New test suite `eosio_guilds_tests` (WBP-2002): authorisation matrix and behaviour tests for
  the `guilds.oig` contract at `718903f`, run against its checked-in, sha256-pinned wasm/abi
  in `tests/test_contracts/guilds/`. Tests only; no contract change.

- The three remaining WPS reg/edit pairs (`regproposer`/`editproposer`, `regreviewer`/
  `editreviewer`, `regcommittee`/`edcommittee`) validate their inputs through one shared
  helper each instead of two inline copies (WBP-1998; the WCAP-SYS-2026-003 shape). No
  bound changed. Four messages did: the bio limit no longer says "description", the
  linkedin limit no longer says "linked URL", the empty-image-URL message says what is
  checked ("image URL should be more than 0 characters long", not "not a valid image
  URL"), and `edcommittee` and `rmvreviewer` use their pair's wording for a missing
  account ("committeeman account doesn't exist", "The reviewer account does not exist").

BUG FIXES:

- `editproposal` now enforces the same 30-day minimum `duration` as `regproposal`
  (WCAP-SYS-2026-003). Previously a PENDING proposal could be edited down to 1 day and
  its entire funding goal claimed within 24 hours of approval. Both actions now validate
  through one helper. Along the way: the `total_iterations` floor is 1 on both actions
  (edit had kept the pre-2020 value of 3); `editproposal`'s maximum-duration message is
  now "this proposal is over the maximum duration"; four length-limit messages now state
  the limit the check actually enforces; and `setwpsenv` rejects a
  `max_duration_of_funding` below 30.

- BP guild scores are capped at 100,000,000 (WCAP-SYS-2026-005): `setbpdefscore` rejects a
  larger default, and `get_bp_weight_multiplier` clamps any score read from the guilds table
  before computing the vote multiplier. The ceiling is 38x the largest live score, so no
  current multiplier changes. It bounds the magnitude of a bad value, not the ranking - a
  score near the ceiling still outweighs every live score - so the control over who writes
  the guilds table is unchanged by this. Zero remains valid.

- Three privileged setters compared an unsigned parameter with `>= 0`, which is always true
  (WCAP-SYS-2026-002). `setsbratio` and `setrngrate` now enforce only their real upper
  bounds, with messages that name the constant compared against; `setsbslot` accepts zero
  (disables standbys, as before) and gains a ceiling of 21, the same as the active schedule.
- `claimstandby` computes the payout in integers and clamps it to the standby bucket, as
  `collect_voter_reward` already did for the voters bucket (WCAP-SYS-2026-006). Filed as a
  missing safety net, not a demonstrated exploit: the old double arithmetic would have
  trapped rather than over-paid. A reward that rounds to zero is now refused before the
  bucket is touched instead of failing inside the token transfer.

- `global.total_producer_vote_weight` is clamped at zero alongside each producer's
  `total_votes` (WCAP-SYS-2026-007). Previously the per-producer value was clamped when
  floating-point residue drove it negative but the global accumulator was not, so after
  every vote was withdrawn the global read a large negative number (-2^56 on a test chain).
  The published value is unchanged until the first time the accumulator would have gone
  negative, which a chain with continuous voting never reaches; the field remains a running
  accumulator, not a recomputed sum. The proxy-weight path now clamps the per-producer
  value too, and the disabled `total_votes >= 0` assertion is restored.

- `cleanvotes` may not be run on a proposal that is taking votes, and only a reviewer of the
  proposal's committee may run it while the row exists (WCAP-SYS-2026-016). It edits voters'
  lists without touching the proposal's tally, so on a live proposal it let a re-vote count
  twice and left the voter unable to withdraw. Its range is now bounded by the voter table.
  `voteproposal` stores only the votes that were actually tallied, so a vote cast for a
  proposal that was not yet (or no longer) taking votes can no longer be subtracted later
  from a proposal that never received it.

- `regproposal` and `editproposal` now refuse a `funding_goal` that is not denominated in
  the core symbol at the core precision (WCAP-SYS-2026-011). Previously only `is_valid()`
  and `amount > 0` were checked, so a proposal in a foreign token, or in the core token at
  the wrong precision, could be registered, reviewed, voted on and approved, and failed only
  at `claimfunds` - leaving it APPROVED and unclaimable, removable only by the committee
  (`rejectfund`, then `rmvreject`) and never by the proposer. Both actions also require at
  least one minimum unit per instalment: `claimfunds` pays `funding_goal / total_iterations`
  with truncating division and `eosio.token` refuses a zero transfer, so a smaller goal
  reached the same approved-but-unclaimable state through arithmetic.

- `fill_buckets` computes its two scaled products in 128 bits (WCAP-SYS-2026-014). The
  producer/standby split `total_block_pay * active_producers * PAY_SPLIT_SCALE` wrapped
  `uint64` once a single fill covered about 4.7 days at mainnet supply - buckets fill only
  inside claims, so a chain halt of that length followed by the first claim credited the
  standby bucket with a wrong remainder, and `distribute_tokens * rng_rate` wrapped
  after about 30 days at the maximum permitted rate. No behaviour change for gaps that fit.

- The producer election reconciles the standby table on every run, including when the
  elected standby set is empty (WCAP-SYS-2026-013). Previously `setsbslot(0)` left the
  rows elected under the old setting active: they kept accruing standby share while
  standbys were disabled, could claim it, and took the refilled bucket ahead of the newly
  elected standbys when slots were re-enabled. A deactivated row still claims what it
  earned while active. `setsbslot` and `setsbratio` now settle the pay buckets under the
  split in force before changing it, so a new value applies from that point rather than
  to the whole unfilled interval.

- `claimfunds` computes the instalment cadence in 64 bits and applies it to a 64-bit time
  point (WCAP-SYS-2026-010). Previously `duration * seconds_per_day` was narrowed into a
  `uint32` and the instalment offset was added to a 32-bit `time_point_sec`, so a permitted
  but very long duration wrapped and made instalments claimable early. The voting-window
  check had the same 32-bit product and is computed in 64 bits too. Both `wpsenv` durations
  are capped at 36,500 days, and a proposal's duration is checked against that ceiling when
  it is registered or edited and again when funds are claimed.

## wax-2.10.12-3.0.0

BREAKING CHANGES:

FEATURES:

IMPROVEMENTS:
- voters reward bumped to 2%
- producers sorted by location
- genesis tokens no longer submitted on claimgbmprod nor claimgbmvote. GBM is over and these actions are equivament to claimrewards and voterclaim, respectively

BUG FIXES:

## wax-1.7.0-2.1.1

BREAKING CHANGES:

FEATURES:

IMPROVEMENTS:

BUG FIXES:
- fix more WPS checks

## wax-1.7.0-2.1.0

BREAKING CHANGES:

FEATURES:

IMPROVEMENTS:
- Deployment script

BUG FIXES:

## wax-1.7.0-2.0.2

BREAKING CHANGES:

FEATURES:

IMPROVEMENTS:

BUG FIXES:
- wps global stake tracking fix

BREAKING CHANGES:

## wax-1.7.0-2.0.1

BREAKING CHANGES:

FEATURES:

IMPROVEMENTS:
-relax wps checks

BUG FIXES:

BREAKING CHANGES:

FEATURES:
- Worker proposal system

IMPROVEMENTS:

BUG FIXES:

## (wax-1.7.0-1.3.0)

BREAKING CHANGES:

FEATURES:

IMPROVEMENTS:

BUG FIXES:

## wax-1.7.0-1.2.0

BREAKING CHANGES:
- [KEW-1650] Updated GBM rewards dispensing.

FEATURES:
- [KEW-1394] Token burn for unstaking

IMPROVEMENTS:

BUG FIXES:

## wax-1.7.0-1.1.0

BREAKING CHANGES:

FEATURES:

IMPROVEMENTS:
- [KEW-1468] Removed REX referencies.
- [KEW-1463] Clean up recommendations

BUG FIXES:

## wax-1.7.0-1.0.0

BREAKING CHANGES:
- [KEW-1396] Upgrade to 1.7.0-1.0.0.

FEATURES:

IMPROVEMENTS:

BUG FIXES:

## wax-1.5.2-1.1.2

BREAKING CHANGES:

FEATURES:

IMPROVEMENTS:

BUG FIXES:
- [KEW-1405] Fix for prod per block pay

## wax-1.5.2-1.1.1

BREAKING CHANGES:

FEATURES:

IMPROVEMENTS:
- [KEW-1348] README updated

BUG FIXES:

## wax-1.5.2-1.1.0

BREAKING CHANGES:

FEATURES:

IMPROVEMENTS:
- [KEW-1302] Retroactively award missing genesis awards

BUG FIXES:
