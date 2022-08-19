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

void SourceCode::parse() {
    if (tokens.empty()) return;
    Parser parser(this, tokens);
    tree = parser.parseExpression();
    parser.expect(TokenType::LINEBREAK, "a linebreak is expected");
    if (parser.remains())
        throw ParserException("unterminated tokens", parser.peek());
    type = tree->typeCache;
    if (!parser.returns.empty())
        throw ParserException("wild return", parser.returns.front()->token);
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
                    auto expr = context.make<FnJumpExpr>(token, parseExpression(level));
                    returns.push_back(expr.get());
                    return expr;
                }
                case TokenType::KW_THROW: {
                    next();
                    return context.make<FnJumpExpr>(token, parseExpression(level));
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
                        lhs = context.make<AsExpr>(token, rewind(), std::move(lhs), parseType());
                        break;
                    }
                    case TokenType::KW_IS: {
                        auto token = next();
                        lhs = context.make<IsExpr>(token, rewind(), std::move(lhs), parseType());
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
                    return context.make<ListExpr>(token, token2, std::move(expr));
                }
                case TokenType::AT_BRACKET: {
                    next();
                    std::vector<std::pair<ExprHandle, ExprHandle>> elements;
                    while (true) {
                        if (peek().type == TokenType::RBRACKET) break;
                        auto key = parseExpression();
                        ExprHandle value;
                        if (peek().type == TokenType::OP_COLON) {
                            next();
                            value = parseExpression();
                        } else {
                            value = context.make<ClauseExpr>(peek(), peek());
                        }
                        elements.emplace_back(std::move(key), std::move(value));
                        if (peek().type == TokenType::RBRACKET) break;
                        expectComma();
                    }
                    unexpectedComma(elements.size());
                    auto token2 = next();
                    return context.make<DictExpr>(token, token2, std::move(elements));
                }
                case TokenType::LBRACE:
                    return parseClause();

                case TokenType::IDENTIFIER:
                    return parseId(true);

                case TokenType::KW_FALSE:
                case TokenType::KW_TRUE:
                case TokenType::KW_LINE:
                case TokenType::KW_EOF:
                case TokenType::KW_NAN:
                case TokenType::KW_INF:
                case TokenType::CHARACTER_LITERAL:
                case TokenType::STRING_LITERAL:
                case TokenType::BINARY_INTEGER:
                case TokenType::OCTAL_INTEGER:
                case TokenType::DECIMAL_INTEGER:
                case TokenType::HEXADECIMAL_INTEGER:
                case TokenType::FLOATING_POINT:
                    return context.make<ConstExpr>(next());

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
                case TokenType::KW_TRY:
                    return parseTry();
                case TokenType::KW_FOR:
                    return parseFor();
                case TokenType::KW_FN:
                    return parseFn();
                case TokenType::KW_LET:
                    return parseLet();

                case TokenType::KW_ELSE:
                    throw ParserException("stray 'else'", next());
                case TokenType::KW_CATCH:
                    throw ParserException("stray 'catch'", next());

                case TokenType::KW_RETURN:
                case TokenType::KW_THROW:
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
            case TokenType::RBRACE: flag = false;
            case TokenType::LINEBREAK: next(); continue;
            default: rhs.emplace_back(parseExpression());
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
    unexpectedComma(expr.size());
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
    context.local(context.sourcecode->source(lhs->token), designated);
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
            context.local(context.sourcecode->source(lhs[i]->token), designated[i]);
        }
    }
    throw TypeException(unexpected(type, "tuple type"), segment);
}

ExprHandle Parser::parseFor() {
    auto token = next();
    if (peek().type == TokenType::LPAREN) {
        auto [lhs, designated] = parseParameters(nullptr);
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
        auto [lhs, designated] = parseParameter(nullptr);
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

ExprHandle Parser::parseTry() {
    auto token = next();
    auto lhs = parseClause();
    expect(TokenType::KW_CATCH, "'catch' is expected");
    auto except = parseId(false);
    ReferenceContext::Guard guard(context);
    context.local(context.sourcecode->source(except->token), ScalarTypes::ANY);
    return context.make<TryExpr>(token, std::move(lhs), std::move(except), parseClause());
}

std::pair<IdExprHandle, TypeReference> Parser::parseParameter(const TypeReference& fallback) {
    auto id = parseId(false);
    auto type = optionalType();
    bool underscore = sourcecode->source(id->token) == "_";
    if (type == nullptr) {
        type = underscore ? ScalarTypes::NONE : fallback;
    } else if (underscore && !isNone(type)) {
        throw ParserException("the type of '_' must be none", id->token);
    }
    return {std::move(id), std::move(type)};
}

std::pair<std::vector<IdExprHandle>, std::vector<TypeReference>> Parser::parseParameters(TypeReference const& fallback) {
    expect(TokenType::LPAREN, "'(' is expected");
    std::vector<IdExprHandle> parameters;
    std::vector<TypeReference> P;
    while (true) {
        if (peek().type == TokenType::RPAREN) break;
        auto [id, type] = parseParameter(fallback);
        parameters.emplace_back(std::move(id));
        P.emplace_back(std::move(type));
        neverGonnaGiveYouUp(P.back(), "as parameter or tuple element", rewind());
        if (peek().type == TokenType::RPAREN) break;
        expectComma();
    }
    unexpectedComma(P.size());
    next();
    return {std::move(parameters), std::move(P)};
}

ExprHandle Parser::parseFn() {
    auto token = next();
    auto [parameters, P] = parseParameters(ScalarTypes::ANY);
    auto R = optionalType();
    expect(TokenType::OP_ASSIGN, "'=' is expected before function body");
    auto _hooks = std::move(hooks);
    auto _returns = std::move(returns);
    auto _context = std::move(context);
    context = ReferenceContext(context.sourcecode);
    for (size_t i = 0; i < parameters.size(); ++i) {
        context.local(context.sourcecode->source(parameters[i]->token), P[i]);
    }
    auto clause = parseExpression();
    auto fn = context.make<FnExpr>(token, std::move(parameters), std::make_shared<FuncType>(std::move(P), std::move(R)), std::move(clause), std::move(returns));
    hooks = std::move(_hooks);
    returns = std::move(_returns);
    context = std::move(_context);
    fns.push_back(fn.get());
    return fn;
}

ExprHandle Parser::parseLet() {
    auto token = next();
    if (peek().type == TokenType::LPAREN) {
        auto [lhs, designated] = parseParameters(nullptr);
        expect(TokenType::OP_ASSIGN, "'=' is expected before initializer");
        auto rhs = parseExpression();
        destructuring(lhs, designated, rhs->typeCache, rhs->segment());
        return context.make<LetDestructuringExpr>(token, std::move(lhs), std::move(designated), std::move(rhs));
    } else {
        auto [lhs, designated] = parseParameter(nullptr);
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
            auto id = sourcecode->source(token);
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
            neverGonnaGiveYouUp(K, "as dict key", rewind());
            expectColon();
            auto V = parseType();
            neverGonnaGiveYouUp(V, "as dict value", rewind());
            expect(TokenType::RBRACKET, "missing ']' to match '@['");
            return std::make_shared<DictType>(std::move(K), std::move(V));
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
            unexpectedComma(P.size());
            auto token2 = next();
            if (peek().type == TokenType::OP_COLON) {
                next();
                auto R = parseType();
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