#
# fin-kit High-Performance Financial Computing Library
#
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
#

cmake_minimum_required(VERSION 3.16)
project(fin-kit VERSION 1.0.0 LANGUAGES C)

# Cross-compilation support: inject target platform macros based on toolchain file
if(CMAKE_TOOLCHAIN_FILE)
    get_filename_component(_tc_name "${CMAKE_TOOLCHAIN_FILE}" NAME)
    if(_tc_name MATCHES "mingw")
        # MinGW cross-compiler on Linux does not define _WIN32, so platform.h
        # would fall through to the else branch. Override it explicitly via -D.
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

# Platform name for output directory hierarchy
# Format: <os>_<arch>  e.g. linux_amd64, windows_amd64, macos_arm64
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(_os_name "windows")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(_os_name "macos")
else()
    set(_os_name "linux")
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm64|aarch64$" OR CMAKE_C_COMPILER_TARGET MATCHES "aarch64")
    set(_arch_name "arm64")
else()
    set(_arch_name "amd64")
endif()

set(_platform_dir "${_os_name}_${_arch_name}")

# Output directories: all build artifacts live under build/bin and build/lib
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Compiler flags
add_compile_options(-Wall -Wextra -Wpedantic -ffp-contract=off)

# Build options
option(FC_BUILD_TESTS "Build tests" ON)
option(FC_BUILD_BENCHMARKS "Build benchmarks" ON)
option(FC_BUILD_SHARED "Build shared library" OFF)
option(FC_ENABLE_SANITIZERS "Enable sanitizers (Debug only)" OFF)
option(FC_USE_PREBUILT_MODULES "Use prebuilt libraries from submodules instead of building from source" OFF)

# Sanitizer configuration
if(FC_ENABLE_SANITIZERS)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.github/sanitizers.cmake)
        include(${CMAKE_CURRENT_SOURCE_DIR}/.github/sanitizers.cmake)
    endif()
endif()

# Third-party libraries (populated in W3)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/CMakeLists.txt)
    add_subdirectory(third_party)

    # For GCC: Remove sanitizer flags from third_party targets
    # (Clang uses blacklist, but GCC doesn't support it)
    if(FC_ENABLE_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "GNU")
        foreach(_target finkit_3rdparty_lz4 finkit_3rdparty_xxhash finkit_3rdparty_zstd)
            if(TARGET ${_target})
                get_target_property(_compile_opts ${_target} COMPILE_OPTIONS)
                if(_compile_opts)
                    list(FILTER _compile_opts EXCLUDE REGEX "fsanitize")
                    set_target_properties(${_target} PROPERTIES COMPILE_OPTIONS "${_compile_opts}")
                endif()

                get_target_property(_link_opts ${_target} LINK_OPTIONS)
                if(_link_opts)
                    list(FILTER _link_opts EXCLUDE REGEX "fsanitize")
                    set_target_properties(${_target} PROPERTIES LINK_OPTIONS "${_link_opts}")
                endif()
            endif()
        endforeach()
    endif()
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

# Helper macro to add module (prebuilt from submodule or source)
macro(add_fin_module MODULE_NAME MODULE_PATH)
    # Check if this is a submodule (path starts with core/) or main project module (path starts with src/)
    string(SUBSTRING ${MODULE_PATH} 0 5 _path_prefix)

    if(FC_USE_PREBUILT_MODULES AND _path_prefix STREQUAL "core/")
        # Use prebuilt library from submodule build directory
        # Try platform-specific directory first, then fall back to build root
        set(_module_lib_with_dir "${CMAKE_CURRENT_SOURCE_DIR}/core/${MODULE_NAME}/build/${_platform_dir}/libfinkit_${MODULE_NAME}_static.a")
        set(_module_lib_root "${CMAKE_CURRENT_SOURCE_DIR}/core/${MODULE_NAME}/build/libfinkit_${MODULE_NAME}_static.a")

        if(EXISTS ${_module_lib_with_dir})
            set(_module_lib ${_module_lib_with_dir})
        elseif(EXISTS ${_module_lib_root})
            set(_module_lib ${_module_lib_root})
        else()
            message(WARNING "Prebuilt ${MODULE_NAME} library not found, skipping. Tried:\n  ${_module_lib_with_dir}\n  ${_module_lib_root}")
            return()
        endif()

        add_library(finkit_${MODULE_NAME} STATIC IMPORTED)
        set_target_properties(finkit_${MODULE_NAME} PROPERTIES
            IMPORTED_LOCATION ${_module_lib}
            INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/core/${MODULE_NAME}/include"
        )
        message(STATUS "Using prebuilt ${MODULE_NAME} library: ${_module_lib}")
    else()
        # Build from source (either FC_USE_PREBUILT_MODULES=OFF or this is a main project module)
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE_PATH})
            add_subdirectory(${MODULE_PATH})
        endif()
    endif()
