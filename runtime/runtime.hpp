#pragma once

#include <deque>
#include <bit>
#include <unordered_set>
#include <cmath>
#include <stdexcept>

#include "text-asm.hpp"

namespace Porkchop {

struct Runtime {

    struct Exception : std::runtime_error {
        std::string messages;
        explicit Exception(std::string const& message): std::runtime_error(message), messages(message) {}

        void append(std::string const& message) {
            messages += "\n    ";
            messages += message;
        }

        [[nodiscard]] const char * what() const noexcept override {
            return messages.c_str();
        }
    };

    struct Any {
        size_t value;
        std::string type;
    };

    struct Func {
        size_t func;
        std::vector<size_t> captures;

        size_t call(Assembly *assembly) const;
    };

    Assembly *assembly;
    std::deque<size_t> stack;
    std::vector<size_t> locals;

    Runtime(Assembly *assembly, std::vector<size_t> const &captures)
        : assembly(assembly), locals(captures.begin(), captures.end()) {}

    void local(size_t size) {
        locals.resize(size);
    }

    void dup() {
        stack.push_back(stack.back());
    }

    size_t pop() {
        auto back = stack.back();
        stack.pop_back();
        return back;
    }

    size_t return_() {
        return pop();
    }

    int64_t ipop() {
        return int64_t(pop());
    }

    double fpop() {
        return std::bit_cast<double>(pop());
    }

    std::string *spop() {
        return std::bit_cast<std::string *>(pop());
    }

    std::vector<size_t> npop(size_t n) {
        if (n == 1) return {pop()};
        auto b = std::prev(stack.end(), n), e = stack.end();
        std::vector<size_t> r{b, e};
        stack.erase(b, e);
        return r;
    }

    void const_(size_t value) {
        stack.push_back(value);
    }

    void push(double value) {
        stack.push_back(std::bit_cast<size_t>(value));
    }

    void push(void *ptr) {
        stack.push_back(std::bit_cast<size_t>(ptr));
    }

    void string(std::string const &s) {
        push(new std::string(s));
    }

    void func(size_t index) {
        push(new Func{index});
    }

    void load(size_t index) {
        stack.push_back(locals[index]);
    }

    void store(size_t index) {
        locals[index] = stack.back();
    }

    void tload(size_t index) {
        auto tuple = std::bit_cast<std::vector<size_t> *>(pop());
        stack.push_back(tuple->at(index));
    }

    void lload() {
        auto index = pop();
        auto list = std::bit_cast<std::vector<size_t> *>(pop());
        if (index >= list->size())
            throw Exception("index out of bound");
        stack.push_back(list->at(index));
    }

    void dload() {
        auto key = pop();
        auto dict = std::bit_cast<std::unordered_map<size_t, size_t> *>(pop());
        stack.push_back(dict->at(key));
    }

    void tstore(size_t index) {
        auto tuple = std::bit_cast<std::vector<size_t> *>(pop());
        auto value = stack.back();
        tuple->at(index) = value;
    }

    void lstore() {
        auto index = pop();
        auto list = std::bit_cast<std::vector<size_t> *>(pop());
        if (index >= list->size())
            throw Exception("index out of bound");
        auto value = stack.back();
        list->at(index) = value;
    }

    void dstore() {
        auto key = pop();
        auto dict = std::bit_cast<std::unordered_map<size_t, size_t> *>(pop());
        auto value = stack.back();
        dict->insert_or_assign(key, value);
    }

    void call() {
        stack.emplace_back(std::bit_cast<Func *>(pop())->call(assembly));
    }

    void bind(size_t size) {
        if (size > 0) {
            auto v = npop(size);
            auto f = new Func(*std::bit_cast<Func *>(pop()));
            for (auto e: v) {
                f->captures.push_back(e);
            }
            push(f);
        }
    }

    void as(std::string const &type) {
        auto any = std::bit_cast<Any *>(pop());
        if (any->type != type)
            throw Exception("cannot cast " + any->type + " to " + type);
        stack.emplace_back(any->value);
    }

    void is(std::string const &type) {
        stack.emplace_back(std::bit_cast<Any *>(pop())->type == type);
    }

