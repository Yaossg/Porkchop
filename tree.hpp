#pragma once

#include "token.hpp"
#include "local.hpp"
#include "type.hpp"

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
    void initialize(LocalContext& context) {
        typeCache = evalType(context);
    }

    [[nodiscard]] virtual Segment segment() const = 0;

    [[nodiscard]] virtual TypeReference evalType(LocalContext& context) const = 0;

    [[nodiscard]] virtual int64_t evalConst(SourceCode& sourcecode) const;

    virtual void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const = 0;
};

struct ConstExpr : Expr {
    Token token;

    explicit ConstExpr(Token token): token(token) {}

    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return sourcecode.of(token); }

    [[nodiscard]] Segment segment() const override {
        return token;
    }
};

struct BoolConstExpr : ConstExpr {
    mutable bool parsed;

    explicit BoolConstExpr(Token token): ConstExpr(token) {}

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    [[nodiscard]] int64_t evalConst(SourceCode& sourcecode) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct CharConstExpr : ConstExpr {
    mutable char32_t parsed;

    explicit CharConstExpr(Token token): ConstExpr(token) {}

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct StringConstExpr : ConstExpr {
    mutable std::string parsed;

    explicit StringConstExpr(Token token): ConstExpr(token) {}

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct IntConstExpr : ConstExpr {
    mutable int64_t parsed;
    bool merged;

    explicit IntConstExpr(Token token, bool merged = false): ConstExpr(token), merged(merged) {}

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    [[nodiscard]] int64_t evalConst(SourceCode& sourcecode) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct FloatConstExpr : ConstExpr {
    mutable double parsed;

    explicit FloatConstExpr(Token token): ConstExpr(token) {}

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct LoadExpr : Expr {
    virtual void walkStoreBytecode(SourceCode& sourcecode, Assembler* assembler) const = 0;
};

struct IdExpr : LoadExpr {
    Token token;
    mutable LocalContext::LookupResult lookup;

    explicit IdExpr(Token token): token(token) {}

    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return sourcecode.of(token); }

    [[nodiscard]] Segment segment() const override {
        return token;
    }

    void initLookup(LocalContext& context) const {
        lookup = context.lookup(token);
    }

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;

    void walkStoreBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct PrefixExpr : Expr {
    Token token;
    ExprHandle rhs;

    PrefixExpr(Token token, ExprHandle rhs): token(token), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {rhs.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return sourcecode.of(token); }

    [[nodiscard]] Segment segment() const override {
        return range(token, rhs->segment());
    }

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    [[nodiscard]] int64_t evalConst(SourceCode& sourcecode) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct IdPrefixExpr : Expr {
    Token token;
    std::unique_ptr<LoadExpr> rhs;

    IdPrefixExpr(Token token, std::unique_ptr<LoadExpr> rhs): token(token), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {rhs.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return sourcecode.of(token); }

    [[nodiscard]] Segment segment() const override {
        return range(token, rhs->segment());
    }

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct IdPostfixExpr : Expr {
    Token token;
    std::unique_ptr<LoadExpr> lhs;

    IdPostfixExpr(Token token, std::unique_ptr<LoadExpr> lhs): token(token), lhs(std::move(lhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return sourcecode.of(token); }

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), token);
    }

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct InfixExpr : Expr {
    Token token;
    ExprHandle lhs;
    ExprHandle rhs;

    InfixExpr(Token token, ExprHandle lhs, ExprHandle rhs):
        token(token), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get(), rhs.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return sourcecode.of(token); }

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), rhs->segment());
    }

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    [[nodiscard]] int64_t evalConst(SourceCode& sourcecode) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct AssignExpr : Expr {
    Token token;
    std::unique_ptr<LoadExpr> lhs;
    ExprHandle rhs;

    AssignExpr(Token token, std::unique_ptr<LoadExpr> lhs, ExprHandle rhs):
        token(token), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get(), rhs.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return sourcecode.of(token); }

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), rhs->segment());
    }

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct LogicalExpr : Expr {
    Token token;
    ExprHandle lhs;
    ExprHandle rhs;

    LogicalExpr(Token token, ExprHandle lhs, ExprHandle rhs):
        token(token), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get(), rhs.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return sourcecode.of(token); }

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), rhs->segment());
    }

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    [[nodiscard]] int64_t evalConst(SourceCode& sourcecode) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct AccessExpr : LoadExpr {
    Token token1, token2;
    ExprHandle lhs;
    ExprHandle rhs;

    AccessExpr(Token token1, Token token2, ExprHandle lhs, ExprHandle rhs):
        token1(token1), token2(token2), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get(), rhs.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "[]"; }

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), token2);
    }

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;

    void walkStoreBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct InvokeExpr : Expr {
    Token token1, token2;
    ExprHandle lhs;
    std::vector<ExprHandle> rhs;

    InvokeExpr(Token token1, Token token2, ExprHandle lhs, std::vector<ExprHandle> rhs):
        token1(token1), token2(token2), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret{lhs.get()};
        for (auto&& e : rhs) ret.push_back(e.get());
        return ret;
    }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "()"; }

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), token2);
    }

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct DotExpr : Expr {
    ExprHandle lhs;
    IdExprHandle rhs;

    DotExpr(ExprHandle lhs, IdExprHandle rhs): lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get(), rhs.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "."; }

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), rhs->segment());
    }

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct AsExpr : Expr {
    Token token, token2;
    ExprHandle lhs;
    TypeReference T;

    AsExpr(Token token, Token token2, ExprHandle lhs, TypeReference T):
        token(token), token2(token2), lhs(std::move(lhs)), T(std::move(T)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get(), T.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "as"; }

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), token2);
    }

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    [[nodiscard]] int64_t evalConst(SourceCode& sourcecode) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct IsExpr : Expr {
    Token token, token2;
    ExprHandle lhs;
    TypeReference T;

    IsExpr(Token token, Token token2, ExprHandle lhs, TypeReference T):
        token(token), token2(token2), lhs(std::move(lhs)), T(std::move(T)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get(), T.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "is"; }

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), token2);
    }

    [[nodiscard]] int64_t evalConst(SourceCode& sourcecode) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct DefaultExpr : Expr {
    Token token, token2;
    TypeReference T;

    DefaultExpr(Token token, Token token2, TypeReference T):
        token(token), token2(token2), T(std::move(T)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {T.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "default"; }

    [[nodiscard]] Segment segment() const override {
        return range(token, token2);
    }

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    [[nodiscard]] int64_t evalConst(SourceCode& sourcecode) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct TupleExpr : Expr {
    Token token1, token2;
    std::vector<ExprHandle> elements;

    TupleExpr(Token token1, Token token2, std::vector<ExprHandle> elements):
        token1(token1), token2(token2), elements(std::move(elements)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret;
        for (auto&& e : elements) ret.push_back(e.get());
        return ret;
    }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "()"; }

    [[nodiscard]] Segment segment() const override {
        return range(token1, token2);
    }

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct ListExpr : Expr {
    Token token1, token2;
    std::vector<ExprHandle> elements;

    ListExpr(Token token1, Token token2, std::vector<ExprHandle> elements):
        token1(token1), token2(token2), elements(std::move(elements)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret;
        for (auto&& e : elements) ret.push_back(e.get());
        return ret;
    }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "[]"; }

    [[nodiscard]] Segment segment() const override {
        return range(token1, token2);
    }

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct SetExpr : Expr {
    Token token1, token2;
    std::vector<ExprHandle> elements;

    SetExpr(Token token1, Token token2, std::vector<ExprHandle> elements):
            token1(token1), token2(token2), elements(std::move(elements)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret;
        for (auto&& e : elements) ret.push_back(e.get());
        return ret;
    }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "@[]"; }

    [[nodiscard]] Segment segment() const override {
        return range(token1, token2);
    }

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct DictExpr : Expr {
    Token token1, token2;
    std::vector<std::pair<ExprHandle, ExprHandle>> elements;

    DictExpr(Token token1, Token token2, std::vector<std::pair<ExprHandle, ExprHandle>> elements):
        token1(token1), token2(token2), elements(std::move(elements)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret;
        for (auto&& [e1, e2] : elements) {
            ret.push_back(e1.get());
            ret.push_back(e2.get());
        }
        return ret;
    }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "@[:]"; }

    [[nodiscard]] Segment segment() const override {
        return range(token1, token2);
    }

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct ClauseExpr : Expr {
    Token token1, token2;
    std::vector<ExprHandle> lines;

    explicit ClauseExpr(Token token1, Token token2, std::vector<ExprHandle> lines = {}):
        token1(token1), token2(token2), lines(std::move(lines)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret;
        for (auto&& e : lines) ret.push_back(e.get());
        return ret;
    }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "{}"; }

    [[nodiscard]] Segment segment() const override {
        return range(token1, token2);
    }

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    [[nodiscard]] int64_t evalConst(SourceCode& sourcecode) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct IfElseExpr : Expr {
    Token token;
    ExprHandle cond;
    ExprHandle lhs;
    ExprHandle rhs;

    IfElseExpr(Token token, ExprHandle cond, ExprHandle lhs, ExprHandle rhs):
        token(token), cond(std::move(cond)), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {cond.get(), lhs.get(), rhs.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "if-else"; }

    [[nodiscard]] Segment segment() const override {
        return range(token, rhs->segment());
    }

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    [[nodiscard]] int64_t evalConst(SourceCode& sourcecode) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;

    static void walkBytecode(Expr* cond, Expr* lhs, Expr* rhs, SourceCode& sourcecode, Assembler* assembler);
};

struct LoopHook;
struct LoopExpr;

struct BreakExpr : Expr {
    Token token;
    std::shared_ptr<LoopHook> hook;

    explicit BreakExpr(Token token): token(token) {}

    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "break"; }

    [[nodiscard]] Segment segment() const override {
        return token;
    }

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct YieldExpr : Expr {
    Token token;
    ExprHandle rhs;

    YieldExpr(Token token, ExprHandle rhs): token(token), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {rhs.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "yield"; }

    [[nodiscard]] Segment segment() const override {
        return range(token, rhs->segment());
    }

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct LoopHook {
    std::vector<BreakExpr*> breaks;
    const LoopExpr* loop;
};

struct LoopExpr : Expr {
    Token token;
    ExprHandle clause;
    std::shared_ptr<LoopHook> hook;
    mutable size_t breakpoint;

    LoopExpr(Token token, ExprHandle clause, std::shared_ptr<LoopHook> hook): token(token), clause(std::move(clause)), hook(std::move(hook)) {
        this->hook->loop = this;
        for (auto&& e : this->hook->breaks) {
            e->hook = this->hook;
        }
    }

    [[nodiscard]] Segment segment() const override {
        return range(token, clause->segment());
    }
};

struct WhileExpr : LoopExpr {
    ExprHandle cond;

    WhileExpr(Token token, ExprHandle cond, ExprHandle clause, std::shared_ptr<LoopHook> hook):
        cond(std::move(cond)), LoopExpr(token, std::move(clause), std::move(hook)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {cond.get(), clause.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "while"; }

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct ForExpr : LoopExpr {
    IdExprHandle lhs;
    TypeReference designated;
    ExprHandle rhs;

    ForExpr(Token token, IdExprHandle lhs, TypeReference designated, ExprHandle rhs, ExprHandle clause, std::shared_ptr<LoopHook> hook):
        lhs(std::move(lhs)), designated(std::move(designated)), rhs(std::move(rhs)), LoopExpr(token, std::move(clause), std::move(hook)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get(), designated.get(), rhs.get(), clause.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "for"; }

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct ForDestructuringExpr : LoopExpr {
    std::vector<IdExprHandle> lhs;
    std::vector<TypeReference> designated;
    ExprHandle rhs;

    ForDestructuringExpr(Token token, std::vector<IdExprHandle> lhs, std::vector<TypeReference> designated, ExprHandle rhs, ExprHandle clause, std::shared_ptr<LoopHook> hook):
        lhs(std::move(lhs)), designated(std::move(designated)), rhs(std::move(rhs)), LoopExpr(token, std::move(clause), std::move(hook)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret;
        for (auto&& e : lhs) ret.push_back(e.get());
        for (auto&& e : designated) ret.push_back(e.get());
        ret.push_back(rhs.get());
        ret.push_back(clause.get());
        return ret;
    }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "for"; }

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct ReturnExpr : Expr {
    Token token;
    ExprHandle rhs;

    ReturnExpr(Token token, ExprHandle rhs): token(token), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {rhs.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return sourcecode.of(token); }

    [[nodiscard]] Segment segment() const override {
        return range(token, rhs->segment());
    }

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct FnExprBase : Expr {
    Token token;
    std::shared_ptr<FuncType> prototype;
    size_t index;
    FnExprBase(Token token, std::shared_ptr<FuncType> prototype): token(token), prototype(std::move(prototype)) {}

    [[nodiscard]] TypeReference evalType(LocalContext &context) const override;
};

struct NamedFnExpr : virtual FnExprBase {
    IdExprHandle name;

    explicit NamedFnExpr(IdExprHandle name): name(std::move(name)) {}

    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "fn"; }
};

struct DefinedFnExpr : virtual FnExprBase {
    std::vector<IdExprHandle> parameters;
    ExprHandle clause;
    size_t locals;

    DefinedFnExpr(std::vector<IdExprHandle> parameters, ExprHandle clause):
            parameters(std::move(parameters)), clause(std::move(clause)) {}


    [[nodiscard]] Segment segment() const override {
        return range(token, clause->segment());
    }
};

struct FnDeclExpr : NamedFnExpr {
    Token token2;

    FnDeclExpr(Token token, Token token2, IdExprHandle name, std::shared_ptr<FuncType> prototype):
            FnExprBase(token, std::move(prototype)), NamedFnExpr(std::move(name)), token2(token2) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        return {name.get(), prototype.get()};
    }

    [[nodiscard]] Segment segment() const override {
        return range(token, token2);
    }

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct FnDefExpr : NamedFnExpr, DefinedFnExpr {

    FnDefExpr(Token token, IdExprHandle name, std::vector<IdExprHandle> parameters, std::shared_ptr<FuncType> prototype, ExprHandle clause):
            FnExprBase(token, std::move(prototype)), NamedFnExpr(std::move(name)), DefinedFnExpr(std::move(parameters), std::move(clause)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret;
        ret.push_back(name.get());
        for (auto&& e : parameters) ret.push_back(e.get());
        ret.push_back(prototype.get());
        ret.push_back(clause.get());
        return ret;
    }

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct LambdaExpr : DefinedFnExpr {
    std::vector<IdExprHandle> captures;

    LambdaExpr(Token token, std::vector<IdExprHandle> captures, std::vector<IdExprHandle> parameters, std::shared_ptr<FuncType> prototype, ExprHandle clause):
            FnExprBase(token, std::move(prototype)), captures(std::move(captures)), DefinedFnExpr(std::move(parameters), std::move(clause)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret;
        for (auto&& e : captures) ret.push_back(e.get());
        for (auto&& e : parameters) ret.push_back(e.get());
        ret.push_back(prototype.get());
        ret.push_back(clause.get());
        return ret;
    }

    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "$"; }

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct LetExpr : Expr {
    Token token;
    IdExprHandle lhs;
    TypeReference designated;
    ExprHandle rhs;

    LetExpr(Token token, IdExprHandle lhs, TypeReference designated, ExprHandle rhs):
        token(token), lhs(std::move(lhs)), designated(std::move(designated)), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get(), designated.get(), rhs.get()}; }
    [[nodiscard]] std::string_view descriptor(const SourceCode &sourcecode) const noexcept override { return "let"; }

    [[nodiscard]] Segment segment() const override {
        return range(token, rhs->segment());
    }

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

struct LetDestructuringExpr : Expr {
    Token token;
    std::vector<IdExprHandle> lhs;
    std::vector<TypeReference> designated;
    ExprHandle rhs;

    LetDestructuringExpr(Token token, std::vector<IdExprHandle> lhs, std::vector<TypeReference> designated, ExprHandle rhs):
        token(token), lhs(std::move(lhs)), designated(std::move(designated)), rhs(std::move(rhs)) {}

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

    [[nodiscard]] TypeReference evalType(LocalContext& context) const override;

    void walkBytecode(SourceCode& sourcecode, Assembler* assembler) const override;
};

}