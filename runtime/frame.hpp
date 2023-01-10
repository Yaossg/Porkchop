#pragma once

#include <bit>
#include <unordered_set>
#include <cmath>

#include "assembly.hpp"
#include "vm.hpp"

namespace Porkchop {

struct Frame {
    VM* vm;
    Assembly *assembly;
    Instructions* instructions;
    std::vector<$union> stack;
    std::vector<bool> companion;
    size_t pc = 0;

    Frame(VM* vm, Assembly* assembly, Instructions* instructions, std::vector<$union> captures = {})
            : vm(vm), assembly(assembly), instructions(instructions), stack(std::move(captures)) {}

    void pushToVM() {
        vm->frames.push_back(this);
    }

    void popFromVM() {
        vm->frames.pop_back();
    }

    void local(TypeReference const& type) {
        companion.push_back(!isValueBased(type));
        if (companion.size() > stack.size()) {
            stack.emplace_back(nullptr);
        }
    }

    void dup() {
        stack.push_back(stack.back());
        companion.push_back(companion.back());
    }

    $union pop() {
        auto back = top();
        stack.pop_back();
        companion.pop_back();
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
        auto b1 = std::prev(stack.end(), n), e1 = stack.end();
        auto b2 = std::prev(companion.end(), n), e2 = companion.end();
        std::vector<$union> r1{b1, e1};
        stack.erase(b1, e1);
        companion.erase(b2, e2);
        return r1;
    }

    $union top() {
        return stack.back();
    }

    void push(std::pair<$union, bool> value) {
        push(value.first, value.second);
    }

    void push($union value, bool type) {
        stack.emplace_back(value);
        companion.push_back(type);
    }

    void const_($union value) {
        stack.emplace_back(value);
        companion.push_back(false);
    }

    void push(bool value) {
        stack.emplace_back(value);
        companion.push_back(false);
    }

    void push(int64_t value) {
        stack.emplace_back(value);
        companion.push_back(false);
    }

    void push(double value) {
        stack.emplace_back(value);
        companion.push_back(false);
    }

    void push(Object* object) {
        stack.emplace_back(object);
        companion.push_back(true);
    }

    void push(std::string const& value) {
        push(vm->newObject<String>(value));
    }

    void load(size_t index) {
        stack.push_back(stack[index]);
        companion.push_back(companion[index]);
    }

    void store(size_t index) {
        stack[index] = stack.back();
    }

    void markAll() {
        for (size_t i = 0; i < stack.size(); ++i) {
            if (companion[i] && stack[i].$object)
                stack[i].$object->mark();
        }
    }

    void sconst(size_t index) {
        push(assembly->table[index]);
    }

    void fconst(size_t index) {
        auto func = std::dynamic_pointer_cast<FuncType>(assembly->prototypes[index]);
        push(vm->newObject<Func>(index, func));
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
        VM::ObjectHolder object = opop();
        auto func = object.as<Func>();
        push(func->call(assembly, vm), !isValueBased(func->prototype->R));
    }

