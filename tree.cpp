#include <cmath>
#include "tree.hpp"
#include "assembler.hpp"
#include "diagnostics.hpp"
#include "lexer.hpp"

namespace Porkchop {

void Expr::expect(const TypeReference& expected) const {
    if (!getType(expected)->equals(expected)) {
        Error().with(
                ErrorMessage().error(segment())
                .text("expected ").type(expected).text("but got").type(getType())
                ).raise();
    }
}

void Expr::expect(bool pred(const TypeReference&), const char* expected) const {
    if (!pred(getType())) {
        expect(expected);
    }
}

void Expr::expect(const char *expected) const {
    Error().with(
            ErrorMessage().error(segment())
            .text("expected ").text(expected).text(" but got").type(getType())
            ).raise();
}

void matchOperands(Expr* lhs, Expr* rhs) {
    if (!lhs->getType()->equals(rhs->getType())) {
        Error error;
        error.with(ErrorMessage().error(range(lhs->segment(), rhs->segment())).text("type mismatch on both operands"));
        error.with(ErrorMessage().note(lhs->segment()).text("type of left operand is").type(lhs->getType()));
        error.with(ErrorMessage().note(rhs->segment()).text("type of right operand is").type(rhs->getType()));
        error.raise();
    }
}

void assignable(TypeReference const& type, TypeReference const& expected, Segment segment) {
    if (!expected->assignableFrom(type)) {
        Error().with(
                ErrorMessage().error(segment)
                .type(type).text("is not assignable to").type(expected)
                ).raise();
    }
}

void Expr::neverGonnaGiveYouUp(const char* msg) const {
    Porkchop::neverGonnaGiveYouUp(getType(), msg, segment());
}

$union Expr::requireConst() const {
    if (!isConst()) raise("cannot evaluate at compile-time", segment());
    return constValue;
}

TypeReference ensureElements(std::vector<ExprHandle> const& elements, Segment segment, const char* msg) {
    auto type0 = elements.front()->getType();
    elements.front()->neverGonnaGiveYouUp(msg);
    for (size_t i = 1; i < elements.size(); ++i) {
        if (!elements[i]->getType(elements.front()->getType())->equals(type0)) {
            Error error;
            error.with(ErrorMessage().error(segment).text("type must be identical ").text(msg));
            for (auto&& element : elements) {
                error.with(ErrorMessage().note(element->segment()).text("type of this is").type(element->getType()));
            }
            error.raise();
        }
    }
    return type0;
}


BoolConstExpr::BoolConstExpr(Compiler &compiler, Token token) : ConstExpr(compiler, token) {
    parsed = token.type == TokenType::KW_TRUE;
}

TypeReference BoolConstExpr::evalType(TypeReference const& infer) const {
    return ScalarTypes::BOOL;
}

std::optional<$union> BoolConstExpr::evalConst() const {
    return parsed;
}

void BoolConstExpr::walkBytecode(Assembler* assembler) const {
    assembler->const_(parsed);
}

CharConstExpr::CharConstExpr(Compiler &compiler, Token token) : ConstExpr(compiler, token) {
    parsed = parseChar(compiler.source, token);
}

TypeReference CharConstExpr::evalType(TypeReference const& infer) const {
    return ScalarTypes::CHAR;
}

void CharConstExpr::walkBytecode(Assembler* assembler) const {
    assembler->const_((int64_t)parsed);
}

std::optional<$union> CharConstExpr::evalConst() const {
    return parsed;
}

StringConstExpr::StringConstExpr(Compiler &compiler, Token token) : ConstExpr(compiler, token) {
    parsed = parseString(compiler.source, token);
}

TypeReference StringConstExpr::evalType(TypeReference const& infer) const {
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
        case TokenType::BINARY_INTEGER:
        case TokenType::OCTAL_INTEGER:
        case TokenType::DECIMAL_INTEGER:
        case TokenType::HEXADECIMAL_INTEGER:
            parsed = parseInt(compiler.source, token);
            break;
        default:
            unreachable();
    }
}

TypeReference IntConstExpr::evalType(TypeReference const& infer) const {
    return ScalarTypes::INT;
}

std::optional<$union> IntConstExpr::evalConst() const {
    return parsed;
}

