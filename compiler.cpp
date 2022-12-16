#include "parser.hpp"
#include "function.hpp"

namespace Porkchop {

Compiler::Compiler(std::string original): original(std::move(original)) /* default-constructed members... */ {
    functions.emplace_back(std::make_unique<ExternalFunction>()); // main function
}

std::string_view Compiler::of(Token token) const noexcept {
    return lines.at(token.line).substr(token.column, token.width);
}

void predefined(LocalContext& context) {
    context.defineExternal("print", std::make_shared<FuncType>(std::vector{ScalarTypes::STRING}, ScalarTypes::NONE));
    context.defineExternal("println", std::make_shared<FuncType>(std::vector{ScalarTypes::STRING}, ScalarTypes::NONE));
    context.defineExternal("readLine", std::make_shared<FuncType>(std::vector<TypeReference>{}, ScalarTypes::STRING));
    context.defineExternal("i2s", std::make_shared<FuncType>(std::vector{ScalarTypes::INT}, ScalarTypes::STRING));
    context.defineExternal("f2s", std::make_shared<FuncType>(std::vector{ScalarTypes::FLOAT}, ScalarTypes::STRING));
    context.defineExternal("s2i", std::make_shared<FuncType>(std::vector{ScalarTypes::STRING}, ScalarTypes::INT));
    context.defineExternal("s2f", std::make_shared<FuncType>(std::vector{ScalarTypes::STRING}, ScalarTypes::FLOAT));
    context.defineExternal("exit", std::make_shared<FuncType>(std::vector{ScalarTypes::INT}, ScalarTypes::NEVER));
    context.defineExternal("millis", std::make_shared<FuncType>(std::vector<TypeReference>{}, ScalarTypes::INT));
    context.defineExternal("nanos", std::make_shared<FuncType>(std::vector<TypeReference>{}, ScalarTypes::INT));
    context.defineExternal("getargs", std::make_shared<FuncType>(std::vector<TypeReference>{}, std::make_shared<ListType>(ScalarTypes::STRING)));
    context.defineExternal("output", std::make_shared<FuncType>(std::vector{ScalarTypes::STRING}, ScalarTypes::NONE));
    context.defineExternal("input", std::make_shared<FuncType>(std::vector{ScalarTypes::STRING}, ScalarTypes::NONE));
    context.defineExternal("flush", std::make_shared<FuncType>(std::vector<TypeReference>{}, ScalarTypes::NONE));
    context.defineExternal("eof", std::make_shared<FuncType>(std::vector<TypeReference>{}, ScalarTypes::BOOL));
    context.defineExternal("typename", std::make_shared<FuncType>(std::vector{ScalarTypes::ANY}, ScalarTypes::STRING));
}

void Compiler::parse() {
    if (tokens.empty()) return;
    Parser parser(this, tokens);
    predefined(parser.context);
    std::tie(tree, type) = parser.parseFnBody();
    parser.expect(TokenType::LINEBREAK, "a linebreak is expected");
    if (parser.remains())
        throw ParserException("unterminated tokens", parser.peek());
    locals = parser.context.localTypes.size();
}

void Compiler::compile(Assembler* assembler) {
    assembler->beginFunction();
    assembler->indexed(Opcode::LOCAL, locals);
    tree->walkBytecode(assembler);
    assembler->opcode(Opcode::RETURN);
    assembler->endFunction();
    for (auto&& function : functions) {
        function->assemble(assembler);
    }
}

std::string Compiler::descriptor() const {
    return tree->walkDescriptor();
}

}