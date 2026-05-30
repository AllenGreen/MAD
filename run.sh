#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
BUILD_TYPE="${MAD_BUILD_TYPE:-Debug}"

usage() {
    echo "Usage: $0 [args...]"
    echo ""
    echo "Builds (if needed) and runs the MAD executable."
    echo "All arguments are passed through to the game."
    echo ""
    echo "Environment:"
    echo "  MAD_BUILD_TYPE  Build type (default: Debug)"
    exit 0
}

case "${1:-}" in
    -h|--help|help)
        usage
        ;;
esac

# Ensure build is up to date
"${PROJECT_DIR}/build.sh" "${BUILD_TYPE}"

echo "==> Running MAD"
exec "${BUILD_DIR}/mad" "$@"
