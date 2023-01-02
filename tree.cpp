#include <cmath>
#include "tree.hpp"
#include "assembler.hpp"
#include "diagnostics.hpp"

namespace Porkchop {

$union Expr::evalConst() const {
    throw ConstException("cannot evaluate at compile-time", segment());
}

void Expr::expect(const TypeReference& expected) const {
    if (!typeCache->equals(expected)) {
        throw TypeException(join("expected '", expected->toString(), "' but got '", typeCache->toString(), "'"), segment());
    }
}

void Expr::expect(bool pred(const TypeReference&), const char* expected) const {
    if (!pred(typeCache)) {
        expect(expected);
    }
}

void Expr::expect(const char *expected) const {
    throw TypeException(join("expected ", expected, " but got '", typeCache->toString(), "'"), segment());
}

void Expr::match(const TypeReference &expected, const char *msg) const {
    if (!typeCache->equals(expected)) {
        throw TypeException(join("type mismatch on ", msg, ", the first one is '", expected->toString(),
                                 "', but this one is '", typeCache->toString(), "'"), segment());
    }
}

void Expr::assignable(const TypeReference &expected) const {
    Porkchop::assignable(typeCache, expected, segment());
}

void Expr::neverGonnaGiveYouUp(const char* msg) const {
    Porkchop::neverGonnaGiveYouUp(typeCache, msg, segment());
}

BoolConstExpr::BoolConstExpr(Compiler &compiler, Token token) : ConstExpr(compiler, token) {
    parsed = token.type == TokenType::KW_TRUE;
}

TypeReference BoolConstExpr::evalType() const {
    return ScalarTypes::BOOL;
}

$union BoolConstExpr::evalConst() const {
    return parsed;
}

void BoolConstExpr::walkBytecode(Assembler* assembler) const {
    assembler->const_(parsed);
}

CharConstExpr::CharConstExpr(Compiler &compiler, Token token) : ConstExpr(compiler, token) {
    char32_t parseChar(Compiler& compiler, Token token);
    parsed = parseChar(compiler, token);
}

TypeReference CharConstExpr::evalType() const {
    return ScalarTypes::CHAR;
}

void CharConstExpr::walkBytecode(Assembler* assembler) const {
    assembler->const_((int64_t)parsed);
}

$union CharConstExpr::evalConst() const {
    return parsed;
}

StringConstExpr::StringConstExpr(Compiler &compiler, Token token) : ConstExpr(compiler, token) {
    std::string parseString(Compiler& compiler, Token token);
    parsed = parseString(compiler, token);
}

TypeReference StringConstExpr::evalType() const {
    return ScalarTypes::STRING;
}

void StringConstExpr::walkBytecode(Assembler* assembler) const {
    assembler->sconst(parsed);
}

IntConstExpr::IntConstExpr(Compiler &compiler, Token token, bool merged) : ConstExpr(compiler, token), merged(merged) {
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
            int64_t parseInt(Compiler& compiler, Token token);
            parsed = parseInt(compiler, token);
            break;
        default:
            unreachable();
    }
}

TypeReference IntConstExpr::evalType() const {
    return ScalarTypes::INT;
}

$union IntConstExpr::evalConst() const {
    return parsed;
}

void IntConstExpr::walkBytecode(Assembler* assembler) const {
    assembler->const_(parsed);
}

FloatConstExpr::FloatConstExpr(Compiler &compiler, Token token) : ConstExpr(compiler, token) {
    double parseFloat(Compiler& compiler, Token token);
    parsed = parseFloat(compiler, token);
}

TypeReference FloatConstExpr::evalType() const {
    return ScalarTypes::FLOAT;
}

void FloatConstExpr::walkBytecode(Assembler* assembler) const {
    assembler->const_(parsed);
}

$union FloatConstExpr::evalConst() const {
    return parsed;
}

TypeReference IdExpr::evalType() const {
    return lookup.type;
}

void IdExpr::walkBytecode(Assembler* assembler) const {
    switch (lookup.scope) {
        case LocalContext::LookupResult::Scope::NONE:
            assembler->const0();
            break;
        case LocalContext::LookupResult::Scope::LOCAL:
            assembler->indexed(Opcode::LOAD , lookup.index);
            break;
        case LocalContext::LookupResult::Scope::FUNCTION:
            assembler->indexed(Opcode::FCONST, lookup.index);
            break;
    }
}

void IdExpr::walkStoreBytecode(Assembler* assembler) const {
    switch (lookup.scope) {
        case LocalContext::LookupResult::Scope::NONE:
            assembler->opcode(Opcode::POP);
            assembler->const0();
            break;
        case LocalContext::LookupResult::Scope::LOCAL:
            assembler->indexed(Opcode::STORE, lookup.index);
            break;
        default:
            unreachable();
    }
}

$union IdExpr::evalConst() const {
    if (compiler.of(token) == "_")
        return nullptr;
    return Expr::evalConst();
}

void IdExpr::ensureAssignable() const {
    if (lookup.scope == LocalContext::LookupResult::Scope::FUNCTION) {
        throw ParserException("function is not assignable", segment());
    }
}

