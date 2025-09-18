#include "ast.h"

void LiteralExpr::accept(AST_visitor &v) { v.visit(*this); }
void IdentifierExpr::accept(AST_visitor &v) { v.visit(*this); }
void BinaryExpr::accept(AST_visitor &v) { v.visit(*this); }
void UnaryExpr::accept(AST_visitor &v) { v.visit(*this); }
void CallExpr::accept(AST_visitor &v) { v.visit(*this); }
void FieldExpr::accept(AST_visitor &v) { v.visit(*this); }
void StructExpr::accept(AST_visitor &v) { v.visit(*this); }
void IndexExpr::accept(AST_visitor &v) { v.visit(*this); }
void BlockExpr::accept(AST_visitor &v) { v.visit(*this); }
void IfExpr::accept(AST_visitor &v) { v.visit(*this); }
void WhileExpr::accept(AST_visitor &v) { v.visit(*this); }
void LoopExpr::accept(AST_visitor &v) { v.visit(*this); }
void ReturnExpr::accept(AST_visitor &v) { v.visit(*this); }
void BreakExpr::accept(AST_visitor &v) { v.visit(*this); }
void ContinueExpr::accept(AST_visitor &v) { v.visit(*this); }
void CastExpr::accept(AST_visitor &v) { v.visit(*this); }

void FnItem::accept(AST_visitor &v) { v.visit(*this); }
void StructItem::accept(AST_visitor &v) { v.visit(*this); }
void EnumItem::accept(AST_visitor &v) { v.visit(*this); }
void ImplItem::accept(AST_visitor &v) { v.visit(*this); }
void ConstItem::accept(AST_visitor &v) { v.visit(*this); }

void LetStmt::accept(AST_visitor &v) { v.visit(*this); }
void ExprStmt::accept(AST_visitor &v) { v.visit(*this); }
void ItemStmt::accept(AST_visitor &v) { v.visit(*this); }

void PathType::accept(AST_visitor &v) { v.visit(*this); }
void ArrayType::accept(AST_visitor &v) { v.visit(*this); }

void IdentifierPattern::accept(AST_visitor &v) { v.visit(*this); }

// AST_Walker 递归遍历子节点

void AST_Walker::visit([[maybe_unused]] LiteralExpr &node) { return; }
void AST_Walker::visit([[maybe_unused]] IdentifierExpr &node) { return; }
// LiteralExpr 和 IdentifierExpr 没有子节点
void AST_Walker::visit(BinaryExpr &node) {
    node.left->accept(*this);
    node.right->accept(*this);
}
void AST_Walker::visit(UnaryExpr &node) {node.right->accept(*this); }
void AST_Walker::visit(CallExpr &node) {
    node.callee->accept(*this);
    for (auto &arg : node.arguments) {
        arg->accept(*this);
    }
}
void AST_Walker::visit(FieldExpr &node) { node.base->accept(*this); }
void AST_Walker::visit(StructExpr &node) {
    for (auto &[name, expr] : node.fields) {
        expr->accept(*this);
    }
}
void AST_Walker::visit(IndexExpr &node) {
    node.base->accept(*this);
    node.index->accept(*this);
}
void AST_Walker::visit(BlockExpr &node) {
    for (auto &stmt : node.statements) {
        stmt->accept(*this);
    }
    if (node.tail_statement) {
        node.tail_statement->accept(*this);
    }
}
void AST_Walker::visit(IfExpr &node) {
    node.condition->accept(*this);
    node.then_branch->accept(*this);
    if (node.else_branch) {
        node.else_branch->accept(*this);
    }
}
void AST_Walker::visit(WhileExpr &node) {
    node.condition->accept(*this);
    node.body->accept(*this);
}
void AST_Walker::visit(LoopExpr &node) { node.body->accept(*this); }
void AST_Walker::visit(ReturnExpr &node) {
    if (node.return_value) {
        node.return_value->accept(*this);
    }
}
void AST_Walker::visit(BreakExpr &node) {
    if (node.break_value) {
        node.break_value->accept(*this);
    }
}
void AST_Walker::visit(ContinueExpr &node) {
    if (node.continue_value) {
        node.continue_value->accept(*this);
    }
}
void AST_Walker::visit(CastExpr &node) {
    node.expr->accept(*this);
    node.target_type->accept(*this);
}
void AST_Walker::visit(FnItem &node) {
    for (auto &[name, type] : node.parameters) {
        type->accept(*this);
    }
    if (node.return_type) {
        node.return_type->accept(*this);
    }
    node.body->accept(*this);
}
void AST_Walker::visit(StructItem &node) {
    for (auto &[name, type] : node.fields) {
        type->accept(*this);
    }
}
void AST_Walker::visit([[maybe_unused]] EnumItem &node) { return; }
void AST_Walker::visit(ImplItem &node) {
    for (auto &item : node.methods) {
        item->accept(*this);
    }
}
void AST_Walker::visit(ConstItem &node) {
    node.const_type->accept(*this);
    node.value->accept(*this);
}
void AST_Walker::visit(LetStmt &node) {
    node.pattern->accept(*this);
    node.initializer->accept(*this);
}
void AST_Walker::visit(ExprStmt &node) { node.expr->accept(*this); }
void AST_Walker::visit(ItemStmt &node) { node.item->accept(*this); }
void AST_Walker::visit([[maybe_unused]] PathType &node) { return; }
void AST_Walker::visit(ArrayType &node) {
    node.element_type->accept(*this);
    node.size_expr->accept(*this);
}
void AST_Walker::visit([[maybe_unused]] IdentifierPattern &node) { return; }