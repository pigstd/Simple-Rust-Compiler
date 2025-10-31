### semantic/type 模块

该模块定义语义分析阶段使用的真实类型体系（`RealType`），并实现从 AST 类型节点到真实类型的解析流程。

#### RealType 层级
- `RealType`：抽象基类，包含
  * `RealTypeKind kind`：区分 `UNIT`、`NEVER`、`ARRAY`、`STRUCT`、`ENUM`、`BOOL`、`I32`、`ISIZE`、`U32`、`USIZE`、`ANYINT`、`CHAR`、`STR`、`STRING`、`FUNCTION` 等类型。
  * `ReferenceType is_ref`：保留引用修饰（`NO_REF`、`REF`、`REF_MUT`）。
  * 纯虚函数 `show_real_type_info()` 返回调试字符串，并在各派生类型中实现。
- 常用派生类：
  * `UnitRealType`、`NeverRealType` 等基元类型。
  * `ArrayRealType`：包含元素类型 `element_type`、数组大小表达式 `size_expr`（尚未折叠）以及常量大小 `size`。
  * `StructRealType`、`EnumRealType`：保存名称与声明弱引用，便于比较是否同一类型。
  * `FunctionRealType`：指向 `FnDecl`。

#### 类型解析
- `RealType_ptr find_real_type(Scope_ptr current_scope, Type_ptr type_ast, map<size_t, RealType_ptr> &type_map, vector<Expr_ptr> &const_expr_queue)`：
  * 若目标类型已在 `type_map` 中缓存则直接返回。
  * `PathType` 先在当前作用域及其父作用域的 `type_namespace` 查找结构体/枚举；若找不到则匹配内置类型（`i32`、`bool` 等）。
  * `ArrayType` 递归解析元素类型并把 `size_expr` 加入 `const_expr_queue`，稍后由常量求值阶段处理。
  * `SelfType` 仅允许出现在 `impl` 作用域，直接复用该作用域的 `self_struct`。
  * 解析结果写入 `type_map[type_ast->NodeId]`，供后续阶段复用。

#### 其他辅助
- `real_type_kind_to_string()`：将枚举转为便于错误信息展示的字符串。
- `OtherTypeAndRepeatArrayVisitor`：
  * 继承 `AST_Walker`，在语义分析第三阶段遍历 AST。
  * 成员 `node_scope_map`、`type_map`、`const_expr_queue` 与编译器全局状态共享。
  * 解析 let 语句、`as` 转换、路径表达式、结构体字面量、`RepeatArrayExpr` 等节点的类型需求，并将相关表达式的 `NodeId` 与作用域/常量表达式队列关联起来，供后续步骤做类型检查或常量折叠。
