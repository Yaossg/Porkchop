#include "token.hpp"
#include "diagnostics.hpp"
#include "unicode.hpp"

namespace Porkchop {

[[nodiscard]] constexpr bool isNumberStart(char ch) noexcept {
    return '0' <= ch && ch <= '9';
}

[[nodiscard]] constexpr bool isBinary(char ch) noexcept { return ch == '0' || ch == '1'; }
[[nodiscard]] constexpr bool isOctal(char ch) noexcept { return ch >= '0' && ch <= '7'; }
[[nodiscard]] constexpr bool isDecimal(char ch) noexcept { return ch >= '0' && ch <= '9'; }
[[nodiscard]] constexpr bool isHexadecimal(char ch) noexcept { return ch >= '0' && ch <= '9' || ch >= 'a' && ch <= 'f' || ch >= 'A' && ch <= 'F'; }

// not a punctuation: ` ? $ _ #
// unused: ` ? $
[[nodiscard]] constexpr bool isPunctuation(char ch) {
    return ch == '!' || ch == '"' || ch == '@' || '%' <= ch && ch <= '/' || ':' <= ch && ch <= '>' || '[' <= ch && ch <= '^' || '{' <= ch && ch <= '~';
}

[[nodiscard]] bool isIdentifierStart(char32_t ch) {
    return ch == '_' || isUnicodeIdentifierStart(ch);
}

[[nodiscard]] bool isIdentifierPart(char32_t ch) {
    return isUnicodeIdentifierPart(ch); // contains '_' already
}

struct Tokenizer {
    const char *const o, *p, *q, *const r;
    const size_t line;

    Tokenizer(std::string_view view, size_t line) noexcept:
            o(view.begin()), p(o), q(p), r(view.end()),
            line(line) {}

    [[nodiscard]] size_t column() const noexcept {
        return q - o;
    }

