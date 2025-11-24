#include "semantic/semantic_checker.h"
// #include "ast/visitor.h"
#include <cstddef>
// #include <iostream>
#include <memory>
#include <vector>

Semantic_Checker::Semantic_Checker(std::vector<Item_ptr> &items_) :
    root_scope(std::make_shared<Scope>(nullptr, ScopeKind::Root)), items(items_) {}

void Semantic_Checker::checker() {
    step1_build_scopes_and_collect_symbols();

    step2_resolve_types_and_check();

    add_builtin_methods_and_associated_funcs();

    step3_constant_evaluation_and_control_flow_analysis();
    
    step4_expr_type_and_let_stmt_analysis();
}

void Semantic_Checker::step1_build_scopes_and_collect_symbols() {
    // show ast
    // AST_Printer ast_printer;
    // for (auto &item : items) {
    //     item->accept(ast_printer);
    // }
    // 建作用域树
    ScopeBuilder_Visitor visitor(root_scope, node_scope_map);
    for (auto &item : items) {
        item->accept(visitor);
    }
}

void Semantic_Checker::step2_resolve_types_and_check() {
    Scope_dfs_and_build_type(root_scope);
}

void Semantic_Checker::Scope_dfs_and_build_type(Scope_ptr scope) {
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
            if (fn_decl->ast_node) {
                fn_item_to_decl_map[fn_decl->ast_node->NodeId] = fn_decl;
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
        Scope_dfs_and_build_type(child_scope);
    }
}


void Semantic_Checker::step3_constant_evaluation_and_control_flow_analysis() {
    OtherTypeAndRepeatArrayVisitor let_stmt_visitor(
        node_scope_map,
        type_map,
        const_expr_queue
    );
    for (auto &item : items) {
        item->accept(let_stmt_visitor);
    }
    // std::cerr << "LetStmtAndRepeatArrayVisitor finish\n";
    // 先求所有的 const item
    ConstItemVisitor const_item_visitor(
        false,
        node_scope_map,
        const_value_map,
        type_map,
        const_expr_to_size_map
    );
    for (auto &item : items) {
        item->accept(const_item_visitor);
    }
    // 然后对于 queue 中的 const 去求值，如果已经求了就不用管了
    for (auto expr : const_expr_queue) {
        if (const_expr_to_size_map.find(expr->NodeId) == const_expr_to_size_map.end()) {
            ConstItemVisitor const_expr_visitor(
                true,
                node_scope_map,
                const_value_map,
                type_map,
                const_expr_to_size_map
            );
            expr->accept(const_expr_visitor);
            auto value = const_expr_visitor.const_value;
            size_t size = const_expr_visitor.calc_const_array_size(value);
            const_expr_to_size_map[expr->NodeId] = size;
        }
    }
    // 然后进行控制流检查
    ControlFlowVisitor control_flow_visitor(node_outcome_state_map);
    for (auto &item : items) {
        item->accept(control_flow_visitor);
    }
}

void Semantic_Checker::step4_expr_type_and_let_stmt_analysis() {
    // ArrayTypeVisitor 先跑一遍，补全 ArrayType 的 size
    ArrayTypeVisitor array_type_visitor(type_map, const_expr_to_size_map);
    for (auto &item : items) {
        item->accept(array_type_visitor);
    }
    // std::cerr << "ArrayTypeVisitor finish\n";
    // ExprTypeAndLetStmtVisitor 再跑一遍，求出每个表达式的 RealType 和 PlaceKind
    ExprTypeAndLetStmtVisitor expr_type_visitor(
        false,
        node_type_and_place_kind_map,
        identifier_expr_to_decl_map,
        let_stmt_to_decl_map,
        node_scope_map,
        type_map,
        scope_local_variable_map,
        const_expr_to_size_map,
        node_outcome_state_map,
        call_expr_to_decl_map,
        builtin_method_funcs,
        builtin_associated_funcs
    );
    for (auto &item : items) {
        item->accept(expr_type_visitor);
    }
    
}

