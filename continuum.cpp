#include "local.hpp"
#include "function.hpp"

namespace Porkchop {

void Continuum::compile(Assembler* assembler) {
    for (; funcUntil < functions.size(); ++funcUntil) {
        functions[funcUntil]->write(assembler);
    }
}

Continuum::Continuum() {
    context = std::make_unique<LocalContext>(this, nullptr);
    context->defineExternal("print", std::make_shared<FuncType>(std::vector{ScalarTypes::STRING}, ScalarTypes::NONE));
    context->defineExternal("println", std::make_shared<FuncType>(std::vector{ScalarTypes::STRING}, ScalarTypes::NONE));
    context->defineExternal("readLine", std::make_shared<FuncType>(std::vector<TypeReference>{}, ScalarTypes::STRING));
    context->defineExternal("parseInt", std::make_shared<FuncType>(std::vector{ScalarTypes::STRING}, ScalarTypes::INT));
    context->defineExternal("parseFloat", std::make_shared<FuncType>(std::vector{ScalarTypes::STRING}, ScalarTypes::FLOAT));
    context->defineExternal("exit", std::make_shared<FuncType>(std::vector{ScalarTypes::INT}, ScalarTypes::NEVER));
    context->defineExternal("millis", std::make_shared<FuncType>(std::vector<TypeReference>{}, ScalarTypes::INT));
    context->defineExternal("nanos", std::make_shared<FuncType>(std::vector<TypeReference>{}, ScalarTypes::INT));
    context->defineExternal("getargs", std::make_shared<FuncType>(std::vector<TypeReference>{}, std::make_shared<ListType>(ScalarTypes::STRING)));
    context->defineExternal("output", std::make_shared<FuncType>(std::vector{ScalarTypes::STRING}, ScalarTypes::NONE));
    context->defineExternal("input", std::make_shared<FuncType>(std::vector{ScalarTypes::STRING}, ScalarTypes::NONE));
    context->defineExternal("flush", std::make_shared<FuncType>(std::vector<TypeReference>{}, ScalarTypes::NONE));
    context->defineExternal("eof", std::make_shared<FuncType>(std::vector<TypeReference>{}, ScalarTypes::BOOL));
    context->defineExternal("typename", std::make_shared<FuncType>(std::vector{ScalarTypes::ANY}, ScalarTypes::STRING));
    context->defineExternal("gc", std::make_shared<FuncType>(std::vector<TypeReference>{}, ScalarTypes::NONE));
    context->defineExternal("toBytes", std::make_shared<FuncType>(std::vector{ScalarTypes::STRING}, std::make_shared<ListType>(ScalarTypes::BYTE)));
    context->defineExternal("toChars", std::make_shared<FuncType>(std::vector{ScalarTypes::STRING}, std::make_shared<ListType>(ScalarTypes::CHAR)));
    context->defineExternal("fromBytes", std::make_shared<FuncType>(std::vector<TypeReference>{std::make_shared<ListType>(ScalarTypes::BYTE)}, ScalarTypes::STRING));
    context->defineExternal("fromChars", std::make_shared<FuncType>(std::vector<TypeReference>{std::make_shared<ListType>(ScalarTypes::CHAR)}, ScalarTypes::STRING));
    context->defineExternal("eval", std::make_shared<FuncType>(std::vector{ScalarTypes::ANY, ScalarTypes::STRING}, ScalarTypes::ANY));
}


}