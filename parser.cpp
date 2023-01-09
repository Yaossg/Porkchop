#include "parser.hpp"

namespace Porkchop {

bool isInLevel(TokenType type, Expr::Level level) {
    switch (type) {
        case TokenType::OP_LOR: return level == Expr::Level::LOR;
        case TokenType::OP_LAND: return level == Expr::Level::LAND;
        case TokenType::OP_OR: return level == Expr::Level::OR;
        case TokenType::OP_XOR: return level == Expr::Level::XOR;
        case TokenType::OP_AND: return level == Expr::Level::AND;
        case TokenType::OP_EQ:
        case TokenType::OP_NE:
        case TokenType::OP_EQQ:
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
                case TokenType::KW_BREAK:  {
                    auto expr = make<BreakExpr>(next());
                    if (hooks.empty()) {
                        Error().with(
                                ErrorMessage().error(token)
                                .text("wild").quote("break")
                                ).raise();
                    }
                    hooks.back()->breaks.push_back(expr.get());
                    return expr;
                }
                case TokenType::KW_RETURN: {
                    next();
                    auto expr = make<ReturnExpr>(token, parseExpression(level));
                    returns.push_back(expr.get());
                    return expr;
                }
                case TokenType::KW_YIELD: {
                    next();
                    Token token2 = next();
                    if (token2.type == TokenType::KW_RETURN) {
                        auto expr = make<YieldReturnExpr>(token, token2, parseExpression(level));
                        yieldReturns.push_back(expr.get());
                        return expr;
                    } else if(token2.type == TokenType::KW_BREAK) {
                        auto expr = make<YieldBreakExpr>(token, token2);
                        yieldBreaks.push_back(expr.get());
                        return expr;
                    } else {
                        Error().with(
                                ErrorMessage().error(token)
                                .text("either").quote("yield return").text("or").quote("yield break").text("is expected")
                                ).raise();
                    }
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
                                return make<AssignExpr>(token, std::move(load), std::move(rhs));
                            } else {
                                raise("assignable expression is expected", token);
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
                        lhs = make<LogicalExpr>(token, std::move(lhs), std::move(rhs));
                        break;
                    case Expr::Level::COMPARISON:
                    case Expr::Level::EQUALITY:
                        lhs = make<CompareExpr>(token, std::move(lhs), std::move(rhs));
                        break;
                    [[likely]] default:
                        if (token.type == TokenType::KW_IN) {
                            lhs = make<InExpr>(token, std::move(lhs), std::move(rhs));
                        } else {
                            lhs = make<InfixExpr>(token, std::move(lhs), std::move(rhs));
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
                            return make<IntConstExpr>(Token{token.line, token.column, token.width + token2.width, token2.type}, true);
                        }
                    }
                    return make<PrefixExpr>(token, std::move(rhs));
                }
                case TokenType::OP_NOT:
                case TokenType::OP_INV:
                case TokenType::KW_SIZEOF:
                case TokenType::OP_MUL:
                case TokenType::OP_AND:
                case TokenType::OP_ATAT:
                case TokenType::OP_SHR: {
                    next();
                    auto rhs = parseExpression(level);
                    return make<PrefixExpr>(token, std::move(rhs));
                }
                case TokenType::OP_INC:
                case TokenType::OP_DEC: {
                    next();
                    auto rhs = parseExpression(level);
                    if (auto load = dynamic_pointer_cast<AssignableExpr>(std::move(rhs))) {
                        return make<StatefulPrefixExpr>(token, std::move(load));
                    } else {
                        raise("assignable expression is expected", token);
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
                        lhs = make<InvokeExpr>(token1, token2, std::move(lhs), std::move(expr));
                        break;
                    }
                    case TokenType::LBRACKET: {
                        auto token1 = next();
                        auto rhs = parseExpression();
                        auto token2 = expect(TokenType::RBRACKET, "]");
                        lhs = make<AccessExpr>(token1, token2, std::move(lhs), std::move(rhs));
                        break;
                    }
                    case TokenType::OP_DOT: {
                        next();
                        auto id = parseId(true);
                        lhs = make<DotExpr>(std::move(lhs), std::move(id));
                        break;
                    }
                    case TokenType::KW_AS: {
                        auto token = next();
                        auto type = parseType();
                        lhs = make<AsExpr>(token, rewind(), std::move(lhs), std::move(type));
                        break;
                    }
                    case TokenType::KW_IS: {
                        auto token = next();
                        auto type = parseType();
                        lhs = make<IsExpr>(token, rewind(), std::move(lhs), std::move(type));
                        break;
                    }
                    case TokenType::OP_INC:
                    case TokenType::OP_DEC: {
                        auto token = next();
                        if (auto load = dynamic_pointer_cast<AssignableExpr>(std::move(lhs))) {
                            lhs = make<StatefulPostfixExpr>(token, std::move(load));
                        } else {
                            raise("assignable expression is expected", token);
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
                            return make<ClauseExpr>(token, token2);
                        case 1:
                            return std::move(expr.front());
                        default:
                            return make<TupleExpr>(token, token2, std::move(expr));
                    }
                }
                case TokenType::LBRACKET: {
                    next();
                    auto expr = parseExpressions(TokenType::RBRACKET);
                    auto token2 = next();
                    if (expr.empty()) {
                        Error().with(
                                ErrorMessage().error(range(token, token2))
                                .text("use").quote("default([T])").text("to create an empty list")
                                ).raise();
                    }
                    return make<ListExpr>(token, token2, std::move(expr));
                }
                case TokenType::AT_BRACKET: {
                    next();
                    std::vector<ExprHandle> keys, values;
                    size_t count = 0;
                    while (true) {
                        if (peek().type == TokenType::RBRACKET) break;
                        keys.emplace_back(parseExpression());
                        ExprHandle value;
                        if (peek().type == TokenType::OP_COLON) {
                            next();
                            values.emplace_back(parseExpression());
                            ++count;
                        } else {
                            values.emplace_back(nullptr);
                        }
                        if (peek().type == TokenType::RBRACKET) break;
                        expectComma();
                    }
                    optionalComma(keys.size());
                    auto token2 = next();
                    if (keys.empty()) {
                        Error().with(
                                ErrorMessage().error(range(token, token2))
                                .text("use").quote("default([T])").text("or").quote("default(@[K: V])").text("to create an empty set or dict")
                                ).raise();
                    }
                    if (count == 0) {
                        return make<SetExpr>(token, token2, std::move(keys));
                    } else if (count == keys.size()) {
                        return make<DictExpr>(token, token2, std::move(keys), std::move(values));
                    } else {
                        Error error;
                        error.with(ErrorMessage().error(range(token, token2)).text("missing some values for some keys to create a dict"));
                        for (size_t i = 0; i < keys.size(); ++i) {
                            if (values[i] == nullptr) {
                                error.with(ErrorMessage().note(keys[i]->segment()).text("missing value for this key"));
                            }
                        }
                        error.raise();
                    }
                }
                case TokenType::LBRACE:
                    return parseClause();

                case TokenType::IDENTIFIER:
                    return parseId(true);

                case TokenType::KW_FALSE:
                case TokenType::KW_TRUE:
                    return make<BoolConstExpr>(next());
                case TokenType::CHARACTER_LITERAL:
                    return make<CharConstExpr>(next());
                case TokenType::STRING_QQ:
                    return make<StringConstExpr>(next());
                case TokenType::BINARY_INTEGER:
                case TokenType::OCTAL_INTEGER:
                case TokenType::DECIMAL_INTEGER:
                case TokenType::HEXADECIMAL_INTEGER:
                case TokenType::KW_LINE:
                    return make<IntConstExpr>(next());
                case TokenType::FLOATING_POINT:
                case TokenType::KW_NAN:
                case TokenType::KW_INF:
                    return make<FloatConstExpr>(next());

                case TokenType::STRING_QD: {
                    Token token1 = next(), token2;
                    std::vector<std::unique_ptr<StringConstExpr>> literals;
                    std::vector<ExprHandle> elements;
                    literals.push_back(make<StringConstExpr>(token1));
                    do {
                        elements.push_back(parseExpression(Expr::Level::PRIMARY));
                        token2 = next();
                        literals.push_back(make<StringConstExpr>(token2));
                    } while (token2.type != TokenType::STRING_Q);
                    return make<InterpolationExpr>(token1, token2, std::move(literals), std::move(elements));
                }

                case TokenType::KW_DEFAULT: {
                    next();
                    expect(TokenType::LPAREN, "(");
                    auto type = parseType();
                    auto token2 = expect(TokenType::RPAREN, ")");
                    return make<DefaultExpr>(token, token2, type);
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
                    Error().with(
                            ErrorMessage().error(next())
                            .text("stray").quote("else")
                            ).raise();

                case TokenType::KW_BREAK:
                case TokenType::KW_RETURN:
                case TokenType::KW_YIELD:
                    return parseExpression();

                case TokenType::LINEBREAK:
                    raise("unexpected linebreak", next());
                default:
                    raise("unexpected token", next());
            }
        }
    }
}

