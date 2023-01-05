#pragma once

#include "frame.hpp"

namespace Porkchop {

inline void execute(Assembly* assembly, int argc, const char* argv[]) {
    Porkchop::VM vm;
    Porkchop::Externals::init(&vm, argc, argv);
    Porkchop::Func main_{assembly->prototypes.size() - 1, assembly->prototypes.back()};
    auto ret = main_.call(assembly, &vm);
    std::exit(ret.$int);
}

inline void catching(void proc(int, const char*[]), int argc, const char* argv[]) try {
    proc(argc, argv);
} catch (std::bad_alloc& e) {
    fprintf(stderr, "Out of memory\n");
    std::exit(-10);
} catch (Porkchop::Exception& e) {
    fprintf(stderr, "Runtime exception occurred: \n");
    fprintf(stderr, "%s\n", e.what());
    std::exit(1);
} catch (std::exception& e) {
    fprintf(stderr, "Internal Runtime Error: %s\n", e.what());
    std::exit(-100);
}

}