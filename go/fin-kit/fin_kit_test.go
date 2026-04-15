//go:build fin_kit_cgo

package fin_kit
import "unsafe"

import (
	"runtime"
	"testing"
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

func TestVersion(t *testing.T) {
	// Test version constants
	if VersionMajor != 1 {
		t.Errorf("VersionMajor = %d, want 1", VersionMajor)
	}
	if VersionMinor != 0 {
		t.Errorf("VersionMinor = %d, want 0", VersionMinor)
	}
	if VersionPatch != 0 {
		t.Errorf("VersionPatch = %d, want 0", VersionPatch)
	}
	if Version != "1.0.0" {
		t.Errorf("Version = %q, want %q", Version, "1.0.0")
	}
}

func TestSIMDDetect(t *testing.T) {
	// Initialize library first
	if err := Init(); err != nil {
		t.Fatalf("Init() failed: %v", err)
	}
	defer Cleanup()

	// Test SIMD detection
	level := DetectSIMD()
	t.Logf("Detected SIMD level: %s", level.String())

	// SIMD level should be valid
	if level < SIMDNone || level > SIMDAVX512 {
		t.Errorf("Invalid SIMD level: %d", level)
	}

	// String representation should not be empty
	str := level.String()
	if str == "" {
		t.Error("SIMD level string is empty")
	}
	t.Logf("SIMD level string: %s", str)
}

func TestSIMDParallelism(t *testing.T) {
	if err := Init(); err != nil {
		t.Fatalf("Init() failed: %v", err)
	}
	defer Cleanup()

	parallelism := Parallelism()
	t.Logf("SIMD parallelism: %d doubles/vector", parallelism)

	// Parallelism should be at least 1
	if parallelism < 1 {
		t.Errorf("Parallelism = %d, want >= 1", parallelism)
	}

	// Parallelism should be reasonable (1, 2, 4, or 8)
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

	// All CPUs should support at least scalar operations
	if !HasSIMD(SIMDNone) {
		t.Error("HasSIMD(SIMDNone) returned false")
	}

	// Log what SIMD levels are supported
	levels := []struct {
		level SIMDLevel
		name  string
	}{
		{SIMDNone, "Scalar"},
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

			// Verify size
			if len(data) != tt.size {
				t.Errorf("AlignedAlloc() returned slice of length %d, want %d", len(data), tt.size)
			}

			// Verify alignment (pointer should be divisible by alignment)
			if len(data) > 0 {
				ptr := uintptr(unsafe.Pointer(&data[0]))
				if ptr%uintptr(tt.alignment) != 0 {
					t.Errorf("AlignedAlloc() returned unaligned pointer: %x (alignment=%d)", ptr, tt.alignment)
				}
			}

			// Write to memory to ensure it's accessible
			for i := range data {
				data[i] = byte(i % 256)
			}

			// Explicitly free
			AlignedFree(data)
		})
	}
}

func TestAlignedAllocGC(t *testing.T) {
	if err := Init(); err != nil {
		t.Fatalf("Init() failed: %v", err)
	}
	defer Cleanup()

	// Allocate memory and let GC clean it up
	for i := 0; i < 100; i++ {
		data, err := AlignedAlloc(1024, 32)
		if err != nil {
			t.Fatalf("AlignedAlloc() failed: %v", err)
		}
		// Write to ensure allocation is real
		data[0] = 42
		// Don't call AlignedFree - let finalizer handle it
	}

	// Force GC to run finalizers
	runtime.GC()
	runtime.GC()
}

func TestPlatformInfo(t *testing.T) {
	if err := Init(); err != nil {
		t.Fatalf("Init() failed: %v", err)
	}
	defer Cleanup()

	// Test compiler info
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

	// Test OS info
	os := OS()
	if os == "" {
		t.Error("OS() returned empty string")
	}
	t.Logf("OS: %s", os)

	// Test architecture info
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
		{StatusInvalidPointer, ""},
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
	// Cleanup without Init should be safe (no-op)
	Cleanup()
	Cleanup() // Double cleanup should also be safe
}

func TestMultipleInitCleanup(t *testing.T) {
	// Test reference counting
	if err := Init(); err != nil {
		t.Fatalf("First Init() failed: %v", err)
	}
	if err := Init(); err != nil {
		t.Fatalf("Second Init() failed: %v", err)
	}
	if err := Init(); err != nil {
		t.Fatalf("Third Init() failed: %v", err)
	}

	// Should still be initialized after first cleanup
	Cleanup()
	if !IsInitialized() {
		t.Error("Library should still be initialized after first Cleanup()")
	}

	// Should still be initialized after second cleanup
	Cleanup()
	if !IsInitialized() {
		t.Error("Library should still be initialized after second Cleanup()")
	}

	// Should be cleaned up after third cleanup
	Cleanup()
	if IsInitialized() {
		t.Error("Library should be cleaned up after final Cleanup()")
	}
}

// Benchmark SIMD detection overhead
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

// Benchmark aligned allocation
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
