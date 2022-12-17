#pragma once

#include <unordered_map>

#include "compiler.hpp"

namespace Porkchop {

enum class ScalarTypeKind {
    ANY,
    NONE,
    NEVER,
    BOOL,
    BYTE,
    INT,
    FLOAT,
    CHAR,
    STRING,
};

constexpr std::string_view SCALAR_TYPE_NAME[] = {
    "any",
    "none",
    "never",
    "bool",
    "byte",
    "int",
    "float",
    "char",
    "string"
};

constexpr std::string_view SCALAR_TYPE_DESC[] = {
    "a",
    "v",
    "n",
    "z",
    "b",
    "i",
    "f",
    "c",
    "s"
};

const std::unordered_map<std::string_view, ScalarTypeKind> SCALAR_TYPES {
    {"any",    ScalarTypeKind::ANY},
    {"none",   ScalarTypeKind::NONE},
    {"never",  ScalarTypeKind::NEVER},
    {"bool",   ScalarTypeKind::BOOL},
    {"byte",   ScalarTypeKind::BYTE},
    {"int",    ScalarTypeKind::INT},
    {"float",  ScalarTypeKind::FLOAT},
    {"char",   ScalarTypeKind::CHAR},
    {"string", ScalarTypeKind::STRING},
};

struct Type : Descriptor {
    [[nodiscard]] virtual std::string toString() const = 0;
    [[nodiscard]] virtual bool equals(const TypeReference& type) const noexcept = 0;
    [[nodiscard]] virtual bool assignableFrom(const TypeReference& type) const noexcept {
        return equals(type);
    }
    [[nodiscard]] virtual std::string serialize() const = 0;
};

[[nodiscard]] inline bool isNever(TypeReference const& type) noexcept;

struct ScalarType : Type {
    ScalarTypeKind S;

    explicit ScalarType(ScalarTypeKind S): S(S) {}

    [[nodiscard]] std::string toString() const override {
        return std::string{SCALAR_TYPE_NAME[(size_t) S]};
    }

    [[nodiscard]] bool equals(const TypeReference& type) const noexcept override {
        if (auto scalar = dynamic_cast<const ScalarType*>(type.get()))
            return scalar->S == S;
        return false;
    }

    [[nodiscard]] bool assignableFrom(const TypeReference& type) const noexcept override {
        switch (S) {
            case ScalarTypeKind::NEVER:
                return false;
            case ScalarTypeKind::NONE:
                return !isNever(type);
            default:
                return equals(type);
        }
    }

    [[nodiscard]] std::string_view descriptor() const noexcept override {
        return SCALAR_TYPE_NAME[(size_t) S];
    }

    [[nodiscard]] std::string serialize() const override {
        return std::string{SCALAR_TYPE_DESC[(size_t) S]};
    }
};

namespace ScalarTypes {
const TypeReference ANY = std::make_shared<ScalarType>(ScalarTypeKind::ANY);
const TypeReference NONE = std::make_shared<ScalarType>(ScalarTypeKind::NONE);
const TypeReference NEVER = std::make_shared<ScalarType>(ScalarTypeKind::NEVER);
const TypeReference BOOL = std::make_shared<ScalarType>(ScalarTypeKind::BOOL);
const TypeReference BYTE = std::make_shared<ScalarType>(ScalarTypeKind::BYTE);
const TypeReference INT = std::make_shared<ScalarType>(ScalarTypeKind::INT);
const TypeReference FLOAT = std::make_shared<ScalarType>(ScalarTypeKind::FLOAT);
const TypeReference CHAR = std::make_shared<ScalarType>(ScalarTypeKind::CHAR);
const TypeReference STRING = std::make_shared<ScalarType>(ScalarTypeKind::STRING);
}

[[nodiscard]] inline bool isScalar(TypeReference const& type, ScalarTypeKind kind) noexcept {
    if (auto scalar = dynamic_cast<ScalarType*>(type.get()))
        return scalar->S == kind;
    return false;
}

[[nodiscard]] inline bool isScalar(TypeReference const& type, bool pred(ScalarTypeKind) noexcept) noexcept {
    if (auto scalar = dynamic_cast<ScalarType*>(type.get()))
        return pred(scalar->S);
    return false;
}

[[nodiscard]] inline bool isAny(TypeReference const& type) noexcept {
    return isScalar(type, ScalarTypeKind::ANY);
}

[[nodiscard]] inline bool isNone(TypeReference const& type) noexcept {
    return isScalar(type, ScalarTypeKind::NONE);
}

[[nodiscard]] inline bool isNever(TypeReference const& type) noexcept {
    return isScalar(type, ScalarTypeKind::NEVER);
}

[[nodiscard]] inline bool isByte(TypeReference const& type) noexcept {
    return isScalar(type, ScalarTypeKind::BYTE);
}

[[nodiscard]] inline bool isInt(TypeReference const& type) noexcept {
    return isScalar(type, ScalarTypeKind::INT);
}

[[nodiscard]] inline bool isFloat(TypeReference const& type) noexcept {
    return isScalar(type, ScalarTypeKind::FLOAT);
}

[[nodiscard]] inline bool isChar(TypeReference const& type) noexcept {
    return isScalar(type, ScalarTypeKind::CHAR);
}

[[nodiscard]] inline bool isString(TypeReference const& type) noexcept {
    return isScalar(type, ScalarTypeKind::STRING);
}

[[nodiscard]] inline bool isSimilar(bool pred(TypeReference const&), TypeReference const& type1, TypeReference const& type2) noexcept {
    return pred(type1) && pred(type2);
}

[[nodiscard]] inline bool isArithmetic(TypeReference const& type) noexcept {
    return isScalar(type, [](ScalarTypeKind kind) noexcept { return kind == ScalarTypeKind::INT || kind == ScalarTypeKind::FLOAT; });
}

[[nodiscard]] inline bool isIntegral(TypeReference const& type) noexcept {
    return isScalar(type, [](ScalarTypeKind kind) noexcept { return kind == ScalarTypeKind::INT || kind == ScalarTypeKind::BYTE; });
}

[[nodiscard]] inline bool isCharLike(TypeReference const& type) noexcept {
    return isScalar(type, [](ScalarTypeKind kind) noexcept { return kind == ScalarTypeKind::INT || kind == ScalarTypeKind::CHAR; });
}

[[nodiscard]] inline bool isValueBased(TypeReference const& type) noexcept {
    return isScalar(type, [](ScalarTypeKind kind) noexcept {
        return kind == ScalarTypeKind::NONE || kind == ScalarTypeKind::BOOL || kind == ScalarTypeKind::BYTE
        || kind == ScalarTypeKind::CHAR || kind == ScalarTypeKind::INT || kind == ScalarTypeKind::FLOAT;
    });
}

struct TupleType : Type {
    std::vector<TypeReference> E;

