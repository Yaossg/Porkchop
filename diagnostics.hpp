#pragma once

#include "compiler.hpp"
#include "token.hpp"

namespace Porkchop {

struct SegmentException : std::logic_error {
    Segment segment;
    SegmentException(std::string const& message, Segment segment): std::logic_error(message), segment(segment) {}

    [[nodiscard]] std::string message(Compiler const& compiler) const;
};

using TokenException = SegmentException;
using ParserException = SegmentException;
using TypeException = SegmentException;
using ConstException = SegmentException;

[[nodiscard]] std::string mismatch(TypeReference const& type, const char* msg, TypeReference const& expected);
[[nodiscard]] std::string mismatch(TypeReference const& type, const char* msg, size_t index, TypeReference const& expected);
[[nodiscard]] std::string unexpected(TypeReference const& type, const char* expected);
[[nodiscard]] std::string unexpected(TypeReference const& type, TypeReference const& expected);
[[nodiscard]] std::string unassignable(TypeReference const& type, TypeReference const& expected);
void neverGonnaGiveYouUp(TypeReference const& type, const char* msg, Segment segment);
void neverGonnaGiveYouUp(ExprHandle const& expr, const char* msg);
void match(Expr const* expr1, Expr const* expr2);
void expected(Expr const* expr, bool pred(TypeReference const&), const char* msg);
void expected(Expr const* expr, TypeReference const& expected);
void assignable(TypeReference const& type, TypeReference const& expected, Segment segment);
void assignable(Expr const* expr, TypeReference const& expected);

}