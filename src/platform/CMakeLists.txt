# =============================================================================
# src/platform/CMakeLists.txt - Platform abstraction layer
# =============================================================================

# Platform abstraction layer sources
set(PLATFORM_SOURCES
    simd_detect.c
    mem_aligned.c
    error.c
)

# Platform-specific sources (selected by OS)
if(WIN32)
    list(APPEND PLATFORM_SOURCES platform_win.c)
elseif(APPLE)
    list(APPEND PLATFORM_SOURCES platform_macos.c)
elseif(UNIX AND NOT APPLE)
    list(APPEND PLATFORM_SOURCES platform_linux.c)
endif()

# Create platform object library
add_library(finkit_platform OBJECT ${PLATFORM_SOURCES})

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
