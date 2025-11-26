#include "ir/type_lowering.h"
#include <iostream>
#include <string>

using std::string;
using std::vector;

namespace ir {

namespace {

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
        if (!self_decl) {
            throw std::runtime_error("method missing self struct");
        }
        std::string self_name = decl->self_struct.lock()->name;
        ReferenceType ref = receiver_to_ref(decl->receiver_type);
        auto self_real =
            std::make_shared<StructRealType>(self_name, ref, self_decl);
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

std::shared_ptr<StructType> TypeLowering::declare_struct(StructDecl_ptr decl) {
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
    auto struct_type = std::make_shared<StructType>(name);
    struct_type->set_fields(field_types);
    struct_cache_[name] = struct_type;
    module_.add_type_definition(name, field_texts);
    return struct_type;
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

} // namespace ir
