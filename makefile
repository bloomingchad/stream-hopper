# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -Iinclude
LDFLAGS = -lncursesw -lmpv

# Source files
SRCS = $(wildcard src/*.cpp) $(wildcard src/Core/*.cpp) $(wildcard src/UI/*.cpp) $(wildcard src/UI/Layout/*.cpp)
# FIX: The line below was redundant and caused the linker error.
# The wildcard above already finds FirstRunWizard.cpp.

# Object files
OBJS = $(patsubst src/%.cpp, build/%.o, $(SRCS))

# Executable name
TARGET = build/stream-hopper

# Helper script
HELPER_SCRIPT = api_helper.sh
HELPER_TARGET = build/$(HELPER_SCRIPT)

.PHONY: all clean distclean run

all: $(TARGET)

# Link the executable
$(TARGET): $(OBJS)
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

# Clean up build files only. This is safe for users.
clean:
	rm -rf build

# "Distribution Clean": Cleans build files AND all user-generated data.
# This provides a full factory reset for developers.
distclean: clean
	rm -f radio_*.json
	rm -f *.jsonc
	rm -f stream_hopper_crash.log

# Command to run the application
run: all
	./$(TARGET)
