#pragma once

#include "tree.hpp"
#include "diagnostics.hpp"

namespace Porkchop {

struct Parser {
    Compiler* compiler;
    std::vector<Token>::const_iterator p;
    const std::vector<Token>::const_iterator q;
    std::vector<std::shared_ptr<LoopHook>> hooks;
    std::vector<const ReturnExpr*> returns;
    std::vector<const YieldReturnExpr*> yieldReturns;
    std::vector<const YieldBreakExpr*> yieldBreaks;
    LocalContext& context;

    Parser(Compiler* compiler, std::vector<Token>::const_iterator p, const std::vector<Token>::const_iterator q, LocalContext& context):
            compiler(compiler), p(p), q(q), context(context) {}

    Token next() {
        if (p != q) [[likely]] {
            return *p++;
        } else [[unlikely]] {
            raise("unexpected termination of tokens", rewind());
        }
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
    TypeReference parseType();
    ExprHandle parseFnBody(std::shared_ptr<FuncType> const& func, bool yield, Segment decl);

    std::unique_ptr<ParameterList> parseParameters();

    std::unique_ptr<SimpleDeclarator> parseSimpleDeclarator();
    DeclaratorHandle parseDeclarator();

    Token expect(TokenType type, const char* msg) {
        auto token = next();
        if (token.type != type) {
            Error().with(ErrorMessage().error(token).quote(msg).text("is expected")).raise();
        }
        return token;
    }

    void expectComma() {
        expect(TokenType::OP_COMMA, ",");
    }

    void optionalComma(size_t size) const {
        if (size == 1 && rewind().type == TokenType::OP_COMMA) {
            raise("the additional comma is forbidden beside a single element", rewind());
        }
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

    void raiseReturns(Expr* clause, ErrorMessage msg) {
        Error error;
        error.with(std::move(msg));
        for (auto&& return_ : returns) {
            error.with(ErrorMessage().note(return_->segment()).text("this one returns").type(return_->rhs->typeCache));
        }
        if (!isNever(clause->typeCache)) {
            auto note = ErrorMessage();
            if (auto clause0 = dynamic_cast<ClauseExpr*>(clause)) {
                note.note(clause0->lines.empty() ? clause0->segment() : clause0->lines.back()->segment());
            } else {
                note.note(clause->segment());
            }
            error.with(note.text("expression evaluates").type(clause->typeCache));
        }
        error.raise();
    }

    template<std::derived_from<Expr> E, typename... Args>
        requires std::constructible_from<E, Compiler&, Args...>
    auto make(Args&&... args) {
        auto expr = std::make_unique<E>(*compiler, std::forward<Args>(args)...);
        expr->initialize();
        return expr;
    }
};

}
