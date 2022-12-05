#pragma once

#include <functional>
#include <string>
#include <vector>
#include <bit>
#include <chrono>

#include "../util.hpp"


namespace Porkchop {

using ExternalFunctionR = std::function<std::size_t(std::vector<size_t> const &)>;

namespace Externals {

static FILE* out = stdout;
static FILE* in = stdin;
static bool disableIO = false;

static const auto _args = new std::vector<size_t>();

void init(int argc, const char* argv[]) {
    for (size_t i = 2; i < argc; ++i) {
        _args->push_back(as_size(new std::string(argv[i])));
    }
    if (getenv("PORKCHOP_IO_DISABLE")) {
        disableIO = true;
    }
}

inline size_t print(std::vector<size_t> const &args) {
    fputs(as_string(args[0])->c_str(), out);
    return 0;
}

inline size_t println(std::vector<size_t> const &args) {
    print(args);
    fputc('\n', out);
    fflush(out);
    return 0;
}

inline size_t readLine(std::vector<size_t> const &args) {
    char line[1024];
    fgets(line, sizeof line, in);
    return as_size(new std::string(line));
}

inline size_t i2s(std::vector<size_t> const &args) {
    return as_size(new std::string(std::to_string((int64_t) args[0])));
}

inline size_t f2s(std::vector<size_t> const &args) {
    return as_size(new std::string(std::to_string(as_double(args[0]))));
}

inline size_t s2i(std::vector<size_t> const &args) {
    return std::stoll(*as_string(args[0]));
}

inline size_t s2f(std::vector<size_t> const &args) {
    return as_size(std::stod(*as_string(args[0])));
}

inline size_t exit(std::vector<size_t> const &args) {
    std::exit(args[0]);
}

inline size_t millis(std::vector<size_t> const &args) {
    return std::chrono::system_clock::now().time_since_epoch().count() / 1'000'000LL;
}

inline size_t nanos(std::vector<size_t> const &args) {
    return std::chrono::system_clock::now().time_since_epoch().count();
}

inline size_t getargs(std::vector<size_t> const &args) {
    return as_size(_args);
}

inline size_t output(std::vector<size_t> const &args) {
    if (disableIO || !(out = fopen(as_string(args[0])->c_str(), "w")))
        throw std::runtime_error("failed to reopen output stream");
    return 0;
}

inline size_t input(std::vector<size_t> const &args) {
    if (disableIO || !(in = fopen(as_string(args[0])->c_str(), "r")))
        throw std::runtime_error("failed to reopen input stream");
    return 0;
}

inline size_t flush(std::vector<size_t> const &args) {
    fflush(out);
    return 0;
}

inline size_t eof(std::vector<size_t> const &args) {
    return feof(in);
}

}

}