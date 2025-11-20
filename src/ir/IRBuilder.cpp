#include "ir/IRBuilder.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace ir {

bool IRType::is_integer(int bits) const {
    const auto *int_type = dynamic_cast<const IntegerType *>(this);
    if (int_type == nullptr) {
        return false;
    }
    return bits < 0 || int_type->bit_width() == bits;
}

bool IRType::is_pointer() const {
    return dynamic_cast<const PointerType *>(this) != nullptr;
}

VoidType::VoidType() = default;
VoidType::~VoidType() = default;

std::string VoidType::to_string() const { return "void"; }

IntegerType::IntegerType(int bits) : bit_width_(bits) {}
IntegerType::~IntegerType() = default;

int IntegerType::bit_width() const { return bit_width_; }

std::string IntegerType::to_string() const {
    return "i" + std::to_string(bit_width_);
}

PointerType::PointerType() = default;
PointerType::~PointerType() = default;

std::string PointerType::to_string() const { return "ptr"; }

ArrayType::ArrayType(IRType_ptr element_type, std::size_t element_count)
    : element_type_(std::move(element_type)), element_count_(element_count) {}
ArrayType::~ArrayType() = default;

IRType_ptr ArrayType::element_type() const { return element_type_; }

std::size_t ArrayType::element_count() const { return element_count_; }

std::string ArrayType::to_string() const {
    std::ostringstream oss;
    oss << "[" << element_count_ << " x " << element_type_->to_string()
        << "]";
    return oss.str();
}

StructType::StructType(std::string name) : name_(std::move(name)) {}
StructType::~StructType() = default;

void StructType::set_fields(std::vector<IRType_ptr> fields) {
    fields_ = std::move(fields);
}

const std::vector<IRType_ptr> &StructType::fields() const { return fields_; }

const std::string &StructType::name() const { return name_; }

std::string StructType::to_string() const {
    return "%" + name_;
}

FunctionType::FunctionType(IRType_ptr return_type,
                           std::vector<IRType_ptr> param_types)
    : return_type_(std::move(return_type)),
      param_types_(std::move(param_types)) {}
FunctionType::~FunctionType() = default;

IRType_ptr FunctionType::return_type() const { return return_type_; }

const std::vector<IRType_ptr> &FunctionType::param_types() const {
    return param_types_;
}

std::string FunctionType::to_string() const {
    std::ostringstream oss;
    oss << return_type_->to_string() << " (";
    for (std::size_t i = 0; i < param_types_.size(); ++i) {
        if (i > 0) {
            oss << ", ";
        }
        oss << param_types_[i]->to_string();
    }
    oss << ")";
    return oss.str();
}

IRValue::IRValue(IRType_ptr type) : type_(std::move(type)) {}

IRType_ptr IRValue::type() const { return type_; }

RegisterValue::RegisterValue(std::string name, IRType_ptr type)
    : IRValue(std::move(type)), name_(std::move(name)) {}

const std::string &RegisterValue::name() const { return name_; }

std::string RegisterValue::repr() const { return "%" + name_; }

ConstantValue::ConstantValue(IRType_ptr type, ConstantLiteral literal)
    : IRValue(std::move(type)), literal_(literal) {}

const ConstantLiteral &ConstantValue::literal() const { return literal_; }

std::string ConstantValue::repr() const {
    const auto *int_type = dynamic_cast<const IntegerType *>(type_.get());
    if (int_type != nullptr && int_type->bit_width() == 1) {
        return literal_ ? "true" : "false";
    }
    return std::to_string(literal_);
}

std::string ConstantValue::typed_repr() const {
    return type_->to_string() + " " + repr();
}

GlobalValue::GlobalValue(std::string name, IRType_ptr type,
                         std::string init_text, bool is_const,
                         std::string linkage)
    : IRValue(std::move(type)), name_(std::move(name)),
      init_text_(std::move(init_text)), is_const_(is_const),
      linkage_(std::move(linkage)) {}

const std::string &GlobalValue::name() const { return name_; }

const std::string &GlobalValue::init_text() const { return init_text_; }

bool GlobalValue::is_const() const { return is_const_; }

const std::string &GlobalValue::linkage() const { return linkage_; }

std::string GlobalValue::repr() const { return "@" + name_; }

std::string GlobalValue::typed_repr() const { return "ptr @" + name_; }

std::string GlobalValue::definition_string() const {
    std::ostringstream oss;
    oss << "@" << name_ << " = " << linkage_ << " "
        << (is_const_ ? "constant " : "global ") << type_->to_string()
        << " " << init_text_;
    return oss.str();
}

namespace {

class TypeLiteralValue : public IRValue {
  public:
    explicit TypeLiteralValue(IRType_ptr type)
        : IRValue(std::move(type)) {}

