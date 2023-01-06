#pragma once

#include "local.hpp"

namespace Porkchop {

struct Expr;
struct IdExpr;
using ExprHandle = std::unique_ptr<Expr>;
using IdExprHandle = std::unique_ptr<IdExpr>;
struct Declarator;
using DeclaratorHandle = std::unique_ptr<Declarator>;

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

    Compiler& compiler;

    explicit Expr(Compiler& compiler): compiler(compiler) {}

    TypeReference typeCache;
    void initialize() {
        typeCache = evalType();
    }

    [[nodiscard]] virtual Segment segment() const = 0;

    [[nodiscard]] virtual TypeReference evalType() const = 0;

    [[nodiscard]] virtual $union evalConst() const;

    virtual void walkBytecode(Assembler* assembler) const = 0;

    void expect(TypeReference const& expected) const;

    void expect(bool pred(TypeReference const&), const char* expected) const;

    [[noreturn]] void expect(const char* expected) const;

    void assignable(TypeReference const& expected) const;

    void match(TypeReference const& expected, const char *msg) const;

    void neverGonnaGiveYouUp(const char* msg) const;
};

struct ConstExpr : Expr {
    Token token;

    ConstExpr(Compiler& compiler, Token token): Expr(compiler), token(token) {}

    [[nodiscard]] std::string_view descriptor() const noexcept override { return compiler.of(token); }

    [[nodiscard]] Segment segment() const override {
        return token;
    }
};

struct BoolConstExpr : ConstExpr {
    bool parsed;

    BoolConstExpr(Compiler& compiler, Token token);

    [[nodiscard]] TypeReference evalType() const override;

    [[nodiscard]] $union evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct CharConstExpr : ConstExpr {
    char32_t parsed;

    CharConstExpr(Compiler& compiler, Token token);

    [[nodiscard]] TypeReference evalType() const override;

    [[nodiscard]] $union evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct StringConstExpr : ConstExpr {
    std::string parsed;

    StringConstExpr(Compiler& compiler, Token token);

    [[nodiscard]] TypeReference evalType() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct IntConstExpr : ConstExpr {
    int64_t parsed;
    bool merged;

    IntConstExpr(Compiler& compiler, Token token, bool merged = false);

    [[nodiscard]] TypeReference evalType() const override;

    [[nodiscard]] $union evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct FloatConstExpr : ConstExpr {
    double parsed;

    FloatConstExpr(Compiler& compiler, Token token);

    [[nodiscard]] TypeReference evalType() const override;

    [[nodiscard]] $union evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct AssignableExpr : Expr {
    explicit AssignableExpr(Compiler& compiler): Expr(compiler) {}

    virtual void walkStoreBytecode(Assembler* assembler) const = 0;

    virtual void ensureAssignable() const = 0;
};

struct IdExpr : AssignableExpr {
    Token token;
    LocalContext::LookupResult lookup;

    IdExpr(Compiler& compiler, Token token): AssignableExpr(compiler), token(token) {}

    [[nodiscard]] std::string_view descriptor() const noexcept override { return compiler.of(token); }

    [[nodiscard]] Segment segment() const override {
        return token;
    }

    void initLookup(LocalContext& context) {
        lookup = context.lookup(compiler, token);
    }

    [[nodiscard]] TypeReference evalType() const override;

    [[nodiscard]] $union evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;

    void walkStoreBytecode(Assembler* assembler) const override;

    void ensureAssignable() const override;
};

struct PrefixExpr : Expr {
    Token token;
    ExprHandle rhs;

    PrefixExpr(Compiler& compiler, Token token, ExprHandle rhs): Expr(compiler), token(token), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {rhs.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return compiler.of(token); }

    [[nodiscard]] Segment segment() const override {
        return range(token, rhs->segment());
    }

    [[nodiscard]] TypeReference evalType() const override;

    [[nodiscard]] $union evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct StatefulPrefixExpr : Expr {
    Token token;
    std::unique_ptr<AssignableExpr> rhs;

    StatefulPrefixExpr(Compiler& compiler, Token token, std::unique_ptr<AssignableExpr> rhs): Expr(compiler), token(token), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {rhs.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return compiler.of(token); }

    [[nodiscard]] Segment segment() const override {
        return range(token, rhs->segment());
    }

    [[nodiscard]] TypeReference evalType() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct StatefulPostfixExpr : Expr {
    Token token;
    std::unique_ptr<AssignableExpr> lhs;

    StatefulPostfixExpr(Compiler& compiler, Token token, std::unique_ptr<AssignableExpr> lhs): Expr(compiler), token(token), lhs(std::move(lhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return compiler.of(token); }

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), token);
    }

    [[nodiscard]] TypeReference evalType() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct InfixExprBase : Expr {
    Token token;
    ExprHandle lhs;
    ExprHandle rhs;

    InfixExprBase(Compiler& compiler, Token token, ExprHandle lhs, ExprHandle rhs): Expr(compiler),
        token(token), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get(), rhs.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return compiler.of(token); }

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), rhs->segment());
    }
};

struct InfixExpr : InfixExprBase {
    InfixExpr(Compiler& compiler, Token token, ExprHandle lhs, ExprHandle rhs):
        InfixExprBase(compiler, token, std::move(lhs), std::move(rhs)) {}

    [[nodiscard]] TypeReference evalType() const override;

    [[nodiscard]] $union evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct CompareExpr : InfixExprBase {
    CompareExpr(Compiler& compiler, Token token, ExprHandle lhs, ExprHandle rhs):
        InfixExprBase(compiler, token, std::move(lhs), std::move(rhs)) {}

    [[nodiscard]] TypeReference evalType() const override;

    [[nodiscard]] $union evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct LogicalExpr : InfixExprBase {
    LogicalExpr(Compiler& compiler, Token token, ExprHandle lhs, ExprHandle rhs):
        InfixExprBase(compiler, token, std::move(lhs), std::move(rhs)) {}

    [[nodiscard]] TypeReference evalType() const override;

    [[nodiscard]] $union evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct InExpr : InfixExprBase {
    InExpr(Compiler& compiler, Token token, ExprHandle lhs, ExprHandle rhs):
        InfixExprBase(compiler, token, std::move(lhs), std::move(rhs)) {}

    [[nodiscard]] TypeReference evalType() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct AssignExpr : Expr {
    Token token;
    std::unique_ptr<AssignableExpr> lhs;
    ExprHandle rhs;

    AssignExpr(Compiler& compiler, Token token, std::unique_ptr<AssignableExpr> lhs, ExprHandle rhs): Expr(compiler),
        token(token), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get(), rhs.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return compiler.of(token); }

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), rhs->segment());
    }

    [[nodiscard]] TypeReference evalType() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct AccessExpr : AssignableExpr {
    Token token1, token2;
    ExprHandle lhs;
    ExprHandle rhs;

    AccessExpr(Compiler& compiler, Token token1, Token token2, ExprHandle lhs, ExprHandle rhs): AssignableExpr(compiler),
        token1(token1), token2(token2), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get(), rhs.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "[]"; }

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), token2);
    }

    [[nodiscard]] TypeReference evalType() const override;

    void walkBytecode(Assembler* assembler) const override;

    void walkStoreBytecode(Assembler* assembler) const override;

    void ensureAssignable() const override;
};

