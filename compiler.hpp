#pragma once

#include <memory>

namespace Porkchop {

struct Type;
using TypeReference = std::shared_ptr<Type>;
struct Expr;
struct IdExpr;
using ExprHandle = std::unique_ptr<Expr>;
using IdExprHandle = std::unique_ptr<IdExpr>;
struct Token;
struct Function;
struct Assembler;

struct Compiler {
    std::string original;
    std::vector<std::string_view> lines;
    std::vector<Token> tokens;
    ExprHandle tree;
    TypeReference type;
    std::vector<std::unique_ptr<Function>> functions;

    std::vector<TypeReference> locals;
    size_t nextLabelIndex = 0;

    explicit Compiler(std::string original);

    void tokenize();

    [[nodiscard]] std::string_view of(Token token) const noexcept;

    void parse();

    void compile(Assembler* assembler);

    [[nodiscard]] std::string descriptor() const;
};


[[noreturn]] inline void unreachable(const char* msg) {
    fprintf(stderr, "Unreachable is reached: %s", msg);
    __builtin_unreachable();
}

}