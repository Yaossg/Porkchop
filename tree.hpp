#pragma once

#include "token.hpp"
#include "local.hpp"
#include "type.hpp"
#include "diagnostics.hpp"

namespace Porkchop {

struct Expr : Descriptor {
    enum class Level {
        ASSIGNMENT,
        LOR,
        LAND,
        OR,
        XOR,
        AND,
        EQUALITY,
        COMPARISON,
        SHIFT,
        ADDITION,
        MULTIPLICATION,
        PREFIX,
        POSTFIX,
        PRIMARY
    };
    [[nodiscard]] static Level upper(Level level) noexcept {
        return (Level)((int)level + 1);
    }

    TypeReference typeCache;
    void initialize(ReferenceContext& context) {
        typeCache = evalType(context);
    }

    [[nodiscard]] virtual Segment segment() const = 0;

    [[nodiscard]] virtual TypeReference evalType(ReferenceContext& context) const = 0;

    [[nodiscard]] virtual int64_t evalConst(SourceCode& sourcecode) const {
        throw TypeException("cannot evaluate at compile-time", segment());
    }
};

struct ConstExpr : Expr {
    Token token;

    explicit ConstExpr(Token token): token(token) {}

    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return sourcecode.source(token); }

    [[nodiscard]] Segment segment() const override {
        return token;
    }

    [[nodiscard]] TypeReference evalType(ReferenceContext& context) const override;

    [[nodiscard]] int64_t evalConst(SourceCode& sourcecode) const override;
};
struct LoadExpr : Expr {};
struct IdExpr : LoadExpr {
    Token token;

    explicit IdExpr(Token token): token(token) {}

    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return sourcecode.source(token); }

    [[nodiscard]] Segment segment() const override {
        return token;
    }

    [[nodiscard]] TypeReference evalType(ReferenceContext& context) const override {
        if (auto type = context.lookup(context.sourcecode->source(token))) return type;
        throw TypeException("unable to find the object", token);
    }
};
struct PrefixExpr : Expr {
    Token token;
    ExprHandle rhs;

    PrefixExpr(Token token, ExprHandle rhs): token(token), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {rhs.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return sourcecode.source(token); }

    [[nodiscard]] Segment segment() const override {
        return range(token, rhs->segment());
    }

    [[nodiscard]] TypeReference evalType(ReferenceContext& context) const override;

    [[nodiscard]] int64_t evalConst(SourceCode& sourcecode) const override;
};
struct InfixExpr : Expr {
    Token token;
    ExprHandle lhs;
    ExprHandle rhs;

    InfixExpr(Token token, ExprHandle lhs, ExprHandle rhs): token(token), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get(), rhs.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return sourcecode.source(token); }

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), rhs->segment());
    }

    [[nodiscard]] TypeReference evalType(ReferenceContext& context) const override;

    [[nodiscard]] int64_t evalConst(SourceCode& sourcecode) const override;
};
struct AssignExpr : Expr {
    Token token;
    std::unique_ptr<LoadExpr> lhs;
    ExprHandle rhs;

    AssignExpr(Token token, std::unique_ptr<LoadExpr> lhs, ExprHandle rhs): token(token), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get(), rhs.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return sourcecode.source(token); }

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), rhs->segment());
    }

    [[nodiscard]] TypeReference evalType(ReferenceContext& context) const override;
};
struct LogicalExpr : Expr {
    Token token;
    ExprHandle lhs;
    ExprHandle rhs;

    LogicalExpr(Token token, ExprHandle lhs, ExprHandle rhs): token(token), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get(), rhs.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return sourcecode.source(token); }

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), rhs->segment());
    }

    [[nodiscard]] TypeReference evalType(ReferenceContext& context) const override;

    [[nodiscard]] int64_t evalConst(SourceCode& sourcecode) const override;
};
struct AccessExpr : LoadExpr {
    Token token1, token2;
    ExprHandle lhs;
    ExprHandle rhs;

    AccessExpr(Token token1, Token token2, ExprHandle lhs, ExprHandle rhs): token1(token1), token2(token2), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get(), rhs.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "[]"; }

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), token2);
    }

    [[nodiscard]] TypeReference evalType(ReferenceContext& context) const override;
};
struct InvokeExpr : Expr {
    Token token1, token2;
    ExprHandle lhs;
    std::vector<ExprHandle> rhs;

