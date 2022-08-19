#include "literal.hpp"
#include "diagnostics.hpp"

#include <charconv>

namespace Porkchop {

int64_t parseInt(SourceCode &sourcecode, Token token) try {
    int base;
    switch (token.type) {
        case TokenType::BINARY_INTEGER: base = 2; break;
        case TokenType::OCTAL_INTEGER: base = 8; break;
        case TokenType::DECIMAL_INTEGER: base = 10; break;
        case TokenType::HEXADECIMAL_INTEGER: base = 16; break;
    }
    std::string literal; // default-constructed
    literal = sourcecode.source(token);
    std::erase(literal, '_');
    if (base != 10) {
        literal.erase(literal.front() == '+' || literal.front() == '-', 2);
    }
    return std::stoll(literal, nullptr, base);
} catch (std::out_of_range& e) {
    throw ConstException("int literal out of range", token);
}

double parseFloat(SourceCode &sourcecode, Token token) try {
    std::string literal; // default-constructed
    literal = sourcecode.source(token);
    std::erase(literal, '_');
    return std::stod(literal);
} catch (std::out_of_range& e) {
    throw ConstException("float literal out of range", token);
}

[[nodiscard]] bool isSurrogate(char32_t ch) {
    return 0xD800 <= ch && ch <= 0xDFFF;
}
constexpr char32_t ASCII_UPPER_BOUND = 0x7F;
constexpr char32_t UNICODE_UPPER_BOUND = 0x10FFFF;

char32_t hex(const char* first, const char* last, unsigned bound, Segment segment) {
    unsigned value = 0;
    auto [ptr, ec] = std::from_chars(first, last, value, 16);
    if (ptr != last) {
        throw ConstException("the escape sequence expect exact " + std::to_string(std::distance(first, last)) + " hex digits", segment);
    }
    if (value > bound) {
        ec = std::errc::result_out_of_range;
    }
    if (ec == std::errc{}) {
        if (isSurrogate(value)) {
            throw ConstException("the hex value represents a surrogate", segment);
        } else {
            return value;
        }
    }
    if (ec == std::errc::invalid_argument) {
        throw ConstException("the hex value is invalid", segment);
    }
    if (ec == std::errc::result_out_of_range) {
        throw ConstException("the hex value is out of valid range", segment);
    }
    unreachable("hex escape sequence");
}

[[nodiscard]] constexpr int countl_one(char8_t ch) {
    return std::countl_one((unsigned char) ch);
}

[[nodiscard]] bool notUTF8Continue(char8_t byte) {
    return countl_one(byte) - 1;
}

std::string encodeUnicode(char32_t unicode) {
    if (unicode <= ASCII_UPPER_BOUND) { // ASCII
        return {char(unicode)};
    } else if (unicode <= 0x7FF) { // 2 bytes
        return {char((unicode >> 6) | 0xC0),
                char((unicode & 0x3F) | 0x80)};
    } else if (unicode <= 0xFFFF) { // 3 bytes
        return {char((unicode >> 12) | 0xE0),
                char(((unicode >> 6) & 0x3F) | 0x80),
                char((unicode & 0x3F) | 0x80)};
    } else if (unicode <= UNICODE_UPPER_BOUND) { // 4 bytes
        return {char((unicode >> 18) | 0xF0),
                char(((unicode >> 12) & 0x3F) | 0x80),
                char(((unicode >> 6) & 0x3F) | 0x80),
                char((unicode & 0x3F) | 0x80)};
    }
    unreachable("invalid unicode");
}

struct UnicodeLiteralParser {
    const char *p, *q, *const r;
    const size_t line;
    size_t column;
    Token token;

    UnicodeLiteralParser(std::string_view view, Token token) noexcept:
        p(view.begin()), q(p),
        r(view.end()),
        line(token.line), column(token.column), token(token) {}

