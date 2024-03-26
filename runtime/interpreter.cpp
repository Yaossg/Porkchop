#include "common.hpp"
#include "../common.hpp"
#include "interpretation.hpp"

int main(int argc, const char* argv[]) {
    Porkchop::forceUTF8();
    const int argi = 2;
    if (argc < argi) {
        Porkchop::Error error;
        error.with(Porkchop::ErrorMessage().fatal().text("too few arguments, input file expected"));
        error.with(Porkchop::ErrorMessage().usage().text("PorkchopInterpreter <input> [args...]"));
        error.report(nullptr);
        std::exit(10);
    }
    std::string original = Porkchop::readText(argv[1]);
    Porkchop::Source source;
    Porkchop::tokenize(source, original);
    Porkchop::Continuum continuum;
    Porkchop::Compiler compiler(&continuum, std::move(source));
    Porkchop::parse(compiler);
    Porkchop::Interpretation interpretation(&continuum);
    compiler.compile(&interpretation);
    Porkchop::VM vm;
    vm.init(argi, argc, argv);
    return (int) Porkchop::execute(&vm, &interpretation).$int;
}
