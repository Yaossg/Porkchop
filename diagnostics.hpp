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
void assignable(TypeReference const& type, TypeReference const& expected, Segment segment);

inline const char* ordinalSuffix(size_t ordinal) {
    switch (ordinal % 100) {
        default:
            switch (ordinal % 10) {
                case 1:
                    return "st";
                case 2:
                    return "nd";
                case 3:
                    return "rd";
            }
        case 11:
        case 12:
        case 13: ;
    }
    return "th";
}

inline std::string ordinal(size_t index) {
    return "the " + std::to_string(index + 1) + ordinalSuffix(index + 1);
}

}