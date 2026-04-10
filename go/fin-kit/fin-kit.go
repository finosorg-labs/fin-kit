// Package fin_kit provides a high-performance financial computing library
// with SIMD-optimized primitives, compression codecs, and statistical routines.
//
// Initialization is optional but recommended for production use:
//
//	fin_kit.Init()
//	defer fin_kit.Cleanup()
//
// Without explicit initialization the library operates in lazy mode.
package fin_kit

/*
#include "fin_kit.h"
*/
import "C"

import (
	"errors"
	"runtime"
	"sync"
	"unsafe"
)

// =============================================================================
// Version
// =============================================================================

const (
	VersionMajor = 1
	VersionMinor = 0
	VersionPatch = 0
)

const Version = "1.0.0"

// =============================================================================
// Config
// =============================================================================

type Config struct {
	NumThreads    int
	SIMDLevel     string
	EnableAVX2    bool
	MemoryLimitMB uint64
	Verbose       bool
}

func DefaultConfig() Config {
	return Config{
		NumThreads: runtime.NumCPU(),
		Verbose:    false,
	}
}

// =============================================================================
// Library state
// =============================================================================

type libState struct {
	mu          sync.Mutex
	initialized bool
	cleanedUp   bool
	config      Config
	refCount    int
}

var globalState = &libState{}

// =============================================================================
// Init / Cleanup
// =============================================================================

// Init initializes the fin-kit library with default configuration.
// Safe to call multiple times; only the first call performs actual init.
// Pair with Cleanup via defer:
//
//	fin_kit.Init()
//	defer fin_kit.Cleanup()
func Init() error {
	return InitWithConfig(DefaultConfig())
}

// InitWithConfig initializes the library with an explicit configuration.
func InitWithConfig(cfg Config) error {
	globalState.mu.Lock()
	defer globalState.mu.Unlock()

	if globalState.cleanedUp {
		return errors.New("fin_kit: cannot Init after Cleanup")
	}
	if globalState.initialized {
		globalState.refCount++
		return nil
	}

	if cfg.NumThreads <= 0 {
		cfg.NumThreads = runtime.NumCPU()
	}

	cCfg := C.fin_kit_config_t{
		num_threads:     C.int(cfg.NumThreads),
		enable_avx2:    boolToCInt(cfg.EnableAVX2),
		memory_limit_mb: C.uint(cfg.MemoryLimitMB),
		verbose:         boolToCInt(cfg.Verbose),
	}

	C.fin_kit_lib_init(&cCfg)

	globalState.config = cfg
	globalState.initialized = true
	globalState.refCount = 1

	return nil
}

// Cleanup releases all resources held by the library.
// Safe to call when the library is not initialized (no-op).
// Each Init increments a reference count; Cleanup decrements it;
// resources are freed when the count reaches zero.
func Cleanup() {
	globalState.mu.Lock()
	defer globalState.mu.Unlock()

	if globalState.cleanedUp || !globalState.initialized {
		return
	}

	globalState.refCount--
	if globalState.refCount > 0 {
		return
	}

	C.fin_kit_lib_cleanup()
	globalState.initialized = false
}

// IsInitialized reports whether the library is currently initialized.
func IsInitialized() bool {
	globalState.mu.Lock()
	defer globalState.mu.Unlock()
	return globalState.initialized && !globalState.cleanedUp
}

// =============================================================================
// SIMD support queries
// =============================================================================

type SIMDLevel C.fin_kit_simd_level_t

const (
	SIMDNone  SIMDLevel = 0
	SIMDSSE42 SIMDLevel = 1
	SIMDAVX2  SIMDLevel = 2
	SIMDAVX512 SIMDLevel = 3
)

func (l SIMDLevel) String() string {
	return C.GoString(C.fin_kit_simd_level_string(C.fin_kit_simd_level_t(l)))
}

// DetectSIMD returns the highest SIMD level available on the current CPU.
func DetectSIMD() SIMDLevel {
	return SIMDLevel(C.fin_kit_simd_detect())
}

// HasSIMD reports whether the current CPU supports the given SIMD level.
func HasSIMD(minLevel SIMDLevel) bool {
	return DetectSIMD() >= minLevel
}

// Parallelism returns the SIMD lane width for double-precision values.
func Parallelism() int {
	return int(C.fin_kit_simd_parallelism(C.fin_kit_simd_level_t(DetectSIMD())))
}

// =============================================================================
// Status / Error handling
// =============================================================================

type Status C.int

const (
	StatusOK              Status = 0
	StatusError          Status = 1
	StatusInvalidPointer Status = 2
	StatusInvalidSize    Status = 3
	StatusNoMemory       Status = 4
	StatusInvalidAlign   Status = 5
	StatusNotInitialized Status = 6
	StatusAlreadyDone    Status = 7
	StatusCancellation   Status = 8
	StatusTimeout        Status = 9
	StatusOverflow       Status = 10
	StatusUnderflow      Status = 11
	StatusDivByZero      Status = 12
	StatusInvalidOperand Status = 13
	StatusNotImplemented Status = 14
	StatusAssertionFail  Status = 15
)

func (s Status) Error() error {
	if s == StatusOK {
		return nil
	}
	return errors.New(C.GoString(C.fin_kit_status_string(C.int(s))))
}

func (s Status) String() string {
	return C.GoString(C.fin_kit_status_string(C.int(s)))
}

// =============================================================================
// Aligned memory allocation
// =============================================================================

var (
	ErrAlignment = errors.New("fin_kit: alignment must be a power of two and >= 1")
	ErrSize      = errors.New("fin_kit: size must be > 0")
)

// AlignedAlloc allocates size bytes with the given power-of-two alignment.
// The returned slice must be freed with AlignedFree, or will be reclaimed by GC.
func AlignedAlloc(size int, alignment int) ([]byte, error) {
	if size <= 0 {
		return nil, ErrSize
	}
	if alignment < 1 || (alignment&(alignment-1)) != 0 {
		return nil, ErrAlignment
	}

	ptr := C.fin_kit_aligned_alloc(C.size_t(size), C.size_t(alignment))
	if ptr == nil {
		return nil, errors.New("fin_kit: fin_kit_aligned_alloc failed")
	}

	slice := unsafe.Slice((*byte)(ptr), size)

	runtime.SetFinalizer(&slice, func(b *[]byte) {
		C.fin_kit_aligned_free(unsafe.Pointer(&(*b)[0]))
	})

	return slice, nil
}

// AlignedFree explicitly frees memory allocated by AlignedAlloc.
// Safe to call on nil or empty slices.
func AlignedFree(data []byte) {
	if len(data) == 0 {
		return
	}
	runtime.SetFinalizer(&data, nil)
	C.fin_kit_aligned_free(unsafe.Pointer(&data[0]))
}

// =============================================================================
// Platform info
// =============================================================================

func Compiler() string {
	return C.GoString(C.fin_kit_compiler_name())
}

func CompilerVersion() string {
	return C.GoString(C.fin_kit_compiler_version())
}

func OS() string {
	return C.GoString(C.fin_kit_os_name())
}

func Arch() string {
	return C.GoString(C.fin_kit_arch_name())
}

// =============================================================================
// Helpers
// =============================================================================

func boolToCInt(b bool) C.int {
	if b {
		return 1
	}
	return 0
}