struct InvokeExpr : Expr {
    Token token1, token2;
    ExprHandle lhs;
    std::vector<ExprHandle> rhs;

    InvokeExpr(Compiler& compiler, Token token1, Token token2, ExprHandle lhs, std::vector<ExprHandle> rhs): Expr(compiler),
        token1(token1), token2(token2), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret{lhs.get()};
        for (auto&& e : rhs) ret.push_back(e.get());
        return ret;
    }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "()"; }

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), token2);
    }

    [[nodiscard]] TypeReference evalType() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct DotExpr : Expr {
    ExprHandle lhs;
    IdExprHandle rhs;

    DotExpr(Compiler& compiler, ExprHandle lhs, IdExprHandle rhs): Expr(compiler), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get(), rhs.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "."; }

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), rhs->segment());
    }

    [[nodiscard]] TypeReference evalType() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct AsExpr : Expr {
    Token token, token2;
    ExprHandle lhs;
    TypeReference T;

    AsExpr(Compiler& compiler, Token token, Token token2, ExprHandle lhs, TypeReference T): Expr(compiler),
        token(token), token2(token2), lhs(std::move(lhs)), T(std::move(T)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get(), T.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "as"; }

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), token2);
    }

    [[nodiscard]] TypeReference evalType() const override;

    [[nodiscard]] $union evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct IsExpr : Expr {
    Token token, token2;
    ExprHandle lhs;
    TypeReference T;

    IsExpr(Compiler& compiler, Token token, Token token2, ExprHandle lhs, TypeReference T): Expr(compiler),
        token(token), token2(token2), lhs(std::move(lhs)), T(std::move(T)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get(), T.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "is"; }

    [[nodiscard]] TypeReference evalType() const override;

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), token2);
    }

    [[nodiscard]] $union evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct DefaultExpr : Expr {
    Token token, token2;
    TypeReference T;

    DefaultExpr(Compiler& compiler, Token token, Token token2, TypeReference T): Expr(compiler),
        token(token), token2(token2), T(std::move(T)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {T.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "default"; }

    [[nodiscard]] Segment segment() const override {
        return range(token, token2);
    }

    [[nodiscard]] TypeReference evalType() const override;

    [[nodiscard]] $union evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct TupleExpr : AssignableExpr {
    Token token1, token2;
    std::vector<ExprHandle> elements;

    TupleExpr(Compiler& compiler, Token token1, Token token2, std::vector<ExprHandle> elements): AssignableExpr(compiler),
        token1(token1), token2(token2), elements(std::move(elements)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret;
        for (auto&& e : elements) ret.push_back(e.get());
        return ret;
    }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "()"; }

    [[nodiscard]] Segment segment() const override {
        return range(token1, token2);
    }

    [[nodiscard]] TypeReference evalType() const override;

    void walkBytecode(Assembler* assembler) const override;

    void walkStoreBytecode(Assembler* assembler) const override;

    void ensureAssignable() const override;
};

struct ListExpr : Expr {
    Token token1, token2;
    std::vector<ExprHandle> elements;

    ListExpr(Compiler& compiler, Token token1, Token token2, std::vector<ExprHandle> elements): Expr(compiler),
        token1(token1), token2(token2), elements(std::move(elements)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret;
        for (auto&& e : elements) ret.push_back(e.get());
        return ret;
    }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "[]"; }

    [[nodiscard]] Segment segment() const override {
        return range(token1, token2);
    }

    [[nodiscard]] TypeReference evalType() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct SetExpr : Expr {
    Token token1, token2;
    std::vector<ExprHandle> elements;

    SetExpr(Compiler& compiler, Token token1, Token token2, std::vector<ExprHandle> elements): Expr(compiler),
            token1(token1), token2(token2), elements(std::move(elements)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret;
        for (auto&& e : elements) ret.push_back(e.get());
        return ret;
    }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "@[]"; }

    [[nodiscard]] Segment segment() const override {
        return range(token1, token2);
    }

    [[nodiscard]] TypeReference evalType() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct DictExpr : Expr {
    Token token1, token2;
    std::vector<std::pair<ExprHandle, ExprHandle>> elements;

    DictExpr(Compiler& compiler, Token token1, Token token2, std::vector<std::pair<ExprHandle, ExprHandle>> elements): Expr(compiler),
        token1(token1), token2(token2), elements(std::move(elements)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret;
        for (auto&& [e1, e2] : elements) {
            ret.push_back(e1.get());
            ret.push_back(e2.get());
        }
        return ret;
    }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "@[:]"; }

    [[nodiscard]] Segment segment() const override {
        return range(token1, token2);
    }

    [[nodiscard]] TypeReference evalType() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct ClauseExpr : Expr {
    Token token1, token2;
    std::vector<ExprHandle> lines;

    ClauseExpr(Compiler& compiler, Token token1, Token token2, std::vector<ExprHandle> lines = {}): Expr(compiler),
        token1(token1), token2(token2), lines(std::move(lines)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret;
        for (auto&& e : lines) ret.push_back(e.get());
        return ret;
    }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "{}"; }

    [[nodiscard]] Segment segment() const override {
        return range(token1, token2);
    }

    [[nodiscard]] TypeReference evalType() const override;

    [[nodiscard]] $union evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct IfElseExpr : Expr {
    Token token;
    ExprHandle cond;
    ExprHandle lhs;
    ExprHandle rhs;

    IfElseExpr(Compiler& compiler, Token token, ExprHandle cond, ExprHandle lhs, ExprHandle rhs): Expr(compiler),
        token(token), cond(std::move(cond)), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {cond.get(), lhs.get(), rhs.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "if-else"; }

    [[nodiscard]] Segment segment() const override {
        return range(token, rhs->segment());
    }

    [[nodiscard]] TypeReference evalType() const override;

    [[nodiscard]] $union evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;

    static void walkBytecode(Expr const* cond, Expr const* lhs, Expr const* rhs, Compiler& compiler, Assembler* assembler);
};

struct LoopHook;
struct LoopExpr;

struct BreakExpr : Expr {
    Token token;
    std::shared_ptr<LoopHook> hook;

    BreakExpr(Compiler& compiler, Token token): Expr(compiler), token(token) {}

    [[nodiscard]] std::string_view descriptor() const noexcept override { return "break"; }

    [[nodiscard]] Segment segment() const override {
        return token;
    }

    [[nodiscard]] TypeReference evalType() const override;

    void walkBytecode(Assembler* assembler) const override;
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

    LoopExpr(Compiler& compiler, Token token, ExprHandle clause, std::shared_ptr<LoopHook> hook):
        Expr(compiler), token(token), clause(std::move(clause)), hook(std::move(hook)) {
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

    WhileExpr(Compiler& compiler, Token token, ExprHandle cond, ExprHandle clause, std::shared_ptr<LoopHook> hook):
        cond(std::move(cond)), LoopExpr(compiler, token, std::move(clause), std::move(hook)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {cond.get(), clause.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "while"; }

    [[nodiscard]] TypeReference evalType() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct ReturnExpr : Expr {
    Token token;
    ExprHandle rhs;

    ReturnExpr(Compiler& compiler, Token token, ExprHandle rhs): Expr(compiler), token(token), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {rhs.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "return"; }

    [[nodiscard]] Segment segment() const override {
        return range(token, rhs->segment());
    }

    [[nodiscard]] TypeReference evalType() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct ParameterList : Descriptor {
    std::vector<IdExprHandle> identifiers;
    std::shared_ptr<FuncType> prototype;

    ParameterList(std::vector<IdExprHandle> identifiers, std::shared_ptr<FuncType> prototype)
            : identifiers(std::move(identifiers)), prototype(std::move(prototype)) {}

    [[nodiscard]] std::string_view descriptor() const noexcept override { return "()"; }
    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret;
        for (auto&& e : identifiers) ret.push_back(e.get());
        ret.push_back(prototype.get());
        return ret;
    }

    void declare(Compiler* compiler, LocalContext& context) {
        for (size_t i = 0; i < identifiers.size(); ++i) {
            context.local(compiler->of(identifiers[i]->token), prototype->P[i]);
        }
    }
};

struct FunctionDefinition : Descriptor {
    bool yield;
    ExprHandle clause;
    std::vector<TypeReference> locals;

    FunctionDefinition(bool yield, ExprHandle clause, std::vector<TypeReference> locals)
            : yield(yield), clause(std::move(clause)), locals(std::move(locals)) {}

    [[nodiscard]] std::string_view descriptor() const noexcept override { return yield ? "yield" : "=" ; }
    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {clause.get()}; }
};

struct FnExprBase : Expr {
    Token token;
    std::unique_ptr<ParameterList> parameters;
    size_t index;

    FnExprBase(Compiler& compiler, Token token, std::unique_ptr<ParameterList> parameters): Expr(compiler), token(token), parameters(std::move(parameters)) {}

    [[nodiscard]] TypeReference evalType() const override;
};

struct FnDeclExpr : FnExprBase {
    Token token2;
    IdExprHandle name;

    FnDeclExpr(Compiler& compiler, Token token, Token token2, IdExprHandle name, std::unique_ptr<ParameterList> parameters):
            FnExprBase(compiler, token, std::move(parameters)), name(std::move(name)), token2(token2) {}

    [[nodiscard]] std::string_view descriptor() const noexcept override { return "fn"; }
    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        return {name.get(), parameters.get()};
    }

    [[nodiscard]] Segment segment() const override {
        return range(token, token2);
    }

    void walkBytecode(Assembler* assembler) const override;
};

struct FnDefExpr : FnDeclExpr {
    std::unique_ptr<FunctionDefinition> definition;

    FnDefExpr(Compiler& compiler, Token token, Token token2, IdExprHandle name, std::unique_ptr<ParameterList> parameters):
            FnDeclExpr(compiler, token, token2, std::move(name), std::move(parameters)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        return {name.get(), parameters.get(), definition.get()};
    }

    [[nodiscard]] Segment segment() const override {
        return range(token, definition->clause->segment());
    }

    void walkBytecode(Assembler* assembler) const override;
};

struct LambdaExpr : FnExprBase {
    std::vector<IdExprHandle> captures;
    std::unique_ptr<FunctionDefinition> definition;

    LambdaExpr(Compiler& compiler, Token token, std::vector<IdExprHandle> captures, std::unique_ptr<ParameterList> parameters,
               std::unique_ptr<FunctionDefinition> definition):
            FnExprBase(compiler, token, std::move(parameters)), captures(std::move(captures)), definition(std::move(definition)) {}


    [[nodiscard]] std::string_view descriptor() const noexcept override { return "$"; }
    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret;
        for (auto&& e : captures) ret.push_back(e.get());
        ret.push_back(parameters.get());
        ret.push_back(definition.get());
        return ret;
    }

    [[nodiscard]] Segment segment() const override {
        return range(token, definition->clause->segment());
    }

    void walkBytecode(Assembler* assembler) const override;
};

struct Declarator : Descriptor {
    Compiler& compiler;
    Segment segment;
    TypeReference typeCache;

    explicit Declarator(Compiler& compiler, Segment segment): compiler(compiler), segment(segment) {}

    virtual void infer(TypeReference type) = 0;
    virtual void declare(LocalContext& context) const = 0;
    virtual void walkBytecode(Assembler* assembler) const = 0;
};

struct SimpleDeclarator : Declarator {
    IdExprHandle name;
    TypeReference designated;

    SimpleDeclarator(Compiler& compiler, Segment segment, IdExprHandle name, TypeReference designated): Declarator(compiler, segment), name(std::move(name)), designated(std::move(designated)) {}

    [[nodiscard]] std::string_view descriptor() const noexcept override { return ":"; }
    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {name.get(), designated.get()}; }

    void infer(TypeReference type) override;
    void declare(LocalContext &context) const override;
    void walkBytecode(Assembler *assembler) const override;
};

