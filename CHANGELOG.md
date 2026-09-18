# Changelog

## Pending (wax-2.1.12-X.Y.Z)

BREAKING CHANGES:

FEATURES:

IMPROVEMENTS:

BUG FIXES:

- `cleanvotes` may not be run on a proposal that is taking votes, and only a reviewer of the
  proposal's committee may run it while the row exists (WCAP-SYS-2026-016). It edits voters'
  lists without touching the proposal's tally, so on a live proposal it let a re-vote count
  twice and left the voter unable to withdraw. Its range is now bounded by the voter table.
  `voteproposal` stores only the votes that were actually tallied, so a vote cast for a
  proposal that was not yet (or no longer) taking votes can no longer be subtracted later
  from a proposal that never received it.

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
