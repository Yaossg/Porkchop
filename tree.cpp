#include "tree.hpp"
#include "literal.hpp"


namespace Porkchop {

TypeReference ConstExpr::evalType(ReferenceContext& context) const {
    switch (token.type) {
        case TokenType::KW_FALSE:
        case TokenType::KW_TRUE:
            return ScalarTypes::BOOL;
        case TokenType::CHARACTER_LITERAL:
            std::ignore = parseChar(*context.sourcecode, token);
            return ScalarTypes::CHAR;
        case TokenType::STRING_LITERAL:
            std::ignore = parseString(*context.sourcecode, token);
            return ScalarTypes::STRING;
        case TokenType::BINARY_INTEGER:
        case TokenType::OCTAL_INTEGER:
        case TokenType::DECIMAL_INTEGER:
        case TokenType::HEXADECIMAL_INTEGER:
            std::ignore = parseInt(*context.sourcecode, token);
        case TokenType::KW_LINE:
        case TokenType::KW_EOF:
            return ScalarTypes::INT;
        case TokenType::KW_NAN:
        case TokenType::KW_INF:
        case TokenType::FLOATING_POINT:
            std::ignore = parseFloat(*context.sourcecode, token);
            return ScalarTypes::FLOAT;
        default:
            unreachable("invalid token is classified as const");
    }
}

int64_t ConstExpr::evalConst(SourceCode& sourcecode) const {
    switch (token.type) {
        case TokenType::KW_FALSE:
            return 0;
        case TokenType::KW_TRUE:
            return 1;
        case TokenType::KW_LINE:
            return token.line;
        case TokenType::KW_EOF:
            return -1;
        case TokenType::BINARY_INTEGER:
        case TokenType::OCTAL_INTEGER:
        case TokenType::DECIMAL_INTEGER:
        case TokenType::HEXADECIMAL_INTEGER:
            return parseInt(sourcecode, token);
        default:
            return Expr::evalConst(sourcecode);
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

int64_t PrefixExpr::evalConst(SourceCode& sourcecode) const {
    switch (token.type) {
        case TokenType::OP_ADD:
            return rhs->evalConst(sourcecode);
        case TokenType::OP_SUB:
            return -rhs->evalConst(sourcecode);
        case TokenType::OP_NOT:
            return !rhs->evalConst(sourcecode);
        case TokenType::OP_INV:
            return ~rhs->evalConst(sourcecode);
        default:
            unreachable("invalid token is classified as prefix operator");
    }
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

int64_t nonneg(int64_t value, Segment segment) {
    if (value < 0) throw ConstException("non-negative constant is expected", segment);
    return value;
}
int64_t nonzero(int64_t value, Segment segment) {
    if (value == 0) throw ConstException("non-zero constant is expected", segment);
    return value;
}

int64_t InfixExpr::evalConst(SourceCode& sourcecode) const {
    switch (token.type) {
        case TokenType::OP_OR:
            return lhs->evalConst(sourcecode) | rhs->evalConst(sourcecode);
        case TokenType::OP_XOR:
            return lhs->evalConst(sourcecode) ^ rhs->evalConst(sourcecode);
        case TokenType::OP_AND:
            return lhs->evalConst(sourcecode) & rhs->evalConst(sourcecode);
        case TokenType::OP_EQ:
            return lhs->evalConst(sourcecode) == rhs->evalConst(sourcecode);
        case TokenType::OP_NEQ:
            return lhs->evalConst(sourcecode) != rhs->evalConst(sourcecode);
        case TokenType::OP_LT:
            return lhs->evalConst(sourcecode) < rhs->evalConst(sourcecode);
        case TokenType::OP_GT:
            return lhs->evalConst(sourcecode) > rhs->evalConst(sourcecode);
        case TokenType::OP_LE:
            return lhs->evalConst(sourcecode) <= rhs->evalConst(sourcecode);
        case TokenType::OP_GE:
            return lhs->evalConst(sourcecode) >= rhs->evalConst(sourcecode);
        case TokenType::OP_SHL:
            return lhs->evalConst(sourcecode) << nonneg(rhs->evalConst(sourcecode), rhs->segment());
        case TokenType::OP_SHR:
            return lhs->evalConst(sourcecode) >> nonneg(rhs->evalConst(sourcecode), rhs->segment());
        case TokenType::OP_USHR:
            return ssize_t(size_t(lhs->evalConst(sourcecode)) >> nonneg(rhs->evalConst(sourcecode), rhs->segment()));
        case TokenType::OP_ADD:
            return lhs->evalConst(sourcecode) + rhs->evalConst(sourcecode);
        case TokenType::OP_SUB:
            return lhs->evalConst(sourcecode) - rhs->evalConst(sourcecode);
        case TokenType::OP_MUL:
            return lhs->evalConst(sourcecode) * rhs->evalConst(sourcecode);
        case TokenType::OP_DIV:
            return lhs->evalConst(sourcecode) / nonzero(rhs->evalConst(sourcecode), rhs->segment());
        case TokenType::OP_REM:
            return lhs->evalConst(sourcecode) % nonzero(rhs->evalConst(sourcecode), rhs->segment());
        default:
            unreachable("invalid token is classified as infix operator");
    }
}

TypeReference AssignExpr::evalType(ReferenceContext& context) const {
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

TypeReference LogicalExpr::evalType(ReferenceContext& context) const {
    expected(lhs.get(), ScalarTypes::BOOL);
    expected(rhs.get(), ScalarTypes::BOOL);
    return ScalarTypes::BOOL;
}

int64_t LogicalExpr::evalConst(SourceCode& sourcecode) const {
    switch (token.type) {
        case TokenType::OP_LAND:
            return lhs->evalConst(sourcecode) && rhs->evalConst(sourcecode);
        case TokenType::OP_LOR:
            return lhs->evalConst(sourcecode) || rhs->evalConst(sourcecode);
        default:
            unreachable("invalid token is classified as logical operator");
    }
}

TypeReference AccessExpr::evalType(ReferenceContext& context) const {
    TypeReference type1 = lhs->typeCache, type2 = rhs->typeCache;
    if (isString(type1)) {
        expected(rhs.get(), ScalarTypes::INT);
        return ScalarTypes::CHAR;
    } else if (auto tuple = dynamic_cast<TupleType*>(type1.get())) {
        expected(rhs.get(), ScalarTypes::INT);
        auto index = rhs->evalConst(*context.sourcecode);
        if (0 <= index && index < tuple->E.size()) {
            return tuple->E[index];
        } else {
            throw TypeException("index out of bound", segment());
        }
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
                throw TypeException(unassignable(type, func->P[i]) + " at " + ordinal(i) + " parameter", rhs[i]->segment());
            }
        }
        return func->R;
    } else if (auto tuple = dynamic_cast<TupleType*>(lhs->typeCache.get())) {
        std::vector<FuncType*> candidates;
        for (auto&& e : tuple->E) {
            if (auto func0 = dynamic_cast<FuncType*>(e.get())) {
                if (std::equal(func0->P.begin(), func0->P.end(), rhs.begin(), rhs.end(), [](TypeReference const& type0, ExprHandle const& expr) {
                    return type0->assignableFrom(expr->typeCache);
                })) candidates.push_back(func0);
            } else {
                throw TypeException("all elements of tuple are required to be invocable for functional tuple", lhs->segment());
            }
        }
        switch (candidates.size()) {
            case 0:
                throw TypeException("none of overloaded candidates fits the parameters", lhs->segment());
            case 1:
                return candidates.front()->R;
            default:
                throw TypeException("multiple ambiguous overloaded candidates fit the parameters", lhs->segment());
        }
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
    throw TypeException("cannot cast the expression from " + type->toString() + " to " + T->toString(), segment());
}

int64_t AsExpr::evalConst(SourceCode& sourcecode) const {
    if (!isCompileTime(T)) throw ConstException("compile-time evaluation only support bool and int", segment());
    return lhs->evalConst(sourcecode);
}

TypeReference IsExpr::evalType(ReferenceContext& context) const {
    neverGonnaGiveYouUp(lhs.get(), "");
    return ScalarTypes::BOOL;
}

int64_t IsExpr::evalConst(SourceCode& sourcecode) const {
    if (isAny(lhs->typeCache)) throw ConstException("dynamic type cannot be checked at compile-time", lhs->segment());
    return lhs->typeCache->equals(T);
}

TypeReference DefaultExpr::evalType(ReferenceContext& context) const {
    neverGonnaGiveYouUp(T, "for it has no instance at all", segment());
    if (isAny(T)) throw TypeException("any has no default instance", segment());
    if (auto tuple = dynamic_cast<TupleType*>(T.get())) throw TypeException("tuple has no default instance", segment());
    if (auto func = dynamic_cast<FuncType*>(T.get())) throw TypeException("func has no default instance", segment());
    return T;
}

int64_t DefaultExpr::evalConst(SourceCode& sourcecode) const {
    if (!isCompileTime(T)) throw ConstException("compile-time evaluation only support bool and int", segment());
    return 0;
}

TypeReference TupleExpr::evalType(ReferenceContext &context) const {
    std::vector<TypeReference> E;
    for (auto&& expr: elements) {
        E.push_back(expr->typeCache);
    }
    return std::make_shared<TupleType>(std::move(E));
}

TypeReference ListExpr::evalType(ReferenceContext& context) const {
    auto type0 = elements.front()->typeCache;
    neverGonnaGiveYouUp(elements.front().get(), "as list element");
    for (size_t i = 1; i < elements.size(); ++i) {
        auto type = elements[i]->typeCache;
        if (!type0->equals(type)) throw TypeException(mismatch(type, "list's elements", i, type0), elements[i]->segment());
    }
    return listOf(type0);
}

TypeReference SetExpr::evalType(ReferenceContext &context) const {
    auto type0 = elements.front()->typeCache;
    neverGonnaGiveYouUp(elements.front().get(), "as set element");
    for (size_t i = 1; i < elements.size(); ++i) {
        auto type = elements[i]->typeCache;
        if (!type0->equals(type)) throw TypeException(mismatch(type, "set's elements", i, type0), elements[i]->segment());
    }
    return std::make_shared<SetType>(type0);
}

TypeReference DictExpr::evalType(ReferenceContext& context) const {
    auto key0 = elements.front().first->typeCache;
    auto value0 = elements.front().second->typeCache;
    neverGonnaGiveYouUp(elements.front().first.get(), "as dict key");
    neverGonnaGiveYouUp(elements.front().second.get(), "as dict value");
    for (size_t i = 1; i < elements.size(); ++i) {
        auto key = elements[i].first->typeCache;
        auto value = elements[i].second->typeCache;
        if (!key0->equals(key)) throw TypeException(mismatch(key, "dict's keys", i, key0), elements[i].first->segment());
        if (!value0->equals(value)) throw TypeException(mismatch(value, "dict's values", i, value0), elements[i].second->segment());
    }
    return std::make_shared<DictType>(std::move(key0), std::move(value0));
}

TypeReference ClauseExpr::evalType(ReferenceContext& context) const {
    for (auto&& expr : lines) if (isNever(expr->typeCache)) return ScalarTypes::NEVER;
    return lines.empty() ? ScalarTypes::NONE : lines.back()->typeCache;
}

int64_t ClauseExpr::evalConst(SourceCode& sourcecode) const {
    if (lines.empty()) throw ConstException("compile-time evaluation only support bool and int", segment());
    int64_t value;
    for (auto&& expr : lines) value = expr->evalConst(sourcecode);
    return value;
}

TypeReference IfElseExpr::evalType(ReferenceContext& context) const {
    if (isNever(cond->typeCache)) return ScalarTypes::NEVER;
    expected(cond.get(), ScalarTypes::BOOL);
    try {
        if (cond->evalConst(*context.sourcecode))
            return lhs->typeCache;
        else
            return rhs->typeCache;
    } catch (...) {}
    if (auto either = eithertype(lhs->typeCache, rhs->typeCache)) return either;
    throw TypeException(mismatch(rhs->typeCache, "both clause", lhs->typeCache), segment());
}

int64_t IfElseExpr::evalConst(SourceCode& sourcecode) const {
    return cond->evalConst(sourcecode) ? lhs->evalConst(sourcecode) : rhs->evalConst(sourcecode);
}

TypeReference BreakExpr::evalType(ReferenceContext& context) const {
    return ScalarTypes::NEVER;
}

TypeReference YieldExpr::evalType(ReferenceContext& context) const {
    neverGonnaGiveYouUp(rhs.get(), "");
    return rhs->typeCache;
}

TypeReference WhileExpr::evalType(ReferenceContext& context) const {
    if (isNever(cond->typeCache)) return ScalarTypes::NEVER;
    expected(cond.get(), ScalarTypes::BOOL);
    if (isNever(clause->typeCache)) return ScalarTypes::NEVER;
    try {
        if (cond->evalConst(*context.sourcecode) && hook->breaks.empty())
            return ScalarTypes::NEVER;
    } catch (...) {}
    return hook->yield();
}

TypeReference ForExpr::evalType(ReferenceContext& context) const {
    if (isNever(clause->typeCache)) return ScalarTypes::NEVER;
    return hook->yield();
}

TypeReference ForDestructuringExpr::evalType(ReferenceContext& context) const {
    if (isNever(clause->typeCache)) return ScalarTypes::NEVER;
    return hook->yield();
}

TypeReference TryExpr::evalType(ReferenceContext& context) const {
    if (auto either = eithertype(lhs->typeCache, rhs->typeCache)) return either;
    throw TypeException(mismatch(rhs->typeCache, "both clause", lhs->typeCache), segment());
}

TypeReference FnJumpExpr::evalType(ReferenceContext& context) const {
    neverGonnaGiveYouUp(rhs.get(), "");
    return ScalarTypes::NEVER;
}

TypeReference FnDeclExpr::evalType(ReferenceContext& context) const {
    return T;
}

TypeReference FnDefExpr::evalType(ReferenceContext& context) const {
    return T;
}

TypeReference LetExpr::evalType(ReferenceContext& context) const {
    return designated;
}

TypeReference LetDestructuringExpr::evalType(ReferenceContext& context) const {
    return std::make_shared<TupleType>(designated);
}

}
