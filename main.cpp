#include "common.hpp"
#include "function.hpp"
#include "text-assembler.hpp"
#include "bin-assembler.hpp"

std::unordered_map<std::string, std::string> parseArgs(int argc, const char* argv[]) {
    std::unordered_map<std::string, std::string> args;
    if (argc < 2) {
        fprintf(stderr, "Fatal: Too few arguments, input file expected\n");
        fprintf(stderr, "Usage: Porkchop <input> [options...]\n");
        std::exit(10);
    }
    args["input"] = argv[1];
    for (int i = 2; i < argc; ++i) {
        if (!strcmp("-o", argv[i])) {
            args["output"] = argv[++i];
        } else if (!strcmp("-m", argv[i]) || !strcmp("--mermaid", argv[i])) {
            args["type"] = "mermaid";
        } else if (!strcmp("-t", argv[i]) || !strcmp("--text-asm", argv[i])) {
            args["type"] = "text-asm";
        } else if (!strcmp("-b", argv[i]) || !strcmp("--bin-asm", argv[i])) {
            args["type"] = "bin-asm";
        } else {
            fprintf(stderr, "Fatal: Unknown flag: %s\n", argv[i]);
            std::exit(11);
        }
    }
    if (!args.contains("type")) {
        fprintf(stderr, "Fatal: Output type is not specified");
        std::exit(12);
    }
    if (!args.contains("output")) {
        auto const& input = args["input"];
        args["output"] = input.substr(0, input.find_last_of('.')) + '.' + args["type"];
    }
    return args;
}

struct OutputFile {
    FILE* file;

    explicit OutputFile(std::string const& filename, bool binary) {
        if (filename == "<null>") {
            file = nullptr;
        } else if (filename == "<stdout>") {
            file = stdout;
        } else {
            FILE* o = fopen(filename.c_str(), binary ? "wb" : "w");
            if (o == nullptr) {
                fprintf(stderr, "Failed to open output file: %s\n", filename.c_str());
                std::exit(21);
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
    Porkchop::forceUTF8();
    auto args = parseArgs(argc, argv);
    auto original = Porkchop::readText(args["input"].c_str());
    Porkchop::Compiler compiler(std::move(original));
    Porkchop::parse(compiler);
    auto const& output_type = args["type"];
    OutputFile output_file(args["output"], output_type == "bin-asm");
    if (output_type == "mermaid") {
        auto descriptor = compiler.descriptor();
        output_file.puts(descriptor.c_str());
    } else if (output_type.ends_with("asm")) {
        std::unique_ptr<Porkchop::Assembler> assembler;
        if (output_type == "text-asm") {
            assembler = std::make_unique<Porkchop::TextAssembler>();
        } else {
            assembler = std::make_unique<Porkchop::BinAssembler>();
        }
        compiler.compile(assembler.get());
        output_file.write(assembler.get());
    }
    puts("Compilation is done successfully");
} catch (std::bad_alloc& e) {
    fprintf(stderr, "Out of memory\n");
    std::exit(-10);
} catch (std::exception& e) {
    fprintf(stderr, "Internal Compiler Error: %s\n", e.what());
    std::exit(-100);
}
