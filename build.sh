#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
BUILD_TYPE="${1:-Debug}"
GENERATOR="Unix Makefiles"

# Use Ninja if available
if command -v ninja &>/dev/null; then
    GENERATOR="Ninja"
fi

usage() {
    echo "Usage: $0 [Debug|Release|clean]"
    echo ""
    echo "Commands:"
    echo "  Debug     Build with debug symbols and sanitizers (default)"
    echo "  Release   Build optimized release"
    echo "  clean     Remove build directory"
    echo ""
    echo "Options (via environment):"
    echo "  CMAKE_EXTRA_ARGS  Additional CMake arguments"
    exit 0
}

case "${1:-}" in
    -h|--help|help)
        usage
        ;;
    clean)
        echo "==> Cleaning build directory"
        rm -rf "${BUILD_DIR}"
        echo "==> Clean complete"
        exit 0
        ;;
esac

echo "==> Configuring (${BUILD_TYPE}, generator: ${GENERATOR})"
cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}" \
    -G "${GENERATOR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    ${CMAKE_EXTRA_ARGS:-}

echo "==> Building"
cmake --build "${BUILD_DIR}" --parallel "$(nproc)"

echo "==> Build complete: ${BUILD_DIR}/mad"
