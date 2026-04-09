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

# C standard: C11
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags
if(MSVC)
    add_compile_options(/W4 /fp:precise /Ob0)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -ffp-contract=off)
endif()

# Output directories
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

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
            add_compile_options(-fsanitize=address -fsanitize=undefined -fsanitize=memory)
            add_link_options(-fsanitize=address -fsanitize=undefined -fsanitize=memory)
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
