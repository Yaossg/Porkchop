#pragma once

#include "type.hpp"

namespace Porkchop {

struct FnDeclExpr;
struct FnDefExpr;

struct ReferenceContext {
    std::vector<std::unordered_map<std::string_view, TypeReference>> scopes{{}};
    std::vector<std::unordered_map<std::string_view, FnDeclExpr*>> decl_scopes{{}};
    std::vector<std::unordered_map<std::string_view, FnDefExpr*>> def_scopes{{}};
    SourceCode* sourcecode;
    ReferenceContext* parent;

    explicit ReferenceContext(SourceCode* sourcecode, ReferenceContext* parent);
    void push();
    void pop();
    void global(std::string_view name, TypeReference const& type);
    void local(Token token, TypeReference const& type);
    void declare(Token token, FnDeclExpr* decl);
    void define(Token token, FnDefExpr* def);
    [[nodiscard]] TypeReference lookup(Token token, bool captured = true) const;

    struct Guard {
        ReferenceContext& context;
        explicit Guard(ReferenceContext& context): context(context) {
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
