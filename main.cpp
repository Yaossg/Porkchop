#include <cstdio>
#include <cstring>

#include "parser.hpp"
#include "function.hpp"

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
        } else if (!strcmp("-b", argv[i]) || !strcmp("--bytecode", argv[i])) {
            args["type"] = "bytecode";
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
        } else if (args["type"] == "bytecode") {
            suffix = ".bytecode";
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
};

std::string readAll(std::string const& filename) {
    FILE* input_file = fopen(filename.c_str(), "r");
    if (input_file == nullptr) {
        fprintf(stderr, "Failed to open input file: %s\n", filename.c_str());
        exit(20);
    }
    std::string content;
    do {
        char line[1024];
        memset(line, 0, sizeof line);
        fgets(line, sizeof line, input_file);
        content += line;
    } while (!feof(input_file));
    fclose(input_file);
    return content;
}

int main(int argc, const char* argv[]) try {
#ifdef _WIN32
    system("chcp 65001");
    puts("Porkchop: UTF-8 is adopted via 'chcp 65001' in Windows");
#endif
    auto args = parseArgs(argc, argv);
    Porkchop::SourceCode c(readAll(args["input"]));
    try {
        c.split();
        c.tokenize();
        if (c.tokens.empty()) {
            fprintf(stderr, "Compilation Error: Empty input with nothing to compile");
            return -2;
        }
        c.parse();

        OutputFile of(args["output"]);
        auto const& output_type = args["type"];
        if (output_type == "mermaid") {
            of.puts(c.tree->walkDescriptor(c).c_str());
        } else if (output_type == "bytecode") {
            c.compile();
            for (auto&& line : c.bytecode) {
                of.puts(line.c_str());
                of.puts("\n");
            }
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

