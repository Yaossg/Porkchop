#include "parser.hpp"

namespace Porkchop {

template<typename Derived, typename Base>
std::unique_ptr<Derived> dynamic_pointer_cast(std::unique_ptr<Base>&& base) noexcept {
    if (auto derived = dynamic_cast<Derived*>(base.get())) {
        std::ignore = base.release();
        return std::unique_ptr<Derived>(derived);
    }
    return nullptr;
}

SourceCode::SourceCode(std::string original) noexcept: original(std::move(original)) /* default-constructed members... */ {}

void SourceCode::parse() { // TODO
    if (tokens.empty()) return;
    Parser parser(this, tokens);
    std::tie(tree, type) = parser.parseFnBody();
    parser.expect(TokenType::LINEBREAK, "a linebreak is expected");
    if (parser.remains())
        throw ParserException("unterminated tokens", parser.peek());
}

bool isInLevel(TokenType type, Expr::Level level) {
    switch (type) {
        case TokenType::OP_LOR: return level == Expr::Level::LOR;
        case TokenType::OP_LAND: return level == Expr::Level::LAND;
        case TokenType::OP_OR: return level == Expr::Level::OR;
        case TokenType::OP_XOR: return level == Expr::Level::XOR;
        case TokenType::OP_AND: return level == Expr::Level::AND;
        case TokenType::OP_EQ:
        case TokenType::OP_NEQ: return level == Expr::Level::EQUALITY;
        case TokenType::OP_LT:
        case TokenType::OP_GT:
        case TokenType::OP_LE:
        case TokenType::OP_GE: return level == Expr::Level::COMPARISON;
        case TokenType::OP_SHL:
        case TokenType::OP_SHR:
        case TokenType::OP_USHR: return level == Expr::Level::SHIFT;
        case TokenType::OP_ADD:
        case TokenType::OP_SUB: return level == Expr::Level::ADDITION;
        case TokenType::OP_MUL:
        case TokenType::OP_DIV:
        case TokenType::OP_REM: return level == Expr::Level::MULTIPLICATION;
        default: return false;
    }
}

ExprHandle Parser::parseExpression(Expr::Level level) {
    switch (level) {
        case Expr::Level::ASSIGNMENT: {
            switch (Token token = peek(); token.type) {
                case TokenType::KW_RETURN: {
                    next();
                    auto expr = context.make<ReturnExpr>(token, parseExpression(level));
                    returns.push_back(expr.get());
                    return expr;
                }
                case TokenType::KW_YIELD: {
                    next();
                    auto expr = context.make<YieldExpr>(token, parseExpression(level));
                    if (hooks.empty()) throw ParserException("wild yield", token);
                    hooks.back()->yields.push_back(expr.get());
                    return expr;
                }
                default: {
                    auto lhs = parseExpression(Expr::upper(level));
                    switch (Token token = peek(); token.type) {
                        case TokenType::OP_ASSIGN:
                        case TokenType::OP_ASSIGN_AND:
                        case TokenType::OP_ASSIGN_XOR:
                        case TokenType::OP_ASSIGN_OR:
                        case TokenType::OP_ASSIGN_SHL:
                        case TokenType::OP_ASSIGN_SHR:
                        case TokenType::OP_ASSIGN_USHR:
                        case TokenType::OP_ASSIGN_ADD:
                        case TokenType::OP_ASSIGN_SUB:
                        case TokenType::OP_ASSIGN_MUL:
                        case TokenType::OP_ASSIGN_DIV:
                        case TokenType::OP_ASSIGN_REM: {
                            next();
                            auto rhs = parseExpression(level);
                            if (auto load = dynamic_pointer_cast<LoadExpr>(std::move(lhs))) {
                                return context.make<AssignExpr>(token, std::move(load), std::move(rhs));
                            } else {
                                throw ParserException("id-expression or access expression is expected", token);
                            }
                        }
                    }
                    return lhs;
                }
            }
        }
        [[likely]]
        default: {
            auto lhs = parseExpression(Expr::upper(level));
            while (isInLevel(peek().type, level)) {
                if (Token token = next(); token.type == TokenType::OP_LOR || token.type == TokenType::OP_LAND) {
                    lhs = context.make<LogicalExpr>(token, std::move(lhs), parseExpression(Expr::upper(level)));
                } else {
                    lhs = context.make<InfixExpr>(token, std::move(lhs), parseExpression(Expr::upper(level)));
                }
            }
            return lhs;
        }
        case Expr::Level::PREFIX: {
            switch (Token token = peek(); token.type) {
                case TokenType::OP_ADD:
                case TokenType::OP_SUB:
                case TokenType::OP_NOT:
                case TokenType::OP_INV: {
                    next();
                    auto rhs = parseExpression(level);
                    return context.make<PrefixExpr>(token, std::move(rhs));
                }
                case TokenType::OP_INC:
                case TokenType::OP_DEC: {
                    next();
                    auto rhs = parseExpression(level);
                    if (auto load = dynamic_pointer_cast<LoadExpr>(std::move(rhs))) {
                        return context.make<IdPrefixExpr>(token, std::move(load));
                    } else {
                        throw ParserException("id-expression or access expression is expected", token);
                    }
                }
                default: {
                    return parseExpression(Expr::upper(level));
                }
            }
        }
        case Expr::Level::POSTFIX: {
            auto lhs = parseExpression(Expr::upper(level));
            bool flag = true;
            while (flag) {
                switch (peek().type) {
                    case TokenType::LPAREN: {
                        auto token1 = next();
                        auto expr = parseExpressions(TokenType::RPAREN);
                        auto token2 = next();
                        lhs = context.make<InvokeExpr>(token1, token2, std::move(lhs), std::move(expr));
                        break;
                    }
                    case TokenType::LBRACKET: {
                        auto token1 = next();
                        auto rhs = parseExpression();
                        auto token2 = expect(TokenType::RBRACKET, "missing ']' to match '['");
                        lhs = context.make<AccessExpr>(token1, token2, std::move(lhs), std::move(rhs));
                        break;
                    }
                    case TokenType::OP_DOT: {
                        next();
                        lhs = context.make<DotExpr>(std::move(lhs), parseId(true));
                        break;
                    }
                    case TokenType::KW_AS: {
                        auto token = next();
                        auto type = parseType();
                        lhs = context.make<AsExpr>(token, rewind(), std::move(lhs), std::move(type));
                        break;
                    }
                    case TokenType::KW_IS: {
                        auto token = next();
                        auto type = parseType();
                        lhs = context.make<IsExpr>(token, rewind(), std::move(lhs), std::move(type));
                        break;
                    }
                    case TokenType::OP_INC:
                    case TokenType::OP_DEC: {
                        auto token = next();
                        if (auto load = dynamic_pointer_cast<LoadExpr>(std::move(lhs))) {
                            lhs = context.make<IdPostfixExpr>(token, std::move(load));
                        } else {
                            throw ParserException("id-expression or access expression is expected", token);
                        }
                        break;
                    }
                    default: flag = false;
                }
            }
            return lhs;
        }
        case Expr::Level::PRIMARY: {
            switch (auto token = peek(); token.type) {
                case TokenType::LPAREN: {
                    next();
                    auto expr = parseExpressions(TokenType::RPAREN);
                    auto token2 = next();
                    switch (expr.size()) {
                        case 0:
                            throw ParserException("expression is expected", range(token, token2));
                        case 1:
                            return std::move(expr.front());
                        default:
                            return context.make<TupleExpr>(token, token2, std::move(expr));
                    }
                }
                case TokenType::LBRACKET: {
                    next();
                    auto expr = parseExpressions(TokenType::RBRACKET);
                    auto token2 = next();
                    if (expr.empty()) {
                        throw ParserException("use default([T]) to create empty list", range(token, token2));
                    }
                    return context.make<ListExpr>(token, token2, std::move(expr));
                }
                case TokenType::AT_BRACKET: {
                    next();
                    std::vector<std::pair<ExprHandle, ExprHandle>> elements;
                    size_t values = 0;
                    while (true) {
                        if (peek().type == TokenType::RBRACKET) break;
                        auto key = parseExpression();
                        ExprHandle value;
                        if (peek().type == TokenType::OP_COLON) {
                            next();
                            value = parseExpression();
                            ++values;
                        } else {
                            value = nullptr;
                        }
                        elements.emplace_back(std::move(key), std::move(value));
                        if (peek().type == TokenType::RBRACKET) break;
                        expectComma();
                    }
                    optionalComma(elements.size());
                    auto token2 = next();
                    if (elements.empty()) {
                        throw ParserException("use default(@[T]) or default(@[K: V]) to create empty set or dict", range(token, token2));
                    }
                    if (values == 0) {
                        std::vector<ExprHandle> keys;
                        keys.reserve(elements.size());
                        for (auto&& [key, value] : elements) {
                            keys.push_back(std::move(key));
                        }
                        return context.make<SetExpr>(token, token2, std::move(keys));
                    } else if (values == elements.size()) {
                        return context.make<DictExpr>(token, token2, std::move(elements));
                    } else {
                        throw ParserException("missing values for some keys to create dict", range(token, token2));
                    }
                }
                case TokenType::LBRACE:
                    return parseClause();

                case TokenType::IDENTIFIER:
                    return parseId(true);

                case TokenType::KW_FALSE:
                case TokenType::KW_TRUE:
                    return context.make<BoolConstExpr>(next());
                case TokenType::CHARACTER_LITERAL:
                    return context.make<CharConstExpr>(next());
                case TokenType::STRING_LITERAL:
                    return context.make<StringConstExpr>(next());
                case TokenType::BINARY_INTEGER:
                case TokenType::OCTAL_INTEGER:
                case TokenType::DECIMAL_INTEGER:
                case TokenType::HEXADECIMAL_INTEGER:
                case TokenType::KW_LINE:
                case TokenType::KW_EOF:
                    return context.make<IntConstExpr>(next());
                case TokenType::FLOATING_POINT:
                case TokenType::KW_NAN:
                case TokenType::KW_INF:
                    return context.make<FloatConstExpr>(next());

                case TokenType::KW_DEFAULT: {
                    next();
                    expect(TokenType::LPAREN, "'(' is expected");
                    auto type = parseType();
                    auto token2 = expect(TokenType::RPAREN, "missing ')' to match '('");
                    return context.make<DefaultExpr>(token, token2, type);
                }
                case TokenType::KW_BREAK: {
                    auto expr = context.make<BreakExpr>(next());
                    if (hooks.empty()) throw ParserException("wild break", token);
                    hooks.back()->breaks.push_back(expr.get());
                    return expr;
                }
                case TokenType::KW_WHILE:
                    return parseWhile();
                case TokenType::KW_IF:
                    return parseIf();
                case TokenType::KW_FOR:
                    return parseFor();
                case TokenType::KW_FN:
                    return parseFn();
                case TokenType::OP_DOLLAR:
                    return parseLambda();
                case TokenType::KW_LET:
                    return parseLet();

                case TokenType::KW_ELSE:
                    throw ParserException("stray 'else'", next());

                case TokenType::KW_RETURN:
                case TokenType::KW_YIELD:
                    return parseExpression();

                case TokenType::LINEBREAK:
                    throw ParserException("unexpected linebreak", next());
                default:
                    throw ParserException("unexpected token", next());
            }
        }
    }
}

std::unique_ptr<ClauseExpr> Parser::parseClause() {
    auto token = expect(TokenType::LBRACE, "'{' is expected");
    ReferenceContext::Guard guard(context);
    std::vector<ExprHandle> rhs;
    bool flag = true;
    while (flag) {
        switch (peek().type) {
            default: rhs.emplace_back(parseExpression());
            case TokenType::RBRACE:
            case TokenType::LINEBREAK:
                switch (peek().type) {
                    default: throw ParserException("a linebreak is expected between expressions", peek());
                    case TokenType::RBRACE: flag = false;
                    case TokenType::LINEBREAK: next(); continue;
                }
        }
    }
    return context.make<ClauseExpr>(token, rewind(), std::move(rhs));
}

std::vector<ExprHandle> Parser::parseExpressions(TokenType stop) {
    std::vector<ExprHandle> expr;
    while (true) {
        if (peek().type == stop) break;
        expr.emplace_back(parseExpression());
        if (peek().type == stop) break;
        expectComma();
    }
    optionalComma(expr.size());
    return expr;
}

ExprHandle Parser::parseIf() {
    auto token = next();
    ReferenceContext::Guard guard(context);
    auto cond = parseExpression();
    auto clause = parseClause();
    if (peek().type == TokenType::KW_ELSE) {
        next();
        auto rhs = peek().type == TokenType::KW_IF ? parseIf() : parseClause();
        return context.make<IfElseExpr>(token, std::move(cond), std::move(clause), std::move(rhs));
    }
    return context.make<IfElseExpr>(token, std::move(cond), std::move(clause), context.make<ClauseExpr>(rewind(), rewind()));
}

ExprHandle Parser::parseYieldClause() {
    if (peek().type == TokenType::KW_YIELD) {
        auto token = next();
        auto expr = context.make<YieldExpr>(token, parseClause());
        if (hooks.empty()) throw ParserException("wild yield", token);
        hooks.back()->yields.push_back(expr.get());
        return expr;
    } else {
        return parseClause();
    }
}

ExprHandle Parser::parseWhile() {
    auto token = next();
    pushLoop();
    ReferenceContext::Guard guard(context);
    auto cond = parseExpression();
    auto clause = parseYieldClause();
    return context.make<WhileExpr>(token, std::move(cond), std::move(clause), popLoop());
}

void Parser::declaring(IdExprHandle const& lhs, TypeReference& designated, TypeReference const& type, Segment segment) {
    if (designated == nullptr) designated = type;
    assignable(type, designated, segment);
    context.local(lhs->token, designated);
}

void Parser::destructuring(std::vector<IdExprHandle> const& lhs, std::vector<TypeReference>& designated, TypeReference const& type, Segment segment) {
    if (auto tuple = dynamic_cast<TupleType*>(type.get())) {
        if (designated.size() != tuple->E.size()) {
            throw TypeException("expected " + std::to_string(designated.size()) + " elements but got " + std::to_string(tuple->E.size()), segment);
        }
        for (size_t i = 0; i < designated.size(); ++i) {
            if (designated[i] == nullptr)
                designated[i] = tuple->E[i];
        }
        for (size_t i = 0; i < designated.size(); ++i) {
            assignable(tuple->E[i], designated[i], segment);
        }
        for (size_t i = 0; i < designated.size(); ++i) {
            context.local(lhs[i]->token, designated[i]);
        }
    }
    throw TypeException(unexpected(type, "tuple type"), segment);
}

ExprHandle Parser::parseFor() {
    auto token = next();
    if (peek().type == TokenType::LPAREN) {
        auto [lhs, designated] = parseParameters();
        expect(TokenType::KW_IN, "'in' is expected");
        pushLoop();
        ReferenceContext::Guard guard(context);
        auto rhs = parseExpression();
        if (auto element = iterable(rhs->typeCache)) {
            destructuring(lhs, designated, element, rhs->segment());
            auto clause = parseYieldClause();
            return context.make<ForDestructuringExpr>(token, std::move(lhs), std::move(designated), std::move(rhs), std::move(clause), popLoop());
        }
        throw TypeException(unexpected(rhs->typeCache, "iterable type"), rhs->segment());
    } else {
        auto [lhs, designated] = parseParameter();
        expect(TokenType::KW_IN, "'in' is expected");
        pushLoop();
        ReferenceContext::Guard guard(context);
        auto rhs = parseExpression();
        if (auto element = iterable(rhs->typeCache)) {
            declaring(lhs, designated, element, rhs->segment());
            auto clause = parseYieldClause();
            return context.make<ForExpr>(token, std::move(lhs), std::move(designated), std::move(rhs), std::move(clause), popLoop());
        }
        throw TypeException(unexpected(rhs->typeCache, "iterable type"), rhs->segment());
    }
}

std::pair<IdExprHandle, TypeReference> Parser::parseParameter() {
    auto id = parseId(false);
    auto type = optionalType();
    bool underscore = sourcecode->of(id->token) == "_";
    if (type == nullptr) {
        type = underscore ? ScalarTypes::NONE : nullptr;
    } else if (underscore && !isNone(type)) {
        throw ParserException("the type of '_' must be none", id->token);
    }
    return {std::move(id), std::move(type)};
}

std::pair<std::vector<IdExprHandle>, std::vector<TypeReference>> Parser::parseParameters() {
    expect(TokenType::LPAREN, "'(' is expected");
    std::vector<IdExprHandle> parameters;
    std::vector<TypeReference> P;
    while (true) {
        if (peek().type == TokenType::RPAREN) break;
        auto [id, type] = parseParameter();
        parameters.emplace_back(std::move(id));
        P.emplace_back(std::move(type));
        neverGonnaGiveYouUp(P.back(), "as parameter or tuple element", rewind());
        if (peek().type == TokenType::RPAREN) break;
        expectComma();
    }
    optionalComma(P.size());
    next();
    return {std::move(parameters), std::move(P)};
}

std::pair<ExprHandle, TypeReference> Parser::parseFnBody() {
    auto clause = parseExpression();
    TypeReference type0;
    if (returns.empty()) {
        type0 = clause->typeCache;
    } else {
        type0 = returns[0]->rhs->typeCache;
        for (size_t i = 1; i < returns.size(); ++i) {
            auto type = returns[i]->rhs->typeCache;
            if (!type0->equals(type)) throw TypeException(mismatch(type, "returns", i, type0), returns[i]->segment());
        }
        if (auto type = clause->typeCache; !isNever(type) && !type0->equals(type)) {
            throw TypeException(mismatch(type, "returns and expression result", type0), clause->segment());
        }
    }
    return {std::move(clause), std::move(type0)};
}

ExprHandle Parser::parseFn() {
    auto token = next();
    IdExprHandle name = parseId(false);
    auto [parameters, P] = parseParameters();
    for (size_t i = 0; i < parameters.size(); ++i) {
        if (P[i] == nullptr) {
            throw ParserException("missing type for " + ordinal(i) + " parameter", parameters[i]->segment());
        }
    }
    auto R = optionalType();
    auto F = std::make_shared<FuncType>(std::move(P), std::move(R));
    if (peek().type == TokenType::OP_ASSIGN) {
        auto decl = context.make<FnDeclExpr>(token, rewind(), std::move(name), F);
        next();
        context.declare(decl->name->token, decl.get());
        Parser child(sourcecode, p, q, &context);
        for (size_t i = 0; i < parameters.size(); ++i) {
            child.context.local(parameters[i]->token, F->P[i]);
        }
        auto [clause, type0] = child.parseFnBody();
        p = child.p;
        if (F->R == nullptr) F->R = type0;
        assignable(type0, F->R, range(token, clause->segment()));
        name = std::move(decl->name);
        auto fn = context.make<FnDefExpr>(token, std::move(name), std::move(parameters), std::move(F), std::move(clause));
        context.define(fn->name->token, fn.get());
        sourcecode->fns.push_back(fn.get());
        return fn;
    } else {
        if (F->R == nullptr) {
            throw ParserException("return type of declared function is missing", rewind());
        }
        auto fn = context.make<FnDeclExpr>(token, rewind(), std::move(name), std::move(F));
        context.declare(fn->name->token, fn.get());
        return fn;
    }
}

ExprHandle Parser::parseLambda() {
    auto token = next();
    std::vector<IdExprHandle> captures;
    while (true) {
        if (peek().type == TokenType::LPAREN) break;
        captures.emplace_back(parseId(true));
        if (peek().type == TokenType::LPAREN) break;
        expectComma();
    }
    optionalComma(captures.size());
    auto [parameters, P] = parseParameters();
    for (size_t i = 0; i < parameters.size(); ++i) {
        if (P[i] == nullptr) {
            throw ParserException("missing type for " + ordinal(i) + " parameter", parameters[i]->segment());
        }
    }
    auto R = optionalType();
    auto F = std::make_shared<FuncType>(std::move(P), std::move(R));
    expect(TokenType::OP_ASSIGN, "'=' is expected before lambda body");
    Parser child(sourcecode, p, q, &context);
    for (size_t i = 0; i < captures.size(); ++i) {
        child.context.local(captures[i]->token, F->P[i]);
    }
    for (size_t i = 0; i < parameters.size(); ++i) {
        child.context.local(parameters[i]->token, F->P[i]);
    }
    auto [clause, type0] = child.parseFnBody();
    p = child.p;
    if (F->R == nullptr) F->R = type0;
    assignable(type0, F->R, range(token, clause->segment()));
    auto fn = context.make<LambdaExpr>(token, std::move(captures), std::move(parameters), std::move(F), std::move(clause));
    sourcecode->fns.push_back(fn.get());
    return fn;
}

ExprHandle Parser::parseLet() {
    auto token = next();
    if (peek().type == TokenType::LPAREN) {
        auto [lhs, designated] = parseParameters();
        expect(TokenType::OP_ASSIGN, "'=' is expected before initializer");
        auto rhs = parseExpression();
        destructuring(lhs, designated, rhs->typeCache, rhs->segment());
        return context.make<LetDestructuringExpr>(token, std::move(lhs), std::move(designated), std::move(rhs));
    } else {
        auto [lhs, designated] = parseParameter();
        expect(TokenType::OP_ASSIGN, "'=' is expected before initializer");
        auto rhs = parseExpression();
        declaring(lhs, designated, rhs->typeCache, rhs->segment());
        return context.make<LetExpr>(token, std::move(lhs), std::move(designated), std::move(rhs));
    }
}

TypeReference Parser::parseParenType() {
    expect(TokenType::LPAREN, "'(' is expected");
    auto type = parseType();
    expect(TokenType::RPAREN, "missing ')' to match '('");
    return type;
}

TypeReference Parser::parseType() {
    switch (Token token = next(); token.type) {
        case TokenType::IDENTIFIER: {
            auto id = sourcecode->of(token);
            if (auto it = TYPE_KINDS.find(id); it != TYPE_KINDS.end()) {
                return std::make_shared<ScalarType>(it->second);
            }
            if (id == "typeof") {
                expect(TokenType::LPAREN, "'(' is expected");
                auto expr = parseExpression();
                expect(TokenType::RPAREN, "missing ')' to match '('");
                return expr->typeCache;
            }
            if (id == "elementof") {
                expect(TokenType::LPAREN, "'(' is expected");
                auto type = parseType();
                if (auto list = dynamic_cast<ListType*>(type.get())) {
                    expect(TokenType::RPAREN, "missing ')' to match '('");
                    return list->E;
                } else if (auto set = dynamic_cast<SetType*>(type.get())) {
                    expect(TokenType::RPAREN, "missing ')' to match '('");
                    return set->E;
                } else if (auto tuple = dynamic_cast<TupleType*>(type.get())) {
                    expectComma();
                    auto expr = parseExpression();
                    expect(TokenType::RPAREN, "missing ')' to match '('");
                    auto index = expr->evalConst(*sourcecode);
                    if (0 <= index && index < tuple->E.size()) {
                        return tuple->E[index];
                    } else {
                        throw TypeException("index out of bound", expr->segment());
                    }
                } else {
                    throw TypeException("elementof expect a list or tuple type", token);
                }
            }
            if (id == "keyof") {
                if (auto dict = dynamic_cast<DictType*>(parseParenType().get())) {
                    return dict->K;
                } else {
                    throw TypeException("keyof expect a dict type", token);
                }
            }
            if (id == "valueof") {
                if (auto dict = dynamic_cast<DictType*>(parseParenType().get())) {
                    return dict->V;
                } else {
                    throw TypeException("valueof expect a dict type", token);
                }
            }
            if (id == "returnof") {
                if (auto func = dynamic_cast<FuncType*>(parseParenType().get())) {
                    return func->R;
                } else {
                    throw TypeException("returnof expect a func type", token);
                }
            }
            if (id == "parameterof") {
                expect(TokenType::LPAREN, "'(' is expected");
                auto type = parseType();
                expectComma();
                auto expr = parseExpression();
                expect(TokenType::RPAREN, "missing ')' to match '('");
                expected(expr.get(), ScalarTypes::INT);
                if (auto func = dynamic_cast<FuncType*>(type.get())) {
                    auto index = expr->evalConst(*sourcecode);
                    if (0 <= index && index < func->P.size()) {
                        return func->P[index];
                    } else {
                        throw TypeException("index out of bound", expr->segment());
                    }
                } else {
                    throw TypeException("parameterof expect a func type", token);
                }
            }
        }
        default:
            throw ParserException("type is expected", token);
        case TokenType::LBRACKET: {
            auto E = parseType();
            neverGonnaGiveYouUp(E, "as list element", rewind());
            expect(TokenType::RBRACKET, "missing ']' to match '['");
            return listOf(E);
        }
        case TokenType::AT_BRACKET: {
            auto K = parseType();
            auto V = optionalType();
            expect(TokenType::RBRACKET, "missing ']' to match '@['");
            if (V) {
                neverGonnaGiveYouUp(K, "as dict key", rewind());
                neverGonnaGiveYouUp(V, "as dict value", rewind());
                return std::make_shared<DictType>(std::move(K), std::move(V));
            } else {
                neverGonnaGiveYouUp(K, "as set element", rewind());
                return std::make_shared<SetType>(std::move(K));
            }
        }
        case TokenType::LPAREN: {
            std::vector<TypeReference> P;
            while (true) {
                if (peek().type == TokenType::RPAREN) break;
                P.emplace_back(parseType());
                neverGonnaGiveYouUp(P.back(), "as tuple element or parameter", rewind());
                if (peek().type == TokenType::RPAREN) break;
                expectComma();
            }
            optionalComma(P.size());
            auto token2 = next();
            if (auto R = optionalType()) {
                return std::make_shared<FuncType>(std::move(P), std::move(R));
            } else if (P.size() > 1) {
                return std::make_shared<TupleType>(std::move(P));
            } else {
                throw TypeException("a tuple should contain at least 2 elements", range(token, token2));
            }
        }
    }
}

IdExprHandle Parser::parseId(bool initialize) {
    auto token = next();
    if (token.type != TokenType::IDENTIFIER) throw TokenException("id-expression is expected", token);
    return initialize ? context.make<IdExpr>(token) : std::make_unique<IdExpr>(token);
}

}