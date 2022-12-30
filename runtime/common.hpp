#pragma once

#include "runtime.hpp"

namespace Porkchop {

inline std::string input(const char* name, int argc, const char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Fatal: Too few arguments, input file expected\n");
        fprintf(stderr, "Usage: %s <input> [args...]\n", name);
        std::exit(10);
    }
    return Porkchop::readAll(argv[1]);
}

inline void execute(Assembly& assembly, int argc, const char* argv[]) {
    Porkchop::VM vm;
    Porkchop::Externals::init(&vm, argc, argv);
    Porkchop::Func main_{0, assembly.prototypes[0]};
    auto ret = main_.call(&assembly, &vm);
    std::exit(ret.$int);
}

inline void catching(void proc(int, const char*[]), int argc, const char* argv[]) try {
    proc(argc, argv);
} catch (std::bad_alloc& e) {
    fprintf(stderr, "Out of memory\n");
    std::exit(-10);
} catch (Porkchop::Runtime::Exception& e) {
    fprintf(stderr, "Runtime exception occurred: \n");
    fprintf(stderr, "%s\n", e.what());
    std::exit(1);
} catch (std::exception& e) {
    fprintf(stderr, "Internal Runtime Error: %s\n", e.what());
    std::exit(-100);
}

}