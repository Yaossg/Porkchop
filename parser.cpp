#include "parser.hpp"
#include "util.hpp"

namespace Porkchop {

bool isInLevel(TokenType type, Expr::Level level) {
    switch (type) {
        case TokenType::OP_LOR: return level == Expr::Level::LOR;
        case TokenType::OP_LAND: return level == Expr::Level::LAND;
        case TokenType::OP_OR: return level == Expr::Level::OR;
        case TokenType::OP_XOR: return level == Expr::Level::XOR;
        case TokenType::OP_AND: return level == Expr::Level::AND;
        case TokenType::OP_EQ:
        case TokenType::OP_NE: return level == Expr::Level::EQUALITY;
        case TokenType::OP_LT:
        case TokenType::OP_GT:
        case TokenType::OP_LE:
        case TokenType::OP_GE: return level == Expr::Level::COMPARISON;
        case TokenType::OP_SHL:
        case TokenType::OP_SHR:
        case TokenType::OP_USHR: return level == Expr::Level::SHIFT;
        case TokenType::OP_ADD:
        case TokenType::OP_SUB: return level == Expr::Level::ADDITION;
        case TokenType::KW_IN:
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
                            if (auto load = dynamic_pointer_cast<AssignableExpr>(std::move(lhs))) {
                                return context.make<AssignExpr>(token, std::move(load), std::move(rhs));
                            } else {
                                throw ParserException("assignable expression is expected", token);
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
                auto token = next();
                auto rhs = parseExpression(Expr::upper(level));
                switch (level) {
                    case Expr::Level::LAND:
                    case Expr::Level::LOR:
                        lhs = context.make<LogicalExpr>(token, std::move(lhs), std::move(rhs));
                        break;
                    case Expr::Level::COMPARISON:
                    case Expr::Level::EQUALITY:
                        lhs = context.make<CompareExpr>(token, std::move(lhs), std::move(rhs));
                        break;
                    [[likely]] default:
                        if (token.type == TokenType::KW_IN) {
                            lhs = context.make<InExpr>(token, std::move(lhs), std::move(rhs));
                        } else {
                            lhs = context.make<InfixExpr>(token, std::move(lhs), std::move(rhs));
                        }
                        break;
                }
            }
            return lhs;
        }
        case Expr::Level::PREFIX: {
            switch (Token token = peek(); token.type) {
                case TokenType::OP_ADD:
                case TokenType::OP_SUB: {
                    next();
                    auto rhs = parseExpression(level);
                    if (auto number = dynamic_cast<IntConstExpr*>(rhs.get())) {
                        auto token2 = number->token;
                        if (!number->merged && token.line == token2.line && token.column + token.width == token2.column) {
                            return context.make<IntConstExpr>(Token{token.line, token.column, token.width + token2.width, token2.type}, true);
                        }
                    }
                    return context.make<PrefixExpr>(token, std::move(rhs));
                }
                case TokenType::OP_NOT:
                case TokenType::OP_INV:
                case TokenType::KW_SIZEOF: {
                    next();
                    auto rhs = parseExpression(level);
                    return context.make<PrefixExpr>(token, std::move(rhs));
                }
                case TokenType::OP_INC:
                case TokenType::OP_DEC: {
                    next();
                    auto rhs = parseExpression(level);
                    if (auto load = dynamic_pointer_cast<AssignableExpr>(std::move(rhs))) {
                        return context.make<StatefulPrefixExpr>(token, std::move(load));
                    } else {
                        throw ParserException("assignable expression is expected", token);
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
                        auto id = parseId(true);
                        lhs = context.make<DotExpr>(std::move(lhs), std::move(id));
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
                        if (auto load = dynamic_pointer_cast<AssignableExpr>(std::move(lhs))) {
                            lhs = context.make<StatefulPostfixExpr>(token, std::move(load));
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
                            return context.make<ClauseExpr>(token, token2);
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
    LocalContext::Guard guard(context);
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
    LocalContext::Guard guard(context);
    auto cond = parseExpression();
    auto clause = parseClause();
    if (peek().type == TokenType::KW_ELSE) {
        next();
        auto rhs = peek().type == TokenType::KW_IF ? parseIf() : parseClause();
        return context.make<IfElseExpr>(token, std::move(cond), std::move(clause), std::move(rhs));
    }
    return context.make<IfElseExpr>(token, std::move(cond), std::move(clause), context.make<ClauseExpr>(rewind(), rewind()));
}

ExprHandle Parser::parseWhile() {
    auto token = next();
    pushLoop();
    LocalContext::Guard guard(context);
    auto cond = parseExpression();
    auto clause = parseClause();
    return context.make<WhileExpr>(token, std::move(cond), std::move(clause), popLoop());
}

ExprHandle Parser::parseFor() {
    auto token = next();
    auto declarator = parseDeclarator();
    expect(TokenType::KW_IN, "'in' is expected");
    pushLoop();
    LocalContext::Guard guard(context);
    auto initializer = parseExpression();
    if (auto element = elementof(initializer->typeCache)) {
        declarator->infer(element);
        declarator->declare(context);
        auto clause = parseClause();
        return context.make<ForExpr>(token, std::move(declarator), std::move(initializer), std::move(clause), popLoop());
    } else {
        initializer->expect("iterable type");
    }
}

std::pair<std::vector<IdExprHandle>, std::vector<TypeReference>> Parser::parseParameters() {
    expect(TokenType::LPAREN, "'(' is expected");
    std::pair<std::vector<IdExprHandle>, std::vector<TypeReference>> parameters;
    while (true) {
        if (peek().type == TokenType::RPAREN) break;
        auto declarator = parseSimpleDeclarator();
        parameters.first.push_back(std::move(declarator->name));
        parameters.second.push_back(declarator->designated);
        if (peek().type == TokenType::RPAREN) break;
        expectComma();
    }
    optionalComma(parameters.first.size());
    next();
    return parameters;
}

ExprHandle Parser::parseFnBody(std::shared_ptr<FuncType> const& F) {
    auto clause = parseExpression();
    TypeReference type0;
    if (returns.empty()) {
        type0 = clause->typeCache;
    } else {
        type0 = returns[0]->rhs->typeCache;
        for (size_t i = 1; i < returns.size(); ++i) {
            auto type = returns[i]->rhs->typeCache;
            if (!type0->equals(type)) {
                returns[i]->mismatch(type0, "returns", i);
            }
        }
        if (auto type = clause->typeCache; !isNever(type) && !type0->equals(type)) {
            mismatch(type0, type, "returns and expression body", clause->segment());
        }
    }
    if (F->R == nullptr) {
        F->R = type0;
    } else {
        assignable(type0, F->R, clause->segment());
    }
    return clause;
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
        Parser child(compiler, p, q, &context);
        for (size_t i = 0; i < parameters.size(); ++i) {
            child.context.local(parameters[i]->token, F->P[i]);
        }
        auto clause = child.parseFnBody(F);
        p = child.p;
        name = std::move(decl->name);
        auto fn = context.make<FnDefExpr>(token, std::move(name), std::move(parameters), std::move(F), std::move(clause));
        fn->locals = std::move(child.context.localTypes);
        context.define(fn->name->token, fn.get());
        return fn;
    } else {
        if (F->R == nullptr) {
            throw ParserException("return type of declared function is missing", rewind());
        }
        auto decl = context.make<FnDeclExpr>(token, rewind(), std::move(name), std::move(F));
        context.declare(decl->name->token, decl.get());
        return decl;
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
    Parser child(compiler, p, q, &context);
    for (auto&& capture : captures) {
        child.context.local(capture->token, capture->typeCache);
    }
    for (size_t i = 0; i < parameters.size(); ++i) {
        child.context.local(parameters[i]->token, F->P[i]);
    }
    auto clause = child.parseFnBody(F);
    p = child.p;
    auto lambda = context.make<LambdaExpr>(token, std::move(captures), std::move(parameters), std::move(F), std::move(clause));
    lambda->locals = std::move(child.context.localTypes);
    context.lambda(lambda.get());
    return lambda;
}

ExprHandle Parser::parseLet() {
    auto token = next();
    auto declarator = parseDeclarator();
    expect(TokenType::OP_ASSIGN, "'=' is expected before initializer");
    auto initializer = parseExpression();
    declarator->infer(initializer->typeCache);
    declarator->declare(context);
    return context.make<LetExpr>(token, std::move(declarator), std::move(initializer));
}

TypeReference Parser::parseType() {
    switch (Token token = next(); token.type) {
        case TokenType::IDENTIFIER: {
            auto id = compiler->of(token);
            if (auto it = SCALAR_TYPES.find(id); it != SCALAR_TYPES.end()) {
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
                if (auto tuple = dynamic_cast<TupleType*>(type.get())) {
                    expectComma();
                    auto expr = parseExpression();
                    expect(TokenType::RPAREN, "missing ')' to match '('");
                    expr->expect(ScalarTypes::INT);
                    auto index = expr->evalConst().$int;
                    if (0 <= index && index < tuple->E.size()) {
                        return tuple->E[index];
                    } else {
                        throw TypeException("index out of bound", expr->segment());
                    }
                } else {
                    auto element = elementof(type);
                    expect(TokenType::RPAREN, "missing ')' to match '('");
                    if (element == nullptr) {
                        throw TypeException("elementof expect a tuple, list, set, or dict type", token);
                    }
                    return elementof(type);
                }
            }
            if (id == "returnof") {
                expect(TokenType::LPAREN, "'(' is expected");
                auto type = parseType();
                expect(TokenType::RPAREN, "missing ')' to match '('");
                if (auto func = dynamic_cast<FuncType*>(type.get())) {
                    return func->R;
                } else {
                    throw TypeException("returnof expect a func type", token);
                }
            }
            if (id == "parametersof") {
                expect(TokenType::LPAREN, "'(' is expected");
                auto type = parseType();
                expect(TokenType::RPAREN, "missing ')' to match '('");
                if (auto func = dynamic_cast<FuncType*>(type.get())) {
                    return std::make_shared<TupleType>(func->P);
                } else {
                    throw TypeException("parametersof expect a func type", token);
                }
            }
        }
        default:
            throw ParserException("type is expected", token);
        case TokenType::LBRACKET: {
            auto E = parseType();
            neverGonnaGiveYouUp(E, "as a list element", rewind());
            expect(TokenType::RBRACKET, "missing ']' to match '['");
            return std::make_shared<ListType>(E);
        }
        case TokenType::AT_BRACKET: {
            auto K = parseType();
            auto V = optionalType();
            expect(TokenType::RBRACKET, "missing ']' to match '@['");
            if (V) {
                neverGonnaGiveYouUp(K, "as a dict key", rewind());
                neverGonnaGiveYouUp(V, "as a dict value", rewind());
                return std::make_shared<DictType>(std::move(K), std::move(V));
            } else {
                neverGonnaGiveYouUp(K, "as a set element", rewind());
                return std::make_shared<SetType>(std::move(K));
            }
        }
        case TokenType::LPAREN: {
            std::vector<TypeReference> P;
            while (true) {
                if (peek().type == TokenType::RPAREN) break;
                P.emplace_back(parseType());
                neverGonnaGiveYouUp(P.back(), "as a tuple element or a parameter", rewind());
                if (peek().type == TokenType::RPAREN) break;
                expectComma();
            }
            optionalComma(P.size());
            next();
            if (auto R = optionalType()) {
                return std::make_shared<FuncType>(std::move(P), std::move(R));
            } else {
                switch (P.size()) {
                    case 0:
                        return ScalarTypes::NONE;
                    case 1:
                        return P.front();
                    default:
                        return std::make_shared<TupleType>(std::move(P));
                }
            }
        }
    }
}

IdExprHandle Parser::parseId(bool initialize) {
    auto token = next();
    if (token.type != TokenType::IDENTIFIER) throw ParserException("id-expression is expected", token);
    auto id = std::make_unique<IdExpr>(*compiler, token);
    if (initialize) {
        id->initLookup(context);
        id->initialize();
    }
    return id;
}

std::unique_ptr<SimpleDeclarator> Parser::parseSimpleDeclarator() {
    auto id = parseId(false);
    auto type = optionalType();
    bool underscore = compiler->of(id->token) == "_";
    auto segment = range(id->segment(), rewind());
    if (type == nullptr) {
        if (underscore) type = ScalarTypes::NONE;
    } else {
        neverGonnaGiveYouUp(type, "as a declarator", segment);
        if (underscore && !isNone(type)) {
            throw ParserException("the type of '_' must be none", segment);
        }
    }
    return std::make_unique<SimpleDeclarator>(segment, std::move(id), std::move(type));
}

DeclaratorHandle Parser::parseDeclarator() {
    if (peek().type == TokenType::LPAREN) {
        auto token1 = next();
        std::vector<DeclaratorHandle> elements;
        while (true) {
            if (peek().type == TokenType::RPAREN) break;
            elements.emplace_back(parseDeclarator());
            if (peek().type == TokenType::RPAREN) break;
            expectComma();
        }
        optionalComma(elements.size());
        auto token2 = next();
        auto segment = range(token1, token2);
        switch (elements.size()) {
            case 0:
                throw ParserException("invalid empty declarator", segment);
            case 1:
                return std::move(elements.front());
            default:
                return std::make_unique<TupleDeclarator>(segment, std::move(elements));
        }
    } else {
        return parseSimpleDeclarator();
    }
}

}