#!/usr/bin/env bash
# WCAP v1 Phase 2 — the full automated sweep.
#
# Two engines because neither alone is enough:
#   semgrep      — pattern classes regex can decide (B1, B2, C2, D1, D2)
#   wcap-check.py — classes needing type resolution or decl->def linking (A1, C4, C5)
#
# Usage: .audit/rules/run-sweep.sh [contracts_dir]
set -uo pipefail
DIR="${1:-contracts}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

echo "=== semgrep (Antelope ruleset) ==="
docker run --rm -v "$ROOT":/src -w /src semgrep/semgrep:latest \
  semgrep --config .audit/rules/antelope.yaml --metrics=off --quiet "$DIR" || true

echo
echo "=== wcap-check.py (type-aware) ==="
python3 .audit/rules/wcap-check.py "$DIR" || true

echo
echo "Every hit must be triaged: it becomes a finding, or it is dismissed with a written"
echo "reason. An unexplained suppression is how a real finding gets lost. Triage records"
echo "and findings are tracked privately in Jira (WBP) and Confluence, not in this repo."
