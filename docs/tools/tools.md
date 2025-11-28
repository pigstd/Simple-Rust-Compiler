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

#### `long long safe_stoll(const string &s)`
- **作用**：带溢出检查的字符串转整数函数，支持十进制/十六进制字面量及 Rust 风格整数后缀（`i32/u32/isize/usize`，无后缀默认允许 64 位范围）。
- **流程**：
  1. 解析字面量是否包含 `_suffix`。根据后缀决定允许的最大值：`i32/isize` 对应 `INT32_MAX`、`u32/usize` 对应 `UINT32_MAX`、无后缀对应 `LLONG_MAX`。遇到未知后缀抛出 `CE`。
  2. 检查是否为十六进制前缀（`0x`/`0X`）并调整进制。
  3. 逐字符调用 `ch_to_digit` 校验当前字符是否属于目标进制；出现非法字符或数字超出进制范围即抛 `CE` 异常。
  4. 在累乘加和前执行溢出检查，超过对应上限时抛 `CE`。
- **返回值**：成功解析后的 `long long` 数值。
- **典型用法**：
  ```cpp
  try {
      auto value = safe_stoll("0x1F_u32");
  } catch (const string &err) {
      // 处理 "CE, invalid digit..." 或 "CE, number too large..." 错误
  }
  ```
- **注意事项**：函数仅接受非负整数字面量（如需处理负数由语义阶段拆分成一元负号 + 正数）。
