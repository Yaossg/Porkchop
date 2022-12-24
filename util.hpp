#pragma once

#include <cstdio>
#include <cstring>
#include <string>
#include <memory>

namespace Porkchop {

template<typename T>
inline size_t as_size(T&& t) {
    return std::bit_cast<size_t>(t);
}

inline double as_double(size_t arg) {
    return std::bit_cast<double>(arg);
}

inline bool isInvalidChar(int64_t value) {
    return value < 0 || value > 0x10FFFFLL || 0xD800LL <= value && value <= 0xDFFFLL;
}

inline std::string readAll(std::string const& filename) {
    FILE* input_file = fopen(filename.c_str(), "r");
    if (input_file == nullptr) {
        fprintf(stderr, "Failed to open input file: %s\n", filename.c_str());
        exit(20);
    }
    std::string content;
    do {
        char line[1024];
        memset(line, 0, sizeof line);
        fgets(line, sizeof line, input_file);
        content += line;
    } while (!feof(input_file));
    fclose(input_file);
    return content;
}

inline void forceUTF8() {
#ifdef _WIN32
    system("chcp 65001");
    puts("Porkchop: UTF-8 is adopted via 'chcp 65001' in Windows");
#endif
}

inline std::vector<std::string_view> splitLines(std::string_view view) {
    std::vector<std::string_view> lines;
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
    return lines;
}

[[noreturn]] inline void unreachable() {
    __builtin_unreachable();
}

inline const char* ordinalSuffix(size_t ordinal) {
    switch (ordinal % 100) {
        default:
            switch (ordinal % 10) {
                case 1:
                    return "st";
                case 2:
                    return "nd";
                case 3:
                    return "rd";
            }
        case 11:
        case 12:
        case 13: ;
    }
    return "th";
}

inline std::string ordinal(size_t index) {
    return "the " + std::to_string(index + 1) + ordinalSuffix(index + 1);
}


template<typename Derived, typename Base>
inline std::unique_ptr<Derived> dynamic_pointer_cast(std::unique_ptr<Base>&& base) noexcept {
    if (auto derived = dynamic_cast<Derived*>(base.get())) {
        std::ignore = base.release();
        return std::unique_ptr<Derived>(derived);
    }
    return nullptr;
}


}