    explicit TupleType(std::vector<TypeReference> E): E(std::move(E)) {}

    [[nodiscard]] std::string toString() const override {
        std::string buf = "(";
        bool first = true;
        for (auto&& p : E) {
            if (first) { first = false; } else { buf += ", "; }
            buf += p->toString();
        }
        buf += ")";
        return buf;
    }

    [[nodiscard]] bool equals(const TypeReference& type) const noexcept override {
        if (auto tuple = dynamic_cast<const TupleType*>(type.get())) {
            return std::equal(E.begin(), E.end(), tuple->E.begin(), tuple->E.end(),
                              [](const TypeReference& type1, const TypeReference& type2) { return type1->equals(type2); });
        }
        return false;
    }

    [[nodiscard]] bool assignableFrom(const TypeReference& type) const noexcept override {
        if (auto tuple = dynamic_cast<const TupleType*>(type.get())) {
            return std::equal(E.begin(), E.end(), tuple->E.begin(), tuple->E.end(),
                              [](const TypeReference& type1, const TypeReference& type2) { return type1->assignableFrom(type2); });
        }
        return false;
    }

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret;
        for (auto&& e : E) ret.push_back(e.get());
        return ret;
    }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "()"; }

    [[nodiscard]] std::string serialize() const override {
        std::string ret = "(";
        for (auto&& e : E) ret += e->serialize();
        return ret + ')';
    }
};

struct ListType : Type {
    TypeReference E;

    explicit ListType(TypeReference E): E(std::move(E)) {}

    [[nodiscard]] std::string toString() const override {
        return '[' + E->toString() + ']';
    }

    [[nodiscard]] bool equals(const TypeReference& type) const noexcept override {
        if (auto list = dynamic_cast<const ListType*>(type.get()))
            return list->E->equals(E);
        return false;
    }

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {E.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "[]"; }

    [[nodiscard]] std::string serialize() const override {
        return '[' + E->serialize();
    }
};

struct SetType : Type {
    TypeReference E;

    explicit SetType(TypeReference E): E(std::move(E)) {}

    [[nodiscard]] std::string toString() const override {
        return "@[" + E->toString() + ']';
    }

    [[nodiscard]] bool equals(const TypeReference& type) const noexcept override {
        if (auto list = dynamic_cast<const SetType*>(type.get()))
            return list->E->equals(E);
        return false;
    }

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {E.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "@[]"; }

    [[nodiscard]] std::string serialize() const override {
        return '{' + E->serialize();
    }
};

struct DictType : Type {
    TypeReference K, V;

    explicit DictType(TypeReference K, TypeReference V): K(std::move(K)), V(std::move(V)) {}

