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

.PHONY: default go test clean help sync

default: go

go:
	@echo "==> Building Go module (verify compilation)"
	cd go && go build ./...

test:
	@echo "==> Running Go tests"
	cd go && go test ./... -v

clean:
	rm -rf build

sync:
	@echo "==> Syncing all submodules (init + remote update + restore tracked files)"
	@echo "==> Step 1: Sync submodule URLs from .gitmodules to .git/config"
	@git submodule sync --recursive
	@echo "==> Step 2: Ensure submodules are registered in git index"
	@git config -f .gitmodules --get-regexp '^submodule\..*\.path$$' | while read key path; do \
		if [ ! -d "$$path/.git" ] && [ ! -f "$$path/.git" ]; then \
			echo "  Restoring submodule: $$path"; \
			submodule_name=$$(echo $$key | sed 's/^submodule\.\(.*\)\.path$$/\1/'); \
			url=$$(git config -f .gitmodules --get "submodule.$$submodule_name.url"); \
			git submodule add -f "$$url" "$$path" 2>/dev/null || true; \
		fi; \
	done
	@echo "==> Step 3: Reset and clean submodule working trees"
	@git submodule foreach --recursive 'git reset --hard && git clean -fd' || true
	@echo "==> Step 4: Update all submodules to latest"
	@git submodule update --init --remote --merge --recursive --force
	@echo "==> Submodules synced successfully"

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
