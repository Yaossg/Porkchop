#include "token.hpp"
#include "diagnostics.hpp"
#include "unicode/unicode.hpp"
#include "util.hpp"

namespace Porkchop {

[[nodiscard]] constexpr bool isBinary(char ch) noexcept { return ch == '0' || ch == '1'; }
[[nodiscard]] constexpr bool isOctal(char ch) noexcept { return ch >= '0' && ch <= '7'; }
[[nodiscard]] constexpr bool isDecimal(char ch) noexcept { return ch >= '0' && ch <= '9'; }
[[nodiscard]] constexpr bool isHexadecimal(char ch) noexcept { return ch >= '0' && ch <= '9' || ch >= 'a' && ch <= 'f' || ch >= 'A' && ch <= 'F'; }

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

struct LexContext {
    std::vector<Token> tokens;
    std::vector<Token> braces;
};

struct LineTokenizer {
    LexContext& context;
    const char *const o, *p, *q, *const r;
    const size_t line;

    LineTokenizer(LexContext& context,
                  std::string_view view, size_t line):
                  context(context),
                  o(view.begin()), p(o), q(p), r(view.end()),
                  line(line)
                  { tokenize(); }

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
        auto token = make(type);
        context.tokens.push_back(token);
        switch (type) {
            case TokenType::LPAREN:
            case TokenType::LBRACKET:
            case TokenType::LBRACE:
                context.braces.push_back(token);
                break;
            case TokenType::RPAREN:
                if (context.braces.empty()) {
                    throw TokenException("stray ')'", token);
                } else if (context.braces.back().type != TokenType::LPAREN) {
                    throw TokenException("mismatch braces, '(' is expected", range(context.braces.back(), token));
                } else {
                    context.braces.pop_back();
                }
                break;
            case TokenType::RBRACKET:
                if (context.braces.empty()) {
                    throw TokenException("stray ']'", token);
                } else if (context.braces.back().type != TokenType::LBRACKET) {
                    throw TokenException("mismatch braces, '[' is expected", range(context.braces.back(), token));
                } else {
                    context.braces.pop_back();
                }
                break;
            case TokenType::RBRACE:
                if (context.braces.empty()) {
                    throw TokenException("stray '}'", token);
                } else if (context.braces.back().type != TokenType::LBRACE) {
                    throw TokenException("mismatch braces, '{' is expected", range(context.braces.back(), token));
                } else {
                    context.braces.pop_back();
                }
                break;
        }
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


void Compiler::tokenize() {
    lines = Porkchop::splitLines(original);
    LexContext context;
    for (size_t line = 0; line < lines.size(); ++line) {
        LineTokenizer(context, lines[line], line);
    }
    if (!context.braces.empty()) {
        throw TokenException("open brace unclosed", context.braces.back());
    }
    tokens = std::move(context.tokens);
}

int64_t parseInt(Compiler &compiler, Token token) try {
    int base;
    switch (token.type) {
        case TokenType::BINARY_INTEGER: base = 2; break;
        case TokenType::OCTAL_INTEGER: base = 8; break;
        case TokenType::DECIMAL_INTEGER: base = 10; break;
        case TokenType::HEXADECIMAL_INTEGER: base = 16; break;
    }
    std::string literal; // default-constructed
    literal = compiler.of(token);
    std::erase(literal, '_');
    if (base != 10) {
        literal.erase(literal.front() == '+' || literal.front() == '-', 2);
    }
    return std::stoll(literal, nullptr, base);
} catch (std::out_of_range& e) {
    throw ConstException("int literal out of range", token);
}

double parseFloat(Compiler &compiler, Token token) try {
    std::string literal; // default-constructed
    literal = compiler.of(token);
    std::erase(literal, '_');
    return std::stod(literal);
} catch (std::out_of_range& e) {
    throw ConstException("float literal out of range", token);
}

char32_t parseChar(Compiler& compiler, Token token) {
    return UnicodeParser(compiler.of(token), token).unquoteChar(token);
}

std::string parseString(Compiler& compiler, Token token) {
    return UnicodeParser(compiler.of(token), token).unquoteString();
}

}