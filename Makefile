# fin-kit Makefile
#
# Build Targets:
#   make              build Go module (default)
#   make go           build Go module
#   make test         run Go tests
#
# Utility Targets:
#   make clean        remove build artifacts
#   make help         show detailed help
#
# Submodule Targets:
#   make sync         sync all submodules (init + remote update + restore tracked files)
#
# Note: C library builds are handled by individual submodules in core/
#       Navigate to core/<module>/ and run make there for C builds

.PHONY: default test clean help sync

default: test

test:
	@echo "==> Running  Go tests"
	@cd broker && CGO_ENABLED=1 CGO_CFLAGS_ALLOW="-m(avx2|avx512f|avx512dq|fma|sse4\.2)" go test -vet=all -race -parallel=4 -tags lib ./... -v
	@cd exchange && CGO_ENABLED=1 CGO_CFLAGS_ALLOW="-m(avx2|avx512f|avx512dq|fma|sse4\.2)" go test -vet=all -race -parallel=4 -tags lib ./... -v
	@cd hft && CGO_ENABLED=1 CGO_CFLAGS_ALLOW="-m(avx2|avx512f|avx512dq|fma|sse4\.2)" go test -vet=all -race -parallel=4 -tags lib ./... -v


clean:
	rm -rf build

sync:
	@echo "==> Syncing all submodules to latest remote state (single level, serial mode)"
	@echo "==> Step 1: Clean up removed submodules from git index"
	@git ls-files -s | grep '^160000' | awk '{print $$4}' | while read path; do \
		if ! git config -f .gitmodules --get-regexp "submodule\..*\.path" | grep -q "$$path$$"; then \
			echo "  Removing stale submodule: $$path"; \
			git rm --cached "$$path" 2>/dev/null || true; \
		fi; \
	done
	@echo "==> Step 2: Sync submodule URLs from .gitmodules to .git/config"
	@git submodule sync
	@echo "==> Step 3: Register missing submodules to git index"
	@git config -f .gitmodules --get-regexp '^submodule\..*\.path$$' | cut -d' ' -f2 | while read submodule_path; do \
		if ! git ls-files -s "$$submodule_path" | grep -q '^160000'; then \
			submodule_name=$$(basename "$$submodule_path"); \
			submodule_url=$$(git config -f .gitmodules submodule.$$submodule_name.url 2>/dev/null || git config -f .gitmodules submodule."$$submodule_path".url); \
			if [ -n "$$submodule_url" ]; then \
				echo "  Registering submodule: $$submodule_path"; \
				git submodule add --force "$$submodule_url" "$$submodule_path" 2>/dev/null || true; \
			fi; \
		fi; \
	done
	@echo "==> Step 4: Initialize all submodules (single level, serial, --jobs 1)"
	@git submodule update --init --jobs 1
	@echo "==> Step 5: Update each submodule to latest remote state (serial processing)"
	@git config -f .gitmodules --get-regexp '^submodule\..*\.path$$' | cut -d' ' -f2 | while read submodule_path; do \
		if [ -d "$$submodule_path" ]; then \
			echo "Processing submodule: $$submodule_path"; \
			( \
				cd "$$submodule_path" && \
				echo "  Fetching from remote..." && \
				git fetch origin && \
				submodule_name=$$(basename "$$submodule_path") && \
				branch=$$(git config -f ../.gitmodules submodule.$$submodule_name.branch 2>/dev/null || echo main) && \
				echo "  Checking out branch: $$branch" && \
				(git checkout $$branch 2>/dev/null || git checkout -b $$branch origin/$$branch 2>/dev/null || git checkout main) && \
				echo "  Pulling latest changes..." && \
				git pull origin $$branch 2>/dev/null || git pull origin main && \
				echo "  Resetting to remote state..." && \
				git reset --hard HEAD && \
				git clean -fd \
			) || echo "  Warning: Failed to update $$submodule_path, continuing..."; \
			sleep 1; \
		fi; \
	done
	@echo "==> Step 6: Update parent repo to track latest submodule commits"
	@git add .gitmodules 2>/dev/null || true
	@git config -f .gitmodules --get-regexp '^submodule\..*\.path$$' | cut -d' ' -f2 | while read path; do \
		[ -d "$$path" ] && git add "$$path" 2>/dev/null || true; \
	done
	@echo "==> Submodules synced to latest state successfully (single level only)"
	@echo ""
	@echo "Submodule status:"
	@git submodule status

help:
	@echo "fin-kit Makefile" && \
	echo "" && \
	echo "Build targets:" && \
	echo "  make              build Go module (default)" && \
	echo "  make go           build Go module" && \
	echo "  make test         run Go tests" && \
	echo "" && \
	echo "Utility targets:" && \
	echo "  make clean        remove build artifacts" && \
	echo "  make sync         sync all submodules" && \
	echo "" && \
	echo "Note: C library builds are handled by individual submodules." && \
	echo "      Navigate to core/<module>/ and run make there for C builds."
