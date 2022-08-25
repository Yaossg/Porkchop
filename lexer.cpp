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

    std::vector<Token> tokens;

    Tokenizer(std::string_view view, size_t line):
            o(view.begin()), p(o), q(p), r(view.end()),
            line(line) { tokenize(); }

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

    void add(TokenType type) {
        tokens.push_back(make(type));
    }

    void raise(const char* msg) const {
        throw TokenException(msg, make(TokenType::INVALID));
    }

    void tokenize() {
        while (remains()) {
            switch (char ch = getc()) {
                [[unlikely]] case '\xFF':
                [[unlikely]] case '\xFE': // BOM
                [[unlikely]] case '\0':   // 0 paddings
                    raise("sourcecode is expected to be encoded with UTF-8");
                case '\\': {
                    if (remains()) {
                        raise("stray '\\'");
                    } else {
                        return;
                    }
                }
                [[unlikely]] case '\n':
                [[unlikely]] case '\r':
                    unreachable("a newline within a line");
                [[unlikely]] case '\v':
                [[unlikely]] case '\f':
                [[unlikely]] case '\t':
                    raise("whitespaces other than space are not allowed");
                case ' ':
                    break;
                case *"'":
                    scan<*"'">("unterminated character literal");
                    add(TokenType::CHARACTER_LITERAL);
                    break;
                case '"':
                    scan<'"'>("unterminated string literal");
                    add(TokenType::STRING_LITERAL);
                    break;
                default: {
                    ungetc(ch);
                    if (isNumberStart(ch) || (ch == '+' || ch == '-') && isNumberStart(peekc())) {
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
        add(TokenType::LINEBREAK);
    }

    void addId() {
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

    void addPunct() {
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

    void scanDigits(bool pred(char)) {
        char ch = getc();
        if (!pred(ch)) raise("invalid number literal");
        do ch = getc();
        while (ch == '_' || pred(ch));
        ungetc(ch);
        if (q[-1] == '_') raise("invalid number literal");
    }

    void addNumber() {
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
            }
        }
        add(type);
    }

    template<char QUOTER>
    void scan(const char* message) {
        while (char ch = getc()) {
            switch (ch) {
                case QUOTER: return;
                case '\\': getc();
            }
        }
        raise(message);
    }
};

[[nodiscard]] std::vector<Token> tokenize(std::string_view view, size_t line) {
    return Tokenizer(view, line).tokens;
}

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
        for (auto&& token : Porkchop::tokenize(lines[line], line)) {
            tokens.push_back(token);
        }
    }
}

}