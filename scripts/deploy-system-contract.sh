!/usr/bin/env bash
set -euo pipefail

# ----------------------------
# Config (override via env)
# ----------------------------
# pick ONE of these and retry
# export API_URL=https://wax.api.eosnation.io
export API_URL=https://api.waxsweden.org
# export API_URL=https://api.wax.alohaeos.com
# export API_URL=https://wax.cryptolions.io
# export API_URL=https://api-wax-mainnet.wecan.dev
API_URL="${API_URL:-https://wax.greymass.com}"

# The account that submits the propose (pays proposal RAM)
PROPOSER="${PROPOSER:?Set PROPOSER to the proposing account (pays proposal RAM)}"

# The account that pays CPU/NET for exec
EXECUTER="${EXECUTER:-$PROPOSER}"

# Requested approvals list (permission levels). See below for auto-gen option.
APPROVERS_FILE="${APPROVERS_FILE:-./approvers.json}"

# Directory containing eosio.system.wasm + eosio.system.abi
CONTRACT_DIR="${CONTRACT_DIR:-.}"
# CONTRACT_DIR="${CONTRACT_DIR:-build/contracts/eosio.system}"

WASM_FILE="${WASM_FILE:-eosio.system.wasm}"
ABI_FILE="${ABI_FILE:-eosio.system.abi}"

# Transaction expiration in seconds.
# NOTE: Even if you set this huge, TAPOS still goes stale; plan to exec within hours.
TRX_EXP_SECONDS="${TRX_EXP_SECONDS:-604800}"

# If set to 1, attempts to generate approvers.json from eosio's authority structure
# (works for common patterns like active -> eosio.prods).
GEN_APPROVERS="${GEN_APPROVERS:-0}"

# ----------------------------
# Helpers
# ----------------------------
need() { command -v "$1" >/dev/null 2>&1 || { echo "Missing dependency: $1" >&2; exit 1; }; }

extract_json() {
  awk 'BEGIN{p=0} /^\s*\{/{p=1} p{print}'
}

# EOSIO name-friendly base32 (alphabet: a-z + 1-5)
b32_name() {
  local n="$1"
  local alphabet="abcdefghijklmnopqrstuvwxyz12345"
  local out=""
  if [[ "$n" -eq 0 ]]; then echo "a"; return; fi
  while [[ "$n" -gt 0 ]]; do
    local idx=$(( n % 32 ))
    out="${alphabet:idx:1}${out}"
    n=$(( n / 32 ))
  done
  echo "$out"
}

make_proposal_name() {
  local tag="${1:-sys}"  # <= 3 chars recommended
  local ts enc name
  ts="$(date +%s)"
  enc="$(b32_name "$ts")"
  name="${tag}${enc}"
  echo "${name:0:12}"
}

generate_approvers_from_eosio() {
  # Tries to produce a JSON array like:
  # [{"actor":"bp1","permission":"active"}, ...]
  #
  # Handles two common patterns:
  #  A) eosio@active directly lists producer permissions
  #  B) eosio@active delegates to eosio@eosio.prods, and eosio@eosio.prods lists producers
  #
  # If your chain uses something else, keep your manual approvers.json.
  echo "Generating $APPROVERS_FILE from on-chain eosio permissions..."
  cleos -u "$API_URL" get account eosio -j > /tmp/eosio_acct.json

  # Find active required_auth.accounts list
  local active_accounts
  active_accounts="$(jq -c '
    .permissions[]
    | select(.perm_name=="active")
    | .required_auth.accounts
  ' /tmp/eosio_acct.json)"

  # If active delegates to eosio.prods, resolve eosio.prods
  if echo "$active_accounts" | jq -e '.[] | select(.permission.actor=="eosio" and .permission.permission=="eosio.prods")' >/dev/null 2>&1; then
    jq -c '
      .permissions[]
      | select(.perm_name=="eosio.prods")
      | .required_auth.accounts
      | map({actor:.permission.actor, permission:.permission.permission})
    ' /tmp/eosio_acct.json > "$APPROVERS_FILE"
  else
    # Otherwise use active's accounts directly
    echo "$active_accounts" | jq -c 'map({actor:.permission.actor, permission:.permission.permission})' > "$APPROVERS_FILE"
  fi

  echo "Wrote $(jq length "$APPROVERS_FILE") approvers to $APPROVERS_FILE"
}

