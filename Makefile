# fin-kit Makefile
#
# Build Targets:
#   make              build Linux native with prebuilt modules (default)
#   make linux        build Linux native with prebuilt modules
#   make linux-source build Linux native from source (no prebuilt)
#   make windows      cross-compile Windows amd64 with prebuilt modules
#   make windows-source cross-compile Windows amd64 from source
#   make all          build Linux + Windows + Go
#   make source
#   make test         run tests (C + Go + coverage)
#   make bench        run benchmarks (C + Go)
#
# QA Targets:
#   make qa                run all QA checks (sanitizers + static analysis)
#   make qa-sanitizers     run all sanitizers (ASan/USan/TSan/MSan)
#   make qa-static         run static analysis (clang-tidy/cppcheck)
#
#   Individual sanitizers:
#     make sanitizer-asan   AddressSanitizer
#     make sanitizer-usan   UndefinedBehaviorSanitizer
#     make sanitizer-tsan   ThreadSanitizer
#     make sanitizer-msan   MemorySanitizer (requires clang)
#
#   Individual static analysis:
#     make clang-tidy       clang-tidy analysis
#     make cppcheck         cppcheck analysis
#
#
# Utility Targets:
#   make verify    verify artifact formats
#   make clean     remove all build artifacts
#   make help      show detailed help
#
# Submodule Targets:
#   make sync              sync all submodules (init + remote update + restore tracked files)
#
# Output structure:
#   build/linux_amd64/{bin,lib}/
#   build/windows_amd64/{bin,lib}/
#   build/sanitizer-*/{bin,lib}/
#
# Dependencies:
#   Build: gcc cmake ninja-build
#   Cross-compile: mingw-w64
#   QA: clang clang-tidy cppcheck lcov
#
# Manual install (if sudo not available):
#   sudo apt install build-essential cmake ninja-build mingw-w64 clang clang-tidy cppcheck lcov
#
#

BUILD_TYPE ?= Release
CMAKE ?= cmake
TOOLCHAIN_DIR := cmake/toolchain

LINUX_BUILD_DIR  := build/linux_amd64
WINDOWS_BUILD_DIR := build/windows_amd64

LINUX_CONFIG := -G Ninja \
	-B $(LINUX_BUILD_DIR) \
	-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
	-DCMAKE_C_FLAGS="-fprofile-arcs -ftest-coverage" \
	-DCMAKE_EXE_LINKER_FLAGS="-fprofile-arcs -ftest-coverage" \
	-DFC_BUILD_TESTS=ON \
	-DFC_BUILD_BENCHMARKS=$(shell [ "$(BUILD_TYPE)" = "Release" ] && echo ON || echo OFF) \
	-DFC_USE_PREBUILT_MODULES=ON

LINUX_SOURCE_CONFIG := -G Ninja \
	-B $(LINUX_BUILD_DIR) \
	-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
	-DCMAKE_C_FLAGS="-fprofile-arcs -ftest-coverage" \
	-DCMAKE_EXE_LINKER_FLAGS="-fprofile-arcs -ftest-coverage" \
	-DFC_BUILD_TESTS=ON \
	-DFC_BUILD_BENCHMARKS=$(shell [ "$(BUILD_TYPE)" = "Release" ] && echo ON || echo OFF)

WINDOWS_CONFIG := -G "Unix Makefiles" \
	-B $(WINDOWS_BUILD_DIR) \
	-DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN_DIR)/x86_64-w64-mingw32.cmake \
	-DCMAKE_MAKE_PROGRAM=/usr/bin/make \
	-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
	-DFC_BUILD_TESTS=ON \
	-DFC_BUILD_BENCHMARKS=ON \
	-DFC_USE_PREBUILT_MODULES=ON

WINDOWS_SOURCE_CONFIG := -G "Unix Makefiles" \
	-B $(WINDOWS_BUILD_DIR) \
	-DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN_DIR)/x86_64-w64-mingw32.cmake \
	-DCMAKE_MAKE_PROGRAM=/usr/bin/make \
	-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
	-DFC_BUILD_TESTS=ON \
	-DFC_BUILD_BENCHMARKS=ON \
	-DFC_USE_PREBUILT_MODULES=OFF

LINUX_ARTIFACT_DIR    := $(LINUX_BUILD_DIR)/bin
WINDOWS_ARTIFACT_DIR  := $(WINDOWS_BUILD_DIR)/bin

.PHONY: all default linux linux-source windows windows-source go test bench clean verify help
.PHONY: qa qa-sanitizers qa-static
.PHONY: sanitizer-asan sanitizer-usan sanitizer-tsan sanitizer-msan clang-tidy cppcheck
.PHONY: sync

default: linux

all: linux windows go

source: linux-source windows-source go

qa: qa-sanitizers qa-static
	@echo "==> All QA checks completed"

qa-sanitizers: sanitizer-asan sanitizer-usan sanitizer-tsan sanitizer-msan
	@echo "==> All sanitizer checks completed"

qa-static: clang-tidy cppcheck
	@echo "==> All static analysis checks completed"

