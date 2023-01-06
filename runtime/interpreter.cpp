#include "common.hpp"
#include "../common.hpp"
#include "interpretation.hpp"

int main(int argc, const char* argv[]) {
    Porkchop::forceUTF8();
    const int argi = 2;
    if (argc < argi) {
        fprintf(stderr, "Fatal: Too few arguments, input file expected\n");
        fprintf(stderr, "Usage: PorkchopInterpreter <input> [args...]\n");
        std::exit(10);
    }
    std::string original = Porkchop::readText(argv[1]);
    Porkchop::Source source;
    Porkchop::tokenize(source, original);
    Porkchop::Continuum continuum;
    Porkchop::Compiler compiler(&continuum, std::move(source));
    Porkchop::parse(compiler);
    Porkchop::Interpretation interpretation;
    compiler.compile(&interpretation);
    Porkchop::VM vm;
    vm.init(argi, argc, argv);
    return Porkchop::execute(&vm, &interpretation);
}
