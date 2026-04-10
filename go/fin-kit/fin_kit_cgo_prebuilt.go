//go:build !fin_kit_cgo

// Prebuilt mode: link against prebuilt static library.
// Users need only the Go toolchain.
package fin_kit

/*
#cgo LDFLAGS: ${SRCDIR}/lib/libfinkit.a \
    ${SRCDIR}/lib/libfinkit_platform_static.a \
    ${SRCDIR}/lib/libfinkit_3rdparty_lz4.a \
    ${SRCDIR}/lib/libfinkit_3rdparty_xxhash.a \
    ${SRCDIR}/lib/libfinkit_3rdparty_zstd.a \
    -lm -lpthread
*/
import "C"
