#pragma once

#include "lexer.hpp"
#include "parser.hpp"
#include "function.hpp"

namespace Porkchop {

inline void tokenize(Source& source, std::string const& original) try {
    source.append(original);
} catch (Porkchop::Error& e) {
    e.report(&source);
    std::exit(-3);
}

inline void parse(Compiler& compiler) try {
    compiler.parse(Compiler::Mode::MAIN);
} catch (Porkchop::Error& e) {
    e.report(&compiler.source);
    std::exit(-1);
}

}