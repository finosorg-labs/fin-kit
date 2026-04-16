#
# include/CMakeLists.txt - Public header installation
#

# Copy public headers into build tree for #include <fin-kit/*.h>
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/fin-kit/
     DESTINATION ${CMAKE_BINARY_DIR}/include/fin-kit/
     FILES_MATCHING PATTERN "*.h")

# Install public headers
install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)
