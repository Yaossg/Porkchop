#pragma once

#include "tree.hpp"
#include "diagnostics.hpp"

namespace Porkchop {

struct Parser {
    SourceCode* sourcecode;
    std::vector<Token>::const_iterator p;
    const std::vector<Token>::const_iterator q;
    std::vector<std::shared_ptr<LoopHook>> hooks;
    std::vector<const ReturnExpr*> returns;
    LocalContext context;

    Parser(SourceCode* sourcecode, std::vector<Token> const& tokens): Parser(sourcecode, tokens.cbegin(), tokens.cend(), nullptr) {}

    Parser(SourceCode* sourcecode, std::vector<Token>::const_iterator p, const std::vector<Token>::const_iterator q, LocalContext* parent):
            sourcecode(sourcecode), p(p), q(q), context(this->sourcecode, parent) {}

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
    IdExprHandle parseId(bool initialize);
    ExprHandle parseIf(), parseWhile(), parseFor(), parseFn(), parseLambda(), parseLet();
    TypeReference parseType(), parseParenType();
    std::pair<ExprHandle, TypeReference> parseFnBody();

    std::pair<IdExprHandle, TypeReference> parseParameter();
    std::pair<std::vector<IdExprHandle>, std::vector<TypeReference>> parseParameters();

    void declaring(IdExprHandle const& lhs, TypeReference& designated, TypeReference const& type, Segment segment);
    void destructuring(std::vector<IdExprHandle> const& lhs, std::vector<TypeReference>& designated, TypeReference const& type, Segment segment);

    Token expect(TokenType type, const char* msg) {
        auto token = next();
        if (token.type != type)
            throw ParserException(msg, token);
        return token;
    }
    void expectComma() {
        expect(TokenType::OP_COMMA, "',' is expected");
    }
    void optionalComma(size_t size) const {
        if (size == 1 && rewind().type == TokenType::OP_COMMA)
            throw ParserException("the additional comma is forbidden beside a single element", rewind());
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
