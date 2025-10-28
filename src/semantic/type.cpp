#include "ast/ast.h"
#include "semantic/decl.h"
#include "semantic/scope.h"
#include "semantic/type.h"
#include <cassert>
#include <cstddef>
// #include <iostream>
#include <memory>
#include <string>

string real_type_kind_to_string(RealTypeKind kind) {
    switch (kind) {
        case RealTypeKind::UNIT: return "UNIT";
        case RealTypeKind::NEVER: return "NEVER";
        case RealTypeKind::ARRAY: return "ARRAY";
        case RealTypeKind::STRUCT: return "STRUCT";
        case RealTypeKind::ENUM: return "ENUM";
        case RealTypeKind::BOOL: return "BOOL";
        case RealTypeKind::I32: return "I32";
        case RealTypeKind::ISIZE: return "ISIZE";
        case RealTypeKind::U32: return "U32";
        case RealTypeKind::USIZE: return "USIZE";
        case RealTypeKind::ANYINT: return "ANYINT";
        case RealTypeKind::CHAR: return "CHAR";
        case RealTypeKind::STR: return "STR";
        default: return "UNKNOWN";
    }
}

string UnitRealType::show_real_type_info() {
    return reference_type_to_string(is_ref) + " UNIT";
}
string NeverRealType::show_real_type_info() {
    return reference_type_to_string(is_ref) + " NEVER";
}
string ArrayRealType::show_real_type_info() {
    return reference_type_to_string(is_ref) + " ARRAY[" + std::to_string(size) + "] of " + element_type->show_real_type_info();
}
string StructRealType::show_real_type_info() {
    return reference_type_to_string(is_ref) + " STRUCT " + name;
}
string EnumRealType::show_real_type_info() {
    return reference_type_to_string(is_ref) + " ENUM " + name;
}
string BoolRealType::show_real_type_info() {
    return reference_type_to_string(is_ref) + " BOOL";
}
string I32RealType::show_real_type_info() {
    return reference_type_to_string(is_ref) + " I32";
}
string IsizeRealType::show_real_type_info() {
    return reference_type_to_string(is_ref) + " ISIZE";
}
string U32RealType::show_real_type_info() {
    return reference_type_to_string(is_ref) + " U32";
}
string UsizeRealType::show_real_type_info() {
    return reference_type_to_string(is_ref) + " USIZE";
}
string AnyIntRealType::show_real_type_info() {
    return reference_type_to_string(is_ref) + " ANYINT";
}
string CharRealType::show_real_type_info() {
    return reference_type_to_string(is_ref) + " CHAR";
}
string StrRealType::show_real_type_info() {
    return reference_type_to_string(is_ref) + " STR";
}
string StringRealType::show_real_type_info() {
    return reference_type_to_string(is_ref) + " STRING";
}
string FunctionRealType::show_real_type_info() {
    return reference_type_to_string(is_ref) + " FUNCTION";
}

RealType_ptr find_real_type(Scope_ptr current_scope, Type_ptr type_ast, map<size_t, RealType_ptr> &type_map, vector<Expr_ptr> &const_expr_queue) {
    if (type_map.find(type_ast->NodeId) != type_map.end()) {
        return type_map[type_ast->NodeId];
    }
    RealType_ptr result_type = nullptr;
    ReferenceType ref_type = type_ast->ref_type;
    if (auto path_type = dynamic_cast<PathType*>(type_ast.get())) {
        string name = path_type->name;
        while (current_scope != nullptr) {
            if (current_scope->type_namespace.find(name) != current_scope->type_namespace.end()) {
                TypeDecl_ptr type_decl = current_scope->type_namespace[name];
                if (type_decl->kind == TypeDeclKind::Struct) {
                    auto decl = std::dynamic_pointer_cast<StructDecl>(type_decl);
                    result_type = std::make_shared<StructRealType>(name, ref_type, decl);
                    break;
                } else if (type_decl->kind == TypeDeclKind::Enum) {
                    auto decl = std::dynamic_pointer_cast<EnumDecl>(type_decl);
                    result_type = std::make_shared<EnumRealType>(name, ref_type, decl);
                    break;
                } else {
                    throw string("CE, type name ") + name + " is not a struct or enum";
                }
            } else {
                current_scope = current_scope->parent.lock();
            }
        }
        // 内置类型
        if (result_type == nullptr) {
            if (name == "i32") {
                result_type = std::make_shared<I32RealType>(ref_type);
            } else if (name == "isize") {
                result_type = std::make_shared<IsizeRealType>(ref_type);
            } else if (name == "u32") {
                result_type = std::make_shared<U32RealType>(ref_type);
            } else if (name == "usize") {
                result_type = std::make_shared<UsizeRealType>(ref_type);
            } else if (name == "bool") {
                result_type = std::make_shared<BoolRealType>(ref_type);
            } else if (name == "char") {
                result_type = std::make_shared<CharRealType>(ref_type);
            } else if (name == "str") {
                result_type = std::make_shared<StrRealType>(ref_type);
            } else if (name == "String") {
                result_type = std::make_shared<StringRealType>(ref_type);
            } else {
                throw string("CE, type name ") + name + " not found";
            }
        }
    } else if (auto array_type = dynamic_cast<ArrayType*>(type_ast.get())) {
        RealType_ptr element_type = find_real_type(current_scope, array_type->element_type, type_map, const_expr_queue);
        auto result_array_type = std::make_shared<ArrayRealType>(element_type, array_type->size_expr, ref_type);
        result_type = result_array_type;
        // 数组大小的表达式放入 const_expr_queue
        const_expr_queue.push_back(array_type->size_expr);
    } else if (dynamic_cast<UnitType*>(type_ast.get())) {
        result_type = std::make_shared<UnitRealType>(ref_type);
    } else {
        assert(dynamic_cast<SelfType*>(type_ast.get()) != nullptr);
        // SelfType，一定在 Impl 里面
        auto impl_scope = current_scope;
        while(impl_scope != nullptr && impl_scope->kind != ScopeKind::Impl) {
            impl_scope = impl_scope->parent.lock();
        }
        if (impl_scope == nullptr) {
            throw string("CE, self type used outside of impl");
        }
        assert(impl_scope->self_struct != nullptr);
        result_type = impl_scope->self_struct;
    }
    return type_map[type_ast->NodeId] = result_type;
}

