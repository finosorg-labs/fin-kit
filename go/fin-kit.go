// Package fin_kit provides a high-performance financial computing library
// with SIMD-optimized primitives, compression codecs, and statistical routines.
//
// This is the main integration package. Individual modules are available as submodules:
//   - core/platform: Platform abstraction layer
//   - core/matrix: Matrix and linear algebra operations
//   - core/codec: Data compression and encoding
//
// For Go bindings, import the submodules directly from their respective directories.
package fin_kit

// Version information
const (
	VersionMajor = 1
	VersionMinor = 0
	VersionPatch = 0
	Version      = "1.0.0"
)
