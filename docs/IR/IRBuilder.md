### IR/IRBuilder 模块

本章节描述 IR 生成阶段需要的所有类、构造函数与接口。整体目标是生成**完全符合 LLVM-IR 语法**的文本，这样可以直接交给 `lli/llc/clang`，并在链接阶段与独立维护的 runtime（`runtime.cpp`/`runtime.rs` → `runtime.ll/.bc`）合并。由于语义阶段目前只有 `i32/u32/isize/usize` 四种 32 位整数，因此 `IRType` 中只保留 `i1`（布尔）、`i8`（字符/字节）与 `i32`（所有整数/指针大小）；数组长度、指针运算也基于 32 位。字符串相关类型固定为：`&str = { ptr, i32 }`（指向 UTF-8 缓冲区及长度），`String = { ptr, i32, i32 }`（指针、长度、容量），运行时按此布局实现方法。

初版 IR 选用“内存形式”而非 SSA：每个需要返回值的表达式都会分配一个 `alloca` 槽（由调用方传入或自动生成），控制流分支通过共享槽完成结果合并。例如 `if cond { 2 } else { 3 }` 会先 `alloca` 一个 `i32` 槽，两个分支分别 `store`，合并块再 `load`。这样可以完全避开 `phi` 指令，便于后续逐步实现。

#### IR 命名空间
IR 相关的所有类型、builder、上下文统一置于 `namespace ir` 下：`ir::IRModule`、`ir::VoidType`、`ir::IRBuilder` 等，避免与 AST/semantic 模块的类型冲突。其他模块引用时**禁止**使用 `using namespace ir;`，一律显式写 `ir::` 前缀保证符号清晰。

#### IRType
- **作用**：语义层 `RealType` 到 LLVM 类型的落地表示。采用抽象基类 + 子类的方式描述不同结构，避免在一个类型里堆积所有字段。`IRType` 基类仅持有虚析构函数与 `virtual string to_string() const = 0;`。
- **子类与构造参数**：
  - `VoidType`：表示 Rust 的 `()`，对应 LLVM `void`。构造函数 `VoidType()` 无额外字段。
  - `IntegerType`：字段 `int bit_width`，构造函数 `IntegerType(int bits)`；本项目仅使用 `bits = 1/8/32`，其中 `32` 同时覆盖 `i32/u32/isize/usize`。
  - `PointerType`：字段 `IRType_ptr pointee` 记录指向的元素类型，构造函数 `PointerType(IRType_ptr pointee)`；若确实需要“opaque pointer”，可显式传入 `VoidType` 作为 pointee。`to_string()` 仍输出 `ptr`，但在实现内部任何指针值都能直接追溯到元素类型，`alloca`/`gep`/`load` 不必再靠额外表格推断。
  - `ArrayType`：字段 `IRType_ptr element_type` 与 `size_t element_count`，构造函数 `ArrayType(IRType_ptr elem, size_t count)`。
  - `StructType`：记录一个命名结构体的类型标识 `string name`（例如 `%Str`）。构造函数 `StructType(string name)`；匿名结构体暂不支持，所有结构体必须在模块中以 `%Name = type { ... }` 的形式预先注册，并通过 `void set_fields(vector<IRType_ptr> fields)` 指定字段布局（主要供 `getelementptr` 推导用途，序列化时仍输出 `%Name`）。
  - `FunctionType`：字段 `IRType_ptr return_type`, `vector<IRType_ptr> param_types`，构造函数 `FunctionType(IRType_ptr ret, vector<IRType_ptr> params)`。暂不支持可变参数，所有函数签名均为固定参数列表。
- **核心接口**：
  - `virtual string to_string() const = 0;` 子类输出 LLVM 格式，如 `void`, `i32`, `ptr`, `[4 x i8]`, `{ ptr, i32 }`。
  - `bool is_integer(int bits = -1) const; bool is_pointer() const; bool is_void() const;` 等便捷判定函数可以在基类中以 `dynamic_cast`/`typeid` 实现。

