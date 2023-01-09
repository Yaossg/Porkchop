#pragma once

#include "lexer.hpp"
#include "parser.hpp"
#include "function.hpp"

namespace Porkchop {

inline void tokenize(Source& source, std::string original) try {
    source.append(std::move(original));
} catch (Porkchop::Error& e) {
    e.report(&source, true);
    std::exit(-3);
}

inline void parse(Compiler& compiler) try {
    compiler.parse(false);
} catch (Porkchop::Error& e) {
    e.report(&compiler.source, true);
    std::exit(-1);
}

}