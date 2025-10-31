### tools 模块

该模块收录词法与语义阶段常用的辅助函数，位于 `include/tools/tools.h` 与 `src/tools/tools.cpp`。

#### `int ch_to_digit(const char &ch)`
- **参数**：单个字符 `ch`。
- **返回值**：若是 `'0'~'9'`、`'a'~'f'`、`'A'~'F'` 之一，则返回对应的数值（十六进制场景可用）；否则返回 `-1`。
- **实现细节**：通过区间比较判断字符范围，不抛出异常，便于上层自行处理非法字符。
- **使用示例**：
  ```cpp
  int value = ch_to_digit('B'); // 11
  if (value == -1) { /* 非法输入 */ }
  ```

#### `long long safe_stoll(const string &s, long long max_value)`
- **作用**：带溢出检查的字符串转整数函数，支持以 `0x`/`0X` 前缀表示的十六进制，其余情况视为十进制。
- **流程**：
  1. 若检测到十六进制前缀则调整进制，跳过前缀部分。
  2. 逐字符调用 `ch_to_digit` 校验当前字符是否属于目标进制；出现非法字符或数字超出进制范围即抛出 `CE` 字符串异常。
  3. 在累乘加和前执行溢出检查，若超过 `max_value` 也抛出 `CE` 异常。
- **返回值**：成功解析后的 `long long` 数值。
- **典型用法**：
  ```cpp
  try {
      auto value = safe_stoll("0x1F", UINT32_MAX);
  } catch (const string &err) {
      // 处理 "CE, invalid digit..." 或 "CE, number too large..." 错误
  }
  ```
- **注意事项**：函数不处理 Rust 风格的类型后缀（如 `_i32`），需上层在调用前剥离 suffix。
