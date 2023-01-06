#pragma once

#include "lexer.hpp"
#include "parser.hpp"
#include "function.hpp"

namespace Porkchop {

inline void tokenize(Source& source, std::string original) try {
    source.append(std::move(original));
} catch (Porkchop::SegmentException& e) {
    fprintf(stderr, "%s\n", e.message(source).c_str());
    std::exit(-3);
}

inline void parse(Compiler& compiler) try {
    if (compiler.source.tokens.empty()) {
        fprintf(stderr, "Compilation Error: no token to compile\n");
        std::exit(-2);
    }
    compiler.parse();
    if (auto R = compiler.continuum->functions.back()->prototype()->R; !isNone(R) && !isInt(R)) {
        throw ParserException("main clause should return either none or int", compiler.definition->clause->segment());
    }
} catch (Porkchop::SegmentException& e) {
    fprintf(stderr, "%s\n", e.message(compiler.source).c_str());
    std::exit(-1);
}

}