#pragma once

#include <vector>
#include <memory>

namespace Porkchop {

struct Assembler;
struct FunctionReference;
struct LocalContext;


struct Continuum {
    std::vector<std::unique_ptr<FunctionReference>> functions;
    std::unique_ptr<LocalContext> context;
    size_t labelUntil = 0;
    size_t funcUntil = 0;
    size_t localUntil = 0;

    Continuum();

    void compile(Assembler* assembler);
};


}
