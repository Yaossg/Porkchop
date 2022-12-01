#include "runtime.hpp"

#include "../io.hpp"

int main(int argc, char* argv[]) try {
    forceUTF8();
    if (argc < 2) {
        fprintf(stderr, "Fatal: Too few arguments, input file expected\n");
        fprintf(stderr, "Usage: PorkchopRuntime <input> [args...]\n");
        return 10;
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
} catch (std::bad_alloc& e) {
    fprintf(stderr, "Execution Failed: Runtime run out of memory: %s\n", e.what());
    return -50;
} catch (Porkchop::Runtime::Exception& e) {
    fprintf(stderr, "Runtime exception occurred:\n");
    fprintf(stderr, "%s\n", e.what());
    return 1;
} catch (std::runtime_error& e) {
    fprintf(stderr, "Runtime external exception occurred:\n");
    fprintf(stderr, "%s\n", e.what());
    return 2;
} catch (std::exception& e) {
    fprintf(stderr, "Runtime Internal Error: Unclassified std::exception occurred: %s\n", e.what());
    return -1000;
} catch (...) {
    fprintf(stderr, "Runtime Internal Error: Unknown naked exception occurred\n");
    return -1001;
}

