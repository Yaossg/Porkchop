#include "parser.hpp"
#include "function.hpp"
#include "compiler.hpp"

namespace Porkchop {

Compiler::Compiler(Continuum* continuum, Source source)
        : continuum(continuum), source(std::move(source)) {}

std::string_view Compiler::of(Token token) const noexcept {
    return source.of(token);
}

void Compiler::parse(Mode mode) {
    if (source.tokens.empty())
        Error().with(ErrorMessage().fatal().text("no token to compile")).raise();
    if (!source.greedy.empty()) {
        Error error;
        error.with(ErrorMessage().fatal().text("greedy tokens mismatch, source seems incomplete"));
        for (auto&& token : source.greedy) {
            const char* msg;
            switch (token.type) {
                case TokenType::LPAREN: msg = ")"; break;
                case TokenType::LBRACKET: msg = "]"; break;
                case TokenType::LBRACE: msg = "}"; break;
                default: unreachable();
            }
            error.with(ErrorMessage().note(token).quote(msg).text("is expected to match this"));
        }
        error.raise();
    }
    std::unique_ptr<LocalContext> subcontext;
    if (mode == Mode::EVAL) {
        subcontext = std::make_unique<LocalContext>(continuum, continuum->context.get());
        subcontext->local("context", ScalarTypes::ANY);
    }
    std::unique_ptr<LocalContext>& reference = mode == Mode::EVAL ? subcontext : continuum->context;
    Parser parser(*this, source.tokens.begin(), source.tokens.end(), *reference);
    auto F = std::make_shared<FuncType>(std::vector<TypeReference>{}, nullptr);
    auto tree = parser.parseFnBody(F, false, source.tokens.front());
    if (auto token = parser.next(); token.type != TokenType::LINEBREAK)
        raise("a final linebreak is expected", token);
    if (parser.remains())
        raise("unterminated tokens", parser.peek());
    definition = std::make_unique<FunctionDefinition>(false, std::move(tree), parser.context.localTypes);
    if (mode == Mode::EVAL) {
        continuum->functions.push_back(std::make_unique<EvalFunctionReference>(definition.get(), std::move(F)));
    } else {
        auto main = std::make_unique<MainFunctionReference>(continuum, definition.get(), std::move(F));
        if (auto R = main->prototype()->R; mode == Mode::MAIN && !isNone(R) && !isInt(R)) {
            parser.raiseReturns(definition->clause.get(),
                                ErrorMessage().error(source.tokens.front()).text("main clause should return either").type(ScalarTypes::NONE).text("or").type(ScalarTypes::INT));
        }
        continuum->functions.push_back(std::move(main));
    }
}

void Compiler::compile(Assembler* assembler) const {
    continuum->compile(assembler);
}

std::string Compiler::descriptor() const {
    return definition->clause->walkDescriptor();
}

}