#ifndef SEMANTIC_CHECKER_H
#define SEMANTIC_CHECKER_H


#include "ast.h"
#include "semantic_step1.h"
#include "semantic_step2.h"


struct Semantic_Checker {
    // 这里用 shared_ptr 没有问题，应该
    map<AST_Node_ptr, Scope_ptr> node_scope_map;
    map<Type_ptr, RealType_ptr> type_map;
    Scope_ptr root_scope;
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