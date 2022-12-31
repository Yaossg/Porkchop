#pragma once

#include <variant>

#include "../opcode.hpp"
#include "../type.hpp"
#include "external.hpp"


namespace Porkchop {

using Instructions = std::vector<std::pair<Opcode, std::variant<
        std::monostate,
        size_t,
        std::string,
        TypeReference,
        std::pair<TypeReference, size_t>
>>>;

struct Assembly {
    std::vector<std::variant<Instructions, ExternalFunctionR>> functions;
    std::vector<std::string> table;
    std::vector<std::shared_ptr<FuncType>> prototypes;

    void externalFunctions() {
        functions.emplace_back(Externals::print);
        functions.emplace_back(Externals::println);
        functions.emplace_back(Externals::readLine);
        functions.emplace_back(Externals::parseInt);
        functions.emplace_back(Externals::parseFloat);
        functions.emplace_back(Externals::exit);
        functions.emplace_back(Externals::millis);
        functions.emplace_back(Externals::nanos);
        functions.emplace_back(Externals::getargs);
        functions.emplace_back(Externals::output);
        functions.emplace_back(Externals::input);
        functions.emplace_back(Externals::flush);
        functions.emplace_back(Externals::eof);
        functions.emplace_back(Externals::typename_);
        functions.emplace_back(Externals::gc);
    }
};

}