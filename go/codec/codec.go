// Package codec provides high-performance FIX protocol encoding and decoding.
package codec

/*
#cgo CFLAGS: -I../../include -I../../core/platform/include
#cgo linux LDFLAGS: -L../../build/linux_amd64/lib -lfinkit -lgcov -lm
#cgo darwin LDFLAGS: -L../../build/darwin_amd64/lib -lfinkit
#cgo windows LDFLAGS: -L../../build/windows_amd64/lib -lfinkit
*/
import "C"
