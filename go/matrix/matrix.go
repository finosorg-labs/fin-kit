// Package matrix provides high-performance matrix operations with SIMD optimization.
package matrix

/*
#cgo CFLAGS: -I../../include
#cgo linux LDFLAGS: -L../../build/linux_amd64/lib -lfinkit -lgcov -lm
#cgo darwin LDFLAGS: -L../../build/darwin_amd64/lib -lfinkit
#cgo windows LDFLAGS: -L../../build/windows_amd64/lib -lfinkit

#include <matrix/matrix.h>
*/
import "C"

import (
	"fmt"
)

// checkStatus converts C status code to Go error
func checkStatus(status C.int, operation string) error {
	if status == 0 {
		return nil
	}
	return fmt.Errorf("%s failed with status %d", operation, int(status))
}