    char getc() noexcept {
        if (remains()) [[likely]] {
            return *q++;
        } else [[unlikely]] {
            return 0;
        }
    }
    void ungetc(char ch) {
        if (ch) [[likely]] {
            if (p == q) [[unlikely]] {
                unreachable("ungetc() could only rewind within a single token");
            } else [[likely]] {
                --q;
            }
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
    [[nodiscard]] Token make(TokenType type) const noexcept {
        return {.line = line, .column = size_t(p - o), .width = size_t(q - p), .type = type};
    }
    [[nodiscard]] std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (remains()) {
            switch (char ch = getc()) {
                [[unlikely]] case '\\': {
                    if (remains()) {
                        throw TokenException("stray '\\'", make(TokenType::INVALID));
                    } else {
                        return tokens;
                    }
                }
                    [[unlikely]] case '\v':
                    [[unlikely]] case '\f':
                    [[unlikely]] case '\r':
                    [[unlikely]] case '\n':

                case ' ':
                case '\t':
                    break;
                case *"'":
                    scan<*"'">("unterminated character literal");
                    tokens.push_back(make(TokenType::CHARACTER_LITERAL));
                    break;
                case '"':
                    scan<'"'>("unterminated string literal");
                    tokens.push_back(make(TokenType::STRING_LITERAL));
                    break;
                case ';':
                    tokens.push_back(make(TokenType::LINEBREAK));
                    break;
                case '(':
                    tokens.push_back(make(TokenType::LPAREN));
                    break;
                case ')':
                    tokens.push_back(make(TokenType::RPAREN));
                    break;
                case '[':
                    tokens.push_back(make(TokenType::LBRACKET));
                    break;
                case ']':
                    tokens.push_back(make(TokenType::RBRACKET));
                    break;
                case '{':
                    tokens.push_back(make(TokenType::LBRACE));
                    break;
                case '}':
                    tokens.push_back(make(TokenType::RBRACE));
                    break;
                default: {
                    if (isNumberStart(ch) || (ch == '+' || ch == '-') && isNumberStart(peekc())) {
                        ungetc(ch);
                        tokens.push_back(scanNumber());
                    } else if (isPunctuation(ch)) {
                        ungetc(ch);
                        std::string_view remains{p, r};
                        auto op = OPERATORS.end();
                        for (auto it = OPERATORS.begin(); it != OPERATORS.end(); ++it) {
                            if (remains.starts_with(it->first)
                                && (op == OPERATORS.end() || it->first.length() > op->first.length())) {
                                op = it;
                            }
                        }
                        if (op != OPERATORS.end()) {
                            q += op->first.length();
                            tokens.push_back(make(op->second));
                        } else {
                            throw TokenException("invalid punctuation", make(TokenType::INVALID));
                        }
                    } else {
                        ungetc(ch);
                        std::string_view remains{p, r};
                        UnicodeParser up(remains, line, column());
                        if (!isIdentifierStart(up.decodeUnicode())) {
                            throw TokenException("identifier start is expected", make(TokenType::INVALID));
                        }
                        q = up.q;
                        while (up.remains()) {
                            if (isIdentifierPart(up.decodeUnicode())) {
                                q = up.q;
                            } else {
                                break;
                            }
                        }
                        std::string_view token{p, q};
                        if (auto it = KEYWORDS.find(token); it != KEYWORDS.end()) {
                            tokens.push_back(make(it->second));
                        } else {
                            tokens.push_back(make(TokenType::IDENTIFIER));
                        }
                    }
                }
            }
            step();
        }
        tokens.push_back(make(TokenType::LINEBREAK));
        return tokens;
    }

    void scanDigits(bool pred(char)) {
        char ch = getc();
        if (!pred(ch)) throw TokenException("invalid number literal", make(TokenType::INVALID));
        do ch = getc();
        while (ch == '_' || pred(ch));
        ungetc(ch);
        if (q[-1] == '_') throw TokenException("invalid number literal", make(TokenType::INVALID));
    }

    Token scanNumber() {
        if (peekc() == '+' || peekc() == '-') getc();
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
                        throw TokenException("redundant 0 ahead is forbidden to avoid ambiguity, use 0o if octal", make(TokenType::INVALID));
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
        if (!isPunctuation(peekc())) // neither a number nor an operator, identifier is the only possible case
            throw TokenException("invalid number suffix", make(TokenType::INVALID));
        // classification
        TokenType type = base;
        if (flt) {
            switch (base) {
                [[unlikely]] case TokenType::BINARY_INTEGER:
                    [[unlikely]] case TokenType::OCTAL_INTEGER:
                    throw TokenException("binary or octal float literal is invalid", make(TokenType::INVALID));
                    [[likely]] case TokenType::DECIMAL_INTEGER:
                case TokenType::HEXADECIMAL_INTEGER:
                    type = TokenType::FLOATING_POINT;
            }
        }
        return make(type);
    }

    template<char QUOTER>
    void scan(const char* message) {
        while (char ch = getc()) {
            switch (ch) {
                case QUOTER: return;
                case '\\': getc();
            }
        }
        throw TokenException(message, make(TokenType::INVALID));
    }
};

void SourceCode::split() {
    std::string_view view(original);
    const char *p = view.begin(), *q = p, *r = view.end();
    while (q != r) {
        switch (*q++) {
            [[unlikely]] case '#': {
                lines.emplace_back(p, q - 1);
                while (q != r && *q++ != '\n');
                p = q;
                break;
            }
                [[unlikely]] case '\n': {
                lines.emplace_back(p, q - 1);
                p = q;
                break;
            }
        }
    }
    if (p != q) lines.emplace_back(p, q);
}

void SourceCode::tokenize() {
    for (size_t line = 0; line < lines.size(); ++line) {
        if (std::string_view view = lines[line]; !view.empty()) {
            Tokenizer tokenizer(view, line);
            for (auto&& token : tokenizer.tokenize()) {
                tokens.push_back(token);
            }
        }
    }
}

}