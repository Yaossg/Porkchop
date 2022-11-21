#include "runtime.hpp"

#include "../io.hpp"

int main(int argc, char* argv[]) try {
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
} catch (Porkchop::Runtime::Exception& e) {
    fprintf(stderr, "Runtime exception occurred:\n");
    fprintf(stderr, "%s\n", e.what());
    return 1;
}