- #### IRValue 层级
- `IRValue` 为抽象基类，持有 `IRType_ptr type` 与纯虚函数 `repr()`，其派生类负责具体表现，同时统一提供 `virtual string typed_repr() const` 默认实现（输出 `<type> <repr>`），方便 `store`/`call` 等指令直接引用带类型的文本。
- **`RegisterValue`**：表示 SSA 寄存器或局部地址（`alloca`/`getelementptr` 结果），字段包含 `string name`、可选 `IRInstruction_ptr def`。构造函数 `RegisterValue(string name, IRType_ptr type, IRInstruction_ptr def = nullptr)`，`repr()` 返回 `%name`。
- **`ConstantValue`**：仅针对可内联的标量常量（`i32`、`bool`）。构造函数 `ConstantValue(IRType_ptr type, int64_t literal)`，其中 `literal` 的解释由 `type` 决定（若 `type` 是 `i1` 则只取最低位表示 `true/false`）。提供 `repr()`（返回裸值，如 `42`、`true`）和 `typed_repr()`（返回 `i32 42`、`i1 true`）两种输出形式，分别用于指令参数与需要带类型的上下文。需要占内存的数组/字符串常量会转换成全局值。
- **`GlobalValue`**：继承 `IRValue`，表示模块级全局变量/常量，可直接作为指令操作数。字段包含 `string name`, `IRType_ptr type`, `string init_text`, `bool is_constant`, `string linkage`。构造函数 `GlobalValue(string name, IRType_ptr type, string init_text, bool is_const = true, string linkage = "private")`；`repr()` 输出 `@name`，`typed_repr()` 输出 `ptr @name`，`definition_string()` 返回 `@name = linkage (constant|global) <type> <init_text>`，供模块序列化时使用。

#### IRInstruction
- **字段**：
  - `enum class Opcode { Add, Sub, Mul, SDiv, UDiv, And, Or, Xor, Shl, AShr, LShr, ICmp, Call, Alloca, Load, Store, GEP, Br, CondBr, Ret } opcode;`
  - `vector<IRValue_ptr> operands;`
  - 可选的 `IRType_ptr literal_type;`（仅 `alloca`、`getelementptr`、`call` 等需要显式写出类型的指令使用，用于序列化 `alloca i32`、`getelementptr {ptr, i32}`、`call void` 这类语法时携带类型信息）
  - `IRValue_ptr result;`（有返回值时非空）
  - 额外信息：`ICmpPredicate predicate;`（仅 `ICmp` 使用，枚举值覆盖 `EQ/NE`, 有符号比较 `SLT/SLE/SGT/SGE`，无符号比较 `ULT/ULE/UGT/UGE`），`string call_callee`（仅 `Call` 使用，记录直接调用的符号名）。
  - 跳转指令补充字段：`BasicBlock_ptr target`（无条件 `br`）、`BasicBlock_ptr true_target/false_target`（条件 `br`），用于序列化跳转标签。
- **构造函数**：`IRInstruction(Opcode op, vector<IRValue_ptr> operands, IRValue_ptr result = nullptr);` 根据不同 op 决定是否需要 `predicate` 或 `call_callee`.
- **接口**：
  - `string to_string() const;` 输出 LLVM 指令文本。
  - `bool is_terminator() const;`（用于阻止在 `ret`/`br` 之后继续插入指令，防止生成非法 IR）

#### BasicBlock
- **字段**：`string label; vector<IRInstruction_ptr> instructions;`
- **构造函数**：`BasicBlock(string label);`
- **接口**：
  - `IRInstruction_ptr append(IRInstruction_ptr inst);` 将指令附加在当前块末尾；若块尾已有终止指令（`ret`/`br`），则自动把新指令插入到终止指令之前，避免生成非法 IR（便于先写控制流骨架再补 `store`/`alloca` 等指令）。
  - `IRInstruction_ptr insert_before_terminator(IRInstruction_ptr inst);` 在终止指令前插入，用于在 `ret`/`br` 前补充操作。
  - `IRInstruction_ptr get_terminator() const;` 返回块中的终止指令，便于检测是否还能追加新指令。
  - `string to_string() const;` 序列化该基本块（标签 + 指令列表）。

