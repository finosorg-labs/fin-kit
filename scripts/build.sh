#!/bin/bash
# =============================================================================
# build.sh - fin-kit build script
# =============================================================================

set -e

BUILD_TYPE="${BUILD_TYPE:-Release}"
BUILD_DIR="${BUILD_DIR:-build}"

echo "============================================================"
echo " fin-kit build script"
echo "============================================================"
echo " Build type: ${BUILD_TYPE}"
echo " Build dir:  ${BUILD_DIR}"
echo "============================================================"

# Configure
cmake -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"

# Build
cmake --build "${BUILD_DIR}" --parallel

echo "============================================================"
echo " Build complete: ${BUILD_DIR}"
echo "============================================================"
