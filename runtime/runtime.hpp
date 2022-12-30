#pragma once

#include <bit>
#include <unordered_set>
#include <cmath>

#include "assembly.hpp"
#include "vm.hpp"

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

    Assembly *assembly;
    std::shared_ptr<VM::Frame> frame;

    Runtime(Assembly *assembly, std::shared_ptr<VM::Frame> frame)
            : assembly(assembly), frame(std::move(frame)) {}

    void local(TypeReference const& type) {
        frame->local(type);
    }

    void dup() {
        frame->dup();
    }

    $union top() {
        return frame->stack.back();
    }

    $union pop() {
        auto back = frame->stack.back();
        frame->pop();
        return back;
    }

    int64_t ipop() {
        return pop().$int;
    }

    double fpop() {
        return pop().$float;
    }

    Object* opop() {
        return pop().$object;
    }

    String* spop() {
        return dynamic_cast<String*>(opop());
    }

    std::vector<$union> npop(size_t n) {
        auto b1 = std::prev(frame->stack.end(), n), e1 = frame->stack.end();
        auto b2 = std::prev(frame->companion.end(), n), e2 = frame->companion.end();
        std::vector<$union> r1{b1, e1};
        frame->stack.erase(b1, e1);
        frame->companion.erase(b2, e2);
        return r1;
    }

    $union return_() {
        $union ret = frame->stack.back();
        frame->vm->frames.pop_back();
        return ret;
    }

    void const_($union value) {
        frame->const_(value);
    }

    void push(std::pair<$union, bool> value) {
        frame->push(value.first, value.second);
    }

    void push($union value, bool type) {
        frame->push(value, type);
    }

    void push(bool value) {
        frame->push(value);
    }

    void push(int64_t value) {
        frame->push(value);
    }

    void push(double value) {
        frame->push(value);
    }

    void push(Object* object) {
        frame->push(object);
    }

    void push(std::string const& value) {
        push(frame->vm->newObject<String>(value));
    }

    void sconst(size_t index) {
        push(assembly->table[index]);
    }

    void fconst(size_t index) {
        auto func = std::dynamic_pointer_cast<FuncType>(assembly->prototypes[index]);
        push(frame->vm->newObject<Func>(index, func));
    }

    void load(size_t index) {
        frame->load(index);
    }

    void store(size_t index) {
        frame->store(index);
    }

    void tload(size_t index) {
        auto tuple = dynamic_cast<Tuple*>(opop());
        push(tuple->load(index));
    }

    void lload() {
        auto index = ipop();
        auto list = dynamic_cast<List*>(opop());
        if (index < 0 || index >= list->size())
            throw Exception("index out of bound");
        push(list->load(index), dynamic_cast<ObjectList*>(list));
    }

    void lstore() {
        auto index = ipop();
        auto list = dynamic_cast<List*>(opop());
        if (index < 0 || index >= list->size())
            throw Exception("index out of bound");
        auto value = top();
        list->store(index, value);
    }

    void dload() {
        auto key = pop();
        auto dict = dynamic_cast<Dict*>(opop());
        if (!dict->elements.contains(key))
            throw Exception("missing value for key");
        push(dict->elements.at(key), !isValueBased(dict->prototype->V));
    }

    void dstore() {
        auto key = pop();
        auto dict = dynamic_cast<Dict*>(opop());
        auto value = top();
        dict->elements.insert_or_assign(key, value);
    }

    void call() {
        auto func = dynamic_cast<Func *>(opop());
        frame->vm->temporaries.push_back(func);
        push(func->call(assembly, frame->vm), !isValueBased(func->prototype->R));
        frame->vm->temporaries.pop_back();
    }

    void bind(size_t size) {
        if (size > 0) {
            auto captures = npop(size);
            auto unbound = dynamic_cast<Func*>(opop());
            size_t temporaries = 0;
            for (size_t i = 0; i < captures.size(); ++i) {
                if (!isValueBased(unbound->prototype->P[i])) {
                    frame->vm->temporaries.push_back(captures[i].$object);
                    ++temporaries;
                }
            }
            frame->vm->temporaries.push_back(unbound);
            auto bound = unbound->bind(captures);
            push(bound);
            frame->vm->temporaries.pop_back();
            for (size_t i = 0; i < temporaries; ++i)
                frame->vm->temporaries.pop_back();
        }
    }

    void as(TypeReference const& type) {
        auto object = opop();
        auto type0 = object->getType();
        if (!type->assignableFrom(type0)) {
            throw Exception("cannot cast " + type0->toString() + " to " + type->toString());
        }
        if (isValueBased(type)) {
            const_(dynamic_cast<AnyScalar*>(object)->value);
        } else {
            push(object);
        }
    }

    void is(TypeReference const& type) {
        push(opop()->getType()->equals(type));
    }

    void any(TypeReference const& type) {
        if (isValueBased(type)) {
            push(frame->vm->newObject<AnyScalar>(pop(), dynamic_cast<ScalarType*>(type.get())->S));
        }
    }

    void i2b() {
        auto value = ipop();
        push(value & 0xFF);
    }

    void i2c() {
        auto value = ipop();
        if (isInvalidChar(value))
            throw Exception("int is invalid to cast to char");
        push(value);
    }

    void i2f() {
        auto value = ipop();
        push((double) value);
    }

    void f2i() {
        auto value = int64_t(fpop());
        push(value);
    }

    void tuple(TypeReference const& prototype) {
        auto tuple = std::dynamic_pointer_cast<TupleType>(prototype);
        auto elements = npop(tuple->E.size());
        if (elements.size() == 2) {
            push(frame->vm->newObject<Pair>(elements.front(), elements.back(), tuple->E.front(), tuple->E.back()));
        } else {
            push(frame->vm->newObject<More>(std::move(elements), tuple));
        }
    }

    void list(std::pair<TypeReference, size_t> const& cons) {
        auto elements = npop(cons.second);
        auto list = dynamic_pointer_cast<ListType>(cons.first);
        if (isValueBased(list->E)) {
            switch (auto type = dynamic_cast<ScalarType*>(list->E.get())->S) {
                case ScalarTypeKind::NONE:
                    push(frame->vm->newObject<NoneList>(elements.size()));
                    break;
                case ScalarTypeKind::BOOL: {
                    std::vector<bool> elements0(elements.size());
                    for (size_t i = 0; i < elements.size(); ++i) {
                        elements0[i] = elements[i].$bool;
                    }
                    push(frame->vm->newObject<BoolList>(std::move(elements0)));
                    break;
                }
                case ScalarTypeKind::BYTE: {
                    std::vector<unsigned char> elements0(elements.size());
                    for (size_t i = 0; i < elements.size(); ++i) {
                        elements0[i] = elements[i].$byte;
                    }
                    push(frame->vm->newObject<ByteList>(std::move(elements0)));
                    break;
                }
                default:
                    push(frame->vm->newObject<ScalarList>(std::move(elements), type));
                    break;
            }
        } else {
            push(frame->vm->newObject<ObjectList>(std::move(elements), list));
        }
    }

    void set(std::pair<TypeReference, size_t> const& cons) {
        auto elements = npop(cons.second);
        auto type = std::dynamic_pointer_cast<SetType>(cons.first);
        auto kind = getIdentityKind(type->E);
        Set::underlying set(elements.begin(), elements.end(), 0, Hasher{kind}, Equator{kind});
        push(frame->vm->newObject<Set>(std::move(set), std::move(type)));
    }

    void dict(std::pair<TypeReference, size_t> const& cons) {
        auto elements = npop(cons.second * 2);
        auto type = std::dynamic_pointer_cast<DictType>(cons.first);
        auto kind = getIdentityKind(type->K);
        Dict::underlying map{0, Hasher{kind}, Equator{kind}};
        for (size_t i = 0; i < cons.second; ++i) {
            map.insert_or_assign(elements[2 * i], elements[2 * i + 1]);
        }
        push(frame->vm->newObject<Dict>(std::move(map), std::move(type)));
    }

    void ineg() {
        push(-ipop());
    }

    void fneg() {
        push(-fpop());
    }

    void not_() {
        push(!ipop());
    }

    void inv() {
        push(~ipop());
    }

    void or_() {
        auto value2 = ipop();
        auto value1 = ipop();
        push(value1 | value2);
    }

    void xor_() {
        auto value2 = ipop();
        auto value1 = ipop();
        push(value1 ^ value2);
    }

    void and_() {
        auto value2 = ipop();
        auto value1 = ipop();
        push(value1 & value2);
    }

    void shl() {
        auto value2 = ipop();
        auto value1 = ipop();
        if (value2 < 0)
            throw Exception("shift a negative");
        push(value1 << value2);
    }

    void shr() {
        auto value2 = ipop();
        auto value1 = ipop();
        if (value2 < 0)
            throw Exception("shift a negative");
        push(value1 >> value2);
    }

    void ushr() {
        auto value2 = ipop();
        auto value1 = pop().$size;
        if (value2 < 0)
            throw Exception("shift a negative");
        push(int64_t(value1 >> value2));
    }

    static constexpr size_t less = 0;
    static constexpr size_t equivalent = 1;
    static constexpr size_t greater = 2;
    static constexpr size_t unordered = 3;

    void compare_three_way(std::partial_ordering o) {
        if (o == std::partial_ordering::less) {
            const_(less);
        } else if (o == std::partial_ordering::equivalent) {
            const_(equivalent);
        } else if (o == std::partial_ordering::greater) {
            const_(greater);
        } else if (o == std::partial_ordering::unordered) {
            const_(unordered);
        }
    }

    void ucmp() {
        auto value2 = pop().$size;
        auto value1 = pop().$size;
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
        compare_three_way(value1->value <=> value2->value);
    }

    void ocmp() {
        auto value2 = opop();
        auto value1 = opop();
        compare_three_way(value1->equals(value2) ? std::partial_ordering::equivalent : std::partial_ordering::unordered);
    }

    void eq() {
        auto cmp = pop().$size;
        push(cmp == equivalent);
    }

    void ne() {
        auto cmp = pop().$size;
        push(cmp != equivalent);
    }

    void lt() {
        auto cmp = pop().$size;
        push(cmp == less);
    }

    void gt() {
        auto cmp = pop().$size;
        push(cmp == greater);
    }

    void le() {
        auto cmp = pop().$size;
        push(cmp == less || cmp == equivalent);
    }

    void ge() {
        auto cmp = pop().$size;
        push(cmp == greater || cmp == equivalent);
    }

    void sadd() {
        auto value2 = spop();
        auto value1 = spop();
        push(value1->value + value2->value);
    }

    void iadd() {
        auto value2 = ipop();
        auto value1 = ipop();
        push(value1 + value2);
    }

    void isub() {
        auto value2 = ipop();
        auto value1 = ipop();
        push(value1 - value2);
    }

    void imul() {
        auto value2 = ipop();
        auto value1 = ipop();
        push(value1 * value2);
    }

    void idiv() {
        auto value2 = ipop();
        if (value2 == 0)
            throw Exception("divided by zero");
        auto value1 = ipop();
        push(value1 / value2);
    }

    void irem() {
        auto value2 = ipop();
        if (value2 == 0)
            throw Exception("divided by zero");
        auto value1 = ipop();
        push(value1 % value2);
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
        ++frame->stack[index].$int;
    }

    void dec(size_t index) {
        --frame->stack[index].$int;
    }

    void iter() {
        auto object = opop();
        frame->vm->temporaries.push_back(object);
        push(dynamic_cast<Iterable*>(object)->iterator());
        frame->vm->temporaries.pop_back();
    }

    void peek() {
        auto object = opop();
        auto iter = dynamic_cast<Iterator*>(object);
        push(object);
        push(iter->peek());
    }

    void next() {
        auto object = opop();
        auto iter = dynamic_cast<Iterator*>(object);
        push(object);
        push(iter->next(), !isValueBased(iter->E));
    }

    void i2s() {
        auto value = pop();
        Stringifier sf{ScalarTypeKind::INT};
        push(sf(value));
    }

    void f2s() {
        auto value = pop();
        Stringifier sf{ScalarTypeKind::FLOAT};
        push(sf(value));
    }

    void b2s() {
        auto value = pop();
        Stringifier sf{ScalarTypeKind::BYTE};
        push(sf(value));
    }


    void z2s() {
        const static std::string $true{"true"}, $false{"false"};
        auto value = pop();
        push(value.$bool ? $true : $false);
    }

    void c2s() {
        auto value = pop();
        Stringifier sf{ScalarTypeKind::CHAR};
        push(sf(value));
    }

    void o2s() {
        auto object = opop();
        push(object->toString());
    }

    void add() {
        auto value = pop();
        auto collection = dynamic_cast<Collection*>(opop());
        collection->add(value);
        push(collection);
    }

    void remove() {
        auto value = pop();
        auto collection = dynamic_cast<Collection*>(opop());
        collection->remove(value);
        push(collection);
    }

    void in() {
        auto collection = dynamic_cast<Collection*>(opop());
        auto value = pop();
        push(collection->contains(value));
    }
};

}