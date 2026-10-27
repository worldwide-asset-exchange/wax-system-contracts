---
content_title: Runbook — upgrading an allow-listed contract (guilds.oig, orng.wax)
link_text: Runbook — upgrading an allow-listed contract (guilds.oig, orng.wax)
---

# Runbook: upgrading `guilds.oig` or `orng.wax` without switching their feature off

`eosio.system` trusts two external contracts only while their **deployed code hash** is on an
allow-list it keeps:

| Contract | Allow-list | Setters (`eosio` msig) | What stops, silently, when the hash is not listed | Live (2026-10-26) |
|---|---|---|---|---|
| `guilds.oig` | `global.a.guilds_code_hashes` | `addguildhash`, `rmguildhash` | Weighted voting: every multiplier becomes 1.0 and the schedule reverts to plain vote order at the next election (~1 min). | listed `cca52864…` **== live** |
| `orng.wax` | `global.b.orng_code_hashes` | `addornghash`, `rmornghash`, `setornghash` (replaces the list) | The RNG treasury carve-out: `rng_deposit` is 0 and the money flows to producers/voters/savings instead. | listed `4e516c80…` **≠ live `c913ee95…`** (since 2026-06-03) |

Neither failure raises an error, logs anything, or blocks the deploy. The check runs at every
election (guilds) or every claim (ORNG), so the switch-off is immediate and total. The order
below is therefore mandatory for **any** `setcode` on either account, including a redeploy of the
same source with a different toolchain, and including "small" changes (the 2026-06-03 `orng.wax`
deploy removed one action and turned RNG funding off for five months).

Read the [overview](09_runbook-tokenomics-parameters.md) for the msig mechanics (`$REQ`, `$AUTH`,
`$PROPOSER`).

## Who can deploy

| Account | `setcode` authority | Held by |
|---|---|---|
| `guilds.oig` | its `active`/`owner` | OIG / Kaefer (see `deploy/out/guilds-*/RUNBOOK.md` from the 2026-09 deploys for the last run) |
| `orng.wax` | `orng.wax@deploy` → `deploy.wax@all` (`wax-orng` `Makefile` target `deploy-mainnet`) | ORNG team |
| `eosio` (the allow-list) | `admin.wax@active`, 2-of-3 `admin1/2/3.wax` | WAX |

Two different parties have to act in a fixed order, so this runbook is a coordination checklist
as much as a command list.

## The order

```
 WAX                                   contract owner
 ─────────────────────────────         ────────────────────────────────────
 0. read live hash + allow-list
                                       1. reproducible build of the tag → sha256 of the wasm
 2. add<…>hash(new)        ─────────►  (both hashes now allowed)
                                       3. setcode / setabi
 4. verify live hash == new, feature still on
 5. rm<…>hash(old)
```

### 0. Read the state

```bash
API=https://wax.greymass.com
cleos -u $API get code guilds.oig            # live code hash
cleos -u $API get table eosio eosio global.a | jq '.rows[0].guilds_code_hashes'
cleos -u $API get code orng.wax
cleos -u $API get table eosio eosio global.b | jq '.rows[0].orng_code_hashes'
```

If the live hash is already **not** in the list, the feature is already off; fix that first
(step 2 with the live hash) before planning the upgrade, so the upgrade is measured against a
working baseline.

### 1. Reproducible build

The hash you allow-list must be the hash the owner will deploy, so build the exact tag in the
toolchain the repository pins and record the wasm hash. Use the resource caps on a shared host.

```bash
# wax-orng: Makefile pins waxteam/waxdev:ce-v1.0.3wax01-v4.1.1wax01 and cdt-cpp -abigen -contract=orng -O3
git clone --depth 1 --branch <tag> https://github.com/worldwide-asset-exchange/wax-orng.git orng-<tag>
docker run --rm --memory 8g --cpus 4 -v $PWD/orng-<tag>:/src -w /src waxteam/waxdev:ce-v1.0.3wax01-v4.1.1wax01 \
  bash -c 'mkdir -p build && cdt-cpp -abigen -contract=orng -O3 -I./include -I/usr/local/eosio.cdt/include src/orng.cpp -o build/orng.wasm && sha256sum build/orng.wasm'

# guilds.oig: CMake with CDT 4.1.1wax01 (same image); the 2026-09 deploy used the recipe in deploy/out/guilds-final-deploy/
```

Checked 2026-10-26: `wax-orng` v3.2.1 reproduces to `c913ee95…` (the live code); v3.2.0 to
`daf16db3…`, v3.1.0 to `4939107c…`, v3.0.3 `f70f9912…`, v3.0.2 `032acae7…`, v3.0.1 `300a7c6c…`,
v3.0.0 `ef6f7d61…`. A hash that does not reproduce from a tag must not be allow-listed; tag first.

### 2. Add the new hash (WAX, msig)

```bash
cleos -u $API multisig propose gld.add "$REQ" "$AUTH" eosio addguildhash '{"hash":"<new hash>"}' $PROPOSER 168 -p $PROPOSER@active
cleos -u $API multisig propose rng.add "$REQ" "$AUTH" eosio addornghash  '{"hash":"<new hash>"}' $PROPOSER 168 -p $PROPOSER@active
```

Approve 2 of 3, exec, and confirm the list now holds **both** the old and the new hash. Refusals:
a duplicate hash is rejected. Do not use `setornghash` here; it replaces the list and would drop the
hash the chain is still running.

### 3. Deploy (contract owner)

Only after step 2 has executed. The owner deploys the artefact whose hash was listed, from the
same build, and reports the transaction id.

### 4. Verify (WAX)

```bash
cleos -u $API get code <account>                            # must equal the listed hash
# guilds: the next election keeps weighted order — compare `cleos get schedule` with the top-N by guild score
# orng:   the next claim trace carries a transfer eosio -> orng.wax with memo "treasury" (if the pool has headroom)
```

### 5. Remove the old hash

```bash
cleos -u $API multisig propose gld.rm "$REQ" "$AUTH" eosio rmguildhash '{"hash":"<old hash>"}' $PROPOSER 168 -p $PROPOSER@active
cleos -u $API multisig propose rng.rm "$REQ" "$AUTH" eosio rmornghash  '{"hash":"<old hash>"}' $PROPOSER 168 -p $PROPOSER@active
```

Leaving the old hash listed is a standing authorisation to roll back to that build without a
WAX approval; remove it unless a rollback window is explicitly wanted.

## Rollback

Redeploy the previous build **while its hash is still listed** (which is why step 5 waits for
step 4). If the old hash was already removed, it is step 2 again with the old hash, then the
redeploy.

## Record

Each run gets a `deploy/out/<name>/RUNBOOK.md` (gitignored, like the 2026-09 guilds runs) holding
the tag, the reproduced hash, the proposal names and transaction ids, and link the run from the
Jira ticket that motivated the contract change. The 2026-06-03 `orng.wax` deploy has no such
record; WBP-1955 was never updated, which is how the omission went unnoticed.