    std::string repr() const override { return type_->to_string(); }
};

IRValue_ptr make_type_literal(IRType_ptr type) {
    return std::make_shared<TypeLiteralValue>(std::move(type));
}

std::string typed_value_repr(const IRValue_ptr &value) {
    if (auto constant = std::dynamic_pointer_cast<ConstantValue>(value)) {
        return constant->typed_repr();
    }
    if (auto global = std::dynamic_pointer_cast<GlobalValue>(value)) {
        return global->typed_repr();
    }
    return value->type()->to_string() + " " + value->repr();
}

bool is_void_type(const IRType_ptr &type) {
    return dynamic_cast<const VoidType *>(type.get()) != nullptr;
}

std::unordered_map<const IRValue *, IRType_ptr> &pointer_pointee_map() {
    static std::unordered_map<const IRValue *, IRType_ptr> map;
    return map;
}

void remember_pointer_pointee(const IRValue_ptr &value,
                              const IRType_ptr &pointee) {
    if (!value || !pointee) {
        return;
    }
    pointer_pointee_map()[value.get()] = pointee;
}

IRType_ptr lookup_pointer_pointee(const IRValue_ptr &value) {
    if (!value) {
        return nullptr;
    }
    auto it = pointer_pointee_map().find(value.get());
    if (it == pointer_pointee_map().end()) {
        return nullptr;
    }
    return it->second;
}

IRType_ptr deduce_gep_pointee(IRType_ptr root_type,
                              const std::vector<IRValue_ptr> &indices) {
    if (!root_type) {
        return nullptr;
    }
    IRType_ptr current = root_type;
    bool consumed_pointer = false;
    for (std::size_t i = 0; i < indices.size(); ++i) {
        if (!consumed_pointer) {
            consumed_pointer = true;
            continue;
        }
        if (!current) {
            break;
        }
        if (auto array_type = std::dynamic_pointer_cast<ArrayType>(current)) {
            current = array_type->element_type();
            continue;
        }
        if (auto struct_type = std::dynamic_pointer_cast<StructType>(current)) {
            auto constant =
                std::dynamic_pointer_cast<ConstantValue>(indices[i]);
            if (!constant) {
                throw std::runtime_error(
                    "GEP struct index must be a constant literal");
            }
            if (constant->literal() < 0) {
                throw std::runtime_error(
                    "GEP struct index cannot be negative");
            }
            const auto idx = static_cast<std::size_t>(constant->literal());
            const auto &fields = struct_type->fields();
            if (idx >= fields.size()) {
                throw std::runtime_error("GEP struct index out of range");
            }
            current = fields[idx];
            continue;
        }
    }
    return current ? current : root_type;
}

std::unordered_map<const IRModule *, std::size_t> &string_literal_counters() {
    static std::unordered_map<const IRModule *, std::size_t> counters;
    return counters;
}

std::string encode_string_literal(const std::string &text) {
    std::ostringstream oss;
    oss << "c\"";
    for (char raw : text) {
        const unsigned char ch = static_cast<unsigned char>(raw);
        if (std::isprint(static_cast<unsigned char>(ch)) && ch != '\\' &&
            ch != '\"') {
            oss << static_cast<char>(ch);
            continue;
        }
        oss << "\\";
        oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(ch);
        oss << std::nouppercase << std::dec;
    }
    oss << "\\00\"";
    return oss.str();
}

const char *predicate_to_string(ICmpPredicate predicate) {
    switch (predicate) {
    case ICmpPredicate::EQ:
        return "eq";
    case ICmpPredicate::NE:
        return "ne";
    case ICmpPredicate::SLT:
        return "slt";
    case ICmpPredicate::SLE:
        return "sle";
    case ICmpPredicate::SGT:
        return "sgt";
    case ICmpPredicate::SGE:
        return "sge";
    case ICmpPredicate::ULT:
        return "ult";
    case ICmpPredicate::ULE:
        return "ule";
    case ICmpPredicate::UGT:
        return "ugt";
    case ICmpPredicate::UGE:
        return "uge";
    }
    return "eq";
}

} // namespace

IRInstruction::IRInstruction(Opcode opcode,
                             std::vector<IRValue_ptr> operands,
                             IRValue_ptr result)
    : opcode_(opcode), operands_(std::move(operands)),
      result_(std::move(result)), predicate_(ICmpPredicate::EQ) {}

Opcode IRInstruction::opcode() const { return opcode_; }

const std::vector<IRValue_ptr> &IRInstruction::operands() const {
    return operands_;
}

IRValue_ptr IRInstruction::result() const { return result_; }

void IRInstruction::set_result(IRValue_ptr result) {
    result_ = std::move(result);
}

void IRInstruction::set_predicate(ICmpPredicate predicate) {
    predicate_ = predicate;
}

ICmpPredicate IRInstruction::predicate() const { return predicate_; }

void IRInstruction::set_call_callee(std::string callee) {
    call_callee_ = std::move(callee);
}

const std::string &IRInstruction::call_callee() const { return call_callee_; }

