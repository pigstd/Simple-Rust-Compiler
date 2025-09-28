#include "semantic_step1.h"
#include "semantic_checker.h"

Semantic_Checker::Semantic_Checker() :
    root_scope(std::make_shared<Scope>(nullptr, ScopeKind::Root)) {}

void Semantic_Checker::step1_build_scopes_and_collect_symbols(vector<Item_ptr> &items) {
    ScopeBuilder_Visitor visitor(root_scope, node_scope_map);
    for (auto &item : items) {
        item->accept(visitor);
    }
}

void Semantic_Checker::step2_resolve_types_and_check() {
    Scope_dfs_and_build_type(root_scope, type_map);
}

void Semantic_Checker::checker(vector<Item_ptr> &items) {
    step1_build_scopes_and_collect_symbols(items);
    step2_resolve_types_and_check();
}