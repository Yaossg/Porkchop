#include "token.hpp"

namespace Porkchop {

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

std::string_view SourceCode::of(Token token) const noexcept {
    return lines.at(token.line).substr(token.column, token.width);
}

}
