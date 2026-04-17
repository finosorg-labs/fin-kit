//go:build !lib

// Source mode: compile C sources directly with CGO.
// Usage: go build (default)
// Requires: CGO_CFLAGS_ALLOW="-mavx.*|-msse.*|-mfma|-mavx512.*"
package fin_kit

/*
#cgo CFLAGS: -I../../include -I../../src -O2 -Wall -std=c11 -D_POSIX_C_SOURCE=200112L
#cgo LDFLAGS: -lm

#include <fin-kit/platform/platform.h>
#include <fin-kit/platform/simd_detect.h>
#include <fin-kit/platform/mem_aligned.h>
#include <fin-kit/platform/error.h>
#include <fin-kit/matrix/matrix.h>

// Platform sources
#include "../../src/platform/simd_detect.c"
#include "../../src/platform/mem_aligned.c"
#include "../../src/platform/error.c"
#include "../../src/platform/fc_init.c"

// Platform-specific sources
#if defined(__linux__)
  #include "../../src/platform/platform_linux.c"
#elif defined(__APPLE__)
  #include "../../src/platform/platform_macos.c"
#elif defined(_WIN32)
  #include "../../src/platform/platform_win.c"
#endif

// Matrix sources - scalar and dispatch
#include "../../src/matrix/gemm_scalar.c"
#include "../../src/matrix/gemm_dispatch.c"
#include "../../src/matrix/gemv.c"
#include "../../src/matrix/transpose.c"
#include "../../src/matrix/vector_ops.c"

// SIMD optimized implementations with target attributes
#pragma GCC push_options
#pragma GCC target("sse4.2")
#include "../../src/matrix/gemm_sse42.c"
#pragma GCC pop_options

#pragma GCC push_options
#pragma GCC target("avx2,fma")
#include "../../src/matrix/gemm_avx2.c"
#pragma GCC pop_options

#pragma GCC push_options
#pragma GCC target("avx512f,avx512dq,fma")
#include "../../src/matrix/gemm_avx512.c"
#pragma GCC pop_options
*/
import "C"
