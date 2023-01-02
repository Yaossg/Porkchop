#include "vm.hpp"
#include "runtime.hpp"
#include "../unicode/unicode.hpp"

namespace Porkchop {

$union Func::call(Assembly *assembly, VM* vm) const try {
    auto& f = assembly->functions[func];
    if (std::holds_alternative<Instructions>(f)) {
        auto runtime = std::make_unique<Runtime>(assembly, vm->newFrame(captures), std::get<Instructions>(f));
        if (runtime->code() == Opcode::ASYNC) {
            return vm->newObject<Coroutine>(prototype->R, std::move(runtime));
        } else {
            return runtime->loop();
        }
    } else {
        return std::get<ExternalFunctionR>(f)(vm, captures);
    }
} catch (Exception& e) {
    e.append("at func " + std::to_string(func));
    throw;
}

std::string AnyScalar::toString() {
    return Stringifier{type}(value);
}

bool AnyScalar::equals(Object *other) {
    if (this == other) return true;
    if (auto scalar = dynamic_cast<AnyScalar*>(other)) {
        if (type != scalar->type) return false;
        switch (type) {
            case ScalarTypeKind::NONE:
                return true;
            case ScalarTypeKind::FLOAT:
                return value.$float == scalar->value.$float;
            default:
                return value.$size == scalar->value.$size;
        }
    }
    return false;
}

size_t AnyScalar::hashCode() {
    switch (type) {
        case ScalarTypeKind::NONE:
            return 0;
        case ScalarTypeKind::FLOAT:
            return std::hash<double>()(value.$float);
        default:
            return value.$size;
    }
}

std::string String::toString() {
    return value;
}

bool String::equals(Object *other) {
    if (this == other) return true;
    if (auto string = dynamic_cast<String*>(other)) {
        return value == string->value;
    }
    return false;
}

size_t String::hashCode() {
    return std::hash<std::string>()(value);
}

std::string Pair::toString() {
    return join("(", stringifier(T)(first), ", ", stringifier(U)(second), ")");
}

bool Pair::equals(Object *other) {
    if (this == other) return true;
    if (auto pair = dynamic_cast<Pair*>(other)) {
        return T->equals(pair->T) && U->equals(pair->U)
        && Equator{t}(first, pair->first) && Equator{u}(second, pair->second);
    }
    return false;
}

size_t Pair::hashCode() {
    return (Hasher{t}(first) << 1) ^ Hasher{u}(second);
}

std::string More::toString() {
    std::string buf = "(";
    bool first = true;
    for (size_t i = 0; i < elements.size(); ++i) {
        if (first) { first = false; } else { buf += ", "; }
        buf += stringifier(prototype->E[i])(elements[i]);
    }
    buf += ")";
    return buf;
}

bool More::equals(Object *other) {
    if (this == other) return true;
    if (auto more = dynamic_cast<More *>(other)) {
        if (elements.size() != more->elements.size()) return false;
        if (!prototype->equals(more->prototype)) return false;
        for (size_t i = 0; i < elements.size(); ++i) {
            if (!Equator{getIdentityKind(prototype->E[i])}(elements[i], more->elements[i])) {
                return false;
            }
        }
        return true;
    }
    return false;
}

size_t More::hashCode() {
    size_t hash = 0;
    for (size_t i = 0; i < elements.size(); ++i) {
        hash <<= 1;
        hash ^= Hasher{getIdentityKind(prototype->E[i])}(elements[i]);
    }
    return hash;
}

std::string ObjectList::toString() {
    std::string buf = "[";
    bool first = true;
    for (auto&& element : elements) {
        if (first) { first = false; } else { buf += ", "; }
        buf += element.$object->toString();
    }
    buf += "]";
    return buf;
}

bool ObjectList::equals(Object *other) {
    if (this == other) return true;
    if (auto list = dynamic_cast<ObjectList*>(other)) {
        if (!prototype->equals(list->prototype)) return false;
        return std::equal(elements.begin(), elements.end(), list->elements.begin(), list->elements.end(),
                          []($union lhs, $union rhs) { return lhs.$object->equals(rhs.$object); });
    }
    return Object::equals(other);
}

size_t ObjectList::hashCode() {
    size_t hash = 0;
    for (auto&& element : elements) {
        hash <<= 1;
        hash ^= element.$object->hashCode();
    }
    return hash;
}

std::string NoneList::toString() {
    std::string buf = "[";
    for (size_t i = 0; i < count; ++i) {
        if (i) { buf += ", "; }
        buf += "()";
    }
    buf += "]";
    return buf;
}

bool NoneList::equals(Object *other) {
    if (this == other) return true;
    if (auto list = dynamic_cast<NoneList*>(other)) {
        return count == list->count;
    }
    return Object::equals(other);
}

size_t NoneList::hashCode() {
    return count;
}

std::string BoolList::toString() {
    std::string buf = "[";
    bool first = true;
    for (auto&& element : elements) {
        if (first) { first = false; } else { buf += ", "; }
        buf += element ? "true" : "false";
    }
    buf += "]";
    return buf;
}

bool BoolList::equals(Object *other) {
    if (this == other) return true;
    if (auto list = dynamic_cast<BoolList*>(other)) {
        return elements == list->elements;
    }
    return false;
}

size_t BoolList::hashCode() {
    return std::hash<std::vector<bool>>()(elements);
}

std::string ByteList::toString() {
    Stringifier sf{ScalarTypeKind::BYTE};
    std::string buf = "[";
    bool first = true;
    for (auto&& element : elements) {
        if (first) { first = false; } else { buf += ", "; }
        buf += sf(element);
    }
    buf += "]";
    return buf;
}

bool ByteList::equals(Object *other) {
    if (this == other) return true;
    if (auto list = dynamic_cast<ByteList*>(other)) {
        return elements == list->elements;
    }
    return false;
}

size_t ByteList::hashCode() {
    std::basic_string_view sv{elements.begin(), elements.end()};
    return std::hash<std::string_view>()(reinterpret_cast<std::string_view&>(sv));
}

std::string ScalarList::toString() {
    Stringifier sf{type};
    std::string buf = "[";
    bool first = true;
    for (auto&& element : elements) {
        if (first) { first = false; } else { buf += ", "; }
        buf += sf(element);
    }
    buf += "]";
    return buf;
}

bool ScalarList::equals(Object *other) {
    if (this == other) return true;
    if (auto list = dynamic_cast<ScalarList*>(other)) {
        if (type != list->type) return false;
        Equator equator{type == ScalarTypeKind::FLOAT ? IdentityKind::FLOAT : IdentityKind::SELF};
        return std::equal(elements.begin(), elements.end(), list->elements.begin(), list->elements.end(), equator);
    }
    return false;
}

size_t ScalarList::hashCode() {
    Hasher hasher{type == ScalarTypeKind::FLOAT ? IdentityKind::FLOAT : IdentityKind::SELF};
    size_t hash = 0;
    for (auto&& element : elements) {
        hash += hasher(element);
    }
    return hash;
}

std::string Set::toString() {
    auto sf = stringifier(prototype->E);
    std::string buf = "@[";
    bool first = true;
    for (auto&& element : elements) {
        if (first) { first = false; } else { buf += ", "; }
        buf += sf(element);
    }
    buf += "]";
    return buf;
}

bool Set::equals(Object *other) {
    if (this == other) return true;
    if (auto set = dynamic_cast<Set*>(other)) {
        for (auto&& element : elements) {
            if (!set->elements.contains(element)) {
                return false;
            }
        }
        return true;
    }
    return false;
}

size_t Set::hashCode() {
    Hasher hasher{getIdentityKind(prototype->E)};
    size_t hash = 0;
    for (auto&& element : elements) {
        hash += hasher(element);
    }
    return hash;
}

std::string Dict::toString() {
    auto ksf = stringifier(prototype->K), vsf = stringifier(prototype->V);
    std::string buf = "@[";
    bool first = true;
    for (auto&& [key, value] : elements) {
        if (first) { first = false; } else { buf += ", "; }
        buf += ksf(key);
        buf += ": ";
        buf += vsf(value);
    }
    buf += "]";
    return buf;
}

bool Dict::equals(Object *other) {
    if (this == other) return true;
    if (auto dict = dynamic_cast<Dict*>(other)) {
        Equator valueequator{getIdentityKind(prototype->V)};
        for (auto&& [key, value] : elements) {
            auto it = dict->elements.find(key);
            if (it == dict->elements.end() || !valueequator(it->second, value)) {
                return false;
            }
        }
        return true;
    }
    return false;
}

size_t Dict::hashCode() {
    Hasher keyhasher{getIdentityKind(prototype->K)}, valuehasher{getIdentityKind(prototype->V)};
    size_t hash = 0;
    for (auto&& [key, value]: elements) {
        hash += (keyhasher(key) << 1) ^ valuehasher(value);
    }
    return hash;
}

std::string Stringifier::operator()($union value) const {
    switch (kind) {
        case ScalarTypeKind::NONE:
            return "()";
        case ScalarTypeKind::BOOL:
            return value.$bool ? "true" : "false";
        case ScalarTypeKind::BYTE: {
            char buf[8];
            sprintf(buf, "%hhX", value);
            return buf;
        }
        case ScalarTypeKind::INT:
            return std::to_string(value.$int);
        case ScalarTypeKind::FLOAT: {
            char buf[24];
            sprintf(buf, "%g", value);
            return buf;
        }
        case ScalarTypeKind::CHAR:
            return encodeUnicode(value.$char);
        default:
            return value.$object->toString();
    }
    unreachable();
}

bool ObjectList::ObjectListIterator::equals(Object *other) {
    if (this == other) return true;
    if (auto iter = dynamic_cast<ObjectList::ObjectListIterator*>(other)) {
        return list == iter->list && first == iter->first;
    }
    return false;
}

bool NoneList::NoneListIterator::equals(Object *other) {
    if (this == other) return true;
    if (auto iter = dynamic_cast<NoneList::NoneListIterator*>(other)) {
        return list == iter->list && i == iter->i;
    }
    return false;
}

bool BoolList::BoolListIterator::equals(Object *other) {
    if (this == other) return true;
    if (auto iter = dynamic_cast<BoolList::BoolListIterator*>(other)) {
        return list == iter->list && first == iter->first;
    }
    return false;
}

bool ByteList::ByteListIterator::equals(Object *other) {
    if (this == other) return true;
    if (auto iter = dynamic_cast<ByteList::ByteListIterator*>(other)) {
        return list == iter->list && first == iter->first;
    }
    return false;
}

bool ScalarList::ScalarListIterator::equals(Object *other) {
    if (this == other) return true;
    if (auto iter = dynamic_cast<ScalarList::ScalarListIterator*>(other)) {
        return list == iter->list && first == iter->first;
    }
    return false;
}

bool Set::SetIterator::equals(Object *other) {
    if (this == other) return true;
    if (auto iter = dynamic_cast<Set::SetIterator*>(other)) {
        return set == iter->set && first == iter->first;
    }
    return false;
}

bool Dict::DictIterator::equals(Object *other) {
    if (this == other) return true;
    if (auto iter = dynamic_cast<Dict::DictIterator*>(other)) {
        return dict == iter->dict && first == iter->first;
    }
    return false;
}

void Coroutine::walkMark() {
    if (!isValueBased(E) && cache.has_value() && cache->$object)
        cache->$object->mark();
    runtime->frame->markAll();
}

bool Coroutine::move() {
    if (runtime->code() != Opcode::RETURN) {
        ++runtime->pc;
        cache = runtime->loop();
        return runtime->code() != Opcode::RETURN;
    }
    return false;
}


}