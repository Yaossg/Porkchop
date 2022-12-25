#include "diagnostics.hpp"
#include "tree.hpp"
#include "unicode/unicode.hpp"
#include "util.hpp"

namespace Porkchop {

int digits10(size_t num) noexcept {
    int digits = 1;
    while (num /= 10)
        ++digits;
    return digits;
}

size_t getUnicodeWidth(std::string_view view, size_t line, size_t column) {
    size_t width = 0;
    UnicodeParser up(view, line, column);
    while (up.remains()) {
        width += getUnicodeWidth(up.decodeUnicode());
    }
    return width;
}

std::string SegmentException::message(const Compiler &compiler) const {
    std::string result;
    result += "Compilation Error: ";
    result += std::logic_error::what();
    result += " at line ";
    result += std::to_string(segment.line1 + 1);
    result += " column ";
    result += std::to_string(segment.column1 + 1);
    result += " to ";
    if (segment.line1 != segment.line2) {
        result += "line ";
        result += std::to_string(segment.line2 + 1);
        result += " column ";
    }
    result += std::to_string(segment.column2 + 1);
    result += "\n";
    int digits = digits10(segment.line2 + 1);
    for (size_t line = segment.line1; line <= segment.line2; ++line) {
        auto lineNo = std::to_string(line + 1);
        auto code = compiler.lines.at(line);
        result += "   ";
        result += lineNo;
        result += std::string(digits - lineNo.length() + 1, ' ');
        result += " | ";
        result += code;
        result += "\n   ";
        result += std::string(digits + 1, ' ');
        result += " | ";
        size_t column1 = line == segment.line1 ? segment.column1 : code.find_first_not_of(' ');
        size_t column2 = line == segment.line2 ? segment.column2 : code.length();
        size_t width1 = getUnicodeWidth(code.substr(0, column1), line, 0);
        result += std::string(width1, ' ');
        size_t width2 = getUnicodeWidth(code.substr(column1, column2 - column1), line, column1);
        if (line == segment.line1) {
            result += '^';
            if (width2 > 1)
                result += std::string(width2 - 1, '~');
        } else {
            result += std::string(width2, '~');
        }
        result += '\n';
    }
    return result;
}

void neverGonnaGiveYouUp(const TypeReference &type, const char *msg, Segment segment) {
    if (isNever(type)) {
        throw TypeException(std::string("'never' is never allowed ") + msg, segment);
    }
}

[[noreturn]] void mismatch(TypeReference const& type1, TypeReference const& type2, const char *msg, Segment segment) {
    throw TypeException(join("type mismatch on ", msg, ", the one is '", type1->toString(),
                             "', but the other is '", type2->toString(), "'"), segment);
}

void assignable(TypeReference const& type, TypeReference const& expected, Segment segment) {
    if (!expected->assignableFrom(type)) {
        throw TypeException(join("'", type->toString(), "' is not assignable to '", expected->toString(), "'"), segment);
    }
}

}