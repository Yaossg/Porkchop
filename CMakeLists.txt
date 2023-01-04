cmake_minimum_required(VERSION 3.21)
project(Porkchop)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static")

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
        opcode.hpp assembler.hpp text-assembler.hpp
        util.hpp
        descriptor.hpp
        common.hpp
        bin-assembler.hpp)

add_executable(PorkchopRuntime
        runtime/assembly.hpp
        runtime/external.hpp runtime/external.cpp
        runtime/frame.hpp
        runtime/text-assembly.hpp runtime/bin-assembly.hpp
        runtime/vm.hpp runtime/vm.cpp
        opcode.hpp
        util.hpp
        type.hpp
        descriptor.hpp
        runtime/common.hpp
        unicode/unicode.hpp unicode/unicode.cpp
        runtime/main.cpp
)

add_executable(PorkchopInterpreter
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
        util.hpp
        descriptor.hpp

        runtime/assembly.hpp
        runtime/external.hpp runtime/external.cpp
        runtime/frame.hpp
        runtime/vm.hpp runtime/vm.cpp

        runtime/interpreter.cpp runtime/interpretation.hpp
        runtime/common.hpp common.hpp
        unicode/unicode.hpp unicode/unicode.cpp
)

