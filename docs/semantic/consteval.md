### semantic/consteval 模块

该模块实现常量表达式求值，负责在语义分析第三阶段解析 `const` 声明、数组长度以及 `repeat` 数组的大小。

#### ConstValue 层级
- `ConstValueKind` 区分 `ANYINT`、`I32`、`U32`、`ISIZE`、`USIZE`、`BOOL`、`CHAR`、`UNIT`、`ARRAY`。
- 对应的派生类（如 `I32_ConstValue`、`Bool_ConstValue` 等）存储具体值。
- `Array_ConstValue` 以 `vector<ConstValue_ptr> elements` 保存静态数组字面量的元素结果。

#### ConstItemVisitor
继承自 `AST_Walker`，在遍历过程中维护多张表：
- `map<size_t, Scope_ptr> &node_scope_map`：查找标识符所属作用域。
- `map<ConstDecl_ptr, ConstValue_ptr> &const_value_map`：缓存常量定义的求值结果。
- `map<size_t, RealType_ptr> &type_map`：提供类型信息以便转换。
- `map<size_t, size_t> &const_expr_to_size_map`：记录数组长度与 `repeat` 中的大小表达式对应的常量值。
- `vector<Expr_ptr> &const_expr_queue`（由外部传入）中的每个表达式会单独运行一次 visitor 以获取大小。

核心辅助函数：
- `parse_literal_token_to_const_value()`：将字面量 token 转成 `ConstValue`，处理整型后缀与布尔/字符字面量。
- `calc_const_unary_expr()` / `calc_const_binary_expr()`：对支持的运算符进行常量折叠，包含溢出、除零等检测。
- `cast_anyint_const_to_target_type()`、`const_cast_to_realtype()`：在需要时将 `ANYINT` 或其它整型转换为目标类型。
- `calc_const_array_size()`：确保数组大小表达式的值为 `usize` 或兼容的整数。

工作流程：
1. **第一遍**：`Semantic_Checker` 以 `is_need_to_calculate = false` 遍历整个 AST，求出所有 `const` item 的值并写入 `const_value_map`，同时在遍历过程中把数组长度等常量表达式填入 `const_expr_queue`。
2. **第二遍**：对队列中的表达式逐一设置 `is_need_to_calculate = true` 再次访问，最终把求出的大小存入 `const_expr_to_size_map`。

在求值过程中若遇到未定义常量、类型不匹配或不支持的表达式，visitor 会抛出 `CE` 字符串异常。
