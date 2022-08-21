#include "literal.hpp"
#include "diagnostics.hpp"
#include "unicode.hpp"

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

char32_t UnicodeParser::unquoteChar(Token token) {
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

std::string UnicodeParser::unquoteString() {
    getc(); step();
    std::string result;
    char8_t ch1;
    while ((ch1 = getc()) != '"') {
        switch (queryUTF8Length(ch1)) {
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

char32_t parseChar(SourceCode& sourcecode, Token token) {
    return UnicodeParser(sourcecode.source(token), token).unquoteChar(token);
}

std::string parseString(SourceCode& sourcecode, Token token) {
    return UnicodeParser(sourcecode.source(token), token).unquoteString();
}

}
