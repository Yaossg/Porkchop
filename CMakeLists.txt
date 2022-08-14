cmake_minimum_required(VERSION 3.21)
project(Porkchop)

set(CMAKE_CXX_FLAGS "-static")

set(CMAKE_CXX_STANDARD 20)

add_executable(Porkchop main.cpp token.cpp sourcecode.hpp sourcecode.cpp parser.hpp parser.cpp token.hpp tree.hpp local.hpp tree.cpp type.hpp diagnostics.hpp diagnostics.cpp)
