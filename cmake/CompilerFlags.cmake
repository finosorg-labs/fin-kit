# =============================================================================
# CompilerFlags.cmake - Compiler-specific flags for different SIMD levels
# =============================================================================

if(MSVC)
    set(FC_AVX512_FLAGS "/arch:AVX512")
    set(FC_AVX2_FLAGS   "/arch:AVX2")
    set(FC_SSE42_FLAGS  "")  # x64 targets have SSE4.2 by default
else()
    set(FC_AVX512_FLAGS "-mavx512f -mavx512dq -mavx512bw")
    set(FC_AVX2_FLAGS   "-mavx2 -mfma")
    set(FC_SSE42_FLAGS  "-msse4.2")
endif()

# Common flags shared by all SIMD levels
if(MSVC)
    set(FC_COMMON_FLAGS "/W4 /fp:precise /Ob0")
else()
    set(FC_COMMON_FLAGS "-Wall -Wextra -Wpedantic -ffp-contract=off")
endif()
