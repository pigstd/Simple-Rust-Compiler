#ifndef CONTROLFLOW_H
#define CONTROLFLOW_H

#include <bitset>
#include "ast/ast.h"
#include "ast/visitor.h"

// 控制流分析
// 1. break continue 是否在循环里？
// 2. return break continue 的控制流分析（是否 diverge）

enum class OutcomeType {
    NEXT,
    RETURN,
    BREAK,
    CONTINUE,
    DIVERGE,
};
// OutcomeState: 可能的返回的集合
using OutcomeState = std::bitset<4>;

// 返回这些 status 对应的 OutcomeState
OutcomeState get_outcome_state(vector<OutcomeType> states);

bool has_state(const OutcomeState &states, OutcomeType state);

// 顺序执行的 OutcomeState
OutcomeState sequence_outcome_state(const OutcomeState &first, const OutcomeState &second);
// if else 的 OutcomeState
OutcomeState ifelse_outcome_state(const OutcomeState &Condition, const OutcomeState &if_branch, const OutcomeState &else_branch);
// while 的 OutcomeState
OutcomeState while_outcome_state(const OutcomeState &Condition, const OutcomeState &body);
// loop 的 OutcomeState
OutcomeState loop_outcome_state(const OutcomeState &body);

struct ControlFlowVisitor : public AST_Walker {
    // 这个 visitor 用来做控制流分析
    // 主要是分析 if while loop 的分支是否都返回
    // 以及 return break continue 的 diverge 情况
    size_t loop_depth;
    map<size_t, OutcomeState> &node_outcome_state_map;
    ControlFlowVisitor(map<size_t, OutcomeState> &node_outcome_state_map_) :
        loop_depth(0), node_outcome_state_map(node_outcome_state_map_) {}
    virtual ~ControlFlowVisitor() = default;
    virtual void visit(LiteralExpr &node) override;
    virtual void visit(IdentifierExpr &node) override;
    virtual void visit(BinaryExpr &node) override;
    virtual void visit(UnaryExpr &node) override;
    virtual void visit(CallExpr &node) override;
    virtual void visit(FieldExpr &node) override;
    virtual void visit(StructExpr &node) override;
    virtual void visit(IndexExpr &node) override;
    virtual void visit(BlockExpr &node) override;
    virtual void visit(IfExpr &node) override;
    virtual void visit(WhileExpr &node) override;
    virtual void visit(LoopExpr &node) override;
    virtual void visit(ReturnExpr &node) override;
    virtual void visit(BreakExpr &node) override;
    virtual void visit(ContinueExpr &node) override;
    virtual void visit(CastExpr &node) override;
    virtual void visit(PathExpr &node) override;
    virtual void visit(SelfExpr &node) override;
    virtual void visit(UnitExpr &node) override;
    virtual void visit(ArrayExpr &node) override;
    virtual void visit(RepeatArrayExpr &node) override;
    virtual void visit(FnItem &node) override;
    virtual void visit(StructItem &node) override;
    virtual void visit(EnumItem &node) override;
    virtual void visit(ImplItem &node) override;
    virtual void visit(ConstItem &node) override;
    virtual void visit(LetStmt &node) override;
    virtual void visit(ExprStmt &node) override;
    virtual void visit(ItemStmt &node) override;
    virtual void visit(PathType &node) override;
    virtual void visit(ArrayType &node) override;
    virtual void visit(UnitType &node) override;
    virtual void visit(SelfType &node) override;
    virtual void visit(IdentifierPattern &node) override;
};

#endif // CONTROLFLOW_H