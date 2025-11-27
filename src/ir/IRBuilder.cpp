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

bool IRType::is_void() const {
    return dynamic_cast<const VoidType *>(this) != nullptr;
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

PointerType::PointerType(IRType_ptr pointee) : pointee_(std::move(pointee)) {}
PointerType::~PointerType() = default;

IRType_ptr PointerType::pointee_type() const { return pointee_; }

std::string PointerType::to_string() const { return "ptr"; }

ArrayType::ArrayType(IRType_ptr element_type, std::size_t element_count)
    : element_type_(std::move(element_type)), element_count_(element_count) {}
ArrayType::~ArrayType() = default;

IRType_ptr ArrayType::element_type() const { return element_type_; }

std::size_t ArrayType::element_count() const { return element_count_; }

std::string ArrayType::to_string() const {
    std::ostringstream oss;
    oss << "[" << element_count_ << " x " << element_type_->to_string() << "]";
    return oss.str();
}

StructType::StructType(std::string name) : name_(std::move(name)) {}
StructType::~StructType() = default;

void StructType::set_fields(std::vector<IRType_ptr> fields) {
    fields_ = std::move(fields);
}

const std::vector<IRType_ptr> &StructType::fields() const { return fields_; }

const std::string &StructType::name() const { return name_; }

std::string StructType::to_string() const { return "%" + name_; }

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

std::string IRValue::typed_repr() const {
    return type_->to_string() + " " + repr();
}

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

GlobalValue::GlobalValue(std::string name, IRType_ptr pointee_type,
                         std::string init_text, bool is_const,
                         std::string linkage)
    : IRValue(std::make_shared<PointerType>(pointee_type)),
      name_(std::move(name)), pointee_type_(std::move(pointee_type)),
      init_text_(std::move(init_text)), is_const_(is_const),
      linkage_(std::move(linkage)) {}

const std::string &GlobalValue::name() const { return name_; }

IRType_ptr GlobalValue::pointee_type() const { return pointee_type_; }

const std::string &GlobalValue::init_text() const { return init_text_; }

bool GlobalValue::is_const() const { return is_const_; }

const std::string &GlobalValue::linkage() const { return linkage_; }

std::string GlobalValue::repr() const { return "@" + name_; }

std::string GlobalValue::typed_repr() const { return "ptr @" + name_; }

std::string GlobalValue::definition_string() const {
    std::ostringstream oss;
    oss << "@" << name_ << " = " << linkage_ << " "
        << (is_const_ ? "constant " : "global ") << pointee_type_->to_string()
        << " " << init_text_;
    return oss.str();
}

namespace {

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
                throw std::runtime_error("GEP struct index cannot be negative");
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

IRInstruction::IRInstruction(Opcode opcode, std::vector<IRValue_ptr> operands,
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

void IRInstruction::set_literal_type(IRType_ptr type) {
    literal_type_ = std::move(type);
}

IRType_ptr IRInstruction::literal_type() const { return literal_type_; }

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
    case Opcode::ZExt:
    case Opcode::SExt:
    case Opcode::Trunc: {
        if (!result_) {
            throw std::runtime_error("Cast instruction missing result");
        }
        const char *mnemonic = nullptr;
        switch (opcode_) {
        case Opcode::ZExt:
            mnemonic = "zext";
            break;
        case Opcode::SExt:
            mnemonic = "sext";
            break;
        case Opcode::Trunc:
            mnemonic = "trunc";
            break;
        default:
            mnemonic = "";
            break;
        }
        oss << result_->repr() << " = " << mnemonic << " "
            << operands_[0]->type()->to_string() << " "
            << operands_[0]->repr() << " to "
            << result_->type()->to_string();
        break;
    }
    case Opcode::ICmp:
        if (result_) {
            oss << result_->repr() << " = ";
        }
        oss << "icmp ";
        emit_binary(predicate_to_string(predicate_));
        break;
    case Opcode::Call: {
        if (result_) {
            oss << result_->repr() << " = ";
        }
        IRType_ptr ret_type = literal_type_;
        oss << "call " << ret_type->to_string() << " @" << call_callee_ << "(";
        for (std::size_t i = 0; i < operands_.size(); ++i) {
            if (i > 0) {
                oss << ", ";
            }
            oss << operands_[i]->typed_repr();
        }
        oss << ")";
        break;
    }
    case Opcode::Alloca:
        if (result_) {
            oss << result_->repr() << " = ";
        }
        if (!literal_type_) {
            throw std::runtime_error("alloca missing literal type");
        }
        oss << "alloca " << literal_type_->to_string();
        break;
    case Opcode::Load:
        if (result_) {
            oss << result_->repr() << " = ";
        } else {
            throw std::runtime_error("Error, load instruction must contain result.");
        }
        oss << "load " << result_->type()->to_string() << ", "
            << operands_[0]->typed_repr();
        break;
    case Opcode::Store:
        oss << "store " << operands_[0]->typed_repr() << ", "
            << operands_[1]->typed_repr();
        break;
    case Opcode::GEP:
        if (result_) {
            oss << result_->repr() << " = ";
        }
        if (!literal_type_) {
            throw std::runtime_error("getelementptr missing literal type");
        }
        oss << "getelementptr " << literal_type_->to_string();
        for (const auto &op : operands_) {
            oss << ", " << op->typed_repr();
        }
        break;
    case Opcode::Br:
        oss << "br label %" << branch_target_->label();
        break;
    case Opcode::CondBr:
        oss << "br " << operands_[0]->typed_repr() << ", label %"
            << true_target_->label() << ", label %" << false_target_->label();
        break;
    case Opcode::Ret:
        if (operands_.empty()) {
            oss << "ret void";
        } else {
            oss << "ret " << operands_[0]->typed_repr();
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
    auto &counter = block_name_counter_[label];
    std::string final_label = label + "." + std::to_string(counter++);
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

const std::vector<std::pair<std::string, IRType_ptr>> &
IRFunction::params() const {
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
            oss << params_[i].second->to_string() << " %" << params_[i].first;
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
      data_layout_(std::move(data_layout)) {}

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

const std::vector<std::pair<std::string, std::vector<std::string>>> &
IRModule::type_definitions() const {
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
                                                init_text, is_const, linkage);
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

IRBuilder::IRBuilder(IRModule &module) : module_(module), next_reg_index_(0) {}

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
    return current_function_->create_block(label);
}

IRValue_ptr IRBuilder::create_temp(IRType_ptr type,
                                   const std::string &name_hint) {
    if (!type) {
        throw std::runtime_error("Temporary requires a valid type");
    }
    std::string name;
    if (name_hint.empty()) {
        name = "tmp." + std::to_string(next_reg_index_++);
    } else {
        auto &counter = name_hint_counters_[name_hint];
        name = name_hint + "." + std::to_string(counter++);
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
    auto result = create_temp(std::make_shared<PointerType>(type), name_hint);
    auto inst = std::make_shared<IRInstruction>(
        Opcode::Alloca, std::vector<IRValue_ptr>{}, result);
    inst->set_literal_type(type);
    insert_instruction(inst);
    return result;
}

IRValue_ptr IRBuilder::create_load(IRValue_ptr address,
                                   const std::string &name_hint) {
    auto ptr_type = std::dynamic_pointer_cast<PointerType>(address->type());
    if (!ptr_type || !ptr_type->pointee_type()) {
        throw std::runtime_error("Unknown pointee type for load");
    }
    auto result = create_temp(ptr_type->pointee_type(), name_hint);
    auto inst = std::make_shared<IRInstruction>(
        Opcode::Load, std::vector<IRValue_ptr>{address}, result);
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
    auto pointee = deduce_gep_pointee(element_type, indices);
    if (!pointee) {
        throw std::runtime_error("Could not deduce GEP pointee type");
    } else {
        // std::cerr << "GEP deduced pointee type: " << pointee->to_string()
        //           << '\n';
    }
    auto result_type =
        std::make_shared<PointerType>(pointee ? pointee : element_type);
    auto result = create_temp(result_type, name_hint);
    std::vector<IRValue_ptr> operands;
    operands.reserve(1 + indices.size());
    operands.push_back(base_ptr);
    operands.insert(operands.end(), indices.begin(), indices.end());
    auto inst = std::make_shared<IRInstruction>(Opcode::GEP,
                                                std::move(operands), result);
    inst->set_literal_type(element_type);
    insert_instruction(inst);
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

IRValue_ptr IRBuilder::create_zext(IRValue_ptr value, IRType_ptr target_type,
                                   const std::string &name_hint) {
    return create_cast(Opcode::ZExt, value, target_type, name_hint);
}

IRValue_ptr IRBuilder::create_sext(IRValue_ptr value, IRType_ptr target_type,
                                   const std::string &name_hint) {
    return create_cast(Opcode::SExt, value, target_type, name_hint);
}

IRValue_ptr IRBuilder::create_trunc(IRValue_ptr value, IRType_ptr target_type,
                                    const std::string &name_hint) {
    return create_cast(Opcode::Trunc, value, target_type, name_hint);
}

IRValue_ptr IRBuilder::create_compare(ICmpPredicate predicate, IRValue_ptr lhs,
                                      IRValue_ptr rhs,
                                      const std::string &name_hint) {
    auto result = create_temp(std::make_shared<IntegerType>(1), name_hint);
    auto inst = std::make_shared<IRInstruction>(
        Opcode::ICmp, std::vector<IRValue_ptr>{lhs, rhs}, result);
    inst->set_predicate(predicate);
    insert_instruction(inst);
    return result;
}

IRValue_ptr IRBuilder::create_cast(Opcode opcode, IRValue_ptr value,
                                   IRType_ptr target_type,
                                   const std::string &name_hint) {
    if (!value || !target_type) {
        throw std::runtime_error("Cast requires value and target type");
    }
    auto result = create_temp(target_type, name_hint);
    auto inst = std::make_shared<IRInstruction>(
        opcode, std::vector<IRValue_ptr>{value}, result);
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
    auto inst =
        std::make_shared<IRInstruction>(Opcode::Br, std::vector<IRValue_ptr>{});
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
    std::vector<IRValue_ptr> operands(args.begin(), args.end());
    IRType_ptr call_ret_type =
        ret_type ? ret_type : std::make_shared<VoidType>();
    IRValue_ptr result;
    const bool returns_void = call_ret_type->is_void();
    if (!returns_void) {
        result = create_temp(call_ret_type, name);
    }
    auto inst = std::make_shared<IRInstruction>(Opcode::Call,
                                                std::move(operands), result);
    inst->set_call_callee(callee);
    inst->set_literal_type(call_ret_type);
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

IRValue_ptr IRBuilder::create_i32_constant(int64_t value) {
    auto i32_type = std::make_shared<IntegerType>(32);
    return std::make_shared<ConstantValue>(i32_type, value);
}

IRInstruction_ptr IRBuilder::insert_instruction(IRInstruction_ptr inst) {
    if (!insertion_block_) {
        throw std::runtime_error("Insertion block is not set");
    }
    return insertion_block_->append(std::move(inst));
}

} // namespace ir
