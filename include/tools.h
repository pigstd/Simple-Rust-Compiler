#ifndef TOOLS_H
#define TOOLS_H

#include <climits>
#include <string>
using std::string;

// 如果有不是字符的，或者太大的，直接 throw CE
long long safe_stoll(const string &s, long long max_value);

#endif // TOOLS_H