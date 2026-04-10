# =============================================================================
# benchmarks/CMakeLists.txt - Benchmark configuration
# =============================================================================

if(NOT FC_BUILD_BENCHMARKS)
    message(STATUS "Benchmarks: disabled (FC_BUILD_BENCHMARKS=OFF)")
    return()
endif()

# Benchmark executable
add_executable(finkit_benchmarks
    bench_framework.c
    bench_main.c
    # Add individual module benchmark files here as they are created
)

# Link with platform library and math library
target_link_libraries(finkit_benchmarks
    PRIVATE
        finkit::platform
        m
)

target_compile_definitions(finkit_benchmarks PRIVATE FC_STATIC_BUILD)

# Include directories
target_include_directories(finkit_benchmarks
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR}/include
)

# Compiler flags for benchmarks
if(MSVC)
    # Disable optimizations for accurate measurements
    target_compile_options(finkit_benchmarks PRIVATE /W4 /fp:precise /Ob0 /Od)
else()
    target_compile_options(finkit_benchmarks PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off)
endif()

# Create output directory for results
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/benchmarks/results)

# Installation
install(TARGETS finkit_benchmarks
    RUNTIME DESTINATION bin
)
