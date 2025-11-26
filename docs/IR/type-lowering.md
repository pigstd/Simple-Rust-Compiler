### IR/TypeLowering 模块

本章节描述 IR 生成阶段的第二步：实现 `RealType -> ir::IRType` 的映射，并把语义阶段求出的常量值桥接到 `ir::ConstantValue`。所有接口都围绕 `TypeLowering` 展开，它位于 `namespace ir` 所在模块的编译器侧（`src/ir/type_lowering.*`，与 `IRBuilder` 同级），只依赖语义检查产出的上下文，不直接访问 AST。

#### 设计目标
- 为所有 `RealTypeKind`（含引用修饰）产出 `IRBuilder` 能消费的 `ir::IRType`，布局需完全符合 `docs/IR/IRBuilder.md`：`i1/i8/i32`、`ptr`、`[N x T]`、命名结构体 `{ ... }` 等。
- 针对结构体、枚举、`String` 这类命名聚合类型，负责在模块级注册唯一的 `%StructName = type { ... }`，便于 `IRBuilder` 在后续多次引用。推荐的流程是：遍历 AST 遇到结构体声明时立即调用 `TypeLowering::declare_struct`（并写入 `struct_cache_`），之后所有 `lower(StructRealType)` 都只复用缓存，若缓存缺失视为初始化顺序错误。
- 根据常量求值阶段的 `ConstValue` 创建 `ir::ConstantValue`（目前唯一的 IR 常量形态），仅处理可内联的标量；复合常量保持 `ConstValue` 形态等待第三步生成全局区。
- 不做任何新的常量折叠：`TypeLowering` 只读取语义阶段产出的结果，**不会**尝试在表达式级别插入/替换常量。

#### 依赖数据结构
- IR 生成入口需要暴露的通用资源：`ir::IRModule &module`（注册结构体/函数类型）、`IRBuilder &builder`（需要时可查询内建类型单例，如 `builder.i32_type()`）、`StringTable`（生成字符串常量时重用）。`ConstValue` 由调用者提供，TypeLowering 不再持有 `const_value_map`。

#### TypeLowering 类
```cpp
class TypeLowering {
public:
    TypeLowering(ir::IRModule &module);

    ir::IRType_ptr lower(RealType_ptr type);
    shared_ptr<ir::FunctionType> lower_function(FnDecl_ptr decl);
    shared_ptr<ir::ConstantValue> lower_const(ConstValue_ptr value, RealType_ptr expected_type);
    shared_ptr<ir::StructType> declare_struct(StructDecl_ptr decl); // 在遇到结构体声明时调用，注册 `%Name = type { ... }`
    void declare_builtin_string_types(); // 提前注册 `%Str` / `%String` 等固定布局

private:
    ir::IRModule &module_;
    unordered_map<string, shared_ptr<ir::StructType>> struct_cache_; // `%StructName` → 已声明的 StructType，declare_struct/lower 共享缓存
};
```
- 构造函数：注入 `IRModule`，并缓存 `Void/i1/i8/i32` 等基础 `IRType` 以复用，后续创建 `PointerType` 时总能提供准确的 pointee。
- `lower(RealType_ptr type)`：外部统一入口：若 `type->is_ref != ReferenceType::NO_REF`，先浅拷贝一个 `ReferenceType::NO_REF` 的同类型，再对该副本调用 `lower` 并用结果包装成 `PointerType`；其余情况按 `RealTypeKind` 区分（整数/布尔/数组/结构体/字符串等），其中结构体必须直接命中 `struct_cache_`（未命中即抛 `std::runtime_error("struct not declared")`，提醒调用者先调用 `declare_struct` 注册）。若传入 `nullptr`，立即抛出 `std::runtime_error("invalid RealType")`。
- `lower_function(FnDecl_ptr decl)`：把函数实参/返回值的 `RealType` 分别映射为 `ir::FunctionType`；若返回类型是 `()` 则输出 `void`。参数顺序与 `FnDecl::parameters` 保持一致，`self`（如果存在）也走列表第一位。
- `lower_const(ConstValue_ptr value, RealType_ptr expected_type)`：根据 `ConstValueKind` 推导出 `ir::ConstantValue` 的 bitwidth，并做必要的符号扩展/截断；若传入的是数组/结构体/字符串等复合常量则返回 `nullptr`，由后续阶段处理。函数还会在 `expected_type` 为引用时直接返回 `nullptr`，因为引用常量在 IR 层只能用全局指针表示。
- `declare_struct(StructDecl_ptr decl)`：供“遍历 AST 遇到结构体声明”时调用。根据 `decl->fields` 的顺序逐个调用 `lower(field_type)` 生成字段 `IRType`，然后通过 `module_.add_type_definition(name, field_strings)` 注册 `%Name = type { ... }`，最后把构造好的 `StructType` 写入 `struct_cache_`（供 `lower` 命中），如果外部重复注册则直接复用缓存。
- `declare_builtin_string_types()`：由编译驱动在 TypeLowering 初始化后立即调用，将 `%Str = { ptr(i8*), i32 }` 与 `%String = { ptr(i8*), i32, i32 }` 注册到 `IRModule`，并把返回的 `StructType` 直接写入 `struct_cache_`。之后 `lower(StrRealType)`/`lower(StringRealType)` 只查缓存（命中即返回，未命中直接抛错）。这样 runtime 内建声明 (`IRModule::ensure_runtime_builtins`) 就可以专注于函数签名，不再重复生成这些结构。
- 结构体缓存策略：假设编译驱动会在结构体声明阶段调用 `declare_struct` 并把所有 `%StructName` 写入 `struct_cache_`。若 `lower` 遇到某个结构体时缓存未命中，视为初始化顺序错误，直接抛出 `std::runtime_error("struct not declared")`。

