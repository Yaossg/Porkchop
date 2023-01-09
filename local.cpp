#include "local.hpp"
#include "function.hpp"
#include "diagnostics.hpp"

namespace Porkchop {

LocalContext::LocalContext(Continuum* continuum, LocalContext *parent): continuum(continuum), parent(parent) {}

void LocalContext::push() {
    localIndices.emplace_back();
    declaredIndices.emplace_back();
    definedIndices.emplace_back();
}

void LocalContext::pop() {
    if (!declaredIndices.back().empty() && std::uncaught_exceptions() == 0) {
        size_t index = declaredIndices.back().begin()->second;
        auto function = dynamic_cast<NamedFunctionReference*>(continuum->functions[index].get());
        raise("undefined declared function", function->decl->segment());
    }
    localIndices.pop_back();
    declaredIndices.pop_back();
    definedIndices.pop_back();
}

void LocalContext::local(std::string_view name, const TypeReference& type) {
    if (name == "_") return;
    localIndices.back().insert_or_assign(std::string(name), localTypes.size());
    localTypes.push_back(type);
}

void LocalContext::declare(std::string_view name, FnDeclExpr* decl) {
    if (name == "_") {
        Error().with(
                ErrorMessage().error(decl->segment())
                .text("function name must not be").quote("_")
                ).raise();
    }
    std::string name0(name);
    if (auto it = declaredIndices.back().find(name0); it != declaredIndices.back().end()) {
        if (!decl->parameters->prototype->R) {
            // If the program flow reaches here,
            // it must be the function definition without specified return type,
            // and the function must have been declared elsewhere before.
            // Since declarations must have return type specified,
            // incomplete prototype provided by definition is no longer necessary.
            // Consistency check will be delayed to the actual definition below.
            return;
        }
        size_t index = it->second;
        auto function = dynamic_cast<NamedFunctionReference *>(continuum->functions[index].get());
        decl->expect(function->prototype());
    } else {
        auto function = std::make_unique<NamedFunctionReference>();
        function->decl = decl;
        size_t index = continuum->functions.size();
        declaredIndices.back().insert_or_assign(name0, index);
        continuum->functions.emplace_back(std::move(function));
        decl->index = index;
    }
}

void LocalContext::define(std::string_view name, FnDefExpr* def) {
    if (name == "_") {
        Error().with(
                ErrorMessage().error(def->segment())
                .text("function name must not be").quote("_")
                ).raise();
    }
    std::string name0(name);
    if (auto it = declaredIndices.back().find(name0); it != declaredIndices.back().end()) {
        size_t index = it->second;
        auto function = dynamic_cast<NamedFunctionReference*>(continuum->functions[index].get());
        function->decl->expect(def->typeCache);
        function->def = def;
        declaredIndices.back().erase(it);
        definedIndices.back().insert_or_assign(name0, index);
        def->index = index;
    } else {
        unreachable();
    }
}

void LocalContext::lambda(LambdaExpr* lambda) const {
    size_t index = continuum->functions.size();
    continuum->functions.emplace_back(std::make_unique<LambdaFunctionReference>(lambda));
    lambda->index = index;
}

void LocalContext::defineExternal(std::string_view name, std::shared_ptr<FuncType> const& prototype) {
    size_t index = continuum->functions.size();
    definedIndices.back().insert_or_assign(std::string(name), index);
    continuum->functions.emplace_back(std::make_unique<ExternalFunctionReference>(prototype));
}

LocalContext::LookupResult LocalContext::lookup(Compiler& compiler, Token token, bool local) const {
    std::string name(compiler.of(token));
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
            auto function = dynamic_cast<NamedFunctionReference*>(continuum->functions[index].get());
            if (function->decl->parameters->prototype->R == nullptr)
                raise("recursive function without specified return type", function->decl->segment());
            return {function->decl->parameters->prototype, index, LookupResult::Scope::FUNCTION};
        }
    }
    for (auto it = definedIndices.rbegin(); it != definedIndices.rend(); ++it) {
        if (auto lookup = it->find(name); lookup != it->end()) {
            size_t index = lookup->second;
            return {continuum->functions[index]->prototype(), index, LookupResult::Scope::FUNCTION};
        }
    }
    if (parent) {
        return parent->lookup(compiler, token, false);
    }
    raise("unable to resolve this identifier", token);
}

}