void Scope_dfs_and_build_type(Scope_ptr scope, map<size_t, RealType_ptr> &type_map, vector<Expr_ptr> &const_expr_queue) {
    // 如果是 impl，先找到 impl_struct 对应的 StructDecl
    StructDecl_ptr impl_struct_decl = nullptr;
    if (scope->kind == ScopeKind::Impl) {
        /*
        impl impl_struct {

        }
        将这里面的 fn 和 const 加入到 impl_struct 的 StructDecl 里面
        */
        // 先找到 impl_struct 对应的 StructDecl
        Scope_ptr current_scope = scope;
        while (current_scope != nullptr) {
            if (current_scope->type_namespace.find(scope->impl_struct) != current_scope->type_namespace.end()) {
                TypeDecl_ptr type_decl = current_scope->type_namespace[scope->impl_struct];
                if (type_decl->kind == TypeDeclKind::Struct) {
                    impl_struct_decl = std::dynamic_pointer_cast<StructDecl>(type_decl);
                    break;
                } else {
                    throw string("CE, impl struct name ") + scope->impl_struct + " is not a struct";
                }
            } else {
                current_scope = current_scope->parent.lock();
            }
        }
        if (impl_struct_decl == nullptr) {
            throw string("CE, impl struct name ") + scope->impl_struct + " not found";
        }
        scope->self_struct = std::make_shared<StructRealType>(scope->impl_struct, ReferenceType::NO_REF, impl_struct_decl);
    }
    
    for (auto [name, type_decl] : scope->type_namespace) {
        if (type_decl->kind == TypeDeclKind::Struct) {
            // 解析 fields
            auto struct_decl = std::dynamic_pointer_cast<StructDecl>(type_decl);
            for (auto [field_name, field_type_ast] : struct_decl->ast_node->fields) {
                if (struct_decl->fields.find(field_name) != struct_decl->fields.end()) {
                    throw string("CE, field name ") + field_name + " redefined in struct " + struct_decl->ast_node->struct_name;
                }
                RealType_ptr field_type = find_real_type(scope, field_type_ast, type_map, const_expr_queue);
                struct_decl->fields[field_name] = field_type;
            }
        }
        else {
            auto enum_decl = std::dynamic_pointer_cast<EnumDecl>(type_decl);
            int variant_value = 0;
            for (auto &variant : enum_decl->ast_node->variants) {
                if (enum_decl->variants.find(variant) != enum_decl->variants.end()) {
                    throw string("CE, variant name ") + variant + " redefined in enum " + enum_decl->ast_node->enum_name;
                }
                enum_decl->variants[variant] = variant_value;
                variant_value++;
            }
        }
    }
    for (auto [name, value_decl] : scope->value_namespace) {
        if (value_decl->kind == ValueDeclKind::Function) {
            auto fn_decl = std::dynamic_pointer_cast<FnDecl>(value_decl);
            // 解析 parameters
            for (auto [param_pattern, param_type_ast] : fn_decl->ast_node->parameters) {
                RealType_ptr param_type = find_real_type(scope, param_type_ast, type_map, const_expr_queue);
                fn_decl->parameters.push_back({param_pattern, param_type});
            }
            // 解析 return type
            if (fn_decl->ast_node->return_type != nullptr) {
                RealType_ptr return_type = find_real_type(scope, fn_decl->ast_node->return_type, type_map, const_expr_queue);
                fn_decl->return_type = return_type;
            } else {
                // 没有返回类型，默认为 ()
                fn_decl->return_type = std::make_shared<UnitRealType>(ReferenceType::NO_REF);
            }
        } else if (value_decl->kind == ValueDeclKind::Constant) {
            auto const_decl = std::dynamic_pointer_cast<ConstDecl>(value_decl);
            // 解析 const type
            RealType_ptr const_type = find_real_type(scope, const_decl->ast_node->const_type, type_map, const_expr_queue);
            const_decl->const_type = const_type;
        }
    }
    if (scope->kind == ScopeKind::Impl) {
        for (auto [name, value_decl] : scope->value_namespace) {
            if (impl_struct_decl->associated_const.find(name) != impl_struct_decl->associated_const.end() ||
                impl_struct_decl->methods.find(name) != impl_struct_decl->methods.end() ||
                impl_struct_decl->associated_func.find(name) != impl_struct_decl->associated_func.end()) {
                // 一个 struct 里面的 fn 和 const 不能重名
                throw string("CE, name ") + name + " redefined in impl for struct " + impl_struct_decl->ast_node->struct_name;
            }
            if (value_decl->kind == ValueDeclKind::Function) {
                auto fn_decl = std::dynamic_pointer_cast<FnDecl>(value_decl);
                if (fn_decl->receiver_type != fn_reciever_type::NO_RECEIVER) {
                    // 方法
                    impl_struct_decl->methods[name] = fn_decl;
                    fn_decl->self_struct = impl_struct_decl;
                } else {
                    // 关联函数
                    impl_struct_decl->associated_func[name] = fn_decl;
                    fn_decl->self_struct = impl_struct_decl;
                }
                impl_struct_decl->methods[name] = fn_decl;
            } else if (value_decl->kind == ValueDeclKind::Constant) {
                auto const_decl = std::dynamic_pointer_cast<ConstDecl>(value_decl);
                impl_struct_decl->associated_const[name] = const_decl;
            }
        }
    } else {
        // 不是 impl，如果 fn 里面 receiver_type 不是 NO_RECEIVER 就报错
        // 如果 name = main 并且 scope.kind == Root，标记为 main 函数
        bool find_main = false;
        for (auto [name, value_decl] : scope->value_namespace) {
            if (value_decl->kind == ValueDeclKind::Function) {
                auto fn_decl = std::dynamic_pointer_cast<FnDecl>(value_decl);
                if (fn_decl->receiver_type != fn_reciever_type::NO_RECEIVER) {
                    throw string("CE, function ") + name + " has receiver but not in impl";
                }
                if (name == "main" && scope->kind == ScopeKind::Root) {
                    fn_decl->is_main = true;
                    find_main = true;
                    fn_decl->function_scope.lock()->is_main_scope = true;
                }
            }
        }
        if (scope->kind == ScopeKind::Root && !find_main) {
            throw string("CE, main function not found");
        }
    }
    for (auto &child_scope : scope->children) {
        Scope_dfs_and_build_type(child_scope, type_map, const_expr_queue);
    }
}


