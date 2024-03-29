#pragma once

#include <utility>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <optional>
#include <algorithm>
#include <bitset>

#include "../type.hpp"


namespace Porkchop {

struct VM;
struct Frame;
struct Assembly;
struct List;

struct Hasher {
    IdentityKind kind;
    size_t operator()($union u) const;
};

struct Equator {
    IdentityKind kind;
    bool operator()($union u, $union v) const;
};

struct Stringifier {
    ScalarTypeKind kind;
    std::string operator()($union value) const;
};

inline Stringifier stringifier(TypeReference const& type) {
    return {isValueBased(type) ? dynamic_cast<ScalarType *>(type.get())->S : ScalarTypeKind::ANY};
}

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

struct Object {
    friend struct VM;

    void mark() {
        if (marked) return;
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
    std::vector<Frame*> frames;
    std::vector<Object*> temporaries;
    bool disableGC = false;

    struct ObjectHolder {
        Object* object;

        ObjectHolder(Object* object): object(object) {
            object->vm->temporaries.push_back(object);
        }

        ~ObjectHolder() {
            object->vm->temporaries.pop_back();
        }

        template<typename T>
        T* as() {
            return dynamic_cast<T*>(object);
        }
    };

    struct GCGuard {
        VM* vm;
        GCGuard(VM* vm): vm(vm) {
            vm->disableGC = true;
        }
        ~GCGuard() {
            vm->disableGC = false;
        }
    };

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

    void markAll();

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
        if (disableGC) return;
        markAll();
        sweep();
        maxObjects = std::max(numObjects * 2, 1024);
    }

    void init(int argi, int argc, const char *argv[]);

    FILE* out = stdout;
    FILE* in = stdin;
    bool disableIO = false;
    List* _args;

private:
    Object* firstObject = nullptr;
    int numObjects = 0;
    int maxObjects = 1024;
};

$union call(Assembly *assembly, VM *vm, size_t func, std::vector<$union> captures);

struct Func : Object {
    size_t func;
    std::shared_ptr<FuncType> prototype;
    std::vector<$union> captures;

    Func(size_t func, std::shared_ptr<FuncType> prototype, std::vector<$union> captures = {})
            : func(func), prototype(std::move(prototype)), captures(std::move(captures)) {}

    Func* bind(std::vector<$union> params) {
        auto P = prototype->P;
        P.erase(P.begin(), P.begin() + params.size());
        auto cap = captures;
        cap.insert(cap.end(), params.begin(), params.end());
        return vm->newObject<Func>(func, std::make_shared<FuncType>(std::move(P), prototype->R), std::move(cap));
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

    std::string toString() override;

    bool equals(Object *other) override;

    size_t hashCode() override;
};

struct AnyScalar : Object {
    $union value;
    ScalarTypeKind type;

    AnyScalar($union value, ScalarTypeKind type): value(value), type(type) {}

    TypeReference getType() override { return std::make_shared<ScalarType>(type); }

    std::string toString() override;

    bool equals(Object *other) override;

    size_t hashCode() override;
};

struct Sizeable {
    virtual size_t size() = 0;
};

struct String : Object, Sizeable {
    std::string value;

    explicit String(std::string value): value(std::move(value)) {}

    TypeReference getType() override { return ScalarTypes::STRING; }

    size_t size() override {
        return value.length();
    }

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

struct Iterator;

struct Iterable : Object {
    virtual Iterator* iterator() = 0;
};

struct Iterator : Iterable {
    TypeReference E;
    std::optional<$union> cache;

    virtual bool move() = 0;

    [[nodiscard]] $union get() const {
        if (!cache.has_value()) {
            throw Exception("iterator has no value to yield");
        }
        return cache.value();
    }

    TypeReference getType() override {
        return std::make_shared<IterType>(E);
    }

    Iterator * iterator() override {
        return this;
    }
};


struct Collection : Iterable, Sizeable {
    virtual void add($union element) = 0;
    virtual void remove($union element) = 0;
    virtual bool contains($union element) = 0;
};

struct List : Collection {
    virtual $union load(size_t index) = 0;
    virtual void store(size_t index, $union element) = 0;
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