void Semantic_Checker::add_builtin_methods_and_associated_funcs() {
    // fn print(s: &str) -> ()
    {
        auto print_fn_decl = std::make_shared<FnDecl>(
            nullptr,
            nullptr,
            fn_reciever_type::NO_RECEIVER,
            "print"
        );
        IdentifierPattern_ptr id_pattern = std::make_shared<IdentifierPattern>("s", Mutibility::IMMUTABLE, ReferenceType::NO_REF);
        print_fn_decl->parameters.push_back({id_pattern, std::make_shared<StrRealType>(ReferenceType::REF)});
        print_fn_decl->return_type = std::make_shared<UnitRealType>(ReferenceType::NO_REF);
        string fn_name = "print";
        if (root_scope->value_namespace.find(fn_name) != root_scope->value_namespace.end()) {
            throw string("CE, builtin function name conflict: ") + fn_name;
        }
        root_scope->value_namespace[fn_name] = print_fn_decl;
    }
    // fn println(s: &str) -> ()
    {
        auto println_fn_decl = std::make_shared<FnDecl>(
            nullptr,
            nullptr,
            fn_reciever_type::NO_RECEIVER,
            "println"
        );
        IdentifierPattern_ptr id_pattern = std::make_shared<IdentifierPattern>("s", Mutibility::IMMUTABLE, ReferenceType::NO_REF);
        println_fn_decl->parameters.push_back({id_pattern, std::make_shared<StrRealType>(ReferenceType::REF)});
        println_fn_decl->return_type = std::make_shared<UnitRealType>(ReferenceType::NO_REF);
        string fn_name = "println";
        if (root_scope->value_namespace.find(fn_name) != root_scope->value_namespace.end()) {
            throw string("CE, builtin function name conflict: ") + fn_name;
        }
        root_scope->value_namespace[fn_name] = println_fn_decl;
    }
    // fn printInt(n: i32) -> ()
    {
        auto printint_fn_decl = std::make_shared<FnDecl>(
            nullptr,
            nullptr,
            fn_reciever_type::NO_RECEIVER,
            "printInt"
        );
        IdentifierPattern_ptr id_pattern = std::make_shared<IdentifierPattern>("n", Mutibility::IMMUTABLE, ReferenceType::NO_REF);
        printint_fn_decl->parameters.push_back({id_pattern, std::make_shared<I32RealType>(ReferenceType::NO_REF)});
        printint_fn_decl->return_type = std::make_shared<UnitRealType>(ReferenceType::NO_REF);
        string fn_name = "printInt";
        if (root_scope->value_namespace.find(fn_name) != root_scope->value_namespace.end()) {
            throw string("CE, builtin function name conflict: ") + fn_name;
        }
        root_scope->value_namespace[fn_name] = printint_fn_decl;
    }
    // fn printlnInt(n: i32) -> ()
    {
        auto printlnint_fn_decl = std::make_shared<FnDecl>(
            nullptr,
            nullptr,
            fn_reciever_type::NO_RECEIVER,
            "printlnInt"
        );
        IdentifierPattern_ptr id_pattern = std::make_shared<IdentifierPattern>("n", Mutibility::IMMUTABLE, ReferenceType::NO_REF);
        printlnint_fn_decl->parameters.push_back({id_pattern, std::make_shared<I32RealType>(ReferenceType::NO_REF)});
        printlnint_fn_decl->return_type = std::make_shared<UnitRealType>(ReferenceType::NO_REF);
        string fn_name = "printlnInt";
        if (root_scope->value_namespace.find(fn_name) != root_scope->value_namespace.end()) {
            throw string("CE, builtin function name conflict: ") + fn_name;
        }
        root_scope->value_namespace[fn_name] = printlnint_fn_decl;
    }
    // fn getString() -> String
    {
        auto getstring_fn_decl = std::make_shared<FnDecl>(
            nullptr,
            nullptr,
            fn_reciever_type::NO_RECEIVER,
            "getString"
        );
        getstring_fn_decl->return_type = std::make_shared<StringRealType>(ReferenceType::NO_REF);
        string fn_name = "getString";
        if (root_scope->value_namespace.find(fn_name) != root_scope->value_namespace.end()) {
            throw string("CE, builtin function name conflict: ") + fn_name;
        }
        root_scope->value_namespace[fn_name] = getstring_fn_decl;
    }
    // fn getInt() -> i32
    {
        auto getint_fn_decl = std::make_shared<FnDecl>(
            nullptr,
            nullptr,
            fn_reciever_type::NO_RECEIVER,
            "getInt"
        );
        getint_fn_decl->return_type = std::make_shared<I32RealType>(ReferenceType::NO_REF);
        string fn_name = "getInt";
        if (root_scope->value_namespace.find(fn_name) != root_scope->value_namespace.end()) {
            throw string("CE, builtin function name conflict: ") + fn_name;
        }
        root_scope->value_namespace[fn_name] = getint_fn_decl;
    }
    // fn exit(code: i32) -> ()
    // exit 有特殊行为，标记为 exit 函数
    {
        auto exit_fn_decl = std::make_shared<FnDecl>(
            nullptr,
            nullptr,
            fn_reciever_type::NO_RECEIVER,
            "exit"
        );
        exit_fn_decl->is_exit = true;
        IdentifierPattern_ptr id_pattern = std::make_shared<IdentifierPattern>("code", Mutibility::IMMUTABLE, ReferenceType::NO_REF);
        exit_fn_decl->parameters.push_back({id_pattern, std::make_shared<I32RealType>(ReferenceType::NO_REF)});
        exit_fn_decl->return_type = std::make_shared<UnitRealType>(ReferenceType::NO_REF);
        string fn_name = "exit";
        if (root_scope->value_namespace.find(fn_name) != root_scope->value_namespace.end()) {
            throw string("CE, builtin function name conflict: ") + fn_name;
        }
        root_scope->value_namespace[fn_name] = exit_fn_decl;
    }
    /*
    fn to_string(&self) -> String
    Available on: u32, usize
    Anyint can be converted u32 or usize, So Anyint has to_string method too.
    */
    {
        auto to_string_fn_decl = std::make_shared<FnDecl>(
            nullptr,
            nullptr,
            fn_reciever_type::SELF_REF,
            "to_string"
        );
        to_string_fn_decl->return_type = std::make_shared<StringRealType>(ReferenceType::NO_REF);
        builtin_method_funcs.push_back({RealTypeKind::U32, "to_string", to_string_fn_decl});
        builtin_method_funcs.push_back({RealTypeKind::USIZE, "to_string", to_string_fn_decl});
        builtin_method_funcs.push_back({RealTypeKind::ANYINT, "to_string", to_string_fn_decl});
    }
    /*    
    as_str and as_mut_str
    impl String {
        fn as_str(&self) -> &str
        fn as_mut_str(&mut self) -> &mut str
    }
    Available on: String
    */
    {
        auto as_str_fn_decl = std::make_shared<FnDecl>(
            nullptr,
            nullptr,
            fn_reciever_type::SELF_REF,
            "as_str"
        );
        as_str_fn_decl->return_type = std::make_shared<StrRealType>(ReferenceType::REF);
        builtin_method_funcs.push_back({RealTypeKind::STRING, "as_str", as_str_fn_decl});
        auto as_mut_str_fn_decl = std::make_shared<FnDecl>(
            nullptr,
            nullptr,
            fn_reciever_type::SELF_REF_MUT,
            "as_mut_str"
        );
        as_mut_str_fn_decl->return_type = std::make_shared<StrRealType>(ReferenceType::REF_MUT);
        builtin_method_funcs.push_back({RealTypeKind::STRING, "as_mut_str", as_mut_str_fn_decl});
    }
    /*
    len
    fn len(&self) -> usize
    Available on: [T; N], String, &str
    */
    {
        auto len_fn_decl = std::make_shared<FnDecl>(
            nullptr,
            nullptr,
            fn_reciever_type::SELF_REF,
            "len"
        );
        len_fn_decl->return_type = std::make_shared<UsizeRealType>(ReferenceType::NO_REF);
        builtin_method_funcs.push_back({RealTypeKind::ARRAY, "len", len_fn_decl});
        builtin_method_funcs.push_back({RealTypeKind::STRING, "len", len_fn_decl});
        builtin_method_funcs.push_back({RealTypeKind::STR, "len", len_fn_decl});
    }
    /*
    from
    fn from(&str) -> String
    fn from(&mut str) -> String
    */
    {
        auto from_fn_decl = std::make_shared<FnDecl>(
            nullptr,
            nullptr,
            fn_reciever_type::NO_RECEIVER,
            "from"
        );
        IdentifierPattern_ptr from_id_pattern = std::make_shared<IdentifierPattern>("s", Mutibility::IMMUTABLE, ReferenceType::NO_REF);
        from_fn_decl->parameters.push_back({from_id_pattern, std::make_shared<StrRealType>(ReferenceType::REF)});
        from_fn_decl->return_type = std::make_shared<StringRealType>(ReferenceType::NO_REF);
        builtin_associated_funcs.push_back({RealTypeKind::STRING, "from", from_fn_decl});
        auto from_mut_fn_decl = std::make_shared<FnDecl>(
            nullptr,
            nullptr,
            fn_reciever_type::NO_RECEIVER,
            "from"
        );
        IdentifierPattern_ptr from_mut_id_pattern = std::make_shared<IdentifierPattern>("s", Mutibility::IMMUTABLE, ReferenceType::NO_REF);
        from_mut_fn_decl->parameters.push_back({from_mut_id_pattern, std::make_shared<StrRealType>(ReferenceType::REF_MUT)});
        from_mut_fn_decl->return_type = std::make_shared<StringRealType>(ReferenceType::NO_REF);
        builtin_associated_funcs.push_back({RealTypeKind::STRING, "from", from_mut_fn_decl});
    }
    /*
    append
    fn append(&mut self, s: &str) -> ()
    modifies the String in place.
    */
    {
        auto append_fn_decl = std::make_shared<FnDecl>(
            nullptr,
            nullptr,
            fn_reciever_type::SELF_REF_MUT,
            "append"
        );
        IdentifierPattern_ptr append_id_pattern = std::make_shared<IdentifierPattern>("s", Mutibility::IMMUTABLE, ReferenceType::NO_REF);
        append_fn_decl->parameters.push_back({append_id_pattern, std::make_shared<StrRealType>(ReferenceType::REF)});
        append_fn_decl->return_type = std::make_shared<UnitRealType>(ReferenceType::NO_REF);
        builtin_method_funcs.push_back({RealTypeKind::STRING, "append", append_fn_decl});
    }
}
