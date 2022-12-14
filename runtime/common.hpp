#pragma once

#include "frame.hpp"

namespace Porkchop {

inline $union execute(VM* vm, Assembly* assembly) try {
    auto frame = std::make_unique<Frame>(vm, assembly, &std::get<Instructions>(assembly->functions.back()));
    frame->init();
    return frame->loop();
} catch (Exception& e) {
    e.append("at func " + std::to_string(assembly->functions.size() - 1));
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