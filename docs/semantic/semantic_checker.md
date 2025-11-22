### semantic/semantic_checker 模块

`Semantic_Checker` 统筹语义分析的四个阶段，从作用域构建到类型推断逐步完成。该结构在 `parser` 生成 `vector<Item_ptr>` 后直接操作 AST。

#### 主要成员
- `Scope_ptr root_scope`：根作用域。
- `map<size_t, Scope_ptr> node_scope_map`：记录每个节点所属作用域。
- `map<size_t, RealType_ptr> type_map`：缓存 AST 类型节点解析出的 `RealType`。
- `map<size_t, FnDecl_ptr> fn_item_to_decl_map`：`FnItem` 的 `NodeId` 到注册的 `FnDecl` 的映射，便于后续阶段直接找到声明对象。
- `map<size_t, FnDecl_ptr> call_expr_to_decl_map`：记录每个函数调用表达式最终绑定到的 `FnDecl`，供 IR 生成直接复用。
- `vector<Expr_ptr> const_expr_queue`、`map<size_t, size_t> const_expr_to_size_map`：追踪数组大小等常量表达式。
- `map<ConstDecl_ptr, ConstValue_ptr> const_value_map`：保存常量定义的求值结果。
- `map<size_t, OutcomeState> node_outcome_state_map`：控制流分析结果。
- `map<size_t, pair<RealType_ptr, PlaceKind>> node_type_and_place_kind_map`：表达式类型与左值属性。
- `map<Scope_ptr, Local_Variable_map> scope_local_variable_map`：各作用域内的局部 `let` 绑定。
- `vector<std::tuple<RealTypeKind, string, FnDecl_ptr>> builtin_method_funcs` / `builtin_associated_funcs`：内建方法与关联函数。

#### 工作流程
1. **`step1_build_scopes_and_collect_symbols()`**  
   使用 `ScopeBuilder_Visitor` 遍历 AST，构建完整的作用域树并收集初始符号（函数、结构体、枚举、常量）。同时为所有节点写入 `node_scope_map`。

2. **`step2_resolve_types_and_check()`**  
   深度优先遍历作用域（`Scope_dfs_and_build_type`）：
   - `impl` 作用域会解析 `self_struct` 并把方法/关联函数/关联常量挂入目标结构体的声明对象。
   - 对所有结构体字段、函数参数和返回类型、常量类型调用 `find_real_type()`，在生成 `FnDecl` 的同时把 `FnItem` 的 `NodeId` 写入 `fn_item_to_decl_map`。
   - 校验重复定义、`impl` 中 receiver 的合法性，以及入口函数 `main` 是否存在。

3. **`add_builtin_methods_and_associated_funcs()`**  
   向根作用域注入内建函数（`print`、`println`、`exit` 等），并填充内建方法/关联函数表，例如 `String::from`、`Array::len`、`u32::to_string`。

4. **`step3_constant_evaluation_and_control_flow_analysis()`**  
   - `OtherTypeAndRepeatArrayVisitor` 记录 let/结构体字面量等节点的类型需求并补充常量表达式队列。
   - `ConstItemVisitor` 首先计算所有 `const` 值，再对 `const_expr_queue` 中的表达式逐一求值，写入 `const_expr_to_size_map`。
   - `ControlFlowVisitor` 分析每个节点的控制流结果，检查 `break/continue` 是否在循环内。

5. **`step4_expr_type_and_let_stmt_analysis()`**  
   - `ArrayTypeVisitor` 根据 `const_expr_to_size_map` 回填数组真实长度。
   - `ExprTypeAndLetStmtVisitor` 推断所有表达式的类型与 `PlaceKind`，同时处理 `let` 绑定、函数参数引入、内建方法解析、`as` 转换检查等；当解析到 `CallExpr` 并确定目标函数时，会把该表达式的 `NodeId` → `FnDecl` 写入 `call_expr_to_decl_map`。

执行完上述步骤后，语义分析阶段所需的类型、常量、控制流及局部变量信息全部准备就绪，可供后续（如代码生成）直接使用。
