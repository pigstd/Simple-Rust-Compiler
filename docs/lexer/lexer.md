### lexer 模块

该模块提供将源代码拆分为词法符号（token）的全部数据结构与实现。`Token_type` 枚举覆盖了编译器当前支持的括号、运算符、关键字、字面量与标识符类别，并通过 `token_type_to_string()` 统一转为字符串形式便于报错与调试。

#### Token 结构体
- `Token_type type`：记录词法类别。
- `string value`：保留原始词面文本，字符串与字符字面量会在词法阶段去除引号并处理转义。
- 构造函数 `Token(Token_type _type, string _value)` 在创建时填充两个字段；`show_token()` 将类别和值输出到标准输出，便于调试。

#### Lexer 类
`Lexer` 通过一次扫描将标准输入读入 `tokens` 数组，并提供基于游标的消费接口：

- **关键字/符号映射**：构造函数初始化 `keywords_map` 与 `symbol_map`，识别 Rust 关键字以及多字符运算符（`<<=`, `::` 等），后续扫描阶段均以哈希表匹配实现 O(1) 分类。
- **内部状态机**：`read_and_get_tokens()` 维护多种状态位（块注释、行注释、字符串、raw string、字符、数字、标识符），逐字符处理输入。遇到字符串或字符时使用 `get_escape_character()` 解析常见转义，并在 raw string 场景支持任意数量的 `#`。
- **标记生成**：
  * `add_identifier(string &now_str)` 将累积的标识符判定为关键字或普通标识符。
  * `add_number(string &now_str)` 检查数字合法性，支持十进制与 `0x` 开头的十六进制。
  * 对于运算符和分隔符，按照长度 3→2→1 的优先级在 `symbol_map` 中匹配；未识别的字符会抛出 `CE` 异常。
- **错误处理**：词法阶段发现缺失的结束引号、块注释未闭合、数字/标识符中混入非法字符时均抛出字符串形式的 `CE` 错误提示。
- **消费接口**：
  * `peek_token()` 以常量时间查看当前 token，没有更多 token 时抛出异常。
  * `consume_token()` 前进游标并返回当前 token。
  * `has_more_tokens()` 用于循环式解析。
  * `consume_expect_token(Token_type type)` 在匹配失败时直接报告 `CE, expected ...`，常与语法分析配合使用。

#### 使用示例
```cpp
Lexer lexer;
lexer.read_and_get_tokens();
while (lexer.has_more_tokens()) {
    Token tk = lexer.consume_token();
    std::cout << token_type_to_string(tk.type) << ": " << tk.value << "\n";
}
```
示例演示了通过 `read_and_get_tokens()` 构造 token 序列与 `consume_token()` 逐步读取的基本流程，适合作为语法分析前的预处理步骤。
