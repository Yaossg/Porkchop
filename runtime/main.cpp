#include <cstdio>
#include <cstring>

#include "runtime.hpp"

#include "../io.hpp"

int main(int argc, char* argv[]) {
    forceUTF8();
    if (argc < 2) {
        fprintf(stderr, "Fatal: Too few arguments, input file expected\n");
        fprintf(stderr, "Usage: PorkchopRuntime <input>\n");
        exit(10);
    }
    Porkchop::TextAssembly c(readAll(argv[1]));
    c.parse();
    Porkchop::Runtime::Func main_{0, {}};
    main_.call(&c);
    return 0;
}
