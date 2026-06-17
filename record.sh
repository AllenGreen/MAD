#!/usr/bin/env bash
# MAD capture helper: build (Release), run a scenario headlessly, encode every
# camera to video, and publish an HTML report to /Content for review.
#
#   ./record.sh scenarios/demo_3p.mad
#   ./record.sh scenarios/demo_3p.mad --no-publish
#
# Any extra args are forwarded to tools/capture.py.
set -euo pipefail
cd "$(dirname "$0")"

SCENARIO="${1:-scenarios/demo_3p.mad}"
shift || true

exec python3 tools/capture.py "$SCENARIO" "$@"
