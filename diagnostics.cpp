#include "diagnostics.hpp"
#include "tree.hpp"

namespace Porkchop {

std::string SegmentException::message(const SourceCode &sourcecode) const {
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
    for (size_t line = segment.line1; line <= segment.line2; ++line) {
        auto lineNo = std::to_string(line + 1);
        auto code = sourcecode.lines.at(line);
        result += "    ";
        result += lineNo;
        result += " | ";
        result +=  code;
        result += "\n    ";
        result += std::string(lineNo.length(), ' ');
        result += " | ";
        size_t column1 = 0;
        if (line == segment.line1) {
            column1 = segment.column1;
        }
        size_t column2 = code.length();
        if (line == segment.line2) {
            column2 = segment.column2;
        }
        size_t width = column2 - column1;
        result += std::string(column1, ' ');
        if (line == segment.line1) {
            result += '^';
            if (width > 1)
                result += std::string(width - 1, '~');
        } else {
            result += std::string(width, '~');
        }
        result += '\n';
    }
    return result;
}

void neverGonnaGiveYouUp(const TypeReference &type, const char *msg, Segment segment) {
    if (isNever(type)) throw TypeException(std::string("'never' is not allowed ") + msg, segment);
}

std::string mismatch(const TypeReference &type, const char *msg, const TypeReference &expected) noexcept {
    std::string result;
    result += "types mismatch on ";
    result += msg;
    result += ", the one is '";
    result += expected->toString();
    result += "', but the other is '";
    result += type->toString();
    result += "'";
    return result;
}

std::string mismatch(const TypeReference &type, const char *msg, size_t index, const TypeReference &expected) noexcept {
    std::string result;
    result += "types mismatch on ";
    result += msg;
    result += ", the first one is '";
    result += expected->toString();
    result += "', but the '";
    result += std::to_string(index);
    result += "'th one is '";
    result += type->toString();
    result += "'";
    return result;
}

std::string unexpected(const TypeReference &type, const char *expected) noexcept {
    std::string result;
    result += "expected ";
    result += expected;
    result += " but got '";
    result += type->toString();
    result += "'";
    return result;
}

std::string unexpected(const TypeReference &type, const TypeReference &expected) noexcept {
    std::string result;
    result += "expected '";
    result += expected->toString();
    result += "' but got '";
    result += type->toString();
    result += "'";
    return result;
}

std::string unassignable(const TypeReference &type, const TypeReference &expected) noexcept {
    std::string result;
    result += "'";
    result += type->toString();
    result += "' is not assignable to '";
    result += expected->toString();
    result += "'";
    return result;
}

void neverGonnaGiveYouUp(Expr const* expr, const char* msg) {
    neverGonnaGiveYouUp(expr->typeCache, msg, expr->segment());
}

void matchOnBothOperand(Expr const* expr1, Expr const* expr2) {
    if (!expr1->typeCache->equals(expr2->typeCache))
        throw TypeException(mismatch(expr2->typeCache, "both operands", expr1->typeCache), range(expr1->segment(), expr2->segment()));
}

void expected(Expr const* expr, bool pred(TypeReference const&), const char* msg) {
    if (!pred(expr->typeCache))
        throw TypeException(unexpected(expr->typeCache, msg), expr->segment());
}

void expected(Expr const* expr, TypeReference const& expected) {
    if (!expr->typeCache->equals(expected))
        throw TypeException(unexpected(expr->typeCache, expected), expr->segment());
}

void assignable(Expr const* expr, TypeReference const& expected) {
    if (!expected->assignableFrom(expr->typeCache))
        throw TypeException(unassignable(expr->typeCache, expected), expr->segment());
}
}