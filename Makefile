# Simple top-level Makefile that delegates to CMake (out-of-source build)
BUILD_DIR := build
CMAKE     ?= cmake
BUILD_TYPE ?= Debug

.PHONY: all configure build clean distclean test run

# default: just build (will auto-configure if needed)
all: build

# configure step (generates build system in $(BUILD_DIR))
# only runs cmake if CMakeCache.txt doesn't exist
$(BUILD_DIR)/CMakeCache.txt:
	@echo "Configuring (build dir: $(BUILD_DIR), type: $(BUILD_TYPE))"
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

configure: $(BUILD_DIR)/CMakeCache.txt

# build depends on configure (will auto-configure if needed)
build: $(BUILD_DIR)/CMakeCache.txt
	@echo "Building (dir: $(BUILD_DIR))"
	$(CMAKE) --build $(BUILD_DIR)

# run the main executable
run: build
	@echo "Running $(BUILD_DIR)/code"
	./$(BUILD_DIR)/code

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
