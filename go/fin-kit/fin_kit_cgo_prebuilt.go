//go:build !fin_kit_cgo

// Prebuilt mode: link against prebuilt static library.
// Users need only the Go toolchain.
package fin_kit

/*
#cgo LDFLAGS: ${SRCDIR}/lib/libfinkit.a
*/
import "C"
