#include "common.hpp"
#include <functional>

std::unordered_map<std::string, std::string> parseArgs(int argc, const char* argv[]) {
    std::unordered_map<std::string, std::string> args;
    if (argc < 2) {
        Porkchop::Error error;
        error.with(Porkchop::ErrorMessage().fatal().text("too few arguments, input file expected"));
        error.with(Porkchop::ErrorMessage().usage().text("PorkchopHighlight <input> [options...]"));
        error.report(nullptr, true);
        std::exit(10);
    }
    args["input"] = argv[1];
    args["type"] = "console";
    for (int i = 2; i < argc; ++i) {
        if (!strcmp("-o", argv[i])) {
            if (++i >= argc) {
                Porkchop::Error().with(
                        Porkchop::ErrorMessage().fatal().text("no output file specified")
                ).report(nullptr, false);
                std::exit(11);
            }
            args["output"] = argv[i];
        } else if (!strcmp("--html", argv[i])) {
            args["type"] = "html";
            if (++i >= argc) {
                Porkchop::Error().with(
                        Porkchop::ErrorMessage().fatal().text("no html type specified")
                ).report(nullptr, false);
                std::exit(11);
            }
            args["html-type"] = argv[i];
        } else if (!strcmp("--console", argv[i])) {
            args["type"] = "console";
        } else {
            Porkchop::Error().with(
                    Porkchop::ErrorMessage().fatal().text("unknown flag: ").text(argv[i])
            ).report(nullptr, false);
            std::exit(11);
        }
    }
    if (args["type"] == "console") {
        args["output"] = "<stdout>";
    }
    if (!args.contains("output")) {
        auto const& input = args["input"];
        args["output"] = input.substr(0, input.find_last_of('.')) + '.' + args["type"];
    }
    return args;
}

constexpr auto html_head = R"(<html>
<head>
  <style>
    body {
      font-family: "Consolas", monospace;
      font-size: 14px;
      padding: 20px;
      line-height: 125%;
    }
    .keyword {
      font-weight: bold;
      color: #0000FF;
    }
    .string {
      color: #067D17;
    }
    .number {
      color: #EB5017;
    }
    .identifier {
      color: #00627A;
    }
    .punctuation {
      color: black;
    }
    .comment {
      font-style: italic;
      color: gray;
    }
  </style>
</head>
<body>
)";

constexpr auto html_tail = R"(</body>
</html>
)";

enum class TokenKind {
    KEYWORD, NUMBER, STRING, IDENTIFIER, PUNCTUATION,
};

TokenKind kindOf(Porkchop::TokenType type) {
    switch (type) {
        case Porkchop::TokenType::KW_FALSE:
        case Porkchop::TokenType::KW_TRUE:
        case Porkchop::TokenType::KW_LINE:
        case Porkchop::TokenType::KW_NAN:
        case Porkchop::TokenType::KW_INF:
        case Porkchop::TokenType::KW_WHILE:
        case Porkchop::TokenType::KW_IF:
        case Porkchop::TokenType::KW_ELSE:
        case Porkchop::TokenType::KW_FOR:
        case Porkchop::TokenType::KW_FN:
        case Porkchop::TokenType::KW_BREAK:
        case Porkchop::TokenType::KW_RETURN:
        case Porkchop::TokenType::KW_AS:
        case Porkchop::TokenType::KW_IS:
        case Porkchop::TokenType::KW_LET:
        case Porkchop::TokenType::KW_IN:
        case Porkchop::TokenType::KW_SIZEOF:
        case Porkchop::TokenType::KW_YIELD:
            return TokenKind::KEYWORD;
        case Porkchop::TokenType::BINARY_INTEGER:
        case Porkchop::TokenType::OCTAL_INTEGER:
        case Porkchop::TokenType::DECIMAL_INTEGER:
        case Porkchop::TokenType::HEXADECIMAL_INTEGER:
        case Porkchop::TokenType::FLOATING_POINT:
            return TokenKind::NUMBER;
        case Porkchop::TokenType::STRING_QQ:
        case Porkchop::TokenType::STRING_QD:
        case Porkchop::TokenType::STRING_UD:
        case Porkchop::TokenType::STRING_UQ:
        case Porkchop::TokenType::RAW_STRING_QQ:
        case Porkchop::TokenType::RAW_STRING_QD:
        case Porkchop::TokenType::RAW_STRING_QU:
        case Porkchop::TokenType::RAW_STRING_UU:
        case Porkchop::TokenType::RAW_STRING_UD:
        case Porkchop::TokenType::RAW_STRING_UQ:
        case Porkchop::TokenType::CHARACTER_LITERAL:
            return TokenKind::STRING;
        case Porkchop::TokenType::IDENTIFIER:
            return TokenKind::IDENTIFIER;
        default:
            return TokenKind::PUNCTUATION;
    }
}


