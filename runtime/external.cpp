#include "runtime.hpp"

#include <chrono>

namespace Porkchop::Externals {

static FILE* out = stdout;
static FILE* in = stdin;
static bool disableIO = false;

static List* _args;

std::string& as_string(size_t value) {
    return std::bit_cast<String*>(value)->value;
}

void init(VM* vm, int argc, const char *argv[]) {
    _args = vm->newObject<List>(std::vector<size_t>{}, std::make_shared<ListType>(ScalarTypes::STRING));
    for (size_t i = 2; i < argc; ++i) {
        _args->elements.push_back(as_size(vm->newObject<String>(argv[i])));
    }
    if (getenv("PORKCHOP_IO_DISABLE")) {
        disableIO = true;
    }
}

size_t print(VM* vm, const std::vector<size_t> &args) {
    fputs(as_string(args[0]).c_str(), out);
    return 0;
}

size_t println(VM* vm, const std::vector<size_t> &args) {
    print(vm, args);
    fputc('\n', out);
    fflush(out);
    return 0;
}

size_t readLine(VM* vm, const std::vector<size_t> &args) {
    char line[1024];
    fgets(line, sizeof line, in);
    return as_size(vm->newObject<String>(line));
}

size_t i2s(VM* vm, const std::vector<size_t> &args) {
    return as_size(vm->newObject<String>(std::to_string((int64_t) args[0])));
}

size_t f2s(VM* vm, const std::vector<size_t> &args) {
    return as_size(vm->newObject<String>(std::to_string(as_double(args[0]))));
}

size_t s2i(VM* vm, const std::vector<size_t> &args) {
    return std::stoll(as_string(args[0]));
}

size_t s2f(VM* vm, const std::vector<size_t> &args) {
    return as_size(std::stod(as_string(args[0])));
}

size_t exit(VM* vm, const std::vector<size_t> &args) {
    size_t ret = args[0];
    fprintf(stdout, "\nProgram finished with exit code %zu\n", ret);
    std::exit(ret);
}

size_t millis(VM* vm, const std::vector<size_t> &args) {
    return std::chrono::system_clock::now().time_since_epoch().count() / 1'000'000LL;
}

size_t nanos(VM* vm, const std::vector<size_t> &args) {
    return std::chrono::system_clock::now().time_since_epoch().count();
}

size_t getargs(VM* vm, const std::vector<size_t> &args) {
    return as_size(_args);
}

size_t output(VM* vm, const std::vector<size_t> &args) {
    if (disableIO || !(out = fopen(as_string(args[0]).c_str(), "w")))
        throw std::runtime_error("failed to reopen output stream");
    return 0;
}

size_t input(VM* vm, const std::vector<size_t> &args) {
    if (disableIO || !(in = fopen(as_string(args[0]).c_str(), "r")))
        throw std::runtime_error("failed to reopen input stream");
    return 0;
}

size_t flush(VM* vm, const std::vector<size_t> &args) {
    fflush(out);
    return 0;
}

size_t eof(VM* vm, const std::vector<size_t> &args) {
    return feof(in);
}

size_t typename_(VM* vm, const std::vector<size_t> &args) {
    auto name = std::bit_cast<Object*>(args[0])->getType()->toString();
    return as_size(vm->newObject<String>(name));
}

}