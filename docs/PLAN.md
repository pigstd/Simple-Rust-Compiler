我现在已经完成了 semantic check，我接下来需要利用 semantic check 的信息生成 llvm-IR 的中间表示。

生成 IR 主要有以下几个步骤：

1. **搭建 IR 框架与手写 IR Builder**  
   - 在 `include/src` 下新增 `ir` 模块，定义 `IRModule`、`IRFunction`、`BasicBlock`、`IRValue`、`IRType` 等核心结构，并实现一个 `IRBuilder`/`IRGenContext`，用于维护当前基本块、常量池、字符串表等。  
   - 将 `Semantic_Checker` 产出的 `node_scope_map`、`node_type_and_place_kind_map`、`const_value_map` 等结构注入 `IRGenContext`，补好 IR 生成阶段需要的查询接口；设计自定义的文本 IR 序列化格式（便于调试与测试）。  
   - Tests：新增 `test/ir/test_ir_context.cpp`，构造极简 AST，驱动 `Semantic_Checker` 与 `IRGenContext`，验证可以创建空 Module、自动注入内建函数声明（`print/println/exit`）并输出格式正确的 IR 文本。  
   - Docs：在 `docs/ir/overview.md` 记录 IR 模块的边界、关键数据结构、IR 文法以及初始化顺序。

2. **实现 `RealType -> IRType` 映射与常量折叠**  
   - 编写 `TypeLowering` 工具（独立成类或 `IRGenContext` 的方法），涵盖所有语义阶段支持的类型：整数/布尔/字符、`()`、`!`、字符串（`&str` 与 `String` 的内存布局）、数组（利用 `const_expr_to_size_map`）、结构体/枚举（使用 `Scope` 中的字段顺序）等，统一映射到 `IRType`。  
   - 处理 `ConstDecl` 与 `const_expr_queue` 的值：把 `const_value_map` 中的整形、布尔、字符串常量转成 `IRConstant`，为数组/结构体常量生成聚合常量节点，并能在序列化时稳定输出。  
   - Tests：`test/ir/test_type_lowering.cpp` 逐个断言 `RealTypeKind` 到 `IRType` 的 bit width/字段数量/布局正确；另写一个包含数组与结构体常量的高层测试，验证生成的 IR 文本片段与预期匹配。  
   - Docs：新增 `docs/ir/type-lowering.md`，详细说明各 `RealTypeKind` 如何落到自研 IR 类型，包括字符串/数组/引用的布局选择与原因。

3. **全局项与内建运行时支持生成**  
   - 遍历所有 `Item`，首先为结构体、枚举生成 `IRType`，再声明/定义函数、常量、全局字符串；对需要运行时支持的内建（`print/println/String::from/Array::len` 等）定义 IR 层的抽象函数签名或运行时桩，约定参数/返回值语义。  
   - 生成 `const` 与 `static` 的 IR 全局变量，并确保引用它们的表达式能直接拿到 `IRConstant`，避免重复插入；输出时提供统一的 `.data/.text` 段或自定义段表示。  
   - Tests：新增一组端到端样例程序（例如 `test/ir/programs/global_struct.rs`、`.../string_from.rs`），通过新的测试驱动编译 -> 输出 IR，再由轻量解释器或脚本检查全局常量初始化和值传递是否符合期望。  
   - Docs：`docs/ir/globals.md` 描述全局生成流程、内建运行时 ABI 以及字符串/数组常量的组织方式。
   - Runtime 注意事项：内建函数的真实实现单独放在 `runtime/*.cpp`（或 `.rs`）里，提前用 `clang -S -emit-llvm` / `rustc --emit=llvm-ir` 生成 `runtime.ll` 或 `runtime.bc`，确保符号名与 IR 中声明一致。编译/运行时，将编译器生成的 `program.ll` 与 runtime 的 `runtime.ll` 一并交给 LLVM 工具（`lli program.ll runtime.ll` 或 `llc`+`clang program.o runtime.o`）完成链接，使内建函数在最终可执行中具备实现。

4. **函数体、语句与表达式的 IR 生成**  
   - 在 `IRGenVisitor` 中实现 `visit(FnItem)`，为每个函数建立入口基本块、根据 `scope_local_variable_map` 分配栈空间（可用 `alloca` 风格的虚拟指令）、填充参数；实现 `visit`/`lower_*` 覆盖 `let`、赋值、控制流（`if/while/loop/break/continue/return`）、算术运算、比较、逻辑、数组/结构体访问、方法与关联函数调用。  
   - 根据 `node_type_and_place_kind_map` 判断需要生成 `load`、`store`、`ptr_add`、`memcpy` 等 IR 指令；借助 `node_outcome_state_map` 正确结束基本块，支持 `Never` 返回值分支；若需要，可实现 `cfg` 验证和死块消除。  
   - Tests：`test/ir/run_basic_programs.cpp` 读取若干 `.rx` 样例（算术、循环、数组、结构体、break/continue、`String::from`），生成 IR 写入临时文件并交给 IR 解释器/伪执行脚本验证输出；同时保留覆盖 `break value`、`loop` 返回值等语义的单元测试（可直接断言 IR 文本结构或借助解释器）。  
   - Docs：`docs/ir/statements-and-expr.md` 记录表达式/语句到自定义 IR 的 lowering 规则（含控制流构造、左值处理、方法调用约定）。

5. **整合编译驱动与外部接口**  
   - 更新 `main.cpp` 和 `scripts`：从标准输入或文件读取 `rust` 源码，依次执行 `Lexer -> Parser -> Semantic_Checker -> IRGen`，提供命令行选项（`--emit-ir` 输出自研 IR、`--run-ir` 触发解释器/模拟器执行），确保错误信息统一格式输出。  
   - 在 CI/脚本层面补充新的 IR 测试入口（例如 `scripts/run_ir_tests.sh`），方便一次性执行 `test/ir` 目录下的所有样例。  
   - Tests：新增 `scripts/docs/TODO.md` 中当前迭代的 IR 任务清单，并在 `test/ir` 引入至少 3 个端到端样例（综合控制流、内建函数、结构体操作），通过自动脚本运行。  
   - Docs：在 `docs/ir/testing.md` 写明如何运行 IR 测试、如何阅读生成的 IR 文本，并在根 `README` 的“如何使用”部分补充新的命令（等实现后同步更新）。
