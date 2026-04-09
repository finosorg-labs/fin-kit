# =============================================================================
# DetectSIMD.cmake - CPU SIMD capability detection
# =============================================================================
# Populates FC_SIMD_LEVEL with: SCALAR, SSE42, AVX2, or AVX512
# =============================================================================

include(CheckCSourceCompiles)

# Detect SSE4.2
check_c_source_compiles("
    #include <immintrin.h>
    int main() {
        __m128i a = _mm_setzero_si128();
        return _mm_extract_epi32(a, 0);
    }
" FC_HAS_SSE42)
if(FC_HAS_SSE42)
    set(FC_SIMD_LEVEL "SSE42" PARENT_SCOPE)
endif()

# Detect AVX2
check_c_source_compiles("
    #include <immintrin.h>
    int main() {
        __m256i a = _mm256_setzero_si256();
        return _mm256_extract_epi32(a, 0);
    }
" FC_HAS_AVX2)
if(FC_HAS_AVX2)
    set(FC_SIMD_LEVEL "AVX2" PARENT_SCOPE)
endif()

# Detect AVX-512
check_c_source_compiles("
    #include <immintrin.h>
    int main() {
        __m512i a = _mm512_setzero_si512();
        return _mm512_extract_epi32(a, 0);
    }
" FC_HAS_AVX512)
if(FC_HAS_AVX512)
    set(FC_SIMD_LEVEL "AVX512" PARENT_SCOPE)
endif()

# Default to SCALAR
if(NOT DEFINED FC_SIMD_LEVEL)
    set(FC_SIMD_LEVEL "SCALAR" PARENT_SCOPE)
endif()

message(STATUS "SIMD level: ${FC_SIMD_LEVEL}")
