#ifndef TOOLS_H
#define TOOLS_H

#include <climits>
#include <string>
using std::string;

// 数字字符转数字
// 不是数字字符，返回 -1
int ch_to_digit(const char &ch);

// 如果有不是字符的，或者太大的，直接 throw CE
// 需要考虑有后缀，需要考虑 0x 开头的情况：16 进制
long long safe_stoll(const string &s);

#endif // TOOLS_H
