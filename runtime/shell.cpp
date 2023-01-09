#include "common.hpp"
#include "../common.hpp"
#include "interpretation.hpp"


int main(int argc, const char* argv[]) try {
    Porkchop::forceUTF8();
    const int argi = 1;
    Porkchop::Continuum continuum;
    Porkchop::Interpretation interpretation;
    Porkchop::VM vm;
    vm.init(argi, argc, argv);
    auto frame = std::make_unique<Porkchop::Frame>(&vm, &interpretation, nullptr);
    bool newline = false;
    while (true) {
        if (newline)
            fputs("\n", stdout);
        newline = true;
        Porkchop::Source source;
        try {
            do {
                if (source.lines.empty()) {
                    fputs(">>> ", stdout);
                } else {
                    fputs("... ", stdout);
                }
                fflush(stdout);
                source.append(Porkchop::readLine(stdin));
            } while (!source.braces.empty() || source.lines.back().ends_with('\\'));
        } catch (Porkchop::Error& e) {
            e.report(&source, false);
            continue;
        }
        if (source.tokens.empty()) {
            newline = false;
            continue;
        }
        Porkchop::Compiler compiler(&continuum, std::move(source));
        try {
            compiler.parse(true);
        } catch (Porkchop::Error& e) {
            e.report(&compiler.source, false);
            continue;
        }
        compiler.compile(&interpretation);
        try {
            frame->instructions = &std::get<Porkchop::Instructions>(interpretation.functions.back());
            frame->init();
            auto ret = frame->loop();
            auto type = compiler.continuum->functions.back()->prototype()->R;
            if (!Porkchop::isNone(type)) {
                auto sf = Porkchop::stringifier(type);
                fputs(sf(ret).c_str(), stdout);
            }
        } catch (Porkchop::Exception& e) { // FIXME indeterminate local variable caused by exception
            e.append("at func " + std::to_string(interpretation.functions.size() - 1));
            fprintf(stderr, "Runtime exception occurred: \n");
            fprintf(stderr, "%s", e.what());
        }
    }
} catch (std::bad_alloc& e) {
    fprintf(stderr, "Shell out of memory\n");
    std::exit(-10);
} catch (std::exception& e) {
    fprintf(stderr, "Internal Shell Error: %s\n", e.what());
    std::exit(-100);
}