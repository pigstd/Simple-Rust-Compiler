### IR/Globals 生成模块

本章节描述 PLAN.md 第三步（全局项与内建运行时支持生成）的整体设计、接口和测试计划。目标是把语义阶段可见的每个 `Item` 映射成 IR 模块级别的定义，并在模块入口注入所有必须的内建运行时符号，保证后续函数体 lowering 可以直接引用。

#### 设计目标
- 统一自根作用域起遍历整个 `Scope` 树，访问每个节点的 `type_namespace` / `value_namespace`，把所有 `struct/enum/fn/const` 映射到 `IRModule`：`struct` 生成类型声明、`enum` 跳过、`fn` 输出函数定义壳、`const` 仅在数组场景下生成全局指针。
- 依赖 `TypeLowering` 的结果，不重新做类型推断：结构体、枚举布局必须与 `docs/IR/type-lowering.md` 描述一致，且命名结构体通过 `IRModule::add_type_definition` 只声明一次。
- 只需要 `Semantic_Checker` 的 `const_value_map` 即可获取数组常量的求值结果，scope 遍历直接使用 `Scope` 内部保存的声明对象。
- 只生成数组形态的 `const` 全局存储，并约束 `.data`（常量段）与 `.text`（函数段）之间的输出格式，方便文本 IR 的自动测试。

#### 输入依赖与上下文
- `ir::IRModule &module`：已经在 IRBuilder 阶段创建的模块对象，包含基础类型池和文本序列化工具。
- `TypeLowering &type_lowering`：负责 `RealType -> IRType` 映射与标量常量 `ConstValue -> ir::ConstantValue` 的桥接。
- 语义阶段产物：
  - `map<ConstDecl_ptr, ConstValue_ptr> const_value_map`：获取 `const` 初始化表达式的求值结果。

#### GlobalLoweringDriver 类
该模块落地在 `src/ir/global_lowering.{h,cpp}`，核心入口类如下：
```cpp
class GlobalLoweringDriver {
  public:
    GlobalLoweringDriver(ir::IRModule &module,
                         ir::IRBuilder &builder,
                         TypeLowering &type_lowering,
                         map<ConstDecl_ptr, ConstValue_ptr> &const_values);

    // 入口：传入语义阶段构建好的根 Scope，深度优先遍历并完成所有 struct/enum/fn/const 的 lowering。
    void emit_scope_tree(Scope_ptr root_scope);

  private:
    void visit_scope(Scope_ptr scope);
    void emit_const(ConstDecl_ptr decl);
    void emit_function_decl(FnDecl_ptr decl, bool define_body);
    GlobalValue_ptr ensure_global_bytes(string symbol,
                                        const std::vector<uint8_t> &bytes,
                                        bool is_constant);
    string allocate_symbol(string_view base);
};
```
- 构造函数注入 IRBuilder、TypeLowering 以及 `const_value_map`，内部成员包括：
  - `ir::IRModule &module_`
  - `ir::IRBuilder &builder_`
  - `TypeLowering &type_lowering_`
  - `map<ConstDecl_ptr, ConstValue_ptr> &const_values_`
  - `unordered_map<string, GlobalValue_ptr> globals_`：按符号名保存已创建的全局变量，`allocate_symbol` 会在写入前检查此表避免重名。
  - `vector<string> scope_suffix_stack_`：DFS 过程中记录当前作用域的后缀（例如 `".0.1"`），确保命名唯一。
- `emit_scope_tree` 执行顺序：
  1. 深度优先遍历 `Scope` 树，通过 `visit_scope` 在每个节点先处理 `type_namespace` 中的结构体（`TypeLowering::declare_struct` + `IRModule::add_type_definition`），再处理 `value_namespace` 中的函数/常量，最后递归子 scope。遍历结束后由 `IRModule::serialize()` 负责格式化输出顺序。

