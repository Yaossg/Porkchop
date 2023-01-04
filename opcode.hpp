#pragma once

#include <unordered_map>
#include <string_view>

namespace Porkchop {

enum class Opcode : uint8_t {
    NOP,
    DUP,
    POP,
    JMP,
    JMP0,
    RETURN,
    STRING,
    FUNC,
    LOCAL,
    BIND,
    CONST,
    SCONST,
    FCONST,
    LOAD,
    STORE,
    TLOAD,
    LLOAD,
    LSTORE,
    DLOAD,
    DSTORE,
    CALL,
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
    UCMP,
    ICMP,
    FCMP,
    SCMP,
    OCMP,
    LT,
    LE,
    GT,
    GE,
    EQ,
    NE,
    ITER,
    MOVE,
    GET,
    I2S,
    F2S,
    B2S,
    Z2S,
    C2S,
    O2S,
    ADD,
    REMOVE,
    IN,
    SIZEOF,
    FHASH,
    OHASH,
    YIELD,
    SJOIN,
};

constexpr std::string_view OPCODE_NAME[] = {
    "nop",
    "dup",
    "pop",
    "jmp",
    "jmp0",
    "return",
    "string",
    "func",
    "local",
    "bind",
    "const",
    "sconst",
    "fconst",
    "load",
    "store",
    "tload",
    "lload",
    "lstore",
    "dload",
    "dstore",
    "call",
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
    "ucmp",
    "icmp",
    "fcmp",
    "scmp",
    "ocmp",
    "lt",
    "le",
    "gt",
    "ge",
    "eq",
    "ne",
    "iter",
    "move",
    "get",
    "i2s",
    "f2s",
    "b2s",
    "z2s",
    "c2s",
    "o2s",
    "add",
    "remove",
    "in",
    "sizeof",
    "fhash",
    "ohash",
    "yield",
    "sjoin",
};

const std::unordered_map<std::string_view, Opcode> OPCODES {
    {"nop", Opcode::NOP},
    {"dup", Opcode::DUP},
    {"pop", Opcode::POP},
    {"jmp", Opcode::JMP},
    {"jmp0", Opcode::JMP0},
    {"return", Opcode::RETURN},
    {"string", Opcode::STRING},
    {"func", Opcode::FUNC},
    {"local", Opcode::LOCAL},
    {"bind", Opcode::BIND},
    {"const", Opcode::CONST},
    {"sconst", Opcode::SCONST},
    {"fconst", Opcode::FCONST},
    {"load", Opcode::LOAD},
    {"store", Opcode::STORE},
    {"tload", Opcode::TLOAD},
    {"lload", Opcode::LLOAD},
    {"lstore", Opcode::LSTORE},
    {"dload", Opcode::DLOAD},
    {"dstore", Opcode::DSTORE},
    {"call", Opcode::CALL},
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
    {"ucmp", Opcode::UCMP},
    {"icmp", Opcode::ICMP},
    {"fcmp", Opcode::FCMP},
    {"scmp", Opcode::SCMP},
    {"ocmp", Opcode::OCMP},
    {"lt", Opcode::LT},
    {"le", Opcode::LE},
    {"gt", Opcode::GT},
    {"ge", Opcode::GE},
    {"eq", Opcode::EQ},
    {"ne", Opcode::NE},
    {"iter", Opcode::ITER},
    {"move", Opcode::MOVE},
    {"get", Opcode::GET},
    {"i2s", Opcode::I2S},
    {"f2s", Opcode::F2S},
    {"b2s", Opcode::B2S},
    {"z2s", Opcode::Z2S},
    {"c2s", Opcode::C2S},
    {"o2s", Opcode::O2S},
    {"add", Opcode::ADD},
    {"remove", Opcode::REMOVE},
    {"in", Opcode::IN},
    {"sizeof", Opcode::SIZEOF},
    {"fhash", Opcode::FHASH},
    {"ohash", Opcode::OHASH},
    {"yield", Opcode::YIELD},
    {"sjoin", Opcode::SJOIN},
};

}