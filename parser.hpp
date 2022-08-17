#pragma once

#include "tree.hpp"
#include "diagnostics.hpp"

namespace Porkchop {

struct Parser {
    SourceCode* sourcecode;
    std::vector<Token>::const_iterator p;
    const std::vector<Token>::const_iterator q;
    std::vector<std::shared_ptr<LoopHook>> hooks;
    std::vector<const FnJumpExpr*> returns;
    std::vector<const FnExpr*> fns;
    ReferenceContext context;

    explicit Parser(SourceCode* sourcecode, std::vector<Token> const& tokens): sourcecode(sourcecode), p(tokens.cbegin()), q(tokens.cend()), context(this->sourcecode) {}

    Token next() {
        if (p != q) [[likely]]
            return *p++;
        else [[unlikely]]
            throw ParserException("unexpected termination of tokens", *--p);
    }
    [[nodiscard]] Token peek() const noexcept {
        return *p;
    }
    [[nodiscard]] Token rewind() const noexcept {
        return *std::prev(p);
    }
    [[nodiscard]] bool remains() const noexcept {
        return p != q;
    }

    ExprHandle parseExpression(Expr::Level level = Expr::Level::ASSIGNMENT);
    std::vector<ExprHandle> parseExpressions(TokenType stop);
    std::unique_ptr<ClauseExpr> parseClause();
    std::unique_ptr<IdExpr> parseId(bool initialize);
    ExprHandle parseIf(), parseYieldClause(), parseWhile(), parseFor(), parseTry(), parseFn(), parseLet();
    TypeReference parseType(), parseParenType();

    Token expect(TokenType type, const char* msg) {
        auto token = next();
        if (token.type != type)
            throw ParserException(msg, token);
        return token;
    }
    void expectComma() {
        expect(TokenType::OP_COMMA, "',' is expected");
    }
    void expectColon() {
        expect(TokenType::OP_COLON, "':' is expected");
    }
    TypeReference optionalType() {
        if (peek().type != TokenType::OP_COLON) return nullptr;
        next();
        return parseType();
    }
    void pushLoop() {
        hooks.emplace_back(std::make_shared<LoopHook>());
    }
    auto popLoop() {
        auto hook = std::move(hooks.back());
        hooks.pop_back();
        return hook;
    }
};

}
