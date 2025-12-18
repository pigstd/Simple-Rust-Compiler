#include "ir/type_lowering.h"
#include "semantic/decl.h"
#include <algorithm>
#include <iostream>
#include <ostream>
#include <stdexcept>
#include <string>

using std::string;
using std::vector;

namespace ir {

namespace {

constexpr std::size_t kPointerSizeBytes = 4;

ReferenceType receiver_to_ref(fn_reciever_type receiver) {
    switch (receiver) {
    case fn_reciever_type::SELF:
        return ReferenceType::NO_REF;
    case fn_reciever_type::SELF_REF:
        return ReferenceType::REF;
    case fn_reciever_type::SELF_REF_MUT:
        return ReferenceType::REF_MUT;
    case fn_reciever_type::NO_RECEIVER:
        return ReferenceType::NO_REF;
    }
    return ReferenceType::NO_REF;
}

int64_t read_signed(ConstValue_ptr value) {
    switch (value->kind) {
    case ConstValueKind::I32:
        return std::dynamic_pointer_cast<I32_ConstValue>(value)->value;
    case ConstValueKind::ISIZE:
        return std::dynamic_pointer_cast<Isize_ConstValue>(value)->value;
    default:
        break;
    }
    throw std::runtime_error("ConstValue is not signed integer");
}

uint64_t read_unsigned(ConstValue_ptr value) {
    switch (value->kind) {
    case ConstValueKind::U32:
        return std::dynamic_pointer_cast<U32_ConstValue>(value)->value;
    case ConstValueKind::USIZE:
        return std::dynamic_pointer_cast<Usize_ConstValue>(value)->value;
    default:
        break;
    }
    throw std::runtime_error("ConstValue is not unsigned integer");
}

RealType_ptr strip_reference(const RealType_ptr &type) {
    if (!type) {
        throw std::runtime_error("invalid RealType");
    }
    switch (type->kind) {
    case RealTypeKind::BOOL:
        return std::make_shared<BoolRealType>(ReferenceType::NO_REF);
    case RealTypeKind::CHAR:
        return std::make_shared<CharRealType>(ReferenceType::NO_REF);
    case RealTypeKind::UNIT:
        return std::make_shared<UnitRealType>(ReferenceType::NO_REF);
    case RealTypeKind::NEVER:
        return std::make_shared<NeverRealType>(ReferenceType::NO_REF);
    case RealTypeKind::I32:
        return std::make_shared<I32RealType>(ReferenceType::NO_REF);
    case RealTypeKind::ISIZE:
        return std::make_shared<IsizeRealType>(ReferenceType::NO_REF);
    case RealTypeKind::U32:
        return std::make_shared<U32RealType>(ReferenceType::NO_REF);
    case RealTypeKind::USIZE:
        return std::make_shared<UsizeRealType>(ReferenceType::NO_REF);
    case RealTypeKind::ANYINT:
        return std::make_shared<AnyIntRealType>(ReferenceType::NO_REF);
    case RealTypeKind::STR:
        return std::make_shared<StrRealType>(ReferenceType::NO_REF);
    case RealTypeKind::STRING:
        return std::make_shared<StringRealType>(ReferenceType::NO_REF);
    default:
        break;
    }
    switch (type->kind) {
    case RealTypeKind::ARRAY: {
        auto arr = std::dynamic_pointer_cast<ArrayRealType>(type);
        return std::make_shared<ArrayRealType>(arr->element_type, arr->size_expr,
                                               ReferenceType::NO_REF, arr->size);
    }
    case RealTypeKind::STRUCT: {
        auto struct_type = std::dynamic_pointer_cast<StructRealType>(type);
        return std::make_shared<StructRealType>(
            struct_type->name, ReferenceType::NO_REF, struct_type->decl.lock());
    }
    case RealTypeKind::ENUM: {
        auto enum_type = std::dynamic_pointer_cast<EnumRealType>(type);
        return std::make_shared<EnumRealType>(
            enum_type->name, ReferenceType::NO_REF, enum_type->decl.lock());
    }
    case RealTypeKind::FUNCTION: {
        auto fn_type = std::dynamic_pointer_cast<FunctionRealType>(type);
        return std::make_shared<FunctionRealType>(fn_type->decl.lock(),
                                                  ReferenceType::NO_REF);
    }
    default:
        break;
    }
    throw std::runtime_error("strip_reference: unsupported RealType");
}

} // namespace

TypeLowering::TypeLowering(IRModule &module)
    : module_(module), void_type_(std::make_shared<VoidType>()),
      i1_type_(std::make_shared<IntegerType>(1)),
      i8_type_(std::make_shared<IntegerType>(8)),
      i32_type_(std::make_shared<IntegerType>(32)) {}

IRType_ptr TypeLowering::lower(RealType_ptr type) {
    if (!type) {
        throw std::runtime_error("invalid RealType");
    }
    if (type->is_ref != ReferenceType::NO_REF) {
        auto base = strip_reference(type);
        auto pointee = lower(base);
        return std::make_shared<PointerType>(pointee);
    }
    switch (type->kind) {
    case RealTypeKind::BOOL:
        return i1_type_;
    case RealTypeKind::CHAR:
        return i8_type_;
    case RealTypeKind::UNIT:
    case RealTypeKind::NEVER:
        return void_type_;
    case RealTypeKind::I32:
    case RealTypeKind::ISIZE:
    case RealTypeKind::U32:
    case RealTypeKind::USIZE:
    case RealTypeKind::ANYINT:
        return i32_type_;
    case RealTypeKind::ARRAY: {
        auto array_type = std::dynamic_pointer_cast<ArrayRealType>(type);
        if (!array_type) {
            throw std::runtime_error("invalid ArrayRealType");
        }
        if (array_type->size == 0) {
            throw std::runtime_error("array size missing");
        }
        auto element_ir = lower(array_type->element_type);
        return std::make_shared<ArrayType>(element_ir, array_type->size);
    }
    case RealTypeKind::STRUCT: {
        auto struct_type = std::dynamic_pointer_cast<StructRealType>(type);
        auto decl = struct_type ? struct_type->decl.lock() : nullptr;
        if (!decl) {
            throw std::runtime_error("StructDecl missing");
        }
        auto name = struct_type->decl.lock()->name;
        // std::cerr << "struct name = " << name << '\n';
        auto cached = struct_cache_.find(name);
        if (cached == struct_cache_.end()) {
            throw std::runtime_error("struct not declared");
        }
        return cached->second;
    }
    case RealTypeKind::ENUM:
        return i32_type_;
    case RealTypeKind::STRING: {
        auto it = struct_cache_.find("String");
        if (it == struct_cache_.end()) {
            throw std::runtime_error("String type not declared");
        }
        return it->second;
    }
    case RealTypeKind::STR: {
        auto it = struct_cache_.find("Str");
        if (it == struct_cache_.end()) {
            throw std::runtime_error("Str type not declared");
        }
        return it->second;
    }
    case RealTypeKind::FUNCTION: {
        auto fn_type = std::dynamic_pointer_cast<FunctionRealType>(type);
        auto decl = fn_type ? fn_type->decl.lock() : nullptr;
        if (!decl) {
            throw std::runtime_error("FnDecl missing");
        }
        return lower_function(decl);
    }
    default:
        break;
    }
    throw std::runtime_error("unsupported RealType");
}

std::shared_ptr<FunctionType> TypeLowering::lower_function(FnDecl_ptr decl) {
    if (!decl) {
        throw std::runtime_error("FnDecl missing");
    }
    vector<IRType_ptr> params;
    if (decl->receiver_type != fn_reciever_type::NO_RECEIVER) {
        auto self_decl = decl->self_struct.lock();
        RealType_ptr self_real;
        if (!self_decl) {
            if (!decl->is_builtin) {
                throw std::runtime_error("method missing self struct");
            }

            // builtin 函数的 self 可能不是 struct，但是已经被记录在 decl 里面
            std::cerr << "warning: builtin method " << decl->name
                << " has no self struct decl" << std::endl;
            self_real = decl->builtin_method_self_type;
        } else {
            std::string self_name = decl->self_struct.lock()->name;
            ReferenceType ref = receiver_to_ref(decl->receiver_type);
            self_real = std::make_shared<StructRealType>(self_name, ref, self_decl);
        }
        params.push_back(lower(self_real));
    }
    for (const auto &param : decl->parameters) {
        params.push_back(lower(param.second));
    }
    IRType_ptr ret_type =
        decl->return_type ? lower(decl->return_type) : void_type_;
    // 如果是 main 函数要特判成 i32 返回类型
    if (decl->is_main) {
        ret_type = i32_type_;
    }
    return std::make_shared<FunctionType>(ret_type, params);
}

std::shared_ptr<ConstantValue>
TypeLowering::lower_const(ConstValue_ptr value, RealType_ptr expected_type) {
    if (!value || !expected_type) {
        throw std::runtime_error("const lowering requires value and type");
    }
    if (expected_type->is_ref != ReferenceType::NO_REF) {
        return nullptr;
    }
    if (value->kind == ConstValueKind::ANYINT) {
        throw std::runtime_error("ConstValue ANYINT not concretized");
    }
    if (value->kind == ConstValueKind::ARRAY ||
        value->kind == ConstValueKind::UNIT) {
        return nullptr;
    }
    switch (expected_type->kind) {
    case RealTypeKind::BOOL: {
        if (value->kind != ConstValueKind::BOOL) {
            throw std::runtime_error("const value/type mismatch (bool)");
        }
        auto bool_value = std::dynamic_pointer_cast<Bool_ConstValue>(value);
        return std::make_shared<ConstantValue>(i1_type_,
                                               bool_value->value ? 1 : 0);
    }
    case RealTypeKind::CHAR: {
        if (value->kind != ConstValueKind::CHAR) {
            throw std::runtime_error("const value/type mismatch (char)");
        }
        auto char_value = std::dynamic_pointer_cast<Char_ConstValue>(value);
        return std::make_shared<ConstantValue>(i8_type_, char_value->value);
    }
    case RealTypeKind::I32:
    case RealTypeKind::ISIZE:
    case RealTypeKind::ANYINT: {
        int64_t literal = read_signed(value);
        return std::make_shared<ConstantValue>(i32_type_, literal);
    }
    case RealTypeKind::U32:
    case RealTypeKind::USIZE: {
        uint64_t literal = read_unsigned(value);
        return std::make_shared<ConstantValue>(i32_type_,
                                               static_cast<int64_t>(literal));
    }
    default:
        break;
    }
    return nullptr;
}

std::shared_ptr<StructType>
TypeLowering::declare_struct_stub(StructDecl_ptr decl) {
    if (!decl) {
        throw std::runtime_error("StructDecl missing");
    }
    if (decl->name.empty()) {
        throw std::runtime_error("StructDecl missing name");
    }
    auto name = decl->name;
    auto cached = struct_cache_.find(name);
    if (cached != struct_cache_.end()) {
        return cached->second;
    }
    auto struct_type = std::make_shared<StructType>(name);
    struct_cache_[name] = struct_type;
    pending_struct_defs_.insert(name);
    return struct_type;
}

void TypeLowering::define_struct_fields(StructDecl_ptr decl) {
    if (!decl) {
        throw std::runtime_error("StructDecl missing");
    }
    if (decl->name.empty()) {
        throw std::runtime_error("StructDecl missing name");
    }
    auto name = decl->name;
    auto cached = struct_cache_.find(name);
    if (cached == struct_cache_.end()) {
        throw std::runtime_error("struct stub missing for definition");
    }
    if (!pending_struct_defs_.count(name)) {
        throw std::runtime_error("struct fields already defined: " + name);
    }
    vector<IRType_ptr> field_types;
    vector<string> field_texts;
    const auto &order = decl->field_order;
    field_types.reserve(order.size());
    field_texts.reserve(order.size());
    for (const auto &field_name : order) {
        auto iter = decl->fields.find(field_name);
        if (iter == decl->fields.end()) {
            throw std::runtime_error("struct field type missing");
        }
        auto field_ir = lower(iter->second);
        field_types.push_back(field_ir);
        field_texts.push_back(field_ir->to_string());
    }
    cached->second->set_fields(field_types);
    pending_struct_defs_.erase(name);
    module_.add_type_definition(name, field_texts);
}

void TypeLowering::declare_builtin_string_types() {
    if (!struct_cache_.count("Str")) {
        auto str_struct = std::make_shared<StructType>("Str");
        auto byte_ptr = std::make_shared<PointerType>(i8_type_);
        str_struct->set_fields({byte_ptr, i32_type_});
        struct_cache_["Str"] = str_struct;
    module_.add_type_definition(
        "Str", {byte_ptr->to_string(), i32_type_->to_string()});
    }
    if (!struct_cache_.count("String")) {
        auto string_struct = std::make_shared<StructType>("String");
        auto byte_ptr = std::make_shared<PointerType>(i8_type_);
        string_struct->set_fields({byte_ptr, i32_type_, i32_type_});
        struct_cache_["String"] = string_struct;
        module_.add_type_definition("String",
                                    {byte_ptr->to_string(), i32_type_->to_string(),
                                     i32_type_->to_string()});
    }
}

std::size_t TypeLowering::size_in_bytes(RealType_ptr type) {
    if (!type) {
        throw std::runtime_error("size_in_bytes requires valid type");
    }
    if (type->is_ref != ReferenceType::NO_REF) {
        return kPointerSizeBytes;
    }
    switch (type->kind) {
    case RealTypeKind::BOOL:
    case RealTypeKind::CHAR:
        return 1;
    case RealTypeKind::UNIT:
    case RealTypeKind::NEVER:
        return 0;
    case RealTypeKind::I32:
    case RealTypeKind::ISIZE:
    case RealTypeKind::U32:
    case RealTypeKind::USIZE:
    case RealTypeKind::ANYINT:
    case RealTypeKind::ENUM:
        return 4;
    case RealTypeKind::ARRAY: {
        auto array_type = std::dynamic_pointer_cast<ArrayRealType>(type);
        if (!array_type) {
            throw std::runtime_error("invalid ArrayRealType for size");
        }
        if (array_type->size == 0) {
            throw std::runtime_error("array size missing");
        }
        auto elem_size = size_in_bytes(array_type->element_type);
        return elem_size * array_type->size;
    }
    case RealTypeKind::STRUCT: {
        auto struct_type = std::dynamic_pointer_cast<StructRealType>(type);
        auto decl = struct_type ? struct_type->decl.lock() : nullptr;
        if (!decl) {
            throw std::runtime_error("StructDecl missing for size");
        }
        if (pending_struct_defs_.count(decl->name)) {
            throw std::runtime_error("struct fields not defined: " +
                                     decl->name);
        }
        return size_of_struct(decl->name, decl);
    }
    case RealTypeKind::STR:
        return size_of_builtin_struct("Str");
    case RealTypeKind::STRING:
        return size_of_builtin_struct("String");
    default:
        break;
    }
    throw std::runtime_error("unsupported RealType for size_in_bytes");
}

std::size_t TypeLowering::align_to(std::size_t offset,
                                   std::size_t alignment) const {
    if (alignment == 0) {
        return offset;
    }
    std::size_t remainder = offset % alignment;
    if (remainder == 0) {
        return offset;
    }
    return offset + (alignment - remainder);
}

std::size_t TypeLowering::alignment_of(RealType_ptr type) {
    if (!type) {
        return 1;
    }
    if (type->is_ref != ReferenceType::NO_REF) {
        return kPointerSizeBytes;
    }
    switch (type->kind) {
    case RealTypeKind::BOOL:
    case RealTypeKind::CHAR:
        return 1;
    case RealTypeKind::UNIT:
    case RealTypeKind::NEVER:
        return 1;
    case RealTypeKind::I32:
    case RealTypeKind::ISIZE:
    case RealTypeKind::U32:
    case RealTypeKind::USIZE:
    case RealTypeKind::ANYINT:
    case RealTypeKind::ENUM:
        return 4;
    case RealTypeKind::ARRAY: {
        auto array_type = std::dynamic_pointer_cast<ArrayRealType>(type);
        return alignment_of(array_type->element_type);
    }
    case RealTypeKind::STRUCT: {
        auto struct_type = std::dynamic_pointer_cast<StructRealType>(type);
        auto decl = struct_type ? struct_type->decl.lock() : nullptr;
        if (!decl) {
            throw std::runtime_error("StructDecl missing for alignment");
        }
        return alignment_of_struct(decl->name, decl);
    }
    case RealTypeKind::STR:
        return alignment_of_builtin_struct("Str");
    case RealTypeKind::STRING:
        return alignment_of_builtin_struct("String");
    default:
        break;
    }
    return 1;
}

std::size_t TypeLowering::size_of_struct(const std::string &name,
                                         StructDecl_ptr decl) {
    auto cached = struct_size_cache_.find(name);
    if (cached != struct_size_cache_.end()) {
        return cached->second;
    }
    if (struct_size_in_progress_.count(name)) {
        throw std::runtime_error("cyclic struct size dependency: " + name);
    }
    struct_size_in_progress_.insert(name);
    std::size_t total = 0;
    std::size_t max_align = 1;
    if (!decl) {
        total = size_of_builtin_struct(name);
        struct_size_in_progress_.erase(name);
        return total;
    }
    for (const auto &field_name : decl->field_order) {
        auto it = decl->fields.find(field_name);
        if (it == decl->fields.end()) {
            throw std::runtime_error("struct field missing type: " +
                                     field_name);
        }
        auto field_size = size_in_bytes(it->second);
        auto field_align = alignment_of(it->second);
        max_align = std::max(max_align, field_align);
        total = align_to(total, field_align);
        total += field_size;
    }
    total = align_to(total, max_align);
    struct_size_in_progress_.erase(name);
    struct_size_cache_[name] = total;
    struct_align_cache_[name] = max_align;
    return total;
}

std::size_t TypeLowering::size_of_builtin_struct(const std::string &name) {
    auto cached = struct_size_cache_.find(name);
    if (cached != struct_size_cache_.end()) {
        return cached->second;
    }
    auto type_it = struct_cache_.find(name);
    if (type_it == struct_cache_.end()) {
        throw std::runtime_error("builtin struct type missing: " + name);
    }
    std::size_t total = 0;
    std::size_t max_align = 1;
    for (const auto &field : type_it->second->fields()) {
        auto field_size = size_of_ir_type(field);
        auto field_align = alignment_of_ir_type(field);
        max_align = std::max(max_align, field_align);
        total = align_to(total, field_align);
        total += field_size;
    }
    total = align_to(total, max_align);
    struct_size_cache_[name] = total;
    struct_align_cache_[name] = max_align;
    return total;
}

std::size_t TypeLowering::alignment_of_struct(const std::string &name,
                                              StructDecl_ptr decl) {
    auto align_it = struct_align_cache_.find(name);
    if (align_it != struct_align_cache_.end()) {
        return align_it->second;
    }
    // ensure size computation also stores alignment
    size_of_struct(name, decl);
    auto cached = struct_align_cache_.find(name);
    if (cached == struct_align_cache_.end()) {
        throw std::runtime_error("struct alignment cache missing for " + name);
    }
    return cached->second;
}

std::size_t TypeLowering::alignment_of_builtin_struct(const std::string &name) {
    auto align_it = struct_align_cache_.find(name);
    if (align_it != struct_align_cache_.end()) {
        return align_it->second;
    }
    size_of_builtin_struct(name);
    auto cached = struct_align_cache_.find(name);
    if (cached == struct_align_cache_.end()) {
        throw std::runtime_error("builtin struct alignment missing: " + name);
    }
    return cached->second;
}

std::size_t TypeLowering::size_of_ir_type(const IRType_ptr &type) {
    if (!type) {
        throw std::runtime_error("invalid IRType when computing size");
    }
    if (std::dynamic_pointer_cast<PointerType>(type)) {
        return kPointerSizeBytes;
    }
    if (auto int_type = std::dynamic_pointer_cast<IntegerType>(type)) {
        auto bits = int_type->bit_width();
        return static_cast<std::size_t>((bits + 7) / 8);
    }
    if (auto array_type = std::dynamic_pointer_cast<ArrayType>(type)) {
        auto elem_size = size_of_ir_type(array_type->element_type());
        return elem_size * array_type->element_count();
    }
    if (auto struct_type = std::dynamic_pointer_cast<StructType>(type)) {
        return size_of_builtin_struct(struct_type->name());
    }
    if (std::dynamic_pointer_cast<VoidType>(type)) {
        return 0;
    }
    throw std::runtime_error("unsupported IRType for size computation");
}

std::size_t TypeLowering::alignment_of_ir_type(const IRType_ptr &type) {
    if (!type) {
        return 1;
    }
    if (std::dynamic_pointer_cast<PointerType>(type)) {
        return kPointerSizeBytes;
    }
    if (auto int_type = std::dynamic_pointer_cast<IntegerType>(type)) {
        auto bits = int_type->bit_width();
        if (bits <= 8) {
            return 1;
        }
        if (bits <= 32) {
            return 4;
        }
        return kPointerSizeBytes;
    }
    if (auto array_type = std::dynamic_pointer_cast<ArrayType>(type)) {
        return alignment_of_ir_type(array_type->element_type());
    }
    if (auto struct_type = std::dynamic_pointer_cast<StructType>(type)) {
        return alignment_of_builtin_struct(struct_type->name());
    }
    if (std::dynamic_pointer_cast<VoidType>(type)) {
        return 1;
    }
    throw std::runtime_error("unsupported IRType for alignment computation");
}

} // namespace ir