#### 内部 helper
- `receiver_to_ref(fn_reciever_type)`：将语义阶段记录的 `self/&self/&mut self` 形式映射到 `ReferenceType::NO_REF/REF/REF_MUT`，供 `lower_function` 在拼装参数列表时决定是否生成指针。
- `strip_reference(RealType_ptr)`：创建一个 `is_ref = NO_REF` 的浅拷贝并返回，为 `lower` 提供“先得到裸类型、再统一包成 `PointerType`”的流程，避免到处写 `RealTypeKind` 专门逻辑。
- `read_signed` / `read_unsigned`：分别抽象 `I32/ISIZE` 与 `U32/USIZE` 的 `ConstValue` 读取逻辑，确保 `lower_const` 对于符号扩展/溢出检查保持一致；遇到不支持的 `ConstValueKind` 会抛 `std::runtime_error`。

#### RealType -> IRType 映射细则
- **整型族 (`I32`/`U32`/`ISIZE`/`USIZE`/`ANYINT`)**：全部降为 `std::shared_ptr<ir::IntegerType>(32)`；参考 `IRBuilder` 文档，指针大小与 `usize` 均为 32 bit。
- **布尔 (`BOOL`)**：`i1`。
- **字符 (`CHAR`)**：`i8`，UTF-8 单字节表示。
- **引用修饰 (`REF`/`REF_MUT`)**：拷贝一个 `ReferenceType::NO_REF` 的底层 `RealType`，对其调用 `lower` 并用结果构造 `PointerType(lower(base))`。这样每个指针都随身携带准确的 pointee（虽然 `to_string()` 仍输出 `ptr`），后续 `load/store/gep` 可以直接读取指向类型。
- **`()` (`UNIT`)**：统一映射为 `ir::VoidType`；若某表达式需要物理槽位，则由 IRBuilder 创建零字节栈槽。
- **`!` (`NEVER`)**：理论上没有值，但 IR 层仍以 `void` 表示；调用者负责在控制流级别插入 `unreachable`。
- **数组 (`ARRAY`)**：调用 `lower` 递归得到元素类型，再读取 `ArrayRealType::size` 创建 `ir::ArrayType(elem_ir, size)`。若 `size` 仍为 0 说明语义阶段未填回数组长度，应抛出 `std::runtime_error("array size missing")`。该类型仅用于静态分配的栈槽/全局常量，动态数组（`String`/`Vec`）仍走结构体。
- **字符串切片 (`STR`)**：固定返回结构 `{ ptr(i8*), i32 }`。TypeLowering 应在初始化时调用 `declare_struct` 注册 `%Str = type { ptr, i32 }` 并缓存，后续 `lower(StrRealType)` 直接复用。不要依赖 `IRModule::ensure_runtime_builtins` 来插入这些类型，避免初始化顺序错乱。
- **`String`**：同 `IRBuilder` 设定 `{ ptr(i8*), i32, i32 }`（指针、长度、容量）。同样在 TypeLowering 初始化时通过 `declare_struct` 注册 `%String = type { ... }` 并缓存，`lower(StringRealType)` 必须命中缓存，否则抛错。
- **结构体 (`STRUCT`)**：读取 `StructDecl_ptr decl = struct_type->decl.lock()`，若缓存中尚未存在则调用 `declare_struct(decl)` 生成并注册 `%StructName = type { ... }`（会同步更新 `IRModule` 的类型表）。当语义层用 `Self` 等别名描述该类型时，也必须始终取 `decl->name` 作为 `%Name`，避免把函数名或别名写进 IR。若结构体出现在引用上下文（`StructRealType::is_ref != NO_REF`），优先把结构体类型缓存下来，再在上一条规则应用后返回 `ptr`。若 `decl.lock()` 失败（语义阶段未绑定），TypeLowering 抛出 `std::runtime_error("StructDecl missing")`。
- **枚举 (`ENUM`)**：当前语义阶段把 enum 当 C-like 枚举处理：`EnumDecl::variants` 只有离散值，因此直接下降为 `i32`。若未来支持带负载的 enum，再扩展为 `{ i32, [payload] }`。
- **函数 (`FUNCTION`)**：`lower_function` 中构造：对参数逐个 `lower`（引用参数直接得到 `ptr`），返回类型 `()` → `void`，其余交给 `lower`。
  * 若函数带有 `self` 形参，`FnDecl::receiver_type` 会指明是 `self` / `&self` / `&mut self`，`self_struct` 指向所属结构体。`lower_function` 必须把 `self` 作为参数列表第一项并按对应 `RealType` 降级：按值 `self` → `%StructName`（始终读取 `self_struct->name`，即便语义层把类型写成 `Self`），`&self`/`&mut self` → `ptr`，其他类型（如 `String`）则复用 TypeLowering 既有规则，确保 IR 签名与语义一致。
  * 入口 `main` 函数强制返回 `i32`：即便语义层未显式声明返回类型或写成 `()`, `lower_function` 也会把 `ret_type` 改为 `i32`，与运行时预期的 `main` ABI 对齐。

