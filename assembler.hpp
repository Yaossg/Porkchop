#pragma once

#include "opcode.hpp"

namespace Porkchop {

struct Assembler {
    virtual ~Assembler() = default;

    virtual void const_(bool b) = 0;
    virtual void const_(int64_t i) = 0;
    virtual void const_(double d) = 0;
    virtual void sconst(std::string const& s) = 0;
    virtual void opcode(Opcode opcode) = 0;
    virtual void indexed(Opcode opcode, size_t index) = 0;
    virtual void label(size_t index) = 0;
    virtual void labeled(Opcode opcode, size_t index) = 0;
    virtual void typed(Opcode opcode, TypeReference const& type) = 0;
    virtual void cons(Opcode opcode, TypeReference const& type, size_t size) = 0;

    void const0() { const_(false); }
    void const1() { const_(true); }

    virtual void func(TypeReference const& type) {
        typed(Opcode::FUNC, type);
    }
    virtual void local(TypeReference const& type) {
        typed(Opcode::LOCAL, type);
    }

    virtual void beginFunction() = 0;
    virtual void endFunction() = 0;

    void newFunction(std::vector<TypeReference> const& localTypes, Expr* clause) {
        beginFunction();
        for (auto&& type : localTypes) {
            local(type);
        }
        clause->walkBytecode(this);
        opcode(Opcode::RETURN);
        endFunction();
    }

    virtual void write(FILE* file) = 0;
};

struct TextAssembler : Assembler {
    std::vector<std::string> table;
    std::vector<std::string> assemblies;
    void const_(bool b) override {
        assemblies.emplace_back(b ? "const 1" : "const 0");
    }
    void const_(int64_t i) override {
        char buf[24];
        sprintf(buf, "const %llX", i);
        assemblies.emplace_back(buf);
    }
    void const_(double d) override {
        char buf[24];
        sprintf(buf, "const %llX", d);
        assemblies.emplace_back(buf);
    }
    void opcode(Opcode opcode) override {
        assemblies.emplace_back(std::string{OPCODE_NAME[(size_t) opcode]});
    }
    void indexed(Opcode opcode, size_t index) override {
        char buf[24];
        sprintf(buf, "%s %zu", OPCODE_NAME[(size_t)opcode].data(), index);
        assemblies.emplace_back(buf);
    }
    void sconst(std::string const& s) override {
        size_t index = std::find(table.begin(), table.end(), s) - table.begin();
        if (index == table.size()) {
            table.push_back(s);
        }
        char buf[24];
        sprintf(buf, "sconst %llX", index);
        assemblies.emplace_back(buf);
    }
    void label(size_t index) override {
        char buf[24];
        sprintf(buf, "L%zu: nop", index);
        assemblies.emplace_back(buf);
    }
    void labeled(Opcode opcode, size_t index) override {
        char buf[24];
        sprintf(buf, "%s L%zu", OPCODE_NAME[(size_t)opcode].data(), index);
        assemblies.emplace_back(buf);
    }
    void typed(Opcode opcode, const TypeReference& type) override {
        assemblies.emplace_back(std::string(OPCODE_NAME[(size_t)opcode].data()) + " " + type->serialize());
    }
    void cons(Opcode opcode, const TypeReference &type, size_t size) override {
        char buf[24];
        sprintf(buf, "%s %s%zu", OPCODE_NAME[(size_t)opcode].data(), type->serialize().c_str(), size);
        assemblies.emplace_back(buf);
    }

    void beginFunction() override {
        assemblies.emplace_back("(");
    }

    void endFunction() override {
        assemblies.emplace_back(")");
    }

    void write(FILE* file) override {
        for (auto&& s : table) {
            std::string buf = "string " + std::to_string(s.length()) + " ";
            for (char ch : s) {
                char digits[3];
                sprintf(digits, "%2X", ch);
                buf += digits;
            }
            fputs(buf.c_str(), file);
            fputs("\n", file);
        }
        for (auto&& line : assemblies) {
            fputs(line.c_str(), file);
            fputs("\n", file);
        }
    }
};

struct BinaryAssembler : Assembler {
    // TODO
};

}