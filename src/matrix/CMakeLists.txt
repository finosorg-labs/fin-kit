# Matrix operations module

set(MATRIX_SOURCES
    vector_ops.c
    gemm_scalar.c
    gemm_sse42.c
    gemm_dispatch.c
)

# Create object library for matrix module
add_library(matrix_obj OBJECT ${MATRIX_SOURCES})

target_include_directories(matrix_obj PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# Set compile flags for SIMD variants
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(gemm_sse42.c PROPERTIES COMPILE_FLAGS "-msse4.2")
endif()