void IRInstruction::set_branch_target(BasicBlock_ptr target) {
    branch_target_ = std::move(target);
}

BasicBlock_ptr IRInstruction::branch_target() const { return branch_target_; }

void IRInstruction::set_conditional_targets(BasicBlock_ptr true_target,
                                           BasicBlock_ptr false_target) {
    true_target_ = std::move(true_target);
    false_target_ = std::move(false_target);
}

BasicBlock_ptr IRInstruction::true_target() const { return true_target_; }

BasicBlock_ptr IRInstruction::false_target() const { return false_target_; }

namespace {

const char *opcode_to_string(Opcode opcode) {
    switch (opcode) {
    case Opcode::Add:
        return "add";
    case Opcode::Sub:
        return "sub";
    case Opcode::Mul:
        return "mul";
    case Opcode::SDiv:
        return "sdiv";
    case Opcode::UDiv:
        return "udiv";
    case Opcode::SRem:
        return "srem";
    case Opcode::URem:
        return "urem";
    case Opcode::And:
        return "and";
    case Opcode::Or:
        return "or";
    case Opcode::Xor:
        return "xor";
    case Opcode::Shl:
        return "shl";
    case Opcode::AShr:
        return "ashr";
    case Opcode::LShr:
        return "lshr";
    default:
        break;
    }
    return "";
}

} // namespace

std::string IRInstruction::to_string() const {
    std::ostringstream oss;
    auto emit_binary = [&](const char *mnemonic) {
        oss << mnemonic << " " << operands_[0]->type()->to_string() << " "
            << operands_[0]->repr() << ", " << operands_[1]->repr();
    };

    switch (opcode_) {
    case Opcode::Add:
    case Opcode::Sub:
    case Opcode::Mul:
    case Opcode::SDiv:
    case Opcode::UDiv:
    case Opcode::SRem:
    case Opcode::URem:
    case Opcode::And:
    case Opcode::Or:
    case Opcode::Xor:
    case Opcode::Shl:
    case Opcode::AShr:
    case Opcode::LShr:
        if (result_) {
            oss << result_->repr() << " = ";
        }
        emit_binary(opcode_to_string(opcode_));
        break;
    case Opcode::ICmp:
        if (result_) {
            oss << result_->repr() << " = ";
        }
        oss << "icmp " << predicate_to_string(predicate_) << " "
            << operands_[0]->type()->to_string() << " "
            << operands_[0]->repr() << ", " << operands_[1]->repr();
        break;
    case Opcode::Call: {
        if (result_) {
            oss << result_->repr() << " = ";
        }
        IRType_ptr ret_type = result_ ? result_->type() : nullptr;
        std::size_t arg_index = 0;
        if (!result_) {
            if (!operands_.empty()) {
                ret_type = operands_[0]->type();
                arg_index = 1;
            }
        }
        if (!ret_type) {
            ret_type = std::make_shared<VoidType>();
        }
        oss << "call " << ret_type->to_string() << " @" << call_callee_
            << "(";
        bool first = true;
        for (std::size_t i = arg_index; i < operands_.size(); ++i) {
            if (!result_ && i == 0) {
                continue;
            }
            if (!first) {
                oss << ", ";
            }
            oss << typed_value_repr(operands_[i]);
            first = false;
        }
        oss << ")";
        break;
    }
    case Opcode::Alloca:
        if (result_) {
            oss << result_->repr() << " = ";
        }
        oss << "alloca " << operands_[0]->repr();
        break;
    case Opcode::Load:
        if (result_) {
            oss << result_->repr() << " = ";
        }
        oss << "load " << result_->type()->to_string() << ", "
            << typed_value_repr(operands_[0]);
        break;
    case Opcode::Store:
        oss << "store " << typed_value_repr(operands_[0]) << ", "
            << typed_value_repr(operands_[1]);
        break;
    case Opcode::GEP:
        if (result_) {
            oss << result_->repr() << " = ";
        }
        oss << "getelementptr " << operands_[0]->repr() << ", "
            << typed_value_repr(operands_[1]);
        for (std::size_t i = 2; i < operands_.size(); ++i) {
            oss << ", " << typed_value_repr(operands_[i]);
        }
        break;
    case Opcode::Br:
        oss << "br label %" << branch_target_->label();
        break;
    case Opcode::CondBr:
        oss << "br " << typed_value_repr(operands_[0]) << ", label %"
            << true_target_->label() << ", label %"
            << false_target_->label();
        break;
    case Opcode::Ret:
        if (operands_.empty()) {
            oss << "ret void";
        } else {
            oss << "ret " << typed_value_repr(operands_[0]);
        }
        break;
    }
    return oss.str();
}

bool IRInstruction::is_terminator() const {
    return opcode_ == Opcode::Br || opcode_ == Opcode::CondBr ||
           opcode_ == Opcode::Ret;
}

BasicBlock::BasicBlock(std::string label) : label_(std::move(label)) {}

const std::string &BasicBlock::label() const { return label_; }

