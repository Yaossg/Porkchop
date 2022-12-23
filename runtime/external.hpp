#pragma once

#include <functional>
#include <vector>

#include "../util.hpp"


namespace Porkchop {

struct VM;

using ExternalFunctionR = std::function<std::size_t(VM*, std::vector<size_t> const &)>;

namespace Externals {

void init(VM* vm, int argc, const char* argv[]);
size_t print(VM* vm, std::vector<size_t> const &args);
size_t println(VM* vm, std::vector<size_t> const &args);
size_t readLine(VM* vm, std::vector<size_t> const &args);
size_t i2s(VM* vm, std::vector<size_t> const &args);
size_t f2s(VM* vm, std::vector<size_t> const &args);
size_t s2i(VM* vm, std::vector<size_t> const &args);
size_t s2f(VM* vm, std::vector<size_t> const &args);
size_t exit(VM* vm, std::vector<size_t> const &args);
size_t millis(VM* vm, std::vector<size_t> const &args);
size_t nanos(VM* vm, std::vector<size_t> const &args);
size_t getargs(VM* vm, std::vector<size_t> const &args);
size_t output(VM* vm, std::vector<size_t> const &args);
size_t input(VM* vm, std::vector<size_t> const &args);
size_t flush(VM* vm, std::vector<size_t> const &args);
size_t eof(VM* vm, std::vector<size_t> const &args);
size_t typename_(VM* vm, std::vector<size_t> const &args);

}

}