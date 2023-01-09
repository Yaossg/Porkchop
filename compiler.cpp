#include "parser.hpp"
#include "function.hpp"
#include "compiler.hpp"

namespace Porkchop {

Compiler::Compiler(Continuum* continuum, Source source)
        : continuum(continuum), source(std::move(source)) {}

std::string_view Compiler::of(Token token) const noexcept {
    return source.of(token);
}

void Compiler::parse(bool free) {
    if (source.tokens.empty())
        Error().with(ErrorMessage().fatal().text("no token to compile")).raise();
    Parser parser(this, source.tokens.begin(), source.tokens.end(), *continuum->context);
    auto F = std::make_shared<FuncType>(std::vector<TypeReference>{}, nullptr);
    auto tree = parser.parseFnBody(F, false, source.tokens.front());
    if (auto token = parser.next(); token.type != TokenType::LINEBREAK)
        raise("a final linebreak is expected", token);
    if (parser.remains())
        raise("unterminated tokens", parser.peek());
    definition = std::make_unique<FunctionDefinition>(false, std::move(tree), parser.context.localTypes);
    auto main = std::make_unique<MainFunctionReference>(continuum, definition.get(), std::move(F));
    if (auto R = main->prototype()->R; !free && !isNone(R) && !isInt(R)) {
        parser.raiseReturns(definition->clause.get(),
                            ErrorMessage().error(source.tokens.front()).text("main clause should return either").type(ScalarTypes::NONE).text("or").type(ScalarTypes::INT));
    }
    continuum->functions.push_back(std::move(main));
}

void Compiler::compile(Assembler* assembler) const {
    continuum->compile(assembler);
}

std::string Compiler::descriptor() const {
    return definition->clause->walkDescriptor();
}

}