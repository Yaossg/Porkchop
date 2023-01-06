#include "common.hpp"
#include "../common.hpp"
#include "interpretation.hpp"


int main(int argc, const char* argv[]) {
    Porkchop::forceUTF8();
    const int argi = 1;
    Porkchop::Continuum continuum;
    Porkchop::Interpretation interpretation;
    Porkchop::VM vm;
    vm.init(argi, argc, argv);
    auto frame = std::make_unique<Porkchop::Frame>(&vm, &interpretation, nullptr);
    while (true) {
        Porkchop::Source source;
        try {
            do {
                if (source.lineNo) {
                    fputs("... ", stdout);
                } else {
                    fputs(">>> ", stdout);
                }
                fflush(stdout);
                std::string line = Porkchop::readLine(stdin);
                source.append(std::move(line));
            } while (!source.braces.empty());
        } catch (Porkchop::SegmentException& e) {
            fprintf(stderr, "%s\n", e.message(source).c_str());
            continue;
        }
        if (source.tokens.empty()) continue;
        Porkchop::Compiler compiler(&continuum, std::move(source));
        try {
            compiler.parse();
        } catch (Porkchop::SegmentException& e) {
            fprintf(stderr, "%s\n", e.message(compiler.source).c_str());
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
                puts(sf(ret).c_str());
            }
        } catch (Porkchop::Exception& e) {
            e.append("at func " + std::to_string(interpretation.functions.size() - 1));
            fprintf(stderr, "Runtime exception occurred: \n");
            fprintf(stderr, "%s\n", e.what());
            continue;
        } catch (std::bad_alloc& e) {
            fprintf(stderr, "Out of memory\n");
            std::exit(-10);
        } catch (std::exception& e) {
            fprintf(stderr, "Internal Runtime Error: %s\n", e.what());
            std::exit(-100);
        }
    }
}