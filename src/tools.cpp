#include "tools.h"

long long safe_stoll(const string &s, long long max_value) {
    long long result = 0;
    for (auto &ch : s) {
        if (ch < '0' || ch > '9') {
            throw string("CE, invalid character in number: ") + ch;
        }
        if (result > (max_value - (ch - '0')) / 10) {
            throw string("CE, number out of range");
        }
        result = result * 10 + (ch - '0');
    }
    return result;
}