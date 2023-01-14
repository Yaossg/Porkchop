#pragma once

#include "frame.hpp"

namespace Porkchop {

inline $union execute(VM* vm, Assembly* assembly) try {
    return call(assembly, vm, assembly->functions.size() - 1, {});
} catch (Exception& e) {
    fprintf(stderr, "Runtime exception occurred: \n");
    fprintf(stderr, "%s\n", e.what());
    std::exit(1);
} catch (std::bad_alloc& e) {
    fprintf(stderr, "Runtime out of memory\n");
    std::exit(-10);
} catch (std::exception& e) {
    fprintf(stderr, "Internal Runtime Error: %s\n", e.what());
    std::exit(-100);
}

}