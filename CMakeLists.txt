cmake_minimum_required(VERSION 3.21)
project(Porkchop)

set(CMAKE_CXX_FLAGS "-static")

set(CMAKE_CXX_STANDARD 20)

add_executable(Porkchop
        main.cpp
        type.hpp
        compiler.hpp compiler.cpp
        local.hpp local.cpp
        token.hpp lexer.cpp
        parser.hpp parser.cpp
        tree.hpp tree.cpp
        diagnostics.hpp diagnostics.cpp
        unicode/unicode.hpp unicode/unicode.cpp
        unicode/unicode-id.cpp unicode/unicode-width.cpp
        function.hpp
        opcode.hpp assembler.hpp
        io.hpp)

add_executable(PorkchopRuntime
        runtime/main.cpp
        runtime/assembly.hpp
        runtime/external.hpp
        runtime/runtime.hpp
        runtime/text-asm.hpp
        opcode.hpp
        io.hpp
)

