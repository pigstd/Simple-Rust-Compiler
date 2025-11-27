### semantic/decl 模块

该模块描述语义分析阶段使用的声明对象，负责在作用域树中保存结构体、枚举、函数、常量以及 `let` 绑定的元信息。所有声明都通过 `std::shared_ptr` 管理，避免与 AST 节点形成循环引用。

- #### 基础类型
- `TypeDecl`/`ValueDecl`：分别表示类型空间与值空间的基类，内含枚举 `TypeDeclKind`、`ValueDeclKind` 标识具体种类，并统一持有 `string name`，在语义阶段注册声明时立即填入，方便所有后续阶段直接读取。
- `StructDecl`：
  * `vector<string> field_order`：记录字段声明顺序，`fields` 在以 `map` 维护类型时仍能保持原始顺序。
  * `StructItem_ptr ast_node`：指向结构体的 AST 节点，用于报错或补充诊断。
  * `map<string, RealType_ptr> fields`：在类型解析阶段填充字段的真实类型。
  * `methods`、`associated_func`、`associated_const`：在 `impl` 解析后记录方法、关联函数与关联常量。
- `EnumDecl`：
  * `EnumItem_ptr ast_node`。
  * `map<string, int> variants`：记录枚举变体及其分配值（按出现顺序自增）。

#### 值声明
- `FnDecl`：
  * `FnItem_ptr ast_node` 与 `weak_ptr<Scope> function_scope` 关联到函数体所在作用域。
  * `vector<pair<Pattern_ptr, RealType_ptr>> parameters`、`RealType_ptr return_type` 在第二阶段解析类型后填充。
  * `fn_reciever_type receiver_type` 标记是否有 `self` 形参，`weak_ptr<StructDecl> self_struct` 指明所属结构体。
  * `is_main`、`is_exit`、`is_builtin`、`is_array_len`：分别表示是否是入口函数、`exit`、编译器注入的内建函数，以及“数组 `len`”这种在 IR 阶段可当常量处理的特殊内建。
- `ConstDecl`：保存常量 AST 节点与求值后类型 `const_type`。
- `LetDecl`：表示 `let` 引入的局部变量，基类中的 `name` 保存展开模式后的绑定名（对 `_` 或解构可置空），并记录真实类型 `let_type` 与可变性 `is_mut`。

#### 辅助查找
- `ConstDecl_ptr find_const_decl(Scope_ptr NowScope, string name)`：沿父作用域向上查找常量声明。
这个函数在常量求值时使用。
