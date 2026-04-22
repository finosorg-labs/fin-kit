package matrix

/*
#cgo CFLAGS: -I../../include
#cgo linux LDFLAGS: -L../../build/linux_amd64/lib -lfinkit -lgcov -lm
#cgo darwin LDFLAGS: -L../../build/darwin_amd64/lib -lfinkit
#cgo windows LDFLAGS: -L../../build/windows_amd64/lib -lfinkit

#include <matrix/matrix.h>
*/
import "C"

import (
	"fmt"
	"unsafe"
)

// SolveLinear solves the linear system A*X = B using LU decomposition
// A is n×n coefficient matrix (overwritten with LU factors)
// B is n×nrhs right-hand side matrix (overwritten with solution X)
func SolveLinear(n, nrhs int64, A []float64, lda int64, B []float64, ldb int64) error {
	if len(A) < int(n*lda) {
		return fmt.Errorf("matrix A too small: got %d, need %d", len(A), n*lda)
	}
	if len(B) < int(n*ldb) {
		return fmt.Errorf("matrix B too small: got %d, need %d", len(B), n*ldb)
	}

	status := C.fc_mat_solve_linear_f64(
		C.int64_t(n),
		C.int64_t(nrhs),
		(*C.double)(unsafe.Pointer(&A[0])),
		C.int64_t(lda),
		(*C.double)(unsafe.Pointer(&B[0])),
		C.int64_t(ldb))

	return checkStatus(status, "solve_linear")
}

// Inverse computes the matrix inverse using LU decomposition: A_inv = A^(-1)
// The matrix A is overwritten with its inverse
func Inverse(n int64, A []float64, lda int64) error {
	if len(A) < int(n*lda) {
		return fmt.Errorf("matrix A too small: got %d, need %d", len(A), n*lda)
	}

	status := C.fc_mat_inverse_f64(
		C.int64_t(n),
		(*C.double)(unsafe.Pointer(&A[0])),
		C.int64_t(lda))

	return checkStatus(status, "inverse")
}
