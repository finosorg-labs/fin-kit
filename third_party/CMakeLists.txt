# =============================================================================
# third_party/CMakeLists.txt - Third-party library integration
# =============================================================================
# Integrates lz4, zstd, and xxHash via their C source files.
# All third-party libraries are BSD-licensed.
# =============================================================================

# ---------------------------------------------------------------------------
# lz4 (BSD-2-Clause) - https://lz4.github.io/lz4/
# Source files are directly under third_party/lz4/
# ---------------------------------------------------------------------------
add_library(finkit_3rdparty_lz4 STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/lz4/lz4.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lz4/lz4hc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lz4/lz4frame.c
)

target_include_directories(finkit_3rdparty_lz4
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lz4
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/xxhash
)

target_link_libraries(finkit_3rdparty_lz4 PRIVATE finkit_3rdparty_xxhash)

# Suppress lz4 compiler warnings
target_compile_options(finkit_3rdparty_lz4 PRIVATE -w)

# ---------------------------------------------------------------------------
# xxHash (BSD-2-Clause) - https://cyan4973.github.io/xxHash/
# ---------------------------------------------------------------------------
add_library(finkit_3rdparty_xxhash STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/xxhash/xxhash.c
)

target_include_directories(finkit_3rdparty_xxhash
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/xxhash
)

target_compile_options(finkit_3rdparty_xxhash PRIVATE -w)

# ---------------------------------------------------------------------------
# zstd (BSD-3-Clause) - https://facebook.github.io/zstd/
# Source files are under third_party/zstd/{common,compress,decompress}/
# ---------------------------------------------------------------------------
add_library(finkit_3rdparty_zstd STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/zstd/common/entropy_common.c
    ${CMAKE_CURRENT_SOURCE_DIR}/zstd/common/fse_decompress.c
    ${CMAKE_CURRENT_SOURCE_DIR}/zstd/common/xxhash.c
    ${CMAKE_CURRENT_SOURCE_DIR}/zstd/common/zstd_common.c
    ${CMAKE_CURRENT_SOURCE_DIR}/zstd/common/pool.c
    ${CMAKE_CURRENT_SOURCE_DIR}/zstd/common/threading.c
    ${CMAKE_CURRENT_SOURCE_DIR}/zstd/common/debug.c
    ${CMAKE_CURRENT_SOURCE_DIR}/zstd/common/error_private.c
    ${CMAKE_CURRENT_SOURCE_DIR}/zstd/compress/fse_compress.c
    ${CMAKE_CURRENT_SOURCE_DIR}/zstd/compress/hist.c
    ${CMAKE_CURRENT_SOURCE_DIR}/zstd/compress/huf_compress.c
    ${CMAKE_CURRENT_SOURCE_DIR}/zstd/compress/zstd_compress.c
    ${CMAKE_CURRENT_SOURCE_DIR}/zstd/compress/zstd_compress_sequences.c
    ${CMAKE_CURRENT_SOURCE_DIR}/zstd/compress/zstd_compress_literals.c
    ${CMAKE_CURRENT_SOURCE_DIR}/zstd/compress/zstd_compress_superblock.c
    ${CMAKE_CURRENT_SOURCE_DIR}/zstd/compress/zstd_cwksp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/zstd/compress/zstd_double_fast.c
    ${CMAKE_CURRENT_SOURCE_DIR}/zstd/compress/zstd_fast.c
    ${CMAKE_CURRENT_SOURCE_DIR}/zstd/compress/zstd_lazy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/zstd/compress/zstd_ldm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/zstd/compress/zstd_opt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/zstd/compress/zstdmt_compress.c
    ${CMAKE_CURRENT_SOURCE_DIR}/zstd/compress/zstd_preSplit.c
    ${CMAKE_CURRENT_SOURCE_DIR}/zstd/compress/clevels.h
    ${CMAKE_CURRENT_SOURCE_DIR}/zstd/compress/zstd_compress_internal.h
    ${CMAKE_CURRENT_SOURCE_DIR}/zstd/decompress/huf_decompress.c
    ${CMAKE_CURRENT_SOURCE_DIR}/zstd/decompress/zstd_decompress.c
    ${CMAKE_CURRENT_SOURCE_DIR}/zstd/decompress/zstd_decompress_block.c
    ${CMAKE_CURRENT_SOURCE_DIR}/zstd/decompress/zstd_ddict.c
)

target_include_directories(finkit_3rdparty_zstd
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/zstd
        ${CMAKE_CURRENT_SOURCE_DIR}/zstd/common
        ${CMAKE_CURRENT_SOURCE_DIR}/zstd/compress
        ${CMAKE_CURRENT_SOURCE_DIR}/zstd/decompress
)

target_compile_options(finkit_3rdparty_zstd PRIVATE -w)

# ---------------------------------------------------------------------------
# Combined third-party library for easy linking
# ---------------------------------------------------------------------------
add_library(finkit_3rdparty INTERFACE)
add_library(finkit::3rdparty ALIAS finkit_3rdparty)

target_link_libraries(finkit_3rdparty INTERFACE
    finkit_3rdparty_lz4
    finkit_3rdparty_xxhash
    finkit_3rdparty_zstd
)
