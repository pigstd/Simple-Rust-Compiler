#include "tools.h"

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

long long safe_stoll(const string &s, long long max_value) {
    auto it = s.begin();
    long long result = 0;
    int base = 10;
    if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        it += 2;
    }
    // 别的进制先不管，遇到了再说
    while(it != s.end()) {
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