# Matrix operations module

set(MATRIX_SOURCES
    vector_ops.c
    gemm_scalar.c
    gemm_sse42.c
    gemm_avx2.c
    gemm_avx512.c
    gemm_dispatch.c
    gemv.c
    transpose.c
    decompose.c
    solve.c
)

# Create object library for matrix module
add_library(matrix_obj OBJECT ${MATRIX_SOURCES})

target_include_directories(matrix_obj PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Enable internal tests if building tests
if(FC_BUILD_TESTS)
    target_compile_definitions(matrix_obj PRIVATE FC_ENABLE_INTERNAL_TESTS)
endif()

# Set compile flags for SIMD variants
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(vector_ops.c PROPERTIES COMPILE_FLAGS "-msse4.2 -mavx2 -mfma")
    set_source_files_properties(gemm_sse42.c PROPERTIES COMPILE_FLAGS "-msse4.2")
    set_source_files_properties(gemm_avx2.c PROPERTIES COMPILE_FLAGS "-mavx2 -mfma")
    set_source_files_properties(gemm_avx512.c PROPERTIES COMPILE_FLAGS "-mavx512f -mavx512dq -mfma")
    set_source_files_properties(gemv.c PROPERTIES COMPILE_FLAGS "-msse4.2 -mavx2 -mfma")
    set_source_files_properties(transpose.c PROPERTIES COMPILE_FLAGS "-mavx2")
elseif(MSVC)
    set_source_files_properties(vector_ops.c PROPERTIES COMPILE_FLAGS "/arch:AVX2")
    set_source_files_properties(gemm_avx2.c PROPERTIES COMPILE_FLAGS "/arch:AVX2")
    set_source_files_properties(gemm_avx512.c PROPERTIES COMPILE_FLAGS "/arch:AVX512")
    set_source_files_properties(gemv.c PROPERTIES COMPILE_FLAGS "/arch:AVX2")
    set_source_files_properties(transpose.c PROPERTIES COMPILE_FLAGS "/arch:AVX2")
endif()
