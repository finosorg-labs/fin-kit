package matrix

import (
	"math"
	"testing"

	"fin-kit/platform"
)

const epsilon = 1e-9

func TestGEMM(t *testing.T) {
	if err := platform.Init(); err != nil {
		t.Fatalf("Init() failed: %v", err)
	}
	defer platform.Cleanup()

	// Test case: C = A * B
	// A: 2x3, B: 3x2, C: 2x2
	A := []float64{
		1, 2, 3,
		4, 5, 6,
	}
	B := []float64{
		7, 8,
		9, 10,
		11, 12,
	}
	C := make([]float64, 4)

	err := GEMM(2, 2, 3, 1.0, A, 3, B, 2, 0.0, C, 2)
	if err != nil {
		t.Fatalf("GEMM() failed: %v", err)
	}

	// Expected: C = [[58, 64], [139, 154]]
	expected := []float64{58, 64, 139, 154}
	for i := range C {
		if math.Abs(C[i]-expected[i]) > epsilon {
			t.Errorf("C[%d] = %f, want %f", i, C[i], expected[i])
		}
	}
}

func TestGEMV(t *testing.T) {
	if err := platform.Init(); err != nil {
		t.Fatalf("Init() failed: %v", err)
	}
	defer platform.Cleanup()

	// Test case: y = A * x
	// A: 2x3, x: 3x1, y: 2x1
	A := []float64{
		1, 2, 3,
		4, 5, 6,
	}
	x := []float64{1, 2, 3}
	y := make([]float64, 2)

	err := GEMV(2, 3, 1.0, A, 3, x, 0.0, y)
	if err != nil {
		t.Fatalf("GEMV() failed: %v", err)
	}

	// Expected: y = [14, 32]
	expected := []float64{14, 32}
	for i := range y {
		if math.Abs(y[i]-expected[i]) > epsilon {
			t.Errorf("y[%d] = %f, want %f", i, y[i], expected[i])
		}
	}
}

func TestTranspose(t *testing.T) {
	if err := platform.Init(); err != nil {
		t.Fatalf("Init() failed: %v", err)
	}
	defer platform.Cleanup()

	// Test case: B = A^T
	// A: 2x3, B: 3x2
	A := []float64{
		1, 2, 3,
		4, 5, 6,
	}
	B := make([]float64, 6)

	err := Transpose(2, 3, A, 3, B, 2)
	if err != nil {
		t.Fatalf("Transpose() failed: %v", err)
	}

	// Expected: B = [[1, 4], [2, 5], [3, 6]]
	expected := []float64{1, 4, 2, 5, 3, 6}
	for i := range B {
		if math.Abs(B[i]-expected[i]) > epsilon {
			t.Errorf("B[%d] = %f, want %f", i, B[i], expected[i])
		}
	}
}

func TestVecDot(t *testing.T) {
	if err := platform.Init(); err != nil {
		t.Fatalf("Init() failed: %v", err)
	}
	defer platform.Cleanup()

	x := []float64{1, 2, 3}
	y := []float64{4, 5, 6}

	result, err := VecDot(x, y)
	if err != nil {
		t.Fatalf("VecDot() failed: %v", err)
	}

	// Expected: 1*4 + 2*5 + 3*6 = 32
	expected := 32.0
	if math.Abs(result-expected) > epsilon {
		t.Errorf("VecDot() = %f, want %f", result, expected)
	}
}

func TestVecNormL2(t *testing.T) {
	if err := platform.Init(); err != nil {
		t.Fatalf("Init() failed: %v", err)
	}
	defer platform.Cleanup()

	x := []float64{3, 4}

	result, err := VecNormL2(x)
	if err != nil {
		t.Fatalf("VecNormL2() failed: %v", err)
	}

	// Expected: sqrt(3^2 + 4^2) = 5
	expected := 5.0
	if math.Abs(result-expected) > epsilon {
		t.Errorf("VecNormL2() = %f, want %f", result, expected)
	}
}

func TestVecNormL1(t *testing.T) {
	if err := platform.Init(); err != nil {
		t.Fatalf("Init() failed: %v", err)
	}
	defer platform.Cleanup()

	x := []float64{-3, 4, -5}

	result, err := VecNormL1(x)
	if err != nil {
		t.Fatalf("VecNormL1() failed: %v", err)
	}

	// Expected: |−3| + |4| + |−5| = 12
	expected := 12.0
	if math.Abs(result-expected) > epsilon {
		t.Errorf("VecNormL1() = %f, want %f", result, expected)
	}
}

func TestVecDistanceL2(t *testing.T) {
	if err := platform.Init(); err != nil {
		t.Fatalf("Init() failed: %v", err)
	}
	defer platform.Cleanup()

	x := []float64{1, 2, 3}
	y := []float64{4, 6, 8}

	result, err := VecDistanceL2(x, y)
	if err != nil {
		t.Fatalf("VecDistanceL2() failed: %v", err)
	}

	// Expected: sqrt((4-1)^2 + (6-2)^2 + (8-3)^2) = sqrt(9+16+25) = sqrt(50)
	expected := math.Sqrt(50)
	if math.Abs(result-expected) > epsilon {
		t.Errorf("VecDistanceL2() = %f, want %f", result, expected)
	}
}

func TestVecDistanceL1(t *testing.T) {
	if err := platform.Init(); err != nil {
		t.Fatalf("Init() failed: %v", err)
	}
	defer platform.Cleanup()

	x := []float64{1, 2, 3}
	y := []float64{4, 6, 8}

	result, err := VecDistanceL1(x, y)
	if err != nil {
		t.Fatalf("VecDistanceL1() failed: %v", err)
	}

	// Expected: |4-1| + |6-2| + |8-3| = 3 + 4 + 5 = 12
	expected := 12.0
	if math.Abs(result-expected) > epsilon {
		t.Errorf("VecDistanceL1() = %f, want %f", result, expected)
	}
}

func BenchmarkGEMM(b *testing.B) {
	if err := platform.Init(); err != nil {
		b.Fatalf("Init() failed: %v", err)
	}
	defer platform.Cleanup()

	sizes := []int{64, 128, 256, 512}
	for _, n := range sizes {
		b.Run(string(rune(n)), func(b *testing.B) {
			A := make([]float64, n*n)
			B := make([]float64, n*n)
			C := make([]float64, n*n)

			for i := range A {
				A[i] = float64(i)
				B[i] = float64(i)
			}

			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				_ = GEMM(int64(n), int64(n), int64(n), 1.0, A, int64(n), B, int64(n), 0.0, C, int64(n))
			}
		})
	}
}

func BenchmarkVecDot(b *testing.B) {
	if err := platform.Init(); err != nil {
		b.Fatalf("Init() failed: %v", err)
	}
	defer platform.Cleanup()

	sizes := []int{1000, 10000, 100000}
	for _, n := range sizes {
		b.Run(string(rune(n)), func(b *testing.B) {
			x := make([]float64, n)
			y := make([]float64, n)

			for i := range x {
				x[i] = float64(i)
				y[i] = float64(i)
			}

			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				_, _ = VecDot(x, y)
			}
		})
	}
}
