### ast 模块

该模块集中声明语法树（AST）的全部节点类型、枚举和指针别名，供解析、语义分析、代码生成等阶段统一复用。所有节点均通过访问器模式暴露行为，并使用 `std::shared_ptr` 统一管理生命周期，方便在树结构中共享节点。

#### 基类与指针别名
- **AST_Node**：所有节点的抽象基类，包含 `size_t NodeId` 字段，构建完整 AST 后由外部分配唯一编号，用于调试和基于映射的节点索引；纯虚函数 `accept(AST_visitor &)` 为访问器模式入口。
- **派生基类**：`Expr_Node`、`Stmt_Node`、`Item_Node`、`Type_Node`、`Pattern_Node` 分别作为表达式、语句、项、类型、模式的抽象基类，统一重写 `accept`。
- **共享指针别名**：模块为所有节点及其前置声明提供 `Expr_ptr`、`Stmt_ptr`、`Item_ptr`、`Type_ptr`、`Pattern_ptr` 等别名，代码中统一使用指针而非常量引用，减少拷贝并支持递归结构。

#### 可变性与引用描述
- **Mutibility**：标记标识符是否为 `MUTABLE` 或 `IMMUTABLE`，配合 `mutibility_to_string` 在诊断信息中输出关键字 `mut`。
- **ReferenceType**：描述类型是否以引用方式出现，枚举值 `NO_REF`、`REF`、`REF_MUT` 对应 `T`、`&T`、`&mut T`。`reference_type_to_string` 在调试输出中还原 Rust 风格前缀。
- **Type_Node**：持有 `ReferenceType ref_type` 并在构造函数中显式指定引用类别，所有具体类型节点都继承自它。

#### 运算与字面量枚举
- **LiteralType**：覆盖 `NUMBER`、`STRING`、`CHAR`、`BOOL` 四类字面量（`CSTRING` 预留注释）。`literal_type_to_string` 统一转为可读文本。
- **Binary_Operator**：收录算术、位运算、逻辑、比较以及复合赋值等运算符。`binary_operator_to_string` 返回原始符号，语义分析可复用。
- **Unary_Operator**：囊括 `NEG`、`NOT`、`REF`、`REF_MUT`、`DEREF`。配套的 `unary_operator_to_string` 用于调试输出。
- **fn_reciever_type**：描述 `impl` 方法接收者形态，枚举 `NO_RECEIVER`、`SELF`、`SELF_REF`、`SELF_REF_MUT`，并提供 `fn_reciever_type_to_string` 以生成提示文本。

#### 表达式节点（Expr_Node）
- **LiteralExpr**：封装字面量值，保存 `LiteralType literal_type` 与字符串形式 `value`，语义阶段可按类型解析具体常量。
- **IdentifierExpr**：记录单个标识符名字 `name`，在解析阶段解析变量、函数或路径起点。
- **BinaryExpr**：表示双目运算，字段 `Binary_Operator op`、`Expr_ptr left/right`，构造函数对左右操作数采用移动语义降低拷贝。
- **UnaryExpr**：保存 `Unary_Operator op` 及被操作数 `right`，用于解析前缀表达式。
- **CallExpr**：承载函数或可调用对象调用，保存被调用表达式 `callee` 与逐个参数 `arguments`。
- **FieldExpr**：结构体或元组字段访问，`base` 为被访问对象，`field_name` 为字段名。
- **StructExpr**：结构体字面量构造，用 `Type_ptr struct_name` 指向类型（可为 `SelfType`），`fields` 记录字段-表达式对，解析时保证字段名唯一。
- **IndexExpr**：数组或切片索引，`base` 保存被索引表达式，`index` 为索引表达式，语义阶段检查常量性。
- **BlockExpr**：表示 `{ ... }` 块，`statements` 存放块内语句，`tail_statement` 为可选尾随表达式（无分号时非空），`must_return_unit` 标记推断出的返回类型是否强制为 `()`。
- **IfExpr**：包含 `condition`、`then_branch`、`else_branch`（可空）以及 `must_return_unit`，用于在缺少尾随分号时推断返回值是否为 `()`。
- **WhileExpr**：保存循环条件 `condition` 与主体 `body`，默认返回 `()`。
- **LoopExpr**：保存循环主体 `body`，`must_return_unit` 标记是否强制 `()` 返回值，供 break/return 校验。
- **ReturnExpr**：可选 `return_value`，若为空表示 `return;`。
- **BreakExpr**：持有可选 `break_value`，支持 `break expr` 语法，语义分析阶段校验 `loop` 是否允许返回值。
- **ContinueExpr**：无额外字段，仅代表 `continue`。
- **CastExpr**：类型转换表达式，`expr` 为被转换值，`target_type` 为目标类型。
- **PathExpr**：静态路径访问 `base::name`，其中 `base` 要求为类型节点（含 `SelfType`），`name` 为成员或关联函数。
- **SelfExpr / UnitExpr**：分别对应 `self` 与 `()` 字面值，作为零字段节点存在。
- **RepeatArrayExpr**：记录 `[expr; n]` 形式数组，`element` 为被重复元素，`size` 需解析为常量表达式。
- **ArrayExpr**：普通数组字面量，`elements` 存放每个元素表达式。