std::unique_ptr<ClauseExpr> Parser::parseClause() {
    auto token = expect(TokenType::LBRACE, "{");
    LocalContext::Guard guard(context);
    std::vector<ExprHandle> rhs;
    bool flag = true;
    while (flag) {
        switch (peek().type) {
            default: rhs.emplace_back(parseExpression());
            case TokenType::RBRACE:
            case TokenType::LINEBREAK:
                switch (peek().type) {
                    default: raise("a linebreak is expected between expressions", peek());
                    case TokenType::RBRACE: flag = false;
                    case TokenType::LINEBREAK: next(); continue;
                }
        }
    }
    return make<ClauseExpr>(token, rewind(), std::move(rhs));
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
        return make<IfElseExpr>(token, std::move(cond), std::move(clause), std::move(rhs));
    }
    return make<IfElseExpr>(token, std::move(cond), std::move(clause), make<ClauseExpr>(rewind(), rewind()));
}

ExprHandle Parser::parseWhile() {
    auto token = next();
    pushLoop();
    LocalContext::Guard guard(context);
    auto cond = parseExpression();
    auto clause = parseClause();
    return make<WhileExpr>(token, std::move(cond), std::move(clause), popLoop());
}

ExprHandle Parser::parseFor() {
    auto token = next();
    auto declarator = parseDeclarator();
    expect(TokenType::KW_IN, "in");
    pushLoop();
    LocalContext::Guard guard(context);
    auto initializer = parseExpression();
    if (auto element = elementof(initializer->typeCache)) {
        declarator->infer(element);
        declarator->declare(context);
        auto clause = parseClause();
        return make<ForExpr>(token, std::move(declarator), std::move(initializer), std::move(clause), popLoop());
    } else {
        initializer->expect("iterable type");
    }
}

