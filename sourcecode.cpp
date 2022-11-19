#include "parser.hpp"
#include "function.hpp"

namespace Porkchop {

SourceCode::SourceCode(std::string original) noexcept: original(std::move(original)) /* default-constructed members... */ {
    functions.emplace_back(std::make_unique<ExternalFunction>()); // main function
}

void SourceCode::split() {
    std::string_view view(original);
    const char *p = view.begin(), *q = p, *r = view.end();
    while (q != r) {
        switch (*q++) {
            [[unlikely]] case '#': {
                lines.emplace_back(p, q - 1);
                while (q != r && *q++ != '\n');
                p = q;
                break;
            }
                [[unlikely]] case '\n': {
                lines.emplace_back(p, q - 1);
                p = q;
                break;
            }
        }
    }
    if (p != q) lines.emplace_back(p, q);
}

[[nodiscard]] std::vector<Token> tokenize(std::string_view view, size_t line);
void SourceCode::tokenize() {
    for (size_t line = 0; line < lines.size(); ++line) {
        for (auto&& token : Porkchop::tokenize(lines[line], line)) {
            tokens.push_back(token);
        }
    }
}

void SourceCode::parse() {
    if (tokens.empty()) return;
    Parser parser(this, tokens);
    parser.context.defineExternal("print", std::make_shared<FuncType>(std::vector{ScalarTypes::STRING}, ScalarTypes::NONE));
    parser.context.defineExternal("println", std::make_shared<FuncType>(std::vector{ScalarTypes::STRING}, ScalarTypes::NONE));
    parser.context.defineExternal("readLine", std::make_shared<FuncType>(std::vector<TypeReference>{}, ScalarTypes::STRING));
    parser.context.defineExternal("i2s", std::make_shared<FuncType>(std::vector{ScalarTypes::INT}, ScalarTypes::STRING));
    parser.context.defineExternal("f2s", std::make_shared<FuncType>(std::vector{ScalarTypes::FLOAT}, ScalarTypes::STRING));
    parser.context.defineExternal("s2i", std::make_shared<FuncType>(std::vector{ScalarTypes::STRING}, ScalarTypes::INT));
    parser.context.defineExternal("s2f", std::make_shared<FuncType>(std::vector{ScalarTypes::STRING}, ScalarTypes::FLOAT));
    parser.context.defineExternal("exit", std::make_shared<FuncType>(std::vector{ScalarTypes::INT}, ScalarTypes::NEVER));
    std::tie(tree, type) = parser.parseFnBody();
    parser.expect(TokenType::LINEBREAK, "a linebreak is expected");
    if (parser.remains())
        throw ParserException("unterminated tokens", parser.peek());
    locals = parser.context.localTypes.size();
}

void SourceCode::compile(Assembler* assembler) {
    assembler->beginFunction();
    assembler->indexed(Opcode::LOCAL, locals);
    tree->walkBytecode(*this, assembler);
    assembler->opcode(Opcode::RETURN);
    assembler->endFunction();
    for (auto&& function : functions) {
        function->compile(*this, assembler);
    }
}

}