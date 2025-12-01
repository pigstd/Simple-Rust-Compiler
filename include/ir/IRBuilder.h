#ifndef SIMPLE_RUST_COMPILER_IR_IRBUILDER_H
#define SIMPLE_RUST_COMPILER_IR_IRBUILDER_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ir {

class IRType;
class VoidType;
class IntegerType;
class PointerType;
class ArrayType;
class StructType;
class FunctionType;

class IRValue;
class ConstantValue;
class RegisterValue;
class GlobalValue;

class IRInstruction;
class BasicBlock;
class IRFunction;
class IRModule;
class IRSerializer;

using IRType_ptr = std::shared_ptr<IRType>;
using VoidType_ptr = std::shared_ptr<VoidType>;
using IntegerType_ptr = std::shared_ptr<IntegerType>;
using PointerType_ptr = std::shared_ptr<PointerType>;
using ArrayType_ptr = std::shared_ptr<ArrayType>;
using StructType_ptr = std::shared_ptr<StructType>;
using FunctionType_ptr = std::shared_ptr<FunctionType>;

using IRValue_ptr = std::shared_ptr<IRValue>;
using ConstantValue_ptr = std::shared_ptr<ConstantValue>;
using RegisterValue_ptr = std::shared_ptr<RegisterValue>;
using GlobalValue_ptr = std::shared_ptr<GlobalValue>;

using IRInstruction_ptr = std::shared_ptr<IRInstruction>;
using BasicBlock_ptr = std::shared_ptr<BasicBlock>;
using IRFunction_ptr = std::shared_ptr<IRFunction>;
using IRModule_ptr = std::shared_ptr<IRModule>;
enum class ICmpPredicate {
    EQ,
    NE,
    SLT,
    SLE,
    SGT,
    SGE,
    ULT,
    ULE,
    UGT,
    UGE,
};

enum class Opcode {
    Add,
    Sub,
    Mul,
    SDiv,
    UDiv,
    SRem,
    URem,
    And,
    Or,
    Xor,
    Shl,
    AShr,
    LShr,
    ZExt,
    SExt,
    Trunc,
    ICmp,
    Call,
    Alloca,
    Load,
    Store,
    GEP,
    Br,
    CondBr,
    Ret,
};

using ConstantLiteral = int64_t;

class IRType {
  public:
    virtual ~IRType() = default;
    // 转换成 LLVM IR 的文本形式。
    virtual std::string to_string() const = 0;

    // 检查是否为指定 bit 宽度的整数类型。
    virtual bool is_integer(int bits = -1) const;
    // 检查是否为指针类型。
    virtual bool is_pointer() const;
    // 检查是否为 void 类型。
    virtual bool is_void() const;
};

class VoidType : public IRType {
  public:
    // 构造 void 类型。
    VoidType();
    ~VoidType() override;

    // 输出 void 的字符串形式。
    std::string to_string() const override;
};

class IntegerType : public IRType {
  public:
    // 构造指定 bit 宽度的整数类型。
    explicit IntegerType(int bits);
    ~IntegerType() override;

    // 读取整数类型的 bit 宽度。
    int bit_width() const;
    // 输出整数的 LLVM 表示。
    std::string to_string() const override;

  private:
    int bit_width_;
};

class PointerType : public IRType {
  public:
    // 构造指针类型，pointee 表示被指向的元素类型。
    explicit PointerType(IRType_ptr pointee);
    ~PointerType() override;

    // 返回指针指向的元素类型。
    IRType_ptr pointee_type() const;
    // 输出指针的 LLVM 表示。
    std::string to_string() const override;

  private:
    IRType_ptr pointee_;
};

class ArrayType : public IRType {
  public:
    // 构造数组类型，包括元素类型和数量。
    ArrayType(IRType_ptr element_type, std::size_t element_count);
    ~ArrayType() override;

    // 返回数组的元素类型。
    IRType_ptr element_type() const;
    // 返回数组长度。
    std::size_t element_count() const;
    // 输出数组的 LLVM 表示。
    std::string to_string() const override;

  private:
    IRType_ptr element_type_;
    std::size_t element_count_;
};

