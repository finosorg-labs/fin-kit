//go:build !fin_kit_cgo

// Prebuilt mode: link against prebuilt static library.
// Users need only the Go toolchain.
// The library path should be provided via CGO_LDFLAGS environment variable, e.g.:
//
//	CGO_LDFLAGS="-L/path/to/lib -lfinkit" go build
//
// Or set it in the shell before building.
package fin_kit

/*
#cgo CFLAGS: -I${SRCDIR}/.. -I${SRCDIR}/../../include
#cgo LDFLAGS: -L${SRCDIR}/../../build/windows_amd64/lib -lfinkit -lwinpthread

#include "fin_kit.h"
*/
import "C"
