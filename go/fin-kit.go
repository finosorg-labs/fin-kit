// Package fin_kit provides a high-performance financial computing library
// with SIMD-optimized primitives, compression codecs, and statistical routines.
//
// Initialization is optional but recommended for production use:
//
//	fin_kit.Init()
//	defer fin_kit.Cleanup()
package fin_kit

import (
	"fin-kit/matrix"

	"github.com/finos-org-labs/platform"
)

// Version information
const (
	VersionMajor = 1
	VersionMinor = 0
	VersionPatch = 0
	Version      = "1.0.0"
)

// Re-export platform types and functions for backward compatibility
type Config = platform.Config
type SIMDLevel = platform.SIMDLevel
type Status = platform.Status

const (
	SIMDScalar = platform.SIMDScalar
	SIMDSSE42  = platform.SIMDSSE42
	SIMDAVX2   = platform.SIMDAVX2
	SIMDAVX512 = platform.SIMDAVX512
	SIMDNEON   = platform.SIMDNEON

	StatusOK                = platform.StatusOK
	StatusError             = platform.StatusError
	StatusInvalidArg        = platform.StatusInvalidArg
	StatusOutOfMemory       = platform.StatusOutOfMemory
	StatusNotInitialized    = platform.StatusNotInitialized
	StatusAlreadyInit       = platform.StatusAlreadyInit
	StatusUnsupported       = platform.StatusUnsupported
	StatusInvalidState      = platform.StatusInvalidState
	StatusBufferTooSmall    = platform.StatusBufferTooSmall
	StatusCorruptedData     = platform.StatusCorruptedData
	StatusNotImplemented    = platform.StatusNotImplemented
	StatusDimensionMismatch = platform.StatusDimensionMismatch
)

var (
	// Platform functions
	DefaultConfig   = platform.DefaultConfig
	Init            = platform.Init
	InitWithConfig  = platform.InitWithConfig
	Cleanup         = platform.Cleanup
	IsInitialized   = platform.IsInitialized
	DetectSIMD      = platform.DetectSIMD
	HasSIMD         = platform.HasSIMD
	Parallelism     = platform.Parallelism
	AlignedAlloc    = platform.AlignedAlloc
	AlignedFree     = platform.AlignedFree
	Compiler        = platform.Compiler
	CompilerVersion = platform.CompilerVersion
	OS              = platform.OS
	Arch            = platform.Arch

	// Matrix operations
	GEMM          = matrix.GEMM
	GEMV          = matrix.GEMV
	Transpose     = matrix.Transpose
	VecDot        = matrix.VecDot
	VecNormL2     = matrix.VecNormL2
	VecNormL1     = matrix.VecNormL1
	VecDistanceL2 = matrix.VecDistanceL2
	VecDistanceL1 = matrix.VecDistanceL1
)