    InvokeExpr(Token token1, Token token2, ExprHandle lhs, std::vector<ExprHandle> rhs): token1(token1), token2(token2), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret{lhs.get()};
        for (auto&& e : rhs) ret.push_back(e.get());
        return ret;
    }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "()"; }

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), token2);
    }

    [[nodiscard]] TypeReference evalType(ReferenceContext& context) const override;
};
struct DotExpr : Expr {
    ExprHandle lhs;
    std::unique_ptr<IdExpr> rhs;

    DotExpr(ExprHandle lhs, std::unique_ptr<IdExpr> rhs): lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get(), rhs.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "."; }

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), rhs->segment());
    }

    [[nodiscard]] TypeReference evalType(ReferenceContext& context) const override;
};
struct AsExpr : Expr {
    Token token, token2;
    ExprHandle lhs;
    TypeReference T;

    AsExpr(Token token, Token token2, ExprHandle lhs, TypeReference T): token(token), token2(token2), lhs(std::move(lhs)), T(std::move(T)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get(), T.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "as"; }

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), token2);
    }

    [[nodiscard]] TypeReference evalType(ReferenceContext& context) const override;

    [[nodiscard]] int64_t evalConst(SourceCode& sourcecode) const override;
};
struct IsExpr : Expr {
    Token token, token2;
    ExprHandle lhs;
    TypeReference T;

    IsExpr(Token token, Token token2, ExprHandle lhs, TypeReference T): token(token), token2(token2), lhs(std::move(lhs)), T(std::move(T)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get(), T.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "is"; }

    [[nodiscard]] TypeReference evalType(ReferenceContext& context) const override;

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), token2);
    }

    [[nodiscard]] int64_t evalConst(SourceCode& sourcecode) const override;
};
struct DefaultExpr : Expr {
    Token token, token2;
    TypeReference T;

    DefaultExpr(Token token, Token token2, TypeReference T): token(token), token2(token2), T(std::move(T)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {T.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "default"; }

    [[nodiscard]] Segment segment() const override {
        return range(token, token2);
    }

    [[nodiscard]] TypeReference evalType(ReferenceContext& context) const override;

    [[nodiscard]] int64_t evalConst(SourceCode& sourcecode) const override;
};
struct TupleExpr : Expr {
    Token token1, token2;
    std::vector<ExprHandle> elements;

    TupleExpr(Token token1, Token token2, std::vector<ExprHandle> elements): token1(token1), token2(token2), elements(std::move(elements)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret;
        for (auto&& e : elements) ret.push_back(e.get());
        return ret;
    }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "()"; }

    [[nodiscard]] Segment segment() const override {
        return range(token1, token2);
    }

    [[nodiscard]] TypeReference evalType(ReferenceContext& context) const override;
};
struct ListExpr : Expr {
    Token token1, token2;
    std::vector<ExprHandle> elements;

    ListExpr(Token token1, Token token2, std::vector<ExprHandle> elements): token1(token1), token2(token2), elements(std::move(elements)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret;
        for (auto&& e : elements) ret.push_back(e.get());
        return ret;
    }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "[]"; }

    [[nodiscard]] Segment segment() const override {
        return range(token1, token2);
    }

    [[nodiscard]] TypeReference evalType(ReferenceContext& context) const override;
};
struct DictExpr : Expr {
    Token token1, token2;
    std::vector<std::pair<ExprHandle, ExprHandle>> elements;

    DictExpr(Token token1, Token token2, std::vector<std::pair<ExprHandle, ExprHandle>> elements): token1(token1), token2(token2), elements(std::move(elements)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret;
        for (auto&& [e1, e2] : elements) {
            ret.push_back(e1.get());
            ret.push_back(e2.get());
        }
        return ret;
    }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "@[]"; }

    [[nodiscard]] Segment segment() const override {
        return range(token1, token2);
    }

    [[nodiscard]] TypeReference evalType(ReferenceContext& context) const override;
};
struct ClauseExpr : Expr {
    Token token1, token2;
    std::vector<ExprHandle> lines;

