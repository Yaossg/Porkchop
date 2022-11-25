#pragma once

#include <memory>
#include <string>
#include <vector>

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

    size_t locals;
    size_t nextLabelIndex = 0;

    explicit Compiler(std::string original) noexcept;

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

struct Descriptor {
    [[nodiscard]] virtual std::string_view descriptor(const Compiler& compiler) const noexcept = 0;
    virtual ~Descriptor() noexcept = default;
    [[nodiscard]] virtual std::vector<const Descriptor*> children() const { return {}; }
    [[nodiscard]] std::string walkDescriptor(const Compiler& compiler) const {
        int id = 0;
        std::string buf;
        walkDescriptor(buf, id, compiler);
        return buf;
    }

private:
    void walkDescriptor(std::string& buf, int& id, const Compiler& compiler) const {
        std::string pid = std::to_string(id);
        buf += pid;
        buf += "[\"";
        buf += descriptor(compiler);
        buf += "\"]\n";
        for (auto&& child : children()) {
            ++id;
            buf += pid + "-->" + std::to_string(id) + "\n";
            child->walkDescriptor(buf, id, compiler);
        }
    }
};

}