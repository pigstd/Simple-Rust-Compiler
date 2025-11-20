#include "ir/type_lowering.h"

using std::string;
using std::vector;

namespace ir {

namespace {

string get_struct_name(const StructDecl_ptr &decl) {
    if (!decl) {
        throw TypeLoweringError("StructDecl missing");
    }
    if (decl->ast_node) {
        return decl->ast_node->struct_name;
    }
    throw TypeLoweringError("StructDecl missing ast");
}

vector<string> build_field_order(const StructDecl_ptr &decl) {
    vector<string> order;
    if (decl->ast_node) {
        for (const auto &field : decl->ast_node->fields) {
            order.push_back(field.first);
        }
    } else {
        for (const auto &entry : decl->fields) {
            order.push_back(entry.first);
        }
    }
    return order;
}

ReferenceType receiver_to_ref(fn_reciever_type receiver) {
    switch (receiver) {
    case fn_reciever_type::SELF: return ReferenceType::NO_REF;
    case fn_reciever_type::SELF_REF: return ReferenceType::REF;
    case fn_reciever_type::SELF_REF_MUT: return ReferenceType::REF_MUT;
    case fn_reciever_type::NO_RECEIVER: return ReferenceType::NO_REF;
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
    throw TypeLoweringError("ConstValue is not signed integer");
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
    throw TypeLoweringError("ConstValue is not unsigned integer");
}

} // namespace

TypeLowering::TypeLowering(IRModule &module)
    : module_(module),
      void_type_(std::make_shared<VoidType>()),
      i1_type_(std::make_shared<IntegerType>(1)),
      i8_type_(std::make_shared<IntegerType>(8)),
      i32_type_(std::make_shared<IntegerType>(32)),
      ptr_type_(std::make_shared<PointerType>()) {}

IRType_ptr TypeLowering::lower(RealType_ptr type) {
    if (!type) {
        throw TypeLoweringError("invalid RealType");
    }
    if (type->is_ref != ReferenceType::NO_REF) {
        return ptr_type_;
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
            throw TypeLoweringError("invalid ArrayRealType");
        }
        if (array_type->size == 0) {
            throw TypeLoweringError("array size missing");
        }
        auto element_ir = lower(array_type->element_type);
        return std::make_shared<ArrayType>(element_ir, array_type->size);
    }
    case RealTypeKind::STRUCT: {
        auto struct_type = std::dynamic_pointer_cast<StructRealType>(type);
        auto decl = struct_type ? struct_type->decl.lock() : nullptr;
        if (!decl) {
            throw TypeLoweringError("StructDecl missing");
        }
        auto name = struct_type->name;
        auto cached = struct_cache_.find(name);
        if (cached == struct_cache_.end()) {
            cached = struct_cache_.emplace(name, declare_struct(decl)).first;
        }
        return cached->second;
    }
    case RealTypeKind::ENUM:
        return i32_type_;
    case RealTypeKind::STRING: {
        auto it = struct_cache_.find("String");
        if (it == struct_cache_.end()) {
            throw TypeLoweringError("String type not declared");
        }
        return it->second;
    }
    case RealTypeKind::STR: {
        auto it = struct_cache_.find("Str");
        if (it == struct_cache_.end()) {
            throw TypeLoweringError("Str type not declared");
        }
        return it->second;
    }
    case RealTypeKind::FUNCTION: {
        auto fn_type = std::dynamic_pointer_cast<FunctionRealType>(type);
        auto decl = fn_type ? fn_type->decl.lock() : nullptr;
        if (!decl) {
            throw TypeLoweringError("FnDecl missing");
        }
        return lower_function(decl);
    }
    default:
        break;
    }
    throw TypeLoweringError("unsupported RealType");
}

std::shared_ptr<FunctionType> TypeLowering::lower_function(FnDecl_ptr decl) {
    if (!decl) {
        throw TypeLoweringError("FnDecl missing");
    }
    vector<IRType_ptr> params;
    if (decl->receiver_type != fn_reciever_type::NO_RECEIVER) {
        auto self_decl = decl->self_struct.lock();
        if (!self_decl) {
            throw TypeLoweringError("method missing self struct");
        }
        auto self_name = get_struct_name(self_decl);
        ReferenceType ref = receiver_to_ref(decl->receiver_type);
        auto self_real =
            std::make_shared<StructRealType>(self_name, ref, self_decl);
        params.push_back(lower(self_real));
    }
    for (const auto &param : decl->parameters) {
        params.push_back(lower(param.second));
    }
    IRType_ptr ret_type = decl->return_type ? lower(decl->return_type) : void_type_;
    return std::make_shared<FunctionType>(ret_type, params);
}

std::shared_ptr<ConstantValue> TypeLowering::lower_const(ConstValue_ptr value,
                                                         RealType_ptr expected_type) {
    if (!value || !expected_type) {
        throw TypeLoweringError("const lowering requires value and type");
    }
    if (expected_type->is_ref != ReferenceType::NO_REF) {
        return nullptr;
    }
    if (value->kind == ConstValueKind::ANYINT) {
        throw TypeLoweringError("ConstValue ANYINT not concretized");
    }
    if (value->kind == ConstValueKind::ARRAY ||
        value->kind == ConstValueKind::UNIT) {
        return nullptr;
    }
    switch (expected_type->kind) {
    case RealTypeKind::BOOL: {
        if (value->kind != ConstValueKind::BOOL) {
            throw TypeLoweringError("const value/type mismatch (bool)");
        }
        auto bool_value = std::dynamic_pointer_cast<Bool_ConstValue>(value);
        return std::make_shared<ConstantValue>(i1_type_, bool_value->value ? 1 : 0);
    }
    case RealTypeKind::CHAR: {
        if (value->kind != ConstValueKind::CHAR) {
            throw TypeLoweringError("const value/type mismatch (char)");
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
    auto name = get_struct_name(decl);
    auto cached = struct_cache_.find(name);
    if (cached != struct_cache_.end()) {
        return cached->second;
    }
    vector<IRType_ptr> field_types;
    vector<string> field_texts;
    auto order = build_field_order(decl);
    field_types.reserve(order.size());
    field_texts.reserve(order.size());
    for (const auto &field_name : order) {
        auto iter = decl->fields.find(field_name);
        if (iter == decl->fields.end()) {
            throw TypeLoweringError("struct field type missing");
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
        str_struct->set_fields({ptr_type_, i32_type_});
        struct_cache_["Str"] = str_struct;
        module_.add_type_definition("Str",
                                    {ptr_type_->to_string(), i32_type_->to_string()});
    }
    if (!struct_cache_.count("String")) {
        auto string_struct = std::make_shared<StructType>("String");
        string_struct->set_fields({ptr_type_, i32_type_, i32_type_});
        struct_cache_["String"] = string_struct;
        module_.add_type_definition(
            "String",
            {ptr_type_->to_string(), i32_type_->to_string(), i32_type_->to_string()});
    }
}

} // namespace ir
