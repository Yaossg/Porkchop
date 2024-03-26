#include "diagnostics.hpp"
#include "tree.hpp"
#include "unicode/unicode.hpp"

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

std::string ErrorMessage::build(Source* source) {
    if (message.ends_with(' ')) message.pop_back();
    if (textOnly || source == nullptr) return message + '\n';
    std::string result;
    result += message;
    result += "\n";
    int digits = digits10(segment.line2 + 1);
    for (size_t line = segment.line1; line <= segment.line2; ++line) {
        auto lineNo = std::to_string(line + 1);
        auto code = source->lines.at(line);
        result += "   ";
        result += lineNo;
        result += std::string(digits - lineNo.length() + 1, ' ');
        result += " | ";
        result += code;
        result += "\n   ";
        result += std::string(digits + 1, ' ');
        result += " | ";
        if (size_t head = code.find_first_not_of(' '); head != std::string::npos) {
            size_t column1 = line == segment.line1 ? segment.column1 : head;
            size_t column2 = line == segment.line2 ? segment.column2 : code.length();
            size_t width1 = getUnicodeWidth(code.substr(0, column1), line, 0);
            result += std::string(width1, ' ');
            size_t width2 = getUnicodeWidth(code.substr(column1, column2 - column1), line, column1);
            std::string underline;
            if (line == segment.line1) {
                underline += '^';
                if (width2 > 1)
                    underline += std::string(width2 - 1, '~');
            } else {
                underline += std::string(width2, '~');
            }
            result += colored(underline);
        }
        result += '\n';
    }
    return result;
}

void raise(const char *msg, Segment segment) {
    Error().with(
            ErrorMessage().error(segment)
            .text(msg)
            ).raise();
}

void neverGonnaGiveYouUp(const TypeReference &type, const char *msg, Segment segment) {
    if (isNever(type)) {
        Error().with(
                ErrorMessage().error(segment)
                .type(ScalarTypes::NEVER).text("is never allowed ").text(msg)
                ).raise();
    }
}

void Error::report(Source* source, bool newline) {
    std::string buf;
    for (auto&& message : messages) {
        buf += message.build(source);
    }
    if (!newline)
        buf.pop_back();
    fprintf(stderr, "%s", buf.c_str());
    fflush(stderr);
}

FILE* open(const char *filename, const char *mode) {
    FILE* file = fopen(filename, mode);
    if (file == nullptr) {
        Error error;
        error.with(ErrorMessage().fatal().text("failed to open input file: ").text(filename));
        error.report(nullptr);
        std::exit(20);
    }
    return file;
}

}