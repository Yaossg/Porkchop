#pragma once

#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <string>


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

inline std::vector<std::string_view> splitLines(std::string const& entire) {
    std::vector<std::string_view> lines;
    std::string_view view(entire);
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
