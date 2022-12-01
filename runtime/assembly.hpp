#pragma once

#include <variant>

#include "../opcode.hpp"
#include "external.hpp"


namespace Porkchop {

using Instructions = std::vector<std::pair<Opcode, std::variant<
        std::monostate,
        std::size_t,
        std::string
>>>;

struct Assembly {
    std::vector<std::variant<Instructions, ExternalFunction>> functions;

    void externalFunctions() {
        functions.emplace_back(Externals::print);
        functions.emplace_back(Externals::println);
        functions.emplace_back(Externals::readLine);
        functions.emplace_back(Externals::i2s);
        functions.emplace_back(Externals::f2s);
        functions.emplace_back(Externals::s2i);
        functions.emplace_back(Externals::s2f);
        functions.emplace_back(Externals::exit);
        functions.emplace_back(Externals::millis);
        functions.emplace_back(Externals::nanos);
        functions.emplace_back(Externals::getargs);
        functions.emplace_back(Externals::output);
        functions.emplace_back(Externals::input);
        functions.emplace_back(Externals::flush);
        functions.emplace_back(Externals::eof);
    }
};

}