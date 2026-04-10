# =============================================================================
# fin-kit High-Performance Financial Computing Library
# =============================================================================
# Build:
#   cmake -B build && cmake --build build
#
# Options:
#   -DCMAKE_BUILD_TYPE=Release   # Default
#   -DCMAKE_BUILD_TYPE=Debug
#   -DFC_ENABLE_SANITIZERS=ON   # ASan/UBSan/MSan (Debug only)
#
# Install:
#   cmake --install build --prefix /usr/local
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(fin-kit VERSION 1.0.0 LANGUAGES C)

# Cross-compilation support: inject target platform macros based on toolchain file
if(CMAKE_TOOLCHAIN_FILE)
    get_filename_component(_tc_name "${CMAKE_TOOLCHAIN_FILE}" NAME)
    if(_tc_name MATCHES "mingw")
        # MinGW cross-compiler on Linux does not define _WIN32, so platform.h
        # would fall through to the else branch. Override it explicitly.
        # Use -D to ensure the compiler sees the macro as an integer, not as a
        # token "FC_OS_WINDOWS=1" which would fail #if FC_OS_WINDOWS checks.
        add_compile_definitions(-DFC_OS_WINDOWS=1 -DFC_OS_LINUX=0 -DFC_OS_MACOS=0)
    endif()
endif()

# C standard: C11
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Coverage build type (GCC/Clang don't natively support "Coverage" as a type)
set(CMAKE_C_FLAGS_COVERAGE "-g -O0 -fprofile-arcs -ftest-coverage")
set(CMAKE_EXE_LINKER_FLAGS_COVERAGE "--coverage")
set(CMAKE_SHARED_LINKER_FLAGS_COVERAGE "--coverage")

# Output directories
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Compiler flags
if(MSVC)
    add_compile_options(/W4 /fp:precise /Ob0)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -ffp-contract=off)
endif()

# Build options
option(FC_BUILD_TESTS "Build tests" ON)
option(FC_BUILD_BENCHMARKS "Build benchmarks" ON)
option(FC_BUILD_SHARED "Build shared library" OFF)
option(FC_ENABLE_SANITIZERS "Enable sanitizers (Debug only)" OFF)

# Sanitizer configuration
if(FC_ENABLE_SANITIZERS)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "")
        if(MSVC)
            add_compile_options(/fsanitize=address)
        else()
            # FC_SANITIZER_TYPE is set by the CI matrix (address|undefined|thread|memory)
            # Only enable ONE sanitizer at a time to avoid conflicts.
            # GCC does not support -fsanitize=memory.
            # -fsanitize=address and -fsanitize=memory are mutually exclusive.
            if(FC_SANITIZER_TYPE STREQUAL "address")
                add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
                add_link_options(-fsanitize=address)
            elseif(FC_SANITIZER_TYPE STREQUAL "undefined")
                add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer)
                add_link_options(-fsanitize=undefined)
            elseif(FC_SANITIZER_TYPE STREQUAL "thread")
                add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
                add_link_options(-fsanitize=thread)
            elseif(FC_SANITIZER_TYPE STREQUAL "memory")
                # MSan is clang-only (GCC does not support -fsanitize=memory)
                add_compile_options(-fsanitize=memory -fno-omit-frame-pointer)
                add_link_options(-fsanitize=memory)
            else()
                # Fallback: address only
                add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
                add_link_options(-fsanitize=address)
            endif()
        endif()
    endif()
endif()

# Third-party libraries (populated in W3)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/CMakeLists.txt)
    add_subdirectory(third_party)
else()
    message(STATUS "third_party/: CMakeLists.txt will be added in W3")
endif()

# Include directory (public headers)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/include/CMakeLists.txt)
    include(${CMAKE_CURRENT_SOURCE_DIR}/include/CMakeLists.txt)
else()
    message(WARNING "include/CMakeLists.txt not found")
endif()

# CMake helper scripts (populated in W2)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cmake/DetectSIMD.cmake)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/DetectSIMD.cmake)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompilerFlags.cmake)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompilerFlags.cmake)
endif()

# Platform abstraction layer (populated in W2)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/platform)
    add_subdirectory(src/platform)
endif()

# Module libraries (populated in subsequent phases)
# M01 Matrix
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/matrix)
    add_subdirectory(src/matrix)
endif()

# M02 Statistics
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/stats)
    add_subdirectory(src/stats)
endif()

# M03 Sorting
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/sort)
    add_subdirectory(src/sort)
endif()

# M04 Math functions
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/math_funcs)
    add_subdirectory(src/math_funcs)
endif()

# M05 Random number generation
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/random)
    add_subdirectory(src/random)
endif()

# M06 Codec
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/codec)
    add_subdirectory(src/codec)
endif()

# M07 Data structures
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/ds)
    add_subdirectory(src/ds)
endif()

# M08 Hash
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/hash)
    add_subdirectory(src/hash)
endif()

# M09 String operations
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/string_ops)
    add_subdirectory(src/string_ops)
endif()

# M10 Optimization
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/optim)
    add_subdirectory(src/optim)
endif()

# M11 Crypto trading
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto_trading)
    add_subdirectory(src/crypto_trading)
endif()

# M12 Exchange
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/exchange)
    add_subdirectory(src/exchange)
endif()

# Test suite
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests AND FC_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Benchmark suite
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks AND FC_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# Install public headers
install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# Version header generation
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/fin-kit/version.h
    @ONLY
)

# ---------------------------------------------------------------------------
# Combined library and export set (must come after all add_subdirectory calls)
# ---------------------------------------------------------------------------
set(ALL_MODULES finkit_platform)
set(_deps "")
foreach(_mod IN LISTS ALL_MODULES)
    if(TARGET ${_mod})
        list(APPEND ALL_EXPORTS ${_mod})
    endif()
endforeach()

# Create combined library for users who link against the whole library
if(ALL_EXPORTS)
    add_library(finkit STATIC $<TARGET_OBJECTS:finkit_platform>)
    add_library(finkit::finkit ALIAS finkit)
    target_include_directories(finkit PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    target_compile_definitions(finkit PRIVATE FC_STATIC_BUILD)

    # Install library and register the export set first (creates "fin-kit-targets")
    install(TARGETS finkit
        EXPORT fin-kit-targets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
    )

    # Then export for build-tree consumers (find_package from build dir)
    export(EXPORT fin-kit-targets
        FILE "${CMAKE_BINARY_DIR}/${PROJECT_NAME}Targets.cmake"
        NAMESPACE ${PROJECT_NAME}::
    )

    install(EXPORT fin-kit-targets
        FILE fin-kitTargets.cmake
        NAMESPACE ${PROJECT_NAME}::
        DESTINATION lib/cmake/${PROJECT_NAME}
    )
endif()

# Install configuration
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cmake/InstallConfig.cmake)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/InstallConfig.cmake)
endif()

# Print build configuration
message(STATUS "")
message(STATUS "============================================================")
message(STATUS " fin-kit ${PROJECT_VERSION}")
message(STATUS "============================================================")
message(STATUS "  C Compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "============================================================")
message(STATUS "")
