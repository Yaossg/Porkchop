#pragma once

#include "tree.hpp"
#include "assembler.hpp"

namespace Porkchop {

struct Function {
    virtual ~Function() = default;
    [[nodiscard]] virtual TypeReference prototype() const = 0;
    virtual void compile(Compiler& compiler, Assembler* assembler) const = 0;
};

struct NamedFunction : Function {
    FnDeclExpr* decl;
    FnDefExpr* def;
    void compile(Compiler& compiler, Assembler* assembler) const override {
        assembler->beginFunction();
        assembler->indexed(Opcode::LOCAL, def->locals);
        def->clause->walkBytecode(compiler, assembler);
        assembler->opcode(Opcode::RETURN);
        assembler->endFunction();
    }

    [[nodiscard]] TypeReference prototype() const override {
        return def != nullptr ? def->prototype : decl->prototype;
    }
};

struct LambdaFunction : Function {
    LambdaExpr* lambda;
    void compile(Compiler& compiler, Assembler* assembler) const override {
        assembler->beginFunction();
        assembler->indexed(Opcode::LOCAL, lambda->locals);
        lambda->clause->walkBytecode(compiler, assembler);
        assembler->opcode(Opcode::RETURN);
        assembler->endFunction();
    }

    [[nodiscard]] TypeReference prototype() const override {
        return lambda->prototype;
    }
};

struct ExternalFunction : Function {
    TypeReference type;
    void compile(Compiler& compiler, Assembler* assembler) const override {}

    [[nodiscard]] TypeReference prototype() const override {
        return type;
    }
};

}