# `deploy/` — deploying `eosio.system` to mainnet

One script, three steps, no keys. It replaces `deploy-system-contract.bash` (eosc, an interactive
key vault, an uncapped `make dev-docker-all`), which did not know about the chain parameter every
deploy of this contract has needed.

```
deploy/system-contract.sh build  <tag>    # reproducible build of the tag -> deploy/out/<tag>/
deploy/system-contract.sh plan   <tag>    # read mainnet -> unsigned msig proposals + RUNBOOK.md
deploy/system-contract.sh verify <tag>    # after exec: live code hash == built hash
```

`deploy/out/` is gitignored. The people who hold the keys run the commands the runbook prints;
the script never signs, never broadcasts, and never sees a key.

## How the deploy actually works on WAX

- `eosio@active` is `admin.wax@active`, a 2-of-3 of `admin1.wax`, `admin2.wax`, `admin3.wax`.
  Every `eosio` action therefore goes through `eosio.msig`: propose, approve, exec. The script
  derives the `requested` list from that authority tree at plan time, so it follows any re-keying.
- `eosio.msig`'s `exec` runs the proposal's actions as **inline actions**, and an inline action is
  bounded by the chain parameter `max_inline_action_size` (300,000 bytes on mainnet). The
  `eosio.system` wasm has been larger than that for years, so every deploy raises the parameter,
  deploys, and restores it: three proposals, executed in order. That is what mainnet did on
  2026-01-28 and 2026-02-17 (proposals by `4tioi.waa`, executed by `admin2.wax`), and `plan` writes
  the same sequence from live data, raising only the parameter(s) the measured sizes need and
  restoring exactly the values it read.
- The propose transaction carries the packed transaction (wasm + abi), so it is bounded by
  `max_transaction_net_usage` (512 KiB) and bills that much RAM to the proposer until exec. `plan`
  checks both and prints the proposer's free RAM.

## The build is the same build the audit verifies

`build` exports the tag with `git archive` and compiles it inside the image pinned in the
`Makefile`, exactly as [`.audit/reproduce-deployed.sh`](../.audit/reproduce-deployed.sh) does, so
the hash the runbook shows is the hash `.audit/verify-hashes.sh` will match after the deploy. On a
shared host, cap the container: `DOCKER_OPTS="--cgroup-parent=wax-build.slice --cpus 6"`.

## What the runbook contains

Sizes measured from the packed actions, the parameter(s) to raise and the exact values, the
proposer's RAM headroom and open proposals, the approvals needed, and the `cleos` lines for
propose / review / approve / exec / verify, plus the undo. Approvers compare the packed
transaction on chain (`cleos multisig review`) with `02-set-contract.packed_trx.hex` before
approving; its sha256 is in the runbook.

Before proposing, cancel any older proposal for `eosio` that names an action the new ABI removes:
after `setcode`, an unimplemented action name executes as a silent no-op.

## Environment

| variable | default | |
|---|---|---|
| `WAX_API` | `https://wax.greymass.com` | chain API used for reads and by `cleos` |
| `PROPOSER` | `admin2.wax` | msig proposer; needs free RAM for the packed proposal |
| `EXPIRE_DAYS` | `7` | proposal transaction lifetime |
| `IMAGE` | `waxteam/waxdev:<Makefile DEV_VERSION>` | toolchain image; also supplies `cleos` |
| `DOCKER_OPTS` | `--cpus 6` | build container caps |
| `JOBS` | `6` | `make -j` |
