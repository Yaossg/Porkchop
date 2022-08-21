#pragma once

#include "sourcecode.hpp"


namespace Porkchop {

enum class TokenType {
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
    KW_TRY,
    KW_CATCH,
    KW_FOR,
    KW_FN,
    KW_THROW,
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
    OP_NEQ,
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

    LINEBREAK,

    INVALID
};

struct Token {
    size_t line, column, width;
    TokenType type;

    operator struct Segment() const noexcept;
};

extern const std::unordered_map<std::string_view, TokenType> KEYWORDS;
extern const std::unordered_map<std::string_view, TokenType> OPERATORS;

}