#ifndef SEMANTIC_CHECKER_H
#define SEMANTIC_CHECKER_H


#include "ast.h"
#include "semantic_step1.h"
#include "semantic_step2.h"
#include "semantic_step3.h"
#include <cstddef>

struct Semantic_Checker {
    Scope_ptr root_scope;
    // 这里用 shared_ptr 没有问题，应该
    map<AST_Node_ptr, Scope_ptr> node_scope_map;
    // 记录 AST 节点对应的作用域
    map<Type_ptr, RealType_ptr> type_map;
    // 记录 AST 的 Type 对应的真正的类型 RealType
    vector<Expr_ptr> const_expr_queue;
    // 在第二步解析类型的时候，数组大小的表达式还没有被解析
    // 第三步的时候，let 语句的数组大小表达式以及 repeat array 的 size 表达式需要被解析
    // 这些表达式都必须是常量表达式，在求完所有的常量之后去求这个常量表达式的值
    map<Expr_ptr, size_t> const_expr_to_size_map;
    // 记录 Expr 对应的 size
    // 需要记录的数组大小只记录 ast 树上的节点
    // 在第三步解析完常量表达式之后，把 ast 树上的节点放在这里面
    map<ConstDecl_ptr, ConstValue_ptr> const_value_map;
    // 记录 const_decl 对应的 const_value
    map<AST_Node_ptr, bool> is_diverge_map;
    // 记录 AST 节点是否 diverge
    Semantic_Checker();
    // 总的 checker
    // 分为 4 步
    void checker(vector<Item_ptr> &items);
    void step1_build_scopes_and_collect_symbols(vector<Item_ptr> &items);
    void step2_resolve_types_and_check();
    // TO DO
};
// 用来记录 AST 节点对应的作用域

#endif // SEMANTIC_CHECKER_H