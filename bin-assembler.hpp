#pragma once

#include "assembler.hpp"

namespace Porkchop {

struct ByteBuf {
    std::vector<uint8_t> buffer;

    void clear() {
        buffer.clear();
    }

    [[nodiscard]] size_t size() const {
        return buffer.size();
    }

    [[nodiscard]] const void* data() const {
        return buffer.data();
    }

    ByteBuf& append(Opcode opcode) {
        buffer.push_back((uint8_t) opcode);
        return *this;
    }

    ByteBuf& append(std::string_view view) {
        size_t size = buffer.size();
        buffer.resize(size + view.length());
        memcpy(buffer.data() + size, view.data(), view.length());
        return *this;
    }

    ByteBuf& append(size_t var) {
        buffer.reserve(buffer.size() + 10);
        while (var > 0x7F) {
            buffer.push_back(0x80 | (var & 0x7F));
            var >>= 7;
        }
        buffer.push_back(var & 0x7F);
        return *this;
    }

    ByteBuf& append(ByteBuf const& buf) {
        size_t size = buffer.size();
        buffer.resize(size + buf.size());
        memcpy(buffer.data() + size, buf.data(), buf.size());
        return *this;
    }

    void write(FILE* file) const {
        fwrite(data(), 1, size(), file);
    }
};


struct BinAssembler : Assembler {
    std::vector<std::string> table;
    std::vector<TypeReference> prototypes;
    std::unordered_map<size_t, size_t> labels;
    size_t instructions = 0;
    std::vector<ByteBuf> functions;
    ByteBuf buffer;

    void const_(size_t size) {
        buffer.append(Opcode::CONST).append(size);
        ++instructions;
    }

    void const_(bool b) override {
        const_((size_t)b);
    }
    void const_(int64_t i) override {
        const_((size_t)i);
    }
    void const_(double d) override {
        const_(std::bit_cast<size_t>(d));
    }
    void opcode(Opcode opcode) override {
        buffer.append(opcode);
        ++instructions;
    }
    void indexed(Opcode opcode, size_t index) override {
        buffer.append(opcode).append(index);
        ++instructions;
    }
    void sconst(std::string const& s) override {
        size_t index = std::find(table.begin(), table.end(), s) - table.begin();
        if (index == table.size()) {
            table.push_back(s);
        }
        indexed(Opcode::SCONST, index);
    }
    void label(size_t index) override {
        labels[index] = instructions;
        opcode(Opcode::NOP);
    }
    void labeled(Opcode opcode, size_t index) override {
        buffer.append(opcode).append(index);
        ++instructions;
    }
    void typed(Opcode opcode, const TypeReference& type) override {
        buffer.append(opcode).append(type->serialize());
        ++instructions;
    }
    void cons(Opcode opcode, const TypeReference &type, size_t size) override {
        buffer.append(opcode).append(type->serialize()).append(size);
        ++instructions;
    }

    void func(const TypeReference &type) override {
        prototypes.push_back(type);
    }

    void beginFunction() override {
        instructions = 0;
        buffer.clear();
    }

    void endFunction() override {
        functions.push_back(std::move(buffer));
    }

    void write(FILE* file) override {
        ByteBuf buf;
        buf.append(table.size());
        for (auto&& string : table) {
            buf.append(string.length()).append(string);
        }
        buf.append(prototypes.size());
        for (auto&& prototype : prototypes) {
            buf.append(prototype->serialize());
        }
        buf.append(labels.size());
        for (auto&& [key, value] : labels) {
            buf.append(key).append(value);
        }
        for (auto&& function : functions) {
            buf.append(function.buffer.size()).append(function);
        }
        buf.write(file);
    }
};


}