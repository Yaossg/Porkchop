#pragma once

#include "tree.hpp"
#include "assembler.hpp"

namespace Porkchop {

struct FunctionReference {
    virtual ~FunctionReference() = default;
    [[nodiscard]] virtual TypeReference prototype() const = 0;
    virtual void assemble(Assembler* assembler) const = 0;
};

struct NamedFunctionReference : FunctionReference {
    FnDeclExpr* decl;
    FnDefExpr* def;

    void assemble(Assembler* assembler) const override {
        assembler->newFunction(def->definition.get());
    }

    [[nodiscard]] TypeReference prototype() const override {
        return decl->typeCache;
    }
};

struct LambdaFunctionReference : FunctionReference {
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

struct ExternalFunctionReference : FunctionReference {
    TypeReference type;

    void assemble(Assembler* assembler) const override {}

    [[nodiscard]] TypeReference prototype() const override {
        return type;
    }
};

struct MainFunctionReference : FunctionReference {
    FunctionDefinition* definition;
    TypeReference type;

    void assemble(Assembler* assembler) const override {
        assembler->newFunction(definition);
    }

    [[nodiscard]] TypeReference prototype() const override {
        return type;
    }
};

}