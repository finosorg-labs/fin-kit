package platform

import (
	"runtime"
	"testing"
	"unsafe"
)

func TestInit(t *testing.T) {
	// Test basic initialization
	err := Init()
	if err != nil {
		t.Fatalf("Init() failed: %v", err)
	}
	defer Cleanup()

	// Verify library is initialized
	if !IsInitialized() {
		t.Error("IsInitialized() returned false after Init()")
	}

	// Test double initialization (should be safe)
	err = Init()
	if err != nil {
		t.Errorf("Second Init() failed: %v", err)
	}
}

func TestInitWithConfig(t *testing.T) {
	cfg := Config{
		NumThreads: 4,
		EnableAVX2: false,
		Verbose:    false,
	}

	err := InitWithConfig(cfg)
	if err != nil {
		t.Fatalf("InitWithConfig() failed: %v", err)
	}
	defer Cleanup()

	if !IsInitialized() {
		t.Error("IsInitialized() returned false after InitWithConfig()")
	}
}

func TestSIMDDetect(t *testing.T) {
	if err := Init(); err != nil {
		t.Fatalf("Init() failed: %v", err)
	}
	defer Cleanup()

	level := DetectSIMD()
	t.Logf("Detected SIMD level: %s", level.String())

	if level < SIMDScalar || level > SIMDAVX512 {
		t.Errorf("Invalid SIMD level: %d", level)
	}

	str := level.String()
	if str == "" {
		t.Error("SIMD level string is empty")
	}
}

func TestSIMDParallelism(t *testing.T) {
	if err := Init(); err != nil {
		t.Fatalf("Init() failed: %v", err)
	}
	defer Cleanup()

	parallelism := Parallelism()
	t.Logf("SIMD parallelism: %d doubles/vector", parallelism)

	if parallelism < 1 {
		t.Errorf("Parallelism = %d, want >= 1", parallelism)
	}

	validValues := []int{1, 2, 4, 8}
	valid := false
	for _, v := range validValues {
		if parallelism == v {
			valid = true
			break
		}
	}
	if !valid {
		t.Errorf("Parallelism = %d, want one of %v", parallelism, validValues)
	}
}

func TestHasSIMD(t *testing.T) {
	if err := Init(); err != nil {
		t.Fatalf("Init() failed: %v", err)
	}
	defer Cleanup()

	if !HasSIMD(SIMDScalar) {
		t.Error("HasSIMD(SIMDScalar) returned false")
	}

	levels := []struct {
		level SIMDLevel
		name  string
	}{
		{SIMDScalar, "Scalar"},
		{SIMDSSE42, "SSE4.2"},
		{SIMDAVX2, "AVX2"},
		{SIMDAVX512, "AVX-512"},
	}

	for _, l := range levels {
		supported := HasSIMD(l.level)
		t.Logf("%s support: %v", l.name, supported)
	}
}

func TestAlignedAlloc(t *testing.T) {
	if err := Init(); err != nil {
		t.Fatalf("Init() failed: %v", err)
	}
	defer Cleanup()

	tests := []struct {
		name      string
		size      int
		alignment int
		wantErr   bool
	}{
		{"16-byte aligned", 1024, 16, false},
		{"32-byte aligned", 2048, 32, false},
		{"64-byte aligned", 4096, 64, false},
		{"invalid size", 0, 16, true},
		{"invalid alignment", 1024, 3, true},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			data, err := AlignedAlloc(tt.size, tt.alignment)
			if (err != nil) != tt.wantErr {
				t.Errorf("AlignedAlloc() error = %v, wantErr %v", err, tt.wantErr)
				return
			}
			if err != nil {
				return
			}

			if len(data) != tt.size {
				t.Errorf("AlignedAlloc() returned slice of length %d, want %d", len(data), tt.size)
			}

			if len(data) > 0 {
				ptr := uintptr(unsafe.Pointer(&data[0]))
				if ptr%uintptr(tt.alignment) != 0 {
					t.Errorf("AlignedAlloc() returned unaligned pointer: %x (alignment=%d)", ptr, tt.alignment)
				}
			}

			for i := range data {
				data[i] = byte(i % 256)
			}

			AlignedFree(data)
		})
	}
}

func TestAlignedAllocGC(t *testing.T) {
	if err := Init(); err != nil {
		t.Fatalf("Init() failed: %v", err)
	}
	defer Cleanup()

	for i := 0; i < 100; i++ {
		data, err := AlignedAlloc(1024, 32)
		if err != nil {
			t.Fatalf("AlignedAlloc() failed: %v", err)
		}
		data[0] = 42
	}

	runtime.GC()
	runtime.GC()
}

func TestInfo(t *testing.T) {
	if err := Init(); err != nil {
		t.Fatalf("Init() failed: %v", err)
	}
	defer Cleanup()

	compiler := Compiler()
	if compiler == "" {
		t.Error("Compiler() returned empty string")
	}
	t.Logf("Compiler: %s", compiler)

	compilerVer := CompilerVersion()
	if compilerVer == "" {
		t.Error("CompilerVersion() returned empty string")
	}
	t.Logf("Compiler version: %s", compilerVer)

	os := OS()
	if os == "" {
		t.Error("OS() returned empty string")
	}
	t.Logf("OS: %s", os)

	arch := Arch()
	if arch == "" {
		t.Error("Arch() returned empty string")
	}
	t.Logf("Architecture: %s", arch)
}

func TestStatus(t *testing.T) {
	tests := []struct {
		status Status
		want   string
	}{
		{StatusOK, "Success"},
		{StatusError, ""},
		{StatusInvalidArg, ""},
	}

	for _, tt := range tests {
		str := tt.status.String()
		if str == "" {
			t.Errorf("Status(%d).String() returned empty string", tt.status)
		}
		t.Logf("Status(%d) = %q", tt.status, str)
	}
}

func TestCleanupWithoutInit(t *testing.T) {
	Cleanup()
	Cleanup()
}

func TestMultipleInitCleanup(t *testing.T) {
	if err := Init(); err != nil {
		t.Fatalf("First Init() failed: %v", err)
	}
	if err := Init(); err != nil {
		t.Fatalf("Second Init() failed: %v", err)
	}
	if err := Init(); err != nil {
		t.Fatalf("Third Init() failed: %v", err)
	}

	Cleanup()
	if !IsInitialized() {
		t.Error("Library should still be initialized after first Cleanup()")
	}

	Cleanup()
	if !IsInitialized() {
		t.Error("Library should still be initialized after second Cleanup()")
	}

	Cleanup()
	if IsInitialized() {
		t.Error("Library should be cleaned up after final Cleanup()")
	}
}

func BenchmarkSIMDDetect(b *testing.B) {
	if err := Init(); err != nil {
		b.Fatalf("Init() failed: %v", err)
	}
	defer Cleanup()

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_ = DetectSIMD()
	}
}

func BenchmarkAlignedAlloc(b *testing.B) {
	if err := Init(); err != nil {
		b.Fatalf("Init() failed: %v", err)
	}
	defer Cleanup()

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		data, err := AlignedAlloc(4096, 64)
		if err != nil {
			b.Fatalf("AlignedAlloc() failed: %v", err)
		}
		AlignedFree(data)
	}
}
