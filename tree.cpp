#include "tree.hpp"


namespace Porkchop {

TypeReference ConstExpr::evalType(ReferenceContext& context) const {
    switch (token.type) {
        case TokenType::KW_FALSE:
        case TokenType::KW_TRUE:
            return ScalarTypes::BOOL;
        case TokenType::CHARACTER_LITERAL:
            return ScalarTypes::CHAR;
        case TokenType::STRING_LITERAL:
            return ScalarTypes::STRING;
        case TokenType::KW_LINE:
        case TokenType::KW_EOF:
        case TokenType::BINARY_INTEGER:
        case TokenType::OCTAL_INTEGER:
        case TokenType::DECIMAL_INTEGER:
        case TokenType::HEXADECIMAL_INTEGER:
            return ScalarTypes::INT;
        case TokenType::KW_NAN:
        case TokenType::KW_INF:
        case TokenType::DECIMAL_FLOAT:
        case TokenType::HEXADECIMAL_FLOAT:
            return ScalarTypes::FLOAT;
        default:
            unreachable("invalid token is classified as const");
    }
}

TypeReference PrefixExpr::evalType(ReferenceContext& context) const {
    switch (token.type) {
        case TokenType::OP_ADD:
        case TokenType::OP_SUB:
            expected(rhs.get(), isArithmetic, "arithmetic type");
            break;
        case TokenType::OP_NOT:
            expected(rhs.get(), ScalarTypes::BOOL);
            break;
        case TokenType::OP_INV:
            expected(rhs.get(), isIntegral, "integral type");
            break;
        default:
            unreachable("invalid token is classified as prefix operator");
    }
    return rhs->typeCache;
}

TypeReference InfixExpr::evalType(ReferenceContext& context) const {
    auto type1 = lhs->typeCache, type2 = rhs->typeCache;
    switch (token.type) {
        case TokenType::OP_OR:
        case TokenType::OP_XOR:
        case TokenType::OP_AND:
            expected(lhs.get(), isIntegral, "integral type");
            matchOnBothOperand(lhs.get(), rhs.get());
            return type1;
        case TokenType::OP_EQ:
        case TokenType::OP_NEQ:
        case TokenType::OP_LT:
        case TokenType::OP_GT:
        case TokenType::OP_LE:
        case TokenType::OP_GE:
            matchOnBothOperand(lhs.get(), rhs.get());
            return ScalarTypes::BOOL;
        case TokenType::OP_SHL:
        case TokenType::OP_SHR:
        case TokenType::OP_USHR:
            expected(lhs.get(), isIntegral, "integral type");
            expected(rhs.get(), ScalarTypes::INT);
            return type1;
        case TokenType::OP_ADD:
            if (isString(type1) && !isNever(type2) || isString(type2) && !isNever(type1)) return ScalarTypes::STRING;
        case TokenType::OP_SUB:
        case TokenType::OP_MUL:
        case TokenType::OP_DIV:
        case TokenType::OP_REM:
            expected(lhs.get(), isArithmetic, "arithmetic type");
            matchOnBothOperand(lhs.get(), rhs.get());
            return type1;
        default:
            unreachable("invalid token is classified as infix operator");
    }
}

TypeReference AssignExpr::evalType(ReferenceContext &context) const {
    auto type1 = lhs->typeCache, type2 = rhs->typeCache;
    switch (token.type) {
        case TokenType::OP_ASSIGN:
            assignable(rhs.get(), lhs->typeCache);
            return type1;
        case TokenType::OP_ASSIGN_AND:
        case TokenType::OP_ASSIGN_XOR:
        case TokenType::OP_ASSIGN_OR:
            expected(lhs.get(), isIntegral, "integral type");
            expected(rhs.get(), type1);
            return type1;
        case TokenType::OP_ASSIGN_SHL:
        case TokenType::OP_ASSIGN_SHR:
        case TokenType::OP_ASSIGN_USHR:
            expected(lhs.get(), isIntegral, "integral type");
            expected(rhs.get(), ScalarTypes::INT);
            return type1;
        case TokenType::OP_ASSIGN_ADD:
            if (isString(type1) || !isNever(type2)) return ScalarTypes::STRING;
        case TokenType::OP_ASSIGN_SUB:
        case TokenType::OP_ASSIGN_MUL:
        case TokenType::OP_ASSIGN_DIV:
        case TokenType::OP_ASSIGN_REM:
            expected(lhs.get(), isArithmetic, "arithmetic type");
            expected(rhs.get(), type1);
        default:
            unreachable("invalid token is classified as assignment operator");
    }
}

TypeReference LogicalExpr::evalType(ReferenceContext &context) const {
    expected(lhs.get(), ScalarTypes::BOOL);
    expected(rhs.get(), ScalarTypes::BOOL);
    return ScalarTypes::BOOL;
}

TypeReference AccessExpr::evalType(ReferenceContext& context) const {
    TypeReference type1 = lhs->typeCache, type2 = rhs->typeCache;
    if (isString(type1)) {
        expected(rhs.get(), ScalarTypes::INT);
        return ScalarTypes::CHAR;
    } else if (auto list = dynamic_cast<ListType*>(type1.get())) {
        expected(rhs.get(), ScalarTypes::INT);
        return list->E;
    } else if (auto dict = dynamic_cast<DictType*>(type1.get())) {
        assignable(rhs.get(), dict->K);
        return dict->V;
    }
    throw TypeException(unexpected(type1, "iterable type"), lhs->segment());
}

TypeReference InvokeExpr::evalType(ReferenceContext& context) const {
    if (auto func = dynamic_cast<FuncType*>(lhs->typeCache.get())) {
        if (rhs.size() != func->P.size()) {
            throw TypeException("expected " + std::to_string(func->P.size()) + " parameters but got " + std::to_string(rhs.size()), range(token1, token2));
        }
        for (size_t i = 0; i < rhs.size(); ++i) {
            if (auto type = rhs[i]->typeCache; !func->P[i]->assignableFrom(type)) {
                throw TypeException(unassignable(type, func->P[i]) + " at the " + std::to_string(i + 1) + "th parameter", rhs[i]->segment());
            }
        }
        return func->R;
    }
    throw TypeException(unexpected(lhs->typeCache, "invocable type"), lhs->segment());
}

TypeReference DotExpr::evalType(ReferenceContext& context) const {
    if (auto func = dynamic_cast<FuncType*>(rhs->typeCache.get())) {
        if (func->P.empty()) throw TypeException(unexpected(rhs->typeCache, "fn with at least one parameter"), rhs->segment());
        if (func->P[0]->assignableFrom(lhs->typeCache))
            return std::make_shared<FuncType>(std::vector<TypeReference>{std::next(func->P.begin()), func->P.end()}, func->R);
        throw TypeException(unassignable(lhs->typeCache, func->P[0]), rhs->segment());
    }
    throw TypeException(unexpected(rhs->typeCache, "invocable type"), rhs->segment());
}

TypeReference AsExpr::evalType(ReferenceContext& context) const {
    auto type = lhs->typeCache;
    if (T->assignableFrom(type) || isAny(type) && !isNever(T)
        || isSimilar(isArithmetic, type, T)
        || isSimilar(isIntegral, type, T)
        || isSimilar(isCharLike, type, T)
        || isSimilar(isCharListLike, type, T)
        || isSimilar(isIntOrByteList, type, T)
        || isSimilar(isFloatOrByteList, type, T)
        || isSimilar(isCharOrByteList, type, T)
        || isSimilar(isStringOrByteList, type, T)) return T;
    if (auto list1 = dynamic_cast<ListType*>(type.get()), list2 = dynamic_cast<ListType*>(T.get()); list1 && list2) {
        if (isAny(list2->E)) return T;
    } else if (auto dict1 = dynamic_cast<DictType*>(type.get()), dict2 = dynamic_cast<DictType*>(T.get()); dict1 && dict2) {
        if ((isAny(dict2->K) || dict2->K->equals(dict1->K))
            && (isAny(dict2->V) || dict2->V->equals(dict1->V))) return T;
    }
    throw TypeException("cannot cast the expression from " + type->toString() + " to " + T->toString(), segment());
}

TypeReference IsExpr::evalType(ReferenceContext &context) const {
    neverGonnaGiveYouUp(lhs.get(), "");
    return ScalarTypes::BOOL;
}

TypeReference DefaultExpr::evalType(ReferenceContext &context) const {
    neverGonnaGiveYouUp(T, "for it has no instance at all", segment());
    return T;
}

TypeReference ListExpr::evalType(ReferenceContext& context) const {
    if (rhs.empty()) return listOf(ScalarTypes::ANY);
    auto type0 = rhs.front()->typeCache;
    neverGonnaGiveYouUp(rhs.front().get(), "as list element");
    for (size_t i = 1; i < rhs.size(); ++i) {
        auto type = rhs[i]->typeCache;
        if (!type0->equals(type)) throw TypeException(mismatch(type, "list's elements", i, type0), rhs[i]->segment());
    }
    return listOf(type0);
}

TypeReference DictExpr::evalType(ReferenceContext& context) const {
    if (rhs.empty()) return listOf(ScalarTypes::ANY);
    auto key0 = rhs.front().first->typeCache;
    auto value0 = rhs.front().second->typeCache;
    neverGonnaGiveYouUp(rhs.front().first.get(), "as dict key");
    neverGonnaGiveYouUp(rhs.front().second.get(), "as dict value");
    for (size_t i = 1; i < rhs.size(); ++i) {
        auto key = rhs[i].first->typeCache;
        auto value = rhs[i].second->typeCache;
        if (!key0->equals(key)) throw TypeException(mismatch(key, "dict's keys", i, key0), rhs[i].first->segment());
        if (!value0->equals(value)) throw TypeException(mismatch(value, "dict's values", i, value0), rhs[i].second->segment());
    }
    return std::make_shared<DictType>(key0, value0);
}

TypeReference ClauseExpr::evalType(ReferenceContext &context) const {
    ReferenceContext::Guard guard(context);
    for (auto&& expr : rhs) if (isNever(expr->typeCache)) return ScalarTypes::NEVER;
    return rhs.empty() ? ScalarTypes::NONE : rhs.back()->typeCache;
}

TypeReference IfElseExpr::evalType(ReferenceContext &context) const {
    ReferenceContext::Guard guard(context);
    expected(cond.get(), ScalarTypes::BOOL);
    if (auto either = eithertype(lhs->typeCache, rhs->typeCache)) return either;
    throw TypeException(mismatch(rhs->typeCache, "both clause", lhs->typeCache), segment());
}

TypeReference BreakExpr::evalType(ReferenceContext &context) const {
    return ScalarTypes::NEVER;
}

TypeReference YieldExpr::evalType(ReferenceContext &context) const {
    neverGonnaGiveYouUp(rhs.get(), "");
    return rhs->typeCache;
}

TypeReference WhileExpr::evalType(ReferenceContext& context) const {
    ReferenceContext::Guard guard(context);
    auto type = cond->typeCache;
    if (isNever(type)) return ScalarTypes::NEVER;
    expected(cond.get(), ScalarTypes::BOOL);
    if (isNever(clause->typeCache)) return ScalarTypes::NEVER;
    return hook->yield();
}

TypeReference ForExpr::evalType(ReferenceContext& context) const {
    ReferenceContext::Guard guard(context);
    if (auto type = iterable(rhs->typeCache)) {
        context.local(context.sourcecode->source(lhs->token), type);
        if (isNever(clause->typeCache)) return ScalarTypes::NEVER;
        return hook->yield();
    }
    throw TypeException(unexpected(rhs->typeCache, "iterable type"), rhs->segment());
}

TypeReference TryExpr::evalType(ReferenceContext& context) const {
    TypeReference type1, type2;
    type1 = lhs->typeCache;
    ReferenceContext::Guard guard(context);
    context.local(context.sourcecode->source(except->token), ScalarTypes::ANY);
    type2 = rhs->typeCache;
    if (auto either = eithertype(type1, type2)) return either;
    throw TypeException(mismatch(type2, "both clause", type1), segment());
}

TypeReference FnJumpExpr::evalType(ReferenceContext &context) const {
    neverGonnaGiveYouUp(rhs.get(), "");
    return ScalarTypes::NEVER;
}

TypeReference FnExpr::evalType(ReferenceContext& context) const {
    TypeReference type0;
    if (returns.empty()) {
        type0 = clause->typeCache;
    } else {
        type0 = returns[0]->rhs->typeCache;
        for (size_t i = 1; i < returns.size(); ++i) {
            auto type = returns[i]->rhs->typeCache;
            if (!type0->equals(type)) throw TypeException(mismatch(type, "fn's returns", i, type0), returns[i]->segment());
        }
        if (auto type = clause->typeCache; !isNever(type) && !type0->equals(type)) {
            throw TypeException(mismatch(type, "fn's returns and expression body", type0), clause->segment());
        }
    }
    if (T->R == nullptr) T->R = type0;
    else if (!T->R->assignableFrom(type0)) throw TypeException(unassignable(type0, T->R), segment());
    return T;
}

TypeReference LetExpr::evalType(ReferenceContext& context) const {
    if (designated == nullptr) designated = rhs->typeCache;
    neverGonnaGiveYouUp(designated, "", segment());
    assignable(rhs.get(), designated);
    context.local(context.sourcecode->source(lhs->token), designated);
    return designated;
}

}
