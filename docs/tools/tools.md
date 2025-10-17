### tools 部分

该模块用于提供一些简单的工具函数

function: `int ch_to_digit(const char &ch);`

作用：将字符转换为数字，考虑字符是数字或者字母（用于高进制）的情况，如果不是则返回 $-1$。

function: `long long safe_stoll(const string &s, long long max_value);`

将字符串 `s` 转化为上限是 `max_value` 的整数。

默认十进制，不支持后缀处理，支持 `0x` 开头的 16 进制。

如果不是数字，或者超过 `max_value`，则抛出相应的异常（CE）。