#include "../runtime/common.hpp"
#include "../common.hpp"
#include "../runtime/interpretation.hpp"

int main(int argc, const char* argv[]) {
    Porkchop::forceUTF8();
    const int argi = 2;
    if (argc < argi) {
        Porkchop::Error error;
        error.with(Porkchop::ErrorMessage().fatal().text("too few arguments, input file expected"));
        error.with(Porkchop::ErrorMessage().usage().text("PorkchopTest <input> [args...]"));
        error.report(nullptr);
        std::exit(10);
    }
    std::string input = argv[1];
    std::string output = input.substr(0, input.find_last_of('.')) + ".o";
    freopen(output.c_str(), "w", stdout);
    std::string original = Porkchop::readText(input.c_str());
    Porkchop::Source source;
    Porkchop::tokenize(source, original);
    Porkchop::Continuum continuum;
    Porkchop::Compiler compiler(&continuum, std::move(source));
    Porkchop::parse(compiler);
    std::ignore = compiler.descriptor(); // for coverage
    Porkchop::Interpretation interpretation(&continuum);
    compiler.compile(&interpretation);
    Porkchop::VM vm;
    vm.init(argi, argc, argv);
    auto ret = Porkchop::execute(&vm, &interpretation);
    auto type = compiler.continuum->functions.back()->prototype()->R;
    auto sf = Porkchop::stringifier(type);
    fputs("Exited with returned object: ", stdout);
    fputs(sf(ret).c_str(), stdout);
    fclose(stdout);
    auto o = Porkchop::readText(output.c_str());
    remove(output.c_str());
    std::string ref = input.substr(0, input.find_last_of('.')) + ".r";
    auto r = Porkchop::readText(ref.c_str());
    return !(r == o);
}
