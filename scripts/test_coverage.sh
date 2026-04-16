#!/bin/bash
# Run tests with coverage on already built binaries
# Usage: scripts/test_coverage.sh [build_dir]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${1:-$PROJECT_ROOT/build/linux_amd64}"

# Check if test binary exists
if [ ! -f "$BUILD_DIR/bin/finkit_tests" ]; then
    echo "Error: Test binary not found at $BUILD_DIR/bin/finkit_tests"
    echo "Please build with coverage flags first:"
    echo "  cmake -B $BUILD_DIR -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS=\"-fprofile-arcs -ftest-coverage -O0\""
    echo "  cmake --build $BUILD_DIR --target finkit_tests"
    exit 1
fi

# Run tests
echo ""
echo "Running tests..."
cd "$BUILD_DIR"
./bin/finkit_tests

# Generate coverage report
echo ""
echo "============================================================"
echo "Coverage Report - Matrix Module"
echo "============================================================"
echo ""

# Process matrix module files
total_lines=0
total_executed=0

for gcda in $(find src/matrix -name "*.gcda" 2>/dev/null | sort); do
    dir=$(dirname "$gcda")
    base=$(basename "$gcda" .gcda)
    # base is like "gemm_dispatch.c", extract the actual filename
    filename="$base"

    # Run gcov in the directory containing the .gcda/.gcno files
    output=$(cd "$dir" && gcov "$base.gcda" 2>/dev/null)

    # Extract coverage info
    if echo "$output" | grep -q "Lines executed"; then
        percent=$(echo "$output" | grep "Lines executed" | head -1 | sed 's/Lines executed:\([0-9.]*\)% of \([0-9]*\)/\1/')
        lines=$(echo "$output" | grep "Lines executed" | head -1 | sed 's/Lines executed:\([0-9.]*\)% of \([0-9]*\)/\2/')

        if [ -n "$lines" ] && [ -n "$percent" ]; then
            executed=$(awk "BEGIN {printf \"%.0f\", $lines * $percent / 100}")
            printf "%-30s %6d / %6d lines (%6.2f%%)\n" "$filename" "$executed" "$lines" "$percent"
            total_lines=$((total_lines + lines))
            total_executed=$((total_executed + executed))
        fi
    fi
done

if [ "$total_lines" -gt 0 ]; then
    overall=$(awk "BEGIN {printf \"%.2f\", ($total_executed / $total_lines) * 100}")
    echo ""
    echo "------------------------------------------------------------"
    printf "%-30s %6d / %6d lines (%6.2f%%)\n" "TOTAL" "$total_executed" "$total_lines" "$overall"
    echo "============================================================"
else
    echo "No coverage data found. Make sure the build was configured with coverage flags."
    exit 1
fi

echo ""
echo "Detailed .gcov files available in:"
echo "  $BUILD_DIR/src/matrix/CMakeFiles/matrix_obj.dir/"
echo ""

exit 0