void BasicBlock::set_label(std::string label) { label_ = std::move(label); }

IRInstruction_ptr BasicBlock::append(IRInstruction_ptr inst) {
    if (!inst) {
        return nullptr;
    }
    if (!instructions_.empty() && instructions_.back()->is_terminator()) {
        if (inst->is_terminator()) {
            throw std::runtime_error(
                "Cannot append a terminator after block terminator");
        }
        return insert_before_terminator(std::move(inst));
    }
    instructions_.push_back(std::move(inst));
    return instructions_.back();
}

IRInstruction_ptr BasicBlock::insert_before_terminator(IRInstruction_ptr inst) {
    auto it = std::find_if(instructions_.begin(), instructions_.end(),
                           [](const IRInstruction_ptr &candidate) {
                               return candidate->is_terminator();
                           });
    if (it == instructions_.end()) {
        instructions_.push_back(std::move(inst));
        return instructions_.back();
    }
    it = instructions_.insert(it, std::move(inst));
    return *it;
}

IRInstruction_ptr BasicBlock::get_terminator() const {
    for (auto it = instructions_.rbegin(); it != instructions_.rend(); ++it) {
        if ((*it)->is_terminator()) {
            return *it;
        }
    }
    return nullptr;
}

const std::vector<IRInstruction_ptr> &BasicBlock::instructions() const {
    return instructions_;
}

std::string BasicBlock::to_string() const {
    std::ostringstream oss;
    oss << label_ << ":\n";
    for (const auto &inst : instructions_) {
        oss << "    " << inst->to_string() << "\n";
    }
    return oss.str();
}

IRFunction::IRFunction(std::string name, IRType_ptr fn_type,
                       bool is_declaration)
    : name_(std::move(name)), fn_type_(std::move(fn_type)),
      is_declaration_(is_declaration) {}

const std::string &IRFunction::name() const { return name_; }

IRType_ptr IRFunction::type() const { return fn_type_; }

bool IRFunction::is_declaration() const { return is_declaration_; }

void IRFunction::set_declaration(bool is_declaration) {
    is_declaration_ = is_declaration;
}

BasicBlock_ptr IRFunction::get_entry_block() {
    if (blocks_.empty()) {
        return nullptr;
    }
    return blocks_.front();
}

BasicBlock_ptr IRFunction::create_block(const std::string &label) {
    std::string final_label = label;
    auto dot_pos = final_label.rfind('.');
    auto has_suffix = [&]() {
        if (dot_pos == std::string::npos || dot_pos + 1 >= final_label.size()) {
            return false;
        }
        auto offset = static_cast<std::string::difference_type>(dot_pos + 1);
        auto suffix_begin = final_label.begin() + offset;
        return std::all_of(suffix_begin, final_label.end(), [](char ch) {
            return std::isdigit(static_cast<unsigned char>(ch));
        });
    }();
    if (!has_suffix) {
        std::string prefix = label + ".";
        std::size_t next_index = 0;
        for (const auto &block : blocks_) {
            const auto &existing = block->label();
            if (existing.rfind(prefix, 0) == 0) {
                auto suffix = existing.substr(prefix.size());
                if (!suffix.empty()) {
                    try {
                        std::size_t value = std::stoul(suffix);
                        next_index = std::max(next_index, value + 1);
                    } catch (...) {
                    }
                }
            }
        }
        final_label = prefix + std::to_string(next_index);
    }
    auto block = std::make_shared<BasicBlock>(final_label);
    blocks_.push_back(block);
    return block;
}

const std::vector<BasicBlock_ptr> &IRFunction::blocks() const {
    return blocks_;
}

IRValue_ptr IRFunction::add_param(const std::string &name, IRType_ptr type) {
    params_.emplace_back(name, type);
    return std::make_shared<RegisterValue>(name, std::move(type));
}

const std::vector<std::pair<std::string, IRType_ptr>> &IRFunction::params() const {
    return params_;
}

std::string IRFunction::signature_string() const {
    auto fn_type = std::dynamic_pointer_cast<FunctionType>(fn_type_);
    if (!fn_type) {
        throw std::runtime_error("IRFunction type was not a FunctionType");
    }
    std::ostringstream oss;
    oss << fn_type->return_type()->to_string() << " @" << name_ << "(";
    if (!params_.empty()) {
        for (std::size_t i = 0; i < params_.size(); ++i) {
            if (i > 0) {
                oss << ", ";
            }
            oss << params_[i].second->to_string() << " %"
                << params_[i].first;
        }
    } else {
        const auto &param_types = fn_type->param_types();
        for (std::size_t i = 0; i < param_types.size(); ++i) {
            if (i > 0) {
                oss << ", ";
            }
            oss << param_types[i]->to_string();
        }
    }
    oss << ")";
    return oss.str();
}

