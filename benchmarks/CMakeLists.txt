#
# benchmarks/CMakeLists.txt - Benchmark configuration
#

if(NOT FC_BUILD_BENCHMARKS)
    message(STATUS "Benchmarks: disabled (FC_BUILD_BENCHMARKS=OFF)")
    return()
endif()

# Benchmark framework library
add_library(bench_framework OBJECT
    bench_framework.c
)

target_include_directories(bench_framework
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_BINARY_DIR}/include
)

target_link_libraries(bench_framework
    PUBLIC
        finkit::finkit
)

# Benchmark executable
add_executable(finkit_benchmarks
    bench_main.c
    matrix/bench_matrix.c
    matrix/bench_vector_ops.c
    matrix/bench_gemm.c
    matrix/bench_gemv.c
    matrix/bench_transpose.c
    matrix/bench_decompose.c
    matrix/bench_solve.c
    matrix/bench_tridiag.c
    codec/bench_codec.c
    codec/bench_fix_codec.c
)

# Add codec benchmarks subdirectory
add_subdirectory(codec)

# Link with benchmark framework and combined library
target_link_libraries(finkit_benchmarks
    PRIVATE
        bench_framework
        finkit::finkit
)

target_compile_definitions(finkit_benchmarks PRIVATE FC_STATIC_BUILD)

# Include directories
target_include_directories(finkit_benchmarks
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_BINARY_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Compiler flags for benchmarks
target_compile_options(finkit_benchmarks PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off)

# Create output directory for results
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/benchmarks/results)

# Installation
install(TARGETS finkit_benchmarks
    RUNTIME DESTINATION bin
)
