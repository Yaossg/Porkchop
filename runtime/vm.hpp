#pragma once

#include <utility>
#include <vector>
#include <unordered_set>
#include <unordered_map>

#include "../type.hpp"
#include "../util.hpp"


namespace Porkchop {

struct VM;
struct Assembly;

struct Object {
    friend struct VM;

    void mark() {
        if (this == nullptr || marked) return;
        marked = true;
        walkMark();
    }

    virtual ~Object() = default;

    virtual TypeReference getType() = 0;

    virtual std::string toString() {
        return join("(", getType()->toString(), ")@", std::to_string(hashCode()));
    }

    virtual bool equals(Object* other) {
        return this == other;
    }

    virtual size_t hashCode() {
        return std::hash<void*>()(this);
    }

protected:
    bool marked = false;
    Object* nextObject = nullptr;
    VM* vm = nullptr;
    virtual void walkMark() {}
};

struct VM {
    struct Frame {
        VM* vm;
        std::vector<$union> stack;
        std::vector<bool> companion;

        explicit Frame(VM* vm): vm(vm) {}

        void init(std::vector<$union> const& captures) {
            stack = captures;
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

        void pop() {
            stack.pop_back();
            companion.pop_back();
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
                if (frame->companion[i] && frame->stack[i].$object)
                    frame->stack[i].$object->mark();
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
    std::shared_ptr<FuncType> prototype;
    std::vector<$union> captures;

    Func(size_t func, std::shared_ptr<FuncType> prototype): func(func), prototype(std::move(prototype)) {}

    Func* bind(std::vector<$union> const& params) {
        auto P = prototype->P;
        P.erase(P.begin(), P.begin() + params.size());
        auto copy = vm->newObject<Func>(func, std::make_shared<FuncType>(std::move(P), prototype->R));
        copy->captures = captures;
        for (auto param : params) {
            copy->captures.push_back(param);
        }
        return copy;
    }

    void walkMark() override {
        for (size_t i = 0; i < captures.size(); ++i) {
            if (!isValueBased(prototype->P[i])) {
                captures[i].$object->mark();
            }
        }
    }

    TypeReference getType() override { return prototype; }

    $union call(Assembly *assembly, VM *vm) const;
};

enum class IdentityKind {
    SELF, FLOAT, OBJECT
};

struct Hasher {
    IdentityKind kind;
    size_t operator()($union u) const {
        switch (kind) {
            case IdentityKind::SELF:
                return u.$size;
            case IdentityKind::FLOAT:
                return std::hash<double>()(u.$float);
            case IdentityKind::OBJECT:
                return u.$object->hashCode();
        }
        unreachable();
    }
};

struct Equator {
    IdentityKind kind;
    bool operator()($union u, $union v) const {
        switch (kind) {
            case IdentityKind::SELF:
                return u.$size == v.$size;
            case IdentityKind::FLOAT:
                return u.$float == v.$float;
            case IdentityKind::OBJECT:
                return u.$object->equals(v.$object);
        }
        unreachable();
    }
};

inline IdentityKind getIdentityKind(TypeReference const& type) {
    if (isFloat(type)) return IdentityKind::FLOAT;
    if (isValueBased(type)) return IdentityKind::SELF;
    return IdentityKind::OBJECT;
}

struct Stringifier {
    ScalarTypeKind kind;
    std::string operator()($union value) const;
};

inline Stringifier stringifier(TypeReference const& type) {
    return {isValueBased(type) ? dynamic_cast<ScalarType *>(type.get())->S : ScalarTypeKind::ANY};
}

struct AnyScalar : Object {
    $union value;
    ScalarTypeKind type;

    AnyScalar($union value, ScalarTypeKind type): value(value), type(type) {}

    TypeReference getType() override { return std::make_shared<ScalarType>(type); }

    std::string toString() override;

    bool equals(Object *other) override;

    size_t hashCode() override;
};

struct String : Object {
    std::string value;

    explicit String(std::string value): value(std::move(value)) {}

    TypeReference getType() override { return ScalarTypes::STRING; }

    std::string toString() override;

    bool equals(Object *other) override;

    size_t hashCode() override;
};

struct Tuple : Object {
    virtual std::pair<$union, bool> load(size_t index) = 0;
};

struct Pair : Tuple {
    $union first, second;
    TypeReference T, U;
    IdentityKind t, u;
    Pair($union first, $union second, TypeReference T, TypeReference U)
        : first(first), second(second),
        T(std::move(T)), U(std::move(U)),
        t(getIdentityKind(this->T)), u(getIdentityKind(this->U)) {}

    void walkMark() override {
        if (t == IdentityKind::OBJECT) first.$object->mark();
        if (u == IdentityKind::OBJECT) second.$object->mark();
    }

    std::pair<$union, bool> load(size_t index) override {
        return index == 0 ? std::pair{first, t == IdentityKind::OBJECT} : std::pair{second, u == IdentityKind::OBJECT};
    }

    TypeReference getType() override { return std::make_shared<TupleType>(std::vector{T, U}); }

    std::string toString() override;

    bool equals(Object *other) override;

    size_t hashCode() override;
};

struct More : Tuple {
    std::vector<$union> elements;
    std::shared_ptr<TupleType> prototype;

    More(std::vector<$union> elements, std::shared_ptr<TupleType> prototype)
            : elements(std::move(elements)), prototype(std::move(prototype)) {}

    void walkMark() override {
        for (size_t i = 0; i < elements.size(); ++i) {
            if (!isValueBased(prototype->E[i])) {
                elements[i].$object->mark();
            }
        }
    }

    std::pair<$union, bool> load(size_t index) override {
        return {elements[index], !isValueBased(prototype->E[index])};
    }

    TypeReference getType() override { return prototype; }

    std::string toString() override;

    bool equals(Object *other) override;

    size_t hashCode() override;
};

struct Iterator : Object {
    TypeReference E;

    virtual bool peek() = 0;
    virtual $union next() = 0;

    TypeReference getType() override { __builtin_unreachable(); }
};

struct Iterable : Object {
    virtual Iterator* iterator() = 0;
};

struct Collection : Iterable {
    virtual void add($union element) = 0;
    virtual void remove($union element) = 0;
};

struct List : Collection {
    virtual $union load(size_t index) = 0;
    virtual void store(size_t index, $union element) = 0;
    virtual size_t size() = 0;
};

struct ObjectList : List {
    std::vector<$union> elements;
    std::shared_ptr<ListType> prototype;

    ObjectList(std::vector<$union> elements, std::shared_ptr<ListType> prototype)
        : elements(std::move(elements)), prototype(std::move(prototype)) {}

    void walkMark() override {
        for (auto&& element : elements) {
            element.$object->mark();
        }
    }

    TypeReference getType() override { return prototype; }

    $union load(size_t index) override {
        return elements[index];
    }

    void store(size_t index, $union element) override {
        elements[index] = element;
    }

    void add($union element) override {
        elements.push_back(element);
    }

    void remove($union element) override {
        elements.erase(std::find_if(elements.begin(), elements.end(),
                                    [element]($union element0){ return element0.$object->equals(element.$object); }));
    }

    size_t size() override {
        return elements.size();
    }

    struct ObjectListIterator : Iterator {
        ObjectList* list;
        std::vector<$union>::iterator first, last;

        explicit ObjectListIterator(ObjectList* list): list(list), first(list->elements.begin()), last(list->elements.end()) {
            E = list->prototype->E;
        }

        void walkMark() override {
            list->mark();
        }

        bool peek() override {
            return first != last;
        }

        $union next() override {
            return *first++;
        }
    };

    Iterator * iterator() override {
        return vm->newObject<ObjectListIterator>(this);
    }

    std::string toString() override;

    bool equals(Object *other) override;

    size_t hashCode() override;
};

struct NoneList : List {
    size_t count;

    explicit NoneList(size_t count): count(count) {}

    TypeReference getType() override { return std::make_shared<ListType>(ScalarTypes::NONE); }

    void store(size_t index, $union element) override {}

    $union load(size_t index) override {
        return nullptr;
    }

    void add($union element) override {
        ++count;
    }

    void remove($union element) override {
        --count;
    }

    size_t size() override {
        return count;
    }

    struct NoneListIterator : Iterator {
        NoneList* list;
        size_t countdown;

        explicit NoneListIterator(NoneList* list): list(list), countdown(list->count) {
            E = ScalarTypes::NONE;
        }

        void walkMark() override {
            list->mark();
        }

        bool peek() override {
            return countdown;
        }

        $union next() override {
            --countdown;
            return nullptr;
        }
    };

    Iterator * iterator() override {
        return vm->newObject<NoneListIterator>(this);
    }

    std::string toString() override;

    bool equals(Object *other) override;

    size_t hashCode() override;
};

struct BoolList : List {
    std::vector<bool> elements;

    explicit BoolList(std::vector<bool> elements): elements(std::move(elements)) {}

    TypeReference getType() override { return std::make_shared<ListType>(ScalarTypes::BOOL); }

    void store(size_t index, $union element) override {
        elements[index] = element.$bool;
    }

    $union load(size_t index) override {
        return (bool) elements[index];
    }

    void add($union element) override {
        elements.push_back(element.$bool);
    }

    void remove($union element) override {
        elements.erase(std::find(elements.begin(), elements.end(), element.$bool));
    }

    size_t size() override {
        return elements.size();
    }

    struct BoolListIterator : Iterator {
        BoolList* list;
        std::vector<bool>::iterator first, last;

        explicit BoolListIterator(BoolList* list): list(list), first(list->elements.begin()), last(list->elements.end()) {
            E = ScalarTypes::BOOL;
        }

        void walkMark() override {
            list->mark();
        }

        bool peek() override {
            return first != last;
        }

        $union next() override {
            return (bool) *first++;
        }
    };

    Iterator * iterator() override {
        return vm->newObject<BoolListIterator>(this);
    }

    std::string toString() override;

    bool equals(Object *other) override;

    size_t hashCode() override;
};

struct ByteList : List {
    std::vector<unsigned char> elements;

    explicit ByteList(std::vector<unsigned char> elements): elements(std::move(elements)) {}

    TypeReference getType() override { return std::make_shared<ListType>(ScalarTypes::BYTE); }

    void store(size_t index, $union element) override {
        elements[index] = element.$byte;
    }

    $union load(size_t index) override {
        return elements[index];
    }

    void add($union element) override {
        elements.push_back(element.$byte);
    }

    void remove($union element) override {
        elements.erase(std::find(elements.begin(), elements.end(), element.$byte));
    }

    size_t size() override {
        return elements.size();
    }

    struct ByteListIterator : Iterator {
        ByteList* list;
        std::vector<unsigned char>::iterator first, last;

        explicit ByteListIterator(ByteList* list): list(list), first(list->elements.begin()), last(list->elements.end()) {
            E = ScalarTypes::BYTE;
        }

        void walkMark() override {
            list->mark();
        }

        bool peek() override {
            return first != last;
        }

        $union next() override {
            return (bool) *first++;
        }
    };

    Iterator * iterator() override {
        return vm->newObject<ByteListIterator>(this);
    }

    std::string toString() override;

    bool equals(Object *other) override;

    size_t hashCode() override;
};

struct ScalarList : List {
    std::vector<$union> elements;
    ScalarTypeKind type;

    explicit ScalarList(std::vector<$union> elements, ScalarTypeKind type): elements(std::move(elements)), type(type) {}

    TypeReference getType() override { return std::make_shared<ListType>(std::make_shared<ScalarType>(type)); }

    void store(size_t index, $union element) override {
        elements[index] = element.$byte;
    }

    $union load(size_t index) override {
        return elements[index];
    }

    void add($union element) override {
        elements.push_back(element);
    }

    void remove($union element) override {
        Equator equator{type == ScalarTypeKind::FLOAT ? IdentityKind::FLOAT : IdentityKind::SELF};
        elements.erase(std::find_if(elements.begin(), elements.end(),
                                    [element, equator]($union element0){ return equator(element0, element); }));
    }

    size_t size() override {
        return elements.size();
    }

    struct ScalarListIterator : Iterator {
        ScalarList* list;
        std::vector<$union>::iterator first, last;

        explicit ScalarListIterator(ScalarList* list): list(list), first(list->elements.begin()), last(list->elements.end()) {
            E = ScalarTypes::BYTE;
        }

        void walkMark() override {
            list->mark();
        }

        bool peek() override {
            return first != last;
        }

        $union next() override {
            return *first++;
        }
    };

    Iterator * iterator() override {
        return vm->newObject<ScalarListIterator>(this);
    }

    std::string toString() override;

    bool equals(Object *other) override;

    size_t hashCode() override;
};

struct Set : Collection {
    using underlying = std::unordered_set<$union, Hasher, Equator>;
    underlying elements;
    std::shared_ptr<SetType> prototype;

    Set(underlying elements, std::shared_ptr<SetType> prototype)
        : elements(std::move(elements)), prototype(std::move(prototype)) {}

    void walkMark() override {
        if (isValueBased(prototype->E)) return;
        for (auto&& element : elements) {
            element.$object->mark();
        }
    }

    TypeReference getType() override { return prototype; }

    void add($union element) override {
        elements.insert(element);
    }

    void remove($union element) override {
        elements.erase(element);
    }

    struct SetIterator : Iterator {
        Set* set;
        underlying::iterator first, last;

        explicit SetIterator(Set* set): set(set), first(set->elements.begin()), last(set->elements.end()) {
            E = set->prototype->E;
        }

        void walkMark() override {
            set->mark();
        }

        bool peek() override {
            return first != last;
        }

        $union next() override {
            return *first++;
        }

    };

    Iterator * iterator() override {
        return vm->newObject<SetIterator>(this);
    }

    std::string toString() override;

    bool equals(Object *other) override;

    size_t hashCode() override;
};

struct Dict : Collection {
    using underlying = std::unordered_map<$union, $union, Hasher, Equator>;
    underlying elements;
    std::shared_ptr<DictType> prototype;

    Dict(underlying elements, std::shared_ptr<DictType> prototype)
        : elements(std::move(elements)), prototype(std::move(prototype)) {}

    void walkMark() override {
        auto k = isValueBased(prototype->K);
        auto v = isValueBased(prototype->V);
        if (k && v) return;
        for (auto&& [key, value] : elements) {
             if (!k) {
                 key.$object->mark();
             }
             if (!v) {
                 value.$object->mark();
             }
        }
    }

    TypeReference getType() override { return prototype; }

    void add($union element) override {
        auto pair = dynamic_cast<Pair*>(element.$object);
        elements.insert_or_assign(pair->first, pair->second);
    }

    void remove($union element) override {
        elements.erase(element);
    }

    struct DictIterator : Iterator {
        Dict* dict;
        underlying::iterator first, last;

        explicit DictIterator(Dict* dict): dict(dict), first(dict->elements.begin()), last(dict->elements.end()) {
            E = std::make_shared<TupleType>(std::vector{dict->prototype->K, dict->prototype->V});
        }

        void walkMark() override {
            dict->mark();
        }

        bool peek() override {
            return first != last;
        }

        $union next() override {
            auto [key, value] = *first++;
            return vm->newObject<Pair>(key, value, dict->prototype->K, dict->prototype->V);
        }

    };

    Iterator * iterator() override {
        return vm->newObject<DictIterator>(this);
    }

    std::string toString() override;

    bool equals(Object *other) override;

    size_t hashCode() override;

};


}