TypeReference PrefixExpr::evalType() const {
    auto type = rhs->typeCache;
    switch (token.type) {
        case TokenType::OP_ADD:
        case TokenType::OP_SUB:
            rhs->expect(isArithmetic, "arithmetic type");
            break;
        case TokenType::OP_NOT:
            rhs->expect(ScalarTypes::BOOL);
            break;
        case TokenType::OP_INV:
            rhs->expect(isIntegral, "integral type");
            break;
        case TokenType::KW_SIZEOF:
            if (!isString(type)
                && !dynamic_cast<SetType*>(type.get())
                && !dynamic_cast<ListType*>(type.get())
                && !dynamic_cast<DictType*>(type.get())
                && !dynamic_cast<TupleType*>(type.get())) {
                rhs->expect("sizeable type");
            }
            return ScalarTypes::INT;
        case TokenType::OP_ATAT:
            rhs->neverGonnaGiveYouUp("to hash");
            return ScalarTypes::INT;
        case TokenType::OP_AND:
            if (auto element = elementof(type)) {
                return std::make_shared<IterType>(element);
            }
            rhs->expect("iterable type");
            break;
        case TokenType::OP_MUL:
            if (auto iter = dynamic_cast<IterType*>(type.get())) {
                return iter->E;
            }
            rhs->expect("iterator type");
            break;
        case TokenType::OP_SHR:
            if (auto iter = dynamic_cast<IterType*>(type.get())) {
                return ScalarTypes::BOOL;
            }
            rhs->expect("iterator type");
            break;
        default:
            unreachable();
    }
    return rhs->typeCache;
}

$union PrefixExpr::evalConst() const {
    auto type = rhs->typeCache;
    if (token.type == TokenType::KW_SIZEOF) {
        if (auto tuple = dynamic_cast<TupleType*>(type.get())) {
            return tuple->E.size();
        }
    }
    auto value = rhs->evalConst();
    switch (token.type) {
        case TokenType::OP_ADD:
            return value;
        case TokenType::OP_SUB:
            if (isInt(type)) {
                return -value.$int;
            } else {
                return -value.$float;
            }
        case TokenType::OP_NOT:
            return !value.$bool;
        case TokenType::OP_INV:
            if (isInt(type)) {
                return ~value.$int;
            } else {
                return (uint8_t)~value.$byte;
            }
        case TokenType::OP_ATAT:
            if (isFloat(type)) {
                return (int64_t) std::hash<double>()(value.$float);
            } else {
                return value;
            }
        default:
            return Expr::evalConst();
    }
}

void PrefixExpr::walkBytecode(Assembler* assembler) const {
    auto type = rhs->typeCache;
    if (token.type == TokenType::KW_SIZEOF) {
        if (auto tuple = dynamic_cast<TupleType*>(type.get())) {
            assembler->const_((int64_t) tuple->E.size());
        } else {
            rhs->walkBytecode(assembler);
            assembler->opcode(Opcode::SIZEOF);
        }
        return;
    }
    rhs->walkBytecode(assembler);
    switch (token.type) {
        case TokenType::OP_ADD:
            break;
        case TokenType::OP_SUB:
            assembler->opcode(isIntegral(type) ? Opcode::INEG : Opcode::FNEG);
            break;
        case TokenType::OP_NOT:
            assembler->opcode(Opcode::NOT);
            break;
        case TokenType::OP_INV:
            assembler->opcode(Opcode::INV);
            if (isByte(type)) assembler->opcode(Opcode::I2B);
            break;
        case TokenType::OP_ATAT:
            switch (getIdentityKind(type)) {
                case IdentityKind::FLOAT:
                    assembler->opcode(Opcode::FHASH);
                    break;
                case IdentityKind::OBJECT:
                    assembler->opcode(Opcode::OHASH);
                    break;
            }
            break;
        case TokenType::OP_AND:
            assembler->opcode(Opcode::ITER);
            break;
        case TokenType::OP_MUL:
            assembler->opcode(Opcode::GET);
            break;
        case TokenType::OP_SHR:
            assembler->opcode(Opcode::MOVE);
            break;
        default:
            unreachable();
    }
}

TypeReference StatefulPrefixExpr::evalType() const {
    rhs->ensureAssignable();
    rhs->expect(ScalarTypes::INT);
    return ScalarTypes::INT;
}

void StatefulPrefixExpr::walkBytecode(Assembler* assembler) const {
    if (auto id = dynamic_cast<IdExpr*>(rhs.get())) {
        assembler->indexed(token.type == TokenType::OP_INC ? Opcode::INC : Opcode::DEC, id->lookup.index);
        id->walkBytecode(assembler);
    } else {
        rhs->walkBytecode(assembler);
        assembler->const1();
        assembler->opcode(token.type == TokenType::OP_INC ? Opcode::IADD : Opcode::ISUB);
        rhs->walkStoreBytecode(assembler);
    }
}

TypeReference StatefulPostfixExpr::evalType() const {
    lhs->ensureAssignable();
    lhs->expect(ScalarTypes::INT);
    return ScalarTypes::INT;
}

void StatefulPostfixExpr::walkBytecode(Assembler* assembler) const {
    if (auto id = dynamic_cast<IdExpr*>(lhs.get())) {
        id->walkBytecode(assembler);
        assembler->indexed(token.type == TokenType::OP_INC ? Opcode::INC : Opcode::DEC, id->lookup.index);
    } else {
        lhs->walkBytecode(assembler);
        assembler->opcode(Opcode::DUP);
        assembler->const1();
        assembler->opcode(token.type == TokenType::OP_INC ? Opcode::IADD : Opcode::ISUB);
        lhs->walkStoreBytecode(assembler);
        assembler->opcode(Opcode::POP);
    }
}

