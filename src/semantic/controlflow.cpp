#include "semantic/controlflow.h"


OutcomeState get_outcome_state(vector<OutcomeType> states) {
    OutcomeState result_state;
    for (auto state : states) {
        size_t stateid = static_cast<size_t>(state);
        result_state[stateid] = true;
    }
    return result_state;
}

bool has_state(const OutcomeState &states, OutcomeType state) {
    size_t stateid = static_cast<size_t>(state);
    return states[stateid];
}

OutcomeState sequence_outcome_state(const OutcomeState &first, const OutcomeState &second) {
    if (has_state(first, OutcomeType::NEXT)) {
        auto result = first;
        result[static_cast<size_t>(OutcomeType::NEXT)] = false;
        result |= second;
        return result;
    } else {
        return first;
    }
}

OutcomeState ifelse_outcome_state(const OutcomeState &Condition, const OutcomeState &if_branch, const OutcomeState &else_branch) {
    if (!has_state(Condition, OutcomeType::NEXT)) {
        return Condition;
    }
    auto result = Condition;
    result[static_cast<size_t>(OutcomeType::NEXT)] = false;
    result |= if_branch | else_branch;
    return result;
}

OutcomeState while_outcome_state(const OutcomeState &Condition, const OutcomeState &body) {
    if (!has_state(Condition, OutcomeType::NEXT)) {
        return Condition;
    }    
    // Condition 一定有 NEXT，body 有可能不运行，所以还是可以有 NEXT
    auto result = Condition;
    if (has_state(body, OutcomeType::DIVERGE)) {
        result[static_cast<size_t>(OutcomeType::DIVERGE)] = true;
    }
    if (has_state(body, OutcomeType::RETURN)) {
        result[static_cast<size_t>(OutcomeType::RETURN)] = true;
    }
    return result;
}

OutcomeState loop_outcome_state(const OutcomeState &body) {
    if (has_state(body, OutcomeType::BREAK)) {
        auto result = get_outcome_state({OutcomeType::NEXT});
        if (has_state(body, OutcomeType::DIVERGE)) {
            result[static_cast<size_t>(OutcomeType::DIVERGE)] = true;
        }
        if (has_state(body, OutcomeType::RETURN)) {
            result[static_cast<size_t>(OutcomeType::RETURN)] = true;
        }
        return result;
    } else {
        // 有可能死循环或者 return 了
        auto result = get_outcome_state({OutcomeType::DIVERGE});
        if (has_state(body, OutcomeType::RETURN)) {
            result[static_cast<size_t>(OutcomeType::RETURN)] = true;
        }
        return result;
    }
}

void ControlFlowVisitor::visit(LiteralExpr &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}

void ControlFlowVisitor::visit(IdentifierExpr &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}

