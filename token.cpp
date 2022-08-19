#include "token.hpp"
#include "diagnostics.hpp"

namespace Porkchop {

[[nodiscard]] constexpr bool isIdentifierStart(char ch) noexcept {
    return 'A' <= ch && ch <= 'Z' || 'a' <= ch && ch <= 'z' || ch == '_' || ch == '$';
}
[[nodiscard]] constexpr bool isNumberStart(char ch) noexcept {
    return '0' <= ch && ch <= '9';
}
[[nodiscard]] constexpr bool isIdentifierContinue(char ch) noexcept {
    return isIdentifierStart(ch) || isNumberStart(ch);
}

[[nodiscard]] constexpr bool isBinary(char ch) noexcept { return ch == '0' || ch == '1'; }
[[nodiscard]] constexpr bool isOctal(char ch) noexcept { return ch >= '0' && ch <= '7'; }
[[nodiscard]] constexpr bool isDecimal(char ch) noexcept { return ch >= '0' && ch <= '9'; }
[[nodiscard]] constexpr bool isHexadecimal(char ch) noexcept { return ch >= '0' && ch <= '9' || ch >= 'a' && ch <= 'f' || ch >= 'A' && ch <= 'F'; }

const std::unordered_map<std::string_view, TokenType> KEYWORDS {
    {"false", TokenType::KW_FALSE},
    {"true", TokenType::KW_TRUE},
    {"__LINE__", TokenType::KW_LINE},
    {"EOF", TokenType::KW_EOF},
    {"nan", TokenType::KW_NAN},
    {"inf", TokenType::KW_INF},
    {"while", TokenType::KW_WHILE},
    {"if", TokenType::KW_IF},
    {"else", TokenType::KW_ELSE},
    {"try", TokenType::KW_TRY},
    {"catch", TokenType::KW_CATCH},
    {"for", TokenType::KW_FOR},
    {"fn", TokenType::KW_FN},
    {"throw", TokenType::KW_THROW},
    {"break", TokenType::KW_BREAK},
    {"return", TokenType::KW_RETURN},
    {"yield", TokenType::KW_YIELD},
    {"as", TokenType::KW_AS},
    {"is", TokenType::KW_IS},
    {"default", TokenType::KW_DEFAULT},
    {"let", TokenType::KW_LET},
    {"in", TokenType::KW_IN}
};

const std::unordered_map<std::string_view, TokenType> OPERATORS {
    {"=", TokenType::OP_ASSIGN},
    {"&=", TokenType::OP_ASSIGN_AND},
    {"^=", TokenType::OP_ASSIGN_XOR},
    {"|=", TokenType::OP_ASSIGN_OR},
    {"<<=", TokenType::OP_ASSIGN_SHL},
    {">>=", TokenType::OP_ASSIGN_SHR},
    {">>>=", TokenType::OP_ASSIGN_USHR},
    {"+=", TokenType::OP_ASSIGN_ADD},
    {"-=", TokenType::OP_ASSIGN_SUB},
    {"*=", TokenType::OP_ASSIGN_MUL},
    {"/=", TokenType::OP_ASSIGN_DIV},
    {"%=", TokenType::OP_ASSIGN_REM},
    {"&&", TokenType::OP_LAND},
    {"||", TokenType::OP_LOR},
    {"&", TokenType::OP_AND},
    {"^", TokenType::OP_XOR},
    {"|", TokenType::OP_OR},
    {"==", TokenType::OP_EQ},
    {"!=", TokenType::OP_NEQ},
    {"<", TokenType::OP_LT},
    {">", TokenType::OP_GT},
    {"<=", TokenType::OP_LE},
    {">=", TokenType::OP_GE},
    {"<<", TokenType::OP_SHL},
    {">>", TokenType::OP_SHR},
    {">>>", TokenType::OP_USHR},
    {"+", TokenType::OP_ADD},
    {"-", TokenType::OP_SUB},
    {"*", TokenType::OP_MUL},
    {"/", TokenType::OP_DIV},
    {"%", TokenType::OP_REM},
    {"!", TokenType::OP_NOT},
    {"~", TokenType::OP_INV},
    {",", TokenType::OP_COMMA},
    {"@[", TokenType::AT_BRACKET},
    {".", TokenType::OP_DOT},
    {":", TokenType::OP_COLON}
    // unused single ASCII characters: ` ? @
};

struct Tokenizer {
    const char *p, *q, *const r;
    const size_t line;
    size_t column;

    Tokenizer(std::string_view view, size_t line) noexcept:
            p(view.begin()), q(p),
            r(view.end()),
            line(line), column(0) {}

    char getc() noexcept {
        if (remains()) [[likely]] {
            ++column;
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
                --column;
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
        return {.line = line, .column = column - (q - p), .width = size_t(q - p), .type = type};
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
                    if (isIdentifierStart(ch)) {
                        while (isIdentifierContinue(ch = getc()));
                        ungetc(ch);
                        std::string_view token{p, q};
                        if (auto it = KEYWORDS.find(token); it != KEYWORDS.end()) {
                            tokens.push_back(make(it->second));
                        } else {
                            tokens.push_back(make(TokenType::IDENTIFIER));
                        }
                    } else if (isNumberStart(ch) || (ch == '+' || ch == '-') && isNumberStart(peekc())) {
                        ungetc(ch);
                        tokens.push_back(scanNumber());
                    } else {
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
                            column += op->first.length();
                            tokens.push_back(make(op->second));
                        } else {
                            throw TokenException("invalid character", make(TokenType::INVALID));
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
        if (isIdentifierContinue(peekc())) throw TokenException("invalid number suffix", make(TokenType::INVALID));
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

std::string_view SourceCode::source(Token token) const noexcept {
    return lines.at(token.line).substr(token.column, token.width);
}

}
