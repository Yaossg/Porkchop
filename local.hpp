#pragma once

#include "type.hpp"
#include "compiler.hpp"

namespace Porkchop {

struct FnDeclExpr;
struct FnDefExpr;
struct LambdaExpr;

struct LocalContext {
    std::vector<std::unordered_map<std::string, size_t>> localIndices{{}};
    std::vector<TypeReference> localTypes;

    std::vector<std::unordered_map<std::string, size_t>> declaredIndices{{}};
    std::vector<std::unordered_map<std::string, size_t>> definedIndices{{}};

    Continuum* continuum;
    LocalContext* parent;

    explicit LocalContext(Continuum* continuum, LocalContext* parent);

    void push();
    void pop();
    void checkDeclared();
    void local(std::string_view name, TypeReference const& type);
    void declare(std::string_view name, FnDeclExpr* decl);
    void define(std::string_view name, FnDefExpr* def);
    void defineExternal(std::string_view name, std::shared_ptr<FuncType> const& prototype);
    void lambda(LambdaExpr* lambda) const;

    struct LookupResult {
        TypeReference type;
        size_t index;
        enum class Scope {
            NONE, LOCAL, FUNCTION
        } scope;
    };

    [[nodiscard]] LookupResult lookup(Compiler& compiler, Token token, bool local = true) const;

    struct Guard {
        LocalContext& context;
        explicit Guard(LocalContext& context): context(context) {
            context.push();
        }
        ~Guard() noexcept(false) {
            context.pop();
        }
    };
};

}
