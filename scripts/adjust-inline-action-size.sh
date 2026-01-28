#!/usr/bin/env bash
set -euo pipefail

# ----------------------------
# Adjust max_inline_action_size for System Contract Deployment
# ----------------------------
# This script creates msig proposals to:
#   1. INCREASE max_inline_action_size (to allow large contract deployment)
#   2. RESTORE max_inline_action_size (after deployment)
#
# Usage:
#   # Step 1: Increase the limit
#   ./adjust-inline-action-size.sh increase
#
#   # Step 2: After deployment, restore the limit
#   ./adjust-inline-action-size.sh restore
#
#   # Or just view current parameters
#   ./adjust-inline-action-size.sh status
# ----------------------------

# Config
API_URL="${API_URL:-https://api.waxsweden.org}"
PROPOSER="${PROPOSER:?Set PROPOSER to the proposing account}"
EXECUTER="${EXECUTER:-$PROPOSER}"
APPROVERS_FILE="${APPROVERS_FILE:-./approvers.json}"
TRX_EXP_SECONDS="${TRX_EXP_SECONDS:-604800}"

# Target size for deployment (512KB should be plenty)
NEW_MAX_INLINE_ACTION_SIZE="${NEW_MAX_INLINE_ACTION_SIZE:-800000}"

# Original value to restore (will be fetched from chain if not set)
ORIGINAL_MAX_INLINE_ACTION_SIZE="${ORIGINAL_MAX_INLINE_ACTION_SIZE:-}"

# ----------------------------
# Helpers
# ----------------------------
need() { command -v "$1" >/dev/null 2>&1 || { echo "Missing dependency: $1" >&2; exit 1; }; }

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

get_current_params() {
  # Fetch current blockchain parameters from the global table
  cleos -u "$API_URL" get table eosio eosio global -l 1 | jq -r '.rows[0]'
}

get_current_inline_size() {
  get_current_params | jq -r '.max_inline_action_size'
}

create_setparams_trx() {
  local new_inline_size="$1"
  local output_file="$2"

  echo "Fetching current blockchain parameters..."
  local current_params
  current_params="$(get_current_params)"

  echo "Current max_inline_action_size: $(echo "$current_params" | jq -r '.max_inline_action_size')"
  echo "New max_inline_action_size: $new_inline_size"

  # Build the setparams action data with only blockchain_parameters fields
  # Note: blockchain_parameters_v1 extends blockchain_parameters and adds max_action_return_value_size
  local params_json
  params_json=$(echo "$current_params" | jq --argjson new_size "$new_inline_size" '{
    max_block_net_usage: .max_block_net_usage,
    target_block_net_usage_pct: .target_block_net_usage_pct,
    max_transaction_net_usage: .max_transaction_net_usage,
    base_per_transaction_net_usage: .base_per_transaction_net_usage,
    net_usage_leeway: .net_usage_leeway,
    context_free_discount_net_usage_num: .context_free_discount_net_usage_num,
    context_free_discount_net_usage_den: .context_free_discount_net_usage_den,
    max_block_cpu_usage: .max_block_cpu_usage,
    target_block_cpu_usage_pct: .target_block_cpu_usage_pct,
    max_transaction_cpu_usage: .max_transaction_cpu_usage,
    min_transaction_cpu_usage: .min_transaction_cpu_usage,
    max_transaction_lifetime: .max_transaction_lifetime,
    deferred_trx_expiration_window: .deferred_trx_expiration_window,
    max_transaction_delay: .max_transaction_delay,
    max_inline_action_size: $new_size,
    max_inline_action_depth: .max_inline_action_depth,
    max_authority_depth: .max_authority_depth
  }')

  echo "Generating setparams transaction..."

  # Use cleos push action with -s -j -d to generate properly serialized transaction
  # -s = don't sign, -j = output JSON, -d = don't broadcast, -x = expiration
  local action_data="{\"params\":$params_json}"

  cleos -u "$API_URL" push action eosio setparams "$action_data" \
    -p eosio@active -s -j -d -x "$TRX_EXP_SECONDS" \
    | jq 'del(.signatures?, .context_free_data?)' \
    > "$output_file"

  echo "Transaction saved to: $output_file"
}

propose_trx() {
  local proposal="$1"
  local inner_trx_file="$2"
  local description="$3"

  echo "Proposing '$proposal' ($description) ..."

  # Use cleos multisig propose_trx which handles serialization properly
  cleos -u "$API_URL" multisig propose_trx "$proposal" "$APPROVERS_FILE" \
    "$inner_trx_file" "$PROPOSER" -p "$PROPOSER@active"
}

print_approve_commands() {
  local proposal="$1"

  echo
  echo "Approve commands:"
  jq -r --arg api "$API_URL" --arg proposer "$PROPOSER" --arg proposal "$proposal" '
    .[] | "cleos -u \($api) push action eosio.msig approve '\''{\"proposer\":\"\($proposer)\",\"proposal_name\":\"\($proposal)\",\"level\":{\"actor\":\"\(.actor)\",\"permission\":\"\(.permission)\"}}'\'' -p \(.actor)@\(.permission)"
  ' "$APPROVERS_FILE"

  echo
  echo "Exec command:"
  echo "cleos -u $API_URL push action eosio.msig exec '{\"proposer\":\"$PROPOSER\",\"proposal_name\":\"$proposal\",\"executer\":\"$EXECUTER\"}' -p $EXECUTER@active"
  echo
  echo "Review:"
  echo "cleos -u $API_URL multisig review $PROPOSER $proposal"
}

