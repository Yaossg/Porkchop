#pragma once

#include "tree.hpp"
#include "assembler.hpp"

namespace Porkchop {

struct Function {
    virtual ~Function() = default;
    [[nodiscard]] virtual TypeReference prototype() const = 0;
    virtual void assemble(Assembler* assembler) const = 0;
};

struct NamedFunction : Function {
    FnDeclExpr* decl;
    FnDefExpr* def;

    void assemble(Assembler* assembler) const override {
        assembler->newFunction(def->definition.get());
    }

    [[nodiscard]] TypeReference prototype() const override {
        return decl->typeCache;
    }
};

struct LambdaFunction : Function {
    LambdaExpr* lambda;

    void assemble(Assembler* assembler) const override {
        assembler->newFunction(lambda->definition.get());
    }

    [[nodiscard]] TypeReference prototype() const override {
        std::vector<TypeReference> P;
        for (auto&& capture : lambda->captures) {
            P.push_back(capture->typeCache);
        }
        auto prototype = lambda->parameters->prototype;
        P.insert(P.end(), prototype->P.begin(), prototype->P.end());
        return std::make_shared<FuncType>(std::move(P), prototype->R);
    }
};

struct ExternalFunction : Function {
    TypeReference type;

    void assemble(Assembler* assembler) const override {}

    [[nodiscard]] TypeReference prototype() const override {
        return type;
    }
};

}