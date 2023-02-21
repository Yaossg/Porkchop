#pragma once

#include "../token.hpp"
#include "../util.hpp"

namespace Porkchop {

unsigned char getUnicodeWidth(char32_t ch) noexcept;
unsigned char getUnicodeID(char32_t ch) noexcept;
inline bool isUnicodeIdentifierStart(char32_t ch) {
    return getUnicodeID(ch) > 1;
}
inline bool isUnicodeIdentifierPart(char32_t ch) {
    return getUnicodeID(ch) > 0;
}

[[nodiscard]] inline bool isSurrogate(char32_t ch) {
    return 0xD800 <= ch && ch <= 0xDFFF;
}
constexpr char32_t ASCII_UPPER_BOUND = 0x7F;
constexpr char32_t UNICODE_UPPER_BOUND = 0x10FFFF;

char32_t parseHex(const char* first, const char* last, unsigned bound, Segment segment);

[[nodiscard]] inline constexpr int countl_one(char8_t ch) {
    return std::countl_one((unsigned char) ch);
}

[[nodiscard]] inline constexpr bool notUTF8Continue(char8_t byte) {
    return countl_one(byte) - 1;
}

[[nodiscard]] std::string encodeUnicode(char32_t unicode);

struct UnicodeParser {
    const char *p, *q, *const r;
    const size_t line;
    size_t column;

    UnicodeParser(std::string_view view, size_t line, size_t column) noexcept:
            p(view.begin()), q(p),
            r(view.end()),
            line(line), column(column) {}

    UnicodeParser(std::string_view view, Token token) noexcept:
            UnicodeParser(view, token.line, token.column) {}


    char getc() {
        if (remains()) {
            ++column;
            return *q++;
        } else {
            unreachable();
        }
    }
    [[nodiscard]] char peekc() const noexcept {
        return *q;
    }
    [[nodiscard]] bool remains() const noexcept {
        return q != r;
    }
    void step() noexcept {
        p = q;
    }
    [[nodiscard]] Segment make() const noexcept {
        return Token{.line = line, .column = column - (q - p), .width = size_t(q - p), .type = TokenType::INVALID};
    }

    char parseHexASCII();
    char32_t parseHexUnicode();
    [[nodiscard]] int successiveUTF8Length(char8_t byte) const;
    void requireUTF8Continue(char8_t byte) const;

    char32_t decodeUnicode();
    char32_t unquoteChar(Token token);
    std::string unquoteString(bool escape);
    [[noreturn]] void raise(const char* msg) const;
};


}