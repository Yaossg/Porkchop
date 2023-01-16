#pragma once

#include <string>
#include <vector>
#include <deque>

#include "token.hpp"

namespace Porkchop {

struct Token;

struct Source {
    std::deque<std::string> snippets;
    std::vector<std::string_view> lines;
    std::vector<Token> tokens;
    enum class BraceType {
        CODE, STRING, RAW_STRING
    };
    std::vector<BraceType> braces;
    std::vector<Token> greedy;
    bool raw = false;

    [[nodiscard]] std::string_view of(Token token) const noexcept;
    void append(std::string code);
    bool remains();
};

}