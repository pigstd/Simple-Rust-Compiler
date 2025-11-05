### semantic/scope 模块

该模块负责构建作用域树并维护符号表，是语义分析第一阶段的核心。每个作用域使用 `Scope` 结构建模，并通过 `ScopeBuilder_Visitor` 在遍历 AST 时建立。

#### Scope 结构
- `weak_ptr<Scope> parent` 与 `vector<Scope_ptr> children`：形成树状层级，避免循环引用。
- `ScopeKind kind`：区分 `Root`、`Block`、`Function`、`Impl` 四类作用域。
- `map<string, TypeDecl_ptr> type_namespace` / `map<string, ValueDecl_ptr> value_namespace`：记录类型与值命名空间的符号，支持重名检测。
- `string impl_struct` 与 `RealType_ptr self_struct`：`Impl` 作用域下用于追踪 `impl SomeStruct` 的目标类型，后续类型解析会把真实 `StructRealType` 写入 `self_struct`。
- `bool is_main_scope`、`bool has_exit`：用于检查入口函数与 `exit` 调用要求。

#### ScopeBuilder_Visitor
- 继承自 `AST_Walker`，遍历 AST 时：
  * 在进入函数、块、impl 时创建新作用域并入栈，离开时出栈。
  * 将每个 AST 节点的 `NodeId` 映射到当前作用域，填充 `node_scope_map`。
- 作用域归类策略：
  * 顶层 `FnItem`、`StructItem`、`EnumItem`、`ConstItem` 会立即向当前作用域注册相应的 `FnDecl`、`StructDecl`、`EnumDecl`、`ConstDecl`，并检测重复定义。
  * `FnItem` 的函数体块不会再次创建匿名块作用域，借助 `block_is_in_function` 标志避免重复嵌套。
  * `ImplItem` 会创建 `ScopeKind::Impl` 子作用域，并将 `node.struct_name` 存入 `impl_struct`，供后续阶段解析 `Self`。
- 遍历任意节点前均将 `node_scope_map[node.NodeId]` 设为当前作用域，为后续语义分析（查找常量、作用域定位等）提供索引。