    auto find($union element) {
        return std::find_if(elements.begin(), elements.end(),
                            [element]($union element0){ return element0.$object->equals(element.$object); });
    }

    bool contains($union element) override {
        return find(element) != elements.end();
    }

    void remove($union element) override {
        elements.erase(find(element));
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

        bool move() override {
            if (first != last) {
                cache = *first++;
                return true;
            }
            return false;
        }

        bool equals(Object *other) override;
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

    bool contains($union element) override {
        return true;
    }

    size_t size() override {
        return count;
    }

    struct NoneListIterator : Iterator {
        NoneList* list;
        size_t i;

        explicit NoneListIterator(NoneList* list): list(list), i(list->count + 1) {
            E = ScalarTypes::NONE;
            cache = nullptr;
        }

        void walkMark() override {
            list->mark();
        }

        bool move() override {
            return i && --i;
        }

        bool equals(Object *other) override;
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

    auto find($union element) {
        return std::find(elements.begin(), elements.end(), element.$bool);
    }

    void remove($union element) override {
        elements.erase(find(element));
    }

    bool contains($union element) override {
        return find(element) != elements.end();
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

        bool move() override {
            if (first != last) {
                cache = (bool) *first++;
                return true;
            }
            return false;
        }

        bool equals(Object *other) override;
    };

    Iterator * iterator() override {
        return vm->newObject<BoolListIterator>(this);
    }

    std::string toString() override;

    bool equals(Object *other) override;

    size_t hashCode() override;
};

struct ByteList : List {
    std::vector<uint8_t> elements;

    explicit ByteList(std::vector<uint8_t> elements): elements(std::move(elements)) {}

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

    auto find($union element) {
        return std::find(elements.begin(), elements.end(), element.$byte);
    }

    void remove($union element) override {
        elements.erase(find(element));
    }

    bool contains($union element) override {
        return find(element) != elements.end();
    }

    size_t size() override {
        return elements.size();
    }

    struct ByteListIterator : Iterator {
        ByteList* list;
        std::vector<uint8_t>::iterator first, last;

        explicit ByteListIterator(ByteList* list): list(list), first(list->elements.begin()), last(list->elements.end()) {
            E = ScalarTypes::BYTE;
        }

        void walkMark() override {
            list->mark();
        }

        bool move() override {
            if (first != last) {
                cache = *first++;
                return true;
            }
            return false;
        }

        bool equals(Object *other) override;
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
        elements[index] = element;
    }

    $union load(size_t index) override {
        return elements[index];
    }

    void add($union element) override {
        elements.push_back(element);
    }

    auto find($union element) {
        Equator equator{type == ScalarTypeKind::FLOAT ? IdentityKind::FLOAT : IdentityKind::SELF};
        return std::find_if(elements.begin(), elements.end(),
                            [element, equator]($union element0){ return equator(element0, element); });
    }

    void remove($union element) override {
        elements.erase(find(element));
    }

    bool contains($union element) override {
        return find(element) != elements.end();
    }

    size_t size() override {
        return elements.size();
    }

    struct ScalarListIterator : Iterator {
        ScalarList* list;
        std::vector<$union>::iterator first, last;

        explicit ScalarListIterator(ScalarList* list): list(list), first(list->elements.begin()), last(list->elements.end()) {
            E = std::make_shared<ScalarType>(list->type);
        }

        void walkMark() override {
            list->mark();
        }

        bool move() override {
            if (first != last) {
                cache = *first++;
                return true;
            }
            return false;
        }

        bool equals(Object *other) override;
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

    bool contains($union element) override {
        return elements.contains(element);
    }

    size_t size() override {
        return elements.size();
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

        bool move() override {
            if (first != last) {
                cache = *first++;
                return true;
            }
            return false;
        }

        bool equals(Object *other) override;
    };

    Iterator * iterator() override {
        return vm->newObject<SetIterator>(this);
    }

    std::string toString() override;

    bool equals(Object *other) override;

    size_t hashCode() override;
};

struct NoneSet : Collection {
    bool state;

    explicit NoneSet(bool state): state(state) {}

    TypeReference getType() override { return std::make_shared<SetType>(ScalarTypes::NONE); }

    void add($union element) override {
        state = true;
    }

    void remove($union element) override {
        state = false;
    }

    bool contains($union element) override {
        return state;
    }

    size_t size() override {
        return state;
    }

    struct NoneSetIterator : Iterator {
        NoneSet* set;

        explicit NoneSetIterator(NoneSet* set): set(set) {
            E = ScalarTypes::NONE;
        }

        void walkMark() override {
            set->mark();
        }

        bool move() override {
            if (!cache.has_value()) {
                cache = nullptr;
                return true;
            }
            return false;
        }

        bool equals(Object *other) override;
    };

    Iterator * iterator() override {
        return vm->newObject<NoneSetIterator>(this);
    }

    std::string toString() override;

    bool equals(Object *other) override;

    size_t hashCode() override;
};

struct BoolSet : Collection {
    bool falseState;
    bool trueState;

    BoolSet(): falseState(false), trueState(false) {}

    TypeReference getType() override { return std::make_shared<SetType>(ScalarTypes::BOOL); }

    void add($union element) override {
        (element.$bool ? trueState : falseState) = true;
    }

    void remove($union element) override {
        (element.$bool ? trueState : falseState) = false;
    }

    bool contains($union element) override {
        return element.$bool ? trueState : falseState;
    }

    size_t size() override {
        return falseState + trueState;
    }

    struct BoolSetIterator : Iterator {
        BoolSet* set;
        bool falseState;
        bool trueState;

        explicit BoolSetIterator(BoolSet* set): set(set), falseState(set->falseState), trueState(set->trueState) {
            E = ScalarTypes::BOOL;
        }

        void walkMark() override {
            set->mark();
        }

        bool move() override {
            if (falseState) {
                cache = false;
                falseState = false;
                return true;
            } else if (trueState) {
                cache = true;
                trueState = false;
                return true;
            }
            return false;
        }

        bool equals(Object *other) override;
    };

    Iterator * iterator() override {
        return vm->newObject<BoolSetIterator>(this);
    }

    std::string toString() override;

    bool equals(Object *other) override;

    size_t hashCode() override;
};

struct ByteSet : Collection {
    std::bitset<256> set;

    ByteSet() = default;

    TypeReference getType() override { return std::make_shared<SetType>(ScalarTypes::BYTE); }

    void add($union element) override {
        set.set(element.$byte);
    }

    void remove($union element) override {
        set.reset(element.$byte);
    }

    bool contains($union element) override {
        return set.test(element.$byte);
    }

    size_t size() override {
        return set.count();
    }

    struct ByteSetIterator : Iterator {
        ByteSet* set;
        size_t index;

        explicit ByteSetIterator(ByteSet* set): set(set), index(0) {
            E = ScalarTypes::BYTE;
        }

        void walkMark() override {
            set->mark();
        }

        bool move() override {
            for (; index < 256; ++index) {
                if (set->set.test(index)) {
                    cache = index++;
                    return true;
                }
            }
            return false;
        }

        bool equals(Object *other) override;
    };

    Iterator * iterator() override {
        return vm->newObject<ByteSetIterator>(this);
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

    bool contains($union element) override {
        return elements.contains(element);
    }

    size_t size() override {
        return elements.size();
    }

    struct DictIterator : Iterator {
        Dict* dict;
        underlying::iterator first, last;

        explicit DictIterator(Dict* dict): dict(dict), first(dict->elements.begin()), last(dict->elements.end()) {
            E = std::make_shared<TupleType>(std::vector{dict->prototype->K, dict->prototype->V});
        }

        void walkMark() override {
            dict->mark();
            if (cache.has_value())
                cache->$object->mark();
        }

        bool move() override {
            if (first != last) {
                auto [key, value] = *first++;
                cache = vm->newObject<Pair>(key, value, dict->prototype->K, dict->prototype->V);
                return true;
            }
            return false;
        }

        bool equals(Object *other) override;
    };

    Iterator * iterator() override {
        return vm->newObject<DictIterator>(this);
    }

    std::string toString() override;

    bool equals(Object *other) override;

    size_t hashCode() override;

};

struct Coroutine : Iterator {
    std::unique_ptr<Frame> frame;
    
    Coroutine(TypeReference R, std::unique_ptr<Frame> frame);

    void walkMark() override;

    bool move() override;
};

}
