#include "parser.hpp"
#include "function.hpp"
#include "compiler.hpp"

namespace Porkchop {

Compiler::Compiler(Continuum* continuum, Source source)
        : continuum(continuum), source(std::move(source)) {}

std::string_view Compiler::of(Token token) const noexcept {
    return source.of(token);
}

void Compiler::parse() {
    Parser parser(this, source.tokens.begin(), source.tokens.end(), *continuum->context);
    auto F = std::make_shared<FuncType>(std::vector<TypeReference>{}, nullptr);
    auto tree = parser.parseFnBody(F, false);
    parser.expect(TokenType::LINEBREAK, "a final linebreak is expected");
    if (parser.remains())
        throw ParserException("unterminated tokens", parser.peek());
    definition = std::make_unique<FunctionDefinition>(false, std::move(tree), parser.context.localTypes);
    continuum->functions.push_back(std::make_unique<MainFunctionReference>(continuum, definition.get(), std::move(F)));
}

void Compiler::compile(Assembler* assembler) const {
    continuum->compile(assembler);
}

std::string Compiler::descriptor() const {
    return definition->clause->walkDescriptor();
}

}