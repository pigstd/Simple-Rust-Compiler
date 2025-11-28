#include "tools/tools.h"
#include <climits>
#include <cstdint>

int ch_to_digit(const char &ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    } else if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    } else if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    } else {
        return -1;
    }
}

long long safe_stoll(const string &s) {
    if (s.empty()) {
        throw string("CE, empty integer literal");
    }
    std::string digits = s;
    long long max_value = LLONG_MAX;
    auto apply_suffix = [&](const string &suffix, long long limit) {
        if (digits.size() <= suffix.size()) {
            throw string("CE, invalid integer literal: ") + s;
        }
        digits = digits.substr(0, digits.size() - suffix.size());
        max_value = limit;
    };
    if (s.size() >= 3 && s.compare(s.size() - 3, 3, "i32") == 0) {
        apply_suffix("i32", INT32_MAX);
    } else if (s.size() >= 3 && s.compare(s.size() - 3, 3, "u32") == 0) {
        apply_suffix("u32", UINT32_MAX);
    } else if (s.size() >= 5 && s.compare(s.size() - 5, 5, "isize") == 0) {
        apply_suffix("isize", INT32_MAX);
    } else if (s.size() >= 5 && s.compare(s.size() - 5, 5, "usize") == 0) {
        apply_suffix("usize", UINT32_MAX);
    }
    auto it = digits.begin();
    long long result = 0;
    int base = 10;
    if (digits.size() >= 2 && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) {
        base = 16;
        it += 2;
    }
    // 别的进制先不管，遇到了再说
    if (it == digits.end()) {
        throw string("CE, invalid number literal: ") + s;
    }
    while(it != digits.end()) {
        int digit = ch_to_digit(*it);
        if (digit == -1 || digit >= base) {
            throw string("CE, invalid digit in number: ") + *it + " base = " + std::to_string(base);
        }
        if (result > (max_value - digit) / base) {
            throw string("CE, number too large: ") + s;
        }
        result = result * base + digit;
        it++;
    }
    return result;
}
