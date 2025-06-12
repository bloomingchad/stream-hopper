# Makefile for the stream-hopper project
# Supports separate src, include, and build directories.
# USES nlohmann/json as a pre-compiled library for faster builds.

# Compiler and flags
CXX := g++
CXXFLAGS := -std=c++14 -g -Wall -Wextra -pthread

# --- Directories ---
TARGET_NAME := stream-hopper
SRCDIR := src
INCDIR := include
BUILDDIR := build

# --- Files ---
# Explicitly list all C++ source files to be compiled.
SRCS := $(SRCDIR)/radio.cpp \
        $(SRCDIR)/RadioPlayer.cpp \
        $(SRCDIR)/RadioStream.cpp \
        $(SRCDIR)/UIManager.cpp \
        $(SRCDIR)/json.cpp

OBJS := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SRCS))
TARGET := $(BUILDDIR)/$(TARGET_NAME)

# --- Include Paths & Libraries ---
# Add our own include directory to the include paths
CPPFLAGS := -I$(INCDIR)
CPPFLAGS += $(shell pkg-config --cflags mpv ncursesw)
LIBS := $(shell pkg-config --libs mpv ncursesw)

# --- Targets ---

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "==> Linking target: $@"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@ $(LIBS)
	@echo "==> Build complete: $(TARGET)"

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	@echo "==> Compiling: $<"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

.PHONY: clean all

clean:
	@echo "==> Cleaning build directory..."
	rm -rf $(BUILDDIR)
