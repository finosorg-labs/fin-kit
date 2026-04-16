#!/bin/bash
# Coverage report generation script for fin-kit

set -e  # Exit on error, but we'll handle build errors specially

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build/coverage"

echo "============================================================"
echo "fin-kit Coverage Report Generator"
echo "============================================================"
echo ""

# Clean previous build
if [ -d "$BUILD_DIR" ]; then
    echo "Cleaning previous coverage build..."
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with coverage flags
echo "Configuring with coverage instrumentation..."
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_C_FLAGS="-fprofile-arcs -ftest-coverage -O0" \
      -DCMAKE_EXE_LINKER_FLAGS="-fprofile-arcs -ftest-coverage" \
      "$PROJECT_ROOT"

# Build
echo ""
echo "Building tests only..."
cmake --build . --target finkit_tests --parallel

# Check if test binary exists
if [ ! -f "./bin/finkit_tests" ]; then
    echo "Error: Test binary not found at ./bin/finkit_tests"
    exit 1
fi

# Run tests
echo ""
echo "Running tests..."
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

for gcda in $(find "$BUILD_DIR/src/matrix" -name "*.gcda" 2>/dev/null | sort); do
    dir=$(dirname "$gcda")
    base=$(basename "$gcda" .gcda)
    # Remove .c from base if it exists (e.g., gemm_scalar.c.gcda -> gemm_scalar)
    base=${base%.c}
    filename="${base}.c"

    # Run gcov in the directory containing the .gcda/.gcno files
    output=$(cd "$dir" && gcov "$base.gcda" 2>/dev/null)

    # Extract coverage info - format: "Lines executed:98.06% of 103"
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
fi

echo ""
echo "Detailed .gcov files available in:"
echo "  $BUILD_DIR/src/matrix/CMakeFiles/matrix_obj.dir/"
echo ""

exit 0
