#!/bin/bash
#
# run_benchmarks.sh - Run all benchmarks
#

set -e

BUILD_DIR="${BUILD_DIR:-build}"

echo "============================================================"
echo " fin-kit benchmark runner"
echo "============================================================"

if [ ! -d "${BUILD_DIR}" ]; then
    echo "Error: Build directory not found: ${BUILD_DIR}"
    echo "Run build.sh first"
    exit 1
fi

# Run all benchmarks in the build directory
if [ -d "${BUILD_DIR}/benchmarks" ]; then
    echo "Running benchmarks..."
    for bench in "${BUILD_DIR}"/benchmarks/*; do
        if [ -x "${bench}" ]; then
            echo "--- $(basename ${bench}) ---"
            "${bench}"
        fi
    done
else
    echo "Warning: benchmarks/ directory not found in build"
fi

echo "============================================================"
echo " Benchmark complete"
echo "============================================================"