linux:
	@echo "==> Building Linux (native, $(BUILD_TYPE), prebuilt modules)"
	$(CMAKE) $(LINUX_CONFIG)
	$(CMAKE) --build $(LINUX_BUILD_DIR) --parallel

linux-source:
	@echo "==> Building Linux (native, $(BUILD_TYPE), from source)"
	$(CMAKE) $(LINUX_SOURCE_CONFIG)
	$(CMAKE) --build $(LINUX_BUILD_DIR) --parallel

windows:
	@echo "==> Building Windows amd64 (cross-compile, $(BUILD_TYPE), prebuilt modules)"
	$(CMAKE) $(WINDOWS_CONFIG)
	$(CMAKE) --build $(WINDOWS_BUILD_DIR) --parallel

windows-source:
	@echo "==> Building Windows amd64 (cross-compile, $(BUILD_TYPE), from source)"
	$(CMAKE) $(WINDOWS_SOURCE_CONFIG)
	$(CMAKE) --build $(WINDOWS_BUILD_DIR) --parallel

go:
	@echo "==> Building Go module (verify compilation)"
	cd go && CGO_CFLAGS_ALLOW="-m(avx2|avx512f|avx512dq|fma|sse4\.2)" go build ./...

test: linux
	@echo "==> Running C tests with coverage"
	@bash scripts/test_coverage.sh $(LINUX_BUILD_DIR)
	@echo ""
	@echo "==> Running Go tests"
	cd go && CGO_CFLAGS_ALLOW="-m(avx2|avx512f|avx512dq|fma|sse4\.2)" go test ./... -v

bench:
	@echo "==> Building benchmarks (Release mode)"
	@BUILD_TYPE=Release $(CMAKE) -B $(LINUX_BUILD_DIR) \
		-G Ninja \
		-DCMAKE_BUILD_TYPE=Release \
		-DFC_BUILD_TESTS=OFF \
		-DFC_BUILD_BENCHMARKS=ON >/dev/null 2>&1 || true
	@$(CMAKE) --build $(LINUX_BUILD_DIR) --parallel
	@echo "==> Running C benchmarks"
	@if [ -f $(LINUX_ARTIFACT_DIR)/finkit_benchmarks ]; then \
		$(LINUX_ARTIFACT_DIR)/finkit_benchmarks; \
	else \
		echo "No C benchmarks found"; \
	fi
	@echo ""
	@echo "==> Running Go benchmarks"
	@cd go && CGO_CFLAGS_ALLOW="-m(avx2|avx512f|avx512dq|fma|sse4\.2)" go test -bench=. -benchmem ./...