#### 常量值桥接
- `lower_const(ConstValue_ptr value, RealType_ptr expected_type)` 的行为：
  * 先把 `expected_type` 降为 `ir::IRType`，并根据目标类型决定允许的 `ConstValueKind`。例如 `Bool_ConstValue` 只能映射到 `i1`，`Char_ConstValue` 只能映射到 `i8`。若 `expected_type` 为引用或聚合类型（数组/结构体/String/&str），直接返回 `nullptr`，提醒调用者改走全局常量路径。
  * `ConstValueKind::ANYINT` 在 const-eval 完成后理论上不应再出现：若仍遇到该枚举，直接抛出 `std::runtime_error("ConstValue ANYINT not concretized")` 要求语义阶段补全类型。其余整型常量按 `I32/ISIZE` 符号扩展、`U32/USIZE` 零扩展处理。
  * `ConstValueKind::CHAR` 输出 `i8` 的整数，使用 UTF-8 codepoint；`ConstValueKind::BOOL` 输出 `i1`。
  * `ConstValueKind::ARRAY`、`ConstValueKind::UNIT`、`Str slice` 等复合值返回 `nullptr`，调用者据此判断需要退回到“生成全局数据/运行时初始化”的流程。
  * 函数在必要时抛出（或返回 `nullptr` 并让上层诊断）类型不匹配的错误，避免语义与 IR 结果不一致。所有错误信息都带上 `ConstValueKind` 与 `expected_type->show_real_type_info()`，方便定位。
- 调用者在从 `ConstDecl` 获取 `ConstValue_ptr` 后直接调用 `lower_const` 即可。由于本阶段不进行常量折叠，`TypeLowering` 既不会修改 AST，也不会把运算符替换为常量指令；也就是说 `const` 的值在 IR 层仍旧通过 `ConstantValue` 或全局常量加载。

#### 与 IRBuilder 的协作
- `TypeLowering` 负责把所有命名结构体注册进 `IRModule`，`IRBuilder` 只消费 `ir::StructType` 和 `ir::ArrayType`，不再关心语义数据。
- 对于 `String`/`&str`，`TypeLowering` 会在模块入口创建一次 `%String`、`%StrSlice`，且以 `builder.string_struct_type()` 之类的别名形式暴露给 IR 构建代码。
- `lower_function` 返回的 `ir::FunctionType` 直接交给 `IRModule::declare_function`，保证入口函数 `main` 以及内建函数签名（如 `print({ ptr, i32 }) -> void`）与 `docs/IR/IRBuilder.md` 一致。

#### 测试约定
- `test/TypeLowering/test_type_lowering.cpp`：
  * 使用伪造的 `StructDecl`/`EnumDecl`，调用 `lower` 并断言 `to_string()` 结果，例如 `Struct` → `%Point = type { i32, i32 }`、`&Point` → `ptr`，并进一步通过 `PointerType::pointee_type()` 验证指针确实指向 `%Point`。
  * 构造 `ArrayRealType` 并确认当 `size` 已由语义阶段写回时能正确生成 `[N x T]`；再构造一个未填写 `size` 的数组，断言 `lower` 会抛出缺失数组长度的错误。
  * 验证 `StrRealType`、`StringRealType` 均获得固定布局，并且多次调用重用缓存（比较 `StructType` 指针地址）。
  * 函数签名测试：创建一个伪造的 `FnDecl`，包含引用参数和 `()` 返回值，断言 `lower_function` 产出的 `FunctionType` 序列化为 `void (ptr, i32)`.
- `lower_const()` 覆盖：
  * 输入 `Bool_ConstValue`/`I32_ConstValue`/`Char_ConstValue` 时返回 `ir::ConstantValue`，并检验 `typed_repr()`。
  * 输入 `Array_ConstValue`/`Unit_ConstValue`/`Str`（`ConstValueKind::ARRAY` 包含字符）返回 `nullptr`。
  * 传入与预期类型不匹配时断言会抛出 `std::runtime_error`，说明函数不会悄悄折叠或强行截断。
