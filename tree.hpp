#pragma once

#include "token.hpp"
#include "local.hpp"
#include "type.hpp"

namespace Porkchop {

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

    [[nodiscard]] virtual size_t evalConst() const;

    virtual void walkBytecode(Assembler* assembler) const = 0;
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

    [[nodiscard]] size_t evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct CharConstExpr : ConstExpr {
    char32_t parsed;

    CharConstExpr(Compiler& compiler, Token token);

    [[nodiscard]] TypeReference evalType() const override;

    [[nodiscard]] size_t evalConst() const override;

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

    [[nodiscard]] size_t evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct FloatConstExpr : ConstExpr {
    double parsed;

    FloatConstExpr(Compiler& compiler, Token token);

    [[nodiscard]] TypeReference evalType() const override;

    [[nodiscard]] size_t evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct LoadExpr : Expr {
    explicit LoadExpr(Compiler& compiler): Expr(compiler) {}

    virtual void walkStoreBytecode(Assembler* assembler) const = 0;

    virtual void checkLoadExpr() const {}
};

struct IdExpr : LoadExpr {
    Token token;
    LocalContext::LookupResult lookup;

    IdExpr(Compiler& compiler, Token token): LoadExpr(compiler), token(token) {}

    [[nodiscard]] std::string_view descriptor() const noexcept override { return compiler.of(token); }

    [[nodiscard]] Segment segment() const override {
        return token;
    }

    void initLookup(LocalContext& context) {
        lookup = context.lookup(token);
    }

    [[nodiscard]] TypeReference evalType() const override;

    [[nodiscard]] size_t evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;

    void walkStoreBytecode(Assembler* assembler) const override;
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

    [[nodiscard]] size_t evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct IdPrefixExpr : Expr {
    Token token;
    std::unique_ptr<LoadExpr> rhs;

    IdPrefixExpr(Compiler& compiler, Token token, std::unique_ptr<LoadExpr> rhs): Expr(compiler), token(token), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {rhs.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return compiler.of(token); }

    [[nodiscard]] Segment segment() const override {
        return range(token, rhs->segment());
    }

    [[nodiscard]] TypeReference evalType() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct IdPostfixExpr : Expr {
    Token token;
    std::unique_ptr<LoadExpr> lhs;

    IdPostfixExpr(Compiler& compiler, Token token, std::unique_ptr<LoadExpr> lhs): Expr(compiler), token(token), lhs(std::move(lhs)) {}

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

    [[nodiscard]] size_t evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct CompareExpr : InfixExprBase {
    CompareExpr(Compiler& compiler, Token token, ExprHandle lhs, ExprHandle rhs):
        InfixExprBase(compiler, token, std::move(lhs), std::move(rhs)) {}

    [[nodiscard]] TypeReference evalType() const override;

    [[nodiscard]] size_t evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct LogicalExpr : InfixExprBase {
    LogicalExpr(Compiler& compiler, Token token, ExprHandle lhs, ExprHandle rhs):
            InfixExprBase(compiler, token, std::move(lhs), std::move(rhs)) {}

    [[nodiscard]] TypeReference evalType() const override;

    [[nodiscard]] size_t evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct AssignExpr : Expr {
    Token token;
    std::unique_ptr<LoadExpr> lhs;
    ExprHandle rhs;

    AssignExpr(Compiler& compiler, Token token, std::unique_ptr<LoadExpr> lhs, ExprHandle rhs): Expr(compiler),
        token(token), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get(), rhs.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return compiler.of(token); }

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), rhs->segment());
    }

    [[nodiscard]] TypeReference evalType() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct AccessExpr : LoadExpr {
    Token token1, token2;
    ExprHandle lhs;
    ExprHandle rhs;

    AccessExpr(Compiler& compiler, Token token1, Token token2, ExprHandle lhs, ExprHandle rhs): LoadExpr(compiler),
        token1(token1), token2(token2), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {lhs.get(), rhs.get()}; }
    [[nodiscard]] std::string_view descriptor() const noexcept override { return "[]"; }

    [[nodiscard]] Segment segment() const override {
        return range(lhs->segment(), token2);
    }

    [[nodiscard]] TypeReference evalType() const override;

    void walkBytecode(Assembler* assembler) const override;

    void walkStoreBytecode(Assembler* assembler) const override;
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

    [[nodiscard]] size_t evalConst() const override;

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

    [[nodiscard]] size_t evalConst() const override;

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

    [[nodiscard]] size_t evalConst() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct TupleExpr : LoadExpr {
    Token token1, token2;
    std::vector<ExprHandle> elements;

    TupleExpr(Compiler& compiler, Token token1, Token token2, std::vector<ExprHandle> elements): LoadExpr(compiler),
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

    void checkLoadExpr() const override;
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

    [[nodiscard]] size_t evalConst() const override;

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

    [[nodiscard]] size_t evalConst() const override;

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
    [[nodiscard]] std::string_view descriptor() const noexcept override { return compiler.of(token); }

    [[nodiscard]] Segment segment() const override {
        return range(token, rhs->segment());
    }

    [[nodiscard]] TypeReference evalType() const override;

    void walkBytecode(Assembler* assembler) const override;
};

struct FnExprBase : Expr {
    Token token;
    std::shared_ptr<FuncType> prototype;
    size_t index;

    FnExprBase(Compiler& compiler, Token token, std::shared_ptr<FuncType> prototype): Expr(compiler), token(token), prototype(std::move(prototype)) {}

    [[nodiscard]] TypeReference evalType() const override;
};

struct NamedFnExpr : virtual FnExprBase {
    IdExprHandle name;

    explicit NamedFnExpr(IdExprHandle name): name(std::move(name)) {}

    [[nodiscard]] std::string_view descriptor() const noexcept override { return "fn"; }
};

struct DefinedFnExpr : virtual FnExprBase {
    std::vector<IdExprHandle> parameters;
    ExprHandle clause;
    std::vector<TypeReference> locals;

    DefinedFnExpr(std::vector<IdExprHandle> parameters, ExprHandle clause):
            parameters(std::move(parameters)), clause(std::move(clause)) {}


    [[nodiscard]] Segment segment() const override {
        return range(token, clause->segment());
    }
};

struct FnDeclExpr : NamedFnExpr {
    Token token2;

    FnDeclExpr(Compiler& compiler, Token token, Token token2, IdExprHandle name, std::shared_ptr<FuncType> prototype):
            FnExprBase(compiler, token, std::move(prototype)), NamedFnExpr(std::move(name)), token2(token2) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        return {name.get(), prototype.get()};
    }

    [[nodiscard]] Segment segment() const override {
        return range(token, token2);
    }

    void walkBytecode(Assembler* assembler) const override;
};

struct FnDefExpr : NamedFnExpr, DefinedFnExpr {

    FnDefExpr(Compiler& compiler, Token token, IdExprHandle name, std::vector<IdExprHandle> parameters, std::shared_ptr<FuncType> prototype, ExprHandle clause):
            FnExprBase(compiler, token, std::move(prototype)), NamedFnExpr(std::move(name)), DefinedFnExpr(std::move(parameters), std::move(clause)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret;
        ret.push_back(name.get());
        for (auto&& e : parameters) ret.push_back(e.get());
        ret.push_back(prototype.get());
        ret.push_back(clause.get());
        return ret;
    }

    void walkBytecode(Assembler* assembler) const override;
};

struct LambdaExpr : DefinedFnExpr {
    std::vector<IdExprHandle> captures;

    LambdaExpr(Compiler& compiler, Token token, std::vector<IdExprHandle> captures, std::vector<IdExprHandle> parameters, std::shared_ptr<FuncType> prototype, ExprHandle clause):
            FnExprBase(compiler, token, std::move(prototype)), captures(std::move(captures)), DefinedFnExpr(std::move(parameters), std::move(clause)) {}

    [[nodiscard]] std::vector<const Descriptor*> children() const override {
        std::vector<const Descriptor*> ret;
        for (auto&& e : captures) ret.push_back(e.get());
        for (auto&& e : parameters) ret.push_back(e.get());
        ret.push_back(prototype.get());
        ret.push_back(clause.get());
        return ret;
    }

    [[nodiscard]] std::string_view descriptor() const noexcept override { return "$"; }

    void walkBytecode(Assembler* assembler) const override;
};

struct Declarator : Descriptor {
    Segment segment;
    TypeReference typeCache;

    explicit Declarator(Segment segment): segment(segment) {}

    virtual void infer(TypeReference type) = 0;
    virtual void declare(LocalContext& context) const = 0;
    virtual void walkBytecode(Assembler* assembler) const = 0;
};

struct SimpleDeclarator : Declarator {
    IdExprHandle name;
    TypeReference designated;

    SimpleDeclarator(Segment segment, IdExprHandle name, TypeReference designated): Declarator(segment), name(std::move(name)), designated(std::move(designated)) {}

    [[nodiscard]] std::string_view descriptor() const noexcept override { return ":"; }
    [[nodiscard]] std::vector<const Descriptor*> children() const override { return {name.get(), designated.get()}; }

    void infer(TypeReference type) override;
    void declare(LocalContext &context) const override;
    void walkBytecode(Assembler *assembler) const override;
};

struct TupleDeclarator : Declarator  {
    std::vector<DeclaratorHandle> elements;

    TupleDeclarator(Segment segment, std::vector<DeclaratorHandle> elements): Declarator(segment), elements(std::move(elements)) {}

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

}
