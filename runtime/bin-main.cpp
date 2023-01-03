#include "common.hpp"
#include "bin-assembly.hpp"

std::vector<uint8_t> readBin(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (file == nullptr) {
        fprintf(stderr, "Failed to open input file: %s\n", filename);
        std::exit(20);
    }
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);
    std::vector<uint8_t> fileBuffer(size);
    fread(fileBuffer.data(), 1, size, file);
    fclose(file);
    return fileBuffer;
}

void proc(int argc, const char *argv[]) {
    Porkchop::forceUTF8();
    if (argc < 2) {
        fprintf(stderr, "Fatal: Too few arguments, input file expected\n");
        fprintf(stderr, "Usage: PorkchopBinRuntime <input> [args...]\n");
        std::exit(10);
    }
    Porkchop::BinAssembly assembly(readBin(argv[1]));
    assembly.parse();
    Porkchop::execute(assembly, argc, argv);
}

int main(int argc, const char* argv[]) {
    Porkchop::catching(proc, argc, argv);
}