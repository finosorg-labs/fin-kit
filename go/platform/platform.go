// Package platform provides platform detection, SIMD capabilities, and memory management.
package platform

/*
#cgo CFLAGS: -I../../include
#cgo linux LDFLAGS: -L../../build/linux_amd64/lib -lfinkit -lgcov -lm
#cgo darwin LDFLAGS: -L../../build/darwin_amd64/lib -lfinkit
#cgo windows LDFLAGS: -L../../build/windows_amd64/lib -lfinkit

#include <platform/platform.h>
#include <platform/simd_detect.h>
#include <platform/mem_aligned.h>
#include <platform/error.h>

// Forward declarations for fc_init/fc_cleanup (no public header)
int fc_init(void);
void fc_cleanup(void);
*/
import "C"

import (
	"fmt"
	"runtime"
	"sync"
	"unsafe"
)

// Track allocated pointers to prevent double-free
var allocTracker = struct {
	sync.Mutex
	ptrs map[uintptr]bool
}{
	ptrs: make(map[uintptr]bool),
}

// Config holds library configuration options
type Config struct {
	NumThreads    int
	SIMDLevel     string
	EnableAVX2    bool
	MemoryLimitMB uint64
	Verbose       bool
}

// DefaultConfig returns default configuration
func DefaultConfig() Config {
	return Config{
		NumThreads:    runtime.NumCPU(),
		SIMDLevel:     "auto",
		EnableAVX2:    true,
		MemoryLimitMB: 0,
		Verbose:       false,
	}
}

type libState struct {
	refCount int
	mu       sync.Mutex
}

var state = &libState{}

// Init initializes the library with default configuration
func Init() error {
	return InitWithConfig(DefaultConfig())
}

// InitWithConfig initializes the library with custom configuration
func InitWithConfig(cfg Config) error {
	state.mu.Lock()
	defer state.mu.Unlock()

	// Allow multiple Init calls (reference counting)
	if state.refCount > 0 {
		state.refCount++
		return nil
	}

	status := C.fc_init()
	if status != C.FC_OK {
		return fmt.Errorf("initialization failed with status %d", status)
	}

	state.refCount = 1
	return nil
}

// Cleanup releases library resources
func Cleanup() {
	state.mu.Lock()
	defer state.mu.Unlock()

	if state.refCount <= 0 {
		return
	}

	state.refCount--
	if state.refCount == 0 {
		C.fc_cleanup()
	}
}

// IsInitialized returns whether the library is initialized
func IsInitialized() bool {
	state.mu.Lock()
	defer state.mu.Unlock()
	return state.refCount > 0
}

// SIMDLevel represents SIMD instruction set level
type SIMDLevel C.fc_simd_level_t

const (
	SIMDScalar SIMDLevel = C.FC_SIMD_SCALAR
	SIMDSSE42  SIMDLevel = C.FC_SIMD_SSE42
	SIMDAVX2   SIMDLevel = C.FC_SIMD_AVX2
	SIMDAVX512 SIMDLevel = C.FC_SIMD_AVX512
	SIMDNEON   SIMDLevel = C.FC_SIMD_NEON
)

func (l SIMDLevel) String() string {
	return C.GoString(C.fc_simd_level_string(C.fc_simd_level_t(l)))
}

// DetectSIMD detects the highest SIMD level supported by the CPU
func DetectSIMD() SIMDLevel {
	return SIMDLevel(C.fc_detect_simd())
}

// HasSIMD checks if the CPU supports at least the specified SIMD level
func HasSIMD(minLevel SIMDLevel) bool {
	detected := DetectSIMD()
	return detected >= minLevel
}

// Parallelism returns the number of doubles per SIMD vector
func Parallelism() int {
	level := DetectSIMD()
	return int(C.fc_simd_parallelism(C.fc_simd_level_t(level)))
}

// Status represents operation status codes
type Status int

