# Compiler and flags
CXX = g++
# Using the flags from your build log for consistency
CXXFLAGS = -std=c++17 -g -Wall -Wextra -pthread -Iinclude -I/usr/include/fribidi -I/usr/include/cdio -I/usr/include/ffmpeg -I/usr/include/freetype2 -I/usr/include/libpng16 -I/usr/include/harfbuzz -I/usr/include/glib-2.0 -I/usr/lib64/glib-2.0/include -I/usr/include/sysprof-6 -pthread -I/usr/include/libxml2 -I/usr/include/lua-5.1 -I/usr/include/SDL2 -D_GNU_SOURCE=1 -I/usr/include/uchardet -I/usr/include/vapoursynth -I/usr/include/python3.13 -DWITH_GZFILEOP -I/usr/include/AL -I/usr/include/pipewire-0.3 -I/usr/include/spa-0.2 -D_REENTRANT -I/usr/include/libdrm -I/usr/include/ffnvcodec -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600

LDFLAGS = -pthread
LDLIBS = -lmpv -lncursesw -ltinfo

# Executable name
TARGET = build/stream-hopper

# Source files
# Correctly find all .cpp files in src and any of its subdirectories.
SRCS = $(shell find src -type f -name '*.cpp')

# Object files
# This replaces the 'src/' prefix with 'build/' and the '.cpp' suffix with '.o'
OBJS = $(SRCS:src/%.cpp=build/%.o)

# Helper Scripts
SCRIPTS = api_helper.sh

# Default target
all: $(TARGET)

# Rule to link the executable
$(TARGET): $(OBJS)
	@echo "==> Linking target: $@"
	$(CXX) $(LDFLAGS) $(OBJS) -o $@ $(LDLIBS)
	@echo "==> Preparing helper scripts..."
	@cp $(SCRIPTS) build/
	@chmod +x build/$(SCRIPTS)


# Pattern rule to compile .cpp files from src/ and its subdirectories
build/%.o: src/%.cpp
	@echo "==> Compiling: $<"
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Phony targets
clean:
	@echo "==> Cleaning build directory"
	rm -rf build

.PHONY: all clean
