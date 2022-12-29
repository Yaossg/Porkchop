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
        assembler->newFunction(def->locals, def->clause);
    }

    [[nodiscard]] TypeReference prototype() const override {
        return def != nullptr ? def->prototype : decl->prototype;
    }
};

struct LambdaFunction : Function {
    LambdaExpr* lambda;

    void assemble(Assembler* assembler) const override {
        assembler->newFunction(lambda->locals, lambda->clause);
    }

    [[nodiscard]] TypeReference prototype() const override {
        std::vector<TypeReference> P;
        for (auto&& capture : lambda->captures) {
            P.push_back(capture->typeCache);
        }
        for (auto&& param : lambda->prototype->P) {
            P.push_back(param);
        }
        return std::make_shared<FuncType>(std::move(P), lambda->prototype->R);
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