class StructType : public IRType {
  public:
    // 构造命名结构体类型。
    explicit StructType(std::string name);
    ~StructType() override;

    // 设置结构体字段类型列表。
    void set_fields(std::vector<IRType_ptr> fields);
    // 获取结构体字段类型列表。
    const std::vector<IRType_ptr> &fields() const;
    // 获取结构体名字。
    const std::string &name() const;
    // 输出结构体在 LLVM 中的表示。
    std::string to_string() const override;

  private:
    std::string name_;
    std::vector<IRType_ptr> fields_;
};

class FunctionType : public IRType {
  public:
    // 构造函数类型，包括返回值与参数类型。
    FunctionType(IRType_ptr return_type, std::vector<IRType_ptr> param_types);
    ~FunctionType() override;

    // 读取返回值类型。
    IRType_ptr return_type() const;
    // 读取参数类型列表。
    const std::vector<IRType_ptr> &param_types() const;
    // 输出函数类型在 LLVM 中的片段。
    std::string to_string() const override;

  private:
    IRType_ptr return_type_;
    std::vector<IRType_ptr> param_types_;
};

class IRValue : public std::enable_shared_from_this<IRValue> {
  public:
    // 使用对应类型构造 IRValue。
    explicit IRValue(IRType_ptr type);
    virtual ~IRValue() = default;

    // 获取 Value 的类型。
    IRType_ptr type() const;
    // 返回 Value 在指令中的文本表示。
    virtual std::string repr() const = 0;
    // 返回带类型的文本表示。
    virtual std::string typed_repr() const;

  protected:
    IRType_ptr type_;
};

class RegisterValue : public IRValue {
  public:
    // 构造寄存器值，带名字。
    RegisterValue(std::string name, IRType_ptr type);
    ~RegisterValue() override = default;

    // 返回寄存器名字。
    const std::string &name() const;
    // 输出 `%name` 形式的文本。
    std::string repr() const override;

  private:
    std::string name_;
};

class ConstantValue : public IRValue {
  public:
    // 构造字面量常量。
    ConstantValue(IRType_ptr type, ConstantLiteral literal);
    ~ConstantValue() override = default;

    // 访问常量字面量。
    const ConstantLiteral &literal() const;
    // 输出裸值形式。
    std::string repr() const override;
    // 输出带类型的常量。
    std::string typed_repr() const override;

  private:
    ConstantLiteral literal_;
};

class GlobalValue : public IRValue {
  public:
    // 构造全局符号引用。
    GlobalValue(std::string name, IRType_ptr pointee_type,
                std::string init_text, bool is_const = true,
                std::string linkage = "private");
    ~GlobalValue() override = default;

    // 返回全局名字。
    const std::string &name() const;
    // 返回存储类型。
    IRType_ptr pointee_type() const;
    // 返回初始值文本。
    const std::string &init_text() const;
    // 是否为常量。
    bool is_const() const;
    // 获取链接属性。
    const std::string &linkage() const;
    // 输出 `@name` 形式。
    std::string repr() const override;
    // 输出 `ptr @name` 形式。
    std::string typed_repr() const override;
    // 输出全局定义文本。
    std::string definition_string() const;

  private:
    std::string name_;
    IRType_ptr pointee_type_;
    std::string init_text_;
    bool is_const_;
    std::string linkage_;
};

class IRInstruction : public std::enable_shared_from_this<IRInstruction> {
  public:
    // 构造一条指令，包含操作码、操作数和可选结果。
    IRInstruction(Opcode opcode, std::vector<IRValue_ptr> operands,
                  IRValue_ptr result = nullptr);

    // 返回指令操作码。
    Opcode opcode() const;
    // 返回操作数列表。
    const std::vector<IRValue_ptr> &operands() const;
    // 返回结果值，可能为空。
    IRValue_ptr result() const;
    // 设置指令的结果寄存器。
    void set_result(IRValue_ptr result);

    // 设置/获取需要输出的类型字面量（alloca/gep/call）。
    void set_literal_type(IRType_ptr type);
    IRType_ptr literal_type() const;

    // 设置整数比较谓词。
    void set_predicate(ICmpPredicate predicate);
    // 获取比较谓词。
    ICmpPredicate predicate() const;

