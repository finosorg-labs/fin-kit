package matrix

/*
#cgo CFLAGS: -I../../include
#cgo linux LDFLAGS: -L../../build/linux_amd64/lib -lfinkit -lgcov -lm
#cgo darwin LDFLAGS: -L../../build/darwin_amd64/lib -lfinkit
#cgo windows LDFLAGS: -L../../build/windows_amd64/lib -lfinkit

#include <matrix/gemm.h>
*/
import "C"

import (
	"fmt"
	"unsafe"
)

// GEMM performs general matrix multiplication: C = alpha*A*B + beta*C
// A is m×k, B is k×n, C is m×n
func GEMM(m, n, k int64, alpha float64, A []float64, lda int64, B []float64, ldb int64, beta float64, C []float64, ldc int64) error {
	if len(A) < int(m*lda) {
		return fmt.Errorf("matrix A too small: got %d, need %d", len(A), m*lda)
	}
	if len(B) < int(k*ldb) {
		return fmt.Errorf("matrix B too small: got %d, need %d", len(B), k*ldb)
	}
	if len(C) < int(m*ldc) {
		return fmt.Errorf("matrix C too small: got %d, need %d", len(C), m*ldc)
	}

	status := C.fc_mat_gemm_f64(
		C.int64_t(m), C.int64_t(n), C.int64_t(k),
		C.double(alpha),
		(*C.double)(unsafe.Pointer(&A[0])), C.int64_t(lda),
		(*C.double)(unsafe.Pointer(&B[0])), C.int64_t(ldb),
		C.double(beta),
		(*C.double)(unsafe.Pointer(&C[0])), C.int64_t(ldc))

	return checkStatus(status, "gemm")
}