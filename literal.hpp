#include "token.hpp"

namespace Porkchop {

int64_t parseInt(SourceCode& sourcecode, Token token);
double parseFloat(SourceCode& sourcecode, Token token);
char32_t parseChar(SourceCode& sourcecode, Token token);
std::string parseString(SourceCode& sourcecode, Token token);

}