package matrix

/*
#cgo CFLAGS: -I../../include
#cgo linux LDFLAGS: -L../../build/linux_amd64/lib -lfinkit -lgcov -lm
#cgo darwin LDFLAGS: -L../../build/darwin_amd64/lib -lfinkit
#cgo windows LDFLAGS: -L../../build/windows_amd64/lib -lfinkit

#include <matrix/vector_ops.h>
*/
import "C"

import (
	"fmt"
	"unsafe"
)

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