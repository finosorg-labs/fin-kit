// Package matrix provides high-performance matrix operations with SIMD optimization.
package matrix

/*
#cgo CFLAGS: -I../../include
#cgo linux LDFLAGS: -L../../build/linux_amd64/lib -lfinkit -lgcov -lm
#cgo darwin LDFLAGS: -L../../build/darwin_amd64/lib -lfinkit
#cgo windows LDFLAGS: -L../../build/windows_amd64/lib -lfinkit

#include <fin-kit/matrix/matrix.h>
*/
import "C"

import (
	"fmt"
	"unsafe"
)

// checkStatus converts C status code to Go error
func checkStatus(status C.int, operation string) error {
	if status == 0 {
		return nil
	}
	return fmt.Errorf("%s failed with status %d", operation, int(status))
}

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

// VecDot computes the dot product of two vectors
func VecDot(x, y []float64) (float64, error) {
	n := len(x)
	if n != len(y) {
		return 0, fmt.Errorf("vectors must have same length: got %d and %d", n, len(y))
	}
	if n == 0 {
		return 0, fmt.Errorf("vectors cannot be empty")
	}

	var result C.double
	status := C.fc_vec_dot_f64(
		(*C.double)(unsafe.Pointer(&x[0])),
		(*C.double)(unsafe.Pointer(&y[0])),
		C.int64_t(n),
		&result)

	if err := checkStatus(status, "dot product"); err != nil {
		return 0, err
	}
	return float64(result), nil
}

// VecNormL2 computes the L2 norm (Euclidean norm) of a vector
func VecNormL2(x []float64) (float64, error) {
	if len(x) == 0 {
		return 0, fmt.Errorf("vector cannot be empty")
	}

	var result C.double
	status := C.fc_vec_norm_l2_f64(
		(*C.double)(unsafe.Pointer(&x[0])),
		C.int64_t(len(x)),
		&result)

	if err := checkStatus(status, "L2 norm"); err != nil {
		return 0, err
	}
	return float64(result), nil
}

// VecNormL1 computes the L1 norm (Manhattan norm) of a vector
func VecNormL1(x []float64) (float64, error) {
	if len(x) == 0 {
		return 0, fmt.Errorf("vector cannot be empty")
	}

	var result C.double
	status := C.fc_vec_norm_l1_f64(
		(*C.double)(unsafe.Pointer(&x[0])),
		C.int64_t(len(x)),
		&result)

	if err := checkStatus(status, "L1 norm"); err != nil {
		return 0, err
	}
	return float64(result), nil
}

// VecDistanceL2 computes the L2 distance between two vectors
func VecDistanceL2(x, y []float64) (float64, error) {
	n := len(x)
	if n != len(y) {
		return 0, fmt.Errorf("vectors must have same length: got %d and %d", n, len(y))
	}
	if n == 0 {
		return 0, fmt.Errorf("vectors cannot be empty")
	}

	var result C.double
	status := C.fc_vec_distance_l2_f64(
		(*C.double)(unsafe.Pointer(&x[0])),
		(*C.double)(unsafe.Pointer(&y[0])),
		C.int64_t(n),
		&result)

	if err := checkStatus(status, "L2 distance"); err != nil {
		return 0, err
	}
	return float64(result), nil
}

// VecDistanceL1 computes the L1 distance between two vectors
func VecDistanceL1(x, y []float64) (float64, error) {
	n := len(x)
	if n != len(y) {
		return 0, fmt.Errorf("vectors must have same length: got %d and %d", n, len(y))
	}
	if n == 0 {
		return 0, fmt.Errorf("vectors cannot be empty")
	}

	var result C.double
	status := C.fc_vec_distance_l1_f64(
		(*C.double)(unsafe.Pointer(&x[0])),
		(*C.double)(unsafe.Pointer(&y[0])),
		C.int64_t(n),
		&result)

	if err := checkStatus(status, "L1 distance"); err != nil {
		return 0, err
	}
	return float64(result), nil
}
