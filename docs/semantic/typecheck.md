### semantic/typecheck 模块

该模块实现语义分析的第四阶段：补全数组类型信息、推断每个表达式的真实类型，并处理 `let` 绑定的引入与合法性检查。

#### 基础工具
- `PlaceKind`：标记表达式是否可作为左值：
  * `NotPlace`：纯右值（字面量、算术结果等）。
  * `ReadOnlyPlace`：不可变左值（不可变变量、只读引用）。
  * `ReadWritePlace`：可写左值（可变变量、可变引用解引用）。
- `type_merge(left, right, is_assignment)`：合并两个类型，支持 `Never` 与 `AnyInt` 的特殊规则，以及 `&mut T` 到 `&T` 的隐式收缩。用于 if/loop 分支合并、赋值类型匹配等。
- `type_is_number()`、`type_of_literal()`、`copy()`：分别判断是否整数类型、根据字面量推断类型并校验后缀、深拷贝 `RealType`。

#### ArrayTypeVisitor
- 遍历 AST（继承 `AST_Walker`），当访问到 `ArrayType` 时，从 `type_map` 中取出对应的 `ArrayRealType`，再根据 `const_expr_to_size_map` 补写 `size` 字段。
- 其余节点直接委托基类递归，确保不会改变原有遍历逻辑。

#### ExprTypeAndLetStmtVisitor
负责主体类型推断与 `let` 检查：
- 维护的核心成员：
  * `map<size_t, pair<RealType_ptr, PlaceKind>> &node_type_and_place_kind_map`：为每个节点存储推断出的类型和左值属性。
  * `map<size_t, Scope_ptr> &node_scope_map`、`map<Scope_ptr, Local_Variable_map> &scope_local_variable_map`：提供符号查找与局部变量表。
  * `map<size_t, RealType_ptr> &type_map`、`map<size_t, size_t> &const_expr_to_size_map`：获取预解析的类型与数组长度。
  * 控制流信息 `map<size_t, OutcomeState> &node_outcome_state_map`，用于推断 `loop`、`if` 的返回类型。
  * `builtin_method_funcs`、`builtin_associated_funcs`：存放编译器内建的方法/关联函数签名，按 `RealTypeKind` 和名称匹配。
- 关键逻辑：
  * `find_value_decl` / `find_type_decl` 先查局部 `LetDecl`，再查作用域的值/类型命名空间。
  * `get_method_func()`、`get_associated_func()` 根据基础类型、PlaceKind、函数名解析方法或关联函数，并在不存在时回退到内建表。
  * `check_let_stmt()` 校验 `let` 绑定的初始值类型是否可赋给目标类型，同时识别字面量溢出等错误。
  * `intro_let_stmt()` 将模式中绑定的变量插入当前作用域的局部变量表，供后续标识符解析。
  * `check_cast()` 验证 `as` 转换合法性。
  * `visit(FnItem &)` 期间更新 `now_func_decl`，用于识别 `return`/`exit` 要求，遍历函数体时将参数以 `LetDecl` 形式引入函数作用域。
  * 对于 `BinaryExpr`、`UnaryExpr`、`CallExpr` 等节点，根据运算规则合并类型；控制流结构（`if/loop`）结合 `node_outcome_state_map` 判断分支是否必定返回，从而允许推断结果类型为 `Never` 或 `()`。

#### 结果
语义检查完成后：
- `node_type_and_place_kind_map` 存有每个表达式节点的类型与可变性信息。
- `scope_local_variable_map` 含有每个作用域内注册的局部变量，供后续阶段（若有）复用。
