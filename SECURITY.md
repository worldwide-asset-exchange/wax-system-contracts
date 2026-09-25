# Security Policy

This policy covers the WAX system contracts in this repository: `eosio.system`, `eosio.msig`,
`eosio.token`, `eosio.wrap`, `eosio.bios` and `eosio.boot`. These contracts hold the WAX
mainnet token supply, run producer elections and rewards, and gate every privileged change to
the chain, so a defect here is a defect in the chain itself. Please report anything you find
privately, and give us time to fix it before it is disclosed.

## Supported versions

Releases are git tags; the contract that secures WAX mainnet is the `eosio.system` WASM built
from a tag. Only the tag currently deployed on mainnet, and the `develop` branch it came from,
receive security fixes.

| Version | Supported | Notes |
|---|---|---|
| `develop` @ `e7739ed` (release tag to follow) | Yes | The 2026-09 review's remediations, merged; the next `eosio` deployment. |
| `wax-3.3.0` (`715ddba`) | Yes, until the above is deployed | Deployed on mainnet as `eosio` (`eosio.system`) since 2026-02-17. A reproducible build of this tag matches the on-chain code hash bit-for-bit — see [`.audit/verify-hashes.sh`](.audit/verify-hashes.sh). |
| `wax-3.2.x` and older | No | Superseded. |

Which tag is live is verifiable at any time for `eosio.system`: build the tag inside the image
pinned in the `Makefile` and compare the SHA-256 of the WASM with `get_code_hash` for the
`eosio` account.

The other three deployed contracts — `eosio.msig`, `eosio.token`, `eosio.wrap` — were deployed
from earlier tags of this repository, so `verify-hashes.sh` reports a mismatch for them
against the current source. Which tag and toolchain each one reproduces from is recorded in
the provenance table in [`.audit/README.md`](.audit/README.md) (`eosio.token` and
`eosio.wrap`: tag `wax-1.7.0-2.0.0`, bit for bit). A report against the deployed code of
those three is in scope.

## Reporting a vulnerability

**Do not open a public GitHub issue for a security problem.** Public issues are visible to
everyone, including anyone who could exploit the defect before it is fixed.

Email **security@wax.io** instead. Please include:

- the contract, action and source location (file and line, and the commit or tag);
- the impact as you understand it — what an attacker can do, and what they need to do it;
- steps to reproduce, ideally as a unit test against `tests/` or a `cleos` transcript against a
  local chain; and
- whether you have shared it with anyone else.

We do not publish a PGP key and do not offer an encrypted channel; reports are received by
plain email. Keep the first message to what is needed to triage (location, impact, how to
reproduce) and we will take the rest from there.

## What to expect

| Step | Target |
|---|---|
| Acknowledgement of your report | within **2 business days** |
| Triage and severity assignment | within **5 business days** |
| Fix prepared in private, with a regression test, and published when deployed | Critical/High: as fast as a safe fix allows · Medium: within 30 days · Low: next scheduled release |
| Status updates while a fix is in progress | at least every 14 days |

Severity is assessed on impact and likelihood using the matrix in the WAX Contract Audit
Protocol (WCAP), which is the same protocol applied in our internal reviews.

Deployment is slower than the fix. A change to a deployed system contract is executed through
the `eosio.msig` contract and requires approval from a supermajority of the active block
producers. That process takes days, not hours, and we cannot shorten it unilaterally. We will
tell you where the fix is in that process.

## Disclosure policy

We practise coordinated disclosure.

- We ask for a **90-day embargo** from the date we acknowledge the report, extended by mutual
  agreement if the fix is waiting on the producer multisig or on a nodeos release.
- We will not take legal action against, or report to authorities, anyone who researches these
  contracts in good faith: testing against a local chain or the WAX testnet, not against
  mainnet accounts they do not control, not degrading the network, and not accessing or
  destroying other people's data. If you are unsure whether something is in bounds, ask first.
- When the fix is deployed we will publish an advisory here on GitHub naming the finding, the
  fix and the affected versions, and will credit you unless you ask us not to.
- If we cannot fix a reported issue within the embargo we will say so and agree on a path with
  you, rather than let the embargo lapse silently.

## Scope

**In scope:** the contracts under `contracts/` in this repository, and the code deployed on WAX
mainnet under the accounts `eosio`, `eosio.msig`, `eosio.token` and `eosio.wrap` (see
"Supported versions" for which of those currently reproduce from this source); the tests under
`tests/`; the CI workflows under `.github/workflows/`; and `deploy-system-contract.bash`.

**Out of scope here, but still please report it to the same address:** the WAX node software
(`nodeos` and its host functions, in
[`wax-blockchain`](https://github.com/worldwide-asset-exchange/wax-blockchain)), the contract
development toolkit ([`wax-cdt`](https://github.com/worldwide-asset-exchange/wax-cdt)), and the
`guilds.oig` contract, which is deployed separately and whose data the election in
`eosio.system` reads. We will route it.

**Out of scope entirely:** third-party dApps and tokens deployed on WAX, the WAX Cloud Wallet
and other wax.io web properties (report those to the same address, but they are not governed by
this policy), social engineering, denial of service against public API nodes, and findings that
require a compromised block-producer key or a majority of the producer multisig as a
precondition.

## Security tooling in this repository

Contract-security checks run on every pull request, not once at audit time. The first two
below are wired into CI as ratchets in `.github/workflows/wcap-static.yml`: they pass on the
known baseline and fail on anything new. The third is run by hand, quarterly and at every
system-contract deployment.

- [`.audit/rules/wcap-check.py`](.audit/rules/wcap-check.py) — type-aware checks for the
  Antelope defect classes that regex cannot decide (missing authorisation, dead unsigned
  bounds, floating-point in a consensus comparator, disabled invariant checks);
- [`.audit/rules/antelope.yaml`](.audit/rules/antelope.yaml) — a semgrep ruleset for the
  pattern-detectable classes (notification forgery, overflow in rate arithmetic, unbounded
  table iteration, RAM billed to the wrong payer);
- [`.audit/verify-hashes.sh`](.audit/verify-hashes.sh) — verifies that the WASM from a
  reproducible build matches the code hash live on mainnet.

See [`.audit/README.md`](.audit/README.md).

Findings from our own reviews are tracked privately until they are fixed and deployed, and are
referenced in this repository by tracker ID only. Publishing an unremediated finding against the
contract securing mainnet would itself be a disclosure problem.

## Security reviews

- **2026-09 — internal review of `wax-system-contracts` at tag `wax-3.3.0` (commit `715ddba`)
  under WCAP v1.** Security review performed internally using Claude (Anthropic) — Opus 5 for
  audit phases 0–5, Fable 5.1 for remediation and reporting. The tooling from that review is
  what runs in CI today; the report is published separately once remediation is complete.
- Earlier third-party reviews of WAX components (Hacken, Sentnl) are held by the WAX team and
  available on request.

## Bug bounty

There is **no bug bounty programme at present**. We say so plainly rather than leave it
implied. Reports are still very welcome, will be acknowledged and credited as described above,
and we will revisit a paid programme as part of the security roadmap.
