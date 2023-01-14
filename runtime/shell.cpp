#include "common.hpp"
#include "../common.hpp"
#include "interpretation.hpp"

void onCommand(std::string_view line, Porkchop::Continuum& continuum, std::unique_ptr<Porkchop::Frame>& frame) {
    size_t space = line.find(' ');
    std::string_view command = line.substr(0, space);
    auto args = space == std::string::npos ? "" : line.substr(space);
    if (args.starts_with(' '))
        args.remove_prefix(args.find_first_not_of(' '));
    if (command == "/help") {
        puts("/help");
        puts("/exit");
        puts("/lets");
        puts("/fns");
        puts("/drop <variable-name>");
    } else if (command == "/exit") {
        std::exit(0);
    } else if (command == "/lets") {
        for (auto&& [name, index] : continuum.context->localIndices.back()) {
            auto type = continuum.context->localTypes[index];
            auto sf = Porkchop::stringifier(type);
            fprintf(stdout, "let %s: %s = %s\n", Porkchop::render("\x1b[97m", name).c_str(), type->toString().c_str(), sf(frame->stack[index]).c_str());
        }
    } else if (command == "/fns") {
        for (auto&& [name, index] : continuum.context->definedIndices.back()) {
            auto&& function = continuum.functions[index];
            fprintf(stdout, "fn %s%s\n", Porkchop::render("\x1b[97m", name).c_str(), function->prototype()->toString().c_str());
        }
    } else if (command == "/drop") {
        if (space == std::string::npos) {
            Porkchop::Error error;
            error.with(Porkchop::ErrorMessage().usage().text("/drop <variable-name>"));
            error.report(nullptr, true);
        } else {
            std::string name(args);
            if (auto it = continuum.context->localIndices.back().find(name); it != continuum.context->localIndices.back().end()) {
                auto index = it->second;
                frame->stack[index] = nullptr;
                frame->companion[index] = false;
                continuum.context->localIndices.back().erase(it);
                fprintf(stdout, "variable '%s' dropped\n", Porkchop::render("\x1b[97m", name).c_str());
            } else {
                Porkchop::Error error;
                error.with(Porkchop::ErrorMessage().fatal().text("no such a variable called").quote(name));
                error.report(nullptr, true);
            }
        }
    } else {
        Porkchop::Error error;
        error.with(Porkchop::ErrorMessage().usage().text("/help"));
        error.report(nullptr, true);
    }
}

int main(int argc, const char* argv[]) try {
    Porkchop::forceUTF8();
    const int argi = 1;
    Porkchop::Continuum continuum;
    Porkchop::Interpretation interpretation(&continuum);
    Porkchop::VM vm;
    vm.init(argi, argc, argv);
    auto frame = std::make_unique<Porkchop::Frame>(&vm, &interpretation);
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
                    onCommand(line, continuum, frame);
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
            auto type = compiler.continuum->functions.back()->prototype()->R;
            frame->init(interpretation.functions.size() - 1);
            auto ret = frame->loop();
            if (!Porkchop::isNone(type)) {
                auto sf = Porkchop::stringifier(type);
                fputs(sf(ret).c_str(), stdout);
            }
        } catch (Porkchop::Exception& e) { // FIXME indeterminate local variable caused by exception
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