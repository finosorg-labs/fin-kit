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

// SVD computes the Singular Value Decomposition: A = U * Sigma * V^T
// The matrix A (m×n) is destroyed on output
// s contains the singular values in descending order (length min(m,n))
// U contains the left singular vectors (m×m), can be nil if not needed
// VT contains the right singular vectors transposed (n×n), can be nil if not needed
func SVD(m, n int64, A []float64, lda int64, s []float64, U []float64, ldu int64, VT []float64, ldvt int64) error {
	minMN := m
	if n < m {
		minMN = n
	}

	if len(A) < int(m*lda) {
		return fmt.Errorf("matrix A too small: got %d, need %d", len(A), m*lda)
	}
	if len(s) < int(minMN) {
		return fmt.Errorf("singular values array too small: got %d, need %d", len(s), minMN)
	}

	var uPtr *C.double
	if U != nil {
		if len(U) < int(m*ldu) {
			return fmt.Errorf("matrix U too small: got %d, need %d", len(U), m*ldu)
		}
		uPtr = (*C.double)(unsafe.Pointer(&U[0]))
	}

	var vtPtr *C.double
	if VT != nil {
		if len(VT) < int(n*ldvt) {
			return fmt.Errorf("matrix VT too small: got %d, need %d", len(VT), n*ldvt)
		}
		vtPtr = (*C.double)(unsafe.Pointer(&VT[0]))
	}

	status := C.fc_mat_svd_f64(
		C.int64_t(m),
		C.int64_t(n),
		(*C.double)(unsafe.Pointer(&A[0])),
		C.int64_t(lda),
		(*C.double)(unsafe.Pointer(&s[0])),
		uPtr,
		C.int64_t(ldu),
		vtPtr,
		C.int64_t(ldvt))

	return checkStatus(status, "svd")
}

// EigenSymmetric computes the eigenvalue decomposition for symmetric matrices: A = Q * Lambda * Q^T
// Only the lower triangle of A is accessed, and A is destroyed on output
// w contains the eigenvalues in ascending order (length n)
// Q contains the eigenvectors (n×n), column i is the eigenvector for w[i]
func EigenSymmetric(n int64, A []float64, lda int64, w []float64, Q []float64, ldq int64) error {
	if len(A) < int(n*lda) {
		return fmt.Errorf("matrix A too small: got %d, need %d", len(A), n*lda)
	}
	if len(w) < int(n) {
		return fmt.Errorf("eigenvalues array too small: got %d, need %d", len(w), n)
	}
	if len(Q) < int(n*ldq) {
		return fmt.Errorf("matrix Q too small: got %d, need %d", len(Q), n*ldq)
	}

	status := C.fc_mat_eig_sym_f64(
		C.int64_t(n),
		(*C.double)(unsafe.Pointer(&A[0])),
		C.int64_t(lda),
		(*C.double)(unsafe.Pointer(&w[0])),
		(*C.double)(unsafe.Pointer(&Q[0])),
		C.int64_t(ldq))

	return checkStatus(status, "eig_sym")
}
