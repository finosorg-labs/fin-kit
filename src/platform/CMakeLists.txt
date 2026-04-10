# =============================================================================
# src/platform/CMakeLists.txt - Platform abstraction layer
# =============================================================================

# Platform abstraction layer sources
set(PLATFORM_SOURCES
    simd_detect.c
    mem_aligned.c
    error.c
)

# Platform-specific sources (selected by TARGET OS, not host OS)
# CMAKE_SYSTEM_NAME is set by the toolchain file and reflects the target platform
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    list(APPEND PLATFORM_SOURCES platform_win.c)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND PLATFORM_SOURCES platform_macos.c)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND PLATFORM_SOURCES platform_linux.c)
endif()

# Create platform object library
add_library(finkit_platform OBJECT ${PLATFORM_SOURCES})

target_compile_definitions(finkit_platform PRIVATE FC_STATIC_BUILD)

# Set include directories for this library
target_include_directories(finkit_platform
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
    PRIVATE
        ${CMAKE_BINARY_DIR}/include
)

# Apply common compiler flags
if(MSVC)
    target_compile_options(finkit_platform PRIVATE /W4 /fp:precise)
else()
    target_compile_options(finkit_platform PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off)
endif()

# Create static library from platform objects
add_library(finkit_platform_static STATIC $<TARGET_OBJECTS:finkit_platform>)
target_include_directories(finkit_platform_static
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

# Install platform headers
install(
    FILES
        platform.h
        simd_detect.h
        mem_aligned.h
        error.h
    DESTINATION include/fin-kit/platform
)

# Alias for use in other parts of the project
add_library(finkit::platform ALIAS finkit_platform_static)