std::string IRFunction::to_string() const {
    if (is_declaration_) {
        std::ostringstream oss;
        oss << "declare " << signature_string() << "\n";
        return oss.str();
    }
    std::ostringstream oss;
    oss << "define " << signature_string() << " {\n";
    for (std::size_t i = 0; i < blocks_.size(); ++i) {
        oss << blocks_[i]->to_string();
        if (i + 1 < blocks_.size()) {
            oss << "\n";
        }
    }
    oss << "}\n";
    return oss.str();
}

void IRFunction::dump() const { std::cerr << to_string(); }

IRModule::IRModule(std::string target_triple, std::string data_layout)
    : target_triple_(std::move(target_triple)),
      data_layout_(std::move(data_layout)), builtins_injected_(false) {}

const std::string &IRModule::target_triple() const { return target_triple_; }

const std::string &IRModule::data_layout() const { return data_layout_; }

void IRModule::set_target_triple(std::string target_triple) {
    target_triple_ = std::move(target_triple);
}

void IRModule::set_data_layout(std::string data_layout) {
    data_layout_ = std::move(data_layout);
}

void IRModule::add_type_definition(std::string name,
                                   std::vector<std::string> fields) {
    type_definitions_.emplace_back(std::move(name), std::move(fields));
}

const std::vector<
    std::pair<std::string, std::vector<std::string>>> &IRModule::type_definitions() const {
    return type_definitions_;
}

void IRModule::add_module_comment(std::string comment) {
    module_comments_.push_back(std::move(comment));
}

const std::vector<std::string> &IRModule::module_comments() const {
    return module_comments_;
}

GlobalValue_ptr IRModule::create_global(const std::string &name,
                                        IRType_ptr type,
                                        const std::string &init_text,
                                        bool is_const,
                                        const std::string &linkage) {
    for (const auto &global : globals_) {
        if (global->name() == name) {
            throw std::runtime_error("Global already exists: " + name);
        }
    }
    auto global = std::make_shared<GlobalValue>(name, std::move(type),
                                                init_text, is_const,
                                                linkage);
    remember_pointer_pointee(global, global->type());
    globals_.push_back(global);
    return global;
}

IRFunction_ptr IRModule::declare_function(const std::string &name,
                                          IRType_ptr fn_type,
                                          bool /*is_builtin*/) {
    for (const auto &fn : functions_) {
        if (fn->name() == name) {
            return fn;
        }
    }
    auto fn = std::make_shared<IRFunction>(name, std::move(fn_type), true);
    functions_.push_back(fn);
    return fn;
}

IRFunction_ptr IRModule::define_function(const std::string &name,
                                         IRType_ptr fn_type) {
    for (const auto &fn : functions_) {
        if (fn->name() == name) {
            fn->set_declaration(false);
            return fn;
        }
    }
    auto fn = std::make_shared<IRFunction>(name, std::move(fn_type), false);
    functions_.push_back(fn);
    return fn;
}

const std::vector<GlobalValue_ptr> &IRModule::globals() const {
    return globals_;
}

const std::vector<IRFunction_ptr> &IRModule::functions() const {
    return functions_;
}

void IRModule::ensure_runtime_builtins() {
    if (builtins_injected_) {
        return;
    }
    builtins_injected_ = true;
    auto void_type = std::make_shared<VoidType>();
    auto i32_type = std::make_shared<IntegerType>(32);
    auto ptr_type = std::make_shared<PointerType>();
    auto str_struct = std::make_shared<StructType>("Str");
    str_struct->set_fields({ptr_type, i32_type});
    auto string_struct = std::make_shared<StructType>("String");
    string_struct->set_fields({ptr_type, i32_type, i32_type});

    declare_function("print",
                     std::make_shared<FunctionType>(void_type,
                                                    std::vector<IRType_ptr>{
                                                        str_struct}),
                     true);
    declare_function("println",
                     std::make_shared<FunctionType>(void_type,
                                                    std::vector<IRType_ptr>{
                                                        str_struct}),
                     true);
    declare_function(
        "exit",
        std::make_shared<FunctionType>(void_type, std::vector<IRType_ptr>{i32_type}),
        true);
    declare_function(
        "String_from",
        std::make_shared<FunctionType>(string_struct,
                                       std::vector<IRType_ptr>{str_struct}),
        true);
    declare_function("Array_len",
                     std::make_shared<FunctionType>(
                         i32_type, std::vector<IRType_ptr>{ptr_type}),
                     true);
}

