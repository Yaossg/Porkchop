#include <cstdio>
#include <cstring>

#include "parser.hpp"

std::unordered_map<std::string, std::string> parseArgs(int argc, const char* argv[]) {
    std::unordered_map<std::string, std::string> args;
    if (argc < 2) {
        fprintf(stderr, "Too few arguments, input file expected\n");
        exit(10);
    }
    args["this"] = argv[0];
    args["input"] = argv[1];
    args["type"] = "default";
    for (int i = 2; i < argc; ++i) {
        if (!strcmp("-o", argv[i])) {
            ++i;
            args["output"] = argv[i];
        } else if (!strcmp("-m", argv[i]) || !strcmp("--mermaid", argv[i])) {
            args["type"] = "mermaid";
        } else {
            fprintf(stderr, "Unknown flag: %s\n", argv[i]);
            exit(11);
        }
    }
    if (!args.contains("output")) {
        args["output"] = args["input"] + ".mermaid";
    }
    return args;
}

int main(int argc, const char* argv[]) try {
#ifdef _WIN32
    system("chcp 65001");
    puts("Porkchop: UTF-8 is adopted via 'chcp 65001' in Windows");
#endif
    auto args = parseArgs(argc, argv);
    auto const& input_filename = args["input"];
    FILE* input_file = fopen(input_filename.c_str(), "r");
    if (input_file == nullptr) {
        fprintf(stderr, "Failed to open input file: %s\n", input_filename.c_str());
        return 20;
    }
    std::string script;
    do {
        char line[1024];
        memset(line, 0, sizeof line);
        fgets(line, sizeof line, input_file);
        script += line;
    } while (!feof(input_file));
    fclose(input_file);
    Porkchop::SourceCode c(script);
    try {
        c.split();
        c.tokenize();
        if (c.tokens.empty()) {
            fprintf(stderr, "Compilation Error: nothing to input");
            return -2;
        }
        c.parse();
    } catch (Porkchop::SegmentException& e) {
        fprintf(stderr, "%s\n", e.message(c).c_str());
        return -1;
    }
    auto const& output_filename = args["output"];
    FILE* output_file = output_filename == "<stdout>" ? stdout : fopen(output_filename.c_str(), "w");
    if (output_file == nullptr) {
        fprintf(stderr, "Failed to open output file: %s\n", output_filename.c_str());
        return 21;
    }
    auto const& output_type = args["type"];
    if (output_type == "mermaid") {
        fputs(c.tree->walkDescriptor(c).c_str(), output_file);
    }
    if (output_file != stdout) {
        fclose(output_file);
    }
    fprintf(stdout, "Compilation is done successfully: %s\n", output_filename.c_str());
    return 0;
}  catch (std::bad_alloc& e) {
    fprintf(stderr, "Program run out of memory: %s\n", e.what());
    return -100;
} catch (std::exception& e) {
    fprintf(stderr, "Unclassified std::exception occurred: %s\n", e.what());
    return -998;
} catch (...) {
    fprintf(stderr, "Unknown exception occurred\n");
    return -999;
}