cmd_status() {
  echo "=== Current Blockchain Parameters ==="
  echo
  local params
  params="$(get_current_params)"
  echo "max_inline_action_size: $(echo "$params" | jq -r '.max_inline_action_size') bytes"
  echo "max_inline_action_depth: $(echo "$params" | jq -r '.max_inline_action_depth')"
  echo "max_authority_depth: $(echo "$params" | jq -r '.max_authority_depth')"
  echo
  echo "All parameters:"
  echo "$params" | jq '{
    max_block_net_usage,
    target_block_net_usage_pct,
    max_transaction_net_usage,
    base_per_transaction_net_usage,
    net_usage_leeway,
    context_free_discount_net_usage_num,
    context_free_discount_net_usage_den,
    max_block_cpu_usage,
    target_block_cpu_usage_pct,
    max_transaction_cpu_usage,
    min_transaction_cpu_usage,
    max_transaction_lifetime,
    deferred_trx_expiration_window,
    max_transaction_delay,
    max_inline_action_size,
    max_inline_action_depth,
    max_authority_depth
  }'
}

cmd_increase() {
  need cleos
  need jq

  [[ -f "$APPROVERS_FILE" ]] || {
    echo "Missing $APPROVERS_FILE"
    exit 1
  }

  # Save current value for restore
  local current_size
  current_size="$(get_current_inline_size)"
  echo "Current max_inline_action_size: $current_size"
  echo "Will increase to: $NEW_MAX_INLINE_ACTION_SIZE"
  echo
  echo "IMPORTANT: Save this value to restore later:"
  echo "  export ORIGINAL_MAX_INLINE_ACTION_SIZE=$current_size"
  echo

  local ts proposal trx_file
  ts="$(date +%s)"
  proposal="incsz$(b32_name "$ts")"
  proposal="${proposal:0:12}"
  trx_file="trx-setparams-increase.json"

  create_setparams_trx "$NEW_MAX_INLINE_ACTION_SIZE" "$trx_file"
  propose_trx "$proposal" "$trx_file" "increase max_inline_action_size to $NEW_MAX_INLINE_ACTION_SIZE"
  print_approve_commands "$proposal"

  echo "============================================================"
  echo "DEPLOYMENT WORKFLOW"
  echo "============================================================"
  echo
  echo "1. Get approvals for proposal: $proposal"
  echo "2. Execute the proposal to increase max_inline_action_size"
  echo "3. Deploy your system contract"
  echo "4. Run: ORIGINAL_MAX_INLINE_ACTION_SIZE=$current_size ./adjust-inline-action-size.sh restore"
  echo
}

cmd_restore() {
  need cleos
  need jq

  [[ -f "$APPROVERS_FILE" ]] || {
    echo "Missing $APPROVERS_FILE"
    exit 1
  }

  if [[ -z "$ORIGINAL_MAX_INLINE_ACTION_SIZE" ]]; then
    echo "ERROR: ORIGINAL_MAX_INLINE_ACTION_SIZE not set"
    echo "Set it to the value you want to restore, e.g.:"
    echo "  export ORIGINAL_MAX_INLINE_ACTION_SIZE=300000"
    exit 1
  fi

  local current_size
  current_size="$(get_current_inline_size)"
  echo "Current max_inline_action_size: $current_size"
  echo "Will restore to: $ORIGINAL_MAX_INLINE_ACTION_SIZE"
  echo

  local ts proposal trx_file
  ts="$(date +%s)"
  proposal="ressz$(b32_name "$ts")"
  proposal="${proposal:0:12}"
  trx_file="trx-setparams-restore.json"

  create_setparams_trx "$ORIGINAL_MAX_INLINE_ACTION_SIZE" "$trx_file"
  propose_trx "$proposal" "$trx_file" "restore max_inline_action_size to $ORIGINAL_MAX_INLINE_ACTION_SIZE"
  print_approve_commands "$proposal"
}

usage() {
  echo "Usage: $0 <command>"
  echo
  echo "Commands:"
  echo "  status    - Show current blockchain parameters"
  echo "  increase  - Create proposal to increase max_inline_action_size"
  echo "  restore   - Create proposal to restore max_inline_action_size"
  echo
  echo "Environment variables:"
  echo "  API_URL                        - API endpoint (default: https://api.waxsweden.org)"
  echo "  PROPOSER                       - Account submitting proposals (required)"
  echo "  APPROVERS_FILE                 - Path to approvers JSON (default: ./approvers.json)"
  echo "  NEW_MAX_INLINE_ACTION_SIZE     - Size to increase to (default: 800000 > 512KB)"
  echo "  ORIGINAL_MAX_INLINE_ACTION_SIZE - Size to restore to (required for restore)"
  echo
  echo "Example workflow:"
  echo "  1. export PROPOSER=myaccount"
  echo "  2. ./adjust-inline-action-size.sh status"
  echo "  3. ./adjust-inline-action-size.sh increase"
  echo "  4. [get approvals and execute]"
  echo "  5. [deploy system contract]"
  echo "  6. export ORIGINAL_MAX_INLINE_ACTION_SIZE=300000"
  echo "  7. ./adjust-inline-action-size.sh restore"
  echo "  8. [get approvals and execute]"
}

main() {
  local cmd="${1:-}"

  case "$cmd" in
    status)
      cmd_status
      ;;
    increase)
      cmd_increase
      ;;
    restore)
      cmd_restore
      ;;
    *)
      usage
      exit 1
      ;;
  esac
}

main "$@"
