#pragma once

#include "token.hpp"

namespace Porkchop {

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

struct SegmentException : std::logic_error {
    Segment segment;
    SegmentException(std::string const& message, Segment segment): std::logic_error(message), segment(segment) {}

    [[nodiscard]] std::string message(SourceCode const& sourcecode) const;
};

using TokenException = SegmentException;
using ParserException = SegmentException;
using TypeException = SegmentException;

void neverGonnaGiveYouUp(TypeReference const& type, const char* msg, Segment segment);
[[nodiscard]] std::string mismatch(TypeReference const& type, const char* msg, TypeReference const& expected) noexcept;
[[nodiscard]] std::string mismatch(TypeReference const& type, const char* msg, size_t index, TypeReference const& expected) noexcept;
[[nodiscard]] std::string unexpected(TypeReference const& type, const char* expected) noexcept;
[[nodiscard]] std::string unexpected(TypeReference const& type, TypeReference const& expected) noexcept;
[[nodiscard]] std::string unassignable(TypeReference const& type, TypeReference const& expected) noexcept;
void neverGonnaGiveYouUp(Expr const* expr, const char* msg);
void matchOnBothOperand(Expr const* expr1, Expr const* expr2);
void expected(Expr const* expr, bool pred(TypeReference const&), const char* msg);
void expected(Expr const* expr, TypeReference const& expected);
void assignable(Expr const* expr, TypeReference const& expected);

}