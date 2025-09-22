#include "visitor.h"
#include "ast.h"
#include <iostream>
#include <ostream>

using std::cout;
using std::endl;

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
void AST_Walker::visit(PathExpr &node) { node.base->accept(*this); }
void AST_Walker::visit([[maybe_unused]] SelfExpr &node) { return; }
void AST_Walker::visit([[maybe_unused]] UnitExpr &node) { return; }
void AST_Walker::visit(ArrayExpr &node) {
    for (auto &elem : node.elements) {
        elem->accept(*this);
    }
}
void AST_Walker::visit(RepeatArrayExpr &node) {
    node.element->accept(*this);
    node.size->accept(*this);
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
void AST_Walker::visit([[maybe_unused]] UnitType &node) { return; }
void AST_Walker::visit([[maybe_unused]] IdentifierPattern &node) { return; }

// AST_Printer: 有些调用 AST_Walker 遍历会比较方便，但是有些还是要自己写遍历
void AST_Printer::visit(LiteralExpr &node) {
    string tab = string("\t", depth);
    cout << tab << "LiteralExpr, type =  " << literal_type_to_string(node.literal_type)
         << ", value = " << node.value << endl;
    depth++;
    AST_Walker::visit(node);
    depth--;
}
void AST_Printer::visit(IdentifierExpr &node) {
    string tab = string("\t", depth);
    cout << tab << "IdentifierExpr, name =  " << node.name << endl;
    depth++;
    AST_Walker::visit(node);
    depth--;
}
void AST_Printer::visit(BinaryExpr &node) {
    string tab = string("\t", depth);
    cout << tab << "BinaryExpr, op =  " << binary_operator_to_string(node.op) << endl;
    cout << tab << "Left: " << endl;
    depth++;
    node.left->accept(*this);
    cout << tab << "Right: " << endl;
    node.right->accept(*this);
    depth--;
}
void AST_Printer::visit(UnaryExpr &node) {
    string tab = string("\t", depth);
    cout << tab << "UnaryExpr, op =  " << unary_operator_to_string(node.op) << endl;
    cout << tab << "Right : " << endl;
    depth++;
    AST_Walker::visit(node);
    depth--;
}
void AST_Printer::visit(CallExpr &node) {
    string tab = string("\t", depth);
    cout << tab << "CallExpr, arguments size =  " << node.arguments.size() << endl;
    cout << tab << "Callee: " << endl;
    depth++;
    node.callee->accept(*this);
    cout << tab << "Arguments: " << endl;
    for (auto &args : node.arguments)
        args->accept(*this);
    depth--;
}
void AST_Printer::visit(FieldExpr &node) {
    string tab = string("\t", depth);
    cout << tab << "FieldExpr, field name =  " << node.field_name << endl;
    cout << tab << "Base : " << endl;
    depth++;
    AST_Walker::visit(node);
    depth--;
}
void AST_Printer::visit(StructExpr &node) {
    string tab = string("\t", depth);
    cout << tab << "StructExpr, struct name =  " << node.struct_name << endl;
    cout << tab << "Fields : " << endl;
    depth++;
    for (auto &[name, value] : node.fields) {
        cout << tab << "name = " << name << ", value : " << endl;
        value->accept(*this);
    }
    depth--;
}

// NOT FINISH
// TO DO !!!