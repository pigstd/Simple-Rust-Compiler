#include "ast.h"
#include "semantic_step1.h"
#include "semantic_step3.h"
#include <cstddef>
#include <vector>
#include "semantic_checker.h"

Semantic_Checker::Semantic_Checker(std::vector<Item_ptr> &items_) :
    root_scope(std::make_shared<Scope>(nullptr, ScopeKind::Root)), items(items_) {}

void Semantic_Checker::step1_build_scopes_and_collect_symbols() {
    ScopeBuilder_Visitor visitor(root_scope, node_scope_map);
    for (auto &item : items) {
        item->accept(visitor);
    }
}

void Semantic_Checker::step2_resolve_types_and_check() {
    Scope_dfs_and_build_type(root_scope, type_map, const_expr_queue);
}

void Semantic_Checker::step3_constant_evaluation_and_control_flow_analysis() {
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
        if (const_expr_to_size_map.find(expr) == const_expr_to_size_map.end()) {
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
            const_expr_to_size_map[expr] = size;
        }
    }
    // 然后进行控制流检查
    ControlFlowVisitor control_flow_visitor(node_outcome_state_map);
    for (auto &item : items) {
        item->accept(control_flow_visitor);
    }
}

void Semantic_Checker::checker() {
    step1_build_scopes_and_collect_symbols();
    step2_resolve_types_and_check();
    step3_constant_evaluation_and_control_flow_analysis();
}