std::unique_ptr<ParameterList> Parser::parseParameters() {
    expect(TokenType::LPAREN, "(");
    std::vector<IdExprHandle> identifiers;
    std::vector<TypeReference> P;
    while (true) {
        if (peek().type == TokenType::RPAREN) break;
        auto declarator = parseSimpleDeclarator();
        identifiers.push_back(std::move(declarator->name));
        P.push_back(declarator->designated);
        if (declarator->designated == nullptr) {
            raise("missing type for the parameter", declarator->segment);
        }
        if (peek().type == TokenType::RPAREN) break;
        expectComma();
    }
    optionalComma(identifiers.size());
    next();
    return std::make_unique<ParameterList>(std::move(identifiers), std::make_shared<FuncType>(std::move(P),nullptr));
}

ExprHandle Parser::parseFnBody(std::shared_ptr<FuncType> const& F, bool yield, Segment decl) {
    ExprHandle clause;
    TypeReference type0;
    if (yield) {
        clause = parseClause();
        if (!returns.empty()) {
            Error error;
            for (auto&& return_ : returns) {
                error.with(ErrorMessage().error(return_->segment()).quote("return").text("within yielding function"));
            }
            error.with(ErrorMessage().note(decl).text("declared here"));
            error.raise();
        }
        if (F->R && !dynamic_cast<IterType*>(F->R.get())) {
            raise("yielding function must return iter type", decl);
        }
        if (yieldReturns.empty()) {
            if (F->R) {
                return clause;
            } else {
                Error error;
                error.with(ErrorMessage().error(clause->segment()).text("return type of yielding function cannot be deduced without a yield return"));
                error.with(ErrorMessage().note(decl).text("declared here"));
                error.raise();
            }
        } else {
            type0 = yieldReturns[0]->rhs->typeCache;
            for (size_t i = 1; i < yieldReturns.size(); ++i) {
                if (!type0->equals(yieldReturns[i]->typeCache)) {
                    Error error;
                    error.with(ErrorMessage().error(decl).text("multiple yield returns conflict in type"));
                    for (auto&& return_ : yieldReturns) {
                        error.with(ErrorMessage().note(return_->segment()).text("this one yield returns").type(return_->rhs->typeCache));
                    }
                    error.raise();
                }
            }
            type0 = std::make_shared<IterType>(type0);
        }
    } else {
        clause = parseExpression();
        if (!yieldReturns.empty()) {
            Error error;
            for (auto&& return_ : yieldReturns) {
                error.with(ErrorMessage().error(return_->segment()).quote("yield return").text("within non-yielding function"));
            }
            error.with(ErrorMessage().note(decl).text("declared here"));
            error.raise();
        }
        if (!yieldBreaks.empty()) {
            Error error;
            for (auto&& break_ : yieldBreaks) {
                error.with(ErrorMessage().error(break_->segment()).quote("yield break").text("within non-yielding function"));
            }
            error.with(ErrorMessage().note(decl).text("declared here"));
            error.raise();
        }
        if (returns.empty()) {
            type0 = clause->typeCache;
        } else {
            type0 = isNever(clause->typeCache) ? returns[0]->rhs->typeCache : clause->typeCache;
            for (auto&& i : returns) {
                if (!type0->equals(i->typeCache)) {
                    raiseReturns(clause.get(), ErrorMessage().error(decl).text("multiple returns conflict in type"));
                }
            }
        }
    }
    if (F->R == nullptr) {
        F->R = type0;
    } else if (!F->R->assignableFrom(type0)){
        Error error;
        error.with(ErrorMessage().error(clause->segment()).text("actual return type of the function is not assignable to specified one"));
        error.with(ErrorMessage().note().type(type0).text("is not assignable to").type(F->R));
        error.with(ErrorMessage().note(decl).text("declared here"));
        error.raise();
    }
    return clause;
}

