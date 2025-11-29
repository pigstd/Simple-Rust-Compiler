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
    void emit_const(ConstDecl_ptr decl, const string &suffix);
    void emit_function_decl(FnDecl_ptr decl, const string &suffix,
                            bool define_body);
    string current_scope_suffix() const;
};
```
- 构造函数注入 IRBuilder、TypeLowering 以及 `const_value_map`，内部成员包括：
  - `ir::IRModule &module_`
  - `ir::IRBuilder &builder_`
  - `TypeLowering &type_lowering_`
  - `map<ConstDecl_ptr, ConstValue_ptr> &const_values_`
  - `unordered_map<string, GlobalValue_ptr> globals_`：按符号名保存已创建的全局变量。
  - `vector<string> scope_suffix_stack_`：DFS 过程中记录“从根到当前 scope 的路径”。只在**进入新的子 scope**前压入 `".N"` 片段（`N` 为该子 scope 在父节点内的计数），退出时弹出，栈内容串联后即为类似 `".0.1"` 的作用域后缀。
- `emit_scope_tree` 执行顺序：
  1. 深度优先遍历 `Scope` 树，通过 `visit_scope` 在每个节点先完成结构体的“三阶段”注册：第一遍 `declare_struct_stub` 建立占位，第二遍 `define_struct_fields` 写入字段，第三遍（新增）针对本节点的所有 `StructDecl` 构造临时 `StructRealType` 并调用 `TypeLowering::size_in_bytes`，把聚合大小预先缓存；这样后续无论是 IRGen 还是测试都能直接命中 `size_in_bytes` 的缓存无需再次递归。
  2. 在结构体处理完毕后访问 `value_namespace` 中的函数/常量，最后递归子 scope。遍历结束后由 `IRModule::serialize()` 负责格式化输出顺序。

#### Item 遍历与符号命名
`visit_scope` 统一贯穿整个 Scope 树：在进入某个 scope 时，先把 `type_namespace` 里的结构体拆成“占位”和“定义”两遍，确保相互引用不会因 `map` 顺序出错；再处理 `value_namespace`（函数、常量），最后递归子节点。语义阶段已经把函数/常量按作用域注册好，因此不需要再回到 AST；仅需依据 `Scope` 中的 decl 生成 IR。`scope_suffix_stack_` 只在递归子 scope 时更新：每次进入子 scope 之前先用局部 `local_counter` 分配一个 `"." + counter` 片段压栈，离开时弹栈，于是每个 scope 都拥有一条稳定的 DFS 路径。
- **结构体**：为避免 `map` 遍历顺序导致的依赖问题，`visit_scope` 在处理 `type_namespace` 时会分两遍：
  1. 第一遍调用 `TypeLowering::declare_struct_stub`，只根据 `StructDecl->name` 创建 `%Name = type {}` 占位并写入缓存，确保同一作用域内的结构体即使互相引用也能先拿到 `StructType`。
  2. 第二遍再调用 `TypeLowering::define_struct_fields`，此时所有依赖的结构体都已经有占位，填充字段顺序与 `StructDecl::field_order` 保持一致。
  这样就算 `type_namespace` 的 `map` 字典序与声明顺序不同，也不会出现“先处理 Outer 但 Inner 尚未注册”的异常。
- **枚举**：语义上用 `i32` 表示，本阶段什么都不用做——既无需写出声明，也不再重复登记；TypeLowering 会在需要时把 `RealTypeKind::ENUM` 统一映射为 `i32`。
- **函数**：`emit_function_decl` 直接调用 `IRModule::define_function` 创建 `define <sig> @symbol(...)` 的函数壳，并为每个形参分配 `%arg.N` 名字；基本块和具体指令在步骤 4 中填充。命名规则：
  1. `scope_suffix_stack_` 代表当前 scope 的 DFS 路径，例如 `{".0", ".1"}` 拼成 `".0.1"`；根作用域路径为空。
  2. 处理某个 scope 的值声明时直接把这条路径附在原名后面：若路径为空，则保留原名；若路径非空，则使用 `原名 + 路径`。数组常量额外在前缀处加 `"const."`。
- 形参命名：按照 `FnDecl::parameters` 的顺序生成 `%arg.N`，`N` 从 0 递增，与 `IRFunction::add_param` 的顺序一致。函数没有显式参数时签名内保持空列表。
- **子 scope**：只有在递归子 scope 之前会使用 `local_counter` 生成新的 `"." + counter` 片段并压入 `scope_suffix_stack_`，退出时弹出即可。
- **常量 `const`（仅数组）**：`emit_const` 只接受数组类型的声明：
  1. 调用 `TypeLowering::lower(const_decl->const_type)` 得到 `[N x T]`，并从 `const_value_map` 中取出同一个 `ConstDecl` 的求值结果。
  2. 直接构造符号名 `"const." + const_decl->name + suffix` 并回写，保证后续引用使用同一符号。
  3. 将 `ConstValue` 递归格式化成 `[ T elem0, ... ]` 或 `{ ... }`，写入 `IRModule` 的全局表。
- **字符串字面量**：若后续阶段需要把 `&str` literal 或任意字节序列落成全局，可直接调用 `IRBuilder::create_string_literal(text)`（已封装好 `[len x i8]` 全局和 `c"..."` 编码），无需另外的 helper。
符号命名直接拼接“语义名 + DFS 路径”：
- 函数使用 `decl->name + suffix`，根作用域因路径为空保持原名。
- 数组常量在原名前额外加 `const.`，再拼接路径，例如 `@const.NAME.0.1`。
- 当前设计不再额外检查冲突，如需拓展可在未来恢复检测逻辑。

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
