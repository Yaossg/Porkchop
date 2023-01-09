#pragma once

#include <stdexcept>
#include <memory>

#include "source.hpp"
#include "token.hpp"
#include "type.hpp"

namespace Porkchop {

inline std::string render(std::string_view color, std::string_view text) {
    static const bool disableColor = std::getenv("PORKCHOP_COLOR_DISABLE");
    if (disableColor) return std::string(text);
    return join(color, text, "\x1b[m");
}

struct ErrorMessage {

    enum class Kind {
        ERROR, NOTE
    } kind;

    [[nodiscard]] std::string colored(std::string_view text) const {
        static constexpr auto colors = {"\x1b[91m", "\x1b[96m"};
        return render(std::data(colors)[(int) kind], text);
    }

    std::string message;
    Segment segment;
    bool textOnly = false;

    ErrorMessage& fatal() {
        kind = Kind::ERROR;
        message = colored("fatal: ");
        textOnly = true;
        return *this;
    }

    ErrorMessage& usage() {
        kind = Kind::NOTE;
        message = colored("usage: ");
        textOnly = true;
        return *this;
    }

    ErrorMessage& error(Segment seg) {
        kind = Kind::ERROR;
        message = colored("error: ");
        segment = seg;
        return *this;
    }

    ErrorMessage& note() {
        kind = Kind::NOTE;
        message = colored("note: ");
        textOnly = true;
        return *this;
    }

    ErrorMessage& note(Segment seg) {
        kind = Kind::NOTE;
        message = colored("note: ");
        segment = seg;
        return *this;
    }

    ErrorMessage& text(std::string_view text) {
        message += text;
        return *this;
    }

    ErrorMessage& num(size_t num) {
        if (message.ends_with(' ')) message.pop_back();
        message += join(' ', std::to_string(num), ' ');
        return *this;
    }

    ErrorMessage& num(int64_t num) {
        if (message.ends_with(' ')) message.pop_back();
        message += join(' ', std::to_string(num), ' ');
        return *this;
    }

    ErrorMessage& quote(std::string_view text) {
        if (message.ends_with(' ')) message.pop_back();
        message += join(" '", render("\x1b[97m", text), "' ");
        return *this;
    }

    ErrorMessage& type(TypeReference const& type) {
        return quote(type->toString());
    }

    std::string build(Source* source);
};

struct Error : std::exception {
    std::vector<ErrorMessage> messages;

    Error& with(ErrorMessage message) {
        messages.emplace_back(std::move(message));
        return *this;
    }

    [[nodiscard]] const char * what() const noexcept override {
        return "Porkchop::Error";
    }

    [[noreturn]] void raise() {
        throw std::move(*this);
    }

    void report(Source* source, bool newline);
};


[[noreturn]] void raise(const char* msg, Segment segment);


void neverGonnaGiveYouUp(TypeReference const& type, const char* msg, Segment segment);
}