#pragma once

#include <functional>
#include <vector>

#include "../util.hpp"


namespace Porkchop {

using ExternalFunctionR = std::function<std::size_t(std::vector<size_t> const &)>;

namespace Externals {

void init(int argc, const char* argv[]);
size_t print(std::vector<size_t> const &args);
size_t println(std::vector<size_t> const &args);
size_t readLine(std::vector<size_t> const &args);
size_t i2s(std::vector<size_t> const &args);
size_t f2s(std::vector<size_t> const &args);
size_t s2i(std::vector<size_t> const &args);
size_t s2f(std::vector<size_t> const &args);
size_t exit(std::vector<size_t> const &args);
size_t millis(std::vector<size_t> const &args);
size_t nanos(std::vector<size_t> const &args);
size_t getargs(std::vector<size_t> const &args);
size_t output(std::vector<size_t> const &args);
size_t input(std::vector<size_t> const &args);
size_t flush(std::vector<size_t> const &args);
size_t eof(std::vector<size_t> const &args);
size_t typename_(std::vector<size_t> const &args);

}

}