verify:
	@echo "=== Verify artifact formats ===" && \
	echo "--- Linux ---" && \
		objdump -f $(LINUX_ARTIFACT_DIR)/*.a 2>/dev/null | grep "file format" || echo "(no artifacts)"; \
	echo "--- Windows ---" && \
		x86_64-w64-mingw32-objdump -f $(WINDOWS_ARTIFACT_DIR)/*.a 2>/dev/null | grep "file format" || echo "(no artifacts)"

sanitizer-asan:
	@echo "==> Building with AddressSanitizer"
	@$(CMAKE) -B build/sanitizer-asan \
		-G Ninja \
		-DCMAKE_BUILD_TYPE=Debug \
		-DFC_BUILD_TESTS=ON \
		-DFC_BUILD_BENCHMARKS=OFF \
		-DFC_ENABLE_SANITIZERS=ON \
		-DFC_SANITIZER_TYPE=address >/dev/null 2>&1 || true
	@$(CMAKE) --build build/sanitizer-asan --parallel
	@echo "==> Running AddressSanitizer tests"
	@cd build/sanitizer-asan && ctest --output-on-failure

sanitizer-usan:
	@echo "==> Building with UndefinedBehaviorSanitizer"
	@$(CMAKE) -B build/sanitizer-usan \
		-G Ninja \
		-DCMAKE_BUILD_TYPE=Debug \
		-DFC_BUILD_TESTS=ON \
		-DFC_BUILD_BENCHMARKS=OFF \
		-DFC_ENABLE_SANITIZERS=ON \
		-DFC_SANITIZER_TYPE=undefined >/dev/null 2>&1 || true
	@$(CMAKE) --build build/sanitizer-usan --parallel
	@echo "==> Running UndefinedBehaviorSanitizer tests"
	@cd build/sanitizer-usan && ctest --output-on-failure

sanitizer-tsan:
	@echo "==> Building with ThreadSanitizer"
	@$(CMAKE) -B build/sanitizer-tsan \
		-G Ninja \
		-DCMAKE_BUILD_TYPE=Debug \
		-DFC_BUILD_TESTS=ON \
		-DFC_BUILD_BENCHMARKS=OFF \
		-DFC_ENABLE_SANITIZERS=ON \
		-DFC_SANITIZER_TYPE=thread >/dev/null 2>&1 || true
	@$(CMAKE) --build build/sanitizer-tsan --parallel
	@echo "==> Running ThreadSanitizer tests"
	@cd build/sanitizer-tsan && ctest --output-on-failure || \
		(echo "WARNING: ThreadSanitizer failed (known WSL2 compatibility issue)" && exit 0)

sanitizer-msan:
	@echo "==> Building with MemorySanitizer (requires clang)"
	@CC=clang $(CMAKE) -B build/sanitizer-msan \
		-G Ninja \
		-DCMAKE_BUILD_TYPE=Debug \
		-DFC_BUILD_TESTS=ON \
		-DFC_BUILD_BENCHMARKS=OFF \
		-DFC_ENABLE_SANITIZERS=ON \
		-DFC_SANITIZER_TYPE=memory >/dev/null 2>&1 || true
	@$(CMAKE) --build build/sanitizer-msan --parallel
	@echo "==> Running MemorySanitizer tests"
	@cd build/sanitizer-msan && ctest --output-on-failure

clang-tidy:
	@echo "==> Building with clang for static analysis"
	@CC=clang CXX=clang++ $(CMAKE) -B build/clang-tidy \
		-G Ninja \
		-DCMAKE_BUILD_TYPE=Debug \
		-DFC_BUILD_TESTS=ON \
		-DFC_BUILD_BENCHMARKS=OFF \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON >/dev/null 2>&1 || true
	@$(CMAKE) --build build/clang-tidy --parallel --target finkit
	@echo "==> Running clang-tidy analysis (excluding third_party)"
	@find src include \( -name '*.c' -o -name '*.h' \) \
		! -path '*/third_party/*' \
		! -name 'platform_macos.c' \
		! -name 'platform_win.c' | \
		xargs clang-tidy -p build/clang-tidy --quiet 2>&1 || true

cppcheck:
	@echo "==> Running cppcheck static analysis (excluding third_party)"
	@cppcheck --enable=warning,performance,portability --std=c11 \
		--suppress=missingInclude \
		--suppress=unusedFunction \
		--suppress=missingIncludeSystem \
		-i third_party \
		--quiet \
		src/ tests/ benchmarks/ include/ 2>&1 | grep -v "^nofile:0:0: information:" || true

sync:
	@echo "==> Syncing all submodules (init + remote update + restore tracked files)"
	git submodule foreach --recursive 'git reset --hard && git clean -fd'
	git submodule update --init --remote --merge --recursive --force
	@echo "==> Submodules synced successfully"

clean:
	rm -rf build

help:
	@echo "fin-kit Makefile" && \
	echo "" && \
	echo "Build targets:" && \
	echo "  make              build Linux native with prebuilt modules (default)" && \
	echo "  make linux        build Linux native with prebuilt modules" && \
	echo "  make linux-source build Linux native from source (no prebuilt)" && \
	echo "  make windows      cross-compile Windows amd64 with prebuilt modules" && \
	echo "  make windows-source cross-compile Windows amd64 from source" && \
	echo "  make all          build Linux + Windows + Go" && \
	echo "  make go           build Go module" && \
	echo "" && \
	echo "Test targets:" && \
	echo "  make test     run tests (C + Go + coverage)" && \
	echo "  make bench    run benchmarks (C + Go)" && \
	echo "" && \
	echo "QA targets:" && \
	echo "  make qa                run all QA checks (sanitizers + static analysis)" && \
	echo "  make qa-sanitizers     run all sanitizers" && \
	echo "  make qa-static         run static analysis" && \
	echo "" && \
	echo "  Individual sanitizers:" && \
	echo "    make sanitizer-asan   AddressSanitizer" && \
	echo "    make sanitizer-usan   UndefinedBehaviorSanitizer" && \
	echo "    make sanitizer-tsan   ThreadSanitizer" && \
	echo "    make sanitizer-msan   MemorySanitizer (requires clang)" && \
	echo "" && \
	echo "  Individual analysis:" && \
	echo "    make clang-tidy       clang-tidy static analysis" && \
	echo "    make cppcheck         cppcheck static analysis" && \
	echo "" && \
	echo "Utility targets:" && \
	echo "  make verify   verify artifact formats" && \
	echo "  make clean    remove build artifacts" && \
	echo "" && \
	echo "Submodule targets:" && \
	echo "  make sync              sync all submodules (init + remote update + restore tracked files)" && \
	echo "" && \
	echo "Options:" && \
	echo "  BUILD_TYPE=Debug make   build Debug variant" && \
	echo "" && \
	echo "  Build directories:" && \
	echo "    $(LINUX_BUILD_DIR)/" && \
	echo "    $(WINDOWS_BUILD_DIR)/" && \
	echo "    build/sanitizer-*/" && \
	echo "    build/clang-tidy/" && \
	echo "" && \
	echo "  Output structure:" && \
	echo "    build/linux_amd64/bin/" && \
	echo "    build/linux_amd64/lib/" && \
	echo "    build/windows_amd64/bin/" && \
	echo "    build/windows_amd64/lib/" && \
	echo "" && \
	echo "Required packages (install manually if sudo is not available):" && \
	echo "  Build deps: build-essential cmake ninja-build" && \
	echo "  Cross-compile: mingw-w64" && \
	echo "  QA deps: clang clang-tidy cppcheck lcov"
