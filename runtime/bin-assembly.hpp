#pragma once

#include "assembly.hpp"

namespace Porkchop {

struct ByteStream {
    const uint8_t* cur;

    uint8_t next() {
        return *cur++;
    }

    Opcode readCode() {
        return (Opcode) next();
    }

    size_t readVarInt() {
        size_t result = 0;
        uint8_t byte;
        do {
            result <<= 7;
            byte = next();
            result |= byte & 0x7F;
        } while (byte & 0x80);
        return result;
    }

    TypeReference readType() {
        return deserialize(reinterpret_cast<const char*&>(cur));
    }

    std::string readString() {
        size_t size = readVarInt();
        std::string string{reinterpret_cast<const char*>(cur), size};
        cur += size;
        return string;
    }
};

struct BinAssembly : Assembly {
    std::vector<uint8_t> fileBuffer;
    ByteStream stream{};
    std::unordered_map<size_t, size_t> labels;

    explicit BinAssembly(std::vector<uint8_t> fileBuffer): fileBuffer(std::move(fileBuffer)) {
        stream.cur = this->fileBuffer.data();
        parse();
    }

    void parseFunction() {
        auto functionSize = stream.readVarInt();
        ByteStream stream0{stream.cur};
        stream.cur += functionSize;
        Instructions instructions;
        while (stream0.cur != stream.cur) {
            switch (auto opcode = stream0.readCode()) {
                case Opcode::JMP:
                case Opcode::JMP0:
                    // label
                    instructions.emplace_back(opcode, labels[stream0.readVarInt()]);
                    break;
                case Opcode::SCONST:
                case Opcode::FCONST:
                case Opcode::BIND:
                case Opcode::LOAD:
                case Opcode::STORE:
                case Opcode::TLOAD:
                case Opcode::INC:
                case Opcode::DEC:
                case Opcode::SJOIN:
                case Opcode::CONST:
                    // index or size or const
                    instructions.emplace_back(opcode, stream0.readVarInt());
                    break;
                case Opcode::AS:
                case Opcode::IS:
                case Opcode::ANY:
                case Opcode::TUPLE:
                case Opcode::LOCAL: {
                    // type
                    instructions.emplace_back(opcode, stream0.readType());
                    break;
                }
                case Opcode::LIST:
                case Opcode::SET:
                case Opcode::DICT: {
                    // type size
                    auto type = stream0.readType();
                    auto size = stream0.readVarInt();
                    instructions.emplace_back(opcode, std::pair{std::move(type), size});
                    break;
                }
                default: {
                    instructions.emplace_back(opcode, std::monostate{});
                    break;
                }
            }
        }
        functions.emplace_back(std::move(instructions));
    }

    void parse() {
        table.resize(stream.readVarInt());
        for (auto&& s : table) {
            s = stream.readString();
        }
        prototypes.resize(stream.readVarInt());
        for (auto&& t : prototypes) {
            t = dynamic_pointer_cast<FuncType>(stream.readType());
        }
        auto labelSize = stream.readVarInt();
        labels.reserve(labelSize);
        for (size_t i = 0; i < labelSize; ++i) {
            auto key = stream.readVarInt();
            auto value = stream.readVarInt();
            labels[key] = value;
        }
        functions.reserve(prototypes.size());
        for (size_t i = functions.size(); i < prototypes.size(); ++i) {
            parseFunction();
        }
    }
};

}