#pragma once

#include <deque>
#include <bit>
#include <unordered_set>
#include <cmath>

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

    struct Iter {
        virtual ~Iter() = default;

        [[nodiscard]] virtual bool peek() const = 0;
        virtual size_t next() = 0;
    };

    template<typename It>
    struct IterImpl : Iter {
        It first, last;
        IterImpl(It first, It last): first(first), last(last) {}

        [[nodiscard]] bool peek() const override {
            return first != last;
        }

        size_t next() override {
            return *first++;
        }
    };


    template<typename It>
    struct DictIterImpl : Iter {
        It first, last;
        DictIterImpl(It first, It last): first(first), last(last) {}

        [[nodiscard]] bool peek() const override {
            return first != last;
        }

        size_t next() override {
            auto [key, value] = *first++;
            return std::bit_cast<size_t>(new std::vector{key, value});
        }
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

    static constexpr size_t less = 0;
    static constexpr size_t equivalent = 1;
    static constexpr size_t greater = 2;
    static constexpr size_t unordered = 3;

    void compare_three_way(std::partial_ordering o) {
        if (o == std::partial_ordering::less) {
            stack.push_back(less);
        } else if (o == std::partial_ordering::equivalent) {
            stack.push_back(equivalent);
        } else if (o == std::partial_ordering::greater) {
            stack.push_back(greater);
        } else if (o == std::partial_ordering::unordered) {
            stack.push_back(unordered);
        }
    }

    void ucmp() {
        auto value2 = pop();
        auto value1 = pop();
        compare_three_way(value1 <=> value2);
    }

    void icmp() {
        auto value2 = ipop();
        auto value1 = ipop();
        compare_three_way(value1 <=> value2);
    }

    void fcmp() {
        auto value2 = fpop();
        auto value1 = fpop();
        compare_three_way(value1 <=> value2);
    }

    void scmp() {
        auto value2 = spop();
        auto value1 = spop();
        compare_three_way(*value1 <=> *value2);
    }

    void eq() {
        auto cmp = pop();
        stack.push_back(cmp == equivalent);
    }

    void ne() {
        auto cmp = pop();
        stack.push_back(cmp != equivalent);
    }

    void lt() {
        auto cmp = pop();
        stack.push_back(cmp == less);
    }

    void gt() {
        auto cmp = pop();
        stack.push_back(cmp == greater);
    }

    void le() {
        auto cmp = pop();
        stack.push_back(cmp == less || cmp == equivalent);
    }

    void ge() {
        auto cmp = pop();
        stack.push_back(cmp == greater || cmp == equivalent);
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

    void iter(size_t type) {
        switch (type) {
            case 0: {
                auto set = std::bit_cast<std::unordered_set<size_t> *>(pop());
                push(new IterImpl{set->begin(), set->end()});
                break;
            }
            case 1: {
                auto list = std::bit_cast<std::vector<size_t> *>(pop());
                push(new IterImpl{list->begin(), list->end()});
                break;
            }
            case 2: {
                auto dict = std::bit_cast<std::unordered_map<size_t, size_t> *>(pop());
                push(new DictIterImpl{dict->begin(), dict->end()});
                break;
            }
        }
    }

    void peek() {
        auto iter = std::bit_cast<Iter*>(stack.back());
        stack.push_back(iter->peek());
    }

    void next() {
        auto iter = std::bit_cast<Iter*>(stack.back());
        stack.push_back(iter->next());
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
                case Opcode::UCMP:
                    runtime.ucmp();
                    break;
                case Opcode::ICMP:
                    runtime.icmp();
                    break;
                case Opcode::FCMP:
                    runtime.fcmp();
                    break;
                case Opcode::SCMP:
                    runtime.scmp();
                    break;
                case Opcode::EQ:
                    runtime.eq();
                    break;
                case Opcode::NE:
                    runtime.ne();
                    break;
                case Opcode::LT:
                    runtime.lt();
                    break;
                case Opcode::GT:
                    runtime.gt();
                    break;
                case Opcode::LE:
                    runtime.le();
                    break;
                case Opcode::GE:
                    runtime.ge();
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
                case Opcode::ITER:
                    runtime.iter(std::get<size_t>(args));
                    break;
                case Opcode::PEEK:
                    runtime.peek();
                    break;
                case Opcode::NEXT:
                    runtime.next();
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