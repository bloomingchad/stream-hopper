# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -Iinclude
LDFLAGS = -lncursesw -lmpv

# Source files
SRCS = $(wildcard src/*.cpp) $(wildcard src/Core/*.cpp) $(wildcard src/UI/*.cpp) $(wildcard src/UI/Layout/*.cpp)

# Object files
OBJS = $(patsubst src/%.cpp, build/%.o, $(SRCS))

# Executable name
TARGET = build/stream-hopper

# Helper script
HELPER_SCRIPT = api_helper.sh
HELPER_TARGET = build/$(HELPER_SCRIPT)

# Default config files to be copied to build directory
CONFIG_FILES = search_providers.jsonc
CONFIG_TARGETS = $(patsubst %,build/%,$(CONFIG_FILES))


.PHONY: all clean distclean run

all: $(TARGET)

# Link the executable
$(TARGET): $(OBJS) $(CONFIG_TARGETS) # Depend on config files being copied
	@mkdir -p build
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo "Linking complete: $(TARGET)"

# Compile source files into object files
build/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Rule to copy the helper script to the build directory and make it executable
$(TARGET): $(HELPER_TARGET)
$(HELPER_TARGET): $(HELPER_SCRIPT)
	@mkdir -p build
	cp $(HELPER_SCRIPT) $(HELPER_TARGET)
	chmod +x $(HELPER_TARGET)

# Rule to copy default config files to the build directory
$(CONFIG_TARGETS): build/% : %
	@mkdir -p build
	cp $< $@

# Clean up build files only. This is safe for users.
clean:
	rm -rf build

# "Distribution Clean": Cleans build files AND all user-generated data.
# This provides a full factory reset for developers.
# It explicitly DOES NOT remove search_providers.jsonc from the source directory,
# but it will remove the copy from the build/ directory if clean is called first.
distclean: clean
	rm -f radio_*.json      # User session data
	rm -f volume_offsets.jsonc # User volume normalization data
	rm -f stations.jsonc    # User's main station list
	rm -f *.jsonc           # Any other curated lists like techno.jsonc, etc. (but not search_providers.jsonc in source)
	rm -f stream_hopper_crash.log

# Command to run the application
run: all
	./$(TARGET)
