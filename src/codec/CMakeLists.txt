#
# src/codec/CMakeLists.txt - Codec module (FIX protocol, JSON, CSV)
#

# Codec module sources
set(CODEC_SOURCES
    fix_codec.c
)

# Create object library for codec module
add_library(codec_obj OBJECT ${CODEC_SOURCES})

target_include_directories(codec_obj PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/core/platform/include
)

# Enable internal tests if building tests
if(FC_BUILD_TESTS)
    target_compile_definitions(codec_obj PRIVATE FC_ENABLE_INTERNAL_TESTS)
endif()

# Set compile flags for SIMD variants in fix_codec.c
# The file contains multiple SIMD implementations with function-level target attributes
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    # Enable SSE4.2 and AVX2 for the entire file (function attributes will control actual usage)
    set_source_files_properties(fix_codec.c PROPERTIES COMPILE_FLAGS "-msse4.2 -mavx2")
elseif(MSVC)
    set_source_files_properties(fix_codec.c PROPERTIES COMPILE_FLAGS "/arch:AVX2")
endif()
