# MIT License
#
# Copyright (c) 2025 Emerald Chess Engine
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

#=============================================================================#
#                     C o m p i l e r   a n d   F l a g s                     #
#=============================================================================#
CXX = g++
CXXFLAGS = -Wall -std=c++17 -O3
STATIC_LIBS = -static-libstdc++ -static-libgcc -static

#=============================================================================#
#                    S o u r c e s   a n d   T a r g e t s                    #
#=============================================================================#
SOURCE_DIR = ./src
BUILD_DIR = ./build
TARGET = engine
ARCH = native
VERSION = dbg

SRCS = $(wildcard $(SOURCE_DIR)/*.cpp) $(wildcard $(SOURCE_DIR)/*.c)
OBJS = $(patsubst $(SOURCE_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))

TARGET_EXE = $(BUILD_DIR)/$(TARGET).$(VERSION).$(ARCH).exe
CXXFLAGS += -march=$(ARCH)

#=============================================================================#
#                       C o m p i l e   a n d   L i n k                       #
#=============================================================================#

# Default target
all: $(TARGET_EXE)

# Build static executable
link-static: CXXFLAGS += $(STATIC_LIBS)
link-static: $(TARGET_EXE)


$(TARGET_EXE): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.c
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean target
clean:
	powershell -noprofile -command "rm -r $(BUILD_DIR)/*.o"
	powershell -noprofile -command "rm -r $(BUILD_DIR)/$(TARGET).$(VERSION).*.exe"

# Phony targets
.PHONY: all clean link-static