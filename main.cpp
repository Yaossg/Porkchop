#include "parser.hpp"
#include "function.hpp"

#include "io.hpp"

std::unordered_map<std::string, std::string> parseArgs(int argc, const char* argv[]) {
    std::unordered_map<std::string, std::string> args;
    if (argc < 2) {
        fprintf(stderr, "Fatal: Too few arguments, input file expected\n");
        fprintf(stderr, "Usage: Porkchop <input> [options...]\n");
        exit(10);
    }
    args["this"] = argv[0];
    args["input"] = argv[1];
    for (int i = 2; i < argc; ++i) {
        if (!strcmp("-o", argv[i])) {
            ++i;
            args["output"] = argv[i];
        } else if (!strcmp("-m", argv[i]) || !strcmp("--mermaid", argv[i])) {
            args["type"] = "mermaid";
        } else if (!strcmp("-t", argv[i]) || !strcmp("--text-asm", argv[i])) {
            args["type"] = "text-asm";
        } else {
            fprintf(stderr, "Fatal: Unknown flag: %s\n", argv[i]);
            exit(11);
        }
    }
    if (!args.contains("type")) {
        fprintf(stderr, "Fatal: Output type is not specified");
        exit(12);
    }
    if (!args.contains("output")) {
        std::string suffix;
        if (args["type"] == "mermaid") {
            suffix = ".mermaid";
        } else if (args["type"] == "text-asm") {
            suffix = ".text-asm";
        } else {
            suffix = ".output";
        }
        args["output"] = args["input"] + suffix;
    }
    return args;
}

struct OutputFile {
    FILE* file;
    explicit OutputFile(std::string const& filename) {
        if (filename == "<null>") {
            file = nullptr;
        } else if (filename == "<stdout>") {
            file = stdout;
        } else {
            FILE* o = fopen(filename.c_str(), "w");
            if (o == nullptr) {
                fprintf(stderr, "Failed to open output file: %s\n", filename.c_str());
                exit(21);
            }
            file = o;
        }
    }

    ~OutputFile() {
        if (file != nullptr && file != stdout) {
            fclose(file);
        }
    }

    void puts(const char* str) const {
        if (file != nullptr)
            fputs(str, file);
    }

    void write(Porkchop::Assembler* assembler) const {
        if (file != nullptr)
            assembler->write(file);
    }
};

int main(int argc, const char* argv[]) try {
    forceUTF8();
    auto args = parseArgs(argc, argv);
    Porkchop::SourceCode c(readAll(args["input"]));
    try {
        c.tokenize();
        if (c.tokens.empty()) {
            fprintf(stderr, "Compilation Error: Empty input with nothing to compile");
            return -2;
        }
        c.parse();

        OutputFile of(args["output"]);
        auto const& output_type = args["type"];
        if (output_type == "mermaid") {
            auto descriptor = c.descriptor();
            of.puts(descriptor.c_str());
        } else if (output_type == "text-asm") {
            auto assembler = std::make_unique<Porkchop::TextAssembler>();
            c.compile(assembler.get());
            of.write(assembler.get());
        }
        fprintf(stdout, "Compilation is done successfully\n");
        return EXIT_SUCCESS;
    } catch (Porkchop::SegmentException& e) {
        fprintf(stderr, "%s\n", e.message(c).c_str());
        return -1;
    }
} catch (std::bad_alloc& e) {
    fprintf(stderr, "Compilation Failed: Compiler run out of memory: %s\n", e.what());
    return -2;
} catch (std::exception& e) {
    fprintf(stderr, "Compiler Internal Error: Unclassified std::exception occurred: %s\n", e.what());
    return -1000;
} catch (...) {
    fprintf(stderr, "Compiler Internal Error: Unknown naked exception occurred\n");
    return -1001;
}