#### 项节点（Item_Node）
- **FnItem**：函数及方法定义，字段包括函数名 `function_name`、接收者类型 `receiver_type`、形参 `(Pattern_ptr, Type_ptr)` 列表、可空返回类型 `return_type`（`nullptr` 表示 `()`）以及函数体 `body`。构造函数对参数和函数体采用移动语义。
- **StructItem**：结构体定义，保存结构体名 `struct_name` 与字段列表 `fields`（字段名与类型指针）。
- **EnumItem**：简单枚举定义，`variants` 只存储常量枚举项名称。
- **ImplItem**：`impl` 块，包含目标结构体 `struct_name` 与方法列表 `methods`（复用 `Item_ptr`，通常指向 `FnItem`）。
- **ConstItem**：常量定义，要求显式类型 `const_type` 与初始化表达式 `value`，不支持类型推导。

#### 语句节点（Stmt_Node）
- **LetStmt**：`let` 绑定，字段包含匹配模式 `pattern`、显式类型 `type` 与可选初始化器 `initializer`，当为空时表示 `let` 声明但未赋值。
- **ExprStmt**：表达式语句，记录 `expr` 以及布尔值 `is_semi` 指示是否带有分号，用于区分语句表达式与尾随表达式。
- **ItemStmt**：将任意 `Item_ptr` 视为语句出现，例如模块顶层或块内的项。

#### 类型与模式节点
- **PathType**：命名类型（含泛型占位），字段 `name` 为解析后的标识符文本；继承的 `ref_type` 表明是否出现 `&`/`&mut`。
- **ArrayType**：静态数组类型 `[T; n]`，保存元素类型 `element_type` 和常量大小表达式 `size_expr`。
- **UnitType / SelfType**：分别对应 `()` 与 `Self`，仅依赖 `ref_type` 区分引用形态。
- **IdentifierPattern**：唯一受支持的模式类型，字段 `name` 为绑定名，`is_mut` 记录是否带 `mut`，`is_ref` 记录 `&`/`&mut` 修饰，便于语义分析阶段生成适当的绑定属性。

#### 辅助函数与访问器
- 所有节点实现 `accept(AST_visitor &)`，在 `src/ast/ast.cpp` 中与访问器接口协作，实现遍历与语义阶段回调。
- `*_to_string` 系列函数在报错、调试或序列化 AST 时统一输出符号文本，保持与 Rust 语法一致性。

#### 使用示例
```cpp
// 解析表达式：mut x = foo(1) + 2;
auto name = std::make_shared<IdentifierPattern>("x", Mutibility::MUTABLE, ReferenceType::NO_REF);
auto ty = std::make_shared<PathType>("i32", ReferenceType::NO_REF);
auto call = std::make_shared<CallExpr>(
    std::make_shared<IdentifierExpr>("foo"),
    vector<Expr_ptr>{std::make_shared<LiteralExpr>(LiteralType::NUMBER, "1")}
);
auto rhs = std::make_shared<BinaryExpr>(
    Binary_Operator::ADD,
    call,
    std::make_shared<LiteralExpr>(LiteralType::NUMBER, "2")
);
auto let_stmt = std::make_shared<LetStmt>(name, ty, rhs);
```
示例展示了不同节点之间的组合方式：`IdentifierPattern` 与 `PathType` 描述变量绑定，`CallExpr`、`LiteralExpr`、`BinaryExpr` 构造表达式，再通过 `LetStmt` 嵌入语句上下文。访问器可据此遍历并执行类型检查或代码生成。
