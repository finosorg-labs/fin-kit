package matrix

/*
#cgo CFLAGS: -I../../include
#cgo linux LDFLAGS: -L../../build/linux_amd64/lib -lfinkit -lgcov -lm
#cgo darwin LDFLAGS: -L../../build/darwin_amd64/lib -lfinkit
#cgo windows LDFLAGS: -L../../build/windows_amd64/lib -lfinkit

#include <matrix/gemv.h>
*/
import "C"

import (
	"fmt"
	"unsafe"
)

// GEMV performs matrix-vector multiplication: y = alpha*A*x + beta*y
// A is m×n, x is n×1, y is m×1
func GEMV(m, n int64, alpha float64, A []float64, lda int64, x []float64, beta float64, y []float64) error {
	if len(A) < int(m*lda) {
		return fmt.Errorf("matrix A too small: got %d, need %d", len(A), m*lda)
	}
	if len(x) < int(n) {
		return fmt.Errorf("vector x too small: got %d, need %d", len(x), n)
	}
	if len(y) < int(m) {
		return fmt.Errorf("vector y too small: got %d, need %d", len(y), m)
	}

	status := C.fc_mat_gemv_f64(
		C.int64_t(m), C.int64_t(n),
		C.double(alpha),
		(*C.double)(unsafe.Pointer(&A[0])), C.int64_t(lda),
		(*C.double)(unsafe.Pointer(&x[0])),
		C.double(beta),
		(*C.double)(unsafe.Pointer(&y[0])))

	return checkStatus(status, "gemv")
}