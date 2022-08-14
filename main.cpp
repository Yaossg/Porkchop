#include <cstdio>
#include <cstring>

#include "parser.hpp"

int main(int argc, const char* argv[]) try {
    if (argc < 2) {
        fprintf(stderr, "Too few arguments, input file expected\n");
        return 10;
    }
    const char* input_filename = argv[1];
    FILE* input_file = fopen(input_filename, "r");
    if (input_file == nullptr) {
        fprintf(stderr, "Failed to open input file: %s\n", input_filename);
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
    std::string output_filename;
    if (argc > 2) {
        if (strcmp(argv[2], "-o") != 0) {
            fprintf(stderr, "Unknown flag, -o expected\n");
            return 11;
        }
        if (argc < 4) {
            fprintf(stderr, "Too few arguments, output file expected\n");
            return 12;
        }
        if (argc > 4) {
            fprintf(stderr, "Too many arguments\n");
            return 13;
        }
        output_filename = argv[3];
    } else {
        output_filename = input_filename;
        output_filename += ".mermaid";
    }
    FILE* output_file = output_filename == "<stdout>" ? stdout : fopen(output_filename.c_str(), "w");
    if (output_file == nullptr) {
        fprintf(stderr, "Failed to open output file: %s\n", input_filename);
        return 21;
    }
    fputs(c.tree->walkDescriptor(c).c_str(), output_file);
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