TypeReference InfixExpr::evalType() const {
    auto type1 = lhs->typeCache, type2 = rhs->typeCache;
    switch (token.type) {
        case TokenType::OP_OR:
        case TokenType::OP_XOR:
        case TokenType::OP_AND:
            lhs->expect(isIntegral, "integral type");
            rhs->match(lhs->typeCache, "both operands");
            return type1;
        case TokenType::OP_SHL:
        case TokenType::OP_SHR:
        case TokenType::OP_USHR:
            lhs->expect(isIntegral, "integral type");
            rhs->expect(ScalarTypes::INT);
            return type1;
        case TokenType::OP_ADD:
            if (isString(type1) || isString(type2)) {
                lhs->neverGonnaGiveYouUp("toString");
                return ScalarTypes::STRING;
            }
            rhs->match(lhs->typeCache, "both operands");
            lhs->expect(isArithmetic, "arithmetic");
            return type1;
        case TokenType::OP_SUB:
        case TokenType::OP_MUL:
        case TokenType::OP_DIV:
        case TokenType::OP_REM:
            rhs->match(lhs->typeCache, "both operands");
            lhs->expect(isArithmetic, "arithmetic");
            return type1;
        default:
            unreachable();
    }
}

$union InfixExpr::evalConst() const {
    switch (token.type) {
        case TokenType::OP_OR:
            return lhs->evalConst().$size | rhs->evalConst().$size;
        case TokenType::OP_XOR:
            return lhs->evalConst().$size ^ rhs->evalConst().$size;
        case TokenType::OP_AND:
            return lhs->evalConst().$size & rhs->evalConst().$size;
        case TokenType::OP_SHL:
            if (isInt(lhs->typeCache)) {
                return lhs->evalConst().$int << rhs->evalConst().$int;
            } else {
                return (uint8_t)(lhs->evalConst().$byte << rhs->evalConst().$int);
            }
        case TokenType::OP_SHR:
            if (isInt(lhs->typeCache)) {
                return lhs->evalConst().$int >> rhs->evalConst().$int;
            } else {
                return (uint8_t)(lhs->evalConst().$byte >> rhs->evalConst().$int);
            }
        case TokenType::OP_USHR:
            if (isInt(lhs->typeCache)) {
                return lhs->evalConst().$size >> rhs->evalConst().$int;
            } else {
                return (uint8_t)(lhs->evalConst().$byte >> rhs->evalConst().$int);
            }
        case TokenType::OP_ADD:
            if (isInt(lhs->typeCache)) {
                return lhs->evalConst().$int + rhs->evalConst().$int;
            } else {
                return lhs->evalConst().$float + rhs->evalConst().$float;
            }
        case TokenType::OP_SUB:
            if (isInt(lhs->typeCache)) {
                return lhs->evalConst().$int - rhs->evalConst().$int;
            } else {
                return lhs->evalConst().$float - rhs->evalConst().$float;
            }
        case TokenType::OP_MUL:
            if (isInt(lhs->typeCache)) {
                return lhs->evalConst().$int * rhs->evalConst().$int;
            } else {
                return lhs->evalConst().$float * rhs->evalConst().$float;
            }
        case TokenType::OP_DIV:
            if (isInt(lhs->typeCache)) {
                int64_t divisor = rhs->evalConst().$int;
                if (divisor == 0) throw ConstException("divided by zero", segment());
                return lhs->evalConst().$int / divisor;
            } else {
                return lhs->evalConst().$float / rhs->evalConst().$float;
            }
        case TokenType::OP_REM:
            if (isInt(lhs->typeCache)) {
                int64_t divisor = rhs->evalConst().$int;
                if (divisor == 0) throw ConstException("divided by zero", segment());
                return lhs->evalConst().$int % divisor;
            } else {
                return std::fmod(lhs->evalConst().$float, rhs->evalConst().$float);
            }
        default:
            unreachable();
    }
}

void toStringBytecode(Assembler* assembler, TypeReference const& type) {
    if (auto scalar = dynamic_cast<ScalarType*>(type.get())) {
        switch (scalar->S) {
            case ScalarTypeKind::NONE:
                assembler->opcode(Opcode::POP);
                assembler->sconst("()");
                break;
            case ScalarTypeKind::BOOL:
                assembler->opcode(Opcode::Z2S);
                break;
            case ScalarTypeKind::BYTE:
                assembler->opcode(Opcode::B2S);
                break;
            case ScalarTypeKind::INT:
                assembler->opcode(Opcode::I2S);
                break;
            case ScalarTypeKind::FLOAT:
                assembler->opcode(Opcode::F2S);
                break;
            case ScalarTypeKind::CHAR:
                assembler->opcode(Opcode::C2S);
                break;
            case ScalarTypeKind::ANY:
                assembler->opcode(Opcode::O2S);
                break;
        }
    } else {
        assembler->opcode(Opcode::O2S);
    }
}

void InfixExpr::walkBytecode(Assembler* assembler) const {
    if (token.type == TokenType::OP_ADD && isString(typeCache)) {
        lhs->walkBytecode(assembler);
        toStringBytecode(assembler, lhs->typeCache);
        rhs->walkBytecode(assembler);
        toStringBytecode(assembler, rhs->typeCache);
        assembler->opcode(Opcode::SADD);
        return;
    }
    lhs->walkBytecode(assembler);
    rhs->walkBytecode(assembler);
    bool i = isInt(lhs->typeCache);
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
            if (isByte(lhs->typeCache)) assembler->opcode(Opcode::I2B);
            break;
        case TokenType::OP_SHR:
            assembler->opcode(Opcode::SHR);
            break;
        case TokenType::OP_USHR:
            assembler->opcode(Opcode::USHR);
            break;
        case TokenType::OP_ADD:
            assembler->opcode(i ? Opcode::IADD : Opcode::FADD);
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
            unreachable();
    }
}