    [[nodiscard]] std::string toString() const override {
        return "@[" + K->toString() + ": " + V->toString() + ']';
    }

    [[nodiscard]] bool equals(const TypeReference& type) const noexcept override {
        if (auto dict = dynamic_cast<const DictType*>(type.get()))
            return dict->K->equals(K) && dict->V->equals(V);
        return false;
    }

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {K.get(), V.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "@[:]"; }

    [[nodiscard]] std::string serialize() const override {
        return '@' + K->serialize() + V->serialize();
    }
};

struct FuncType : Type {
    std::vector<TypeReference> P;
    TypeReference R;

    explicit FuncType(std::vector<TypeReference> P, TypeReference R): P(std::move(P)), R(std::move(R)) {}

    [[nodiscard]] std::string toString() const override {
        std::string buf = "(";
        bool first = true;
        for (auto&& p : P) {
            if (first) { first = false; } else { buf += ", "; }
            buf += p->toString();
        }
        buf += "): ";
        buf += R->toString();
        return buf;
    }

    [[nodiscard]] bool equals(const TypeReference& type) const noexcept override {
        if (auto func = dynamic_cast<const FuncType*>(type.get())) {
            return func->R->equals(R) &&
            std::equal(P.begin(), P.end(), func->P.begin(), func->P.end(),
                       [](const TypeReference& type1, const TypeReference& type2) { return type1->equals(type2); });
        }
        return false;
    }

    [[nodiscard]] bool assignableFrom(const TypeReference& type) const noexcept override {
        if (auto func = dynamic_cast<const FuncType*>(type.get())) {
            return (func->R->assignableFrom(R) || isNever(R) && isNever(func->R)) &&
            std::equal(P.begin(), P.end(), func->P.begin(), func->P.end(),
                       [](const TypeReference& type1, const TypeReference& type2) { return type1->assignableFrom(type2); });
        }
        return false;
    }

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret;
        for (auto&& e : P) ret.push_back(e.get());
        ret.push_back(R.get());
        return ret;
    }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "():"; }

    [[nodiscard]] std::string serialize() const override {
        std::string ret = "$";
        for (auto&& e : P) ret += e->serialize();
        return ret + ':' + R->serialize();
    }
};

[[nodiscard]] inline TypeReference eithertype(TypeReference const& type1, TypeReference const& type2) noexcept {
    if (type1->equals(type2)) return type1;
    if (isNever(type1)) return type2;
    if (isNever(type2)) return type1;
    if (isNone(type1) || isNone(type2)) return ScalarTypes::NONE;
    return nullptr;
}

[[nodiscard]] inline TypeReference elementof(TypeReference const& type) noexcept {
    if (auto set = dynamic_cast<SetType*>(type.get())) {
        return set->E;
    } else if (auto list = dynamic_cast<ListType*>(type.get())) {
        return list->E;
    } else if (auto dict = dynamic_cast<DictType*>(type.get())) {
        return std::make_shared<TupleType>(std::vector{dict->K, dict->V});
    } else {
        return nullptr;
    }
}

[[nodiscard]] inline size_t iterableID(TypeReference const& type) noexcept {
    if (dynamic_cast<SetType*>(type.get())) {
        return 0;
    } else if (dynamic_cast<ListType*>(type.get())) {
        return 1;
    } else if (dynamic_cast<DictType*>(type.get())) {
        return 2;
    } else {
        return -1;
    }
}

inline TypeReference deserialize(const char*& str) {
    switch (*str++) {
        case 'a': return ScalarTypes::ANY;
        case 'v': return ScalarTypes::NONE;
        case 'n': return ScalarTypes::NEVER;
        case 'z': return ScalarTypes::BOOL;
        case 'b': return ScalarTypes::BYTE;
        case 'i': return ScalarTypes::INT;
        case 'f': return ScalarTypes::FLOAT;
        case 'c': return ScalarTypes::CHAR;
        case 's': return ScalarTypes::STRING;
        case '(': {
            std::vector<TypeReference> elements;
            while (*str != ')') {
                elements.emplace_back(deserialize(str));
            }
            ++str;
            return std::make_shared<TupleType>(std::move(elements));
        }
        case '[': return std::make_shared<ListType>(deserialize(str));
        case '{': return std::make_shared<SetType>(deserialize(str));
        case '@': {
            auto key = deserialize(str);
            auto value = deserialize(str);
            return std::make_shared<DictType>(std::move(key), std::move(value));
        }
        case '$': {
            std::vector<TypeReference> parameters;
            while (*str != ':') {
                parameters.emplace_back(deserialize(str));
            }
            ++str;
            return std::make_shared<FuncType>(std::move(parameters), deserialize(str));
        }
    }
    std::string msg = "error to deserialize type at ";
    msg += --str;
    throw std::runtime_error(msg);
}

}