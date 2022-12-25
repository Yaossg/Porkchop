#include "local.hpp"
#include "function.hpp"
#include "diagnostics.hpp"

namespace Porkchop {

LocalContext::LocalContext(Compiler *compiler, LocalContext *parent) : compiler(compiler), parent(parent) {}

void LocalContext::push() {
    localIndices.emplace_back();
    declaredIndices.emplace_back();
    definedIndices.emplace_back();
}

void LocalContext::pop() {
    if (!declaredIndices.back().empty() && std::uncaught_exceptions() == 0) {
        size_t index = declaredIndices.back().begin()->second;
        auto function = dynamic_cast<NamedFunction*>(compiler->functions[index].get());
        throw TypeException("undefined declared function", function->decl->segment());
    }
    localIndices.pop_back();
    declaredIndices.pop_back();
    definedIndices.pop_back();
}

void LocalContext::local(Token token, const TypeReference& type) {
    std::string_view name = compiler->of(token);
    if (name == "_") return;
    localIndices.back().insert_or_assign(name, localTypes.size());
    localTypes.push_back(type);
}

void LocalContext::declare(Token token, FnDeclExpr* decl) {
    std::string_view name = compiler->of(token);
    if (name == "_") throw TypeException("function name must not be '_'", decl->segment());
    auto function = std::make_unique<NamedFunction>();
    function->decl = decl;
    size_t index = compiler->functions.size();
    declaredIndices.back().insert_or_assign(name, index);
    compiler->functions.emplace_back(std::move(function));
    decl->index = index;
}

void LocalContext::define(Token token, FnDefExpr* def) {
    std::string_view name = compiler->of(token);
    if (name == "_") throw TypeException("function name must not be '_'", def->segment());
    if (auto it = declaredIndices.back().find(name); it != declaredIndices.back().end()) {
        size_t index = it->second;
        auto function = dynamic_cast<NamedFunction*>(compiler->functions[index].get());
        function->decl->expect(def->prototype);
        function->def = def;
        declaredIndices.back().erase(it);
        definedIndices.back().insert_or_assign(name, index);
        def->index = index;
    } else {
        auto function = std::make_unique<NamedFunction>();
        function->def = def;
        size_t index = compiler->functions.size();
        definedIndices.back().insert_or_assign(name, index);
        compiler->functions.emplace_back(std::move(function));
        def->index = index;
    }
}

void LocalContext::lambda(LambdaExpr* lambda) const {
    auto function = std::make_unique<LambdaFunction>();
    function->lambda = lambda;
    size_t index = compiler->functions.size();
    compiler->functions.emplace_back(std::move(function));
    lambda->index = index;
}

void LocalContext::defineExternal(std::string_view name, TypeReference const& prototype) {
    auto function = std::make_unique<ExternalFunction>();
    function->type = prototype;
    size_t index = compiler->functions.size();
    definedIndices.back().insert_or_assign(name, index);
    compiler->functions.emplace_back(std::move(function));
}

LocalContext::LookupResult LocalContext::lookup(Token token, bool local) const {
    std::string_view name = compiler->of(token);
    if (name == "_") return {ScalarTypes::NONE, 0, LookupResult::Scope::NONE};
    if (local) {
        for (auto it = localIndices.rbegin(); it != localIndices.rend(); ++it) {
            if (auto lookup = it->find(name); lookup != it->end()) {
                size_t index = lookup->second;
                return {localTypes[index], index, LookupResult::Scope::LOCAL};
            }
        }
    }
    for (auto it = declaredIndices.rbegin(); it != declaredIndices.rend(); ++it) {
        if (auto lookup = it->find(name); lookup != it->end()) {
            size_t index = lookup->second;
            auto function = dynamic_cast<NamedFunction*>(compiler->functions[index].get());
            if (function->decl->prototype->R == nullptr)
                throw TypeException("recursive function without specified return type", function->decl->segment());
            return {function->decl->prototype, index, LookupResult::Scope::FUNCTION};
        }
    }
    for (auto it = definedIndices.rbegin(); it != definedIndices.rend(); ++it) {
        if (auto lookup = it->find(name); lookup != it->end()) {
            size_t index = lookup->second;
            return {compiler->functions[index]->prototype(), index, LookupResult::Scope::FUNCTION};
        }
    }
    return parent ? parent->lookup(token, false) : throw TypeException("unable to resolve this identifier", token);
}

}