TypeReference CompareExpr::evalType() const {
    rhs->match(lhs->typeCache, "both operands");
    auto type = lhs->typeCache;
    bool equality =token.type == TokenType::OP_EQ
            || token.type == TokenType::OP_NE
            || token.type == TokenType::OP_EQQ
            || token.type == TokenType::OP_NEQ;
    if (auto scalar = dynamic_cast<ScalarType*>(type.get())) {
        switch (scalar->S) {
            case ScalarTypeKind::ANY:
            case ScalarTypeKind::NONE:
                if (!equality) {
                    throw TypeException("none and any only support equality operators", segment());
                }
            case ScalarTypeKind::NEVER:
                lhs->neverGonnaGiveYouUp("in relational operations");
        }
    } else if (!equality) {
        throw TypeException("compound types only support equality operators", segment());
    }
    return ScalarTypes::BOOL;
}

$union CompareExpr::evalConst() const {
    auto type = lhs->typeCache;
    if (!isValueBased(type)) throw ConstException("constant comparison between unsupported types", segment());
    if (isNone(type)) return token.type == TokenType::OP_EQ || token.type == TokenType::OP_EQQ;
    auto value1 = lhs->evalConst();
    auto value2 = rhs->evalConst();
    std::partial_ordering cmp = value1.$size <=> value2.$size;
    if (isInt(type)) {
        cmp = value1.$int <=> value2.$int;
    } else if (isFloat(type)) {
        cmp = value1.$float <=> value2.$float;
    }
    switch (token.type) {
        case TokenType::OP_EQ:
        case TokenType::OP_EQQ:
            return cmp == 0;
        case TokenType::OP_NE:
        case TokenType::OP_NEQ:
            return cmp != 0;
        case TokenType::OP_LT:
            return cmp < 0;
        case TokenType::OP_GT:
            return cmp > 0;
        case TokenType::OP_LE:
            return cmp <= 0;
        case TokenType::OP_GE:
            return cmp >= 0;
        default:
            unreachable();
    }
}

void CompareExpr::walkBytecode(Assembler *assembler) const {
    auto type = lhs->typeCache;
    if (isNone(type)) {
        assembler->const_(token.type == TokenType::OP_EQ || token.type == TokenType::OP_EQQ);
        return;
    }
    lhs->walkBytecode(assembler);
    rhs->walkBytecode(assembler);
    if (token.type == TokenType::OP_EQQ || token.type == TokenType::OP_NEQ) {
        assembler->opcode(Opcode::UCMP);
    } else if (auto scalar = dynamic_cast<ScalarType*>(type.get())) {
        switch (scalar->S) {
            case ScalarTypeKind::BOOL:
            case ScalarTypeKind::BYTE:
            case ScalarTypeKind::CHAR:
                assembler->opcode(Opcode::UCMP);
                break;
            case ScalarTypeKind::INT:
                assembler->opcode(Opcode::ICMP);
                break;
            case ScalarTypeKind::FLOAT:
                assembler->opcode(Opcode::FCMP);
                break;
            case ScalarTypeKind::STRING:
                assembler->opcode(Opcode::SCMP);
                break;
            default:
                unreachable();
        }
    } else {
        assembler->opcode(Opcode::OCMP);
    }
    switch (token.type) {
        case TokenType::OP_EQ:
        case TokenType::OP_EQQ:
            assembler->opcode(Opcode::EQ);
            break;
        case TokenType::OP_NE:
        case TokenType::OP_NEQ:
            assembler->opcode(Opcode::NE);
            break;
        case TokenType::OP_LT:
            assembler->opcode(Opcode::LT);
            break;
        case TokenType::OP_GT:
            assembler->opcode(Opcode::GT);
            break;
        case TokenType::OP_LE:
            assembler->opcode(Opcode::LE);
            break;
        case TokenType::OP_GE:
            assembler->opcode(Opcode::GE);
            break;
        default:
            unreachable();
    }
}

TypeReference LogicalExpr::evalType() const {
    lhs->expect(ScalarTypes::BOOL);
    rhs->expect(ScalarTypes::BOOL);
    return ScalarTypes::BOOL;
}

$union LogicalExpr::evalConst() const {
    if (token.type == TokenType::OP_LAND) {
        return lhs->evalConst().$bool && rhs->evalConst().$bool;
    } else {
        return lhs->evalConst().$bool || rhs->evalConst().$bool;
    }
}

void LogicalExpr::walkBytecode(Assembler* assembler) const {
    if (token.type == TokenType::OP_LAND) {
        static const BoolConstExpr zero{compiler, {0, 0, 0, TokenType::KW_FALSE}};
        IfElseExpr::walkBytecode(lhs.get(), rhs.get(), &zero, compiler, assembler);
    } else {
        static const BoolConstExpr one{compiler, {0, 0, 0, TokenType::KW_TRUE}};
        IfElseExpr::walkBytecode(lhs.get(), &one, rhs.get(), compiler, assembler);
    }
}

TypeReference InExpr::evalType() const {
    if (auto element = elementof(rhs->typeCache, true)) {
        if (auto dict = dynamic_cast<DictType*>(rhs->typeCache.get())) {
            element = dict->K;
        }
        lhs->expect(element);
        return ScalarTypes::BOOL;
    }
    rhs->expect("iterable type");
}