endmacro()

# Module libraries (populated in subsequent phases)
# M00 Platform (base layer)
add_fin_module(platform core/platform)

# M06 Codec (depends on platform, so tell it platform is already available)
# Note: Codec submodule only supports prebuilt libraries, not source builds
if(NOT FC_USE_PREBUILT_MODULES)
    set(FC_PLATFORM_ALREADY_ADDED TRUE)
    # Force codec to use prebuilt even when other modules build from source
    set(_codec_lib "${CMAKE_CURRENT_SOURCE_DIR}/core/codec/build/${_platform_dir}/libfinkit_codec_static.a")
    if(EXISTS ${_codec_lib})
        add_library(finkit_codec STATIC IMPORTED)
        set_target_properties(finkit_codec PROPERTIES
            IMPORTED_LOCATION ${_codec_lib}
            INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/core/codec/include"
        )
        message(STATUS "Using prebuilt codec library (codec doesn't support source builds): ${_codec_lib}")
    else()
        message(WARNING "Codec prebuilt library not found, skipping codec module: ${_codec_lib}")
    endif()
else()
    add_fin_module(codec core/codec)
endif()
add_fin_module(matrix src/matrix)

# M02 Statistics
add_fin_module(stats src/stats)

# M03 Sorting
add_fin_module(sort src/sort)

# M04 Math functions
add_fin_module(math_funcs src/math_funcs)

# M05 Random number generation
add_fin_module(random src/random)

# M07 Data structures
add_fin_module(ds src/ds)

# M08 Hash
add_fin_module(hash src/hash)

# M09 String operations
add_fin_module(string_ops src/string_ops)

# M10 Optimization
add_fin_module(optim src/optim)

# M11 Crypto trading
add_fin_module(crypto_trading src/crypto_trading)

# M12 Exchange
add_fin_module(exchange src/exchange)

# Test suite
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests AND FC_BUILD_TESTS)
    enable_testing()
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
    ${CMAKE_BINARY_DIR}/include/fin-kit/version.h
    @ONLY
)

# Install generated version header
install(
    FILES ${CMAKE_BINARY_DIR}/include/fin-kit/version.h
    DESTINATION include/fin-kit
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

# Collect all object libraries
set(ALL_OBJECTS "")
if(TARGET finkit_platform)
    get_target_property(_platform_type finkit_platform TYPE)
    if(_platform_type STREQUAL "OBJECT_LIBRARY")
        list(APPEND ALL_OBJECTS $<TARGET_OBJECTS:finkit_platform>)
    endif()
endif()
if(TARGET matrix_obj)
    list(APPEND ALL_OBJECTS $<TARGET_OBJECTS:matrix_obj>)
endif()
if(TARGET codec_obj)
    list(APPEND ALL_OBJECTS $<TARGET_OBJECTS:codec_obj>)
endif()

# Create combined library for users who link against the whole library
if(ALL_EXPORTS)
    add_library(finkit STATIC ${ALL_OBJECTS})
    add_library(finkit::finkit ALIAS finkit)
    target_include_directories(finkit PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    target_compile_definitions(finkit PRIVATE FC_STATIC_BUILD)

    # Link platform library (either built from source or prebuilt)
    if(TARGET finkit_platform)
        target_link_libraries(finkit PRIVATE finkit_platform m)
    else()
        target_link_libraries(finkit PRIVATE m)
    endif()

    # Install library and register the export set first (creates "fin-kit-targets")
    # Only export if using prebuilt modules (to avoid export set conflicts with submodules)
    if(FC_USE_PREBUILT_MODULES)
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
    else()
        # In source build mode, just install the library without export
        install(TARGETS finkit
            ARCHIVE DESTINATION lib
            LIBRARY DESTINATION lib
        )
    endif()
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
message(STATUS "  Platform:   ${_platform_dir}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Output:     ${CMAKE_BINARY_DIR}/{bin,lib}/")
message(STATUS "============================================================")
message(STATUS "")
