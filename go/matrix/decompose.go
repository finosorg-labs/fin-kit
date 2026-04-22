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

// LUDecompose computes the LU decomposition with partial pivoting: A = P*L*U
// The matrix A is overwritten with L and U factors (L in lower triangle with unit diagonal, U in upper triangle)
// ipiv contains the pivot indices (row i was interchanged with row ipiv[i])
func LUDecompose(n int64, A []float64, lda int64, ipiv []int64) error {
	if len(A) < int(n*lda) {
		return fmt.Errorf("matrix A too small: got %d, need %d", len(A), n*lda)
	}
	if len(ipiv) < int(n) {
		return fmt.Errorf("pivot array too small: got %d, need %d", len(ipiv), n)
	}

	status := C.fc_mat_lu_decompose_f64(
		C.int64_t(n),
		(*C.double)(unsafe.Pointer(&A[0])),
		C.int64_t(lda),
		(*C.int64_t)(unsafe.Pointer(&ipiv[0])))

	return checkStatus(status, "lu_decompose")
}

// QRDecompose computes the QR decomposition using Householder reflections: A = Q*R
// The matrix A (m×n) is overwritten with R in the upper triangle and Householder vectors in the lower triangle
// tau contains the scaling factors for the Householder reflectors
func QRDecompose(m, n int64, A []float64, lda int64, tau []float64) error {
	if m < n {
		return fmt.Errorf("matrix must be tall or square: got m=%d, n=%d", m, n)
	}
	if len(A) < int(m*lda) {
		return fmt.Errorf("matrix A too small: got %d, need %d", len(A), m*lda)
	}
	if len(tau) < int(n) {
		return fmt.Errorf("tau array too small: got %d, need %d", len(tau), n)
	}

	status := C.fc_mat_qr_decompose_f64(
		C.int64_t(m),
		C.int64_t(n),
		(*C.double)(unsafe.Pointer(&A[0])),
		C.int64_t(lda),
		(*C.double)(unsafe.Pointer(&tau[0])))

	return checkStatus(status, "qr_decompose")
}

// CholeskyDecompose computes the Cholesky decomposition for symmetric positive definite matrices: A = L*L^T
// Only the lower triangle of A is accessed and overwritten with L
func CholeskyDecompose(n int64, A []float64, lda int64) error {
	if len(A) < int(n*lda) {
		return fmt.Errorf("matrix A too small: got %d, need %d", len(A), n*lda)
	}

	status := C.fc_mat_cholesky_decompose_f64(
		C.int64_t(n),
		(*C.double)(unsafe.Pointer(&A[0])),
		C.int64_t(lda))

	return checkStatus(status, "cholesky_decompose")
}
