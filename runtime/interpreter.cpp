#include "runtime.hpp"

#include "../parser.hpp"
#include "../function.hpp"

namespace Porkchop {

struct Interpretation : Assembler, Assembly {
    bool initialized = false;
    Instructions instructions;
    std::unordered_map<size_t, size_t> labels;

    void const_(bool b) override {
        instructions.push_back(std::pair<Opcode, size_t>{Opcode::CONST, b});
    }
    void const_(int64_t i) override {
        instructions.push_back(std::pair<Opcode, size_t>{Opcode::CONST, i});
    }
    void const_(double d) override {
        instructions.push_back(std::pair<Opcode, size_t>{Opcode::CONST, as_size(d)});
    }
    void opcode(Opcode opcode) override {
        instructions.push_back(std::pair<Opcode, std::monostate>{opcode, {}});
    }
    void indexed(Opcode opcode, size_t index) override {
        instructions.push_back(std::pair<Opcode, size_t>{opcode, index});
    }
    void string(std::string const& s) override {
        instructions.push_back(std::pair<Opcode, std::string>{Opcode::STRING, s});
    }
    void label(size_t index) override {
        labels[index] = instructions.size();
        instructions.push_back(std::pair<Opcode, std::monostate>{Opcode::NOP, {}});
    }
    void labeled(Opcode opcode, size_t index) override {
        instructions.push_back(std::pair<Opcode, size_t>{opcode, index});
    }
    void typed(Opcode opcode, const TypeReference& type) override {
        instructions.push_back(std::pair<Opcode, std::string>{opcode, type->toString()});
    }

    void beginFunction() override {

    }

    void processLabels() {
        for (auto& [opcode, args] : instructions) {
            if (opcode == Opcode::JMP || opcode == Opcode::JMP0) {
                auto& label = get<size_t>(args);
                label = labels[label];
            }
        }
    }

    void endFunction() override {
        processLabels();
        functions.emplace_back(std::move(instructions));
        if (!initialized) {
            externalFunctions();
            initialized = true;
        }
    }

    void write(FILE* file) override {}
};

}


int main(int argc, const char* argv[]) try {
    Porkchop::forceUTF8();
    if (argc < 2) {
        fprintf(stderr, "Fatal: Too few arguments, input file expected\n");
        fprintf(stderr, "Usage: PorkchopInterpreter <input> [args...]\n");
        return 10;
    }
    Porkchop::Compiler c(Porkchop::readAll(argv[1]));
    try {
        c.tokenize();
        if (c.tokens.empty()) {
            fprintf(stderr, "Compilation Error: Empty input with nothing to compile\n");
            return -2;
        }
        c.parse();
        Porkchop::Interpretation interpretation;
        c.compile(&interpretation);
        Porkchop::Externals::init(argc, argv);
        Porkchop::Runtime::Func main_{0, {}};
        auto ret = main_.call(&interpretation);
        fprintf(stdout, "\nProgram finished with exit code %zu\n", ret);
        return 0;

    } catch (Porkchop::SegmentException& e) {
        fprintf(stderr, "%s\n", e.message(c).c_str());
        return -1;
    }
} catch (std::bad_alloc& e) {
    fprintf(stderr, "Interpretation Failed: Interpreter run out of memory: %s\n", e.what());
    return -50;
} catch (Porkchop::Runtime::Exception& e) {
    fprintf(stderr, "Runtime exception occurred:\n");
    fprintf(stderr, "%s\n", e.what());
    return 1;
}  catch (std::runtime_error& e) {
    fprintf(stderr, "Runtime external exception occurred:\n");
    fprintf(stderr, "%s\n", e.what());
    return 2;
} catch (std::exception& e) {
    fprintf(stderr, "Interpreter Internal Error: Unclassified std::exception occurred: %s\n", e.what());
    return -1000;
} catch (...) {
    fprintf(stderr, "Interpreter Internal Error: Unknown naked exception occurred\n");
    return -1001;
}

