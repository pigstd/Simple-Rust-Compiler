#ifndef SEMANTIC_CHECKER_H
#define SEMANTIC_CHECKER_H


#include "ast/ast.h"
#include "semantic/decl.h"
#include "semantic/type.h"
#include "semantic/consteval.h"
#include "semantic/typecheck.h"
#include <cstddef>
#include <tuple>

struct Semantic_Checker {
    // 这里用 shared_ptr 没有问题，应该
    Scope_ptr root_scope;

    // 记录 AST 节点对应的作用域
    // 使用 NodeId 作为 key
    map<size_t, Scope_ptr> node_scope_map;

    // 记录 AST 的 Type 对应的真正的类型 RealType
    // 使用 NodeId 作为 key
    map<size_t, RealType_ptr> type_map;

    // 每个 FnItem 对应的语义声明，key 为 FnItem 的 NodeId
    map<size_t, FnDecl_ptr> fn_item_to_decl_map;

    // 每个函数调用表达式最终绑定到的 FnDecl
    map<size_t, FnDecl_ptr> call_expr_to_decl_map;

    // 每个 IdentifierExpr 最终解析到的 ValueDecl
    map<size_t, ValueDecl_ptr> identifier_expr_to_decl_map;

    // 在第二步解析类型的时候，数组大小的表达式还没有被解析
    // 第三步的时候，let 语句的数组大小表达式以及 repeat array 的 size 表达式需要被解析
    // 这些表达式都必须是常量表达式，在求完所有的常量之后去求这个常量表达式的值
    vector<Expr_ptr> const_expr_queue;

    // 记录 Expr 对应的 size
    // 需要记录的数组大小只记录 ast 树上的节点
    // 在第三步解析完常量表达式之后，把 ast 树上的节点放在这里面
    map<size_t, size_t> const_expr_to_size_map;

    // 记录 const_decl 对应的 const_value
    map<ConstDecl_ptr, ConstValue_ptr> const_value_map;

    // 记录每个 AST 树节点的 OutComeState
    map<size_t, OutcomeState> node_outcome_state_map;

    // AST 树的所有 item 节点
    vector<Item_ptr> &items;

    // 每个表达式节点的 RealType 和 PlaceKind
    map<size_t, pair<RealType_ptr, PlaceKind>> node_type_and_place_kind_map;

    // 记录每个 Scope 的局部变量
    map<Scope_ptr, Local_Variable_map> scope_local_variable_map;

    // 内置方法 e.g. array.len()
    // (类型, 方法名, 方法的 FnDecl)
    // 在第二轮之后填充进去
    vector<std::tuple<RealTypeKind, string, FnDecl_ptr>> builtin_method_funcs;

    // 内置关联函数 e.g. String::from()
    vector<std::tuple<RealTypeKind, string, FnDecl_ptr>> builtin_associated_funcs;

    Semantic_Checker(vector<Item_ptr> &items_);
    // 总的 checker
    // 分为 4 步
    void checker();
    // step1 : 建作用域树 + 符号初收集
    void step1_build_scopes_and_collect_symbols();
    // step2:
    // 1. 解析所有的 struct 和 enum 的类型
    // 2. 解析 fn const 的类型
    // 3. 解析 impl 的 self_struct
    // let 的类型之后解析
    void step2_resolve_types_and_check();
    // step3:
    // 常量求值
    // 处理出所有 let 的 type（除了数组大小）
    // 常量表达式（数组大小，repeat array 的 size）
    // 控制流分析
    void step3_constant_evaluation_and_control_flow_analysis();
    // step4:
    // 求出所有表达式的 RealType
    // 以及是否可读，是否可写的性质
    // 引入 let 语句
    // 这样就完成了全部的 semantic check
    void step4_expr_type_and_let_stmt_analysis();
    // 添加内置函数
    void add_builtin_methods_and_associated_funcs();
    // 遍历 scope
    void Scope_dfs_and_build_type(Scope_ptr scope);
};
// 用来记录 AST 节点对应的作用域

#endif // SEMANTIC_CHECKER_H
