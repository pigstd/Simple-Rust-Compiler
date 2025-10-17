### ast 模块

该模块集中声明语法树（AST）的全部节点类型、枚举和指针别名，供解析、语义分析等阶段统一使用。

**抽象基类**
- `AST_Node`：所有节点的基类，字段 `size_t NodeId` 用于分配唯一标识；纯虚函数 `accept(AST_visitor &)` 支持访问器模式。
- `Expr_Node` / `Stmt_Node` / `Item_Node`：表达式、语句、Item 的抽象基类，均继承 `AST_Node` 并覆写 `accept`。
- `Type_Node`：类型节点基类，在 `AST_Node` 基础上增加 `ReferenceType ref_type` 成员，记录 `&T` / `&mut T` 等引用信息；构造函数要求传入引用种类。
- `Pattern_Node`：模式节点基类，当前只用于 `IdentifierPattern`。

**shared_ptr 类型别名**
- `AST_Node_ptr` 以及 `Expr_ptr`、`Stmt_ptr`、`Item_ptr`、`Type_ptr`、`Pattern_ptr` 分别对基类使用 `std::shared_ptr` 包装。
- 每个具象节点也声明同名的 `*_ptr` 别名（如 `LiteralExpr_ptr`、`FnItem_ptr`），方便在解析器和语义检查中直接引用强类型指针。

**枚举与辅助函数**
- `Mutibility { IMMUTABLE, MUTABLE }` 搭配 `mutibility_to_string` 输出读写属性。
- `ReferenceType { NO_REF, REF, REF_MUT }` 搭配 `reference_type_to_string` 描述引用类别。
- `LiteralType { NUMBER, STRING, CHAR, BOOL }` 与 `literal_type_to_string` 标识字面量类型（保留对 C 字符串的扩展注释）。
- `Binary_Operator` 覆盖算术、逻辑、比较、移位以及所有赋值变体，使用 `binary_operator_to_string` 获取可读文本。
- `Unary_Operator { NEG, NOT, REF, REF_MUT, DEREF }` 使用 `unary_operator_to_string` 输出符号名称。

**表达式节点**
- `LiteralExpr`：`LiteralType literal_type`、`string value`，保存字面量原始值。
- `IdentifierExpr`：`string name`，普通标识符。
- `BinaryExpr`：`Binary_Operator op`、`Expr_ptr left/right`，二元运算。
- `UnaryExpr`：`Unary_Operator op`、`Expr_ptr right`，一元运算。
- `CallExpr`：`Expr_ptr callee`、`vector<Expr_ptr> arguments`，函数或方法调用。
- `FieldExpr`：`Expr_ptr base`、`string field_name`（在实现文件中定义），结构体字段访问。
- `StructExpr`：`PathExpr_ptr struct_name` 与 `vector<pair<string, Expr_ptr>> fields`，结构体字面量。
- `IndexExpr`：`Expr_ptr base`、`Expr_ptr index`，数组/切片访问。
- `BlockExpr`：`vector<Stmt_ptr> statements` 与可选 `Expr_ptr tail_statement`，`{ ... }` 表达式。
- `IfExpr`：`Expr_ptr condition`、`BlockExpr_ptr then_branch`、可选 `Expr_ptr else_branch`。
- `WhileExpr`：`Expr_ptr condition`、`BlockExpr_ptr body`。
- `LoopExpr`：`BlockExpr_ptr body`，无限循环。
- `ReturnExpr` / `BreakExpr`：可选 `Expr_ptr` 保存返回或跳出值。
- `ContinueExpr`：不含额外字段，表示 `continue`。
- `CastExpr`：`Expr_ptr expr`、`Type_ptr target_type`，处理 `as` 转换。
- `PathExpr`：携带命名路径信息（如 `Point::new`）。
- `SelfExpr`：代表 `self`，无额外字段。
- `UnitExpr`：代表 `()`，无额外字段。
- `ArrayExpr`：`vector<Expr_ptr> elements`。
- `RepeatArrayExpr`：`Expr_ptr element`、`Expr_ptr size` 表示 `[expr; n]`。

**Item 节点**
- `FnItem`：`string name`、`fn_reciever_type receiver_type`、`vector<pair<Pattern_ptr, Type_ptr>> parameters`、可选 `Type_ptr return_type` 与 `BlockExpr_ptr body`。
- `StructItem`：`string name`、`vector<pair<string, Type_ptr>> fields`。
- `EnumItem`：`string name`、`vector<string> variants`；当前枚举项不携带数据。
- `ImplItem`：实现目标与 `vector<FnItem_ptr> methods`（具体成员在实现中定义）。
- `ConstItem`：`string name`、`Type_ptr const_type`、`Expr_ptr value`，描述 `const` 声明。

**语句节点**
- `LetStmt`：`Pattern_ptr pattern`、`Type_ptr type`、可选 `Expr_ptr initializer`，对应 `let` 绑定。
- `ExprStmt`：`Expr_ptr expr`，普通表达式语句。
- `ItemStmt`：`Item_ptr item`，块级 `item` 语句。

**类型与模式节点**
- `PathType`：保存命名类型路径。
- `ArrayType`：`Type_ptr element_type`、`Expr_ptr size_expr`，表示 `[T; n]`。
- `UnitType`：代表 `()`。
- `SelfType`：代表 `Self`。
- `IdentifierPattern`：`string name`，当前唯一支持的模式形式。

头文件中仅给出结构声明和 `accept` 接口，具体逻辑在 `src/ast` 下实现，供访问器、语义检查和代码生成等阶段复用统一的数据结构。
