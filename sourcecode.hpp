#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <string_view>
#include <stdexcept>
#include <deque>

namespace Porkchop {

struct Type;
using TypeReference = std::shared_ptr<Type>;
struct Expr;
struct IdExpr;
using ExprHandle = std::unique_ptr<Expr>;
using IdExprHandle = std::unique_ptr<IdExpr>;
struct Token;
struct Function;

struct SourceCode {
    std::string original;
    std::vector<std::string_view> lines;
    std::vector<Token> tokens;
    ExprHandle tree;
    TypeReference type;
    std::vector<std::unique_ptr<Function>> functions;
    std::vector<std::string> bytecode;

    size_t locals;
    size_t nextLabelIndex = 0;

    explicit SourceCode(std::string original) noexcept;

    void split();

    void tokenize();

    void parse();

    void compile();

    [[nodiscard]] std::string_view of(Token token) const noexcept;
};


[[noreturn]] inline void unreachable(const char* msg) {
    fprintf(stderr, "Unreachable is reached: %s", msg);
    __builtin_unreachable();
}

struct Descriptor {
    [[nodiscard]] virtual std::string_view descriptor(const SourceCode& sourcecode) const noexcept = 0;
    virtual ~Descriptor() noexcept = default;
    [[nodiscard]] virtual std::vector<const Descriptor*> children() const { return {}; }
    [[nodiscard]] std::string walkDescriptor(const SourceCode& sourcecode) const {
        int id = 0;
        std::string buf;
        walkDescriptor(buf, id, sourcecode);
        return buf;
    }

private:
    void walkDescriptor(std::string& buf, int& id, const SourceCode& sourcecode) const {
        std::string pid = std::to_string(id);
        buf += pid;
        buf += "[\"";
        buf += descriptor(sourcecode);
        buf += "\"]\n";
        for (auto&& child : children()) {
            ++id;
            buf += pid + "-->" + std::to_string(id) + "\n";
            child->walkDescriptor(buf, id, sourcecode);
        }
    }
};

}