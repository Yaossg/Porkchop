#pragma once

#include <stdexcept>
#include <memory>

#include "source.hpp"
#include "token.hpp"

namespace Porkchop {

struct Type;
using TypeReference = std::shared_ptr<Type>;

struct SegmentException : std::logic_error {
    Segment segment;
    SegmentException(std::string const& message, Segment segment): std::logic_error(message), segment(segment) {}

    [[nodiscard]] std::string message(Source const& source) const;
};

using TokenException = SegmentException;
using ParserException = SegmentException;
using TypeException = SegmentException;
using ConstException = SegmentException;

void neverGonnaGiveYouUp(TypeReference const& type, const char* msg, Segment segment);
void assignable(TypeReference const& type, TypeReference const& expected, Segment segment);

}