void InExpr::walkBytecode(Assembler *assembler) const {
    lhs->walkBytecode(assembler);
    rhs->walkBytecode(assembler);
    assembler->opcode(Opcode::IN);
}

TypeReference AssignExpr::evalType() const {
    lhs->ensureAssignable();
    auto type1 = lhs->typeCache, type2 = rhs->typeCache;
    if (auto element = elementof(type1, true);
            element && (token.type == TokenType::OP_ASSIGN_ADD || token.type == TokenType::OP_ASSIGN_SUB)) {
        bool remove = token.type == TokenType::OP_ASSIGN_SUB;
        if (auto dict = dynamic_cast<DictType*>(type1.get()); dict && remove) {
            element = dict->K;
        }
        rhs->expect(element);
        return type1;
    }
    switch (token.type) {
        case TokenType::OP_ASSIGN:
            rhs->assignable(lhs->typeCache);
            return type1;
        case TokenType::OP_ASSIGN_AND:
        case TokenType::OP_ASSIGN_XOR:
        case TokenType::OP_ASSIGN_OR:
            lhs->expect(isIntegral, "integral type");
            rhs->expect(type1);
            return type1;
        case TokenType::OP_ASSIGN_SHL:
        case TokenType::OP_ASSIGN_SHR:
        case TokenType::OP_ASSIGN_USHR:
            lhs->expect(isIntegral, "integral type");
            rhs->expect(ScalarTypes::INT);
            return type1;
        case TokenType::OP_ASSIGN_ADD:
            if (isString(lhs->typeCache))
                return ScalarTypes::STRING;
        case TokenType::OP_ASSIGN_SUB:
        case TokenType::OP_ASSIGN_MUL:
        case TokenType::OP_ASSIGN_DIV:
        case TokenType::OP_ASSIGN_REM:
            lhs->expect(isArithmetic, "arithmetic type");
            rhs->expect(type1);
            return type1;
        default:
            unreachable();
    }
}

void AssignExpr::walkBytecode(Assembler* assembler) const {
    if (token.type == TokenType::OP_ASSIGN) {
        rhs->walkBytecode(assembler);
        lhs->walkStoreBytecode(assembler);
    } else if (auto element = elementof(lhs->typeCache, true);
        element && (token.type == TokenType::OP_ASSIGN_ADD || token.type == TokenType::OP_ASSIGN_SUB)) {
        lhs->walkBytecode(assembler);
        rhs->walkBytecode(assembler);
        assembler->opcode(token.type == TokenType::OP_ASSIGN_SUB ? Opcode::REMOVE : Opcode::ADD);
    } else {
        bool i = isInt(lhs->typeCache);
        lhs->walkBytecode(assembler);
        rhs->walkBytecode(assembler);
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
                if (isString(lhs->typeCache)) {
                    toStringBytecode(assembler, rhs->typeCache);
                    assembler->opcode(Opcode::SADD);
                } else {
                    assembler->opcode(i ? Opcode::IADD : Opcode::FADD);
                }
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
                unreachable();
        }
        lhs->walkStoreBytecode(assembler);
    }
}

TypeReference AccessExpr::evalType() const {
    TypeReference type1 = lhs->typeCache, type2 = rhs->typeCache;
    if (auto tuple = dynamic_cast<TupleType*>(type1.get())) {
        rhs->expect(ScalarTypes::INT);
        int64_t index = rhs->evalConst().$int;
        if (0 <= index && index < tuple->E.size()) {
            return tuple->E[index];
        } else {
            throw TypeException("index out of bound", segment());
        }
    } else if (auto list = dynamic_cast<ListType*>(type1.get())) {
        rhs->expect(ScalarTypes::INT);
        return list->E;
    } else if (auto dict = dynamic_cast<DictType*>(type1.get())) {
        rhs->expect(dict->K);
        return dict->V;
    }
    lhs->expect("indexable type");
}

void AccessExpr::walkBytecode(Assembler* assembler) const {
    lhs->walkBytecode(assembler);
    TypeReference type1 = lhs->typeCache;
    if (auto tuple = dynamic_cast<TupleType*>(type1.get())) {
        assembler->indexed(Opcode::TLOAD, rhs->evalConst().$int);
    } else if (auto list = dynamic_cast<ListType*>(type1.get())) {
        rhs->walkBytecode(assembler);
        assembler->opcode(Opcode::LLOAD);
    } else if (auto dict = dynamic_cast<DictType*>(type1.get())) {
        rhs->walkBytecode(assembler);
        assembler->opcode(Opcode::DLOAD);
    } else {
        unreachable();
    }
}

void AccessExpr::walkStoreBytecode(Assembler* assembler) const {
    lhs->walkBytecode(assembler);
    TypeReference type1 = lhs->typeCache;
    if (auto list = dynamic_cast<ListType*>(type1.get())) {
        rhs->walkBytecode(assembler);
        assembler->opcode(Opcode::LSTORE);
    } else if (auto dict = dynamic_cast<DictType*>(type1.get())) {
        rhs->walkBytecode(assembler);
        assembler->opcode(Opcode::DSTORE);
    } else {
        unreachable();
    }
}

void AccessExpr::ensureAssignable() const {
    if (auto tuple = dynamic_cast<TupleType*>(lhs->typeCache.get())) {
        throw ParserException("tuple is immutable and its elements are not assignable", segment());
    }
}