int main(int argc, const char* argv[]) try {
    Porkchop::forceUTF8();
    auto args = parseArgs(argc, argv);
    auto original = Porkchop::readText(args["input"].c_str());
    Porkchop::Source source;
    Porkchop::tokenize(source, original);
    auto const& output_type = args["type"];
    Porkchop::OutputFile output_file(args["output"], false);
    std::function<void(std::string, bool)> output;
    std::function<void(std::string const&, Porkchop::Token)> renderer;
    std::function<void(std::string const&)> comment;
    if (output_type == "html") {
        output = [&output_file](std::string text, bool escape) {
            if (escape) {
                Porkchop::replaceAll(text, "&", "&amp;");
                Porkchop::replaceAll(text, "<", "&lt;");
                Porkchop::replaceAll(text, ">", "&gt;");
                Porkchop::replaceAll(text, "\"", "&quot;");
                Porkchop::replaceAll(text, " ", "&nbsp;");
                Porkchop::replaceAll(text, "\n", "<br>\n");
            }
            output_file.puts(text.c_str());
        };
        renderer = [&output](std::string const& line, Porkchop::Token token) {
            output("<span class=\"", false);
            switch (kindOf(token.type)) {
                case TokenKind::KEYWORD:
                    output("keyword", false);
                    break;
                case TokenKind::NUMBER:
                    output("number", false);
                    break;
                case TokenKind::STRING:
                    output("string", false);
                    break;
                case TokenKind::IDENTIFIER:
                    output("identifier", false);
                    break;
                case TokenKind::PUNCTUATION:
                    output("punctuation", false);
                    break;
            }
            output("\">", false);
            output(line.substr(token.column, token.width), true);
            output("</span>", false);
        };
        comment = [&output](std::string const& text) {
            output("<span class=\"comment\">", false);
            output(text, true);
            output("</span>", false);
        };
    } else {
        output = [&output_file](std::string text, bool) {
            output_file.puts(text.c_str());
        };
        renderer = [&output](std::string const& line, Porkchop::Token token) {
            switch (kindOf(token.type)) {
                case TokenKind::KEYWORD:
                    output("\x1b[95m", false);
                    break;
                case TokenKind::NUMBER:
                    output("\x1b[94m", false);
                    break;
                case TokenKind::STRING:
                    output("\x1b[92m", false);
                    break;
                case TokenKind::IDENTIFIER:
                    output("\x1b[36m", false);
                    break;
                case TokenKind::PUNCTUATION:
                    output("\x1b[0m", false);
                    break;
            }
            output(line.substr(token.column, token.width), true);
        };
        comment = [&output](std::string const& text) {
            output("\x1b[90m\x1b[3m", false);
            output(text, true);
            output("\x1b[0m", false);
        };
    }
    bool head = output_type == "html" && args["html-type"] != "headless";
    if (head) {
        output_file.puts(html_head);
    }
    auto iterator = source.tokens.begin();
    for (size_t i = 0; i < source.lines.size(); ++i) {
        auto const& line = source.lines[i];
        std::vector<Porkchop::Token> tokens;
        while (iterator != source.tokens.end() && iterator->line == i) {
            tokens.push_back(*iterator);
            ++iterator;
        }
        if (!tokens.empty() && tokens.back().type == Porkchop::TokenType::LINEBREAK) tokens.pop_back();
        if (tokens.empty()) {
            comment(line);
        } else {
            output(line.substr(0, tokens.front().column), true);
            for (size_t j = 0; j < tokens.size() - 1; ++j) {
                auto token = tokens[j];
                auto next = tokens[j + 1];
                renderer(line, token);
                output(line.substr(token.column + token.width, next.column - (token.column + token.width)), true);
            }
            auto token = tokens.back();
            renderer(line, token);
            comment(line.substr(token.column + token.width));
        }
        output("\n", true);
    }
    if (head) {
        output_file.puts(html_tail);
    }
} catch (std::bad_alloc& e) {
    fprintf(stderr, "Compiler out of memory\n");
    std::exit(-10);
} catch (std::exception& e) {
    fprintf(stderr, "Internal Compiler Error: %s\n", e.what());
    std::exit(-100);
}
