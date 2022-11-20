#include "unicode.hpp"

#include <charconv>

namespace Porkchop {

char32_t hex(const char *first, const char *last, unsigned int bound, Segment segment) {
    unsigned value = 0;
    auto [ptr, ec] = std::from_chars(first, last, value, 16);
    if (ptr != last) {
        throw ConstException("the escape sequence expect exact " + std::to_string(std::distance(first, last)) + " hex digits", segment);
    }
    if (value > bound) {
        ec = std::errc::result_out_of_range;
    }
    if (ec == std::errc{}) {
        if (isSurrogate(value)) {
            throw ConstException("the hex value represents a surrogate", segment);
        } else {
            return value;
        }
    }
    switch (ec) {
        case std::errc::invalid_argument:
            throw ConstException("the hex value is invalid", segment);
        case std::errc::result_out_of_range:
            throw ConstException("the hex value is out of valid range", segment);
        default:
            unreachable("hex escape sequence");
    }
}

std::string encodeUnicode(char32_t unicode) {
    if (unicode <= ASCII_UPPER_BOUND) { // ASCII
        return {char(unicode)};
    } else if (unicode <= 0x7FF) { // 2 bytes
        return {char((unicode >> 6) | 0xC0),
                char((unicode & 0x3F) | 0x80)};
    } else if (unicode <= 0xFFFF) { // 3 bytes
        return {char((unicode >> 12) | 0xE0),
                char(((unicode >> 6) & 0x3F) | 0x80),
                char((unicode & 0x3F) | 0x80)};
    } else if (unicode <= UNICODE_UPPER_BOUND) { // 4 bytes
        return {char((unicode >> 18) | 0xF0),
                char(((unicode >> 12) & 0x3F) | 0x80),
                char(((unicode >> 6) & 0x3F) | 0x80),
                char((unicode & 0x3F) | 0x80)};
    }
    throw std::out_of_range("invalid unicode beyond 0x10FFFF");
}


char32_t UnicodeParser::decodeUnicode() {
    char32_t result;
    char8_t ch1 = getc();
    switch (queryUTF8Length(ch1)) {
        case 1:
            result = ch1;
            break;
        case 2: {
            char8_t ch2 = getc();
            requireContinue(ch2);
            result = ((ch1 & ~0xC0) << 6) | (ch2 & ~0x80);
        } break;
        case 3: {
            char8_t ch2 = getc();
            requireContinue(ch2);
            char8_t ch3 = getc();
            requireContinue(ch3);
            result = ((ch1 & ~0xE0) << 12) | ((ch2 & ~0x80) << 6) | (ch3 & ~0x80);
        } break;
        case 4: {
            char8_t ch2 = getc();
            requireContinue(ch2);
            char8_t ch3 = getc();
            requireContinue(ch3);
            char8_t ch4 = getc();
            requireContinue(ch4);
            result = ((ch1 & ~0xF0) << 18) | ((ch2 & ~0x80) << 12) | ((ch3 & ~0x80) << 6) | (ch4 & ~0x80);
        } break;
    }
    if (isSurrogate(result))
        throw ConstException("the value of UTF-8 series represents a surrogate", make());
    if (result > UNICODE_UPPER_BOUND)
        throw ConstException("the value of UTF-8 series exceeds upper bound of Unicode", make());
    return result;
}

}