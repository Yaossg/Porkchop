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
}

void ReferenceContext::pop() {
    if (!decl_scopes.back().empty() && std::uncaught_exceptions() == 0)
        throw TypeException("undefined function", decl_scopes.back().begin()->second->segment());
    scopes.pop_back();
    decl_scopes.pop_back();
}

void ReferenceContext::global(std::string_view name, const TypeReference &type) {
    scopes.front().emplace(name, type);
}

void ReferenceContext::local(Token token, const TypeReference &type) {
    std::string_view name = sourcecode->source(token);
    if (name == "_") return;
    scopes.back().emplace(name, type);
    if (auto it = decl_scopes.back().find(name); it != decl_scopes.back().end()) {
        if (it->second->T->equals(type)) {
            decl_scopes.back().erase(it);
        } else {
            expected(it->second, type);
        }
    }
}

void ReferenceContext::decl(Token token, FnDeclExpr* decl) {
    std::string_view name = sourcecode->source(token);
    if (name == "_") throw TypeException("function name must not be '_'", decl->segment());
    decl_scopes.back().emplace(name, decl);
}

TypeReference ReferenceContext::lookup(Token token) const {
    std::string_view name = sourcecode->source(token);
    if (name == "_") return ScalarTypes::NONE;
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        if (auto lookup = it->find(name); lookup != it->end())
            return lookup->second;
    }
    for (auto it = decl_scopes.rbegin(); it != decl_scopes.rend(); ++it) {
        if (auto lookup = it->find(name); lookup != it->end()) {
            if (auto func = dynamic_cast<FuncType*>(lookup->second->T.get())) {
                if (func->R == nullptr)
                    throw TypeException("recursive function without specified return type", lookup->second->segment());
            }
            return lookup->second->T;
        }
    }
    return parent ? parent->lookup(token) : throw TypeException("unable to resolve this identifier", token);
}
}