    char getc() {
        if (remains()) [[likely]] {
            ++column;
            return *q++;
        } else [[unlikely]] {
            throw ConstException("unexpected termination of literal", make());
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

    char32_t parseHexASCII() {
        const char *first = q;
        getc(); getc();
        const char *last = q;
        return hex(first, last, ASCII_UPPER_BOUND, make());
    }

    char32_t parseHexUnicode() {
        const char *first = q;
        getc(); getc(); getc(); getc(); getc(); getc();
        const char *last = q;
        return hex(first, last, UNICODE_UPPER_BOUND, make());
    }

    [[nodiscard]] int UTF8Length(char8_t byte) const {
        switch(int ones = countl_one(byte)) {
            case 0:
                return 1;
            case 2:
            case 3:
            case 4:
                return ones;
            case 1:
                throw ConstException("unexpected termination of UTF-8 multibyte series", make());
            default:
                throw ConstException("UTF-8 series of 5 or more bytes is unsupported yet", make());
        }
    }

    void requireContinue(char8_t byte) const {
        if (notUTF8Continue(byte))
            throw ConstException("unexpected UTF-8 multibyte series termination", make());
    }

    char32_t decodeUnicode() {
        char32_t result;
        char8_t ch1 = getc();
        switch (UTF8Length(ch1)) {
            case 1:
                result = ch1;
                break;
            case 2: {
                char8_t ch2 = getc();
                requireContinue(ch2);
                result = ((ch1 & ~0xC0) << 6) | (ch2 & ~0x80);
            } break;
            case 3: {
                char8_t ch2 = getc();
                requireContinue(ch2);
                char8_t ch3 = getc();
                requireContinue(ch3);
                result = ((ch1 & ~0xE0) << 12) | ((ch2 & ~0x80) << 6) | (ch3 & ~0x80);
            } break;
            case 4: {
                char8_t ch2 = getc();
                requireContinue(ch2);
                char8_t ch3 = getc();
                requireContinue(ch3);
                char8_t ch4 = getc();
                requireContinue(ch4);
                result = ((ch1 & ~0xF0) << 18) | ((ch2 & ~0x80) << 12) | ((ch3 & ~0x80) << 6) | (ch4 & ~0x80);
            } break;
        }
        if (isSurrogate(result))
            throw ConstException("the value of UTF-8 series represents a surrogate", make());
        if (result > UNICODE_UPPER_BOUND)
            throw ConstException("the value of UTF-8 series exceeds upper bound of Unicode", make());
        return result;
    }

    char32_t unquoteChar() {
        getc(); step();
        char32_t result;
        char8_t ch1 = peekc();
        if (ch1 == '\\') {
            getc();
            switch (getc()) {
                case '\'': result = '\''; break;
                case '\"': result = '\"'; break;
                case '\\': result = '\\'; break;
                case '0': result = '\0'; break;
                case 'a': result = '\a'; break;
                case 'b': result = '\b'; break;
                case 'f': result = '\f'; break;
                case 'n': result = '\n'; break;
                case 'r': result = '\r'; break;
                case 't': result = '\t'; break;
                case 'v': result = '\v'; break;
                case 'x': { // ASCII hex
                    result = parseHexASCII();
                    break;
                }
                case 'u': { // Unicode hex
                    result = parseHexUnicode();
                    break;
                }
                default:
                    throw ConstException("unknown escape sequence", make());
            }
        } else if (ch1 == '\'') {
            throw ConstException("empty character literal", token);
        } else {
            result = decodeUnicode();
        }
        if (getc() != '\'') throw ConstException("multiple characters in the character literal", token);
        return result;
    }

    std::string unquoteString() {
        getc(); step();
        std::string result;
        char8_t ch1;
        while ((ch1 = getc()) != '"') {
            switch (UTF8Length(ch1)) {
                case 1:
                    if (ch1 == '\\') {
                        switch (getc()) {
                            case '\'': result += '\''; break;
                            case '\"': result += '\"'; break;
                            case '\\': result += '\\'; break;
                            case '0': result += '\0'; break;
                            case 'a': result += '\a'; break;
                            case 'b': result += '\b'; break;
                            case 'f': result += '\f'; break;
                            case 'n': result += '\n'; break;
                            case 'r': result += '\r'; break;
                            case 't': result += '\t'; break;
                            case 'v': result += '\v'; break;
                            case 'x': { // ASCII hex
                                result += parseHexASCII();
                                break;
                            }
                            case 'u': { // Unicode hex
                                result += encodeUnicode(parseHexUnicode());
                                break;
                            }
                            default:
                                throw ConstException("unknown escape sequence", make());
                        }
                    } else result += ch1;
                    break;
                case 2: {
                    char8_t ch2 = getc();
                    requireContinue(ch2);
                    result += ch1;
                    result += ch2;
                } break;
                case 3: {
                    char8_t ch2 = getc();
                    requireContinue(ch2);
                    char8_t ch3 = getc();
                    requireContinue(ch3);
                    result += ch1;
                    result += ch2;
                    result += ch3;
                } break;
                case 4: {
                    char8_t ch2 = getc();
                    requireContinue(ch2);
                    char8_t ch3 = getc();
                    requireContinue(ch3);
                    char8_t ch4 = getc();
                    requireContinue(ch4);
                    result += ch1;
                    result += ch2;
                    result += ch3;
                    result += ch4;
                } break;
            }
            step();
        }
        return result;
    }
};
char32_t parseChar(SourceCode& sourcecode, Token token) {
    return UnicodeLiteralParser(sourcecode.source(token), token).unquoteChar();
}
std::string parseString(SourceCode& sourcecode, Token token) {
    return UnicodeLiteralParser(sourcecode.source(token), token).unquoteString();
}

}
