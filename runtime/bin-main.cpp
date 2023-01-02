#include "common.hpp"
#include "bin-assembly.hpp"

void proc(int argc, const char *argv[]) {
    Porkchop::forceUTF8();
    if (argc < 2) {
        fprintf(stderr, "Fatal: Too few arguments, input file expected\n");
        fprintf(stderr, "Usage: PorkchopBinRuntime <input> [args...]\n");
        std::exit(10);
    }
    const char* filename = argv[1];
    FILE* input_file = fopen(filename, "rb");
    if (input_file == nullptr) {
        fprintf(stderr, "Failed to open input file: %s\n", filename);
        std::exit(20);
    }
    Porkchop::BinAssembly assembly(input_file);
    assembly.parse();
    Porkchop::execute(assembly, argc, argv);
}

int main(int argc, const char* argv[]) {
    Porkchop::catching(proc, argc, argv);
}