# =============================================================================
# CompilerFlags.cmake - Compiler-specific flags for different SIMD levels
# =============================================================================

set(FC_AVX512_FLAGS "-mavx512f -mavx512dq -mavx512bw")
set(FC_AVX2_FLAGS   "-mavx2 -mfma")
set(FC_SSE42_FLAGS  "-msse4.2")

# Common flags shared by all SIMD levels
set(FC_COMMON_FLAGS "-Wall -Wextra -Wpedantic -ffp-contract=off")
