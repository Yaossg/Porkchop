#include "runtime.hpp"

#include "../io.hpp"

int main(int argc, char* argv[]) try {
    forceUTF8();
    if (argc < 2) {
        fprintf(stderr, "Fatal: Too few arguments, input file expected\n");
        fprintf(stderr, "Usage: PorkchopRuntime <input> [args...]\n");
        exit(10);
    }
    Porkchop::TextAssembly c(readAll(argv[1]));
    c.parse();
    auto args = std::bit_cast<std::vector<size_t>*>(Porkchop::Externals::getargs({}));
    for (size_t i = 2; i < argc; ++i) {
        args->push_back(std::bit_cast<size_t>(new std::string(argv[i])));
    }
    Porkchop::Runtime::Func main_{0, {}};
    auto ret = main_.call(&c);
    fprintf(stdout, "Program finished with exit code %zu", ret);
    return 0;
} catch (Porkchop::Runtime::Exception& e) {
    fprintf(stderr, "Runtime exception occurred:\n");
    fprintf(stderr, "%s\n", e.what());
    return 1;
}
