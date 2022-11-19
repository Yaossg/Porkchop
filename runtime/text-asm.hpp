#pragma once

#include <charconv>

#include "assembly.hpp"
#include "../io.hpp"

namespace Porkchop {

struct TextAssembly : Assembly {
    std::string original;
    std::vector<std::string_view> lines;
    bool initialized = false;

    explicit TextAssembly(std::string original) : original(std::move(original)) {}

    void parse() {
        lines = ::splitLines(original);
        for (auto it = lines.begin(); it != lines.end(); ++it) {
            if (*it == "(") {
                std::vector<std::string_view> collect;
                while (*++it != ")") {
                    collect.push_back(*it);
                }
                FunctionParser parser{collect};
                parser.processLabels();
                parser.processInstructions();
                functions.emplace_back(std::move(parser.instructions));
                if (!initialized) {
                    externalFunctions();
                    initialized = true;
                }
            }
        }
    }

    struct FunctionParser {
        std::vector<std::string_view> lines;
        std::unordered_map<std::string_view, size_t> labels;
        Instructions instructions;

        void processLabels() {
            for (size_t i = 0; i < lines.size(); ++i) {
                auto &line = lines[i];
                if (line.starts_with('L')) {
                    size_t colon = line.find(':');
                    labels[line.substr(0, colon)] = i;
                    line = line.substr(colon + 1);
                }
                if (line.starts_with(' '))
                    line.remove_prefix(line.find_first_not_of(' '));
            }
        }

        void processInstructions() {
            for (auto line: lines) {
                size_t space = line.find(' ');
                auto opcode = OPCODES.at(line.substr(0, space));
                if (space != std::string::npos) {
                    auto args = line.substr(space);
                    if (args.starts_with(' '))
                        args.remove_prefix(args.find_first_not_of(' '));
                    switch (opcode) {
                        case Opcode::JMP:
                        case Opcode::JMP0:
                            // label
                            instructions.emplace_back(opcode, labels[args]);
                            break;
                        case Opcode::CONST:
                            // hex
                            instructions.emplace_back(opcode, strtoull(args.data(), nullptr, 16));
                            break;
                        case Opcode::STRING: {
                            // string
                            char *ptr;
                            size_t len = strtoull(args.data(), &ptr, 10);
                            args = args.substr(ptr - args.data());
                            if (args.starts_with(' '))
                                args.remove_prefix(args.find_first_not_of(' '));
                            std::string s;
                            for (size_t i = 0; i < len; ++i) {
                                unsigned char ch;
                                std::from_chars(args.data() + 2 * i, args.data() + 2 * i + 2, ch, 16);
                                s += ch;
                            }
                            instructions.emplace_back(opcode, s);
                            break;
                        }
                        case Opcode::FUNC:
                        case Opcode::LOAD:
                        case Opcode::STORE:
                        case Opcode::TLOAD:
                        case Opcode::TSTORE:
                        case Opcode::BIND:
                        case Opcode::LOCAL:
                        case Opcode::TUPLE:
                        case Opcode::LIST:
                        case Opcode::SET:
                        case Opcode::DICT:
                            // index or size
                            instructions.emplace_back(opcode, strtoull(args.data(), nullptr, 10));
                            break;
                        case Opcode::AS:
                        case Opcode::IS:
                        case Opcode::ANY: {
                            // type
                            instructions.emplace_back(opcode, std::string(args));
                            break;
                        }
                    }
                } else {
                    instructions.emplace_back(opcode, std::monostate());
                }
            }
        }
    };

};

}