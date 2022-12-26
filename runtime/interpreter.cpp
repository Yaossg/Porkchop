#include "common.hpp"
#include "../common.hpp"
#include "interpretation.hpp"

void proc(int argc, const char* argv[]) {
    Porkchop::forceUTF8();
    Porkchop::Compiler compiler(Porkchop::input("PorkchopInterpreter", argc, argv));
    Porkchop::parse(compiler);
    Porkchop::Interpretation interpretation;
    compiler.compile(&interpretation);
    Porkchop::execute(interpretation, argc, argv);

}

int main(int argc, const char* argv[]) {
    Porkchop::catching(proc, argc, argv);
}
