#pragma once

#include <deque>
#include <unordered_map>

#include "type.hpp"
#include "compiler.hpp"

namespace Porkchop {

struct FnDeclExpr;
struct FnDefExpr;
struct LambdaExpr;

struct LocalContext {
    std::deque<std::unordered_map<std::string_view, size_t>> localIndices{{}};
    std::vector<TypeReference> localTypes;

    std::deque<std::unordered_map<std::string_view, size_t>> declaredIndices{{}};
    std::deque<std::unordered_map<std::string_view, size_t>> definedIndices{{}};

    Compiler* compiler;
    LocalContext* parent;

    explicit LocalContext(Compiler* compiler, LocalContext* parent);

    void push();
    void pop();
    void local(Token token, TypeReference const& type);
    void declare(Token token, FnDeclExpr* decl);
    void define(Token token, FnDefExpr* def);
    void defineExternal(std::string_view name, TypeReference const& prototype);
    void lambda(LambdaExpr* lambda) const;

    struct LookupResult {
        TypeReference type;
        size_t index;
        enum class Scope {
            NONE, LOCAL, FUNCTION
        } scope;
    };

    [[nodiscard]] LookupResult lookup(Token token, bool local = true) const;

    struct Guard {
        LocalContext& context;
        explicit Guard(LocalContext& context): context(context) {
            context.push();
        }
        ~Guard() noexcept(false) {
            context.pop();
        }
    };

    template<std::derived_from<Expr> E, typename... Args>
        requires std::constructible_from<E, Compiler&, Args...>
    auto make(Args&&... args) {
        auto expr = std::make_unique<E>(*compiler, std::forward<Args>(args)...);
        expr->initialize();
        return expr;
    }

};

}
