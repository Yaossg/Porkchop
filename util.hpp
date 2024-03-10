#pragma once

#include <cstdio>
#include <cstring>
#include <string>
#include <memory>
#include <vector>

namespace Porkchop {

inline bool isInvalidChar(int64_t value) {
    return value < 0 || value > 0x10FFFFLL || 0xD800LL <= value && value <= 0xDFFFLL;
}

FILE* open(const char* filename, const char* mode);

inline std::string readText(const char* filename) {
    FILE* input_file = open(filename, "rb");
    fseek(input_file, 0, SEEK_END);
    size_t size = ftell(input_file);
    fseek(input_file, 0, SEEK_SET);
    std::string fileBuffer(size, '\0');
    fread(fileBuffer.data(), 1, size, input_file);
    fclose(input_file);
    return fileBuffer;
}

inline std::vector<uint8_t> readBin(const char* filename) {
    FILE* input_file = open(filename, "rb");
    fseek(input_file, 0, SEEK_END);
    size_t size = ftell(input_file);
    fseek(input_file, 0, SEEK_SET);
    std::vector<uint8_t> fileBuffer(size);
    fread(fileBuffer.data(), 1, size, input_file);
    fclose(input_file);
    return fileBuffer;
}

inline std::string readLine(FILE* file) {
    std::string line;
    do {
        char buf[256];
        fgets(buf, sizeof buf, file);
        line += buf;
    } while (!line.ends_with('\n') && !feof(file));
    if (line.ends_with('\n'))
        line.pop_back();
    return line;
}

inline void forceUTF8() {
#ifdef _WIN32
    system("chcp>nul 65001");
#endif
}

inline std::vector<std::string_view> splitLines(std::string_view view) {
    std::vector<std::string_view> lines;
    const char *p = view.begin(), *q = p;
    while (q != view.end()) {
        if (*q == '\n' || *q == '\r') {
            lines.emplace_back(p, q);
            if (q[0] == '\r' && q[1] == '\n') {
                ++q;
            }
            p = ++q;
        } else {
            ++q;
        }
    }
    lines.emplace_back(p, q);
    return lines;
}

[[noreturn]] inline void unreachable() {
    __builtin_unreachable();
}

template<typename... Args>
inline std::string join(Args&&... args) {
    std::string result;
    ((result += args), ...);
    return result;
}

template<typename Derived, typename Base>
inline std::unique_ptr<Derived> dynamic_pointer_cast(std::unique_ptr<Base>&& base) noexcept {
    if (auto derived = dynamic_cast<Derived*>(base.get())) {
        std::ignore = base.release();
        return std::unique_ptr<Derived>(derived);
    }
    return nullptr;
}

inline void replaceAll(std::string& inout, std::string_view what, std::string_view with) {
    for (size_t pos = 0; std::string::npos != (pos = inout.find(what.data(), pos, what.length())); pos += with.length()) {
        inout.replace(pos, what.length(), with.data(), with.length());
    }
}



struct OutputFile {
    FILE* file;

    explicit OutputFile(std::string const& filename, bool bin) {
        if (filename == "<null>") {
            file = nullptr;
        } else if (filename == "<stdout>") {
            file = stdout;
        } else {
            file = Porkchop::open(filename.c_str(), bin ? "wb" : "w");
        }
    }

    ~OutputFile() {
        if (file != nullptr && file != stdout) {
            fclose(file);
        }
    }

    void puts(const char* str) const {
        if (file != nullptr)
            fputs(str, file);
    }
};

}