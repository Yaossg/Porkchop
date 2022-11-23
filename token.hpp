#pragma once

#include <unordered_map>
#include <string_view>
#include <vector>

namespace Porkchop {

enum class TokenType {
    INVALID,

    IDENTIFIER,
    KW_FALSE,
    KW_TRUE,
    KW_LINE,
    KW_EOF,
    KW_NAN,
    KW_INF,
    KW_WHILE,
    KW_IF,
    KW_ELSE,
    KW_FOR,
    KW_FN,
    KW_BREAK,
    KW_RETURN,
    KW_YIELD,
    KW_AS,
    KW_IS,
    KW_DEFAULT,
    KW_LET,
    KW_IN,

    LPAREN,
    RPAREN,
    LBRACKET,
    RBRACKET,
    LBRACE,
    RBRACE,

    OP_ASSIGN,
    OP_ASSIGN_AND,
    OP_ASSIGN_XOR,
    OP_ASSIGN_OR,
    OP_ASSIGN_SHL,
    OP_ASSIGN_SHR,
    OP_ASSIGN_USHR,
    OP_ASSIGN_ADD,
    OP_ASSIGN_SUB,
    OP_ASSIGN_MUL,
    OP_ASSIGN_DIV,
    OP_ASSIGN_REM,
    OP_LOR,
    OP_LAND,
    OP_OR,
    OP_XOR,
    OP_AND,
    OP_EQ,
    OP_NE,
    OP_LT,
    OP_GT,
    OP_LE,
    OP_GE,
    OP_SHL,
    OP_SHR,
    OP_USHR,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_REM,
    OP_NOT,
    OP_INV,
    OP_COMMA,
    OP_DOLLAR,
    OP_INC,
    OP_DEC,
    AT_BRACKET,
    OP_DOT,
    OP_COLON,

    CHARACTER_LITERAL,
    STRING_LITERAL,
    BINARY_INTEGER,
    OCTAL_INTEGER,
    DECIMAL_INTEGER,
    HEXADECIMAL_INTEGER,
    FLOATING_POINT,

    LINEBREAK
};

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
    {"for", TokenType::KW_FOR},
    {"fn", TokenType::KW_FN},
    {"break", TokenType::KW_BREAK},
    {"return", TokenType::KW_RETURN},
    {"yield", TokenType::KW_YIELD},
    {"as", TokenType::KW_AS},
    {"is", TokenType::KW_IS},
    {"default", TokenType::KW_DEFAULT},
    {"let", TokenType::KW_LET},
    {"in", TokenType::KW_IN}
};

const std::unordered_map<std::string_view, TokenType> PUNCTUATIONS {
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
    {"!=", TokenType::OP_NE},
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
    {":", TokenType::OP_COLON},
    {";", TokenType::LINEBREAK},
    {"(", TokenType::LPAREN},
    {")", TokenType::RPAREN},
    {"[", TokenType::LBRACKET},
    {"]", TokenType::RBRACKET},
    {"{", TokenType::LBRACE},
    {"}", TokenType::RBRACE},
    {"$", TokenType::OP_DOLLAR},
    {"++", TokenType::OP_INC},
    {"--", TokenType::OP_DEC}
};

struct Token {
    size_t line, column, width;
    TokenType type;

    operator struct Segment() const noexcept;
};

struct Segment {
    size_t line1, line2, column1, column2;
};

inline Segment range(Token from, Token to) noexcept {
    return {.line1 = from.line, .line2 = to.line, .column1 = from.column, .column2 = to.column + to.width};
}

inline Token::operator Segment() const noexcept {
    return range(*this, *this);
}

inline Segment range(Segment from, Segment to) noexcept {
    return {.line1 = from.line1, .line2 = to.line2, .column1 = from.column1, .column2 = to.column2};
}

}