const (
	StatusOK                Status = C.FC_OK
	StatusInvalidArg        Status = C.FC_ERR_INVALID_ARG
	StatusDimensionMismatch Status = C.FC_ERR_DIMENSION_MISMATCH
	StatusSingularMatrix    Status = C.FC_ERR_SINGULAR_MATRIX
	StatusNotPositiveDef    Status = C.FC_ERR_NOT_POSITIVE_DEF
	StatusConvergence       Status = C.FC_ERR_CONVERGENCE
	StatusWorkspaceTooSmall Status = C.FC_ERR_WORKSPACE_TOO_SMALL
	StatusOverflow          Status = C.FC_ERR_OVERFLOW
	StatusNaNInput          Status = C.FC_ERR_NAN_INPUT
	StatusAlignment         Status = C.FC_ERR_ALIGNMENT
	StatusNotInitialized    Status = C.FC_ERR_NOT_INITIALIZED
	StatusBufferTooSmall    Status = C.FC_ERR_BUFFER_TOO_SMALL
	StatusOutOfMemory       Status = C.FC_ERR_OUT_OF_MEMORY
	StatusIllegalState      Status = C.FC_ERR_ILLEGAL_STATE
	StatusNotImplemented    Status = C.FC_ERR_NOT_IMPLEMENTED
	StatusAssertionFailed   Status = C.FC_ERR_ASSERTION_FAILED

	// Aliases for backward compatibility
	StatusError         = StatusInvalidArg
	StatusAlreadyInit   = StatusIllegalState
	StatusUnsupported   = StatusNotImplemented
	StatusInvalidState  = StatusIllegalState
	StatusCorruptedData = StatusIllegalState
)

func (s Status) Error() error {
	if s == StatusOK {
		return nil
	}
	return fmt.Errorf("%s", s.String())
}

func (s Status) String() string {
	return C.GoString(C.fc_status_string(C.fc_status_t(s)))
}

// AlignedAlloc allocates memory aligned to the specified boundary
func AlignedAlloc(size int, alignment int) ([]byte, error) {
	if size <= 0 {
		return nil, fmt.Errorf("size must be positive")
	}
	if alignment <= 0 || (alignment&(alignment-1)) != 0 {
		return nil, fmt.Errorf("alignment must be a power of 2")
	}

	ptr := C.fc_aligned_alloc(C.size_t(size), C.size_t(alignment))
	if ptr == nil {
		return nil, fmt.Errorf("allocation failed")
	}

	// Track this allocation
	ptrVal := uintptr(ptr)
	allocTracker.Lock()
	allocTracker.ptrs[ptrVal] = true
	allocTracker.Unlock()

	// Create a Go slice backed by C memory
	data := (*[1 << 30]byte)(ptr)[:size:size]

	// Set finalizer to free memory when slice is garbage collected
	runtime.SetFinalizer(&data, func(d *[]byte) {
		if len(*d) > 0 {
			p := unsafe.Pointer(&(*d)[0])
			pv := uintptr(p)

			allocTracker.Lock()
			if allocTracker.ptrs[pv] {
				C.fc_aligned_free(p)
				delete(allocTracker.ptrs, pv)
			}
			allocTracker.Unlock()
		}
	})

	return data, nil
}

// AlignedFree explicitly frees aligned memory
func AlignedFree(data []byte) {
	if len(data) == 0 {
		return
	}

	p := unsafe.Pointer(&data[0])
	pv := uintptr(p)

	allocTracker.Lock()
	defer allocTracker.Unlock()

	if allocTracker.ptrs[pv] {
		runtime.SetFinalizer(&data, nil)
		C.fc_aligned_free(p)
		delete(allocTracker.ptrs, pv)
	}
}

// Compiler returns the compiler name
func Compiler() string {
	return "gcc/clang"
}

// CompilerVersion returns the compiler version
func CompilerVersion() string {
	return "unknown"
}

// OS returns the operating system name
func OS() string {
	return runtime.GOOS
}

// Arch returns the CPU architecture
func Arch() string {
	return runtime.GOARCH
}
