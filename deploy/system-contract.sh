#!/usr/bin/env bash
# deploy/system-contract.sh - the eosio.system deploy, as data.
#
# Three steps, no keys:
#   build  <tag>   reproducible build of <tag> inside the pinned toolchain image -> deploy/out/<tag>/
#   plan   <tag>   read mainnet, then write the unsigned eosio.msig proposals and a runbook for the
#                  operators who hold the keys: raise the chain parameter the deploy needs (only if
#                  it needs it), set code + abi, restore the parameter.
#   verify <tag>   after exec: the code hash live on eosio equals the wasm built here.
#
# Why three proposals: eosio@active is admin.wax@active, a 2-of-3 of admin{1,2,3}.wax, so every
# eosio action goes through eosio.msig. On WAX the msig exec runs the proposal's actions inline,
# and an inline action is limited by the chain parameter max_inline_action_size (300,000 bytes on
# mainnet, unchanged since it was set), which eosio.system.wasm has exceeded for years. Mainnet's
# deploys (2026-01-28, 2026-02-17: raise -> setcode -> restore, one msig proposal each) are the
# procedure this script writes down from live data instead of memory.
#
# Environment (all optional):
#   WAX_API      chain API            default https://wax.greymass.com
#   PROPOSER     msig proposer        default admin2.wax  (needs free RAM >= the packed proposal)
#   EXPIRE_DAYS  proposal lifetime    default 7
#   IMAGE        toolchain image      default waxteam/waxdev:<DEV_VERSION from the TAG's Makefile>
#   WCAP_DOCKER_OPTS  build container caps, default "--cpus 6" (shared host: add --cgroup-parent=...)
#   WCAP_JOBS         make -j, default 6            (both as .audit/reproduce-deployed.sh reads them)
# Needs docker, git, curl, jq. cleos runs inside the image; nothing is installed on the host.
# The build itself is .audit/reproduce-deployed.sh - one recipe, so the hash the runbook prints is
# the hash verify-hashes.sh will check after the deploy.
set -euo pipefail

STEP="${1:-}"; TAG="${2:-}"
[[ -n "$STEP" && -n "$TAG" ]] || { sed -n '2,25p' "$0"; exit 2; }

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
API="${WAX_API:-https://wax.greymass.com}"
PROPOSER="${PROPOSER:-admin2.wax}"
EXPIRE_DAYS="${EXPIRE_DAYS:-7}"
# The image the tag itself pins, so an older tag (a rollback) is built with its own toolchain.
IMAGE="${IMAGE:-waxteam/waxdev:$(git -C "$ROOT" show "$TAG:Makefile" | sed -n 's/^DEV_VERSION=//p')}"
OUT="$ROOT/deploy/out/$TAG"
CONTRACT_ACCOUNT=eosio

chain() { curl -sf --max-time 20 -X POST "$API/v1/chain/$1" -d "$2"; }
# cleos from the pinned image, as the calling user, with deploy/out/<tag> mounted at /opt/out.
# --network host so a local API (a dry run against a test chain) works the same as mainnet.
cleos() {
  docker run --rm --cpus 1 --network host --user "$(id -u):$(id -g)" -e HOME=/tmp \
    -v "$OUT":/opt/out -w /opt/out "$IMAGE" cleos -u "$API" "$@"
}
sha() { sha256sum "$1" | cut -d' ' -f1; }

# ---------------------------------------------------------------- build
do_build() {
  local commit; commit="$(git -C "$ROOT" rev-parse --short "$TAG^{commit}")"
  rm -rf "$OUT" && mkdir -p "$OUT"
  echo "build $TAG ($commit) in $IMAGE"
  # reproduce-deployed.sh builds the ref in the image and compares every wasm with mainnet. Before
  # the deploy nothing matches (exit 1) - expected; a build failure also exits 1, so the artefact
  # decides. Exit 2 is "mainnet unreachable".
  local rc=0
  WCAP_SCRATCH="$OUT" "$ROOT/.audit/reproduce-deployed.sh" "$TAG" "$IMAGE" || rc=$?
  (( rc != 2 )) || echo "mainnet not reachable; the build may be fine but could not be compared"
  local work="$OUT/reproduce-${TAG//\//_}"
  [[ -f "$work/build/contracts/eosio.system/eosio.system.wasm" ]] || { echo "build failed (logs under $work/build)"; exit 1; }
  cp "$work/build/contracts/eosio.system/eosio.system.wasm" "$work/build/contracts/eosio.system/eosio.system.abi" "$OUT/"
  cp "$work/build/cmake.log" "$work/build/make.log" "$OUT/"
  rm -rf "$work"
  (cd "$OUT" && sha256sum eosio.system.wasm eosio.system.abi > SHA256SUMS)
  {
    echo "tag       $TAG"
    echo "commit    $commit"
    echo "image     $IMAGE"
    echo "built     $(date -u +%FT%TZ)"
    echo "wasm      $(stat -c %s "$OUT/eosio.system.wasm") bytes"
    echo "abi       $(stat -c %s "$OUT/eosio.system.abi") bytes (json)"
  } > "$OUT/BUILD.txt"
  cat "$OUT/BUILD.txt" "$OUT/SHA256SUMS"
}

