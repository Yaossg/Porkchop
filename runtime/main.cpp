#include "common.hpp"
#include "text-assembly.hpp"

void proc(int argc, const char *argv[]) {
    Porkchop::forceUTF8();
    Porkchop::TextAssembly assembly(Porkchop::input("PorkchopRuntime", argc, argv));
    assembly.parse();
    Porkchop::execute(assembly, argc, argv);
}

int main(int argc, const char* argv[]) {
    Porkchop::catching(proc, argc, argv);
}