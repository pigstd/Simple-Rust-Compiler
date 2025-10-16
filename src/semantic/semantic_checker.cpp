#include "ast/ast.h"
#include "semantic/semantic_step1.h"
#include "semantic/semantic_step2.h"
#include "semantic/semantic_step3.h"
// #include "ast/visitor.h"
#include <cstddef>
// #include <iostream>
#include <memory>
#include <vector>
#include "semantic/semantic_checker.h"

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
    Scope_dfs_and_build_type(root_scope, type_map, const_expr_queue);
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
        node_scope_map,
        type_map,
        scope_local_variable_map,
        const_expr_to_size_map,
        node_outcome_state_map,
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
            fn_reciever_type::NO_RECEIVER
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
            fn_reciever_type::NO_RECEIVER
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
            fn_reciever_type::NO_RECEIVER
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
            fn_reciever_type::NO_RECEIVER
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
            fn_reciever_type::NO_RECEIVER
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
            fn_reciever_type::NO_RECEIVER
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
            fn_reciever_type::NO_RECEIVER
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
            fn_reciever_type::SELF_REF
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
            fn_reciever_type::SELF_REF
        );
        as_str_fn_decl->return_type = std::make_shared<StrRealType>(ReferenceType::REF);
        builtin_method_funcs.push_back({RealTypeKind::STRING, "as_str", as_str_fn_decl});
        auto as_mut_str_fn_decl = std::make_shared<FnDecl>(
            nullptr,
            nullptr,
            fn_reciever_type::SELF_REF_MUT
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
            fn_reciever_type::SELF_REF
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
            fn_reciever_type::NO_RECEIVER
        );
        IdentifierPattern_ptr from_id_pattern = std::make_shared<IdentifierPattern>("s", Mutibility::IMMUTABLE, ReferenceType::NO_REF);
        from_fn_decl->parameters.push_back({from_id_pattern, std::make_shared<StrRealType>(ReferenceType::REF)});
        from_fn_decl->return_type = std::make_shared<StringRealType>(ReferenceType::NO_REF);
        builtin_associated_funcs.push_back({RealTypeKind::STRING, "from", from_fn_decl});
        auto from_mut_fn_decl = std::make_shared<FnDecl>(
            nullptr,
            nullptr,
            fn_reciever_type::NO_RECEIVER
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
            fn_reciever_type::SELF_REF_MUT
        );
        IdentifierPattern_ptr append_id_pattern = std::make_shared<IdentifierPattern>("s", Mutibility::IMMUTABLE, ReferenceType::NO_REF);
        append_fn_decl->parameters.push_back({append_id_pattern, std::make_shared<StrRealType>(ReferenceType::REF)});
        append_fn_decl->return_type = std::make_shared<UnitRealType>(ReferenceType::NO_REF);
        builtin_method_funcs.push_back({RealTypeKind::STRING, "append", append_fn_decl});
    }
}