TypeReference InvokeExpr::evalType() const {
    if (auto func = dynamic_cast<FuncType*>(lhs->typeCache.get())) {
        if (rhs.size() != func->P.size()) {
            throw TypeException(join("expected ", std::to_string(func->P.size()), " parameters but got ", std::to_string(rhs.size())),
                                range(token1, token2));
        }
        for (size_t i = 0; i < rhs.size(); ++i) {
            rhs[i]->assignable(func->P[i]);
        }
        return func->R;
    }
    lhs->expect("invocable type");
}

void InvokeExpr::walkBytecode(Assembler* assembler) const {
    lhs->walkBytecode(assembler);
    for (auto& e : rhs) {
        e->walkBytecode(assembler);
    }
    if (!rhs.empty())
        assembler->indexed(Opcode::BIND, rhs.size());
    assembler->opcode(Opcode::CALL);
}

TypeReference DotExpr::evalType() const {
    if (auto func = dynamic_cast<FuncType*>(rhs->typeCache.get())) {
        if (func->P.empty()) {
            rhs->expect("function with at least one parameter");
        }
        lhs->assignable(func->P[0]);
        return std::make_shared<FuncType>(std::vector<TypeReference>{std::next(func->P.begin()), func->P.end()},func->R);
    }
    rhs->expect("invocable type");
}

void DotExpr::walkBytecode(Assembler* assembler) const {
    rhs->walkBytecode(assembler);
    lhs->walkBytecode(assembler);
    assembler->indexed(Opcode::BIND, 1);
}

TypeReference AsExpr::evalType() const {
    auto type = lhs->typeCache;
    if (T->assignableFrom(type) || isAny(T) && !isNever(type) || isAny(type) && !isNever(T)
        || isSimilar(isArithmetic, type, T)
        || isSimilar(isIntegral, type, T)
        || isSimilar(isCharLike, type, T)) return T;
    throw TypeException(join("cannot cast the expression from '", type->toString(), "' to '", T->toString(), "'"), segment());
}

$union AsExpr::evalConst() const {
    if (!isValueBased(T)) throw ConstException("unsupported type for constant evaluation", segment());
    auto value = lhs->evalConst();
    if (isInt(lhs->typeCache)) {
        if (isByte(T)) {
            return value.$byte;
        } else if (isChar(T)) {
            if (isInvalidChar(value.$int))
                throw ConstException("int is invalid to cast to char", segment());
            return value.$char;
        } else if (isFloat(T)) {
            return (double)value.$int;
        }
    } else if (isInt(T)) {
        if (isFloat(lhs->typeCache)) {
            return (int64_t)value.$float;
        }
    }
    return value;
}

