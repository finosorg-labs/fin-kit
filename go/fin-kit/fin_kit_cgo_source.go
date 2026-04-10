//go:build fin_kit_cgo

// Source mode: compile C sources directly.
// Usage: go build -tags fin_kit_cgo
// Requires GCC or Clang.
package fin_kit

/*
#cgo CFLAGS: -O2 -Wall -std=c11 -I${SRCDIR}/.. -I${SRCDIR}/../../include -I${SRCDIR}/../../src

#include "fin_kit_source.c.h"
#include "../../src/platform/simd_detect.c"
#include "../../src/platform/mem_aligned.c"
#include "../../src/platform/error.c"
*/
import "C"