std::string IRModule::to_string() const {
    std::ostringstream oss;
    for (const auto &comment : module_comments_) {
        oss << comment << "\n";
    }
    oss << "target triple = \"" << target_triple_ << "\"\n";
    oss << "target datalayout = \"" << data_layout_ << "\"\n\n";

    if (!type_definitions_.empty()) {
        for (const auto &entry : type_definitions_) {
            oss << "%" << entry.first << " = type { ";
            for (std::size_t i = 0; i < entry.second.size(); ++i) {
                if (i > 0) {
                    oss << ", ";
                }
                oss << entry.second[i];
            }
            oss << " }\n";
        }
        oss << "\n";
    }

    if (!globals_.empty()) {
        for (const auto &global : globals_) {
            oss << global->definition_string() << "\n";
        }
        oss << "\n";
    }

    std::vector<IRFunction_ptr> declarations;
    std::vector<IRFunction_ptr> definitions;
    for (const auto &fn : functions_) {
        if (fn->is_declaration()) {
            declarations.push_back(fn);
        } else {
            definitions.push_back(fn);
        }
    }

    if (!declarations.empty()) {
        for (const auto &fn : declarations) {
            oss << fn->to_string() << "\n";
        }
    }

    if (!definitions.empty()) {
        if (!declarations.empty()) {
            // Extra blank line between declarations and definitions.
        }
        for (std::size_t i = 0; i < definitions.size(); ++i) {
            oss << definitions[i]->to_string();
            if (i + 1 < definitions.size()) {
                oss << "\n";
            }
        }
    }

    return oss.str();
}

void IRModule::dump() const { std::cerr << to_string(); }

IRSerializer::IRSerializer(const IRModule &module) : module_(module) {}

std::string IRSerializer::serialize() const { return module_.to_string(); }

IRBuilder::IRBuilder(IRModule &module)
    : module_(module), next_reg_index_(0) {}

IRModule &IRBuilder::module() { return module_; }

const IRModule &IRBuilder::module() const { return module_; }

void IRBuilder::set_insertion_point(BasicBlock_ptr block) {
    insertion_block_ = std::move(block);
    if (!insertion_block_) {
        current_function_.reset();
        return;
    }
    for (const auto &fn : module_.functions()) {
        for (const auto &candidate : fn->blocks()) {
            if (candidate == insertion_block_) {
                current_function_ = fn;
                return;
            }
        }
    }
    throw std::runtime_error("Insertion block does not belong to module");
}

BasicBlock_ptr IRBuilder::insertion_block() const { return insertion_block_; }

BasicBlock_ptr IRBuilder::create_block(const std::string &label) {
    if (!current_function_) {
        throw std::runtime_error("No current function to attach block");
    }
    auto &counter = block_name_counter_[label];
    std::string final_label = label + "." + std::to_string(counter++);
    return current_function_->create_block(final_label);
}

IRValue_ptr IRBuilder::create_temp(IRType_ptr type,
                                   const std::string &name_hint) {
    if (!type) {
        throw std::runtime_error("Temporary requires a valid type");
    }
    std::string name = name_hint;
    if (name.empty()) {
        name = std::to_string(next_reg_index_++);
    }
    return std::make_shared<RegisterValue>(name, std::move(type));
}

IRValue_ptr IRBuilder::create_temp_alloca(IRType_ptr type,
                                          const std::string &name_hint) {
    if (!current_function_) {
        throw std::runtime_error(
            "Cannot allocate temporary without current function");
    }
    auto entry = current_function_->get_entry_block();
    if (!entry) {
        entry = current_function_->create_block("entry");
    }
    auto saved_block = insertion_block_;
    set_insertion_point(entry);
    auto slot = create_alloca(std::move(type), name_hint);
    set_insertion_point(saved_block);
    return slot;
}

IRValue_ptr IRBuilder::create_alloca(IRType_ptr type,
                                     const std::string &name_hint) {
    auto ptr_type = std::make_shared<PointerType>();
    auto result = create_temp(ptr_type, name_hint);
    std::vector<IRValue_ptr> operands;
    operands.push_back(make_type_literal(type));
    auto inst = std::make_shared<IRInstruction>(Opcode::Alloca,
                                                std::move(operands), result);
    insert_instruction(inst);
    remember_pointer_pointee(result, type);
    return result;
}

IRValue_ptr IRBuilder::create_load(IRValue_ptr address,
                                   const std::string &name_hint) {
    auto value_type = lookup_pointer_pointee(address);
    if (!value_type) {
        throw std::runtime_error("Unknown pointee type for load");
    }
    auto result = create_temp(value_type, name_hint);
    auto inst =
        std::make_shared<IRInstruction>(Opcode::Load,
                                        std::vector<IRValue_ptr>{address},
                                        result);
    insert_instruction(inst);
    return result;
}

void IRBuilder::create_store(IRValue_ptr value, IRValue_ptr address) {
    auto inst = std::make_shared<IRInstruction>(
        Opcode::Store, std::vector<IRValue_ptr>{value, address});
    insert_instruction(inst);
}