    void any(std::string const &type) {
        push(new Any{pop(), type});
    }

    void i2b() {
        auto value = ipop();
        if (value < 0 || value > 0xFFLL)
            throw Exception("int is invalid to cast to byte");
        stack.push_back(value);
    }

    void i2c() {
        auto value = ipop();
        if (value < 0 || value > 0x10FFFFLL || 0xD800LL <= value && value <= 0xDFFFLL)
            throw Exception("int is invalid to cast to char");
        stack.push_back(value);
    }

    void i2f() {
        auto value = ipop();
        push((double) value);
    }

    void f2i() {
        auto value = int64_t(fpop());
        stack.push_back(value);
    }

    void tuple(size_t size) {
        push(new std::vector(npop(size)));
    }

    void list(size_t size) {
        push(new std::vector(npop(size)));
    }

    void set(size_t size) {
        auto v = npop(size);
        push(new std::unordered_set(v.begin(), v.end()));
    }

    void dict(size_t size) {
        auto v = npop(2 * size);
        auto m = new std::unordered_map<size_t, size_t>;
        for (size_t i = 0; i < size; ++i) {
            m->insert_or_assign(v[2 * i], v[2 * i + 1]);
        }
        push(m);
    }

    void ineg() {
        stack.push_back(-ipop());
    }

    void fneg() {
        push(-fpop());
    }

    void not_() {
        stack.push_back(!ipop());
    }

    void inv() {
        stack.push_back(~ipop());
    }

    void or_() {
        auto value2 = ipop();
        auto value1 = ipop();
        stack.push_back(value1 | value2);
    }

    void xor_() {
        auto value2 = ipop();
        auto value1 = ipop();
        stack.push_back(value1 ^ value2);
    }

    void and_() {
        auto value2 = ipop();
        auto value1 = ipop();
        stack.push_back(value1 & value2);
    }

    void shl() {
        auto value2 = ipop();
        auto value1 = ipop();
        stack.push_back(value1 << value2);
    }

    void shr() {
        auto value2 = ipop();
        auto value1 = ipop();
        stack.push_back(value1 >> value2);
    }

    void ushr() {
        auto value2 = ipop();
        auto value1 = pop();
        stack.push_back(value1 >> value2);
    }

    void seq() {
        auto value2 = spop();
        auto value1 = spop();
        stack.push_back(*value1 == *value2);
    }

    void sne() {
        auto value2 = spop();
        auto value1 = spop();
        stack.push_back(*value1 != *value2);
    }

    void slt() {
        auto value2 = spop();
        auto value1 = spop();
        stack.push_back(*value1 < *value2);
    }

    void sgt() {
        auto value2 = spop();
        auto value1 = spop();
        stack.push_back(*value1 > *value2);
    }

    void sle() {
        auto value2 = spop();
        auto value1 = spop();
        stack.push_back(*value1 <= *value2);
    }

    void sge() {
        auto value2 = spop();
        auto value1 = spop();
        stack.push_back(*value1 >= *value2);
    }

    void ieq() {
        auto value2 = ipop();
        auto value1 = ipop();
        stack.push_back(value1 == value2);
    }

    void ine() {
        auto value2 = ipop();
        auto value1 = ipop();
        stack.push_back(value1 != value2);
    }

    void ilt() {
        auto value2 = ipop();
        auto value1 = ipop();
        stack.push_back(value1 < value2);
    }

    void igt() {
        auto value2 = ipop();
        auto value1 = ipop();
        stack.push_back(value1 > value2);
    }

    void ile() {
        auto value2 = ipop();
        auto value1 = ipop();
        stack.push_back(value1 <= value2);
    }

    void ige() {
        auto value2 = ipop();
        auto value1 = ipop();
        stack.push_back(value1 >= value2);
    }

    void feq() {
        auto value2 = fpop();
        auto value1 = fpop();
        stack.push_back(value1 == value2);
    }

    void fne() {
        auto value2 = fpop();
        auto value1 = fpop();
        stack.push_back(value1 != value2);
    }

