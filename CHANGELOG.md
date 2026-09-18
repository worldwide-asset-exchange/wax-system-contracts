# Changelog

## Pending (wax-2.1.12-X.Y.Z)

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
  larger default, and `get_bp_weight_multiplier` clamps both the default and any score read
  from the guilds table before computing the vote multiplier. The ceiling is 38x the largest
  live score, so no current multiplier changes; it only stops an absurd value from reordering
  the producer schedule. Zero remains valid.

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