ExprHandle Parser::parseFn() {
    auto token = next();
    IdExprHandle name = parseId(false);
    auto parameters = parseParameters();
    parameters->prototype->R = optionalType();
    if (auto type = peek().type; type == TokenType::OP_ASSIGN || type == TokenType::KW_YIELD) {
        auto def = make<FnDefExpr>(token, rewind(), std::move(name), std::move(parameters));
        auto token2 = next();
        bool yield = type == TokenType::KW_YIELD;
        context.declare(compiler->of(def->name->token), def.get());
        LocalContext subcontext(compiler->continuum, &context);
        Parser child(compiler, p, q, subcontext);
        def->parameters->declare(compiler, child.context);
        auto clause = child.parseFnBody(def->parameters->prototype, yield, range(token, token2));
        p = child.p;
        def->definition = std::make_unique<FunctionDefinition>(yield, std::move(clause), std::move(child.context.localTypes));
        context.define(compiler->of(def->name->token), def.get());
        return def;
    } else {
        if (parameters->prototype->R == nullptr) {
            raise("return type of declared function is missing", rewind());
        }
        auto decl = make<FnDeclExpr>(token, rewind(), std::move(name), std::move(parameters));
        context.declare(compiler->of(decl->name->token), decl.get());
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
    auto parameters = parseParameters();
    auto R = optionalType();
    auto type = peek().type;
    if (type != TokenType::OP_ASSIGN && type != TokenType::KW_YIELD) {
        raise("lambda body is expected", next());
    }
    auto token2 = next();
    bool yield = type == TokenType::KW_YIELD;
    LocalContext subcontext(compiler->continuum, &context);
    Parser child(compiler, p, q, subcontext);
    for (auto&& capture : captures) {
        child.context.local(compiler->of(capture->token), capture->typeCache);
    }
    parameters->declare(compiler, child.context);
    auto clause = child.parseFnBody(parameters->prototype, yield, range(token, token2));
    p = child.p;
    auto lambda = make<LambdaExpr>(token, std::move(captures), std::move(parameters),
                                   std::make_unique<FunctionDefinition>(yield, std::move(clause), std::move(child.context.localTypes)));
    context.lambda(lambda.get());
    return lambda;
}

ExprHandle Parser::parseLet() {
    auto token = next();
    auto declarator = parseDeclarator();
    expect(TokenType::OP_ASSIGN, "=");
    auto initializer = parseExpression();
    declarator->infer(initializer->typeCache);
    declarator->declare(context);
    return make<LetExpr>(token, std::move(declarator), std::move(initializer));
}

TypeReference Parser::parseType() {
    switch (Token token = next(); token.type) {
        case TokenType::IDENTIFIER: {
            auto id = compiler->of(token);
            if (auto it = SCALAR_TYPES.find(id); it != SCALAR_TYPES.end()) {
                return std::make_shared<ScalarType>(it->second);
            }
            if (id == "typeof") {
                expect(TokenType::LPAREN, "(");
                auto expr = parseExpression();
                expect(TokenType::RPAREN, ")");
                return expr->typeCache;
            }
            if (id == "elementof") {
                expect(TokenType::LPAREN, "(");
                auto type = parseType();
                if (auto tuple = dynamic_cast<TupleType*>(type.get())) {
                    expectComma();
                    auto expr = parseExpression();
                    expect(TokenType::RPAREN, ")");
                    expr->expect(ScalarTypes::INT);
                    auto index = expr->evalConst().$int;
                    if (0 <= index && index < tuple->E.size()) {
                        return tuple->E[index];
                    } else {
                        Error error;
                        error.with(ErrorMessage().error(expr->segment()).text("index out of bound"));
                        error.with(ErrorMessage().note().text("it evaluates to").num(index));
                        error.with(ErrorMessage().note().text("type of this tuple is").type(type));
                        error.raise();
                    }
                } else {
                    expect(TokenType::RPAREN, ")");
                    if (auto element = elementof(type)) {
                        return element;
                    }
                    Error error;
                    error.with(ErrorMessage().error(token).text("elementof expect a tuple, list, set, dict, or iter type").text("but got").type(type));
                    error.raise();
                }
            }
            if (id == "returnof") {
                expect(TokenType::LPAREN, "(");
                auto type = parseType();
                expect(TokenType::RPAREN, ")");
                if (auto func = dynamic_cast<FuncType*>(type.get())) {
                    return func->R;
                }
                Error error;
                error.with(ErrorMessage().error(token).text("elementof expect a func type but got").type(type));
                error.raise();
            }
            if (id == "parametersof") {
                expect(TokenType::LPAREN, "(");
                auto type = parseType();
                expect(TokenType::RPAREN, ")");
                if (auto func = dynamic_cast<FuncType*>(type.get())) {
                    return std::make_shared<TupleType>(func->P);
                }
                Error error;
                error.with(ErrorMessage().error(token).text("elementof expect a func type but got").type(type));
                error.raise();
            }
        }
        default:
            raise("a type is expected", token);
        case TokenType::LBRACKET: {
            auto E = parseType();
            neverGonnaGiveYouUp(E, "as a list element", rewind());
            expect(TokenType::RBRACKET, "]");
            return std::make_shared<ListType>(E);
        }
        case TokenType::AT_BRACKET: {
            auto K = parseType();
            auto V = optionalType();
            expect(TokenType::RBRACKET, "]");
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
        case TokenType::OP_MUL: {
            auto E = parseType();
            neverGonnaGiveYouUp(E, "as a iter element", rewind());
            return std::make_shared<IterType>(E);
        }
    }
}

IdExprHandle Parser::parseId(bool initialize) {
    auto token = next();
    if (token.type != TokenType::IDENTIFIER) raise("id-expression is expected", token);
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
            Error().with(
                    ErrorMessage().error(segment)
                    .text("the type of").quote("_").text("must be none")
                    ).raise();
        }
    }
    return std::make_unique<SimpleDeclarator>(*compiler, segment, std::move(id), std::move(type));
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
                raise("invalid empty declarator", segment);
            case 1:
                return std::move(elements.front());
            default:
                return std::make_unique<TupleDeclarator>(*compiler, segment, std::move(elements));
        }
    } else {
        return parseSimpleDeclarator();
    }
}

}