    // 记录 call 的被调函数名。
    void set_call_callee(std::string callee);
    // 获取被调函数名。
    const std::string &call_callee() const;

    // 设置无条件跳转的目标块。
    void set_branch_target(BasicBlock_ptr target);
    // 获取无条件跳转目标。
    BasicBlock_ptr branch_target() const;

    // 设置条件跳转的真假目标。
    void set_conditional_targets(BasicBlock_ptr true_target,
                                 BasicBlock_ptr false_target);
    // 获取条件跳转的真分支。
    BasicBlock_ptr true_target() const;
    // 获取条件跳转的假分支。
    BasicBlock_ptr false_target() const;

    // 序列化指令为字符串。
    std::string to_string() const;
    // 判断是否为终止指令。
    bool is_terminator() const;

  private:
    Opcode opcode_;
    std::vector<IRValue_ptr> operands_;
    IRValue_ptr result_;
    IRType_ptr literal_type_;
    ICmpPredicate predicate_;
    std::string call_callee_;
    BasicBlock_ptr branch_target_;
    BasicBlock_ptr true_target_;
    BasicBlock_ptr false_target_;
};

class BasicBlock : public std::enable_shared_from_this<BasicBlock> {
  public:
    // 使用标签创建基本块。
    explicit BasicBlock(std::string label);

    // 获取块标签。
    const std::string &label() const;
    // 设置块标签。
    void set_label(std::string label);

    // 在块末尾追加指令。
    IRInstruction_ptr append(IRInstruction_ptr inst);
    // 在终止指令前插入指令。
    IRInstruction_ptr insert_before_terminator(IRInstruction_ptr inst);
    // 返回块当前的终止指令。
    IRInstruction_ptr get_terminator() const;

    // 返回该块的指令列表。
    const std::vector<IRInstruction_ptr> &instructions() const;
    // 序列化基本块到字符串。
    std::string to_string() const;

  private:
    std::string label_;
    std::vector<IRInstruction_ptr> instructions_;
};

class IRFunction : public std::enable_shared_from_this<IRFunction> {
  public:
    // 构造函数定义或声明。
    IRFunction(std::string name, IRType_ptr fn_type,
               bool is_declaration = false);

    // 获取函数名。
    const std::string &name() const;
    // 获取函数类型。
    IRType_ptr type() const;
    // 是否为声明。
    bool is_declaration() const;
    // 标记为声明或定义。
    void set_declaration(bool is_declaration);

    // 获取入口基本块。
    BasicBlock_ptr get_entry_block();
    // 创建一个新的基本块。
    BasicBlock_ptr create_block(const std::string &label);
    // 获取函数的基本块列表。
    const std::vector<BasicBlock_ptr> &blocks() const;

    // 添加形参并返回对应寄存器。
    IRValue_ptr add_param(const std::string &name, IRType_ptr type);
    // 获取参数列表。
    const std::vector<std::pair<std::string, IRType_ptr>> &params() const;

    // 生成函数签名字符串。
    std::string signature_string() const;
    // 序列化整个函数。
    std::string to_string() const;
    // 将函数信息输出到 stderr。
    void dump() const;

  private:
    std::string name_;
    IRType_ptr fn_type_;
    std::vector<std::pair<std::string, IRType_ptr>> params_;
    std::vector<BasicBlock_ptr> blocks_;
    bool is_declaration_;
    std::unordered_map<std::string, std::size_t> block_name_counter_;
};

class IRModule : public std::enable_shared_from_this<IRModule> {
  public:
    // 构造一个 IR 模块，指定 triple 和数据布局。
    IRModule(std::string target_triple, std::string data_layout);

    // 获取 target triple。
    const std::string &target_triple() const;
    // 获取 data layout。
    const std::string &data_layout() const;
    // 设置 target triple。
    void set_target_triple(std::string target_triple);
    // 设置 data layout。
    void set_data_layout(std::string data_layout);

    // 添加结构体等类型定义的文本。
    void add_type_definition(std::string name, std::vector<std::string> fields);
    // 获取全部类型定义。
    const std::vector<std::pair<std::string, std::vector<std::string>>> &
    type_definitions() const;

