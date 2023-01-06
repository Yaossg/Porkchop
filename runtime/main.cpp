#include "common.hpp"
#include "text-assembly.hpp"
#include "bin-assembly.hpp"

int main(int argc, const char* argv[]) {
    Porkchop::forceUTF8();
    const int argi = 3;
    if (argc < argi) {
        fprintf(stderr, "Fatal: Too few arguments, input file and its type expected\n");
        fprintf(stderr, "Usage: PorkchopRuntime <type> <input> [args...]\n");
        std::exit(10);
    }
    const char* input_type = argv[1];
    const char* input_file = argv[2];
    enum class InputType {
        TEXT, BIN
    } parsedType;
    if (!strcmp("-t", input_type) || !strcmp("--text-asm", input_type)) {
        parsedType = InputType::TEXT;
    } else if (!strcmp("-b", input_type) || !strcmp("--bin-asm", input_type)) {
        parsedType = InputType::BIN;
    } else {
        fprintf(stderr, "Fatal: Unknown input type: %s\n", input_type);
        std::exit(11);
    }
    std::unique_ptr<Porkchop::Assembly> assembly;
    if (parsedType == InputType::TEXT) {
        assembly = std::make_unique<Porkchop::TextAssembly>(Porkchop::readText(input_file));
    } else {
        assembly = std::make_unique<Porkchop::BinAssembly>(Porkchop::readBin(input_file));
    }
    Porkchop::VM vm;
    vm.init(argi, argc, argv);
    return Porkchop::execute(&vm, assembly.get());
}