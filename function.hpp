#pragma once

#include "tree.hpp"
#include "assembler.hpp"

namespace Porkchop {

// Stored raw pointers will be soon invalidated once assemblies generated.
// Only prototype() is persistent.
struct FunctionReference {

    virtual ~FunctionReference() = default;

    virtual void assemble(Assembler* assembler) const = 0;

    [[nodiscard]] virtual std::shared_ptr<FuncType> prototype() {
        return typeCache;
    }

    void write(Assembler* assembler) {
        assembler->func(prototype());
        assemble(assembler);
    }

protected:
    std::shared_ptr<FuncType> typeCache;
};

struct NamedFunctionReference : FunctionReference {
    FnDeclExpr* decl;
    FnDefExpr* def;

    void assemble(Assembler* assembler) const override {
        assembler->newFunction(def->definition.get());
    }

    [[nodiscard]] std::shared_ptr<FuncType> prototype() override {
        if (typeCache) return typeCache;
        return typeCache = decl->parameters->prototype;
    }
};

struct LambdaFunctionReference : FunctionReference {
    LambdaExpr* lambda;

    explicit LambdaFunctionReference(LambdaExpr* lambda): lambda(lambda) {
        std::vector<TypeReference> P;
        for (auto&& capture : lambda->captures) {
            P.push_back(capture->typeCache);
        }
        auto prototype = lambda->parameters->prototype;
        P.insert(P.end(), prototype->P.begin(), prototype->P.end());
        typeCache = std::make_shared<FuncType>(std::move(P), prototype->R);
    }

    void assemble(Assembler* assembler) const override {
        assembler->newFunction(lambda->definition.get());
    }
};

struct ExternalFunctionReference : FunctionReference {

    explicit ExternalFunctionReference(std::shared_ptr<FuncType> type) {
        typeCache = std::move(type);
    }

    void assemble(Assembler* assembler) const override {}
};

struct MainFunctionReference : FunctionReference {
    Continuum* continuum;
    FunctionDefinition* definition;

    MainFunctionReference(Continuum* continuum, FunctionDefinition* definition, std::shared_ptr<FuncType> prototype)
        : continuum(continuum), definition(definition) {
        typeCache = std::move(prototype);
    }

    void assemble(Assembler* assembler) const override {
        assembler->newMainFunction(continuum, definition);
    }
};

}