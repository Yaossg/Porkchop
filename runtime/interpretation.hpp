#pragma once

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
    void sconst(std::string const& s) override {
        size_t index = std::find(table.begin(), table.end(), s) - table.begin();
        if (index == table.size()) {
            table.push_back(s);
        }
        instructions.push_back(std::pair<Opcode, size_t>{Opcode::SCONST, index});
    }
    void label(size_t index) override {
        labels[index] = instructions.size();
        instructions.push_back(std::pair<Opcode, std::monostate>{Opcode::NOP, {}});
    }
    void labeled(Opcode opcode, size_t index) override {
        instructions.push_back(std::pair<Opcode, size_t>{opcode, index});
    }
    void typed(Opcode opcode, const TypeReference& type) override {
        instructions.push_back(std::pair<Opcode, TypeReference>{opcode, type});
    }
    void cons(Opcode opcode, const TypeReference &type, size_t size) override {
        instructions.push_back(std::pair<Opcode, std::pair<TypeReference, size_t>>{opcode, {type, size}});
    }

    void func(const TypeReference &type) override {
        prototypes.push_back(std::dynamic_pointer_cast<FuncType>(type));
    }

    void beginFunction() override {
        instructions.clear();
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
