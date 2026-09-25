#!/usr/bin/env bash
# WCAP v1 — invariant I7 forensics (class E4). Companion to verify-hashes.sh.
#
# verify-hashes.sh answers "does the CURRENT source reproduce what is deployed?". This
# script answers the follow-up when it does not: "which tag, built with which toolchain,
# does?". It builds one git ref of this repository inside one toolchain image and compares
# every contract WASM it produces against the code hash live on mainnet.
#
# Usage:
#   .audit/reproduce-deployed.sh <git-ref> <docker-image> [extra cmake args...]
#
# Examples (the ones that established the 2026-09 provenance table in README.md):
#   .audit/reproduce-deployed.sh wax-1.7.0-2.0.0 waxteam/dev:wax-1.6.1-1.2.1
#   .audit/reproduce-deployed.sh wax-3.1.0-wax1-3.0.1 waxteam/dev:v3.1.0-wax1-v3.0.1 \
#       -Dcdt_DIR=/tmp/cdt/build/lib/cmake/cdt
#
# The build runs with --rm and no --name, so it never collides with the Makefile's
# development container, and it is capped so it cannot take a shared host down: set
# WCAP_DOCKER_OPTS to override the default "--cpus 6" (e.g. add --cgroup-parent=...).
# The ref is exported with `git archive` into a scratch directory; the working tree is
# never touched. Tests are not built.
set -euo pipefail

REF="${1:?git ref (tag or commit)}"
IMAGE="${2:?toolchain image}"
shift 2
EXTRA_CMAKE="$*"
API="${WAX_API:-https://wax.greymass.com}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="${WCAP_SCRATCH:-$(mktemp -d)}/reproduce-${REF//\//_}"
DOCKER_OPTS="${WCAP_DOCKER_OPTS:---cpus 6}"

declare -A DEPLOYED=(
  [eosio]=eosio.system
  [eosio.msig]=eosio.msig
  [eosio.token]=eosio.token
  [eosio.wrap]=eosio.wrap
)

rm -rf "$WORK" && mkdir -p "$WORK"
git -C "$ROOT" archive "$REF" | tar -x -C "$WORK"
echo "ref $REF ($(git -C "$ROOT" rev-parse --short "$REF^{commit}")) -> $WORK"
echo "image $IMAGE"

docker pull -q "$IMAGE" >/dev/null
# shellcheck disable=SC2086
docker run --rm $DOCKER_OPTS -e EXTRA_CMAKE="$EXTRA_CMAKE" -v "$WORK":/opt/contracts -w /opt/contracts "$IMAGE" bash -lc '
  set -e
  (cdt-cpp --version || eosio-cpp --version) 2>/dev/null | head -1
  rm -rf build && mkdir build && cd build
  cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF ${EXTRA_CMAKE:-} .. >cmake.log 2>&1 || { tail -20 cmake.log; exit 1; }
  make -j"${WCAP_JOBS:-6}" >make.log 2>&1 || { tail -30 make.log; exit 1; }
' || { echo "build failed in $IMAGE (logs under $WORK/build)"; exit 1; }

fail=0
printf '%-13s %-66s %s\n' ACCOUNT BUILT MATCH
for account in "${!DEPLOYED[@]}"; do
  contract="${DEPLOYED[$account]}"
  wasm="$(find "$WORK/build" -name "${contract}.wasm" -not -path '*CMakeFiles*' -print -quit)"
  if [[ -z "$wasm" ]]; then
    printf '%-13s %-66s %s\n' "$account" "NOT BUILT" "SKIP"; fail=1; continue
  fi
  built="$(sha256sum "$wasm" | cut -d' ' -f1)"
  onchain="$(curl -sf --max-time 20 -X POST "$API/v1/chain/get_code_hash" \
               -d "{\"account_name\":\"$account\"}" | jq -r '.code_hash')"
  if [[ "$built" == "$onchain" ]]; then
    printf '%-13s %-66s %s\n' "$account" "$built" "YES"
  else
    printf '%-13s %-66s %s\n' "$account" "$built" "no"; fail=1
  fi
done
echo
echo "A YES row means: the code at $REF, built in $IMAGE, is the code securing that account"
echo "on mainnet. Record ref, image and hash in the provenance table in .audit/README.md."
exit $fail
