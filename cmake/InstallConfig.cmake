# =============================================================================
# InstallConfig.cmake - Installation configuration
# =============================================================================

include(CMakePackageConfigHelpers)

# Generate version file from template
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/fin-kit/version.h"
    @ONLY
)

# Install CMake config files
install(
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DetectSIMD.cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompilerFlags.cmake"
    DESTINATION share/fin-kit/cmake
)

# Install version header
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/include/fin-kit/version.h"
    DESTINATION include/fin-kit
)

# Generate and install CMake export set
install(
    EXPORT fin-kit-targets
    FILE fin-kitTargets.cmake
    NAMESPACE fin-kit::
    DESTINATION lib/cmake/fin-kit
)