IRValue_ptr IRBuilder::create_gep(IRValue_ptr base_ptr, IRType_ptr element_type,
                                  const std::vector<IRValue_ptr> &indices,
                                  const std::string &name_hint) {
    auto ptr_type = std::make_shared<PointerType>();
    auto result = create_temp(ptr_type, name_hint);
    std::vector<IRValue_ptr> operands;
    operands.reserve(2 + indices.size());
    operands.push_back(make_type_literal(element_type));
    operands.push_back(base_ptr);
    operands.insert(operands.end(), indices.begin(), indices.end());
    auto inst = std::make_shared<IRInstruction>(Opcode::GEP,
                                                std::move(operands), result);
    insert_instruction(inst);
    auto pointee = deduce_gep_pointee(element_type, indices);
    remember_pointer_pointee(result, pointee ? pointee : element_type);
    return result;
}

IRValue_ptr IRBuilder::create_simple_arith(Opcode opcode, IRValue_ptr lhs,
                                           IRValue_ptr rhs,
                                           const std::string &name_hint) {
    auto result = create_temp(lhs->type(), name_hint);
    auto inst = std::make_shared<IRInstruction>(
        opcode, std::vector<IRValue_ptr>{lhs, rhs}, result);
    insert_instruction(inst);
    return result;
}

IRValue_ptr IRBuilder::create_add(IRValue_ptr lhs, IRValue_ptr rhs,
                                  const std::string &name_hint) {
    return create_simple_arith(Opcode::Add, lhs, rhs, name_hint);
}

IRValue_ptr IRBuilder::create_sub(IRValue_ptr lhs, IRValue_ptr rhs,
                                  const std::string &name_hint) {
    return create_simple_arith(Opcode::Sub, lhs, rhs, name_hint);
}

IRValue_ptr IRBuilder::create_mul(IRValue_ptr lhs, IRValue_ptr rhs,
                                  const std::string &name_hint) {
    return create_simple_arith(Opcode::Mul, lhs, rhs, name_hint);
}

IRValue_ptr IRBuilder::create_sdiv(IRValue_ptr lhs, IRValue_ptr rhs,
                                   const std::string &name_hint) {
    return create_simple_arith(Opcode::SDiv, lhs, rhs, name_hint);
}

IRValue_ptr IRBuilder::create_udiv(IRValue_ptr lhs, IRValue_ptr rhs,
                                   const std::string &name_hint) {
    return create_simple_arith(Opcode::UDiv, lhs, rhs, name_hint);
}

IRValue_ptr IRBuilder::create_srem(IRValue_ptr lhs, IRValue_ptr rhs,
                                   const std::string &name_hint) {
    return create_simple_arith(Opcode::SRem, lhs, rhs, name_hint);
}

IRValue_ptr IRBuilder::create_urem(IRValue_ptr lhs, IRValue_ptr rhs,
                                   const std::string &name_hint) {
    return create_simple_arith(Opcode::URem, lhs, rhs, name_hint);
}

IRValue_ptr IRBuilder::create_and(IRValue_ptr lhs, IRValue_ptr rhs,
                                  const std::string &name_hint) {
    return create_simple_arith(Opcode::And, lhs, rhs, name_hint);
}

IRValue_ptr IRBuilder::create_or(IRValue_ptr lhs, IRValue_ptr rhs,
                                 const std::string &name_hint) {
    return create_simple_arith(Opcode::Or, lhs, rhs, name_hint);
}

IRValue_ptr IRBuilder::create_xor(IRValue_ptr lhs, IRValue_ptr rhs,
                                  const std::string &name_hint) {
    return create_simple_arith(Opcode::Xor, lhs, rhs, name_hint);
}

IRValue_ptr IRBuilder::create_shl(IRValue_ptr lhs, IRValue_ptr rhs,
                                  const std::string &name_hint) {
    return create_simple_arith(Opcode::Shl, lhs, rhs, name_hint);
}

IRValue_ptr IRBuilder::create_lshr(IRValue_ptr lhs, IRValue_ptr rhs,
                                   const std::string &name_hint) {
    return create_simple_arith(Opcode::LShr, lhs, rhs, name_hint);
}

IRValue_ptr IRBuilder::create_ashr(IRValue_ptr lhs, IRValue_ptr rhs,
                                   const std::string &name_hint) {
    return create_simple_arith(Opcode::AShr, lhs, rhs, name_hint);
}

IRValue_ptr IRBuilder::create_compare(ICmpPredicate predicate,
                                      IRValue_ptr lhs, IRValue_ptr rhs,
                                      const std::string &name_hint) {
    auto result = create_temp(std::make_shared<IntegerType>(1), name_hint);
    auto inst = std::make_shared<IRInstruction>(
        Opcode::ICmp, std::vector<IRValue_ptr>{lhs, rhs}, result);
    inst->set_predicate(predicate);
    insert_instruction(inst);
    return result;
}

IRValue_ptr IRBuilder::create_icmp_eq(IRValue_ptr lhs, IRValue_ptr rhs,
                                      const std::string &name_hint) {
    return create_compare(ICmpPredicate::EQ, lhs, rhs, name_hint);
}

IRValue_ptr IRBuilder::create_icmp_ne(IRValue_ptr lhs, IRValue_ptr rhs,
                                      const std::string &name_hint) {
    return create_compare(ICmpPredicate::NE, lhs, rhs, name_hint);
}

