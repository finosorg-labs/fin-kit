//go:build fin_kit_cgo

// Source mode: compile C sources directly.
// Usage: go build -tags fin_kit_cgo
// Requires GCC or Clang.
package fin_kit

/*
#cgo CFLAGS: -O2 -Wall -std=c11 -I${SRCDIR} -I${SRCDIR}/../../src -I${SRCDIR}/../../third_party
#cgo LDFLAGS: -lm -lpthread
#include "fin_kit_cgo.c"
*/
import "C"
