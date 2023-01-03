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

    virtual void func(TypeReference const& type) = 0;

    virtual void beginFunction() = 0;
    virtual void endFunction() = 0;

    void newFunction(std::vector<TypeReference> const& localTypes, ExprHandle const& clause, bool async) {
        beginFunction();
        for (auto&& type : localTypes) {
            typed(Opcode::LOCAL, type);
        }
        if (async) opcode(Opcode::ASYNC);
        clause->walkBytecode(this);
        opcode(Opcode::RETURN);
        endFunction();
    }

    virtual void write(FILE* file) = 0;
};

}