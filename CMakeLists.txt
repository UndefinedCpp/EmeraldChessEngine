cmake_minimum_required(VERSION 3.10)
project(EmeraldChessEngine CXX)

# Use C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use g++
set(CMAKE_CXX_COMPILER g++)

# Collect all .cpp files from src
file(GLOB SRC_FILES src/*.cpp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -mavx2 -mfma -march=core-avx2 -static")

# Output to build/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)

add_executable(engine ${SRC_FILES})