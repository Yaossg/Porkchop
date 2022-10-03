#pragma once

#include "type.hpp"

namespace Porkchop {

struct FnDeclExpr;
struct FnDefExpr;
struct LambdaExpr;

struct LocalContext {
    std::deque<std::unordered_map<std::string_view, size_t>> localIndices{{}};
    std::vector<TypeReference> localTypes;

    std::deque<std::unordered_map<std::string_view, size_t>> declaredIndices{{}};
    std::deque<std::unordered_map<std::string_view, size_t>> definedIndices{{}};

    SourceCode* sourcecode;
    LocalContext* parent;

    explicit LocalContext(SourceCode* sourcecode, LocalContext* parent);
    void push();
    void pop();
    void local(Token token, TypeReference const& type);
    void declare(Token token, FnDeclExpr* decl);
    void define(Token token, FnDefExpr* def);
    void defineExternal(std::string_view name, TypeReference const& prototype);
    void lambda(LambdaExpr* lambda) const;

    struct LookupResult {
        TypeReference type;
        bool function;
        size_t index;
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

    template<typename E, typename... Args>
    auto make(Args&&... args) {
        auto expr = std::make_unique<E>(std::forward<Args>(args)...);
        expr->initialize(*this);
        return expr;
    }

};

}
