#pragma once

#include <utility>
#include <vector>
#include <unordered_set>
#include <unordered_map>

#include "../type.hpp"

namespace Porkchop {

struct Object {
    friend struct VM;

    void mark() {
        if (marked) return;
        marked = true;
        walkMark();
    }

    virtual TypeReference getType() = 0;

    virtual ~Object() = default;

protected:
    bool marked = false;
    Object* nextObject = nullptr;
    VM* vm = nullptr;
    virtual void walkMark() = 0;
};

struct VM {
    struct Frame {
        VM* vm;
        std::vector<size_t> stack;
        std::vector<bool> companion;

        explicit Frame(VM* vm): vm(vm) {}

        void init(std::vector<size_t> const& captures, std::vector<bool> const& companion0) {
            stack = captures;
            companion = companion0;
        }

        void dup() {
            stack.push_back(stack.back());
            companion.push_back(companion.back());
        }

        void pop() {
            stack.pop_back();
            companion.pop_back();
        }

        void push(std::pair<size_t, bool> value) {
            stack.push_back(value.first);
            companion.push_back(value.second);
        }

        void push(size_t value) {
            stack.push_back(value);
            companion.push_back(false);
        }

        void push(Object* object) {
            stack.push_back(as_size(object));
            companion.push_back(true);
        }

        void load(size_t index) {
            stack.push_back(stack[index]);
            companion.push_back(companion[index]);
        }

        void store(size_t index) {
            stack[index] = stack.back();
        }

    };

    std::vector<std::shared_ptr<Frame>> frames;
    std::vector<Object*> temporaries;

    template<std::derived_from<Object> T, typename... Args>
        requires std::constructible_from<T, Args...>
    T* newObject(Args&&... args) {
        if (numObjects > maxObjects) gc();
        auto object = new T(std::forward<Args>(args)...);
        object->nextObject = firstObject;
        object->vm = this;
        firstObject = object;
        ++numObjects;
        return object;
    }

    void markAll() {
        for (auto&& frame : frames) {
            for (size_t i = 0; i < frame->stack.size(); ++i) {
                if (frame->companion[i])
                    std::bit_cast<Object*>(frame->stack[i])->mark();
            }
        }
        for (auto&& temporary : temporaries) {
            temporary->mark();
        }
    }

    void sweep() {
        Object** object = &firstObject;
        while (*object) {
            if ((*object)->marked) {
                (*object)->marked = false;
                object = &(*object)->nextObject;
            } else {
                Object* garbage = *object;
                *object = garbage->nextObject;
                delete garbage;
                --numObjects;
            }
        }
    }

    void gc() {
        markAll();
        sweep();
        maxObjects = std::max(numObjects * 2, 1024);
    }

    std::shared_ptr<Frame> newFrame() {
        frames.emplace_back(std::make_shared<Frame>(this));
        return frames.back();
    }

private:
    Object* firstObject = nullptr;
    int numObjects = 0;
    int maxObjects = 1024;
};


struct Func : Object {
    size_t func;
    TypeReference prototype;
    std::vector<size_t> captures;
    std::vector<bool> companion;
    bool companionR;

    Func(size_t func, TypeReference prototype): func(func), prototype(std::move(prototype)) {
        companionR = !isValueBased(dynamic_cast<FuncType*>(this->prototype.get())->R);
    }
    Func(const Func&) = default;

    Func* copy() {
        return vm->newObject<Func>(*this);
    }

    void bind(size_t capture, bool type) {
        captures.push_back(capture);
        companion.push_back(type);
    }

    void walkMark() override {
        for (size_t i = 0; i < captures.size(); ++i) {
            if (companion[i])
                std::bit_cast<Object*>(captures[i])->mark();
        }
    }

    TypeReference getType() override { return prototype; }

    std::pair<size_t, bool> call(Assembly *assembly, VM *vm) const;
};

struct AnyScalar : Object {
    size_t value;
    ScalarTypeKind type;

    AnyScalar(size_t value, ScalarTypeKind type): value(value), type(type) {}

    void walkMark() override {}

    TypeReference getType() override { return std::make_shared<ScalarType>(type); }
};

struct String : Object {
    std::string value;

    explicit String(std::string  value): value(std::move(value)) {}

    void walkMark() override {}

    TypeReference getType() override { return ScalarTypes::STRING; }
};

struct Tuple : Object {
    std::vector<size_t> elements;
    TypeReference prototype;
    TupleType* tuple;