    void flt() {
        auto value2 = fpop();
        auto value1 = fpop();
        stack.push_back(value1 < value2);
    }

    void fgt() {
        auto value2 = fpop();
        auto value1 = fpop();
        stack.push_back(value1 > value2);
    }

    void fle() {
        auto value2 = fpop();
        auto value1 = fpop();
        stack.push_back(value1 <= value2);
    }

    void fge() {
        auto value2 = fpop();
        auto value1 = fpop();
        stack.push_back(value1 >= value2);
    }

    void sadd() {
        auto value2 = spop();
        auto value1 = spop();
        push(new std::string(*value1 + *value2));
    }

    void iadd() {
        auto value2 = ipop();
        auto value1 = ipop();
        stack.push_back(value1 + value2);
    }

    void isub() {
        auto value2 = ipop();
        auto value1 = ipop();
        stack.push_back(value1 - value2);
    }

    void imul() {
        auto value2 = ipop();
        auto value1 = ipop();
        stack.push_back(value1 * value2);
    }

    void idiv() {
        auto value2 = ipop();
        if (value2 == 0)
            throw Exception("divided by zero");
        auto value1 = ipop();
        stack.push_back(value1 / value2);
    }

    void irem() {
        auto value2 = ipop();
        if (value2 == 0)
            throw Exception("divided by zero");
        auto value1 = ipop();
        stack.push_back(value1 % value2);
    }

    void fadd() {
        auto value2 = fpop();
        auto value1 = fpop();
        push(value1 + value2);
    }

    void fsub() {
        auto value2 = fpop();
        auto value1 = fpop();
        push(value1 - value2);
    }

    void fmul() {
        auto value2 = fpop();
        auto value1 = fpop();
        push(value1 * value2);
    }

    void fdiv() {
        auto value2 = fpop();
        auto value1 = fpop();
        push(value1 / value2);
    }

    void frem() {
        auto value2 = fpop();
        auto value1 = fpop();
        push(fmod(value1, value2));
    }

    void inc(size_t index) {
        ++locals[index];
    }