void IntConstExpr::walkBytecode(Assembler* assembler) const {
    assembler->const_(parsed);
}

FloatConstExpr::FloatConstExpr(Compiler &compiler, Token token) : ConstExpr(compiler, token) {
    parsed = parseFloat(compiler.source, token);
}

TypeReference FloatConstExpr::evalType(TypeReference const& infer) const {
    return ScalarTypes::FLOAT;
}

void FloatConstExpr::walkBytecode(Assembler* assembler) const {
    assembler->const_(parsed);
}

std::optional<$union> FloatConstExpr::evalConst() const {
    return parsed;
}

TypeReference IdExpr::evalType(TypeReference const& infer) const {
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

std::optional<$union> IdExpr::evalConst() const {
    if (compiler.of(token) == "_")
        return nullptr;
    return Expr::evalConst();
}

void IdExpr::ensureAssignable() const {
    if (lookup.scope == LocalContext::LookupResult::Scope::FUNCTION) {
        raise("function is not assignable", segment());
    }
}

TypeReference PrefixExpr::evalType(TypeReference const& infer) const {
    auto type = rhs->getType();
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
    return rhs->getType();
}

std::optional<$union> PrefixExpr::evalConst() const {
    auto type = rhs->getType();
    if (token.type == TokenType::KW_SIZEOF) {
        if (auto tuple = dynamic_cast<TupleType*>(type.get())) {
            return tuple->E.size();
        }
    }
    if (!rhs->isConst()) return std::nullopt;
    auto value = rhs->requireConst();
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
    auto type = rhs->getType();
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

TypeReference StatefulPrefixExpr::evalType(TypeReference const& infer) const {
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

TypeReference StatefulPostfixExpr::evalType(TypeReference const& infer) const {
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

TypeReference InfixExpr::evalType(TypeReference const& infer) const {
    auto type1 = lhs->getType(), type2 = rhs->getType();
    switch (token.type) {
        case TokenType::OP_OR:
        case TokenType::OP_XOR:
        case TokenType::OP_AND:
            lhs->expect(isIntegral, "integral type");
            matchOperands(lhs.get(), rhs.get());
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
            matchOperands(lhs.get(), rhs.get());
            lhs->expect(isArithmetic, "arithmetic");
            return type1;
        case TokenType::OP_SUB:
        case TokenType::OP_MUL:
        case TokenType::OP_DIV:
        case TokenType::OP_REM:
            matchOperands(lhs.get(), rhs.get());
            lhs->expect(isArithmetic, "arithmetic");
            return type1;
        default:
            unreachable();
    }
}

std::optional<$union> InfixExpr::evalConst() const {
    if (!lhs->isConst() || !rhs->isConst()) return std::nullopt;
    auto value1 = lhs->requireConst(), value2 = rhs->requireConst();
    switch (token.type) {
        case TokenType::OP_OR:
            return value1.$size | value2.$size;
        case TokenType::OP_XOR:
            return value1.$size ^ value2.$size;
        case TokenType::OP_AND:
            return value1.$size & value2.$size;
        case TokenType::OP_SHL:
            if (isInt(lhs->getType())) {
                return value1.$int << value2.$int;
            } else {
                return (uint8_t)(value1.$byte << value2.$int);
            }
        case TokenType::OP_SHR:
            if (isInt(lhs->getType())) {
                return value1.$int >> value2.$int;
            } else {
                return (uint8_t)(value1.$byte >> value2.$int);
            }
        case TokenType::OP_USHR:
            if (isInt(lhs->getType())) {
                return value1.$size >> value2.$int;
            } else {
                return (uint8_t)(value1.$byte >> value2.$int);
            }
        case TokenType::OP_ADD:
            if (isInt(lhs->getType())) {
                return value1.$int + value2.$int;
            } else {
                return value1.$float + value2.$float;
            }
        case TokenType::OP_SUB:
            if (isInt(lhs->getType())) {
                return value1.$int - value2.$int;
            } else {
                return value1.$float - value2.$float;
            }
        case TokenType::OP_MUL:
            if (isInt(lhs->getType())) {
                return value1.$int * value2.$int;
            } else {
                return value1.$float * value2.$float;
            }
        case TokenType::OP_DIV:
            if (isInt(lhs->getType())) {
                int64_t divisor = value2.$int;
                if (divisor == 0) raise("divided by zero", segment());
                return value1.$int / divisor;
            } else {
                return value1.$float / value2.$float;
            }
        case TokenType::OP_REM:
            if (isInt(lhs->getType())) {
                int64_t divisor = value2.$int;
                if (divisor == 0) raise("divided by zero", segment());
                return value1.$int % divisor;
            } else {
                return std::fmod(value1.$float, value2.$float);
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
    if (token.type == TokenType::OP_ADD && isString(getType())) {
        lhs->walkBytecode(assembler);
        toStringBytecode(assembler, lhs->getType());
        rhs->walkBytecode(assembler);
        toStringBytecode(assembler, rhs->getType());
        assembler->opcode(Opcode::SADD);
        return;
    }
    lhs->walkBytecode(assembler);
    rhs->walkBytecode(assembler);
    bool i = isInt(lhs->getType());
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
            if (isByte(lhs->getType())) assembler->opcode(Opcode::I2B);
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

TypeReference CompareExpr::evalType(TypeReference const& infer) const {
    matchOperands(lhs.get(), rhs.get());
    auto type = lhs->getType();
    bool equality =token.type == TokenType::OP_EQ
            || token.type == TokenType::OP_NE
            || token.type == TokenType::OP_EQQ
            || token.type == TokenType::OP_NEQ;
    if (auto scalar = dynamic_cast<ScalarType*>(type.get())) {
        switch (scalar->S) {
            case ScalarTypeKind::ANY:
            case ScalarTypeKind::NONE:
                if (!equality) {
                    raise("none and any only support equality operators", segment());
                }
            case ScalarTypeKind::NEVER:
                lhs->neverGonnaGiveYouUp("in relational operations");
        }
    } else if (!equality) {
        raise("compound types only support equality operators", segment());
    }
    return ScalarTypes::BOOL;
}

std::optional<$union> CompareExpr::evalConst() const {
    auto type = lhs->getType();
    if (!isValueBased(type)) return Expr::evalConst();
    if (isNone(type)) return token.type == TokenType::OP_EQ || token.type == TokenType::OP_EQQ;
    if (!lhs->isConst() || !rhs->isConst()) return std::nullopt;
    auto value1 = lhs->requireConst(), value2 = rhs->requireConst();
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
    auto type = lhs->getType();
    if (isNone(type)) {
        assembler->const_(token.type == TokenType::OP_EQ || token.type == TokenType::OP_EQQ);
        return;
    }
    lhs->walkBytecode(assembler);
    rhs->walkBytecode(assembler);
    Opcode opcode;
    if (token.type == TokenType::OP_EQQ || token.type == TokenType::OP_NEQ) {
        opcode = Opcode::UCMP;
    } else if (auto scalar = dynamic_cast<ScalarType*>(type.get())) {
        switch (scalar->S) {
            case ScalarTypeKind::BOOL:
            case ScalarTypeKind::BYTE:
            case ScalarTypeKind::CHAR:
                opcode = Opcode::UCMP;
                break;
            case ScalarTypeKind::INT:
                opcode = Opcode::ICMP;
                break;
            case ScalarTypeKind::FLOAT:
                opcode = Opcode::FCMP;
                break;
            case ScalarTypeKind::STRING:
                opcode = Opcode::SCMP;
                break;
            default:
                unreachable();
        }
    } else {
        opcode = Opcode::OCMP;
    }
    size_t cmp;
    switch (token.type) {
        case TokenType::OP_EQ:
        case TokenType::OP_EQQ:
            cmp = 0;
            break;
        case TokenType::OP_NE:
        case TokenType::OP_NEQ:
            cmp = 1;
            break;
        case TokenType::OP_LT:
            cmp = 2;
            break;
        case TokenType::OP_GT:
            cmp = 3;
            break;
        case TokenType::OP_LE:
            cmp = 4;
            break;
        case TokenType::OP_GE:
            cmp = 5;
            break;
        default:
            unreachable();
    }
    assembler->indexed(opcode, cmp);
}

TypeReference LogicalExpr::evalType(TypeReference const& infer) const {
    lhs->expect(ScalarTypes::BOOL);
    rhs->expect(ScalarTypes::BOOL);
    return ScalarTypes::BOOL;
}

std::optional<$union> LogicalExpr::evalConst() const {
    auto conjunction = token.type == TokenType::OP_LAND;
    if (!lhs->isConst()) return std::nullopt;
    auto value1 = lhs->requireConst();
    if (conjunction == value1.$bool) {
        if (!rhs->isConst()) return std::nullopt;
        return rhs->requireConst();
    }
    return value1.$bool;
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

TypeReference InExpr::evalType(TypeReference const& infer) const {
    if (auto element = elementof(rhs->getType(), true)) {
        if (auto dict = dynamic_cast<DictType*>(rhs->getType().get())) {
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

TypeReference InfixInvokeExpr::evalType(TypeReference const& infer) const {
    if (auto func = dynamic_cast<FuncType*>(infix->getType().get())) {
        if (func->P.size() != 2) {
            Error error;
            error.with(ErrorMessage().error(segment()).text("infix invocation expected a function with exact two parameters but got").num(func->P.size()));
            error.with(ErrorMessage().note(infix->segment()).text("type of this function is").type(infix->getType()));
            error.raise();
        }
        if (!func->P[0]->assignableFrom(lhs->getType(func->P[0]))) {
            Error error;
            error.with(ErrorMessage().error(lhs->segment()).type(lhs->getType()).text("is not assignable to").type(func->P[0]));
            error.with(ErrorMessage().note(infix->segment()).text("type of this function is").type(infix->getType()));
            error.raise();
        }
        if (!func->P[1]->assignableFrom(rhs->getType(func->P[1]))) {
            Error error;
            error.with(ErrorMessage().error(rhs->segment()).type(rhs->getType()).text("is not assignable to").type(func->P[1]));
            error.with(ErrorMessage().note(infix->segment()).text("type of this function is").type(infix->getType()));
            error.raise();
        }
        return func->R;
    }
    infix->expect("invocable type");
}

void InfixInvokeExpr::walkBytecode(Assembler *assembler) const {
    lhs->walkBytecode(assembler);
    rhs->walkBytecode(assembler);
    infix->walkBytecode(assembler);
    assembler->indexed(Opcode::BIND, 2);
    assembler->opcode(Opcode::CALL);
}

TypeReference AssignExpr::evalType(TypeReference const& infer) const {
    lhs->ensureAssignable();
    auto type1 = lhs->getType();
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
            assignable(rhs->getType(lhs->getType()), lhs->getType(), segment());
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
            if (isString(lhs->getType()))
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
    } else if (auto element = elementof(lhs->getType(), true);
        element && (token.type == TokenType::OP_ASSIGN_ADD || token.type == TokenType::OP_ASSIGN_SUB)) {
        lhs->walkBytecode(assembler);
        rhs->walkBytecode(assembler);
        assembler->opcode(token.type == TokenType::OP_ASSIGN_SUB ? Opcode::REMOVE : Opcode::ADD);
    } else {
        bool i = isInt(lhs->getType());
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
                if (isString(lhs->getType())) {
                    toStringBytecode(assembler, rhs->getType());
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

TypeReference AccessExpr::evalType(TypeReference const& infer) const {
    TypeReference type1 = lhs->getType();
    if (auto tuple = dynamic_cast<TupleType*>(type1.get())) {
        rhs->expect(ScalarTypes::INT);
        int64_t index = rhs->requireConst().$int;
        if (0 <= index && index < tuple->E.size()) {
            return tuple->E[index];
        } else {
            Error error;
            error.with(ErrorMessage().error(rhs->segment()).text("index out of bound"));
            error.with(ErrorMessage().note().text("it evaluates to").num(index));
            error.with(ErrorMessage().note(lhs->segment()).text("type of this tuple is").type(lhs->getType()));
            error.raise();
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
    TypeReference type1 = lhs->getType();
    if (auto tuple = dynamic_cast<TupleType*>(type1.get())) {
        assembler->indexed(Opcode::TLOAD, rhs->requireConst().$int);
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
    TypeReference type1 = lhs->getType();
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
    if (auto tuple = dynamic_cast<TupleType*>(lhs->getType().get())) {
        raise("tuple is immutable and its elements are not assignable", segment());
    }
}

TypeReference InvokeExpr::evalType(TypeReference const& infer) const {
    if (auto func = dynamic_cast<FuncType*>(lhs->getType().get())) {
        if (rhs.size() != func->P.size()) {
            Error error;
            error.with(ErrorMessage().error(range(token1, token2)).text("expected").num(func->P.size()).text("parameters but got").num(rhs.size()));
            error.with(ErrorMessage().note(lhs->segment()).text("type of this function is").type(lhs->getType()));
            error.raise();
        }
        for (size_t i = 0; i < rhs.size(); ++i) {
            if (!func->P[i]->assignableFrom(rhs[i]->getType(func->P[i]))) {
                Error error;
                error.with(ErrorMessage().error(rhs[i]->segment()).type(rhs[i]->getType()).text("is not assignable to").type(func->P[i]));
                error.with(ErrorMessage().note(lhs->segment()).text("type of this function is").type(lhs->getType()));
                error.raise();
            }
        }
        return func->R;
    }
    lhs->expect("invocable type");
}

void InvokeExpr::walkBytecode(Assembler* assembler) const {
    for (auto& e : rhs) {
        e->walkBytecode(assembler);
    }
    lhs->walkBytecode(assembler);
    if (!rhs.empty()) {
        assembler->indexed(Opcode::BIND, rhs.size());
    }
    assembler->opcode(Opcode::CALL);
}

TypeReference DotExpr::evalType(TypeReference const& infer) const {
    if (auto func = dynamic_cast<FuncType*>(rhs->getType().get())) {
        if (func->P.empty()) {
            rhs->expect("a function with at least one parameter");
        }
        if (!func->P.front()->assignableFrom(lhs->getType(func->P.front()))) {
            Error error;
            error.with(ErrorMessage().error(lhs->segment()).type(lhs->getType()).text("is not assignable to").type(func->P.front()));
            error.with(ErrorMessage().note(rhs->segment()).text("type of this function is").type(rhs->getType()));
            error.raise();
        }
        return std::make_shared<FuncType>(std::vector<TypeReference>{std::next(func->P.begin()), func->P.end()},func->R);
    }
    rhs->expect("invocable type");
}

void DotExpr::walkBytecode(Assembler* assembler) const {
    lhs->walkBytecode(assembler);
    rhs->walkBytecode(assembler);
    assembler->indexed(Opcode::BIND, 1);
}

TypeReference BindExpr::evalType(TypeReference const& infer) const {
    if (auto func = dynamic_cast<FuncType*>(lhs->getType().get())) {
        if (rhs.size() > func->P.size()) {
            Error error;
            error.with(ErrorMessage().error(range(token1, token2)).text("expected at most").num(func->P.size()).text("parameters but got").num(rhs.size()));
            error.with(ErrorMessage().note(lhs->segment()).text("type of this function is").type(lhs->getType()));
            error.raise();
        }
        for (size_t i = 0; i < rhs.size(); ++i) {
            if (!func->P[i]->assignableFrom(rhs[i]->getType(func->P[i]))) {
                Error error;
                error.with(ErrorMessage().error(rhs[i]->segment()).type(rhs[i]->getType()).text("is not assignable to").type(func->P[i]));
                error.with(ErrorMessage().note(lhs->segment()).text("type of this function is").type(lhs->getType()));
                error.raise();
            }
        }
        auto P = func->P;
        P.erase(P.begin(), P.begin() + rhs.size());
        return std::make_shared<FuncType>(std::move(P), func->R);
    }
    lhs->expect("invocable type");
}

void BindExpr::walkBytecode(Assembler *assembler) const {
    for (auto& e : rhs) {
        e->walkBytecode(assembler);
    }
    lhs->walkBytecode(assembler);
    if (!rhs.empty()) {
        assembler->indexed(Opcode::BIND, rhs.size());
    }
}

TypeReference AsExpr::evalType(TypeReference const& infer) const {
    auto type = lhs->getType(T);
    if (T->assignableFrom(type) || isAny(T) && !isNever(type) || isAny(type) && !isNever(T)
        || isSimilar(isArithmetic, type, T)
        || isSimilar(isIntegral, type, T)
        || isSimilar(isCharLike, type, T)) return T;
    Error().with(
            ErrorMessage().error(segment())
            .text("cannot cast this expression from").type(type).text("to").type(T)
            ).raise();
}

std::optional<$union> AsExpr::evalConst() const {
    if (!isValueBased(T)) raise("unsupported type for constant evaluation", segment());
    if (!lhs->isConst()) return std::nullopt;
    auto value = lhs->requireConst();
    if (isInt(lhs->getType())) {
        if (isByte(T)) {
            return value.$byte;
        } else if (isChar(T)) {
            if (isInvalidChar(value.$int)) {
                Error error;
                error.with(ErrorMessage().error(segment()).text("invalid").type(ScalarTypes::INT).text("to cast to").type(ScalarTypes::CHAR));
                error.with(ErrorMessage().note(lhs->segment()).text("it evaluates to ").text(std::to_string(value.$int)));
                error.raise();
            }
            return value.$char;
        } else if (isFloat(T)) {
            return (double)value.$int;
        }
    } else if (isInt(T)) {
        if (isFloat(lhs->getType())) {
            return (int64_t)value.$float;
        }
    }
    return value;
}

void AsExpr::walkBytecode(Assembler* assembler) const {
    lhs->walkBytecode(assembler);
    if (lhs->getType()->equals(T)) return;
    if (isAny(lhs->getType())) {
        assembler->typed(Opcode::AS, T);
    } else if (isAny(T)) {
        if (isValueBased(lhs->getType())) {
            assembler->typed(Opcode::ANY, lhs->getType());
        }
    } else if (isNone(T)) {
        assembler->opcode(Opcode::POP);
        assembler->const0();
    } else if (isInt(lhs->getType())) {
        if (isByte(T)) {
            assembler->opcode(Opcode::I2B);
        } else if (isChar(T)) {
            assembler->opcode(Opcode::I2C);
        } else if (isFloat(T)) {
            assembler->opcode(Opcode::I2F);
        }
    } else if (isInt(T)) {
        if (isFloat(lhs->getType())) {
            assembler->opcode(Opcode::F2I);
        }
    }
}

TypeReference IsExpr::evalType(TypeReference const& infer) const {
    lhs->neverGonnaGiveYouUp("to check its type");
    Porkchop::neverGonnaGiveYouUp(T, "here for it has no instance at all", segment());
    return ScalarTypes::BOOL;
}

std::optional<$union> IsExpr::evalConst() const {
    if (isAny(lhs->getType())) raise("dynamic typing cannot be checked at compile-time", lhs->segment());
    return lhs->getType()->equals(T);
}

void IsExpr::walkBytecode(Assembler* assembler) const {
    if (isAny(lhs->getType())) {
        lhs->walkBytecode(assembler);
        assembler->typed(Opcode::IS, T);
    } else {
        assembler->const_(requireConst().$int);
    }
}

TypeReference TupleExpr::evalType(TypeReference const& infer) const {
    std::vector<TypeReference> E;
    auto tuple = dynamic_cast<TupleType*>(infer.get());
    if (tuple != nullptr && tuple->E.size() != elements.size()) tuple = nullptr;
    for (size_t i = 0; i < elements.size(); ++i) {
        E.push_back(elements[i]->getType(tuple == nullptr ? nullptr : tuple->E[i]));
    }
    return std::make_shared<TupleType>(std::move(E));
}

void TupleExpr::walkBytecode(Assembler* assembler) const {
    for (auto&& e : elements) {
        e->walkBytecode(assembler);
    }
    assembler->typed(Opcode::TUPLE, getType());
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
            raise("assignable expression is expected", element->segment());
        }
    }
}

TypeReference ListExpr::evalType(TypeReference const& infer) const {
    if (elements.empty()) {
        if (dynamic_cast<ListType*>(infer.get()))
            return infer;
        raise("element type is unspecified for this list", segment());
    }
    return std::make_shared<ListType>(ensureElements(elements, segment(), "as elements of a list"));
}

void ListExpr::walkBytecode(Assembler* assembler) const {
    for (auto&& e : elements) {
        e->walkBytecode(assembler);
    }
    assembler->cons(Opcode::LIST, getType(), elements.size());
}

TypeReference SetExpr::evalType(TypeReference const& infer) const {
    if (elements.empty()) {
        if (dynamic_cast<SetType*>(infer.get()) || dynamic_cast<DictType*>(infer.get()))
            return infer;
        raise("element type is unspecified for this set or dict", segment());
    }
    return std::make_shared<SetType>(ensureElements(elements, segment(), "as elements of a set"));
}

void SetExpr::walkBytecode(Assembler* assembler) const {
    for (auto&& e : elements) {
        e->walkBytecode(assembler);
    }
    if (elements.empty() && dynamic_cast<DictType*>(getType().get())) {
        assembler->cons(Opcode::DICT, getType(), 0);
    } else {
        assembler->cons(Opcode::SET, getType(), elements.size());
    }
}

TypeReference DictExpr::evalType(TypeReference const& infer) const {
    return std::make_shared<DictType>(ensureElements(keys, segment(), "as keys of a dict"),
                                      ensureElements(values, segment(), "as values of a dict"));
}

void DictExpr::walkBytecode(Assembler* assembler) const {
    for (size_t i = 0; i < keys.size(); ++i) {
        keys[i]->walkBytecode(assembler);
        values[i]->walkBytecode(assembler);
    }
    assembler->cons(Opcode::DICT, getType(), keys.size());
}

TypeReference ClauseExpr::evalType(TypeReference const& infer) const {
    if (lines.empty()) return ScalarTypes::NONE;
    for (size_t i = 0; i < lines.size() - 1; ++i) {
        if (isNever(lines[i]->getType())) {
            Error error;
            error.with(ErrorMessage().error(lines[i + 1]->segment()).text("this line is unreachable"));
            error.with(ErrorMessage().note(lines[i]->segment()).text("since the previous line never returns"));
            error.raise();
        }
    }
    return lines.back()->getType();
}

std::optional<$union> ClauseExpr::evalConst() const {
    if (lines.empty()) return nullptr;
    $union value = nullptr;
    for (auto&& expr : lines) {
        if (!expr->isConst()) return std::nullopt;
        value = expr->requireConst();
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

TypeReference IfElseExpr::evalType(TypeReference const& infer) const {
    cond->expect(ScalarTypes::BOOL);
    if (auto either = eithertype(lhs->getType(), rhs->getType())) {
        return either;
    } else {
        matchOperands(lhs.get(), rhs.get());
        unreachable();
    }
}

std::optional<$union> IfElseExpr::evalConst() const {
    if (!cond->isConst()) return std::nullopt;
    Expr* expr = (cond->requireConst().$bool ? lhs.get() : rhs.get());
    if (!expr->isConst()) return std::nullopt;
    return expr->requireConst();
}

void IfElseExpr::walkBytecode(Assembler* assembler) const {
    walkBytecode(cond.get(), lhs.get(), rhs.get(), compiler, assembler);
}

void IfElseExpr::walkBytecode(Expr const* cond, Expr const* lhs, Expr const* rhs, Compiler& compiler, Assembler* assembler) {
    size_t A = compiler.continuum->labelUntil++;
    size_t B = compiler.continuum->labelUntil++;
    cond->walkBytecode(assembler);
    assembler->labeled(Opcode::JMP0, A);
    lhs->walkBytecode(assembler);
    assembler->labeled(Opcode::JMP, B);
    assembler->label(A);
    rhs->walkBytecode(assembler);
    assembler->label(B);
}

TypeReference BreakExpr::evalType(TypeReference const& infer) const {
    return ScalarTypes::NEVER;
}

void BreakExpr::walkBytecode(Assembler* assembler) const {
    size_t A = hook->loop->breakpoint;
    assembler->labeled(Opcode::JMP, A);
}

TypeReference WhileExpr::evalType(TypeReference const& infer) const {
    if (isNever(cond->getType())) return ScalarTypes::NEVER;
    cond->expect(ScalarTypes::BOOL);
    if (cond->isConst() && cond->requireConst().$bool && hook->breaks.empty())
        return ScalarTypes::NEVER;
    return ScalarTypes::NONE;
}

void WhileExpr::walkBytecode(Assembler* assembler) const {
    size_t A = compiler.continuum->labelUntil++;
    size_t B = compiler.continuum->labelUntil++;
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

TypeReference ReturnExpr::evalType(TypeReference const& infer) const {
    rhs->neverGonnaGiveYouUp("to return");
    return ScalarTypes::NEVER;
}

void ReturnExpr::walkBytecode(Assembler* assembler) const {
    rhs->walkBytecode(assembler);
    assembler->opcode(Opcode::RETURN);
}

TypeReference FnExprBase::evalType(TypeReference const& infer) const {
    return parameters->prototype;
}

void FnDeclExpr::walkBytecode(Assembler* assembler) const {
    assembler->indexed(Opcode::FCONST, index);
}

void FnDefExpr::walkBytecode(Assembler* assembler) const {
    assembler->indexed(Opcode::FCONST, index);
}

void LambdaExpr::walkBytecode(Assembler* assembler) const {
    for (auto&& e : captures) {
        e->walkBytecode(assembler);
    }
    assembler->indexed(Opcode::FCONST, index);
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
    context.local(compiler.of(name->token), designated);
    name->initLookup(context);
}

void SimpleDeclarator::walkBytecode(Assembler *assembler) const {
    name->walkStoreBytecode(assembler);
}

void TupleDeclarator::infer(TypeReference type) {
    if (auto tuple = dynamic_cast<TupleType*>(type.get())) {
        if (elements.size() != tuple->E.size()) {
            Error error;
            error.with(ErrorMessage().error(segment).text("expected").num(tuple->E.size()).text("elements but got").num(elements.size()));
            error.with(ErrorMessage().note().text("initializer for this tuple is").type(type));
            error.raise();
        }
        std::vector<TypeReference> types;
        for (size_t i = 0; i < elements.size(); ++i) {
            elements[i]->infer(tuple->E[i]);
            types.push_back(elements[i]->typeCache);
        }
        typeCache = std::make_shared<TupleType>(std::move(types));
    } else {
        Error()
        .with(ErrorMessage().error(segment).text("expected a tuple type but got").type(typeCache))
        .raise();
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

TypeReference LetExpr::evalType(TypeReference const& infer) const {
    return declarator->typeCache;
}

void LetExpr::walkBytecode(Assembler *assembler) const {
    initializer->walkBytecode(assembler);
    declarator->walkBytecode(assembler);
}

TypeReference ForExpr::evalType(TypeReference const& infer) const {
    if (isNever(clause->getType())) return ScalarTypes::NEVER;
    return ScalarTypes::NONE;
}

void ForExpr::walkBytecode(Assembler *assembler) const {
    size_t A = compiler.continuum->labelUntil++;
    size_t B = compiler.continuum->labelUntil++;
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

TypeReference YieldReturnExpr::evalType(TypeReference const& infer) const {
    rhs->neverGonnaGiveYouUp("to yield return");
    return rhs->getType();
}

void YieldReturnExpr::walkBytecode(Assembler *assembler) const {
    rhs->walkBytecode(assembler);
    assembler->opcode(Opcode::YIELD);
}

TypeReference YieldBreakExpr::evalType(TypeReference const& infer) const {
    return ScalarTypes::NEVER;
}

void YieldBreakExpr::walkBytecode(Assembler *assembler) const {
    assembler->const0();
    assembler->opcode(Opcode::RETURN);
}

TypeReference InterpolationExpr::evalType(TypeReference const& infer) const {
    for (auto&& element : elements) {
        element->getType();
    }
    return ScalarTypes::STRING;
}

void InterpolationExpr::walkBytecode(Assembler *assembler) const {
    for (size_t i = 0; i < elements.size(); ++i) {
        literals[i]->walkBytecode(assembler);
        elements[i]->walkBytecode(assembler);
        toStringBytecode(assembler, elements[i]->getType());
    }
    literals.back()->walkBytecode(assembler);
    assembler->indexed(Opcode::SJOIN, literals.size() + elements.size());
}

TypeReference RawStringExpr::evalType(TypeReference const& infer) const {
    for (auto&& element : elements) {
        element->getType();
    }
    return ScalarTypes::STRING;
}

void RawStringExpr::walkBytecode(Assembler *assembler) const {
    for (auto&& element : elements) {
        element->walkBytecode(assembler);
        toStringBytecode(assembler, element->getType());
    }
    assembler->indexed(Opcode::SJOIN, elements.size());
}

}
