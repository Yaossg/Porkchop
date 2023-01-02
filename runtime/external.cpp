#include "runtime.hpp"
#include "../unicode/unicode.hpp"

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
        _args->add(vm->newObject<String>(argv[i]));
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

$union parseInt(VM* vm, const std::vector<$union> &args) {
    return std::stoll(as_string(args[0]));
}

$union parseFloat(VM* vm, const std::vector<$union> &args) {
    return std::stod(as_string(args[0]));
}

$union exit(VM* vm, const std::vector<$union> &args) {
    auto ret = args[0];
    std::exit(ret.$int);
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
        throw Exception("failed to reopen output stream");
    }
    return nullptr;
}

$union input(VM* vm, const std::vector<$union> &args) {
    if (disableIO || !(in = fopen(as_string(args[0]).c_str(), "r"))) {
        throw Exception("failed to reopen input stream");
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
    auto name = args[0].$object->getType()->toString();
    return vm->newObject<String>(name);
}

$union gc(VM* vm, std::vector<$union> const &args) {
    vm->gc();
    return nullptr;
}

$union toBytes(VM* vm, std::vector<$union> const &args) {
    auto& string = as_string(args[0]);
    std::vector<uint8_t> bytes(string.begin(), string.end());
    return vm->newObject<ByteList>(std::move(bytes));
}

$union toChars(VM* vm, std::vector<$union> const &args) {
    auto& string = as_string(args[0]);
    std::vector<$union> chars;
    chars.reserve(string.length());
    try {
        UnicodeParser parser(string, 0, 0);
        while (parser.remains()) chars.emplace_back(parser.decodeUnicode());
    } catch (...) {
        throw Exception("failed to decode Unicode");
    }
    chars.shrink_to_fit();
    return vm->newObject<ScalarList>(std::move(chars), ScalarTypeKind::CHAR);
}

$union fromBytes(VM* vm, std::vector<$union> const &args) {
    auto list = dynamic_cast<ByteList*>(args[0].$object);
    std::string string{list->elements.begin(), list->elements.end()};
    return vm->newObject<String>(std::move(string));
}

$union fromChars(VM* vm, std::vector<$union> const &args) {
    auto list = dynamic_cast<ScalarList*>(args[0].$object);
    std::string string;
    string.reserve(list->elements.size());
    for (auto element : list->elements) {
        string += encodeUnicode(element.$char);
    }
    return vm->newObject<String>(std::move(string));
}


}