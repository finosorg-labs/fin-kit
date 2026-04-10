# =============================================================================
# tests/CMakeLists.txt - Test configuration
# =============================================================================

if(NOT FC_BUILD_TESTS)
    message(STATUS "Tests: disabled (FC_BUILD_TESTS=OFF)")
    return()
endif()

# Test executable
add_executable(finkit_tests
    test_framework.c
    test_main.c
    # Add individual module test files here as they are created
)

# Link with platform library and math library
target_link_libraries(finkit_tests
    PRIVATE
        finkit::platform
        m
)

target_compile_definitions(finkit_tests PRIVATE FC_STATIC_BUILD)

# Include directories
target_include_directories(finkit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR}/include
)

# Compiler flags for tests
if(MSVC)
    target_compile_options(finkit_tests PRIVATE /W4 /fp:precise)
else()
    target_compile_options(finkit_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Run tests with ctest
enable_testing()

add_test(
    NAME "finkit_tests"
    COMMAND finkit_tests
)

# Platform-specific test configuration
if(WIN32)
    # Windows: Use proper DLL search path
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set_tests_properties(finkit_tests PROPERTIES
            ENVIRONMENT "PATH=${CMAKE_BINARY_DIR}\\lib;${CMAKE_BINARY_DIR}\\bin;$ENV{PATH}"
        )
    endif()
elseif(UNIX AND NOT APPLE)
    # Linux: Add library path
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Android")
        set_tests_properties(finkit_tests PROPERTIES
            ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib:${LD_LIBRARY_PATH}"
        )
    endif()
elseif(APPLE)
    # macOS: Add library path
    set_tests_properties(finkit_tests PROPERTIES
        ENVIRONMENT "DYLD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib:${DYLD_LIBRARY_PATH}"
    )
endif()

# Installation
install(TARGETS finkit_tests
    RUNTIME DESTINATION bin
)
