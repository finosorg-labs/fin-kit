#
# DetectSIMD.cmake - CPU SIMD capability detection
#
# Populates FC_SIMD_LEVEL with: SCALAR, SSE42, AVX2, or AVX512
# Then creates an interface library that links compiler flags from CompilerFlags.cmake
#

include(CheckCSourceCompiles)

# Detect SSE4.2
check_c_source_compiles("
    #include <immintrin.h>
    int main() {
        __m128i a = _mm_setzero_si128();
        return _mm_extract_epi32(a, 0);
    }
" FC_HAS_SSE42)

# Detect AVX2
check_c_source_compiles("
    #include <immintrin.h>
    int main() {
        __m256i a = _mm256_setzero_si256();
        return _mm256_extract_epi32(a, 0);
    }
" FC_HAS_AVX2)

# Detect AVX-512
check_c_source_compiles("
    #include <immintrin.h>
    int main() {
        __m512i a = _mm512_setzero_si512();
        return _mm512_extract_epi32(a, 0);
    }
" FC_HAS_AVX512)

# Determine the highest SIMD level
if(FC_HAS_AVX512)
    set(FC_SIMD_LEVEL "AVX512")
elseif(FC_HAS_AVX2)
    set(FC_SIMD_LEVEL "AVX2")
elseif(FC_HAS_SSE42)
    set(FC_SIMD_LEVEL "SSE42")
else()
    set(FC_SIMD_LEVEL "SCALAR")
endif()

# Promote to cache so it persists across re-runs and is visible to IDEs
set(FC_SIMD_LEVEL "${FC_SIMD_LEVEL}" CACHE STRING "Highest supported SIMD instruction set")
message(STATUS "SIMD level: ${FC_SIMD_LEVEL}")
