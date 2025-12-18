# Simple top-level Makefile that delegates to CMake (out-of-source build)
BUILD_DIR := build
CMAKE     ?= cmake
BUILD_TYPE ?= Debug

.PHONY: all configure build clean distclean test run

# default: configure + build
all: configure build

# configure step (generates build system in $(BUILD_DIR))
configure:
	@echo "Configuring (build dir: $(BUILD_DIR), type: $(BUILD_TYPE))"
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

# build using whatever generator CMake selected
build:
	@echo "Building (dir: $(BUILD_DIR))"
	$(CMAKE) --build $(BUILD_DIR)

# run tests via ctest (if you use CTest)
test:
	@echo "Running tests (dir: $(BUILD_DIR))"
	cd $(BUILD_DIR) && ctest --output-on-failure

# convenience: run compiled binary (adjust path/target as needed)
run:
	@echo "Building then running default target"
	$(CMAKE) --build $(BUILD_DIR)

# remove build artifacts but keep build dir
clean:
	@echo "Cleaning build directory"
	-rm -rf $(BUILD_DIR)/*

# remove build dir entirely
distclean:
	@echo "Removing entire build directory"
	-rm -rf $(BUILD_DIR)

# allow overriding BUILD_DIR, BUILD_TYPE from command line:
# e.g.  make BUILD_TYPE=Release
