#include "lexer.hpp"
#include "source.hpp"
#include "unicode/unicode.hpp"


namespace Porkchop {

std::string_view Source::of(Token token) const noexcept {
    return lines.at(token.line).operator std::string_view().substr(token.column, token.width);
}

void Source::append(std::string const& code) {
    for (auto original : splitLines(code)) {
        std::string transformed;
        size_t width = 0;
        UnicodeParser parser(original, lines.size(), 0);
        while (parser.remains()) {
            char32_t ch = parser.decodeUnicode();
            if (ch == '\t') {
                size_t padding = 4 - (width & 3);
                transformed += std::string(padding, ' ');
                width += padding;
            } else {
                transformed += encodeUnicode(ch);
                width += getUnicodeWidth(ch);
            }
        }
        lines.emplace_back(std::move(transformed));
        LineTokenizer(*this, lines.back());
    }
}

bool Source::remains() {
    return !greedy.empty() || lines.back().ends_with('\\') || raw;
}

}