    explicit ClauseExpr(Token token1, Token token2, std::vector<ExprHandle> lines = {}): token1(token1), token2(token2), lines(std::move(lines)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret;
        for (auto&& e : lines) ret.push_back(e.get());
        return ret;
    }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "{}"; }

    [[nodiscard]] Segment segment() const override {
        return range(token1, token2);
    }

    [[nodiscard]] TypeReference evalType(ReferenceContext& context) const override;

    [[nodiscard]] int64_t evalConst(SourceCode& sourcecode) const override;
};
struct IfElseExpr : Expr {
    Token token;
    ExprHandle cond;
    std::unique_ptr<ClauseExpr> lhs;
    ExprHandle rhs;

    IfElseExpr(Token token, ExprHandle cond, std::unique_ptr<ClauseExpr> lhs, ExprHandle rhs): token(token), cond(std::move(cond)), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {cond.get(), lhs.get(), rhs.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "if-else"; }

    [[nodiscard]] Segment segment() const override {
        return range(token, rhs->segment());
    }

    [[nodiscard]] TypeReference evalType(ReferenceContext& context) const override;

    [[nodiscard]] int64_t evalConst(SourceCode& sourcecode) const override;
};
struct LoopHook;
struct LoopExpr;
struct BreakExpr : Expr {
    Token token;
    std::shared_ptr<LoopHook> hook;

    explicit BreakExpr(Token token): token(token) {}

    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return sourcecode.source(token); }

    [[nodiscard]] Segment segment() const override {
        return token;
    }

    [[nodiscard]] TypeReference evalType(ReferenceContext& context) const override;
};
struct YieldExpr : Expr {
    Token token;
    ExprHandle rhs;
    std::shared_ptr<LoopHook> hook;

    YieldExpr(Token token, ExprHandle rhs): token(token), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {rhs.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return sourcecode.source(token); }

    [[nodiscard]] Segment segment() const override {
        return range(token, rhs->segment());
    }

    [[nodiscard]] TypeReference evalType(ReferenceContext& context) const override;
};
struct LoopHook {
    std::vector<const BreakExpr*> breaks;
    std::vector<const YieldExpr*> yields;
    const LoopExpr* loop;

    [[nodiscard]] TypeReference yield() const {
        if (yields.empty()) return ScalarTypes::NONE;
        auto type0 = yields.front()->typeCache;
        for (size_t i = 1; i < yields.size(); ++i) {
            auto type = yields[i]->typeCache;
            if (!type0->equals(type)) throw TypeException(mismatch(type, "loop's yields", i, type0), yields[i]->segment());
        }
        return listOf(type0);
    }
};
struct LoopExpr : Expr {
    Token token;
    ExprHandle clause;
    std::shared_ptr<LoopHook> hook;
    LoopExpr(Token token, ExprHandle clause, std::shared_ptr<LoopHook> hook): token(token), clause(std::move(clause)), hook(std::move(hook)) {
        this->hook->loop = this;
    }

    [[nodiscard]] Segment segment() const override {
        return range(token, clause->segment());
    }
};
struct WhileExpr : LoopExpr {
    ExprHandle cond;

    WhileExpr(Token token, ExprHandle cond, ExprHandle clause, std::shared_ptr<LoopHook> hook): cond(std::move(cond)), LoopExpr(token, std::move(clause), std::move(hook)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {cond.get(), clause.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "while"; }

    [[nodiscard]] TypeReference evalType(ReferenceContext& context) const override;
};
struct ForExpr : LoopExpr {
    std::unique_ptr<IdExpr> lhs;
    TypeReference designated;
    ExprHandle rhs;

    ForExpr(Token token, std::unique_ptr<IdExpr> lhs, TypeReference designated, ExprHandle rhs, ExprHandle clause, std::shared_ptr<LoopHook> hook): lhs(std::move(lhs)), designated(std::move(designated)), rhs(std::move(rhs)), LoopExpr(token, std::move(clause), std::move(hook)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get(), designated.get(), rhs.get(), clause.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "for"; }

