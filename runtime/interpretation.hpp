#pragma once

#include "frame.hpp"

#include "../parser.hpp"
#include "../function.hpp"

namespace Porkchop {

struct Interpretation : Assembler, Assembly {
    Instructions instructions;
    std::unordered_map<size_t, size_t> labels;

    explicit Interpretation(Continuum* continuum) {
        functions.back() = [continuum, this](VM* vm, std::vector<$union> const &args) -> $union {
            Source source;
            try {
                source.append(dynamic_cast<String*>(args[1].$object)->value);
            } catch (Error& e) {
                e.report(&source, true);
                throw Exception("failed to compile script in eval");
            }
            Compiler compiler(continuum, std::move(source));
            try {
                compiler.parse(Compiler::Mode::EVAL);
            } catch (Error& e) {
                e.report(&compiler.source, true);
                throw Exception("failed to compile script in eval");
            }
            compiler.compile(this);
            auto frame = std::make_unique<Frame>(vm, this, &std::get<Instructions>(functions.back()), std::vector{args[0]});
            auto type = continuum->functions.back()->prototype()->R;
            frame->init();
            auto ret = frame->loop();
            if (isValueBased(type))
                ret = vm->newObject<AnyScalar>(ret, dynamic_cast<ScalarType*>(type.get())->S);
            return ret;
        };
    }

    void const_(bool b) override {
        instructions.emplace_back(Opcode::CONST, b);
    }
    void const_(int64_t i) override {
        instructions.emplace_back(Opcode::CONST, $union{i}.$size);
    }
    void const_(double d) override {
        instructions.emplace_back(Opcode::CONST, $union{d}.$size);
    }
    void opcode(Opcode opcode) override {
        instructions.emplace_back(opcode, std::monostate{});
    }
    void indexed(Opcode opcode, size_t index) override {
        instructions.emplace_back(opcode, index);
    }
    void sconst(std::string const& s) override {
        size_t index = std::find(table.begin(), table.end(), s) - table.begin();
        if (index == table.size()) {
            table.push_back(s);
        }
        indexed(Opcode::SCONST, index);
    }
    void label(size_t index) override {
        labels[index] = instructions.size();
        opcode(Opcode::NOP);
    }
    void labeled(Opcode opcode, size_t index) override {
        instructions.emplace_back(opcode, index);
    }
    void typed(Opcode opcode, const TypeReference& type) override {
        instructions.emplace_back(opcode, type);
    }
    void cons(Opcode opcode, const TypeReference &type, size_t size) override {
        instructions.emplace_back(opcode, std::pair{type, size});
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
    }

    void write(FILE* file) override {}
};

}
