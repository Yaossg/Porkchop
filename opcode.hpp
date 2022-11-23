#pragma once

#include "type.hpp"

namespace Porkchop {

enum class Opcode {
    NOP,
    DUP,
    POP,
    JMP,
    JMP0,
    CONST,
    STRING,
    FUNC,
    LOAD,
    STORE,
    TLOAD,
    LLOAD,
    DLOAD,
    TSTORE,
    LSTORE,
    DSTORE,
    CALL,
    BIND,
    LOCAL,
    AS,
    IS,
    ANY,
    I2B,
    I2C,
    I2F,
    F2I,
    TUPLE,
    LIST,
    SET,
    DICT,
    RETURN,
    INEG,
    FNEG,
    NOT,
    INV,
    OR,
    XOR,
    AND,
    SHL,
    SHR,
    USHR,
    SEQ,
    IEQ,
    FEQ,
    SNE,
    INE,
    FNE,
    SLT,
    ILT,
    FLT,
    SGT,
    IGT,
    FGT,
    SLE,
    ILE,
    FLE,
    SGE,
    IGE,
    FGE,
    SADD,
    IADD,
    FADD,
    ISUB,
    FSUB,
    IMUL,
    FMUL,
    IDIV,
    FDIV,
    IREM,
    FREM,
    INC,
    DEC,
};

constexpr std::string_view OPCODE_NAME[] = {
    "nop",
    "dup",
    "pop",
    "jmp",
    "jmp0",
    "const",
    "string",
    "func",
    "load",
    "store",
    "tload",
    "lload",
    "dload",
    "tstore",
    "lstore",
    "dstore",
    "call",
    "bind",
    "local",
    "as",
    "is",
    "any",
    "i2b",
    "i2c",
    "i2f",
    "f2i",
    "tuple",
    "list",
    "set",
    "dict",
    "return",
    "ineg",
    "fneg",
    "not",
    "inv",
    "or",
    "xor",
    "and",
    "shl",
    "shr",
    "ushr",
    "seq",
    "ieq",
    "feq",
    "sne",
    "ine",
    "fne",
    "slt",
    "ilt",
    "flt",
    "sgt",
    "igt",
    "fgt",
    "sle",
    "ile",
    "fle",
    "sge",
    "ige",
    "fge",
    "sadd",
    "iadd",
    "fadd",
    "isub",
    "fsub",
    "imul",
    "fmul",
    "idiv",
    "fdiv",
    "irem",
    "frem",
    "inc",
    "dec",
};

const std::unordered_map<std::string_view, Opcode> OPCODES {
    {"nop", Opcode::NOP},
    {"dup", Opcode::DUP},
    {"pop", Opcode::POP},
    {"jmp", Opcode::JMP},
    {"jmp0", Opcode::JMP0},
    {"const", Opcode::CONST},
    {"string", Opcode::STRING},
    {"func", Opcode::FUNC},
    {"load", Opcode::LOAD},
    {"store", Opcode::STORE},
    {"tload", Opcode::TLOAD},
    {"lload", Opcode::LLOAD},
    {"dload", Opcode::DLOAD},
    {"tstore", Opcode::TSTORE},
    {"lstore", Opcode::LSTORE},
    {"dstore", Opcode::DSTORE},
    {"call", Opcode::CALL},
    {"bind", Opcode::BIND},
    {"local", Opcode::LOCAL},
    {"as", Opcode::AS},
    {"is", Opcode::IS},
    {"any", Opcode::ANY},
    {"i2b", Opcode::I2B},
    {"i2c", Opcode::I2C},
    {"i2f", Opcode::I2F},
    {"f2i", Opcode::F2I},
    {"tuple", Opcode::TUPLE},
    {"list", Opcode::LIST},
    {"set", Opcode::SET},
    {"dict", Opcode::DICT},
    {"return", Opcode::RETURN},
    {"ineg", Opcode::INEG},
    {"fneg", Opcode::FNEG},
    {"not", Opcode::NOT},
    {"inv", Opcode::INV},
    {"or", Opcode::OR},
    {"xor", Opcode::XOR},
    {"and", Opcode::AND},
    {"shl", Opcode::SHL},
    {"shr", Opcode::SHR},
    {"ushr", Opcode::USHR},
    {"seq", Opcode::SEQ},
    {"ieq", Opcode::IEQ},
    {"feq", Opcode::FEQ},
    {"sne", Opcode::SNE},
    {"ine", Opcode::INE},
    {"fne", Opcode::FNE},
    {"slt", Opcode::SLT},
    {"ilt", Opcode::ILT},
    {"flt", Opcode::FLT},
    {"sgt", Opcode::SGT},
    {"igt", Opcode::IGT},
    {"fgt", Opcode::FGT},
    {"sle", Opcode::SLE},
    {"ile", Opcode::ILE},
    {"fle", Opcode::FLE},
    {"sge", Opcode::SGE},
    {"ige", Opcode::IGE},
    {"fge", Opcode::FGE},
    {"sadd", Opcode::SADD},
    {"iadd", Opcode::IADD},
    {"fadd", Opcode::FADD},
    {"isub", Opcode::ISUB},
    {"fsub", Opcode::FSUB},
    {"imul", Opcode::IMUL},
    {"fmul", Opcode::FMUL},
    {"idiv", Opcode::IDIV},
    {"fdiv", Opcode::FDIV},
    {"irem", Opcode::IREM},
    {"frem", Opcode::FREM},
    {"inc", Opcode::INC},
    {"dec", Opcode::DEC},
};

struct Assembler {
    virtual ~Assembler() = default;

    virtual void const_(bool b) = 0;
    virtual void const_(int64_t i) = 0;
    virtual void const_(double d) = 0;
    virtual void opcode(Opcode opcode) = 0;
    virtual void indexed(Opcode opcode, size_t index) = 0;
    virtual void string(std::string const& s) = 0;
    virtual void label(size_t index) = 0;
    virtual void labeled(Opcode opcode, size_t index) = 0;
    virtual void typed(Opcode opcode, TypeReference const& type) = 0;

    void const0() { const_(false); }
    void const1() { const_(true); }

    virtual void beginFunction() = 0;
    virtual void endFunction() = 0;


    virtual void write(FILE* file) = 0;
};

struct TextAssembler : Assembler {
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
    void string(std::string const& s) override {
        std::string buf = "string " + std::to_string(s.length()) + " ";
        for (char ch : s) {
            char digits[3];
            sprintf(digits, "%2X", ch);
            buf += digits;
        }
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
        assemblies.emplace_back(std::string(OPCODE_NAME[(size_t)opcode].data()) + " " + type->toString());
    }

    void beginFunction() override {
        assemblies.emplace_back("(");
    }

    void endFunction() override {
        assemblies.emplace_back(")");
    }

    void write(FILE* file) override {
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