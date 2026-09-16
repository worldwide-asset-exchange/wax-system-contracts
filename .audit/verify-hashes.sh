#!/usr/bin/env bash
# WCAP v1 — invariant I7 (upgrade integrity), taxonomy class E4.
#
# Compares the WASM produced by the reproducible build against the code hash
# live on WAX mainnet. A mismatch means the source being audited is not the
# code securing the chain, which is a P0 finding in its own right.
#
# Usage:  .audit/verify-hashes.sh [build-dir]        (default: ./build)
# Re-run quarterly per WCAP Phase 7.

set -euo pipefail

BUILD_DIR="${1:-build}"
API="${WAX_API:-https://wax.greymass.com}"

# contract account on mainnet  ->  wasm produced by the build
declare -A DEPLOYED=(
  [eosio]=eosio.system
  [eosio.msig]=eosio.msig
  [eosio.token]=eosio.token
  [eosio.wrap]=eosio.wrap
)

fail=0
printf '%-13s %-66s %-66s %s\n' ACCOUNT ON-CHAIN BUILT MATCH

for account in "${!DEPLOYED[@]}"; do
  contract="${DEPLOYED[$account]}"

  wasm="$(find "$BUILD_DIR" -name "${contract}.wasm" -print -quit)"
  if [[ -z "$wasm" ]]; then
    printf '%-13s %-66s %-66s %s\n' "$account" "-" "NOT BUILT" "SKIP"
    fail=1
    continue
  fi

  built="$(sha256sum "$wasm" | cut -d' ' -f1)"
  onchain="$(curl -sf --max-time 20 -X POST "$API/v1/chain/get_code_hash" \
               -d "{\"account_name\":\"$account\"}" | jq -r '.code_hash')"

  if [[ "$built" == "$onchain" ]]; then
    printf '%-13s %-66s %-66s %s\n' "$account" "$onchain" "$built" "YES"
  else
    printf '%-13s %-66s %-66s %s\n' "$account" "$onchain" "$built" "** NO **"
    fail=1
  fi
done

if (( fail )); then
  echo
  echo "MISMATCH or missing artifact — see WCAP invariant I7 / class E4."
  echo "A mismatch is not automatically a vulnerability: the deployed contract may"
  echo "predate this commit. Resolve it to a specific deployed commit before"
  echo "concluding, and record the result in .audit/scope.yaml."
  exit 1
fi

echo
echo "All deployed contracts match the audited source. I7 holds."
