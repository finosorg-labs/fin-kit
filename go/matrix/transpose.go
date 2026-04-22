package matrix

/*
#cgo CFLAGS: -I../../include
#cgo linux LDFLAGS: -L../../build/linux_amd64/lib -lfinkit -lgcov -lm
#cgo darwin LDFLAGS: -L../../build/darwin_amd64/lib -lfinkit
#cgo windows LDFLAGS: -L../../build/windows_amd64/lib -lfinkit

#include <matrix/transpose.h>
*/
import "C"

import (
	"fmt"
	"unsafe"
)

// Transpose performs matrix transposition: dst = src^T
// src is rows×cols, dst is cols×rows
func Transpose(rows, cols int64, src []float64, ldSrc int64, dst []float64, ldDst int64) error {
	if len(src) < int(rows*ldSrc) {
		return fmt.Errorf("source matrix too small: got %d, need %d", len(src), rows*ldSrc)
	}
	if len(dst) < int(cols*ldDst) {
		return fmt.Errorf("destination matrix too small: got %d, need %d", len(dst), cols*ldDst)
	}

	status := C.fc_mat_transpose_f64(
		C.int64_t(rows), C.int64_t(cols),
		(*C.double)(unsafe.Pointer(&src[0])), C.int64_t(ldSrc),
		(*C.double)(unsafe.Pointer(&dst[0])), C.int64_t(ldDst))

	return checkStatus(status, "transpose")
}