void AsExpr::walkBytecode(Assembler* assembler) const {
    lhs->walkBytecode(assembler);
    if (lhs->typeCache->equals(T)) return;
    if (isAny(lhs->typeCache)) {
        assembler->typed(Opcode::AS, T);
    } else if (isAny(T)) {
        assembler->typed(Opcode::ANY, lhs->typeCache);
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

TypeReference IsExpr::evalType() const {
    lhs->neverGonnaGiveYouUp("to check its type");
    Porkchop::neverGonnaGiveYouUp(T, "here for it has no instance at all", segment());
    return ScalarTypes::BOOL;
}

$union IsExpr::evalConst() const {
    if (isAny(lhs->typeCache)) throw ConstException("dynamic typing cannot be checked at compile-time", lhs->segment());
    return lhs->typeCache->equals(T);
}

void IsExpr::walkBytecode(Assembler* assembler) const {
    if (isAny(lhs->typeCache)) {
        lhs->walkBytecode(assembler);
        assembler->typed(Opcode::IS, T);
    } else {
        assembler->const_(evalConst().$int);
    }
}

TypeReference DefaultExpr::evalType() const {
    Porkchop::neverGonnaGiveYouUp(T, "here for it has no instance at all", segment());
    if (isAny(T)) throw TypeException("any has no default instance", segment());
    if (auto tuple = dynamic_cast<TupleType*>(T.get())) throw TypeException("tuple has no default instance", segment());
    if (auto func = dynamic_cast<FuncType*>(T.get())) throw TypeException("func has no default instance", segment());
    return T;
}

$union DefaultExpr::evalConst() const {
    if (!isValueBased(T)) throw ConstException("unsupported type for constant evaluation", segment());
    return nullptr;
}

void DefaultExpr::walkBytecode(Assembler* assembler) const {
    if (isValueBased(T)) {
        assembler->const0();
    } else if (isString(T)) {
        assembler->sconst("");
    } else if (auto list = dynamic_cast<ListType*>(T.get())) {
        assembler->cons(Opcode::LIST, typeCache, 0);
    } else if (auto set = dynamic_cast<SetType*>(T.get())) {
        assembler->cons(Opcode::SET, typeCache, 0);
    } else if (auto dict = dynamic_cast<DictType*>(T.get())) {
        assembler->cons(Opcode::DICT, typeCache, 0);
    }
}

TypeReference TupleExpr::evalType() const {
    std::vector<TypeReference> E;
    for (auto&& expr: elements) {
        E.push_back(expr->typeCache);
    }
    return std::make_shared<TupleType>(std::move(E));
}

void TupleExpr::walkBytecode(Assembler* assembler) const {
    for (auto&& e : elements) {
        e->walkBytecode(assembler);
    }
    assembler->typed(Opcode::TUPLE, typeCache);
}

void TupleExpr::walkStoreBytecode(Assembler* assembler) const {
    for (size_t i = 0; i < elements.size(); ++i) {
        assembler->opcode(Opcode::DUP);
        assembler->indexed(Opcode::TLOAD, i);
        if (auto load = dynamic_cast<AssignableExpr*>(elements[i].get())) {
            load->walkStoreBytecode(assembler);
        } else {
            unreachable();
        }
        assembler->opcode(Opcode::POP);
    }
}

void TupleExpr::ensureAssignable() const {
    for (auto&& element : elements) {
        if (auto load = dynamic_cast<AssignableExpr*>(element.get())) {
            load->ensureAssignable();
        } else {
            throw ParserException("assignable expression is expected", element->segment());
        }
    }
}

TypeReference ListExpr::evalType() const {
    auto type0 = elements.front()->typeCache;
    elements.front()->neverGonnaGiveYouUp("as a list element");
    for (size_t i = 1; i < elements.size(); ++i) {
        elements[i]->match(type0, "the elements of list");
    }
    return std::make_shared<ListType>(type0);
}

void ListExpr::walkBytecode(Assembler* assembler) const {
    for (auto&& e : elements) {
        e->walkBytecode(assembler);
    }
    assembler->cons(Opcode::LIST, typeCache, elements.size());
}

TypeReference SetExpr::evalType() const {
    auto type0 = elements.front()->typeCache;
    elements.front()->neverGonnaGiveYouUp("as a set element");
    for (size_t i = 1; i < elements.size(); ++i) {
        elements[i]->match(type0, "the elements of set");
    }
    return std::make_shared<SetType>(type0);
}

void SetExpr::walkBytecode(Assembler* assembler) const {
    for (auto&& e : elements) {
        e->walkBytecode(assembler);
    }
    assembler->cons(Opcode::SET, typeCache, elements.size());
}

TypeReference DictExpr::evalType() const {
    auto key0 = elements.front().first->typeCache;
    auto value0 = elements.front().second->typeCache;
    elements.front().first->neverGonnaGiveYouUp("as a dict key");
    elements.front().second->neverGonnaGiveYouUp("as a dict value");
    for (size_t i = 1; i < elements.size(); ++i) {
        elements[i].first->match(key0, "the keys of dict");
        elements[i].second->match(key0, "the keys of dict");
    }
    return std::make_shared<DictType>(std::move(key0), std::move(value0));
}

void DictExpr::walkBytecode(Assembler* assembler) const {
    for (auto&& [e1, e2] : elements) {
        e1->walkBytecode(assembler);
        e2->walkBytecode(assembler);
    }
    assembler->cons(Opcode::DICT, typeCache, elements.size());
}

TypeReference ClauseExpr::evalType() const {
    for (auto&& expr : lines) if (isNever(expr->typeCache)) return ScalarTypes::NEVER;
    return lines.empty() ? ScalarTypes::NONE : lines.back()->typeCache;
}

$union ClauseExpr::evalConst() const {
    if (lines.empty()) return nullptr;
    $union value = nullptr;
    for (auto&& expr : lines) {
        value = expr->evalConst();
    }
    return value;
}

void ClauseExpr::walkBytecode(Assembler* assembler) const {
    if (lines.empty()) {
        assembler->const0();
    } else {
        bool first = true;
        for (auto&& line : lines) {
            if (first) { first = false; } else { assembler->opcode(Opcode::POP); }
            line->walkBytecode(assembler);
        }
    }
}

TypeReference IfElseExpr::evalType() const {
    if (isNever(cond->typeCache)) return ScalarTypes::NEVER;
    cond->expect(ScalarTypes::BOOL);
    try {
        return (cond->evalConst().$bool ? lhs : rhs)->typeCache;
    } catch (ConstException&) {}
    if (auto either = eithertype(lhs->typeCache, rhs->typeCache)) {
        return either;
    } else {
        rhs->match(lhs->typeCache, "both clauses");
        unreachable();
    }
}

$union IfElseExpr::evalConst() const {
    return cond->evalConst().$bool ? lhs->evalConst() : rhs->evalConst();
}

void IfElseExpr::walkBytecode(Assembler* assembler) const {
    walkBytecode(cond.get(), lhs.get(), rhs.get(), compiler, assembler);
}

void IfElseExpr::walkBytecode(Expr const* cond, Expr const* lhs, Expr const* rhs, Compiler& compiler, Assembler* assembler) {
    size_t A = compiler.nextLabelIndex++;
    size_t B = compiler.nextLabelIndex++;
    cond->walkBytecode(assembler);
    assembler->labeled(Opcode::JMP0, A);
    lhs->walkBytecode(assembler);
    assembler->labeled(Opcode::JMP, B);
    assembler->label(A);
    rhs->walkBytecode(assembler);
    assembler->label(B);
}

TypeReference BreakExpr::evalType() const {
    return ScalarTypes::NEVER;
}

void BreakExpr::walkBytecode(Assembler* assembler) const {
    size_t A = hook->loop->breakpoint;
    assembler->labeled(Opcode::JMP, A);
}

TypeReference WhileExpr::evalType() const {
    if (isNever(cond->typeCache)) return ScalarTypes::NEVER;
    cond->expect(ScalarTypes::BOOL);
    if (isNever(clause->typeCache)) return ScalarTypes::NEVER;
    try {
        if (cond->evalConst().$bool && hook->breaks.empty())
            return ScalarTypes::NEVER;
    } catch (ConstException&) {}
    return ScalarTypes::NONE;
}

void WhileExpr::walkBytecode(Assembler* assembler) const {
    size_t A = compiler.nextLabelIndex++;
    size_t B = compiler.nextLabelIndex++;
    breakpoint = B;
    assembler->label(A);
    cond->walkBytecode(assembler);
    assembler->labeled(Opcode::JMP0, B);
    clause->walkBytecode(assembler);
    assembler->opcode(Opcode::POP);
    assembler->labeled(Opcode::JMP, A);
    assembler->label(B);
    assembler->const0();
}

TypeReference ReturnExpr::evalType() const {
    rhs->neverGonnaGiveYouUp("to return");
    return ScalarTypes::NEVER;
}

void ReturnExpr::walkBytecode(Assembler* assembler) const {
    rhs->walkBytecode(assembler);
    assembler->opcode(Opcode::RETURN);
}

TypeReference FnExprBase::evalType() const {
    return prototype;
}

void FnDeclExpr::walkBytecode(Assembler* assembler) const {
    assembler->indexed(Opcode::FCONST, index);
}

void FnDefExpr::walkBytecode(Assembler* assembler) const {
    assembler->indexed(Opcode::FCONST, index);
}

void LambdaExpr::walkBytecode(Assembler* assembler) const {
    assembler->indexed(Opcode::FCONST, index);
    for (auto&& e : captures) {
        e->walkBytecode(assembler);
    }
    if (!captures.empty())
        assembler->indexed(Opcode::BIND, captures.size());
}

void SimpleDeclarator::infer(TypeReference type) {
    if (designated == nullptr) {
        designated = type;
    } else {
        assignable(type, designated, segment);
    }
    typeCache = designated;
}

void SimpleDeclarator::declare(LocalContext &context) const {
    context.local(name->token, designated);
    name->initLookup(context);
}

void SimpleDeclarator::walkBytecode(Assembler *assembler) const {
    name->walkStoreBytecode(assembler);
}

void TupleDeclarator::infer(TypeReference type) {
    if (auto tuple = dynamic_cast<TupleType*>(type.get())) {
        if (elements.size() != tuple->E.size()) {
            throw TypeException(join("expected ", std::to_string(elements.size()), " elements but got ", std::to_string(tuple->E.size())), segment);
        }
        std::vector<TypeReference> types;
        for (size_t i = 0; i < elements.size(); ++i) {
            elements[i]->infer(tuple->E[i]);
            types.push_back(elements[i]->typeCache);
        }
        typeCache = std::make_shared<TupleType>(std::move(types));
    } else {
        throw TypeException(join("expected a tuple type but got '", type->toString(), "'"), segment);
    }
}

void TupleDeclarator::declare(LocalContext &context) const {
    for (auto&& element : elements) {
        element->declare(context);
    }
}

void TupleDeclarator::walkBytecode(Assembler *assembler) const {
    for (size_t i = 0; i < elements.size(); ++i) {
        assembler->opcode(Opcode::DUP);
        assembler->indexed(Opcode::TLOAD, i);
        elements[i]->walkBytecode(assembler);
        assembler->opcode(Opcode::POP);
    }
}

TypeReference LetExpr::evalType() const {
    return declarator->typeCache;
}

void LetExpr::walkBytecode(Assembler *assembler) const {
    initializer->walkBytecode(assembler);
    declarator->walkBytecode(assembler);
}

TypeReference ForExpr::evalType() const {
    return ScalarTypes::NONE;
}

void ForExpr::walkBytecode(Assembler *assembler) const {
    size_t A = compiler.nextLabelIndex++;
    size_t B = compiler.nextLabelIndex++;
    breakpoint = B;
    initializer->walkBytecode(assembler);
    assembler->opcode(Opcode::ITER);
    assembler->label(A);
    assembler->opcode(Opcode::DUP);
    assembler->opcode(Opcode::MOVE);
    assembler->labeled(Opcode::JMP0, B);
    assembler->opcode(Opcode::DUP);
    assembler->opcode(Opcode::GET);
    declarator->walkBytecode(assembler);
    assembler->opcode(Opcode::POP);
    clause->walkBytecode(assembler);
    assembler->opcode(Opcode::POP);
    assembler->labeled(Opcode::JMP, A);
    assembler->label(B);
    assembler->opcode(Opcode::POP);
    assembler->const0();
}

TypeReference YieldReturnExpr::evalType() const {
    rhs->neverGonnaGiveYouUp("to yield return");
    return ScalarTypes::NEVER;
}

void YieldReturnExpr::walkBytecode(Assembler *assembler) const {
    rhs->walkBytecode(assembler);
    assembler->opcode(Opcode::YIELD);
}

TypeReference YieldBreakExpr::evalType() const {
    return ScalarTypes::NEVER;
}

void YieldBreakExpr::walkBytecode(Assembler *assembler) const {
    assembler->const0();
    assembler->opcode(Opcode::RETURN);
}

TypeReference InterpolationExpr::evalType() const {
    return ScalarTypes::STRING;
}

void InterpolationExpr::walkBytecode(Assembler *assembler) const {
    for (size_t i = 0; i < elements.size(); ++i) {
        literals[i]->walkBytecode(assembler);
        elements[i]->walkBytecode(assembler);
        toStringBytecode(assembler, elements[i]->typeCache);
    }
    literals.back()->walkBytecode(assembler);
    assembler->indexed(Opcode::SJOIN, literals.size() + elements.size());
}

}
