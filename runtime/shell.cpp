#include "common.hpp"
#include "../common.hpp"
#include "interpretation.hpp"

int main(int argc, const char* argv[]) try {
    Porkchop::forceUTF8();
    const int argi = 1;
    Porkchop::Continuum continuum;
    Porkchop::Interpretation interpretation(&continuum);
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
                auto line = Porkchop::readLine(stdin);
                if (source.lines.empty() && line.starts_with('/')) {
                    if (line == "/help") {
                        puts("/help");
                        puts("/exit");
                        puts("/lets");
                        puts("/fns");
                        puts("/drop <variable-name>");
                    } else if (line == "/exit") {
                        std::exit(0);
                    } else if (line == "/lets") {
                        for (auto&& [name, index] : continuum.context->localIndices.back()) {
                            auto type = continuum.context->localTypes[index];
                            auto sf = Porkchop::stringifier(type);
                            fprintf(stdout, "let %s : %s = %s\n", name.c_str(), type->toString().c_str(), sf(frame->stack[index]).c_str());
                        }
                    } else if (line == "/fns") {
                        for (auto&& [name, index] : continuum.context->definedIndices.back()) {
                            auto&& function = continuum.functions[index];
                            fprintf(stdout, "fn %s%s\n", name.c_str(), function->prototype()->toString().c_str());
                        }
                    } else if (line.starts_with("/drop ")) {
                        auto start = line.find_first_not_of(' ', 6);
                        if (start == std::string::npos) {
                            Porkchop::Error error;
                            error.with(Porkchop::ErrorMessage().usage().text("/drop <variable-name>"));
                            error.report(nullptr, true);
                        } else {
                            auto name = line.substr(start);
                            if (auto it = continuum.context->localIndices.back().find(name); it != continuum.context->localIndices.back().end()) {
                                auto index = it->second;
                                frame->stack[index] = nullptr;
                                frame->companion[index] = false;
                                continuum.context->localIndices.back().erase(it);
                                fprintf(stdout, "variable '%s' dropped\n", name.c_str());
                            } else {
                                Porkchop::Error error;
                                error.with(Porkchop::ErrorMessage().fatal().text("no such a variable called").quote(name));
                                error.report(nullptr, true);
                            }
                        }
                    } else if (line == "/drop") {
                        Porkchop::Error error;
                        error.with(Porkchop::ErrorMessage().usage().text("/drop <variable-name>"));
                        error.report(nullptr, true);
                    }
                    break;
                }
                source.append(std::move(line));
            } while (!source.greedy.empty() || source.lines.back().ends_with('\\'));
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
            compiler.parse(Porkchop::Compiler::Mode::SHELL);
            continuum.context->checkDeclared();
        } catch (Porkchop::Error& e) {
            e.report(&compiler.source, false);
            continue;
        }
        compiler.compile(&interpretation);
        try {
            frame->instructions = &std::get<Porkchop::Instructions>(interpretation.functions.back());
            auto type = compiler.continuum->functions.back()->prototype()->R;
            frame->init();
            auto ret = frame->loop();
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