#### Item 遍历与符号命名
`visit_scope` 统一贯穿整个 Scope 树：在进入某个 scope 时，先把 `type_namespace` 里的结构体交给 `TypeLowering::declare_struct` 并立刻 `IRModule::add_type_definition`，确保后续子节点和函数都能引用；再处理 `value_namespace`（函数、常量），最后递归子节点。语义阶段已经把函数/常量按作用域注册好，因此不需要再回到 AST；仅需依据 `Scope` 中的 decl 生成 IR。
- **结构体**：`visit_scope` 遇到 `StructDecl` 时立即调用 `TypeLowering::declare_struct` 注册 `%StructName` 并 `IRModule::add_type_definition` 写入字段布局，保证后续子作用域可以引用。
- **枚举**：语义上用 `i32` 表示，本阶段什么都不用做——既无需写出声明，也不再重复登记；TypeLowering 会在需要时把 `RealTypeKind::ENUM` 统一映射为 `i32`。
- **函数**：`emit_function_decl` 直接调用 `IRModule::define_function` 创建 `define <sig> @symbol(...)` 的函数壳，并为每个形参分配 `%arg.N` 名字；基本块和具体指令在步骤 4 中填充。函数命名在遍历 `Scope` 树时按 DFS 路径生成唯一后缀：
  1. 每个 scope 在 `visit_scope` 开始时声明局部 `size_t local_counter = 0;`。处理需要命名的实体或递归子 scope 时，先将 `N = local_counter++` 推入 `scope_suffix_stack_`。若栈推入后非空，则把当前 `FnDecl` 命名为 `原名 +` 所有栈元素拼出的后缀（例如栈 `{0,1}` → `.0.1`）；若栈之前为空，则保持原名。
  2. 子 scope 结束时弹出栈顶，确保兄弟节点获得新的编号。
  2. 对 `FnDecl`，使用上述规则得到的名字回写到 `decl->name` 中，使后续阶段共享同一符号。
  3. `ConstDecl` 的数组全局同理，直接在 `emit_const` 中拼接对应后缀。
- 这样保证即使处在同一个作用域内出现多个同名函数/常量，也能借由层级计数自动区分。
- **常量 `const`（仅数组）**：`emit_const` 只接受数组类型的声明：
  1. 调用 `TypeLowering::lower(const_decl->const_type)` 得到 `[N x T]`，并从 `const_value_map` 中取出同一个 `ConstDecl` 的求值结果。
  2. 调用 `allocate_symbol(const_decl->name)` 获取不会冲突的 IR 名字，并写回 `const_decl->name`，保证后续引用使用同一符号。
  3. 将 `ConstValue` 递归格式化成 `[ T elem0, ... ]` 或 `{ ... }`，写入 `IRModule` 的全局表。
符号命名使用 `allocate_symbol(base)`：
- `base`：一个语义友好的标识符，例如 `const::Foo::BAR` 或 `fn::main`，便于定位来源。
- 若冲突，函数自动在末尾追加 `.N` 计数器。

#### 数组常量池
数组常量使用 `ensure_global_bytes` helper，把元素 `ir::ConstantValue` 序列编码为十六进制 `\XX`，确保不同目标平台 byte 序列一致。

#### 输出段与序列化
- `IRModule` 输出顺序为“类型定义 → 全局常量/变量 → 函数声明 → 函数定义”，因此第三步生成的内容也仅需按这一顺序注册即可。
- `.data` 段概念暂不引入，数组常量通过 `IRModule::globals_` 直接输出：
    ```llvm
    @const.FOO = private constant [2 x i32] [ i32 1, i32 2 ]
    @arr.bytes.0 = private constant [4 x i8] c"\01\02\03\04"
    ```
`GlobalLoweringDriver` 仅负责往 `IRModule` 注册类型/全局/函数，最终文本输出由外部调用 `module.serialize()` 负责。

#### 测试计划
1. **结构体/数组全局**：编写一个包含结构体声明和数组 `const` 的语言源码，通过 `Semantic_Checker` 获得 `root_scope` 与 `const_value_map`，调用 `GlobalLoweringDriver` 生成 IR，验证输出的 `StructType` 布局与数组初始化文本。
2. **常量互相引用**：源码中定义 `const A: [2; i32] = ...; const B = A;` 等链式引用，验证 `const_value_map` 已在语义阶段把 `B` 的值完全展开，IR 中独立输出 `@const.A` 和 `@const.B`，且二者文本指向不同的常量块（值相同但不复用指针）。
3. **数组长度 helper**：源码包含多个不同长度的数组 `const`，检查生成的 `.data` 段（或 `IRModule` 中的全局符号）是否唯一并符合命名约定。

这些测试的流程：
- 编译指定的源码片段至语义阶段，收集 `root_scope`/`const_value_map`；
- 构造一个 `IRModule` + `GlobalLoweringDriver`，运行 `emit_scope_tree`；
- 直接读取 `IRModule::serialize()` 的文本或遍历模块结构，进行断言；
- 无需进入函数体 lowering 或 runtime 链接阶段。

实现阶段请严格对照本文档，任何新增 helper 或运行时符号都需在此文同步记录。