    // 添加模块级注释行，会在序列化最前方输出。
    void add_module_comment(std::string comment);
    // 读取模块注释列表。
    const std::vector<std::string> &module_comments() const;

    // 创建全局变量或常量。
    GlobalValue_ptr create_global(const std::string &name, IRType_ptr type,
                                  const std::string &init_text,
                                  bool is_const = true,
                                  const std::string &linkage = "private");
    // 声明函数（无函数体）。
    IRFunction_ptr declare_function(const std::string &name, IRType_ptr fn_type,
                                    bool is_builtin = false);
    // 定义函数并返回以便填充。
    IRFunction_ptr define_function(const std::string &name, IRType_ptr fn_type);

    // 返回所有全局。
    const std::vector<GlobalValue_ptr> &globals() const;
    // 返回所有函数。
    const std::vector<IRFunction_ptr> &functions() const;

    // 序列化整个模块。
    std::string to_string() const;
    // 打印模块信息用于调试。
    void dump() const;

  private:
    std::string target_triple_;
    std::string data_layout_;
    std::vector<std::pair<std::string, std::vector<std::string>>>
        type_definitions_;
    std::vector<std::string> module_comments_;
    std::vector<GlobalValue_ptr> globals_;
    std::vector<IRFunction_ptr> functions_;
};

class IRSerializer {
  public:
    // 以模块为输入构造序列化器。
    explicit IRSerializer(const IRModule &module);

    // 输出模块序列化结果。
    std::string serialize() const;

  private:
    const IRModule &module_;
};

class IRBuilder {
  public:
    // 使用模块创建一个 IRBuilder。
    explicit IRBuilder(IRModule &module);

    // 获取 builder 关联的模块。
    IRModule &module();
    // 获取常量引用。
    const IRModule &module() const;

    // 设置当前的插入基本块。
    void set_insertion_point(BasicBlock_ptr block);
    // 获取当前插入块。
    BasicBlock_ptr insertion_block() const;

    BasicBlock_ptr create_block(const std::string &label);
    IRValue_ptr create_temp(IRType_ptr type, const std::string &name_hint = "");
    IRValue_ptr create_temp_alloca(IRType_ptr type,
                                   const std::string &name_hint = "");

    IRValue_ptr create_alloca(IRType_ptr type,
                              const std::string &name_hint = "");
    IRValue_ptr create_load(IRValue_ptr address,
                            const std::string &name_hint = "");
    void create_store(IRValue_ptr value, IRValue_ptr address);
    IRValue_ptr create_gep(IRValue_ptr base_ptr, IRType_ptr element_type,
                           const std::vector<IRValue_ptr> &indices,
                           const std::string &name_hint = "");

    IRValue_ptr create_add(IRValue_ptr lhs, IRValue_ptr rhs,
                           const std::string &name_hint = "");
    IRValue_ptr create_sub(IRValue_ptr lhs, IRValue_ptr rhs,
                           const std::string &name_hint = "");
    IRValue_ptr create_mul(IRValue_ptr lhs, IRValue_ptr rhs,
                           const std::string &name_hint = "");
    IRValue_ptr create_sdiv(IRValue_ptr lhs, IRValue_ptr rhs,
                            const std::string &name_hint = "");
    IRValue_ptr create_udiv(IRValue_ptr lhs, IRValue_ptr rhs,
                            const std::string &name_hint = "");
    IRValue_ptr create_srem(IRValue_ptr lhs, IRValue_ptr rhs,
                            const std::string &name_hint = "");
    IRValue_ptr create_urem(IRValue_ptr lhs, IRValue_ptr rhs,
                            const std::string &name_hint = "");
    IRValue_ptr create_and(IRValue_ptr lhs, IRValue_ptr rhs,
                           const std::string &name_hint = "");
    IRValue_ptr create_or(IRValue_ptr lhs, IRValue_ptr rhs,
                          const std::string &name_hint = "");
    IRValue_ptr create_xor(IRValue_ptr lhs, IRValue_ptr rhs,
                           const std::string &name_hint = "");
    IRValue_ptr create_shl(IRValue_ptr lhs, IRValue_ptr rhs,
                           const std::string &name_hint = "");
    IRValue_ptr create_lshr(IRValue_ptr lhs, IRValue_ptr rhs,
                            const std::string &name_hint = "");
    IRValue_ptr create_ashr(IRValue_ptr lhs, IRValue_ptr rhs,
                            const std::string &name_hint = "");
    IRValue_ptr create_zext(IRValue_ptr value, IRType_ptr target_type,
                            const std::string &name_hint = "");
    IRValue_ptr create_sext(IRValue_ptr value, IRType_ptr target_type,
                            const std::string &name_hint = "");
    IRValue_ptr create_trunc(IRValue_ptr value, IRType_ptr target_type,
                             const std::string &name_hint = "");

