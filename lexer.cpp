#include "lexer.hpp"
#include "diagnostics.hpp"
#include "unicode/unicode.hpp"

namespace Porkchop {

[[nodiscard]] constexpr bool isBinary(char ch) noexcept {
    return ch == '0' || ch == '1';
}

[[nodiscard]] constexpr bool isOctal(char ch) noexcept {
    return ch >= '0' && ch <= '7';
}

[[nodiscard]] constexpr bool isDecimal(char ch) noexcept {
    return ch >= '0' && ch <= '9';
}

[[nodiscard]] constexpr bool isHexadecimal(char ch) noexcept {
    return ch >= '0' && ch <= '9' || ch >= 'a' && ch <= 'f' || ch >= 'A' && ch <= 'F';
}

[[nodiscard]] constexpr bool isNumberStart(char ch) noexcept {
    return isDecimal(ch);
}

// not a punctuation: _ #
// unused: ` ?
[[nodiscard]] constexpr bool isPunctuation(char ch) {
    return ch == '!' || ch == '"' || ch == '@' || '$' <= ch && ch <= '/' || ':' <= ch && ch <= '>' || '[' <= ch && ch <= '^' || '{' <= ch && ch <= '~';
}

[[nodiscard]] bool isIdentifierStart(char32_t ch) {
    return ch == '_' || isUnicodeIdentifierStart(ch);
}

[[nodiscard]] bool isIdentifierPart(char32_t ch) {
    return isUnicodeIdentifierPart(ch); // contains '_' already
}

void LineTokenizer::addLBrace(Source::BraceType braceType) {
    add(TokenType::LBRACE);
    context.braces.push_back(braceType);
}

Source::BraceType LineTokenizer::addRBrace() {
    add(TokenType::RBRACE);
    if (context.braces.empty()) {
        throw TokenException("stray '}' without '{' to match", context.tokens.back());
    }
    auto type = context.braces.back();
    context.braces.pop_back();
    return type;
}

void LineTokenizer::raise(const char *msg) const {
    throw TokenException(msg, make(TokenType::INVALID));
}

void LineTokenizer::tokenize() {
    while (remains()) {
        switch (char ch = getc()) {
            [[unlikely]] case '\xFF':
            [[unlikely]] case '\xFE': // BOM
            [[unlikely]] case '\0':   // 0 paddings
                raise("sourcecode of Porkchop is required to be encoded with UTF-8");
            case '\\': {
                if (remains()) {
                    raise("stray '\\'");
                } else {
                    return;
                }
            }
            [[unlikely]] case '\n':
            [[unlikely]] case '\r':
            unreachable();
            [[unlikely]] case '\v':
            [[unlikely]] case '\f':
            [[unlikely]] case '\t':
                raise("whitespaces other than spaces are not allowed");
            case ' ':
                break;
            case *"'":
                addChar();
                break;
            case '"':
                addString(true);
                break;
            case '{':
                addLBrace(Source::BraceType::CODE);
                break;
            case '}':
                if (addRBrace() == Source::BraceType::STRING) {
                    step();
                    addString(false);
                }
                break;
            default: {
                ungetc(ch);
                if (isNumberStart(ch)) {
                    addNumber();
                } else if (isPunctuation(ch)) {
                    addPunct();
                } else {
                    addId();
                }
            }
        }
        step();
    }
    if (context.tokens.back().type != TokenType::LINEBREAK)
        add(TokenType::LINEBREAK);
}

void LineTokenizer::addId() {
    std::string_view remains{p, r};
    UnicodeParser up(remains, line, column());
    if (!isIdentifierStart(up.decodeUnicode())) {
        raise("invalid character");
    }
    do q = up.q;
    while (up.remains() && isIdentifierPart(up.decodeUnicode()));
    std::string_view token{p, q};
    if (auto it = KEYWORDS.find(token); it != KEYWORDS.end()) {
        add(it->second);
    } else {
        add(TokenType::IDENTIFIER);
    }
}

void LineTokenizer::addPunct() {
    std::string_view remains{p, r};
    auto punct = PUNCTUATIONS.end();
    for (auto it = PUNCTUATIONS.begin(); it != PUNCTUATIONS.end(); ++it) {
        if (remains.starts_with(it->first)
            && (punct == PUNCTUATIONS.end() || it->first.length() > punct->first.length())) {
            punct = it;
        }
    }
    if (punct != PUNCTUATIONS.end()) {
        q += punct->first.length();
        add(punct->second);
    } else {
        raise("invalid punctuation");
    }
}

void LineTokenizer::scanDigits(bool (*pred)(char) noexcept) {
    char ch = getc();
    if (!pred(ch)) raise("invalid number literal");
    do ch = getc();
    while (ch == '_' || pred(ch));
    ungetc(ch);
    if (q[-1] == '_') raise("invalid number literal");
}

void LineTokenizer::addNumber() {
    // scan number prefix
    TokenType base = TokenType::DECIMAL_INTEGER;
    bool (*pred)(char) noexcept = isDecimal;
    if (peekc() == '0') {
        getc();
        switch (char ch = peekc()) {
            case 'x': case 'X':
                base = TokenType::HEXADECIMAL_INTEGER; pred = isHexadecimal; getc(); break;
            [[unlikely]] case 'o':
            [[unlikely]] case 'O':
                base = TokenType::OCTAL_INTEGER; pred = isOctal; getc(); break;
            case 'b': case 'B':
                base = TokenType::BINARY_INTEGER; pred = isBinary; getc(); break;
            default:
                if (isNumberStart(ch)) {
                    raise("redundant 0 ahead is forbidden to avoid ambiguity, use 0o if octal");
                } else {
                    ungetc('0');
                }
        }
    }
    // scan digits
    scanDigits(pred);
    bool flt = false;
    if (peekc() == '.') {
        getc();
        if (pred(peekc())) {
            scanDigits(pred);
            flt = true;
        } else {
            ungetc('.');
        }
    }
    if (base == TokenType::DECIMAL_INTEGER && (peekc() == 'e' || peekc() == 'E')
        || base == TokenType::HEXADECIMAL_INTEGER && (peekc() == 'p' || peekc() == 'P')) {
        flt = true;
        getc();
        if (peekc() == '+' || peekc() == '-') getc();
        scanDigits(isDecimal);
    }
    // classification
    TokenType type = base;
    if (flt) {
        switch (base) {
            [[unlikely]] case TokenType::BINARY_INTEGER:
            [[unlikely]] case TokenType::OCTAL_INTEGER:
                raise("binary or octal float literal is invalid");
            [[likely]] case TokenType::DECIMAL_INTEGER:
            case TokenType::HEXADECIMAL_INTEGER:
                type = TokenType::FLOATING_POINT;
                break;
        }
    }
    add(type);
}

void LineTokenizer::addChar() {
    while (char ch = getc()) {
        switch (ch) {
            case *"'":
                add(TokenType::CHARACTER_LITERAL);
                return;
            case '\\':
                getc();
                break;
        }
    }
    raise("unterminated character literal");
}

void LineTokenizer::addString(bool first) {
    while (char ch = getc()) {
        switch (ch) {
            case '"':
                add(first ? TokenType::STRING_QQ : TokenType::STRING_Q);
                return;
            case '\\':
                getc();
                break;
            case '$': {
                add(first ? TokenType::STRING_QD : TokenType::STRING_D);
                step();
                if (peekc() == '{') {
                    getc();
                    addLBrace(Source::BraceType::STRING);
                    return;
                } else {
                    addId();
                    step();
                    first = false;
                }
            }
        }
    }
    raise("unterminated string literal");
}

int64_t parseInt(Source& source, Token token) try {
    int base;
    switch (token.type) {
        case TokenType::BINARY_INTEGER: base = 2; break;
        case TokenType::OCTAL_INTEGER: base = 8; break;
        case TokenType::DECIMAL_INTEGER: base = 10; break;
        case TokenType::HEXADECIMAL_INTEGER: base = 16; break;
        default: unreachable();
    }
    std::string literal(source.of(token));
    std::erase(literal, '_');
    if (base != 10) {
        literal.erase(literal.front() == '+' || literal.front() == '-', 2);
    }
    return std::stoll(literal, nullptr, base);
} catch (std::out_of_range& e) {
    throw ConstException("int literal out of range", token);
}

double parseFloat(Source& source, Token token) try {
    std::string literal(source.of(token));
    std::erase(literal, '_');
    return std::stod(literal);
} catch (std::out_of_range& e) {
    throw ConstException("float literal out of range", token);
}

char32_t parseChar(Source& source, Token token) {
    return UnicodeParser(source.of(token), token).unquoteChar(token);
}

std::string parseString(Source& source, Token token) {
    bool skip = token.type == TokenType::STRING_QQ || token.type == TokenType::STRING_QD;
    bool stop = token.type == TokenType::STRING_QQ || token.type == TokenType::STRING_Q;
    return UnicodeParser(source.of(token), token).unquoteString(skip, stop ? '"' : '$');
}

}