IRValue_ptr IRBuilder::create_icmp_slt(IRValue_ptr lhs, IRValue_ptr rhs,
                                       const std::string &name_hint) {
    return create_compare(ICmpPredicate::SLT, lhs, rhs, name_hint);
}

IRValue_ptr IRBuilder::create_icmp_sle(IRValue_ptr lhs, IRValue_ptr rhs,
                                       const std::string &name_hint) {
    return create_compare(ICmpPredicate::SLE, lhs, rhs, name_hint);
}

IRValue_ptr IRBuilder::create_icmp_sgt(IRValue_ptr lhs, IRValue_ptr rhs,
                                       const std::string &name_hint) {
    return create_compare(ICmpPredicate::SGT, lhs, rhs, name_hint);
}

IRValue_ptr IRBuilder::create_icmp_sge(IRValue_ptr lhs, IRValue_ptr rhs,
                                       const std::string &name_hint) {
    return create_compare(ICmpPredicate::SGE, lhs, rhs, name_hint);
}

IRValue_ptr IRBuilder::create_icmp_ult(IRValue_ptr lhs, IRValue_ptr rhs,
                                       const std::string &name_hint) {
    return create_compare(ICmpPredicate::ULT, lhs, rhs, name_hint);
}

IRValue_ptr IRBuilder::create_icmp_ule(IRValue_ptr lhs, IRValue_ptr rhs,
                                       const std::string &name_hint) {
    return create_compare(ICmpPredicate::ULE, lhs, rhs, name_hint);
}

IRValue_ptr IRBuilder::create_icmp_ugt(IRValue_ptr lhs, IRValue_ptr rhs,
                                       const std::string &name_hint) {
    return create_compare(ICmpPredicate::UGT, lhs, rhs, name_hint);
}

IRValue_ptr IRBuilder::create_icmp_uge(IRValue_ptr lhs, IRValue_ptr rhs,
                                       const std::string &name_hint) {
    return create_compare(ICmpPredicate::UGE, lhs, rhs, name_hint);
}

IRValue_ptr IRBuilder::create_not(IRValue_ptr value,
                                  const std::string &name_hint) {
    if (!value->type()->is_integer(1)) {
        throw std::runtime_error("create_not expects an i1 operand");
    }
    auto truth =
        std::make_shared<ConstantValue>(value->type(), static_cast<int64_t>(1));
    return create_simple_arith(Opcode::Xor, value, truth, name_hint);
}

void IRBuilder::create_br(BasicBlock_ptr target) {
    auto inst = std::make_shared<IRInstruction>(Opcode::Br,
                                                std::vector<IRValue_ptr>{});
    inst->set_branch_target(std::move(target));
    insert_instruction(inst);
}

void IRBuilder::create_cond_br(IRValue_ptr condition,
                               BasicBlock_ptr true_target,
                               BasicBlock_ptr false_target) {
    auto inst = std::make_shared<IRInstruction>(
        Opcode::CondBr, std::vector<IRValue_ptr>{condition});
    inst->set_conditional_targets(std::move(true_target),
                                  std::move(false_target));
    insert_instruction(inst);
}

void IRBuilder::create_ret(IRValue_ptr value) {
    std::vector<IRValue_ptr> operands;
    if (value) {
        operands.push_back(value);
    }
    auto inst =
        std::make_shared<IRInstruction>(Opcode::Ret, std::move(operands));
    insert_instruction(inst);
}

IRValue_ptr IRBuilder::create_call(const std::string &callee,
                                   const std::vector<IRValue_ptr> &args,
                                   IRType_ptr ret_type,
                                   const std::string &name) {
    std::vector<IRValue_ptr> operands;
    IRValue_ptr result;
    if (is_void_type(ret_type)) {
        operands.push_back(make_type_literal(ret_type));
    } else {
        result = create_temp(ret_type, name);
    }
    operands.insert(operands.end(), args.begin(), args.end());
    auto inst =
        std::make_shared<IRInstruction>(Opcode::Call, std::move(operands),
                                        result);
    inst->set_call_callee(callee);
    insert_instruction(inst);
    return result;
}

GlobalValue_ptr IRBuilder::create_string_literal(const std::string &text) {
    auto &counter = string_literal_counters()[&module_];
    const std::string name = ".str." + std::to_string(counter++);
    auto i8_type = std::make_shared<IntegerType>(8);
    auto array_type =
        std::make_shared<ArrayType>(i8_type, text.size() + 1 /* null */);
    auto init_text = encode_string_literal(text);
    return module_.create_global(name, array_type, init_text, true, "private");
}

IRInstruction_ptr IRBuilder::insert_instruction(IRInstruction_ptr inst) {
    if (!insertion_block_) {
        throw std::runtime_error("Insertion block is not set");
    }
    return insertion_block_->append(std::move(inst));
}

} // namespace ir
