#pragma once

#include "source.hpp"
#include "util.hpp"

#include <vector>

namespace Porkchop {

struct LineTokenizer {
    Source& context;
    const char *const o, *p, *q, *const r;
    const size_t line;
    bool backslash = false;

    LineTokenizer(Source& context,
                  std::string_view view):
            context(context),
            o(view.begin()), p(o), q(p), r(view.end()),
            line(context.lines.size() - 1)
            { tokenize(); }

    [[nodiscard]] size_t column() const noexcept {
        return q - o;
    }

    char getc() noexcept {
        if (remains()) [[likely]] {
            return *q++;
        } else [[unlikely]] {
            return 0;
        }
    }
    void ungetc(char ch) {
        if (ch) [[likely]] {
            if (p == q) [[unlikely]] {
                unreachable();
            } else [[likely]] {
                --q;
            }
        }
    }
    [[nodiscard]] char peekc() const noexcept {
        return *q;
    }
    [[nodiscard]] bool remains() const noexcept {
        return q != r;
    }
    void step() noexcept {
        p = q;
    }
    [[nodiscard]] Token make(TokenType type) const noexcept {
        return {.line = line, .column = size_t(p - o), .width = size_t(q - p), .type = type};
    }

    void add(TokenType type);
    void addLBrace(Source::BraceType braceType);
    Source::BraceType addRBrace();
    [[noreturn]] void raise(const char* msg) const;
    void tokenize();
    void addId();
    void addPunct();
    void scanDigits(bool pred(char) noexcept);
    void addNumber();
    void addChar();
    void addString(bool first);
    void addRawString(bool first);
    void checkGreedy(const char* left, const char* right, TokenType match);
};

int64_t parseInt(Source& source, Token token);
double parseFloat(Source& source, Token token);
char32_t parseChar(Source& source, Token token);
std::string parseString(Source& source, Token token);

}