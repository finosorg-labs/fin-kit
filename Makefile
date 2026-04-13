# fin-kit Makefile
#
# Targets:
#   make           build Linux native (default)
#   make linux     build Linux native
#   make windows   cross-compile Windows amd64
#   make all       build Linux + Windows
#   make test      run tests (Linux only)
#   make clean     remove all build artifacts
#   make help      show this message
#
# Output structure:
#   build/artifacts/Linux-x86_64/GCC-<BuildType>/   ← Linux artifacts
#   build/artifacts/Windows-x86_64/GCC-<BuildType>/ ← Windows artifacts
#
# Dependencies:
#   Linux:  gcc cmake ninja-build
#   Windows: mingw-w64 cmake make
#
# Manual install (sudo not available):
#   Linux deps  : sudo apt install build-essential cmake ninja-build
#   Windows deps: sudo apt install mingw-w64 cmake make
#
# =============================================================================

BUILD_TYPE ?= Release
CMAKE ?= cmake
TOOLCHAIN_DIR := cmake/toolchain

LINUX_BUILD_DIR  := build/linux_amd64
WINDOWS_BUILD_DIR := build/windows_amd64

LINUX_CONFIG := -G Ninja \
	-B $(LINUX_BUILD_DIR) \
	-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
	-DFC_BUILD_TESTS=ON \
	-DFC_BUILD_BENCHMARKS=$(shell [ "$(BUILD_TYPE)" = "Release" ] && echo ON || echo OFF)

WINDOWS_CONFIG := -G "Unix Makefiles" \
	-B $(WINDOWS_BUILD_DIR) \
	-DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN_DIR)/x86_64-w64-mingw32.cmake \
	-DCMAKE_MAKE_PROGRAM=/usr/bin/make \
	-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
	-DFC_BUILD_TESTS=ON \
	-DFC_BUILD_BENCHMARKS=ON

LINUX_ARTIFACT_DIR    := $(LINUX_BUILD_DIR)/bin
WINDOWS_ARTIFACT_DIR  := $(WINDOWS_BUILD_DIR)/bin

.PHONY: all default linux windows go test clean verify help

default: linux

all: linux windows go

linux:
	@echo "==> Building Linux (native, $(BUILD_TYPE))"
	$(CMAKE) $(LINUX_CONFIG)
	$(CMAKE) --build $(LINUX_BUILD_DIR) --parallel

windows:
	@echo "==> Building Windows amd64 (cross-compile, $(BUILD_TYPE))"
	$(CMAKE) $(WINDOWS_CONFIG)
	$(CMAKE) --build $(WINDOWS_BUILD_DIR) --parallel

go:
	@echo "==> Building Go module (verify compilation)"
	cd go/fin-kit && go build -tags fin_kit_cgo ./...

test:
	@echo "==> Running tests on Linux"
	cd $(LINUX_BUILD_DIR) && ctest --output-on-failure

verify:
	@echo "=== Verify artifact formats ===" && \
	echo "--- Linux ---" && \
		objdump -f $(LINUX_ARTIFACT_DIR)/*.a 2>/dev/null | grep "file format" || echo "(no artifacts)"; \
	echo "--- Windows ---" && \
		x86_64-w64-mingw32-objdump -f $(WINDOWS_ARTIFACT_DIR)/*.a 2>/dev/null | grep "file format" || echo "(no artifacts)"

clean:
	rm -rf build

help:
	@echo "fin-kit Makefile" && \
	echo "" && \
	echo "  make          build Linux native" && \
	echo "  make linux    build Linux native" && \
	echo "  make windows  cross-compile Windows amd64" && \
	echo "  make all      build Linux + Windows" && \
	echo "  make test     run tests (Linux only)" && \
	echo "  make verify   verify artifact formats" && \
	echo "  make clean    remove build artifacts" && \
	echo "" && \
	echo "  BUILD_TYPE=Debug make   build Debug variant" && \
	echo "" && \
	echo "  Build directories:" && \
	echo "    $(LINUX_BUILD_DIR)/" && \
	echo "    $(WINDOWS_BUILD_DIR)/" && \
	echo "" && \
	echo "  Output structure:" && \
	echo "    build/linux_amd64/bin/" && \
	echo "    build/linux_amd64/lib/" && \
	echo "    build/windows_amd64/bin/" && \
	echo "    build/windows_amd64/lib/" && \
	echo "" && \
	echo "Required packages (install manually if sudo is not available):" && \
	echo "  Linux deps : build-essential cmake ninja-build" && \
	echo "  Windows deps: mingw-w64 cmake make"
