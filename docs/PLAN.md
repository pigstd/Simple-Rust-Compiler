我现在已经完成了 semantic check，我接下来需要利用 semantic check 的信息生成 llvm-IR 的中间表示。

生成 IR 主要有以下几个步骤：

1. **搭建 IR 框架与手写 IR Builder**  
   - 在 `include/src` 下新增 `ir` 模块，定义 `IRModule`、`IRFunction`、`BasicBlock`、`IRValue`、`IRType` 等核心结构，并实现一个 `IRBuilder`，用于维护当前基本块、常量池、字符串表等（细节以 `docs/IR/IRBuilder.md` 为准）。  
   - 将 `Semantic_Checker` 产出的 `node_scope_map`、`node_type_and_place_kind_map`、`const_value_map` 等结构注册到 IR 生成阶段的驱动入口（例如 `IRGenerator`），确保 IRBuilder 的使用者可以随时查询这些语义数据；同时设计自定义的文本 IR 序列化格式（便于调试与测试）。  
   - Tests：新增 `test/IRBuilder/test_ir_builder.cpp`，构造极简 AST，驱动 `Semantic_Checker` 与 IR 生成入口，验证可以创建空 Module、自动注入内建函数声明（`print/println/exit`）并输出格式正确的 IR 文本。  
   - Docs：在 `docs/ir/overview.md` 记录 IR 模块的边界、关键数据结构、IR 文法以及初始化顺序。

2. **实现 `RealType -> IRType` 映射与常量值桥接**  
   - 编写 `TypeLowering` 工具（独立成类或 IR 生成器的辅助模块），涵盖所有语义阶段支持的类型：整数/布尔/字符、`()`、`!`、字符串（`&str` = `{ ptr, i32 }`、`String` = `{ ptr, i32, i32 }`）、数组（利用 `const_expr_to_size_map`）、结构体/枚举（使用 `Scope` 中的字段顺序）等，统一映射到 `IRType`。约定产出的 `IRType` 必须与 `docs/IR/IRBuilder.md` 描述的布局完全一致，并为命名结构体/枚举缓存 `StructType`，方便后续复用。  
   - 处理 `ConstDecl` 与 `const_expr_queue` 的值：仅把 `const_value_map` 中已经求出的结果包成 `ir::ConstantValue`（当前 `IRConstant` 只有这一种形态），并按 `RealType` 推断出目标 `IRType`；本阶段 **不做额外的常量折叠或表达式重写**，复合类型常量仍以 `ConstValue` 形式回传，由下一阶段的全局生成负责决定是否放到全局区。  
   - Tests：`test/TypeLowering/test_type_lowering.cpp` 逐个断言 `RealTypeKind` 到 `IRType` 的 bit width/字段数量/布局正确；额外验证若向 `TypeLowering` 投入 `ConstValue`（如 `Bool_ConstValue`、`I32_ConstValue`）即可得到匹配的 `ir::ConstantValue` 且不会对表达式做常量折叠。  
   - Docs：新增 `docs/IR/type-lowering.md`，详细说明各 `RealTypeKind` 如何落到自研 IR 类型，包括字符串/数组/引用的布局选择与原因，并记录 `ConstValue -> ir::ConstantValue` 的桥接约束。

3. **全局项生成**  
   - 以 `root_scope` 为入口深度优先遍历整棵作用域树：在每个节点先处理结构体（调用 `TypeLowering::declare_struct` 并写入 `IRModule::add_type_definition`），再处理函数/数组常量，最后递归子 scope。  
   - `GlobalLoweringDriver` 负责在遍历过程中根据 DFS 路径生成唯一化后缀（`scope_suffix_stack_`），并通过 `allocate_symbol` 回写到 `FnDecl`/`ConstDecl` 的 `name` 字段，确保任意作用域下的 `len` 等函数都不会冲突。  
   - 仅为数组 `const` 创建 IR 全局，数值来自语义阶段的 `const_value_map`，其余类型的常量留给下一阶段按需加载；不再引入 `.data/.text` 段概念，直接复用 `IRModule` 现有的类型/全局/函数输出顺序。  
   - Tests：编写若干语言源码，跑到语义阶段后调用 `GlobalLoweringDriver::emit_scope_tree`，对 `IRModule::serialize()` 的文本或结构体数据做断言，覆盖结构体定义、函数声明、数组常量以及命名策略。  
   - Docs：`docs/IR/global-lowering.md` 记录上述遍历流程、命名约定、数组常量格式与测试方法。

4. **函数体、语句与表达式的 IR 生成**  
   - 在 `IRGenVisitor` 中实现 `visit(FnItem)`，为每个函数建立入口基本块、根据 `scope_local_variable_map` 分配栈空间（可用 `alloca` 风格的虚拟指令）、填充参数；实现 `visit`/`lower_*` 覆盖 `let`、赋值、控制流（`if/while/loop/break/continue/return`）、算术运算、比较、逻辑、数组/结构体访问、方法与关联函数调用。  
   - 根据 `node_type_and_place_kind_map` 判断需要生成 `load`、`store`、`ptr_add`、`memcpy` 等 IR 指令；借助 `node_outcome_state_map` 正确结束基本块，支持 `Never` 返回值分支；若需要，可实现 `cfg` 验证和死块消除。  
   - Tests：`test/IRBuilder/run_basic_programs.cpp` 读取若干 `.rx` 样例（算术、循环、数组、结构体、break/continue、`String::from`），生成 IR 写入临时文件并交给 IR 解释器/伪执行脚本验证输出；同时保留覆盖 `break value`、`loop` 返回值等语义的单元测试（可直接断言 IR 文本结构或借助解释器）。  
   - Docs：`docs/ir/statements-and-expr.md` 记录表达式/语句到自定义 IR 的 lowering 规则（含控制流构造、左值处理、方法调用约定）。

5. **整合编译驱动与外部接口**  
   - 更新 `main.cpp` 和 `scripts`：从标准输入或文件读取 `rust` 源码，依次执行 `Lexer -> Parser -> Semantic_Checker -> IRGen`，提供命令行选项（`--emit-ir` 输出自研 IR、`--run-ir` 触发解释器/模拟器执行），确保错误信息统一格式输出。  
   - 在 CI/脚本层面补充新的 IR 测试入口（例如 `scripts/run_ir_tests.sh`），方便一次性执行 `test/IRBuilder` 目录下的所有样例。  
   - Tests：新增 `scripts/docs/TODO.md` 中当前迭代的 IR 任务清单，并在 `test/IRBuilder` 引入至少 3 个端到端样例（综合控制流、内建函数、结构体操作），通过自动脚本运行。  
   - Docs：在 `docs/ir/testing.md` 写明如何运行 IR 测试、如何阅读生成的 IR 文本，并在根 `README` 的“如何使用”部分补充新的命令（等实现后同步更新）。
