#include "parser.hpp"
#include "function.hpp"

namespace Porkchop {

Compiler::Compiler(std::string original): original(std::move(original)) /* default-constructed members... */ {}

std::string_view Compiler::of(Token token) const noexcept {
    return lines.at(token.line).substr(token.column, token.width);
}

void predefined(LocalContext& context) {
    context.defineExternal("print", std::make_shared<FuncType>(std::vector{ScalarTypes::STRING}, ScalarTypes::NONE));
    context.defineExternal("println", std::make_shared<FuncType>(std::vector{ScalarTypes::STRING}, ScalarTypes::NONE));
    context.defineExternal("readLine", std::make_shared<FuncType>(std::vector<TypeReference>{}, ScalarTypes::STRING));
    context.defineExternal("parseInt", std::make_shared<FuncType>(std::vector{ScalarTypes::STRING}, ScalarTypes::INT));
    context.defineExternal("parseFloat", std::make_shared<FuncType>(std::vector{ScalarTypes::STRING}, ScalarTypes::FLOAT));
    context.defineExternal("exit", std::make_shared<FuncType>(std::vector{ScalarTypes::INT}, ScalarTypes::NEVER));
    context.defineExternal("millis", std::make_shared<FuncType>(std::vector<TypeReference>{}, ScalarTypes::INT));
    context.defineExternal("nanos", std::make_shared<FuncType>(std::vector<TypeReference>{}, ScalarTypes::INT));
    context.defineExternal("getargs", std::make_shared<FuncType>(std::vector<TypeReference>{}, std::make_shared<ListType>(ScalarTypes::STRING)));
    context.defineExternal("output", std::make_shared<FuncType>(std::vector{ScalarTypes::STRING}, ScalarTypes::NONE));
    context.defineExternal("input", std::make_shared<FuncType>(std::vector{ScalarTypes::STRING}, ScalarTypes::NONE));
    context.defineExternal("flush", std::make_shared<FuncType>(std::vector<TypeReference>{}, ScalarTypes::NONE));
    context.defineExternal("eof", std::make_shared<FuncType>(std::vector<TypeReference>{}, ScalarTypes::BOOL));
    context.defineExternal("typename", std::make_shared<FuncType>(std::vector{ScalarTypes::ANY}, ScalarTypes::STRING));
    context.defineExternal("gc", std::make_shared<FuncType>(std::vector<TypeReference>{}, ScalarTypes::NONE));
    context.defineExternal("toBytes", std::make_shared<FuncType>(std::vector{ScalarTypes::STRING}, std::make_shared<ListType>(ScalarTypes::BYTE)));
    context.defineExternal("toChars", std::make_shared<FuncType>(std::vector{ScalarTypes::STRING}, std::make_shared<ListType>(ScalarTypes::CHAR)));
    context.defineExternal("fromBytes", std::make_shared<FuncType>(std::vector<TypeReference>{std::make_shared<ListType>(ScalarTypes::BYTE)}, ScalarTypes::STRING));
    context.defineExternal("fromChars", std::make_shared<FuncType>(std::vector<TypeReference>{std::make_shared<ListType>(ScalarTypes::CHAR)}, ScalarTypes::STRING));
}

void Compiler::parse() {
    Parser parser(this, tokens);
    predefined(parser.context);
    auto F = std::make_shared<FuncType>(std::vector<TypeReference>{}, nullptr);
    auto tree = parser.parseFnBody(F, false);
    parser.expect(TokenType::LINEBREAK, "a final linebreak is expected");
    if (parser.remains())
        throw ParserException("unterminated tokens", parser.peek());
    definition = std::make_unique<FunctionDefinition>(false, std::move(tree), std::move(parser.context.localTypes));
    auto main = std::make_unique<MainFunctionReference>();
    main->type = F;
    main->definition = definition.get();
    functions.push_back(std::move(main));
}

void Compiler::compile(Assembler* assembler) {
    for (auto&& function : functions) {
        assembler->func(function->prototype());
    }
    for (auto&& function : functions) {
        function->assemble(assembler);
    }
}

std::string Compiler::descriptor() const {
    return definition->clause->walkDescriptor();
}

}