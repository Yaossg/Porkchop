#include "lexer.hpp"

namespace Porkchop {

std::string_view Source::of(Token token) const noexcept {
    return lines.at(token.line).substr(token.column, token.width);
}

void Source::append(std::string code) {
    snippets.push_back(std::move(code));
    for (auto line : splitLines(snippets.back())) {
        lines.push_back(line);
        LineTokenizer(*this, line);
    }
}

}