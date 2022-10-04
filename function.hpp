#pragma once

#include "tree.hpp"

namespace Porkchop {

struct Function {
    virtual ~Function() = default;
    [[nodiscard]] virtual TypeReference prototype() const = 0;
    virtual void compile(SourceCode& sourcecode, std::vector<std::string>& bytecode) const = 0;
};

struct NamedFunction : Function {
    FnDeclExpr* decl;
    FnDefExpr* def;
    void compile(SourceCode& sourcecode, std::vector<std::string>& bytecode) const override {
        char s[30];
        bytecode.emplace_back("(");
        sprintf(s, "local %zu", def->locals);
        bytecode.emplace_back(s);
        def->clause->walkBytecode(sourcecode, bytecode);
        bytecode.emplace_back("return");
        bytecode.emplace_back(")");
    }

    [[nodiscard]] TypeReference prototype() const override {
        return def != nullptr ? def->prototype : decl->prototype;
    }
};

struct LambdaFunction : Function {
    LambdaExpr* lambda;
    void compile(SourceCode& sourcecode, std::vector<std::string>& bytecode) const override {
        char s[30];
        bytecode.emplace_back("(");
        sprintf(s, "local %zu", lambda->locals);
        bytecode.emplace_back(s);
        lambda->clause->walkBytecode(sourcecode, bytecode);
        bytecode.emplace_back("return");
        bytecode.emplace_back(")");
    }

    [[nodiscard]] TypeReference prototype() const override {
        return lambda->prototype;
    }
};

struct ExternalFunction : Function {
    TypeReference type;
    void compile(SourceCode& sourcecode, std::vector<std::string>& bytecode) const override {}

    [[nodiscard]] TypeReference prototype() const override {
        return type;
    }
};

}