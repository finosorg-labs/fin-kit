//go:build lib

// Prebuilt mode: link against prebuilt static library.
// Usage: go build -tags lib
// Requires: prebuilt libfinkit.a in build/{platform}/lib/
package fin_kit

/*
#cgo CFLAGS: -I../../include
#cgo linux,amd64   LDFLAGS: -L../../build/linux_amd64/lib -lfinkit -lm
#cgo darwin,amd64  LDFLAGS: -L../../build/darwin_amd64/lib -lfinkit -lm
#cgo windows,amd64 LDFLAGS: -L../../build/windows_amd64/lib -lfinkit -lm

#include <fin-kit/platform/platform.h>
#include <fin-kit/platform/simd_detect.h>
#include <fin-kit/platform/mem_aligned.h>
#include <fin-kit/platform/error.h>
#include <fin-kit/matrix/matrix.h>
*/
import "C"
