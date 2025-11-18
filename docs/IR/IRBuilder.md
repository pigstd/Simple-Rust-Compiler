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
  - `PointerType`：无需记录具体被指向类型，构造函数 `PointerType()` 始终表示 LLVM 的 `ptr`（opaque pointer 模式），若需额外调试信息可在外部自行关联。
  - `ArrayType`：字段 `IRType_ptr element_type` 与 `size_t element_count`，构造函数 `ArrayType(IRType_ptr elem, size_t count)`。
  - `StructType`：字段 `vector<IRType_ptr> fields` 与可选的 `string name`（用于 `%Struct = type { ... }`）。构造函数 `StructType(vector<IRType_ptr> fields, string name = "")` 支持带名/匿名结构体；`&str` 与 `String` 的布局也通过此类描述。
  - `FunctionType`：字段 `IRType_ptr return_type`, `vector<IRType_ptr> param_types`，构造函数 `FunctionType(IRType_ptr ret, vector<IRType_ptr> params)`。暂不支持可变参数，所有函数签名均为固定参数列表。
- **核心接口**：
  - `virtual string to_string() const = 0;` 子类输出 LLVM 格式，如 `void`, `i32`, `ptr`, `[4 x i8]`, `{ ptr, i32 }`。
  - `bool is_integer(int bits = -1) const; bool is_pointer() const;` 等便捷判定函数可以在基类中以 `dynamic_cast`/`typeid` 实现。

- #### IRValue 层级
- `IRValue` 为抽象基类，持有 `IRType_ptr type` 与纯虚函数 `repr()`，其派生类负责具体表现。
- **`RegisterValue`**：表示 SSA 寄存器或局部地址（`alloca`/`getelementptr` 结果），字段包含 `string name`、可选 `IRInstruction_ptr def`。构造函数 `RegisterValue(string name, IRType_ptr type, IRInstruction_ptr def = nullptr)`，`repr()` 返回 `%name`。
- **`ConstantValue`**：仅针对可内联的标量常量（`i32`、`bool`）。构造函数 `ConstantValue(IRType_ptr type, ConstantLiteral literal)`，其中 `ConstantLiteral = std::variant<int32_t, bool>`；提供 `repr()`（返回裸值，如 `42`、`true`）和 `typed_repr()`（返回 `i32 42`、`i1 true`）两种输出形式，分别用于指令参数与全局初始化。数组/字符串等需要占内存的常量会直接降为 `IRGlobal`，不再存放在 `ConstantValue` 中。
- **`GlobalValue`**：继承 `IRValue`，表示模块级全局变量/常量，可直接作为指令操作数（`repr()` 返回 `@name`，`typed_repr()` 返回 `ptr @name`）。通过 `IRModule::create_global(string name, IRType_ptr type, IRConstant_ptr init, bool is_const = true, string linkage = "private")` 创建并注册到模块，序列化时由模块统一输出 `@name = ...` 定义。

#### IRInstruction
- **字段**：
  - `enum class Opcode { Add, Sub, Mul, SDiv, UDiv, And, Or, Xor, Shl, AShr, LShr, ICmp, Call, Alloca, Load, Store, GEP, Br, CondBr, Ret } opcode;`
  - `vector<IRValue_ptr> operands;`
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
  - `IRInstruction_ptr append(IRInstruction_ptr inst);` 将指令附加在当前块末尾。
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
  - `IRValue_ptr add_param(string name, IRType_ptr type);` 记录形参信息并返回对应的 `RegisterValue` 供函数体使用。
  - `string signature_string() const;` 生成 `define/declare` 语句所需的函数签名文本。
  - `string to_string() const;` 序列化整个函数（声明或定义）。所有新建基本块统一通过 `IRBuilder::create_block` 完成，确保命名唯一性。

#### IRModule
- **字段**：
  - `string target_triple; string data_layout;`
  - `vector<string> type_definitions;`
  - `vector<IRGlobal_ptr> globals;`
  - `vector<IRFunction_ptr> functions;`
- **构造函数**：`IRModule(string target_triple, string data_layout);`
- **接口**：
  - `GlobalValue_ptr create_global(...)`：创建全局变量/常量，返回可在指令中使用的 `GlobalValue` 并登记定义。
  - `IRFunction_ptr declare_function(name, fn_type, is_builtin)`：仅声明函数原型（无函数体），常用于内建/外部函数。
  - `IRFunction_ptr define_function(name, fn_type)`：创建函数定义并返回 `IRFunction_ptr` 以便填充基本块。
  - `void ensure_runtime_builtins()`：注入 `@print`, `@println`, `@exit`, `@String_from`, `@Array_len` 等内建声明，供 IRBuilder 调用。
  - `string to_string() const`：序列化整个模块（target triple、类型定义、globals、functions）。

#### IRBuilder
- **字段**：
  - `IRModule &module; IRFunction_ptr current_function; BasicBlock_ptr insertion_block;`
  - `size_t next_reg_index;`
- **构造函数**：`IRBuilder(IRModule &module);`
- **关键接口**：
  - `void set_insertion_point(block)`：指定后续指令插入的基本块。
  - `BasicBlock_ptr create_block(label)`：在当前函数中新建基本块并返回。内部维护 `unordered_map<string, size_t> block_name_counter`，每个标签会自动拼接 `.N` 后缀（如 `then.0`、`then.1`），避免命名冲突。
  - `IRValue_ptr create_temp(IRType_ptr type)`：分配一个新的 SSA 寄存器（`%N`），用于接收指令结果。
  - `IRValue_ptr create_temp_alloca(type, hint)`：在入口块生成 `alloca`，为表达式结果或局部变量申请栈空间。
  - **算术接口**：`create_add`/`sub`/`mul`/`udiv`/`sdiv`/`urem`/`srem`/`and`/`or`/`xor`/`shl`/`lshr`/`ashr`——生成对应的整数运算指令。
  - **比较/逻辑**：`create_icmp_*`（`eq/ne/slt/...`）、`create_not`——生成布尔结果。
  - **内存操作**：`create_alloca`/`create_load`/`create_store`/`create_gep`——用于声明局部变量、读取/写入地址、计算偏移；其中 `create_gep` 需要调用方显式传入元素类型，PointerType 本身不记录 pointee。
  - **控制流**：`create_br`（无条件跳转）、`create_cond_br`（条件跳转）、`create_ret`（返回值或 `ret void`）。
  - **调用**：`create_call(callee, args, ret_type)`——生成函数调用指令。
  - **字符串字面量**：`create_string_literal(text)`——生成 `[len x i8]` 全局常量并返回指向它的 `GlobalValue`/`RegisterValue`。

#### 序列化与调试
- `IRSerializer`：负责 `IRModule::to_string()`，确保缩进、换行与 LLVM 语法一致；类型定义采用 `%TypeName = type {...}` 格式。
- `void IRModule::dump() const;`、`void IRFunction::dump() const;` 输出到 `stderr`，用于调试。
- 可以在序列化时为指令追加注释（如 `; node_id=123`）帮助排查问题。

#### 测试计划与示例
- `test/ir/test_ir_builder.cpp`：手动创建 `IRModule` + `IRBuilder`，插入若干指令，检查 `to_string()` 与期望 IR 逐行匹配。
- `test/ir/manual_programs.cpp`：编写几个小程序（算术、循环、字符串），完全用 IRBuilder API 构造后交给 `lli program.ll runtime.ll` 验证。
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
