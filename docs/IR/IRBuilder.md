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
  - `PointerType`：字段 `IRType_ptr pointee`，构造函数 `PointerType(IRType_ptr pointee)`。生成 `.ll` 时输出 `ptr`（LLVM 从 15 起默认启用 opaque pointer），仅在注释或调试信息中保留 `pointee` 以方便理解。
  - `ArrayType`：字段 `IRType_ptr element_type` 与 `size_t element_count`，构造函数 `ArrayType(IRType_ptr elem, size_t count)`。
  - `StructType`：字段 `vector<IRType_ptr> fields` 与可选的 `string name`（用于 `%Struct = type { ... }`）。构造函数 `StructType(vector<IRType_ptr> fields, string name = "")` 支持带名/匿名结构体；`&str` 与 `String` 的布局也通过此类描述。
  - `FunctionType`：字段 `IRType_ptr return_type`, `vector<IRType_ptr> param_types`，构造函数 `FunctionType(IRType_ptr ret, vector<IRType_ptr> params)`。暂不支持可变参数，所有函数签名均为固定参数列表。
- **核心接口**：
  - `virtual string to_string() const = 0;` 子类输出 LLVM 格式，如 `void`, `i32`, `i8*`, `[4 x i8]`, `{ i8*, i32 }`.
  - `bool is_integer(int bits = -1) const; bool is_pointer() const;` 等便捷判定函数可以在基类中以 `dynamic_cast`/`typeid` 实现。

- #### IRValue 层级
- `IRValue` 为抽象基类，持有 `IRType_ptr type` 与纯虚函数 `repr()`，其派生类负责具体表现。
- **`RegisterValue`**：表示 SSA 寄存器或局部地址（`alloca`/`getelementptr` 结果），字段包含 `string name`、可选 `IRInstruction_ptr def`。构造函数 `RegisterValue(string name, IRType_ptr type, IRInstruction_ptr def = nullptr)`，`repr()` 返回 `%name`。
- **`ConstantValue`**：仅针对可内联的标量常量（`i32`、`bool`）。构造函数 `ConstantValue(IRType_ptr type, ConstantLiteral literal)`，其中 `ConstantLiteral = std::variant<int32_t, bool>`；`repr()` 输出 `i32 42`、`i1 true` 等文本。数组/字符串等需要占内存的常量会直接降为 `IRGlobal`，不再存放在 `ConstantValue` 中。

#### IRInstruction
- **字段**：
  - `enum class Opcode { Add, Sub, Mul, SDiv, UDiv, And, Or, Xor, Shl, AShr, LShr, ICmp, Call, Alloca, Load, Store, GEP, Br, CondBr, Ret } opcode;`
  - `vector<IRValue_ptr> operands;`
  - `IRValue_ptr result;`（有返回值时非空）
  - 额外信息：`ICmpPredicate predicate;`、`string call_callee;`
- **构造函数**：`IRInstruction(Opcode op, vector<IRValue_ptr> operands, IRValue_ptr result = nullptr);` 根据不同 op 决定是否需要 `predicate` 或 `call_callee`.
- **接口**：
  - `string to_string() const;` 输出 LLVM 指令文本。
  - `bool is_terminator() const;`

#### BasicBlock
- **字段**：`string label; vector<IRInstruction_ptr> instructions;`
- **接口**：
  - `IRInstruction_ptr append(IRInstruction_ptr inst);`
  - `IRInstruction_ptr insert_before_terminator(IRInstruction_ptr inst);`
  - `IRInstruction_ptr get_terminator() const;`
  - `string to_string() const;`

#### IRFunction
- **字段**：
  - `string name; IRType_ptr type;`（`type` 是函数型，内部可拆出返回/参数）
  - `vector<pair<string, IRType_ptr>> params;`
  - `vector<BasicBlock_ptr> blocks;`
  - `bool is_declaration;`
- **接口**：
  - `BasicBlock_ptr create_block(string label);` 自动 push。
  - `BasicBlock_ptr get_entry_block();`
  - `IRValue_ptr add_param(string name, IRType_ptr type);`
  - `string signature_string() const;`
  - `string to_string() const;`

#### IRGlobal
- **字段**：
  - `string name; IRType_ptr type; IRConstant_ptr init;`
  - `bool is_constant; string linkage;`（如 `private`、`dso_local`）
- **接口**：`string to_string() const;` 输出 `@.str.0 = private constant [6 x i8] c"hello\00"`.