    IRValue_ptr create_icmp_eq(IRValue_ptr lhs, IRValue_ptr rhs,
                               const std::string &name_hint = "");
    IRValue_ptr create_icmp_ne(IRValue_ptr lhs, IRValue_ptr rhs,
                               const std::string &name_hint = "");
    IRValue_ptr create_icmp_slt(IRValue_ptr lhs, IRValue_ptr rhs,
                                const std::string &name_hint = "");
    IRValue_ptr create_icmp_sle(IRValue_ptr lhs, IRValue_ptr rhs,
                                const std::string &name_hint = "");
    IRValue_ptr create_icmp_sgt(IRValue_ptr lhs, IRValue_ptr rhs,
                                const std::string &name_hint = "");
    IRValue_ptr create_icmp_sge(IRValue_ptr lhs, IRValue_ptr rhs,
                                const std::string &name_hint = "");
    IRValue_ptr create_icmp_ult(IRValue_ptr lhs, IRValue_ptr rhs,
                                const std::string &name_hint = "");
    IRValue_ptr create_icmp_ule(IRValue_ptr lhs, IRValue_ptr rhs,
                                const std::string &name_hint = "");
    IRValue_ptr create_icmp_ugt(IRValue_ptr lhs, IRValue_ptr rhs,
                                const std::string &name_hint = "");
    IRValue_ptr create_icmp_uge(IRValue_ptr lhs, IRValue_ptr rhs,
                                const std::string &name_hint = "");
    IRValue_ptr create_not(IRValue_ptr value,
                           const std::string &name_hint = "");

    void create_br(BasicBlock_ptr target);
    void create_cond_br(IRValue_ptr condition, BasicBlock_ptr true_target,
                        BasicBlock_ptr false_target);
    void create_ret(IRValue_ptr value = nullptr);

    // 创建函数调用指令。
    IRValue_ptr create_call(const std::string &callee,
                            const std::vector<IRValue_ptr> &args,
                            IRType_ptr ret_type, const std::string &name = "");

    void create_memcpy(IRValue_ptr dst, IRValue_ptr src, IRValue_ptr length,
                       bool is_volatile = false);

    // 创建字符串字面量全局并返回其引用。
    GlobalValue_ptr create_string_literal(const std::string &text);
    // 创建 i32 常量。
    IRValue_ptr create_i32_constant(int64_t value);

  private:
    // 在当前块插入指令。
    IRInstruction_ptr insert_instruction(IRInstruction_ptr inst);
    // 统一处理简单二元运算。
    IRValue_ptr create_simple_arith(Opcode opcode, IRValue_ptr lhs,
                                    IRValue_ptr rhs,
                                    const std::string &name_hint);
    // 统一处理整数比较。
    IRValue_ptr create_compare(ICmpPredicate predicate, IRValue_ptr lhs,
                               IRValue_ptr rhs, const std::string &name_hint);
    IRValue_ptr create_cast(Opcode opcode, IRValue_ptr value,
                            IRType_ptr target_type,
                            const std::string &name_hint);

    IRModule &module_;
    IRFunction_ptr current_function_;
    BasicBlock_ptr insertion_block_;
    std::size_t next_reg_index_;
    std::unordered_map<std::string, std::size_t> name_hint_counters_;
    bool memcpy_declared_ = false;

    void ensure_memcpy_declared();
};

} // namespace ir

#endif // SIMPLE_RUST_COMPILER_IR_IRBUILDER_H
