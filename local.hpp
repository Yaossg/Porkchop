#pragma once


#include "type.hpp"

namespace Porkchop {

struct ReferenceContext {
    std::vector<std::unordered_map<std::string_view, TypeReference>> scopes{{}};
    SourceCode* sourcecode;
    explicit ReferenceContext(SourceCode* sourcecode): sourcecode(sourcecode) {
        global("println", std::make_shared<FuncType>(std::vector{ScalarTypes::STRING}, ScalarTypes::NONE));
        global("exit", std::make_shared<FuncType>(std::vector{ScalarTypes::INT}, ScalarTypes::NEVER));
    }
    void push() {
        scopes.emplace_back();
    }
    void pop() {
        scopes.pop_back();
    }
    void global(std::string_view name, TypeReference const& type) {
        scopes.front().emplace(name, type);
    }
    void local(std::string_view name, TypeReference const& type)  {
        scopes.back().emplace(name, type);
    }
    [[nodiscard]] TypeReference lookup(std::string_view name) const noexcept {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            if (auto lookup = it->find(name); lookup != it->end())
                return lookup->second;
        }
        return nullptr;
    }
    struct Guard {
        ReferenceContext& context;
        explicit Guard(ReferenceContext& context): context(context) {
            context.push();
        }
        ~Guard() {
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