    void bind(size_t size) {
        if (size > 0) {
            VM::GCGuard guard{vm};
            auto captures = npop(size);
            auto object = opop();
            push(dynamic_cast<Func*>(object)->bind(std::move(captures)));
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
            push(vm->newObject<AnyScalar>(pop(), dynamic_cast<ScalarType*>(type.get())->S));
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
        VM::GCGuard guard{vm};
        auto tuple = std::dynamic_pointer_cast<TupleType>(prototype);
        auto elements = npop(tuple->E.size());
        if (elements.size() == 2) {
            push(vm->newObject<Pair>(elements.front(), elements.back(), tuple->E.front(), tuple->E.back()));
        } else {
            push(vm->newObject<More>(std::move(elements), tuple));
        }
    }

    void list(std::pair<TypeReference, size_t> const& cons) {
        auto elements = npop(cons.second);
        auto list = dynamic_pointer_cast<ListType>(cons.first);
        if (isValueBased(list->E)) {
            switch (auto type = dynamic_cast<ScalarType*>(list->E.get())->S) {
                case ScalarTypeKind::NONE:
                    push(vm->newObject<NoneList>(elements.size()));
                    break;
                case ScalarTypeKind::BOOL: {
                    std::vector<bool> elements0(elements.size());
                    for (size_t i = 0; i < elements.size(); ++i) {
                        elements0[i] = elements[i].$bool;
                    }
                    push(vm->newObject<BoolList>(std::move(elements0)));
                    break;
                }
                case ScalarTypeKind::BYTE: {
                    std::vector<uint8_t> elements0(elements.size());
                    for (size_t i = 0; i < elements.size(); ++i) {
                        elements0[i] = elements[i].$byte;
                    }
                    push(vm->newObject<ByteList>(std::move(elements0)));
                    break;
                }
                default:
                    push(vm->newObject<ScalarList>(std::move(elements), type));
                    break;
            }
        } else {
            VM::GCGuard guard{vm};
            push(vm->newObject<ObjectList>(std::move(elements), list));
        }
    }

    void set(std::pair<TypeReference, size_t> const& cons) {
        VM::GCGuard guard{vm};
        auto elements = npop(cons.second);
        auto type = std::dynamic_pointer_cast<SetType>(cons.first);
        if (auto scalar = dynamic_cast<ScalarType*>(type->E.get())) {
            switch (scalar->S) {
                case ScalarTypeKind::NONE: {
                    push(vm->newObject<NoneSet>(!elements.empty()));
                    return;
                }
                case ScalarTypeKind::BOOL: {
                    auto set = vm->newObject<BoolSet>();
                    for (auto&& element : elements)
                        set->add(element);
                    push(set);
                    return;
                }
                case ScalarTypeKind::BYTE: {
                    auto set = vm->newObject<ByteSet>();
                    for (auto&& element : elements)
                        set->add(element);
                    push(set);
                    return;
                }
            }
        }
        auto kind = getIdentityKind(type->E);
        Set::underlying set(elements.begin(), elements.end(), 0, Hasher{kind}, Equator{kind});
        push(vm->newObject<Set>(std::move(set), std::move(type)));
    }

    void dict(std::pair<TypeReference, size_t> const& cons) {
        VM::GCGuard guard{vm};
        auto elements = npop(cons.second * 2);
        auto type = std::dynamic_pointer_cast<DictType>(cons.first);
        auto kind = getIdentityKind(type->K);
        Dict::underlying map{0, Hasher{kind}, Equator{kind}};
        for (size_t i = 0; i < cons.second; ++i) {
            map.insert_or_assign(elements[2 * i], elements[2 * i + 1]);
        }
        push(vm->newObject<Dict>(std::move(map), std::move(type)));
    }

    void ineg() {
        push(-ipop());
    }

    void fneg() {
        push(-fpop());
    }

    void not_() {
        push(!pop().$bool);
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
        push(value1 << value2);
    }

    void shr() {
        auto value2 = ipop();
        auto value1 = ipop();
        push(value1 >> value2);
    }

    void ushr() {
        auto value2 = ipop();
        auto value1 = pop().$size;
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
        ++stack[index].$int;
    }

    void dec(size_t index) {
        --stack[index].$int;
    }

    void iter() {
        VM::ObjectHolder object = opop();
        push(object.as<Iterable>()->iterator());
    }

    void move() {
        VM::ObjectHolder object = opop();
        push(object.as<Iterator>()->move());
    }

    void get() {
        VM::ObjectHolder object = opop();
        auto iter = object.as<Iterator>();
        push(iter->get(), !isValueBased(iter->E));
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

    void sizeof_() {
        auto sizeable = dynamic_cast<Sizeable*>(opop());
        push((int64_t) sizeable->size());
    }

    void fhash() {
        push((int64_t) std::hash<double>()(fpop()));
    }

    void ohash() {
        push((int64_t) opop()->hashCode());
    }

    void sjoin(size_t size) {
        auto strings = npop(size);
        std::string buf;
        for (auto&& string : strings) {
            buf += dynamic_cast<String*>(string.$object)->value;
        }
        push(vm->newObject<String>(std::move(buf)));
    }

    $union yield() {
        $union ret = stack.back();
        popFromVM();
        return ret;
    }

    [[nodiscard]] Opcode code() const {
        return instructions->at(pc).first;
    }

    void init() {
        for (pc = 0; code() == Opcode::LOCAL; ++pc) {
            local(std::get<TypeReference>(instructions->at(pc).second));
        }
    }

    $union loop() {
        for (pushToVM(); pc < instructions->size(); ++pc) {
            switch (auto&& [opcode, args] = instructions->at(pc); opcode) {
                case Opcode::NOP:
                    break;
                case Opcode::DUP:
                    dup();
                    break;
                case Opcode::POP:
                    pop();
                    break;
                case Opcode::JMP:
                    pc = std::get<size_t>(args) - 1;
                    break;
                case Opcode::JMP0:
                    if (!pop().$bool) {
                        pc = std::get<size_t>(args) - 1;
                    }
                    break;
                case Opcode::CONST:
                    const_(std::get<size_t>(args));
                    break;
                case Opcode::SCONST:
                    sconst(std::get<size_t>(args));
                    break;
                case Opcode::FCONST:
                    fconst(std::get<size_t>(args));
                    break;
                case Opcode::LOAD:
                    load(std::get<size_t>(args));
                    break;
                case Opcode::STORE:
                    store(std::get<size_t>(args));
                    break;
                case Opcode::TLOAD:
                    tload(std::get<size_t>(args));
                    break;
                case Opcode::LLOAD:
                    lload();
                    break;
                case Opcode::DLOAD:
                    dload();
                    break;
                case Opcode::LSTORE:
                    lstore();
                    break;
                case Opcode::DSTORE:
                    dstore();
                    break;
                case Opcode::CALL:
                    call();
                    break;
                case Opcode::BIND:
                    bind(std::get<size_t>(args));
                    break;
                case Opcode::AS:
                    as(std::get<TypeReference>(args));
                    break;
                case Opcode::IS:
                    is(std::get<TypeReference>(args));
                    break;
                case Opcode::ANY:
                    any(std::get<TypeReference>(args));
                    break;
                case Opcode::I2B:
                    i2b();
                    break;
                case Opcode::I2C:
                    i2c();
                    break;
                case Opcode::I2F:
                    i2f();
                    break;
                case Opcode::F2I:
                    f2i();
                    break;
                case Opcode::TUPLE:
                    tuple(std::get<TypeReference>(args));
                    break;
                case Opcode::LIST:
                    list(std::get<std::pair<TypeReference, size_t>>(args));
                    break;
                case Opcode::SET:
                    set(std::get<std::pair<TypeReference, size_t>>(args));
                    break;
                case Opcode::DICT:
                    dict(std::get<std::pair<TypeReference, size_t>>(args));
                    break;
                case Opcode::INEG:
                    ineg();
                    break;
                case Opcode::FNEG:
                    fneg();
                    break;
                case Opcode::NOT:
                    not_();
                    break;
                case Opcode::INV:
                    inv();
                    break;
                case Opcode::OR:
                    or_();
                    break;
                case Opcode::XOR:
                    xor_();
                    break;
                case Opcode::AND:
                    and_();
                    break;
                case Opcode::SHL:
                    shl();
                    break;
                case Opcode::SHR:
                    shr();
                    break;
                case Opcode::USHR:
                    ushr();
                    break;
                case Opcode::UCMP:
                    ucmp();
                    break;
                case Opcode::ICMP:
                    icmp();
                    break;
                case Opcode::FCMP:
                    fcmp();
                    break;
                case Opcode::SCMP:
                    scmp();
                    break;
                case Opcode::OCMP:
                    ocmp();
                    break;
                case Opcode::EQ:
                    eq();
                    break;
                case Opcode::NE:
                    ne();
                    break;
                case Opcode::LT:
                    lt();
                    break;
                case Opcode::GT:
                    gt();
                    break;
                case Opcode::LE:
                    le();
                    break;
                case Opcode::GE:
                    ge();
                    break;
                case Opcode::SADD:
                    sadd();
                    break;
                case Opcode::IADD:
                    iadd();
                    break;
                case Opcode::FADD:
                    fadd();
                    break;
                case Opcode::ISUB:
                    isub();
                    break;
                case Opcode::FSUB:
                    fsub();
                    break;
                case Opcode::IMUL:
                    imul();
                    break;
                case Opcode::FMUL:
                    fmul();
                    break;
                case Opcode::IDIV:
                    idiv();
                    break;
                case Opcode::FDIV:
                    fdiv();
                    break;
                case Opcode::IREM:
                    irem();
                    break;
                case Opcode::FREM:
                    frem();
                    break;
                case Opcode::INC:
                    inc(std::get<size_t>(args));
                    break;
                case Opcode::DEC:
                    dec(std::get<size_t>(args));
                    break;
                case Opcode::ITER:
                    iter();
                    break;
                case Opcode::MOVE:
                    move();
                    break;
                case Opcode::GET:
                    get();
                    break;
                case Opcode::I2S:
                    i2s();
                    break;
                case Opcode::F2S:
                    f2s();
                    break;
                case Opcode::B2S:
                    b2s();
                    break;
                case Opcode::Z2S:
                    z2s();
                    break;
                case Opcode::C2S:
                    c2s();
                    break;
                case Opcode::O2S:
                    o2s();
                    break;
                case Opcode::ADD:
                    add();
                    break;
                case Opcode::REMOVE:
                    remove();
                    break;
                case Opcode::IN:
                    in();
                    break;
                case Opcode::SIZEOF:
                    sizeof_();
                    break;
                case Opcode::FHASH:
                    fhash();
                    break;
                case Opcode::OHASH:
                    ohash();
                    break;
                case Opcode::RETURN:
                case Opcode::YIELD:
                    return yield();
                case Opcode::SJOIN:
                    sjoin(std::get<size_t>(args));
                    break;
                default:
                    unreachable();
            }
        }
        unreachable();
    }
};

}