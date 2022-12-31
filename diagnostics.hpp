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
void assignable(TypeReference const& type, TypeReference const& expected, Segment segment);

}