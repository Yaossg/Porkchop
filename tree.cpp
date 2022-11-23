#include "tree.hpp"
#include "assembler.hpp"
#include "diagnostics.hpp"


namespace Porkchop {

int64_t parseInt(Compiler& compiler, Token token);
double parseFloat(Compiler& compiler, Token token);
char32_t parseChar(Compiler& compiler, Token token);
std::string parseString(Compiler& compiler, Token token);

int64_t Expr::evalConst(Compiler &compiler) const {
    throw ConstException("cannot evaluate at compile-time", segment());
}

TypeReference BoolConstExpr::evalType(LocalContext& context) const {
    parsed = token.type == TokenType::KW_TRUE;
    return ScalarTypes::BOOL;
}

int64_t BoolConstExpr::evalConst(Compiler& compiler) const {
    return parsed;
}

void BoolConstExpr::walkBytecode(Compiler& compiler, Assembler* assembler) const {
    assembler->const_(parsed);
}

TypeReference CharConstExpr::evalType(LocalContext& context) const {
    parsed = parseChar(*context.compiler, token);
    return ScalarTypes::CHAR;
}

void CharConstExpr::walkBytecode(Compiler& compiler, Assembler* assembler) const {
    assembler->const_((int64_t)parsed);
}

TypeReference StringConstExpr::evalType(LocalContext &context) const {
    parsed = parseString(*context.compiler, token);
    return ScalarTypes::STRING;
}

void StringConstExpr::walkBytecode(Compiler& compiler, Assembler* assembler) const {
    assembler->string(parsed);
}

TypeReference IntConstExpr::evalType(LocalContext &context) const {
    switch (token.type) {
        case TokenType::KW_LINE:
            parsed = int64_t(token.line);
            break;
        case TokenType::KW_EOF:
            parsed = -1;
            break;
        case TokenType::BINARY_INTEGER:
        case TokenType::OCTAL_INTEGER:
        case TokenType::DECIMAL_INTEGER:
        case TokenType::HEXADECIMAL_INTEGER:
            parsed = parseInt(*context.compiler, token);
            break;
        default:
            unreachable("invalid token is classified as prefix operator");
    }
    return ScalarTypes::INT;
}

int64_t IntConstExpr::evalConst(Compiler& compiler) const {
    return parsed;
}

void IntConstExpr::walkBytecode(Compiler& compiler, Assembler* assembler) const {
    assembler->const_(parsed);
}

TypeReference FloatConstExpr::evalType(LocalContext& context) const {
    parsed = parseFloat(*context.compiler, token);
    return ScalarTypes::FLOAT;
}

void FloatConstExpr::walkBytecode(Compiler& compiler, Assembler* assembler) const {
    assembler->const_(parsed);
}

TypeReference IdExpr::evalType(LocalContext& context) const {
    initLookup(context);
    return lookup.type;
}

void IdExpr::walkBytecode(Compiler& compiler, Assembler* assembler) const {
    if (lookup.function) {
        assembler->indexed(Opcode::FUNC, lookup.index);
    }  else if (compiler.of(token) == "_") {
        assembler->const0();
    } else {
        assembler->indexed(Opcode::LOAD, lookup.index);
    }
}

void IdExpr::walkStoreBytecode(Compiler &compiler, Assembler* assembler) const {
    if (lookup.function) {
        throw ParserException("function is not assignable", segment());
    } else if (compiler.of(token) == "_") {
        assembler->opcode(Opcode::POP);
        assembler->const0();
    } else {
        assembler->indexed(Opcode::STORE, lookup.index);
    }
}

TypeReference PrefixExpr::evalType(LocalContext& context) const {
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

int64_t PrefixExpr::evalConst(Compiler& compiler) const {
    switch (token.type) {
        case TokenType::OP_ADD:
            return rhs->evalConst(compiler);
        case TokenType::OP_SUB:
            return -rhs->evalConst(compiler);
        case TokenType::OP_NOT:
            return !rhs->evalConst(compiler);
        case TokenType::OP_INV:
            return ~rhs->evalConst(compiler);
        default:
            unreachable("invalid token is classified as prefix operator");
    }
}

void PrefixExpr::walkBytecode(Compiler &compiler, Assembler* assembler) const {
    rhs->walkBytecode(compiler, assembler);
    switch (token.type) {
        case TokenType::OP_ADD:
            break;
        case TokenType::OP_SUB:
            assembler->opcode(isIntegral(typeCache) ? Opcode::INEG : Opcode::FNEG);
            break;
        case TokenType::OP_NOT:
            assembler->opcode(Opcode::NOT);
            break;
        case TokenType::OP_INV:
            assembler->opcode(Opcode::INV);
            break;
        default:
            unreachable("invalid token is classified as prefix operator");
    }
}

TypeReference IdPrefixExpr::evalType(LocalContext& context) const {
    expected(rhs.get(), ScalarTypes::INT);
    return ScalarTypes::INT;
}

void IdPrefixExpr::walkBytecode(Compiler &compiler, Assembler* assembler) const {
    if (auto id = dynamic_cast<IdExpr*>(rhs.get())) {
        assembler->indexed(token.type == TokenType::OP_INC ? Opcode::INC : Opcode::DEC, id->lookup.index);
        id->walkBytecode(compiler, assembler);
    } else {
        rhs->walkBytecode(compiler, assembler);
        assembler->const1();
        assembler->opcode(token.type == TokenType::OP_INC ? Opcode::IADD : Opcode::ISUB);
        rhs->walkStoreBytecode(compiler, assembler);
    }
}

TypeReference IdPostfixExpr::evalType(LocalContext& context) const {
    expected(lhs.get(), ScalarTypes::INT);
    return ScalarTypes::INT;
}

void IdPostfixExpr::walkBytecode(Compiler &compiler, Assembler* assembler) const {
    if (auto id = dynamic_cast<IdExpr*>(lhs.get())) {
        id->walkBytecode(compiler, assembler);
        assembler->indexed(token.type == TokenType::OP_INC ? Opcode::INC : Opcode::DEC, id->lookup.index);
    } else {
        lhs->walkBytecode(compiler, assembler);
        assembler->opcode(Opcode::DUP);
        assembler->const1();
        assembler->opcode(token.type == TokenType::OP_INC ? Opcode::IADD : Opcode::ISUB);
        lhs->walkStoreBytecode(compiler, assembler);
        assembler->opcode(Opcode::POP);
    }
}

TypeReference InfixExpr::evalType(LocalContext& context) const {
    auto type1 = lhs->typeCache, type2 = rhs->typeCache;
    switch (token.type) {
        case TokenType::OP_OR:
        case TokenType::OP_XOR:
        case TokenType::OP_AND:
            expected(lhs.get(), isIntegral, "integral type");
            matchOnBothOperand(lhs.get(), rhs.get());
            return type1;
        case TokenType::OP_EQ:
        case TokenType::OP_NE:
        case TokenType::OP_LT:
        case TokenType::OP_GT:
        case TokenType::OP_LE:
        case TokenType::OP_GE:
            if (isString(lhs->typeCache) && isString(rhs->typeCache)) // TODO special message for string cmp
                return ScalarTypes::BOOL;
            expected(lhs.get(), isArithmetic, "arithmetic type"); // TODO more types to support
            matchOnBothOperand(lhs.get(), rhs.get());
            return ScalarTypes::BOOL;
        case TokenType::OP_SHL:
        case TokenType::OP_SHR:
        case TokenType::OP_USHR:
            expected(lhs.get(), isIntegral, "integral type");
            expected(rhs.get(), ScalarTypes::INT);
            return type1;
        case TokenType::OP_ADD:
            if (isString(lhs->typeCache) && isString(rhs->typeCache)) // TODO special message for string add
                return ScalarTypes::STRING;
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

int64_t InfixExpr::evalConst(Compiler& compiler) const {
    switch (token.type) {
        case TokenType::OP_OR:
            return lhs->evalConst(compiler) | rhs->evalConst(compiler);
        case TokenType::OP_XOR:
            return lhs->evalConst(compiler) ^ rhs->evalConst(compiler);
        case TokenType::OP_AND:
            return lhs->evalConst(compiler) & rhs->evalConst(compiler);
        case TokenType::OP_EQ:
            return lhs->evalConst(compiler) == rhs->evalConst(compiler);
        case TokenType::OP_NE:
            return lhs->evalConst(compiler) != rhs->evalConst(compiler);
        case TokenType::OP_LT:
            return lhs->evalConst(compiler) < rhs->evalConst(compiler);
        case TokenType::OP_GT:
            return lhs->evalConst(compiler) > rhs->evalConst(compiler);
        case TokenType::OP_LE:
            return lhs->evalConst(compiler) <= rhs->evalConst(compiler);
        case TokenType::OP_GE:
            return lhs->evalConst(compiler) >= rhs->evalConst(compiler);
        case TokenType::OP_SHL:
            return lhs->evalConst(compiler) << nonneg(rhs->evalConst(compiler), rhs->segment());
        case TokenType::OP_SHR:
            return lhs->evalConst(compiler) >> nonneg(rhs->evalConst(compiler), rhs->segment());
        case TokenType::OP_USHR:
            return int64_t(uint64_t(lhs->evalConst(compiler)) >> nonneg(rhs->evalConst(compiler), rhs->segment()));
        case TokenType::OP_ADD:
            return lhs->evalConst(compiler) + rhs->evalConst(compiler);
        case TokenType::OP_SUB:
            return lhs->evalConst(compiler) - rhs->evalConst(compiler);
        case TokenType::OP_MUL:
            return lhs->evalConst(compiler) * rhs->evalConst(compiler);
        case TokenType::OP_DIV:
            return lhs->evalConst(compiler) / nonzero(rhs->evalConst(compiler), rhs->segment());
        case TokenType::OP_REM:
            return lhs->evalConst(compiler) % nonzero(rhs->evalConst(compiler), rhs->segment());
        default:
            unreachable("invalid token is classified as infix operator");
    }
}

void InfixExpr::walkBytecode(Compiler &compiler, Assembler* assembler) const {
    lhs->walkBytecode(compiler, assembler);
    rhs->walkBytecode(compiler, assembler);
    bool i = isInt(lhs->typeCache);
    bool s = isString(lhs->typeCache);
    switch (token.type) {
        case TokenType::OP_OR:
            assembler->opcode(Opcode::OR);
            break;
        case TokenType::OP_XOR:
            assembler->opcode(Opcode::XOR);
            break;
        case TokenType::OP_AND:
            assembler->opcode(Opcode::AND);
            break;
        case TokenType::OP_SHL:
            assembler->opcode(Opcode::SHL);
            break;
        case TokenType::OP_SHR:
            assembler->opcode(Opcode::SHR);
            break;
        case TokenType::OP_USHR:
            assembler->opcode(Opcode::USHR);
            break;
        case TokenType::OP_EQ:
            assembler->opcode(s ? Opcode::SEQ : i ? Opcode::IEQ : Opcode::FEQ);
            break;
        case TokenType::OP_NE:
            assembler->opcode(s ? Opcode::SNE : i ? Opcode::INE : Opcode::FNE);
            break;
        case TokenType::OP_LT:
            assembler->opcode(s ? Opcode::SLT : i ? Opcode::ILT : Opcode::FLT);
            break;
        case TokenType::OP_GT:
            assembler->opcode(s ? Opcode::SGT : i ? Opcode::IGT : Opcode::FGT);
            break;
        case TokenType::OP_LE:
            assembler->opcode(s ? Opcode::SLE : i ? Opcode::ILE : Opcode::FLE);
            break;
        case TokenType::OP_GE:
            assembler->opcode(s ? Opcode::SGE : i ? Opcode::IGE: Opcode::FGE);
            break;
        case TokenType::OP_ADD:
            assembler->opcode(s ? Opcode::SADD : i ? Opcode::IADD : Opcode::FADD);
            break;
        case TokenType::OP_SUB:
            assembler->opcode(i ? Opcode::ISUB : Opcode::FSUB);
            break;
        case TokenType::OP_MUL:
            assembler->opcode(i ? Opcode::IMUL : Opcode::FMUL);
            break;
        case TokenType::OP_DIV:
            assembler->opcode(i ? Opcode::IDIV : Opcode::FDIV);
            break;
        case TokenType::OP_REM:
            assembler->opcode(i ? Opcode::IREM : Opcode::FREM);
            break;
        default:
            unreachable("invalid token is classified as infix operator");
    }
}

TypeReference AssignExpr::evalType(LocalContext& context) const {
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
            if (isString(lhs->typeCache) && isString(rhs->typeCache))
                return ScalarTypes::STRING;
        case TokenType::OP_ASSIGN_SUB:
        case TokenType::OP_ASSIGN_MUL:
        case TokenType::OP_ASSIGN_DIV:
        case TokenType::OP_ASSIGN_REM:
            expected(lhs.get(), isArithmetic, "arithmetic type");
            expected(rhs.get(), type1);
            return type1;
        default:
            unreachable("invalid token is classified as assignment operator");
    }
}

void AssignExpr::walkBytecode(Compiler &compiler, Assembler* assembler) const {
    if (token.type == TokenType::OP_ASSIGN) {
        rhs->walkBytecode(compiler, assembler);
        lhs->walkStoreBytecode(compiler, assembler);
    } else {
        bool i = isInt(lhs->typeCache);
        bool s = isString(lhs->typeCache);
        lhs->walkBytecode(compiler, assembler);
        rhs->walkBytecode(compiler, assembler);
        switch (token.type) {
            case TokenType::OP_ASSIGN_AND:
                assembler->opcode(Opcode::AND);
                break;
            case TokenType::OP_ASSIGN_XOR:
                assembler->opcode(Opcode::XOR);
                break;
            case TokenType::OP_ASSIGN_OR:
                assembler->opcode(Opcode::OR);
                break;
            case TokenType::OP_ASSIGN_SHL:
                assembler->opcode(Opcode::SHL);
                break;
            case TokenType::OP_ASSIGN_SHR:
                assembler->opcode(Opcode::SHR);
                break;
            case TokenType::OP_ASSIGN_USHR:
                assembler->opcode(Opcode::USHR);
                break;
            case TokenType::OP_ASSIGN_ADD:
                assembler->opcode(s ? Opcode::SADD : i ? Opcode::IADD : Opcode::FADD);
                break;
            case TokenType::OP_ASSIGN_SUB:
                assembler->opcode(i ? Opcode::ISUB: Opcode::FSUB);
                break;
            case TokenType::OP_ASSIGN_MUL:
                assembler->opcode(i ? Opcode::IMUL : Opcode::FMUL);
                break;
            case TokenType::OP_ASSIGN_DIV:
                assembler->opcode(i ? Opcode::IDIV : Opcode::FDIV);
                break;
            case TokenType::OP_ASSIGN_REM:
                assembler->opcode(i ? Opcode::IREM : Opcode::FREM);
                break;
            default:
                unreachable("invalid token is classified as compound assignment operator");
        }
        lhs->walkStoreBytecode(compiler, assembler);
    }
}

TypeReference LogicalExpr::evalType(LocalContext& context) const {
    expected(lhs.get(), ScalarTypes::BOOL);
    expected(rhs.get(), ScalarTypes::BOOL);
    return ScalarTypes::BOOL;
}

int64_t LogicalExpr::evalConst(Compiler& compiler) const {
    switch (token.type) {
        case TokenType::OP_LAND:
            return lhs->evalConst(compiler) && rhs->evalConst(compiler);
        case TokenType::OP_LOR:
            return lhs->evalConst(compiler) || rhs->evalConst(compiler);
        default:
            unreachable("invalid token is classified as logical operator");
    }
}

void LogicalExpr::walkBytecode(Compiler &compiler, Assembler* assembler) const {
    if (token.type == TokenType::OP_LAND) {
        BoolConstExpr zero({});
        zero.parsed = false;
        IfElseExpr::walkBytecode(lhs.get(), rhs.get(), &zero, compiler, assembler);
    } else {
        BoolConstExpr one({});
        one.parsed = true;
        IfElseExpr::walkBytecode(lhs.get(), &one, rhs.get(), compiler, assembler);
    }
}

TypeReference AccessExpr::evalType(LocalContext& context) const {
    TypeReference type1 = lhs->typeCache, type2 = rhs->typeCache;
    if (auto tuple = dynamic_cast<TupleType*>(type1.get())) {
        expected(rhs.get(), ScalarTypes::INT);
        auto index = rhs->evalConst(*context.compiler);
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

void AccessExpr::walkBytecode(Compiler &compiler, Assembler* assembler) const {
    lhs->walkBytecode(compiler, assembler);
    TypeReference type1 = lhs->typeCache;
    if (auto tuple = dynamic_cast<TupleType*>(type1.get())) {
        assembler->indexed(Opcode::TLOAD, rhs->evalConst(compiler));
    } else if (auto list = dynamic_cast<ListType*>(type1.get())) {
        rhs->walkBytecode(compiler, assembler);
        assembler->opcode(Opcode::LLOAD);
    } else if (auto dict = dynamic_cast<DictType*>(type1.get())) {
        rhs->walkBytecode(compiler, assembler);
        assembler->opcode(Opcode::DLOAD);
    } else {
        unreachable("invalid expression is classified as access operator");
    }
}

void AccessExpr::walkStoreBytecode(Compiler &compiler, Assembler* assembler) const {
    lhs->walkBytecode(compiler, assembler);
    TypeReference type1 = lhs->typeCache;
    if (auto tuple = dynamic_cast<TupleType*>(type1.get())) {
        assembler->indexed(Opcode::TSTORE, rhs->evalConst(compiler));
    } else if (auto list = dynamic_cast<ListType*>(type1.get())) {
        rhs->walkBytecode(compiler, assembler);
        assembler->opcode(Opcode::LSTORE);
    } else if (auto dict = dynamic_cast<DictType*>(type1.get())) {
        rhs->walkBytecode(compiler, assembler);
        assembler->opcode(Opcode::DSTORE);
    } else {
        unreachable("invalid expression is classified as access operator");
    }
}

TypeReference InvokeExpr::evalType(LocalContext& context) const {
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
    }
    throw TypeException(unexpected(lhs->typeCache, "invocable type"), lhs->segment());
}

void InvokeExpr::walkBytecode(Compiler &compiler, Assembler* assembler) const {
    lhs->walkBytecode(compiler, assembler);
    for (auto& e : rhs) {
        e->walkBytecode(compiler, assembler);
    }
    assembler->indexed(Opcode::BIND, rhs.size());
    assembler->opcode(Opcode::CALL);
}

TypeReference DotExpr::evalType(LocalContext& context) const {
    if (auto func = dynamic_cast<FuncType*>(rhs->typeCache.get())) {
        if (func->P.empty()) throw TypeException(unexpected(rhs->typeCache, "fn with at least one parameter"), rhs->segment());
        if (func->P[0]->assignableFrom(lhs->typeCache))
            return std::make_shared<FuncType>(std::vector<TypeReference>{std::next(func->P.begin()), func->P.end()}, func->R);
        throw TypeException(unassignable(lhs->typeCache, func->P[0]), rhs->segment());
    }
    throw TypeException(unexpected(rhs->typeCache, "invocable type"), rhs->segment());
}

void DotExpr::walkBytecode(Compiler &compiler, Assembler* assembler) const {
    rhs->walkBytecode(compiler, assembler);
    lhs->walkBytecode(compiler, assembler);
    assembler->indexed(Opcode::BIND, 1);
}

TypeReference AsExpr::evalType(LocalContext& context) const {
    auto type = lhs->typeCache;
    if (T->assignableFrom(type) || isAny(T) && !isNever(type) || isAny(type) && !isNever(T)
        || isSimilar(isArithmetic, type, T)
        || isSimilar(isIntegral, type, T)
        || isSimilar(isCharLike, type, T)) return T;
    throw TypeException("cannot cast the expression from " + type->toString() + " to " + T->toString(), segment());
}

int64_t AsExpr::evalConst(Compiler& compiler) const {
    if (!isCompileTime(T)) throw ConstException("compile-time evaluation only support bool and int", segment());
    return lhs->evalConst(compiler);
}

void AsExpr::walkBytecode(Compiler &compiler, Assembler* assembler) const {
    lhs->walkBytecode(compiler, assembler);
    if (lhs->typeCache->equals(T)) return;
    if (isAny(lhs->typeCache)) {
        assembler->typed(Opcode::AS, T);
    } else if (isAny(T)) {
        assembler->typed(Opcode::ANY, T);
    } else if (isNone(T)) {
        assembler->opcode(Opcode::POP);
        assembler->const0();
    } else if (isInt(lhs->typeCache)) {
        if (isByte(T)) {
            assembler->opcode(Opcode::I2B);
        } else if (isChar(T)) {
            assembler->opcode(Opcode::I2C);
        } else if (isFloat(T)) {
            assembler->opcode(Opcode::I2F);
        }
    } else if (isInt(T)) {
        if (isFloat(lhs->typeCache)) {
            assembler->opcode(Opcode::F2I);
        }
    }
}

TypeReference IsExpr::evalType(LocalContext& context) const {
    neverGonnaGiveYouUp(lhs.get(), "");
    return ScalarTypes::BOOL;
}

int64_t IsExpr::evalConst(Compiler& compiler) const {
    if (isAny(lhs->typeCache)) throw ConstException("dynamic type cannot be checked at compile-time", lhs->segment());
    return lhs->typeCache->equals(T);
}

void IsExpr::walkBytecode(Compiler &compiler, Assembler* assembler) const {
    if (isAny(lhs->typeCache)) {
        lhs->walkBytecode(compiler, assembler);
        assembler->typed(Opcode::IS, T);
    } else {
        assembler->const_(evalConst(compiler));
    }
}

TypeReference DefaultExpr::evalType(LocalContext& context) const {
    neverGonnaGiveYouUp(T, "for it has no instance at all", segment());
    if (isAny(T)) throw TypeException("any has no default instance", segment());
    if (auto tuple = dynamic_cast<TupleType*>(T.get())) throw TypeException("tuple has no default instance", segment());
    if (auto func = dynamic_cast<FuncType*>(T.get())) throw TypeException("func has no default instance", segment());
    return T;
}

int64_t DefaultExpr::evalConst(Compiler& compiler) const {
    if (!isCompileTime(T)) throw ConstException("compile-time evaluation only support bool and int", segment());
    return 0;
}

void DefaultExpr::walkBytecode(Compiler &compiler, Assembler* assembler) const {
    if (isValueBased(T)) {
        assembler->const0();
    } else if (isString(T)) {
        assembler->string("");
    } else if (auto list = dynamic_cast<ListType*>(T.get())) {
        assembler->indexed(Opcode::LIST, 0);
    } else if (auto set = dynamic_cast<SetType*>(T.get())) {
        assembler->indexed(Opcode::SET, 0);
    } else if (auto dict = dynamic_cast<DictType*>(T.get())) {
        assembler->indexed(Opcode::DICT, 0);
    }
}

TypeReference TupleExpr::evalType(LocalContext &context) const {
    std::vector<TypeReference> E;
    for (auto&& expr: elements) {
        E.push_back(expr->typeCache);
    }
    return std::make_shared<TupleType>(std::move(E));
}

void TupleExpr::walkBytecode(Compiler &compiler, Assembler* assembler) const {
    for (auto&& e : elements) {
        e->walkBytecode(compiler, assembler);
    }
    assembler->indexed(Opcode::TUPLE, elements.size());
}

TypeReference ListExpr::evalType(LocalContext& context) const {
    auto type0 = elements.front()->typeCache;
    neverGonnaGiveYouUp(elements.front().get(), "as list element");
    for (size_t i = 1; i < elements.size(); ++i) {
        auto type = elements[i]->typeCache;
        if (!type0->equals(type)) throw TypeException(mismatch(type, "list's elements", i, type0), elements[i]->segment());
    }
    return listOf(type0);
}

void ListExpr::walkBytecode(Compiler &compiler, Assembler* assembler) const {
    for (auto&& e : elements) {
        e->walkBytecode(compiler, assembler);
    }
    assembler->indexed(Opcode::LIST, elements.size());
}

TypeReference SetExpr::evalType(LocalContext &context) const {
    auto type0 = elements.front()->typeCache;
    neverGonnaGiveYouUp(elements.front().get(), "as set element");
    for (size_t i = 1; i < elements.size(); ++i) {
        auto type = elements[i]->typeCache;
        if (!type0->equals(type)) throw TypeException(mismatch(type, "set's elements", i, type0), elements[i]->segment());
    }
    return std::make_shared<SetType>(type0);
}

void SetExpr::walkBytecode(Compiler &compiler, Assembler* assembler) const {
    for (auto&& e : elements) {
        e->walkBytecode(compiler, assembler);
    }
    assembler->indexed(Opcode::SET, elements.size());
}

TypeReference DictExpr::evalType(LocalContext& context) const {
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

void DictExpr::walkBytecode(Compiler &compiler, Assembler* assembler) const {
    for (auto&& [e1, e2] : elements) {
        e1->walkBytecode(compiler, assembler);
        e2->walkBytecode(compiler, assembler);
    }
    assembler->indexed(Opcode::DICT, elements.size());
}

TypeReference ClauseExpr::evalType(LocalContext& context) const {
    for (auto&& expr : lines) if (isNever(expr->typeCache)) return ScalarTypes::NEVER;
    return lines.empty() ? ScalarTypes::NONE : lines.back()->typeCache;
}

int64_t ClauseExpr::evalConst(Compiler& compiler) const {
    if (lines.empty()) throw ConstException("compile-time evaluation only support bool and int", segment());
    int64_t value;
    for (auto&& expr : lines) value = expr->evalConst(compiler);
    return value;
}

void ClauseExpr::walkBytecode(Compiler &compiler, Assembler* assembler) const {
    if (lines.empty()) {
        assembler->const0();
    } else {
        bool first = true;
        for (auto&& line : lines) {
            if (first) { first = false; } else { assembler->opcode(Opcode::POP); }
            line->walkBytecode(compiler, assembler);
        }
    }
}

TypeReference IfElseExpr::evalType(LocalContext& context) const {
    if (isNever(cond->typeCache)) return ScalarTypes::NEVER;
    expected(cond.get(), ScalarTypes::BOOL);
    try {
        if (cond->evalConst(*context.compiler))
            return lhs->typeCache;
        else
            return rhs->typeCache;
    } catch (...) {}
    if (auto either = eithertype(lhs->typeCache, rhs->typeCache)) return either;
    throw TypeException(mismatch(rhs->typeCache, "both clause", lhs->typeCache), segment());
}

int64_t IfElseExpr::evalConst(Compiler& compiler) const {
    return cond->evalConst(compiler) ? lhs->evalConst(compiler) : rhs->evalConst(compiler);
}

void IfElseExpr::walkBytecode(Compiler &compiler, Assembler* assembler) const {
    walkBytecode(cond.get(), lhs.get(), rhs.get(), compiler, assembler);
}

void IfElseExpr::walkBytecode(Expr* cond, Expr* lhs, Expr* rhs, Compiler& compiler, Assembler* assembler) {
    size_t A = compiler.nextLabelIndex++;
    size_t B = compiler.nextLabelIndex++;
    cond->walkBytecode(compiler, assembler);
    assembler->labeled(Opcode::JMP0, A);
    lhs->walkBytecode(compiler, assembler);
    assembler->labeled(Opcode::JMP, B);
    assembler->label(A);
    rhs->walkBytecode(compiler, assembler);
    assembler->label(B);
}

TypeReference BreakExpr::evalType(LocalContext& context) const {
    return ScalarTypes::NEVER;
}

void BreakExpr::walkBytecode(Compiler &compiler, Assembler* assembler) const {
    size_t A = hook->loop->breakpoint;
    assembler->labeled(Opcode::JMP, A);
}

TypeReference YieldExpr::evalType(LocalContext& context) const {
    neverGonnaGiveYouUp(rhs.get(), "");
    return rhs->typeCache;
}

void YieldExpr::walkBytecode(Compiler &compiler, Assembler* assembler) const {
    throw ParserException("yield is not implemented yet", segment());
}

TypeReference WhileExpr::evalType(LocalContext& context) const {
    if (isNever(cond->typeCache)) return ScalarTypes::NEVER;
    expected(cond.get(), ScalarTypes::BOOL);
    if (isNever(clause->typeCache)) return ScalarTypes::NEVER;
    try {
        if (cond->evalConst(*context.compiler) && hook->breaks.empty())
            return ScalarTypes::NEVER;
    } catch (...) {}
    return ScalarTypes::NONE;
}

void WhileExpr::walkBytecode(Compiler &compiler, Assembler* assembler) const {
    size_t A = compiler.nextLabelIndex++;
    size_t B = compiler.nextLabelIndex++;
    breakpoint = B;
    assembler->label(A);
    cond->walkBytecode(compiler, assembler);
    assembler->labeled(Opcode::JMP0, B);
    clause->walkBytecode(compiler, assembler);
    assembler->opcode(Opcode::POP);
    assembler->labeled(Opcode::JMP, A);
    assembler->label(B);
    assembler->const0();
}

TypeReference ForExpr::evalType(LocalContext& context) const {
    if (isNever(clause->typeCache)) return ScalarTypes::NEVER;
    return ScalarTypes::NONE;
}

void ForExpr::walkBytecode(Compiler &compiler, Assembler* assembler) const {
    throw ParserException("for is not implemented yet", segment());
}

TypeReference ForDestructuringExpr::evalType(LocalContext& context) const {
    if (isNever(clause->typeCache)) return ScalarTypes::NEVER;
    return ScalarTypes::NONE;
}

void ForDestructuringExpr::walkBytecode(Compiler &compiler, Assembler* assembler) const {
    throw ParserException("for is not implemented yet", segment());
}

TypeReference ReturnExpr::evalType(LocalContext& context) const {
    neverGonnaGiveYouUp(rhs.get(), "");
    return ScalarTypes::NEVER;
}

void ReturnExpr::walkBytecode(Compiler &compiler, Assembler* assembler) const {
    rhs->walkBytecode(compiler, assembler);
    assembler->opcode(Opcode::RETURN);
}

TypeReference FnExprBase::evalType(LocalContext &context) const {
    return prototype;
}

void FnDeclExpr::walkBytecode(Compiler &compiler, Assembler* assembler) const {
    assembler->indexed(Opcode::FUNC, index);
}

void FnDefExpr::walkBytecode(Compiler &compiler, Assembler* assembler) const {
    assembler->indexed(Opcode::FUNC, index);
}

void LambdaExpr::walkBytecode(Compiler &compiler, Assembler* assembler) const {
    assembler->indexed(Opcode::FUNC, index);
    for (auto&& e : captures) {
        e->walkBytecode(compiler, assembler);
    }
    assembler->indexed(Opcode::BIND, captures.size());
}

TypeReference LetExpr::evalType(LocalContext& context) const {
    return designated;
}

void LetExpr::walkBytecode(Compiler &compiler, Assembler* assembler) const {
    rhs->walkBytecode(compiler, assembler);
    lhs->walkStoreBytecode(compiler, assembler);
}

TypeReference LetDestructuringExpr::evalType(LocalContext& context) const {
    return std::make_shared<TupleType>(designated);
}

void LetDestructuringExpr::walkBytecode(Compiler &compiler, Assembler* assembler) const {
    rhs->walkBytecode(compiler, assembler);
    for (size_t i = 0; i < lhs.size(); ++i) {
        assembler->opcode(Opcode::DUP);
        assembler->indexed(Opcode::TLOAD, i);
        lhs[i]->walkStoreBytecode(compiler, assembler);
        assembler->opcode(Opcode::POP);
    }
    assembler->opcode(Opcode::POP);
    for (auto&& e : lhs) {
        e->walkBytecode(compiler, assembler);
    }
    assembler->indexed(Opcode::TUPLE, lhs.size());
}

}
