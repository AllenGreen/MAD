#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"

usage() {
    echo "Usage: $0 [test_filter]"
    echo ""
    echo "Builds (if needed) and runs tests."
    echo ""
    echo "Arguments:"
    echo "  test_filter   Optional regex to filter which tests to run"
    exit 0
}

case "${1:-}" in
    -h|--help|help)
        usage
        ;;
esac

# Ensure build is up to date
"${PROJECT_DIR}/build.sh" Debug

echo "==> Running tests"
cd "${BUILD_DIR}"

if [[ -n "${1:-}" ]]; then
    ctest --output-on-failure --test-dir "${BUILD_DIR}" -R "$1"
else
    ctest --output-on-failure --test-dir "${BUILD_DIR}"
fi

echo "==> Tests complete"