#### IRModule
- **字段**：
  - `string target_triple; string data_layout;`
  - `vector<string> type_definitions;`
  - `vector<IRGlobal_ptr> globals;`
  - `vector<IRFunction_ptr> functions;`
- **接口**：
  - `IRFunction_ptr declare_function(string name, IRType_ptr fn_type, bool is_builtin);`
  - `IRFunction_ptr define_function(string name, IRType_ptr fn_type);`
  - `IRGlobal_ptr add_global(string name, IRType_ptr type, IRConstant_ptr init, bool is_const, string linkage);`
  - `void ensure_runtime_builtins();` 在模块初始化时注入 `@print`, `@println`, `@exit`, `@String_from`, `@Array_len` 的 `declare` 语句，方便与 runtime.ll 链接。
  - `string to_string() const;`

#### IRBuilder
- **字段**：
  - `IRModule &module; IRFunction_ptr current_function; BasicBlock_ptr insertion_block;`
  - `size_t next_reg_index;`
- **构造函数**：`IRBuilder(IRModule &module);`
- **关键接口**：
  - `void set_insertion_point(BasicBlock_ptr block);`
  - `BasicBlock_ptr create_block(string label);`
  - `IRValue_ptr create_temp(IRType_ptr type);`（返回 `%N` 形式的寄存器）
  - `IRValue_ptr create_temp_alloca(IRType_ptr type, string hint_name = "tmp");`
  - **算术**：`create_add`, `create_sub`, `create_mul`, `create_udiv`, `create_sdiv`, `create_urem`, `create_srem`, `create_and`, `create_or`, `create_xor`, `create_shl`, `create_lshr`, `create_ashr`.
  - **比较/逻辑**：`create_icmp_eq`, `create_icmp_ne`, `create_icmp_slt`, `create_icmp_sle`, `create_icmp_sgt`, `create_icmp_sge`, `create_icmp_ult`, `create_icmp_ule`, `create_icmp_ugt`, `create_icmp_uge`; `create_not`.
  - **内存**：`create_alloca(IRType_ptr type, string hint_name)`, `create_load(IRValue_ptr ptr_val)`, `create_store(IRValue_ptr value, IRValue_ptr ptr_val)`, `create_gep(IRValue_ptr base, vector<IRValue_ptr> indices)`.
  - **控制流**：`create_br(BasicBlock_ptr target)`, `create_cond_br(IRValue_ptr cond, BasicBlock_ptr true_block, BasicBlock_ptr false_block)`, `create_ret(IRValue_ptr value = nullptr)`.
  - **调用**：`IRValue_ptr create_call(string callee, vector<IRValue_ptr> args, IRType_ptr ret_type);`
  - **字符串字面量**：`IRValue_ptr create_string_literal(const string &text);` 在模块中新建 `@.str.N` 常量并返回 `i8*`。

#### 序列化与调试
- `IRSerializer`：负责 `IRModule::to_string()`，确保缩进、换行与 LLVM 语法一致；类型定义采用 `%TypeName = type {...}` 格式。
- `void IRModule::dump() const;`、`void IRFunction::dump() const;` 输出到 `stderr`，用于调试。
- 可以在序列化时为指令追加注释（如 `; node_id=123`）帮助排查问题。

#### 测试计划与示例
- `test/ir/test_ir_builder.cpp`：手动创建 `IRModule` + `IRBuilder`，插入若干指令，检查 `to_string()` 与期望 IR 逐行匹配。
- `test/ir/manual_programs.cpp`：编写几个小程序（算术、循环、字符串），完全用 IRBuilder API 构造后交给 `lli program.ll runtime.ll` 验证。
- 示例片段：
  ```llvm
  @.str.0 = private constant [6 x i8] c"hello\00"

  ; print 接受按值传递的 &str = { ptr, i32 }
  declare void @print({ ptr, i32 })

  define i32 @main() {
  entry:
    %tmp = alloca { ptr, i32 }
    %data_ptr = getelementptr [6 x i8], ptr @.str.0, i32 0, i32 0
    %field0 = getelementptr { ptr, i32 }, ptr %tmp, i32 0, i32 0
    store ptr %data_ptr, ptr %field0
    %field1 = getelementptr { ptr, i32 }, ptr %tmp, i32 0, i32 1
    store i32 5, ptr %field1
    %msg = load { ptr, i32 }, ptr %tmp
    call void @print({ ptr, i32 } %msg)
    ret i32 0
  }
  ```
  展示字符串常量、内建函数声明和基本块结构，为实现与测试提供参考。