    [[nodiscard]] TypeReference evalType(ReferenceContext& context) const override;
};
struct ForDestructuringExpr : LoopExpr {
    std::vector<std::unique_ptr<IdExpr>> lhs;
    std::vector<TypeReference> designated;
    ExprHandle rhs;

    ForDestructuringExpr(Token token, std::vector<std::unique_ptr<IdExpr>> lhs, std::vector<TypeReference> designated, ExprHandle rhs, ExprHandle clause, std::shared_ptr<LoopHook> hook): lhs(std::move(lhs)), designated(std::move(designated)), rhs(std::move(rhs)), LoopExpr(token, std::move(clause), std::move(hook)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret;
        for (auto&& e : lhs) ret.push_back(e.get());
        for (auto&& e : designated) ret.push_back(e.get());
        ret.push_back(rhs.get());
        ret.push_back(clause.get());
        return ret;
    }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "for"; }

    [[nodiscard]] TypeReference evalType(ReferenceContext& context) const override;
};
struct TryExpr : Expr {
    Token token;
    ExprHandle lhs;
    std::unique_ptr<IdExpr> except;
    ExprHandle rhs;

    TryExpr(Token token, ExprHandle lhs, std::unique_ptr<IdExpr> except, ExprHandle rhs): token(token), lhs(std::move(lhs)), except(std::move(except)), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get(), except.get(), rhs.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "try-catch"; }

    [[nodiscard]] Segment segment() const override {
        return range(token, rhs->segment());
    }

    [[nodiscard]] TypeReference evalType(ReferenceContext& context) const override;
};
struct FnJumpExpr : Expr { // return throw
    Token token;
    ExprHandle rhs;

    FnJumpExpr(Token token, ExprHandle rhs): token(token), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {rhs.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return sourcecode.source(token); }

    [[nodiscard]] Segment segment() const override {
        return range(token, rhs->segment());
    }

    [[nodiscard]] TypeReference evalType(ReferenceContext& context) const override;
};
struct FnExpr : Expr {
    Token token;
    std::vector<std::unique_ptr<IdExpr>> parameters;
    std::shared_ptr<FuncType> T;
    ExprHandle clause;
    std::vector<const FnJumpExpr*> returns;

    FnExpr(Token token, std::vector<std::unique_ptr<IdExpr>> parameters, std::shared_ptr<FuncType> T, ExprHandle clause, std::vector<const FnJumpExpr*> returns): token(token), parameters(std::move(parameters)), T(std::move(T)), clause(std::move(clause)), returns(std::move(returns)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret;
        for (auto&& e : parameters) ret.push_back(e.get());
        ret.push_back(T.get());
        ret.push_back(clause.get());
        return ret;
    }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "fn"; }

    [[nodiscard]] Segment segment() const override {
        return range(token, clause->segment());
    }

    [[nodiscard]] TypeReference evalType(ReferenceContext& context) const override;
};
struct LetExpr : Expr {
    Token token;
    std::unique_ptr<IdExpr> lhs;
    TypeReference designated;
    ExprHandle rhs;

    LetExpr(Token token, std::unique_ptr<IdExpr> lhs, TypeReference designated, ExprHandle rhs): token(token), lhs(std::move(lhs)), designated(std::move(designated)), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get(), designated.get(), rhs.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "let"; }

    [[nodiscard]] Segment segment() const override {
        return range(token, rhs->segment());
    }

    [[nodiscard]] TypeReference evalType(ReferenceContext& context) const override;
};
struct LetDestructuringExpr : Expr {
    Token token;
    std::vector<std::unique_ptr<IdExpr>> lhs;
    std::vector<TypeReference> designated;
    ExprHandle rhs;

    LetDestructuringExpr(Token token, std::vector<std::unique_ptr<IdExpr>> lhs, std::vector<TypeReference> designated, ExprHandle rhs): token(token), lhs(std::move(lhs)), designated(std::move(designated)), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret;
        for (auto&& e : lhs) ret.push_back(e.get());
        for (auto&& e : designated) ret.push_back(e.get());
        ret.push_back(rhs.get());
        return ret;
    }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "let"; }

    [[nodiscard]] Segment segment() const override {
        return range(token, rhs->segment());
    }

    [[nodiscard]] TypeReference evalType(ReferenceContext& context) const override;
};
}