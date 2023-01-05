#pragma once

#include <memory>
#include <vector>
#include <string>

namespace Porkchop {

struct Type;
using TypeReference = std::shared_ptr<Type>;
struct Expr;
struct IdExpr;
using ExprHandle = std::unique_ptr<Expr>;
using IdExprHandle = std::unique_ptr<IdExpr>;
struct Token;
struct FunctionReference;
struct Assembler;
struct FunctionDefinition;

struct Compiler {
    std::string original;
    std::vector<std::string_view> lines;
    std::vector<Token> tokens;
    std::unique_ptr<FunctionDefinition> definition;
    std::vector<std::unique_ptr<FunctionReference>> functions;

    size_t nextLabelIndex = 0;

    explicit Compiler(std::string original);

    void tokenize();

    [[nodiscard]] std::string_view of(Token token) const noexcept;

    void parse();

    void compile(Assembler* assembler);

    [[nodiscard]] std::string descriptor() const;
};

}