#### IRFunction
- **字段**：
  - `string name; IRType_ptr type;`（`type` 是函数型，内部可拆出返回/参数）
  - `vector<pair<string, IRType_ptr>> params;`
  - `vector<BasicBlock_ptr> blocks;`
  - `bool is_declaration;`
- **构造函数**：`IRFunction(string name, IRType_ptr fn_type, bool is_declaration = false);`
- **接口**：
  - `BasicBlock_ptr get_entry_block();` 获取入口块指针（若为空则尚未创建），方便将 `alloca` 插入入口。
  - `BasicBlock_ptr create_block(string label);` 在函数内创建新的基本块；无论传入的 `label` 是否带数字后缀，都会基于原始 `label` 追加 `.N`（`label.0/label.1/...`）的形式递增，保证同一函数内标签唯一。
  - `IRValue_ptr add_param(string name, IRType_ptr type);` 记录形参信息并返回对应的 `RegisterValue` 供函数体使用。
  - `string signature_string() const;` 生成 `define/declare` 语句所需的函数签名文本。
  - `string to_string() const;` 序列化整个函数（声明或定义）。所有新建基本块统一通过 `IRFunction::create_block`（通常由 `IRBuilder::create_block` 间接调用）完成，确保命名唯一性。

#### IRModule
- **字段**：
  - `string target_triple; string data_layout;`
  - `vector<pair<string, vector<string>>> type_definitions;`（每个元素保存类型名与字段序列，例如 `{"Str", {"ptr", "i32"}}`，序列化时输出 `%Str = type { ... }`。）
  - `vector<GlobalValue_ptr> globals;`（既用于指令引用，也在模块序列化时输出各自的 `definition_string()`。）
  - `vector<IRFunction_ptr> functions;`
- **构造函数**：`IRModule(string target_triple, string data_layout);`
- **接口**：
  - `GlobalValue_ptr create_global(string name, IRType_ptr type, string init_text, bool is_const = true, string linkage = "private")`：创建全局变量/常量，`init_text` 由调用方提前串好（如 `c"hi\00"` 或 `[2 x i32] [i32 1, i32 2]`），返回可在指令中使用的 `GlobalValue` 并登记定义。
  - `void add_type_definition(string name, vector<string> fields)`：记录命名结构体/别名的字段布局，序列化时输出 `%name = type { ... }`。
  - `void add_module_comment(string text)`：追加一行模块级注释（例如 `; EXPECT: ...`），序列化时位于 `target triple` 之前，可用于 fixture 描述或调试信息。
  - `IRFunction_ptr declare_function(name, fn_type, is_builtin)`：仅声明函数原型（无函数体），常用于内建/外部函数。
  - `IRFunction_ptr define_function(name, fn_type)`：创建函数定义并返回 `IRFunction_ptr` 以便填充基本块。
  - `void ensure_runtime_builtins()`：注入 `@print`, `@println`, `@exit`, `@String_from`, `@Array_len` 等内建声明，供 IRBuilder 调用。该函数假设 `%Str = { ptr, i32 }` 与 `%String = { ptr, i32, i32 }` 已由 TypeLowering（或等效初始化逻辑）提前通过 `add_type_definition` 注册，因此不会重复写入这些结构体类型。
  - `string to_string() const`：序列化整个模块（target triple、类型定义、globals、functions）。

#### IRBuilder
- **字段**：
  - `IRModule &module; IRFunction_ptr current_function; BasicBlock_ptr insertion_block;`
  - `size_t next_reg_index;`
