#pragma once

#include <functional>
#include <string>
#include <vector>
#include <bit>
#include <chrono>


namespace Porkchop {

using ExternalFunction = std::function<std::size_t(std::vector<size_t> const &)>;

namespace Externals {

inline size_t print(std::vector<size_t> const &args) {
    fputs(std::bit_cast<std::string *>(args[0])->c_str(), stdout);
    return 0;
}

inline size_t println(std::vector<size_t> const &args) {
    puts(std::bit_cast<std::string *>(args[0])->c_str());
    return 0;
}

inline size_t readLine(std::vector<size_t> const &args) {
    char line[1024];
    fgets(line, sizeof line, stdin);
    return std::bit_cast<size_t>(new std::string(line));
}

inline size_t i2s(std::vector<size_t> const &args) {
    return std::bit_cast<size_t>(new std::string(std::to_string((int64_t) args[0])));
}

inline size_t f2s(std::vector<size_t> const &args) {
    return std::bit_cast<size_t>(new std::string(std::to_string(std::bit_cast<double>(args[0]))));
}

inline size_t s2i(std::vector<size_t> const &args) {
    return std::stoll(*std::bit_cast<std::string *>(args[0]));
}

inline size_t s2f(std::vector<size_t> const &args) {
    return std::bit_cast<size_t>(std::stod(*std::bit_cast<std::string *>(args[0])));
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

}

}