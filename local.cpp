#include "local.hpp"
#include "tree.hpp"

namespace Porkchop {

ReferenceContext::ReferenceContext(SourceCode *sourcecode, ReferenceContext *parent) : sourcecode(sourcecode), parent(parent) {
    global("println", std::make_shared<FuncType>(std::vector{ScalarTypes::STRING}, ScalarTypes::NONE));
    global("exit", std::make_shared<FuncType>(std::vector{ScalarTypes::INT}, ScalarTypes::NEVER));
}

void ReferenceContext::push() {
    scopes.emplace_back();
    decl_scopes.emplace_back();
    def_scopes.emplace_back();
}

void ReferenceContext::pop() {
    if (!decl_scopes.back().empty() && std::uncaught_exceptions() == 0)
        throw TypeException("undefined function", decl_scopes.back().begin()->second->segment());
    scopes.pop_back();
    decl_scopes.pop_back();
    def_scopes.pop_back();
}

void ReferenceContext::global(std::string_view name, const TypeReference &type) {
    scopes.front().insert_or_assign(name, type);
}

void ReferenceContext::local(Token token, const TypeReference &type) {
    std::string_view name = sourcecode->of(token);
    if (name == "_") return;
    scopes.back().insert_or_assign(name, type);
}

void ReferenceContext::declare(Token token, FnDeclExpr* decl) {
    std::string_view name = sourcecode->of(token);
    if (name == "_") throw TypeException("function name must not be '_'", decl->segment());
    decl_scopes.back().insert_or_assign(name, decl);
}

void ReferenceContext::define(Token token, FnDefExpr* def) {
    std::string_view name = sourcecode->of(token);
    if (name == "_") throw TypeException("function name must not be '_'", def->segment());
    if (auto it = decl_scopes.back().find(name); it != decl_scopes.back().end()) {
        expected(it->second, def->prototype);
        decl_scopes.back().erase(it);
    }
    def_scopes.back().insert_or_assign(name, def);
}

TypeReference ReferenceContext::lookup(Token token, bool captured) const {
    std::string_view name = sourcecode->of(token);
    if (name == "_") return ScalarTypes::NONE;
    if (captured) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            if (auto lookup = it->find(name); lookup != it->end())
                return lookup->second;
        }
    }
    for (auto it = decl_scopes.rbegin(); it != decl_scopes.rend(); ++it) {
        if (auto lookup = it->find(name); lookup != it->end()) {
            if (lookup->second->prototype->R == nullptr)
                throw TypeException("recursive function without specified return type", lookup->second->segment());
            return lookup->second->prototype;
        }
    }
    for (auto it = def_scopes.rbegin(); it != def_scopes.rend(); ++it) {
        if (auto lookup = it->find(name); lookup != it->end()) {
            return lookup->second->prototype;
        }
    }
    return parent ? parent->lookup(token, false) : throw TypeException("unable to resolve this identifier", token);
}
}