struct TupleDeclarator : Declarator  {
    std::vector<DeclaratorHandle> elements;

    TupleDeclarator(Compiler& compiler, Segment segment, std::vector<DeclaratorHandle> elements): Declarator(compiler, segment), elements(std::move(elements)) {}

    [[nodiscard]] std::string_view descriptor() const noexcept override { return "()"; }
    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret;
        for (auto&& e : elements) ret.push_back(e.get());
        return ret;
    }

    void infer(TypeReference type) override;
    void declare(LocalContext &context) const override;
    void walkBytecode(Assembler *assembler) const override;
};

struct LetExpr : Expr {
    Token token;
    DeclaratorHandle declarator;
    ExprHandle initializer;

    LetExpr(Compiler& compiler, Token token, DeclaratorHandle declarator, ExprHandle initializer): Expr(compiler),
           token(token), declarator(std::move(declarator)), initializer(std::move(initializer)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {declarator.get(), initializer.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "let"; }

    [[nodiscard]] Segment segment() const override {
        return range(token, initializer->segment());
    }

    [[nodiscard]] TypeReference evalType() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct ForExpr : LoopExpr {
    DeclaratorHandle declarator;
    ExprHandle initializer;

    ForExpr(Compiler& compiler, Token token, DeclaratorHandle declarator, ExprHandle initializer, ExprHandle clause, std::shared_ptr<LoopHook> hook):
            declarator(std::move(declarator)), initializer(std::move(initializer)), LoopExpr(compiler, token, std::move(clause), std::move(hook)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {declarator.get(), initializer.get(), clause.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "for"; }

    [[nodiscard]] TypeReference evalType() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct YieldReturnExpr : Expr {
    Token token1, token2;
    ExprHandle rhs;

    YieldReturnExpr(Compiler& compiler, Token token1, Token token2, ExprHandle rhs): Expr(compiler), token1(token1), token2(token2), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {rhs.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "yield return"; }

    [[nodiscard]] Segment segment() const override {
        return range(token1, rhs->segment());
    }

    [[nodiscard]] TypeReference evalType() const override;

    void walkBytecode(Assembler* assembler) const override;
};


struct YieldBreakExpr : Expr {
    Token token1, token2;

    YieldBreakExpr(Compiler& compiler, Token token1, Token token2): Expr(compiler), token1(token1), token2(token2) {}

    [[nodiscard]] std::string_view descriptor() const noexcept override { return "yield break"; }

    [[nodiscard]] Segment segment() const override {
        return range(token1, token2);
    }

    [[nodiscard]] TypeReference evalType() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct InterpolationExpr : Expr {
    Token token1, token2;
    std::vector<std::unique_ptr<StringConstExpr>> literals;
    std::vector<ExprHandle> elements;

    InterpolationExpr(Compiler& compiler, Token token1, Token token2,
                      std::vector<std::unique_ptr<StringConstExpr>> literals, std::vector<ExprHandle> elements)
                      : Expr(compiler), token1(token1), token2(token2), literals(std::move(literals)), elements(std::move(elements)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret;
        for (size_t i = 0; i < elements.size(); ++i) {
            ret.push_back(literals[i].get());
            ret.push_back(elements[i].get());
        }
        ret.push_back(literals.back().get());
        return ret;
    }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "\"${}\""; }

    [[nodiscard]] Segment segment() const override {
        return range(token1, token2);
    }

    [[nodiscard]] TypeReference evalType() const override;

    void walkBytecode(Assembler* assembler) const override;
};


}
