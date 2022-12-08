#include "runtime.hpp"

#include <chrono>

namespace Porkchop::Externals {

static FILE* out = stdout;
static FILE* in = stdin;
static bool disableIO = false;

static const auto _args = new std::vector<size_t>();

void init(int argc, const char **argv) {
    for (size_t i = 2; i < argc; ++i) {
        _args->push_back(as_size(new std::string(argv[i])));
    }
    if (getenv("PORKCHOP_IO_DISABLE")) {
        disableIO = true;
    }
}

size_t print(const std::vector<size_t> &args) {
    fputs(as_string(args[0])->c_str(), out);
    return 0;
}

size_t println(const std::vector<size_t> &args) {
    print(args);
    fputc('\n', out);
    fflush(out);
    return 0;
}

size_t readLine(const std::vector<size_t> &args) {
    char line[1024];
    fgets(line, sizeof line, in);
    return as_size(new std::string(line));
}

size_t i2s(const std::vector<size_t> &args) {
    return as_size(new std::string(std::to_string((int64_t) args[0])));
}

size_t f2s(const std::vector<size_t> &args) {
    return as_size(new std::string(std::to_string(as_double(args[0]))));
}

size_t s2i(const std::vector<size_t> &args) {
    return std::stoll(*as_string(args[0]));
}

size_t s2f(const std::vector<size_t> &args) {
    return as_size(std::stod(*as_string(args[0])));
}

size_t exit(const std::vector<size_t> &args) {
    size_t ret = args[0];
    fprintf(stdout, "\nProgram finished with exit code %zu\n", ret);
    std::exit(ret);
}

size_t millis(const std::vector<size_t> &args) {
    return std::chrono::system_clock::now().time_since_epoch().count() / 1'000'000LL;
}

size_t nanos(const std::vector<size_t> &args) {
    return std::chrono::system_clock::now().time_since_epoch().count();
}

size_t getargs(const std::vector<size_t> &args) {
    return as_size(_args);
}

size_t output(const std::vector<size_t> &args) {
    if (disableIO || !(out = fopen(as_string(args[0])->c_str(), "w")))
        throw std::runtime_error("failed to reopen output stream");
    return 0;
}

size_t input(const std::vector<size_t> &args) {
    if (disableIO || !(in = fopen(as_string(args[0])->c_str(), "r")))
        throw std::runtime_error("failed to reopen input stream");
    return 0;
}

size_t flush(const std::vector<size_t> &args) {
    fflush(out);
    return 0;
}

size_t eof(const std::vector<size_t> &args) {
    return feof(in);
}

size_t typename_(const std::vector<size_t> &args) {
    auto any = std::bit_cast<Runtime::Any*>(args[0]);
    return as_size(new std::string(any->type->toString()));
}

}