generate_trx() {
  local acct="$1"
  local out="$2"

  [[ -f "$CONTRACT_DIR/$WASM_FILE" ]] || { echo "Missing wasm: $CONTRACT_DIR/$WASM_FILE" >&2; exit 1; }
  [[ -f "$CONTRACT_DIR/$ABI_FILE"  ]] || { echo "Missing abi : $CONTRACT_DIR/$ABI_FILE" >&2; exit 1; }

  echo "Generating unsigned trx for set contract on '$acct' ..."
  # Explicitly pass WASM + ABI so you are guaranteed which files get used.
  cleos -u "$API_URL" set contract --compression zlib -s -j -d -x "$TRX_EXP_SECONDS" \
    "$acct" "$CONTRACT_DIR" "$WASM_FILE" "$ABI_FILE" \
    | extract_json \
    | jq 'del(.signatures?, .context_free_data?)' \
    > "$out"
}

propose_trx() {
  local proposal="$1"
  local inner_trx_file="$2"

  local payload_file="proposal-${proposal}.json"
  local outer_trx_file="push-proposal-${proposal}.json"

  # Build the msig propose payload (this stays the same)
  jq -n \
    --arg proposer "$PROPOSER" \
    --arg proposal_name "$proposal" \
    --slurpfile requested "$APPROVERS_FILE" \
    --slurpfile trx "$inner_trx_file" \
    '{
      proposer: $proposer,
      proposal_name: $proposal_name,
      requested: $requested[0],
      trx: $trx[0]
    }' > "$payload_file"

  # Build a FULL transaction (outer) that calls eosio.msig::propose
  # so we can use "cleos push transaction <file>"
  local head_num ref_prefix ref_num expiration

  head_num="$(cleos -u "$API_URL" get info | jq -r '.head_block_num')"
  ref_prefix="$(cleos -u "$API_URL" get block "$head_num" | jq '.ref_block_prefix')"
  ref_num=$(( head_num & 65535 ))
  expiration="$(date -u -d "+${TRX_EXP_SECONDS} seconds" +"%Y-%m-%dT%H:%M:%S")"

  jq -n \
    --arg exp "$expiration" \
    --argjson rbn "$ref_num" \
    --argjson rbp "$ref_prefix" \
    --slurpfile payload "$payload_file" \
    --arg actor "$PROPOSER" \
    '{
      expiration: $exp,
      ref_block_num: $rbn,
      ref_block_prefix: $rbp,
      max_net_usage_words: 0,
      max_cpu_usage_ms: 0,
      delay_sec: 0,
      context_free_actions: [],
      actions: [{
        account: "eosio.msig",
        name: "propose",
        authorization: [{actor: $actor, permission: "active"}],
        data: $payload[0]
      }],
      transaction_extensions: []
    }' > "$outer_trx_file"

  echo "Proposing '$proposal' ..."
  cleos -u "$API_URL" push transaction "$outer_trx_file" -p "$PROPOSER@active"
}

print_approve_and_exec_cmds() {
  local proposal="$1"

  echo
  echo "Approve commands (each approver runs their own):"
  jq -r --arg api "$API_URL" --arg proposer "$PROPOSER" --arg proposal "$proposal" '
    .[] | "cleos -u \($api) push action eosio.msig approve '\''{\"proposer\":\"\($proposer)\",\"proposal_name\":\"\($proposal)\",\"level\":{\"actor\":\"\(.actor)\",\"permission\":\"\(.permission)\"}}'\'' -p \(.actor)@\(.permission)"
  ' "$APPROVERS_FILE"

  echo
  echo "Exec command (once approvals satisfy eosio@active authority):"
  echo "cleos -u $API_URL push action eosio.msig exec '{\"proposer\":\"$PROPOSER\",\"proposal_name\":\"$proposal\",\"executer\":\"$EXECUTER\"}' -p $EXECUTER@active"
  echo
  echo "Review (optional):"
  echo "cleos -u $API_URL multisig review $PROPOSER $proposal"
  echo
}

main() {
  need cleos
  need jq

  if [[ "$GEN_APPROVERS" == "1" ]]; then
    generate_approvers_from_eosio
  fi

  [[ -f "$APPROVERS_FILE" ]] || {
    echo "Missing $APPROVERS_FILE"
    echo "Create it as a JSON array of permission levels, e.g.:"
    echo '[{"actor":"bp1","permission":"active"},{"actor":"bp2","permission":"active"}]'
    exit 1
  }

  local proposal="${PROPOSAL_NAME:-$(make_proposal_name "sys")}"
  local trx="trx-eosio-system.json"

  generate_trx "eosio" "$trx"
  propose_trx "$proposal" "$trx"
  print_approve_and_exec_cmds "$proposal"

  echo "Done. Proposal name: $proposal"
}

main "$@"
