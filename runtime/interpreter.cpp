#include "common.hpp"
#include "../common.hpp"
#include "interpretation.hpp"

void proc(int argc, const char* argv[]) {
    Porkchop::forceUTF8();
    if (argc < 2) {
        fprintf(stderr, "Fatal: Too few arguments, input file expected\n");
        fprintf(stderr, "Usage: PorkchopInterpreter <input> [args...]\n");
        std::exit(10);
    }
    std::string original = Porkchop::readText(argv[1]);
    Porkchop::Compiler compiler(std::move(original));
    Porkchop::parse(compiler);
    Porkchop::Interpretation interpretation;
    compiler.compile(&interpretation);
    Porkchop::execute(&interpretation, argc, argv);

}

int main(int argc, const char* argv[]) {
    Porkchop::catching(proc, argc, argv);
}