- **构造函数**：`IRBuilder(IRModule &module);`
- **关键接口**：
  - `void set_insertion_point(block)`：指定后续指令插入的基本块。
  - `BasicBlock_ptr create_block(label)`：在当前函数中新建基本块并返回。`IRFunction::create_block` 会为同名标签在函数内部维护一个计数器，自动拼接 `.N` 后缀（如 `then` 申请两次得到 `then.0/then.1`），因此调用者无需关心命名冲突。
  - `IRValue_ptr create_temp(IRType_ptr type, string name_hint = "")`：分配新的 SSA 寄存器；若 `name_hint` 非空且未冲突则使用 `%name_hint`，否则按 `%0/%1/...` 递增。
- `IRValue_ptr create_temp_alloca(IRType_ptr type, string name_hint = "")`：在入口块生成 `alloca`，为表达式结果或局部变量申请栈空间，返回指向该栈槽的 `RegisterValue`。未显式指定名称时，保持 LLVM 的匿名寄存器命名规则，避免在大规模生成时产生重复名称。
  - **算术接口**：
    - `IRValue_ptr create_add(IRValue_ptr lhs, IRValue_ptr rhs, string name_hint = "")`
    - `IRValue_ptr create_sub(IRValue_ptr lhs, IRValue_ptr rhs, string name_hint = "")`
    - `IRValue_ptr create_mul(IRValue_ptr lhs, IRValue_ptr rhs, string name_hint = "")`
    - `IRValue_ptr create_udiv(IRValue_ptr lhs, IRValue_ptr rhs, string name_hint = "")`
    - `IRValue_ptr create_sdiv(IRValue_ptr lhs, IRValue_ptr rhs, string name_hint = "")`
    - `IRValue_ptr create_urem(IRValue_ptr lhs, IRValue_ptr rhs, string name_hint = "")`
    - `IRValue_ptr create_srem(IRValue_ptr lhs, IRValue_ptr rhs, string name_hint = "")`
    - `IRValue_ptr create_and/ or/ xor/ shl/ lshr/ ashr(IRValue_ptr lhs, IRValue_ptr rhs, string name_hint = "")`
    返回一个新的 `RegisterValue`，类型通常与 `lhs` 相同。
  - **比较/逻辑**：
    - `IRValue_ptr create_icmp_eq/ne/slt/... (IRValue_ptr lhs, IRValue_ptr rhs, string name_hint = "")`
    - `IRValue_ptr create_not(IRValue_ptr value, string name_hint = "")`
    输出 `i1` 类型结果，并可指定寄存器命名 hint。
  - **内存操作**：
  - `IRValue_ptr create_alloca(IRType_ptr type, string name_hint = "")`
    - `IRValue_ptr create_load(IRValue_ptr ptr, string name_hint = "")`
    - `void create_store(IRValue_ptr value, IRValue_ptr ptr)`
    - `IRValue_ptr create_gep(IRValue_ptr base_ptr, IRType_ptr element_type, vector<IRValue_ptr> indices, string name_hint = "")`
    用于声明局部变量、读取/写入地址、计算偏移；`create_gep` 仍需显式提供元素类型（便于在 `ptr` 指向结构体/数组时精确定义索引），但 PointerType 自身已经记录 pointee，`load/store` 等接口可以直接读取指向类型做校验。
  - **控制流**：
    - `void create_br(BasicBlock_ptr target)`
    - `void create_cond_br(IRValue_ptr cond, BasicBlock_ptr true_block, BasicBlock_ptr false_block)`
    - `void create_ret(IRValue_ptr value = nullptr)`（`nullptr` 表示 `ret void`）
  - **调用**：`IRValue_ptr create_call(string callee, vector<IRValue_ptr> args, IRType_ptr ret_type, string name_hint = "")`——生成函数调用指令，若 `ret_type` 不是 `void` 则返回结果寄存器。
  - **字符串字面量**：`create_string_literal(text)`——生成 `[len x i8]` 全局常量并返回指向它的 `GlobalValue`/`RegisterValue`。
  - **常量便捷接口**：`IRValue_ptr create_i32_constant(int64_t value)`——包装 `ConstantValue` 的常用形式，直接返回一个 `i32` 常量寄存器，供循环索引、GEP 偏移等频繁场景复用，避免在调用点反复手写 `std::make_shared<ConstantValue>(i32_type, value)`。

