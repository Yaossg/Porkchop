#pragma once

#include "parser.hpp"

namespace Porkchop {

inline void parse(Compiler& compiler) try {
    compiler.tokenize();
    if (compiler.tokens.empty()) {
        fprintf(stderr, "Compilation Error: no tokens to compile\n");
        std::exit(-2);
    }
    compiler.parse();
} catch (Porkchop::SegmentException& e) {
    fprintf(stderr, "%s\n", e.message(compiler).c_str());
    std::exit(-1);
}

}