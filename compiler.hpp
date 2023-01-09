#pragma once

#include <memory>

#include "source.hpp"
#include "continuum.hpp"

namespace Porkchop {

struct Token;
struct Assembler;
struct FunctionDefinition;

struct Compiler {
    Continuum* continuum;
    Source source;
    std::unique_ptr<FunctionDefinition> definition;

    enum class Mode {
        MAIN, SHELL, EVAL
    };

    explicit Compiler(Continuum* continuum, Source source);

    [[nodiscard]] std::string_view of(Token token) const noexcept;

    void parse(Mode mode);

    void compile(Assembler* assembler) const;

    [[nodiscard]] std::string descriptor() const;
};

}