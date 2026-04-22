package matrix

/*
#cgo CFLAGS: -I${SRCDIR}/../../include
#cgo LDFLAGS: -L${SRCDIR}/../../build/linux_amd64/lib -lfinkit -lm
#include <matrix/tridiag.h>
#include <stdlib.h>
*/
import "C"
import (
	"fmt"
)

// SolveTridiag solves a tridiagonal system using the Thomas algorithm.
// The system is: A*x = d, where A is tridiagonal with:
//   - lower: lower diagonal (length n-1), lower[0] is ignored
//   - diag: main diagonal (length n)
//   - upper: upper diagonal (length n-1), upper[n-1] is ignored
//   - d: right-hand side vector (length n)
//
// Returns the solution vector x.
// Time complexity: O(n)
func SolveTridiag(lower, diag, upper, d []float64) ([]float64, error) {
	n := len(diag)
	if n < 2 {
		return nil, fmt.Errorf("matrix dimension must be at least 2, got %d", n)
	}
	if len(lower) != n || len(upper) != n || len(d) != n {
		return nil, fmt.Errorf("array length mismatch: lower=%d, diag=%d, upper=%d, d=%d",
			len(lower), len(diag), len(upper), len(d))
	}

	x := make([]float64, n)

	ret := C.fc_mat_tridiag_solve_f64(
		C.int64_t(n),
		(*C.double)(&lower[0]),
		(*C.double)(&diag[0]),
		(*C.double)(&upper[0]),
		(*C.double)(&d[0]),
		(*C.double)(&x[0]),
	)

	if ret != 0 {
		return nil, fmt.Errorf("fc_mat_tridiag_solve_f64 failed with code %d", ret)
	}

	return x, nil
}

// SolveTridiagMulti solves a tridiagonal system with multiple right-hand sides.
// The system is: A*X = D, where A is tridiagonal (n×n) and D is n×nrhs.
//
// Parameters:
//   - lower: lower diagonal (length n-1)
//   - diag: main diagonal (length n)
//   - upper: upper diagonal (length n-1)
//   - D: right-hand side matrix (n×nrhs, row-major)
//   - nrhs: number of right-hand sides
//
// Returns the solution matrix X (n×nrhs, row-major).
func SolveTridiagMulti(lower, diag, upper []float64, D []float64, nrhs int) ([]float64, error) {
	n := len(diag)
	if n < 2 {
		return nil, fmt.Errorf("matrix dimension must be at least 2, got %d", n)
	}
	if nrhs < 1 {
		return nil, fmt.Errorf("number of RHS must be at least 1, got %d", nrhs)
	}
	if len(lower) != n || len(upper) != n {
		return nil, fmt.Errorf("array length mismatch: lower=%d, diag=%d, upper=%d",
			len(lower), len(diag), len(upper))
	}
	if len(D) != n*nrhs {
		return nil, fmt.Errorf("D length mismatch: expected %d, got %d", n*nrhs, len(D))
	}

	X := make([]float64, n*nrhs)

	ret := C.fc_mat_tridiag_solve_multi_f64(
		C.int64_t(n),
		C.int64_t(nrhs),
		(*C.double)(&lower[0]),
		(*C.double)(&diag[0]),
		(*C.double)(&upper[0]),
		(*C.double)(&D[0]),
		C.int64_t(nrhs),
		(*C.double)(&X[0]),
		C.int64_t(nrhs),
	)

	if ret != 0 {
		return nil, fmt.Errorf("fc_mat_tridiag_solve_multi_f64 failed with code %d", ret)
	}

	return X, nil
}