#### 序列化与调试
- `IRSerializer`：负责 `IRModule::to_string()`，确保缩进、换行与 LLVM 语法一致；类型定义通过遍历 `type_definitions` 中的 `pair<string, vector<string>>` 拼出 `%TypeName = type { ... }` 格式；所有 IR 文本统一带 `target triple = ...` 与 `target datalayout = ...` 头部，fixture/runner 也据此比对。实现上会借助若干内部工具：
  - `deduce_gep_pointee()`：根据数组/结构索引序列推导 `getelementptr` 结果指向的元素类型；`PointerType` 本身存着 `pointee`，因此 `create_load`/`create_store` 等只需直接查看指针类型即可，还可以在 `GlobalValue`/`alloca` 构造时立刻携带 pointee。
  - `string_literal_counters()` / `encode_string_literal()`：为 `create_string_literal()` 生成唯一的 `.str.N` 名称，同时把原始文本编码为 LLVM `c"..."` 语法（包含转义和结尾的 `\00`）。
  - `predicate_to_string()`、`opcode_to_string()`：把内部枚举（`ICmpPredicate`、`Opcode`）转换为 LLVM 指令助记符，便于 `IRInstruction::to_string()`。
- `void IRModule::dump() const;`、`void IRFunction::dump() const;` 输出到 `stderr`，用于调试。
- 可以在序列化时为指令追加注释（如 `; node_id=123`）帮助排查问题。

#### 测试计划与示例
- `test/IRBuilder/ir_builder_contract_test.cpp`：所有 `IRBuilder`/`IRModule`/`IRFunction` 接口先通过 `static_assert` 固化签名及继承关系，保证头文件与本文档描述一致，即便实现尚未完成也能立即发现接口偏差。
- `test/IRBuilder/ir_builder_fixture_catalog.cpp`：遍历 `test/IRBuilder/fixtures/*.ir.expected` 的期望 IR 文本，只读取文件内容并检查关键 token（block 标签、指令名称等），确保 fixture 自身完整可用。
- `test/IRBuilder/ir_builder_fixture_runner.cpp`：当 `ENABLE_IR_BUILDER_RUNTIME_TESTS` 打开时，利用 `IRBuilder` API 手动构造与各个 fixture 对应的 IR Module，调用 `module.to_string()` 并与期望文本逐字比对，实现 IRBuilder 后即可作为端到端回归测试。

- 简短示例们：
  1. **返回常量 0**
     ```cpp
     auto i32 = std::make_shared<ir::IntegerType>(32);
     auto fn = module.define_function("main", std::make_shared<ir::FunctionType>(i32, std::vector{}));
     auto entry = fn->create_block("entry");
     builder.set_insertion_point(entry);
     builder.create_ret(std::make_shared<ir::ConstantValue>(i32, 0));
     // 生成：
     // define i32 @main() { entry: ret i32 0 }
     ```
  2. **调用 print(&"hi")**
     ```cpp
     auto msg_ptr = builder.create_string_literal("hi");
     auto str_tmp = builder.create_temp_alloca(str_struct_type); // { ptr, i32 }
     // 填充 { ptr, i32 } 并调用 create_call("print", {...}, void_type);
     ```
     对应 IR 片段：
     ```llvm
     @.str.0 = private constant [3 x i8] c"hi\00"
     call void @print({ ptr, i32 } %packed_str)
     ```
  3. **数组常量引用**
     ```cpp
     auto arr = module.create_global("array.0",
         std::make_shared<ir::ArrayType>(i32, 2),
         make_constant_init({1, 2}));
     builder.create_store(std::make_shared<ir::ConstantValue>(i32, 3),
                          builder.create_gep(arr, {...}));
     ```
这些示例展示了 IRBuilder 处理返回值、字符串 &str、数组常量等典型场景，可作为测试基准。