void OtherTypeAndRepeatArrayVisitor::visit(LiteralExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(IdentifierExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(BinaryExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(UnaryExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(CallExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(FieldExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(StructExpr &node) {
    AST_Walker::visit(node);    
    // 解析 node.struct_name
    find_real_type(node_scope_map[node.struct_name->NodeId], node.struct_name, type_map, const_expr_queue);
}
void OtherTypeAndRepeatArrayVisitor::visit(IndexExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(BlockExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(IfExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(WhileExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(LoopExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(ReturnExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(BreakExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(ContinueExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(CastExpr &node) {
    // 将 as 后面的类型解析出来
    AST_Walker::visit(node);
    find_real_type(node_scope_map[node.target_type->NodeId], node.target_type, type_map, const_expr_queue);
}
void OtherTypeAndRepeatArrayVisitor::visit(PathExpr &node) {
    AST_Walker::visit(node);
    // 解析 node.base
    find_real_type(node_scope_map[node.base->NodeId], node.base, type_map, const_expr_queue);
}
void OtherTypeAndRepeatArrayVisitor::visit(SelfExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(UnitExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(ArrayExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(RepeatArrayExpr &node) {
    const_expr_queue.push_back(node.size);
    AST_Walker::visit(node);
}
void OtherTypeAndRepeatArrayVisitor::visit(FnItem &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(StructItem &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(EnumItem &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(ImplItem &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(ConstItem &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(LetStmt &node) {
    if (node.type != nullptr) {
        find_real_type(node_scope_map[node.NodeId], node.type, type_map, const_expr_queue);
        // 这里不管返回值，因为 type_map 里面已经存了
    }
    AST_Walker::visit(node);
}
void OtherTypeAndRepeatArrayVisitor::visit(ExprStmt &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(ItemStmt &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(PathType &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(ArrayType &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(UnitType &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(SelfType &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(IdentifierPattern &node) { AST_Walker::visit(node); }