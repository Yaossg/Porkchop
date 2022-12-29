#include "runtime.hpp"

#include <chrono>

namespace Porkchop::Externals {

static FILE* out = stdout;
static FILE* in = stdin;
static bool disableIO = false;

static List* _args;

std::string& as_string($union value) {
    return dynamic_cast<String*>(value.$object)->value;
}

void init(VM* vm, int argc, const char *argv[]) {
    _args = vm->newObject<ObjectList>(std::vector<$union>{}, std::make_shared<ListType>(ScalarTypes::STRING));
    for (size_t i = 2; i < argc; ++i) {
        _args->append(vm->newObject<String>(argv[i]));
    }
    if (getenv("PORKCHOP_IO_DISABLE")) {
        disableIO = true;
    }
}

$union print(VM* vm, const std::vector<$union> &args) {
    fputs(as_string(args[0]).c_str(), out);
    return nullptr;
}

$union println(VM* vm, const std::vector<$union> &args) {
    print(vm, args);
    fputc('\n', out);
    fflush(out);
    return nullptr;
}

$union readLine(VM* vm, const std::vector<$union> &args) {
    char line[1024];
    fgets(line, sizeof line, in);
    return vm->newObject<String>(line);
}

$union i2s(VM* vm, const std::vector<$union> &args) {
    return vm->newObject<String>(std::to_string(args[0].$int));
}

$union f2s(VM* vm, const std::vector<$union> &args) {
    return vm->newObject<String>(std::to_string(args[0].$int));
}

$union s2i(VM* vm, const std::vector<$union> &args) {
    return std::stoll(as_string(args[0]));
}

$union s2f(VM* vm, const std::vector<$union> &args) {
    return std::stod(as_string(args[0]));
}

$union exit(VM* vm, const std::vector<$union> &args) {
    auto ret = args[0];
    fprintf(stdout, "\nProgram finished with exit code %uz\n", ret);
    std::exit(0);
}

$union millis(VM* vm, const std::vector<$union> &args) {
    return std::chrono::system_clock::now().time_since_epoch().count() / 1'000'000LL;
}

$union nanos(VM* vm, const std::vector<$union> &args) {
    return std::chrono::system_clock::now().time_since_epoch().count();
}

$union getargs(VM* vm, const std::vector<$union> &args) {
    return _args;
}

$union output(VM* vm, const std::vector<$union> &args) {
    if (disableIO || !(out = fopen(as_string(args[0]).c_str(), "w"))) {
        throw Runtime::Exception("failed to reopen output stream");
    }
    return nullptr;
}

$union input(VM* vm, const std::vector<$union> &args) {
    if (disableIO || !(in = fopen(as_string(args[0]).c_str(), "r"))) {
        throw Runtime::Exception("failed to reopen input stream");
    }
    return nullptr;
}

$union flush(VM* vm, const std::vector<$union> &args) {
    fflush(out);
    return nullptr;
}

$union eof(VM* vm, const std::vector<$union> &args) {
    return (bool)feof(in);
}

$union typename_(VM* vm, const std::vector<$union> &args) {
    auto name = std::bit_cast<Object*>(args[0])->getType()->toString();
    return vm->newObject<String>(name);
}

}