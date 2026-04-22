package matrix

import (
	"math"
	"testing"

	"fin-kit/platform"
)

func TestLUDecompose(t *testing.T) {
	if err := platform.Init(); err != nil {
		t.Fatalf("Init() failed: %v", err)
	}
	defer platform.Cleanup()

	// Test 3x3 matrix
	A := []float64{
		2, 1, 1,
		4, 3, 3,
		8, 7, 9,
	}
	ipiv := make([]int64, 3)

	err := LUDecompose(3, A, 3, ipiv)
	if err != nil {
		t.Fatalf("LUDecompose() failed: %v", err)
	}

	// Check that diagonal elements are non-zero
	if math.Abs(A[0]) < epsilon {
		t.Errorf("A[0,0] should be non-zero")
	}
	if math.Abs(A[4]) < epsilon {
		t.Errorf("A[1,1] should be non-zero")
	}
	if math.Abs(A[8]) < epsilon {
		t.Errorf("A[2,2] should be non-zero")
	}
}

func TestQRDecompose(t *testing.T) {
	if err := platform.Init(); err != nil {
		t.Fatalf("Init() failed: %v", err)
	}
	defer platform.Cleanup()

	// Test 3x3 matrix
	A := []float64{
		12, -51, 4,
		6, 167, -68,
		-4, 24, -41,
	}
	tau := make([]float64, 3)

	err := QRDecompose(3, 3, A, 3, tau)
	if err != nil {
		t.Fatalf("QRDecompose() failed: %v", err)
	}

	// Check that R diagonal elements are non-zero
	if math.Abs(A[0]) < epsilon {
		t.Errorf("R[0,0] should be non-zero")
	}
	if math.Abs(A[4]) < epsilon {
		t.Errorf("R[1,1] should be non-zero")
	}
	if math.Abs(A[8]) < epsilon {
		t.Errorf("R[2,2] should be non-zero")
	}
}

func TestCholeskyDecompose(t *testing.T) {
	if err := platform.Init(); err != nil {
		t.Fatalf("Init() failed: %v", err)
	}
	defer platform.Cleanup()

	// Test symmetric positive definite matrix
	A := []float64{
		4, 12, -16,
		12, 37, -43,
		-16, -43, 98,
	}

	err := CholeskyDecompose(3, A, 3)
	if err != nil {
		t.Fatalf("CholeskyDecompose() failed: %v", err)
	}

	// Check that L diagonal elements are positive
	if A[0] <= 0 {
		t.Errorf("L[0,0] should be positive, got %f", A[0])
	}
	if A[4] <= 0 {
		t.Errorf("L[1,1] should be positive, got %f", A[4])
	}
	if A[8] <= 0 {
		t.Errorf("L[2,2] should be positive, got %f", A[8])
	}
}
