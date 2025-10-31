### parser 模块

该模块基于 Pratt 算法实现语法分析器，将 `Lexer` 生成的 token 序列还原为 AST。解析过程中大量复用 `ast/ast.h` 中的共享指针节点，并在完成语法树构建后使用 `ASTIdGenerator` 为每个节点分配唯一 `NodeId`，以便后续语义阶段引用。

#### Parser 类概览
- 构造函数 `Parser(Lexer lexer_)` 以值传递保存词法分析器的状态拷贝。
- `vector<Item_ptr> parse()`：循环调用 `parse_item()` 直至 token 耗尽，随后遍历顶层 item，将节点交给 `ASTIdGenerator` 赋予唯一编号。
- `Item_ptr parse_item()`：根据当前 token 派发到 `parse_fn_item`、`parse_struct_item`、`parse_enum_item`、`parse_impl_item` 或 `parse_const_item`，若遇到未知项则抛出 `PE`/`CE` 异常。

#### 顶层 Item 解析
- `parse_fn_item()`：解析函数签名（含方法接收者 `self` 变体）与 `BlockExpr` 形式的主体，并以 `FnItem` 表示。参数列表通过 `parse_pattern()`、`parse_type()` 获取。
- `parse_struct_item()`：读取结构体字段列表，字段类型由 `parse_type()` 解析。
- `parse_enum_item()`：只支持常量枚举，按顺序收集变体名称。
- `parse_impl_item()`：识别 `impl Struct { ... }` 形式，当前实现只接受方法集合。
- `parse_const_item()`：解析常量的显式类型与初始化表达式。
- `parse_item_statement()` 允许在块内部再次出现 item，并包装为 `ItemStmt`。

#### 语句与块
- `parse_statement()`：区分 `let`、item、控制流表达式和常规表达式四类语句。`if/while/loop/{...}` 在语句上下文中允许省略分号。
- `parse_let_statement()`：处理 `let pattern: Type (= expr)?;`，允许缺省初始化器。
- `parse_expr_statement()`：解析任意表达式并记录尾部分号状态，为块尾随表达式提供支持。
- `parse_block_expression()`：以 `{` 开始的语句块，内部循环调用 `parse_statement()`，根据最后一个语句是否带分号决定 `tail_statement`，并在必要时为无分号的控制流表达式打上 `must_return_unit` 标记。

#### 表达式族
- 控制流表达式：`parse_if_expression()` 支持 `else if` 链，`parse_while_expression()`、`parse_loop_expression()` 处理循环语法并直接返回 `Expr_ptr`。
- `parse_block_expression()` 上述描述的块表达式也是表达式的一部分。
- `parse_expression(int rbp = 0)`：核心 Pratt 解析函数，通过 `nud()` 解析前缀表达式，再根据绑定力与 `led()` 解析中缀/后缀运算。
  * `nud(Token)` 负责字面量、标识符、数组字面量 `[ ]`、重复数组 `[expr; n]`、`Self`/`Struct` 构造、括号分组、控制流表达式以及 `break/continue/return` 等前缀结构，同时根据后续 token 识别路径或结构体初始化。
  * `led(Token, Expr_ptr left)` 根据运算符类型构造 `BinaryExpr`、`CallExpr`、`FieldExpr`、`IndexExpr`、`CastExpr` 等节点，赋值运算符的右结合性通过 `get_rbp()` 调整。
  * `get_binding_power()` 返回 `(nbp, lbp, rbp)` 元组，涵盖所有支持的前缀/中缀运算符以及函数调用、字段访问、数组索引等后缀操作。`get_nbp/get_lbp/get_rbp` 分别抽取并校验对应的绑定力。

#### 类型与模式
- `parse_pattern()`：支持 `&?mut? identifier` 形态的绑定模式，生成 `IdentifierPattern`。
- `parse_type()`：解析引用（`&`、`&mut`）、数组类型 `[T; expr]`、单位类型 `()`、`Self` 与路径类型；数组长度表达式立即入栈等待后续常量折叠处理。

#### 错误处理与用法
- 所有不匹配场景均通过 `lexer.consume_expect_token()` 抛出描述清晰的 `CE/PE` 错误，方便定位语法问题。
- 典型用法：
```cpp
Lexer lexer;
lexer.read_and_get_tokens();
Parser parser(std::move(lexer));
auto items = parser.parse(); // 获取完整的 AST item 列表，并完成 NodeId 标注
```
返回的 `items` 可直接交由语义分析模块进一步处理。