# ---------------------------------------------------------------- plan
# Every leaf permission (one that carries keys) under account@permission, depth-first, as the
# msig "requested" list: this is what `eosc --with-subaccounts` did, from get_account.
resolve_leaves() {
  local acct="$1" perm="$2" depth="${3:-0}"
  (( depth < 6 )) || { echo "authority tree deeper than 5 under $acct@$perm" >&2; exit 1; }
  local auth; auth="$(chain get_account "{\"account_name\":\"$acct\"}" | jq -c --arg p "$perm" '.permissions[] | select(.perm_name==$p) | .required_auth')"
  [[ -n "$auth" ]] || { echo "no permission $acct@$perm" >&2; exit 1; }
  if [[ "$(jq '.keys|length' <<<"$auth")" -gt 0 ]]; then
    echo "{\"actor\":\"$acct\",\"permission\":\"$perm\"}"
  fi
  jq -r '.accounts[] | select(.permission.permission != "eosio.code") | "\(.permission.actor) \(.permission.permission)"' <<<"$auth" |
    while read -r a p; do resolve_leaves "$a" "$p" $((depth + 1)); done
}

roundup() { echo $(( ( $1 + $2 - 1 ) / $2 * $2 )); }

do_plan() {
  [[ -f "$OUT/eosio.system.wasm" && -f "$OUT/eosio.system.abi" ]] || { echo "no build for $TAG: run '$0 build $TAG' first"; exit 1; }
  local built onchain; built="$(sha "$OUT/eosio.system.wasm")"
  onchain="$(chain get_code_hash "{\"account_name\":\"$CONTRACT_ACCOUNT\"}" | jq -r .code_hash)"
  echo "built   eosio.system.wasm $built"
  echo "on-chain $CONTRACT_ACCOUNT           $onchain"
  if [[ "$built" == "$onchain" ]]; then echo "$TAG is already the code on $CONTRACT_ACCOUNT. Nothing to propose."; exit 0; fi

  # --- live chain parameters, shaped to the setparams type the deployed ABI declares
  local abi global fields params
  abi="$(chain get_abi "{\"account_name\":\"$CONTRACT_ACCOUNT\"}" | jq -c .abi)"
  global="$(chain get_table_rows "{\"json\":true,\"code\":\"$CONTRACT_ACCOUNT\",\"scope\":\"$CONTRACT_ACCOUNT\",\"table\":\"global\",\"limit\":1}" | jq -c '.rows[0]')"
  fields="$(jq -c '
    . as $abi
    | (.structs[] | select(.name=="setparams") | .fields[0].type) as $t0
    | ([$abi.types[]? | select(.new_type_name==$t0) | .type] | first // $t0) as $t
    | def own($n): [$abi.structs[] | select(.name==$n) | (.fields[].name)];
      def base($n): ($abi.structs[] | select(.name==$n) | .base);
      [ (if base($t) != "" then own(base($t)) else [] end), own($t) ] | add' <<<"$abi")"
  params="$(jq -c --argjson f "$fields" 'with_entries(select(.key as $k | $f | index($k)))' <<<"$global")"
  local cur_inline cur_net cur_blocknet
  cur_inline="$(jq .max_inline_action_size <<<"$params")"
  cur_net="$(jq .max_transaction_net_usage <<<"$params")"
  cur_blocknet="$(jq .max_block_net_usage <<<"$params")"

  # --- the set-contract transaction, unsigned, and its packed size
  local exp=$(( EXPIRE_DAYS * 86400 ))
  # One cleos run makes the transaction (expiration and TaPoS come from the chain at that moment);
  # the packed form is derived from that same JSON, so the hex is byte for byte what the msig will
  # store and `cleos multisig review` will show. The msig stores it uncompressed.
  cleos set contract "$CONTRACT_ACCOUNT" /opt/out eosio.system.wasm eosio.system.abi --compression none \
    -p "$CONTRACT_ACCOUNT@active" -s -d -j -x "$exp" --suppress-duplicate-check > "$OUT/02-set-contract.trx.json"
  # (the file name, not its contents: the JSON is far larger than one argument may be)
  cleos convert pack_transaction 02-set-contract.trx.json | jq -r .packed_trx > "$OUT/02-set-contract.packed_trx.hex"
  local code_bytes abi_bytes packed_bytes largest
  # action data may be hex (`data`) or, if this cleos decoded it, an object with the hex in `hex_data`
  code_bytes="$(jq '[.actions[] | select(.name=="setcode") | (.hex_data // .data) | length / 2] | add' "$OUT/02-set-contract.trx.json")"
  abi_bytes="$(jq '[.actions[] | select(.name=="setabi") | (.hex_data // .data) | length / 2] | add' "$OUT/02-set-contract.trx.json")"
  packed_bytes=$(( ( $(wc -c < "$OUT/02-set-contract.packed_trx.hex") - 1 ) / 2 ))
  largest=$(( code_bytes > abi_bytes ? code_bytes : abi_bytes ))

  # --- which parameters the deploy needs raised, and to what
  # inline: the msig exec runs setcode/setabi as inline actions; the chain requires
  # data.size() < max_inline_action_size (strict).
  # net: the propose transaction carries the packed transaction; bounded by max_transaction_net_usage.
  local raise="$params" need=() net_raised=0
  if (( largest >= cur_inline )); then
    local new_inline; new_inline="$(roundup $(( largest * 2 )) 100000)"
    raise="$(jq -c --argjson v "$new_inline" '.max_inline_action_size=$v' <<<"$raise")"
    need+=("max_inline_action_size $cur_inline -> $new_inline (largest action $largest bytes)")
  fi
  local propose_bytes=$(( packed_bytes + 512 ))
  if (( propose_bytes > cur_net )); then
    local new_net; new_net="$(roundup $(( propose_bytes * 2 )) 65536)"
    raise="$(jq -c --argjson v "$new_net" '.max_transaction_net_usage=$v' <<<"$raise")"
    need+=("max_transaction_net_usage $cur_net -> $new_net (propose transaction ~$propose_bytes bytes)")
    net_raised=1
    (( new_net <= cur_blocknet )) || { echo "max_transaction_net_usage $new_net would exceed max_block_net_usage $cur_blocknet - raise that too, by hand"; exit 1; }
  fi
  if (( ${#need[@]} )); then
    jq . <<<"$raise"  > "$OUT/01-raise-params.params.json"      # the exact payloads, for approvers to read
    jq . <<<"$params" > "$OUT/03-restore-params.params.json"
    cleos push action "$CONTRACT_ACCOUNT" setparams "{\"params\":$raise}"  -p "$CONTRACT_ACCOUNT@active" -s -d -j -x "$exp" > "$OUT/01-raise-params.trx.json"
    cleos push action "$CONTRACT_ACCOUNT" setparams "{\"params\":$params}" -p "$CONTRACT_ACCOUNT@active" -s -d -j -x "$exp" > "$OUT/03-restore-params.trx.json"
  else
    rm -f "$OUT"/0[13]-*-params.*
  fi

  # --- who must approve, and whether the proposer can carry the proposal
  resolve_leaves "$CONTRACT_ACCOUNT" active | jq -s -c 'unique' > "$OUT/requested.json"
  local prop_acct prop_ram prop_net prop_open
  prop_acct="$(chain get_account "{\"account_name\":\"$PROPOSER\"}")"
  prop_ram="$(jq '.ram_quota - .ram_usage' <<<"$prop_acct")"
  prop_net="$(jq '.net_limit.available' <<<"$prop_acct")"
  prop_open="$(chain get_table_rows "{\"json\":true,\"code\":\"eosio.msig\",\"scope\":\"$PROPOSER\",\"table\":\"proposal\",\"limit\":50}" | jq -r '[.rows[].proposal_name] | join(" ")')"
  local six; six="$(printf '%s' "$built" | cut -c1-6 | tr '0-9a-f' 'abcdefghijklmnop')"
  local n_inc="inc$six" n_set="set$six" n_res="res$six" clash=""
  for n in $n_inc $n_set $n_res; do [[ " $prop_open " == *" $n "* ]] && clash="$clash $n"; done

  # --- the runbook
  {
    echo "# Deploy $TAG to $CONTRACT_ACCOUNT - runbook"
    echo
    echo "Generated $(date -u +%FT%TZ) by deploy/system-contract.sh from live mainnet state. Nothing here is signed."
    echo
    echo "| | |"; echo "|---|---|"
    echo "| tag / commit | \`$TAG\` / \`$(git -C "$ROOT" rev-parse --short "$TAG^{commit}")\` |"
    echo "| eosio.system.wasm | \`$built\` ($(stat -c %s "$OUT/eosio.system.wasm") bytes) |"
    echo "| eosio.system.abi | \`$(sha "$OUT/eosio.system.abi")\` |"
    echo "| on-chain code now | \`$onchain\` |"
    echo "| setcode / setabi action data | $code_bytes / $abi_bytes bytes; packed transaction $packed_bytes bytes |"
    echo "| max_inline_action_size now | $cur_inline |"
    echo "| max_transaction_net_usage now | $cur_net |"
    echo "| proposer | \`$PROPOSER\`; free RAM $prop_ram bytes (the proposal bills ~$propose_bytes until exec); NET available $prop_net bytes (the propose transaction uses ~$propose_bytes); open proposals: ${prop_open:-none} |"
    echo "| approvals needed | $(jq -r '[.[] | "\(.actor)@\(.permission)"] | join(", ")' "$OUT/requested.json") (threshold from eosio@active's tree; see \`cleos get account $CONTRACT_ACCOUNT\`) |"
    echo "| proposal expiry | $EXPIRE_DAYS days from generation |"
    echo
    if (( ${#need[@]} )); then
      echo "## Parameters to raise first, then restore"; echo
      for n in "${need[@]}"; do echo "- $n"; done
      echo; echo "Proposal \`$n_inc\` raises, \`$n_res\` restores the values read today. Both are full \`setparams\` payloads (every field, values as read)."
    else
      echo "## No parameter change needed"; echo; echo "Every action fits the live limits; only proposal \`$n_set\` is used."
    fi
    echo
    echo "## Before proposing"; echo
    (( prop_ram >= propose_bytes )) || echo "- **$PROPOSER has $prop_ram bytes of free RAM; the proposal needs ~$propose_bytes.** Buy RAM for the proposer or choose another (\`PROPOSER=...\`)."
    (( prop_net >= propose_bytes )) || echo "- **$PROPOSER has $prop_net bytes of NET available; the propose transaction needs ~$propose_bytes.** Stake or power up NET for the proposer first (ONLY_BILL_FIRST_AUTHORIZER: the proposer pays), or choose another (\`PROPOSER=...\`)."
    [[ -z "$clash" ]] || echo "- **Proposal name(s)$clash already open under $PROPOSER** (an earlier run for this same build). Cancel or exec those first: \`cleos -u $API multisig cancel $PROPOSER <name> $PROPOSER -p $PROPOSER@active\`."
    echo "- Cancel any older proposal for $CONTRACT_ACCOUNT that names an action this ABI removes (an unimplemented action name executes as a silent no-op after setcode)."
    echo "- Approvers: before approving \`$n_set\`, compare the packed transaction on chain with this file, byte for byte:"
    echo "  \`cleos -u $API multisig review $PROPOSER $n_set | jq -r .packed_transaction | sha256sum\` must print \`$(sha "$OUT/02-set-contract.packed_trx.hex")\` (= \`sha256sum $OUT/02-set-contract.packed_trx.hex\`)."
    (( ${#need[@]} )) && echo "- Approvers: \`01-raise-params.params.json\` and \`03-restore-params.params.json\` are the full setparams payloads; only the raised field(s) differ from today's \`global\` row."
    echo
    local req="$OUT/requested.json"
    if (( net_raised )); then
      echo "## Order matters here"; echo
      echo "The propose transaction for \`$n_set\` is itself larger than today's \`max_transaction_net_usage\`, so \`$n_inc\` must be proposed, approved **and executed** before \`$n_set\` can be proposed. Run steps 1-3 for \`$n_inc\` first, then for \`$n_set\` and \`$n_res\`."; echo
    fi
    echo "## 1. Propose (proposer's key)"; echo; echo '```'
    (( ${#need[@]} )) && echo "cleos -u $API multisig propose_trx $n_inc $req $OUT/01-raise-params.trx.json $PROPOSER -p $PROPOSER@active"
    (( net_raised )) && echo "# ... approve and exec $n_inc (steps 2-3) before continuing ..."
    echo "cleos -u $API multisig propose_trx $n_set $req $OUT/02-set-contract.trx.json $PROPOSER -p $PROPOSER@active"
    (( ${#need[@]} )) && echo "cleos -u $API multisig propose_trx $n_res $req $OUT/03-restore-params.trx.json $PROPOSER -p $PROPOSER@active"
    echo '```'; echo
    echo "## 2. Review and approve (each approver, any that reach the threshold)"; echo; echo '```'
    for n in $( (( ${#need[@]} )) && echo "$n_inc $n_set $n_res" || echo "$n_set"); do
      echo "cleos -u $API multisig review $PROPOSER $n"
      jq -r --arg p "$PROPOSER" --arg n "$n" --arg api "$API" '.[] | "cleos -u \($api) multisig approve \($p) \($n) '"'"'{\"actor\":\"\(.actor)\",\"permission\":\"\(.permission)\"}'"'"' -p \(.actor)@\(.permission)"' "$req"
    done
    echo '```'; echo
    echo "## 3. Execute, in this order, each after the previous is irreversible (~3 minutes)"; echo; echo '```'
    (( ${#need[@]} )) && echo "cleos -u $API multisig exec $PROPOSER $n_inc -p $PROPOSER@active"
    echo "cleos -u $API multisig exec $PROPOSER $n_set -p $PROPOSER@active"
    (( ${#need[@]} )) && echo "cleos -u $API multisig exec $PROPOSER $n_res -p $PROPOSER@active"
    echo '```'; echo
    echo "## 4. Verify"; echo; echo '```'
    echo "$ROOT/deploy/system-contract.sh verify $TAG"
    echo '```'; echo
    echo "## Undo"; echo
    echo "Before exec: \`cleos -u $API multisig cancel $PROPOSER <name> $PROPOSER -p $PROPOSER@active\`. After exec of \`$n_set\`: propose the previous tag the same way; there is no other rollback."
  } > "$OUT/RUNBOOK.md"

  echo
  echo "wrote $OUT/"; ls -1 "$OUT" | sed 's/^/  /'
  echo
  if (( ${#need[@]} )); then printf 'raise: %s\n' "${need[@]}"; else echo "no parameter change needed"; fi
  (( prop_ram >= propose_bytes )) || echo "WARN: $PROPOSER free RAM $prop_ram < $propose_bytes needed for the proposal"
  (( prop_net >= propose_bytes )) || echo "WARN: $PROPOSER NET available $prop_net < $propose_bytes needed to propose"
  [[ -z "$clash" ]] || echo "WARN: proposal name(s)$clash already open under $PROPOSER - cancel or exec them first"
  echo "proposals: $( (( ${#need[@]} )) && echo "$n_inc -> $n_set -> $n_res" || echo "$n_set" )"
  echo "runbook:   $OUT/RUNBOOK.md"
}

# ---------------------------------------------------------------- verify
do_verify() {
  [[ -f "$OUT/eosio.system.wasm" ]] || { echo "no build for $TAG"; exit 1; }
  local built onchain; built="$(sha "$OUT/eosio.system.wasm")"
  onchain="$(chain get_code_hash "{\"account_name\":\"$CONTRACT_ACCOUNT\"}" | jq -r .code_hash)"
  echo "built    $built"
  echo "on-chain $onchain  (last_code_update $(chain get_account "{\"account_name\":\"$CONTRACT_ACCOUNT\"}" | jq -r .last_code_update))"
  local ours theirs
  ours="$(jq -r '[.actions[].name] | sort | join(" ")' "$OUT/eosio.system.abi")"
  theirs="$(chain get_abi "{\"account_name\":\"$CONTRACT_ACCOUNT\"}" | jq -r '[.abi.actions[].name] | sort | join(" ")')"
  [[ "$ours" == "$theirs" ]] && echo "abi      actions match ($(wc -w <<<"$ours"))" || { echo "abi      ACTION LIST DIFFERS"; diff <(tr ' ' '\n' <<<"$ours") <(tr ' ' '\n' <<<"$theirs") || true; }
  if [[ "$built" == "$onchain" ]]; then echo "MATCH: $TAG is the code on $CONTRACT_ACCOUNT."; else echo "NO MATCH"; exit 1; fi
}

case "$STEP" in
  build)  do_build ;;
  plan)   do_plan ;;
  verify) do_verify ;;
  *) sed -n '2,25p' "$0"; exit 2 ;;
esac
