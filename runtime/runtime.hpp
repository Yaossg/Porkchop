#pragma once

#include <deque>
#include <bit>
#include <unordered_set>
#include <cmath>

#include "assembly.hpp"

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
        TypeReference type;
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
            return as_size(new std::vector{key, value});
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
        return as_double(pop());
    }

    std::string *spop() {
        return as_string(pop());
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
        stack.push_back(as_size(value));
    }

    void push(void *ptr) {
        stack.push_back(as_size(ptr));
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

    void as(TypeReference const& type) {
        auto any = std::bit_cast<Any *>(pop());
        if (!any->type->equals(type))
            throw Exception("cannot cast " + any->type->toString() + " to " + type->toString());
        stack.emplace_back(any->value);
    }

    void is(TypeReference const& type) {
        stack.emplace_back(std::bit_cast<Any *>(pop())->type->equals(type));
    }

    void any(TypeReference const& type) {
        push(new Any{pop(), type});
    }

    void i2b() {
        auto value = ipop();
        stack.push_back(value & 0xFF);
    }

    void i2c() {
        auto value = ipop();
        if (isInvalidChar(value))
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
        if (value2 < 0)
            throw Exception("shift a negative");
        stack.push_back(value1 << value2);
    }

    void shr() {
        auto value2 = ipop();
        auto value1 = ipop();
        if (value2 < 0)
            throw Exception("shift a negative");
        stack.push_back(value1 >> value2);
    }

    void ushr() {
        auto value2 = ipop();
        auto value1 = pop();
        if (value2 < 0)
            throw Exception("shift a negative");
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

}