    Tuple(std::vector<size_t> elements, TypeReference prototype): elements(std::move(elements)), prototype(std::move(prototype)) {
        tuple = dynamic_cast<TupleType*>(prototype.get());
    }

    void walkMark() override {
        for (size_t i = 0; i < elements.size(); ++i) {
            if (!isValueBased(tuple->E[i]))
                std::bit_cast<Object*>(elements[i])->mark();
        }
    }

    TypeReference getType() override { return prototype; }

};

struct Iterator : Object {
    TypeReference E;

    virtual bool peek() = 0;
    virtual size_t next() = 0;

    TypeReference getType() override { unreachable("iteration intrinsics"); }
};


struct Iterable : Object {
    virtual Iterator* iterator() = 0;
};

struct List : Iterable {
    std::vector<size_t> elements;
    TypeReference prototype;
    TypeReference E;

    List(std::vector<size_t> elements, TypeReference prototype): elements(std::move(elements)), prototype(std::move(prototype)) {
        E = dynamic_cast<ListType*>(this->prototype.get())->E;
    }

    void walkMark() override {
        if (isValueBased(E)) return;
        for (auto&& element : elements) {
            std::bit_cast<Object*>(element)->mark();
        }
    }

    TypeReference getType() override { return prototype; }


    struct ListIterator : Iterator {
        List* list;
        std::vector<size_t>::iterator first, last;

        explicit ListIterator(List* list): list(list), first(list->elements.begin()), last(list->elements.end()) {
            E = list->E;
        }

        void walkMark() override {
            list->mark();
        }

        bool peek() override {
            return first != last;
        }

        size_t next() override {
            return *first++;
        }

    };

    Iterator * iterator() override {
        return vm->newObject<ListIterator>(this);
    }
};

struct Set : Iterable {
    std::unordered_set<size_t> elements;
    TypeReference prototype;
    TypeReference E;

    Set(std::unordered_set<size_t> elements, TypeReference prototype): elements(std::move(elements)), prototype(std::move(prototype)) {
        E = dynamic_cast<ListType*>(this->prototype.get())->E;
    }

    void walkMark() override {
        if (isValueBased(E)) return;
        for (auto&& element : elements) {
            std::bit_cast<Object*>(element)->mark();
        }
    }

    TypeReference getType() override { return prototype; }


    struct SetIterator : Iterator {
        Set* set;
        std::unordered_set<size_t>::iterator first, last;

        explicit SetIterator(Set* set): set(set), first(set->elements.begin()), last(set->elements.end()) {
            E = set->E;
        }

        void walkMark() override {
            set->mark();
        }

        bool peek() override {
            return first != last;
        }

        size_t next() override {
            return *first++;
        }

    };

    Iterator * iterator() override {
        return vm->newObject<SetIterator>(this);
    }
};

struct Dict : Iterable {
    std::unordered_map<size_t, size_t> elements;
    TypeReference prototype;
    TypeReference K, V;

    Dict(std::unordered_map<size_t, size_t> elements, TypeReference prototype): elements(std::move(elements)), prototype(std::move(prototype)) {
        auto dict = dynamic_cast<DictType*>(this->prototype.get());
        K = dict->K;
        V = dict->V;
    }

    void walkMark() override {
        auto k = isValueBased(K);
        auto v = isValueBased(V);
        if (k && v) return;
        for (auto&& [key, value] : elements) {
             if (!k) {
                 std::bit_cast<Object*>(key)->mark();
             }
             if (!v) {
                 std::bit_cast<Object*>(value)->mark();
             }
        }
    }

    TypeReference getType() override { return prototype; }


    struct DictIterator : Iterator {
        Dict* dict;
        std::unordered_map<size_t, size_t>::iterator first, last;

        explicit DictIterator(Dict* dict): dict(dict), first(dict->elements.begin()), last(dict->elements.end()) {
            E = std::make_shared<TupleType>(std::vector{dict->K, dict->V});
        }

        void walkMark() override {
            dict->mark();
        }

        bool peek() override {
            return first != last;
        }

        size_t next() override {
            auto [key, value] = *first++;
            return as_size(vm->newObject<Tuple>(std::vector{key, value}, E));
        }

    };

    Iterator * iterator() override {
        return vm->newObject<DictIterator>(this);
    }

};


}