    void dec(size_t index) {
        --locals[index];
    }
};

size_t Runtime::Func::call(Assembly *assembly) const try {
    auto& f = assembly->functions[func];
    if (std::holds_alternative<Instructions>(f)) {
        auto& instructions = std::get<Instructions>(f);
        Runtime runtime(assembly, captures);
        for (size_t i = 0; i < instructions.size(); ++i) {
            auto&& [opcode, args] = instructions[i];
            switch (opcode) {
                case Opcode::NOP:
                    break;
                case Opcode::DUP:
                    runtime.dup();
                    break;
                case Opcode::POP:
                    runtime.pop();
                    break;
                case Opcode::JMP:
                    i = std::get<size_t>(args) - 1;
                    break;
                case Opcode::JMP0:
                    if (!runtime.ipop()) {
                        i = std::get<size_t>(args) - 1;
                    }
                    break;
                case Opcode::CONST:
                    runtime.const_(std::get<size_t>(args));
                    break;
                case Opcode::STRING:
                    runtime.string(std::get<std::string>(args));
                    break;
                case Opcode::FUNC:
                    runtime.func(std::get<size_t>(args));
                    break;
                case Opcode::LOAD:
                    runtime.load(std::get<size_t>(args));
                    break;
                case Opcode::STORE:
                    runtime.store(std::get<size_t>(args));
                    break;
                case Opcode::TLOAD:
                    runtime.tload(std::get<size_t>(args));
                    break;
                case Opcode::LLOAD:
                    runtime.lload();
                    break;
                case Opcode::DLOAD:
                    runtime.dload();
                    break;
                case Opcode::TSTORE:
                    runtime.tstore(std::get<size_t>(args));
                    break;
                case Opcode::LSTORE:
                    runtime.lstore();
                    break;
                case Opcode::DSTORE:
                    runtime.dstore();
                    break;
                case Opcode::CALL:
                    runtime.call();
                    break;
                case Opcode::BIND:
                    runtime.bind(std::get<size_t>(args));
                    break;
                case Opcode::LOCAL:
                    runtime.local(std::get<size_t>(args));
                    break;
                case Opcode::AS:
                    runtime.as(std::get<std::string>(args));
                    break;
                case Opcode::IS:
                    runtime.is(std::get<std::string>(args));
                    break;
                case Opcode::ANY:
                    runtime.any(std::get<std::string>(args));
                    break;
                case Opcode::I2B:
                    runtime.i2b();
                    break;
                case Opcode::I2C:
                    runtime.i2c();
                    break;
                case Opcode::I2F:
                    runtime.i2f();
                    break;
                case Opcode::F2I:
                    runtime.f2i();
                    break;
                case Opcode::TUPLE:
                    runtime.tuple(std::get<size_t>(args));
                    break;
                case Opcode::LIST:
                    runtime.list(std::get<size_t>(args));
                    break;
                case Opcode::SET:
                    runtime.set(std::get<size_t>(args));
                    break;
                case Opcode::DICT:
                    runtime.dict(std::get<size_t>(args));
                    break;
                case Opcode::RETURN:
                    return runtime.return_();
                case Opcode::INEG:
                    runtime.ineg();
                    break;
                case Opcode::FNEG:
                    runtime.fneg();
                    break;
                case Opcode::NOT:
                    runtime.not_();
                    break;
                case Opcode::INV:
                    runtime.inv();
                    break;
                case Opcode::OR:
                    runtime.or_();
                    break;
                case Opcode::XOR:
                    runtime.xor_();
                    break;
                case Opcode::AND:
                    runtime.and_();
                    break;
                case Opcode::SHL:
                    runtime.shl();
                    break;
                case Opcode::SHR:
                    runtime.shr();
                    break;
                case Opcode::USHR:
                    runtime.ushr();
                    break;
                case Opcode::SEQ:
                    runtime.seq();
                    break;
                case Opcode::IEQ:
                    runtime.ieq();
                    break;
                case Opcode::FEQ:
                    runtime.feq();
                    break;
                case Opcode::SNE:
                    runtime.sne();
                    break;
                case Opcode::INE:
                    runtime.ine();
                    break;
                case Opcode::FNE:
                    runtime.fne();
                    break;
                case Opcode::SLT:
                    runtime.slt();
                    break;
                case Opcode::ILT:
                    runtime.ilt();
                    break;
                case Opcode::FLT:
                    runtime.flt();
                    break;
                case Opcode::SGT:
                    runtime.sgt();
                    break;
                case Opcode::IGT:
                    runtime.igt();
                    break;
                case Opcode::FGT:
                    runtime.fgt();
                    break;
                case Opcode::SLE:
                    runtime.sle();
                    break;
                case Opcode::ILE:
                    runtime.ile();
                    break;
                case Opcode::FLE:
                    runtime.fle();
                    break;
                case Opcode::SGE:
                    runtime.sge();
                    break;
                case Opcode::IGE:
                    runtime.ige();
                    break;
                case Opcode::FGE:
                    runtime.fge();
                    break;
                case Opcode::SADD:
                    runtime.sadd();
                    break;
                case Opcode::IADD:
                    runtime.iadd();
                    break;
                case Opcode::FADD:
                    runtime.fadd();
                    break;
                case Opcode::ISUB:
                    runtime.isub();
                    break;
                case Opcode::FSUB:
                    runtime.fsub();
                    break;
                case Opcode::IMUL:
                    runtime.imul();
                    break;
                case Opcode::FMUL:
                    runtime.fmul();
                    break;
                case Opcode::IDIV:
                    runtime.idiv();
                    break;
                case Opcode::FDIV:
                    runtime.fdiv();
                    break;
                case Opcode::IREM:
                    runtime.irem();
                    break;
                case Opcode::FREM:
                    runtime.frem();
                    break;
                case Opcode::INC:
                    runtime.inc(std::get<size_t>(args));
                    break;
                case Opcode::DEC:
                    runtime.dec(std::get<size_t>(args));
                    break;
            }
        }
        __builtin_unreachable();
    } else {
        return std::get<ExternalFunction>(f)(captures);
    }
} catch (Runtime::Exception& e) {
    e.append("at func " + std::to_string(func));
    throw;
}

}