void ControlFlowVisitor::visit(BinaryExpr &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = sequence_outcome_state(
        node_outcome_state_map[node.left->NodeId],
        node_outcome_state_map[node.right->NodeId]
    );
}
void ControlFlowVisitor::visit(UnaryExpr &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}
void ControlFlowVisitor::visit(CallExpr &node) {
    AST_Walker::visit(node);
    OutcomeState state = get_outcome_state({OutcomeType::NEXT});
    for (auto arg : node.arguments) {
        state = sequence_outcome_state(state, node_outcome_state_map[arg->NodeId]);
    }
    node_outcome_state_map[node.NodeId] = state;
}
void ControlFlowVisitor::visit(FieldExpr &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = node_outcome_state_map[node.base->NodeId];
}
void ControlFlowVisitor::visit(StructExpr &node) {
    AST_Walker::visit(node);
    OutcomeState state = get_outcome_state({OutcomeType::NEXT});
    for (auto &[type, expr] : node.fields) {
        state = sequence_outcome_state(state, node_outcome_state_map[expr->NodeId]);
    }
    node_outcome_state_map[node.NodeId] = state;
}
void ControlFlowVisitor::visit(IndexExpr &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = sequence_outcome_state(
        node_outcome_state_map[node.base->NodeId],
        node_outcome_state_map[node.index->NodeId]
    );
}
void ControlFlowVisitor::visit(BlockExpr &node) {
    AST_Walker::visit(node);
    OutcomeState state = get_outcome_state({OutcomeType::NEXT});
    for (auto stmt : node.statements) {
        state = sequence_outcome_state(state, node_outcome_state_map[stmt->NodeId]);
    }
    if (node.tail_statement != nullptr) {
        state = sequence_outcome_state(state, node_outcome_state_map[node.tail_statement->NodeId]);
    }
    node_outcome_state_map[node.NodeId] = state;
}
void ControlFlowVisitor::visit(IfExpr &node) {
    AST_Walker::visit(node);
    OutcomeState state = ifelse_outcome_state(
        node_outcome_state_map[node.condition->NodeId],
        node_outcome_state_map[node.then_branch->NodeId],
        node.else_branch == nullptr ? get_outcome_state({OutcomeType::NEXT}) : node_outcome_state_map[node.else_branch->NodeId]
    );
    node_outcome_state_map[node.NodeId] = state;
}
void ControlFlowVisitor::visit(WhileExpr &node) {
    loop_depth++;
    AST_Walker::visit(node);
    loop_depth--;
    OutcomeState state = while_outcome_state(
        node_outcome_state_map[node.condition->NodeId],
        node_outcome_state_map[node.body->NodeId]
    );
    node_outcome_state_map[node.NodeId] = state;
}
void ControlFlowVisitor::visit(LoopExpr &node) {
    loop_depth++;
    AST_Walker::visit(node);
    loop_depth--;
    OutcomeState state = loop_outcome_state(
        node_outcome_state_map[node.body->NodeId]
    );
    node_outcome_state_map[node.NodeId] = state;
}
void ControlFlowVisitor::visit(ReturnExpr &node) {
    // 需要检查是否在函数内 (?)
    // 其实（应该）不需要检查，因为在 const 里面会报错，其他地方的表达式一定是在函数内 (?)
    // 真的遇到怪情况了再说
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::RETURN});
    // 有没有可能 return 里面嵌套了一些怪东西（嵌套 break 等等）？
    // 先不管
}
void ControlFlowVisitor::visit(BreakExpr &node) {
    if (loop_depth == 0) {
        throw string("CE, break statement not in loop");
    }
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::BREAK});
    // 同上，break 里面套了怪东西也不管
}
void ControlFlowVisitor::visit(ContinueExpr &node) {
    if (loop_depth == 0) {
        throw string("CE, continue statement not in loop");
    }
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::BREAK});
    // 同上，continue 里面套了怪东西也不管
}
void ControlFlowVisitor::visit(CastExpr &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = node_outcome_state_map[node.expr->NodeId];
}
void ControlFlowVisitor::visit(PathExpr &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}
void ControlFlowVisitor::visit(SelfExpr &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}
void ControlFlowVisitor::visit(UnitExpr &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}
void ControlFlowVisitor::visit(ArrayExpr &node) {
    AST_Walker::visit(node);
    OutcomeState state = get_outcome_state({OutcomeType::NEXT});
    for (auto expr : node.elements) {
        state = sequence_outcome_state(state, node_outcome_state_map[expr->NodeId]);
    }
    node_outcome_state_map[node.NodeId] = state;
}
void ControlFlowVisitor::visit(RepeatArrayExpr &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = node_outcome_state_map[node.element->NodeId];
}

void ControlFlowVisitor::visit(FnItem &node) {
    // 需要把当前的 loop_depth 归零
    auto prev_loop_depth = loop_depth;
    loop_depth = 0;
    AST_Walker::visit(node);
    loop_depth = prev_loop_depth;
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}
void ControlFlowVisitor::visit(StructItem &node) { 
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}
void ControlFlowVisitor::visit(EnumItem &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}
void ControlFlowVisitor::visit(ImplItem &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}
void ControlFlowVisitor::visit(ConstItem &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}

void ControlFlowVisitor::visit(LetStmt &node) {
    AST_Walker::visit(node);
    // let 好像不能没有 initializer
    // 先这么写了，到时候再看看
    if (node.initializer == nullptr) {
        // let x: i32;
        node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
    } else {
        node_outcome_state_map[node.NodeId] = node_outcome_state_map[node.initializer->NodeId];
    }
}
void ControlFlowVisitor::visit(ExprStmt &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = node_outcome_state_map[node.expr->NodeId];
}
void ControlFlowVisitor::visit(ItemStmt &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}
void ControlFlowVisitor::visit(PathType &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}
void ControlFlowVisitor::visit(ArrayType &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}
void ControlFlowVisitor::visit(UnitType &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}
void ControlFlowVisitor::visit(SelfType &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}
void ControlFlowVisitor::visit(IdentifierPattern &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}
