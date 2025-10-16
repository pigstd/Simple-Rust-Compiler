#include "ast/visitor.h"
#include "ast/ast.h"
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
    node.struct_name->accept(*this);
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
void AST_Walker::visit([[maybe_unused]] ContinueExpr &node) { return; }
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
        name->accept(*this);
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
    node.type->accept(*this);
    if (node.initializer) {
        node.initializer->accept(*this);
    }
}
void AST_Walker::visit(ExprStmt &node) { node.expr->accept(*this); }
void AST_Walker::visit(ItemStmt &node) { node.item->accept(*this); }
void AST_Walker::visit([[maybe_unused]] PathType &node) { return; }
void AST_Walker::visit(ArrayType &node) {
    node.element_type->accept(*this);
    node.size_expr->accept(*this);
}
void AST_Walker::visit([[maybe_unused]] UnitType &node) { return; }
void AST_Walker::visit([[maybe_unused]] SelfType &node) { return; }
void AST_Walker::visit([[maybe_unused]] IdentifierPattern &node) { return; }

// AST_Printer: 有些调用 AST_Walker 遍历会比较方便，但是有些还是要自己写遍历
void AST_Printer::visit(LiteralExpr &node) {
    cout << tab() << "LiteralExpr, type =  " << literal_type_to_string(node.literal_type)
         << ", value = " << node.value << " , NodeId = " << node.NodeId << endl;
    depth++;
    AST_Walker::visit(node);
    depth--;
}
void AST_Printer::visit(IdentifierExpr &node) {
    cout << tab() << "IdentifierExpr, name =  " << node.name << " , NodeId = " << node.NodeId << endl;
    depth++;
    AST_Walker::visit(node);
    depth--;
}
void AST_Printer::visit(BinaryExpr &node) {
    cout << tab() << "BinaryExpr, op =  " << binary_operator_to_string(node.op) << " , NodeId = " << node.NodeId << endl;
    depth++;
    cout << tab() << "Left: " << endl;
    node.left->accept(*this);
    cout << tab() << "Right: " << endl;
    node.right->accept(*this);
    depth--;
}
void AST_Printer::visit(UnaryExpr &node) {
    cout << tab() << "UnaryExpr, op =  " << unary_operator_to_string(node.op) << " , NodeId = " << node.NodeId << endl;
    depth++;
    cout << tab() << "Right : " << endl;
    AST_Walker::visit(node);
    depth--;
}
void AST_Printer::visit(CallExpr &node) {
    cout << tab() << "CallExpr, arguments size =  " << node.arguments.size() << " , NodeId = " << node.NodeId << endl;
    depth++;
    cout << tab() << "Callee: " << endl;
    node.callee->accept(*this);
    cout << tab() << "Arguments: " << endl;
    for (auto &args : node.arguments)
        args->accept(*this);
    depth--;
}
void AST_Printer::visit(FieldExpr &node) {
    cout << tab() << "FieldExpr, field name =  " << node.field_name << " , NodeId = " << node.NodeId << endl;
    depth++;
    cout << tab() << "Base : " << endl;
    AST_Walker::visit(node);
    depth--;
}
void AST_Printer::visit(StructExpr &node) {
    cout << tab() << "StructExpr, NodeId = " << node.NodeId << endl;
    depth++;
    cout << tab() << "Struct Name : " << endl;
    node.struct_name->accept(*this);
    cout << tab() << "Fields : " << endl;
    for (auto &[name, value] : node.fields) {
        cout << tab() << "name = " << name << ", value : " << endl;
        value->accept(*this);
    }
    depth--;
}
void AST_Printer::visit(IndexExpr &node) {
    cout << tab() << "IndexExpr , NodeId = " << node.NodeId << endl;
    depth++;
    cout << tab() << "Base : " << endl;
    node.base->accept(*this);
    cout << tab() << "Index : " << endl;
    node.index->accept(*this);
    depth--;
}
void AST_Printer::visit(BlockExpr &node) {
    cout << tab() << "BlockExpr, statements size =  " << node.statements.size() << " , NodeId = " << node.NodeId << endl;
    depth++;
    cout << tab() << "Statements : " << endl;
    for (auto &stmt : node.statements) {
        stmt->accept(*this);
    }
    if (node.tail_statement) {
        cout << tab() << "Tail Statement : " << endl;
        node.tail_statement->accept(*this);
    }
    depth--;
}
void AST_Printer::visit(IfExpr &node) {
    cout << tab() << "IfExpr , NodeId = " << node.NodeId << endl;
    if (node.must_return_unit) {
        cout << tab() << "(must return unit)" << endl;
    }
    depth++;
    cout << tab() << "Condition : " << endl;
    node.condition->accept(*this);
    cout << tab() << "Then Branch : " << endl;
    node.then_branch->accept(*this);
    if (node.else_branch) {
        cout << tab() << "Else Branch : " << endl;
        node.else_branch->accept(*this);
    }
    depth--;
}
void AST_Printer::visit(WhileExpr &node) {
    cout << tab() << "WhileExpr , NodeId = " << node.NodeId << endl;
    depth++;
    cout << tab() << "Condition : " << endl;
    node.condition->accept(*this);
    cout << tab() << "Body : " << endl;
    node.body->accept(*this);
    depth--;
}
void AST_Printer::visit(LoopExpr &node) {
    cout << tab() << "LoopExpr , NodeId = " << node.NodeId << endl;
    if (node.must_return_unit) {
        cout << tab() << "(must return unit)" << endl;
    }
    depth++;
    cout << tab() << "Body : " << endl;
    node.body->accept(*this);
    depth--;
}
void AST_Printer::visit(ReturnExpr &node) {
    cout << tab() << "ReturnExpr , NodeId = " << node.NodeId << endl;
    depth++;
    if (node.return_value) {
        cout << tab() << "Return Value : " << endl;
        node.return_value->accept(*this);
    }
    depth--;
}
void AST_Printer::visit(BreakExpr &node) {
    cout << tab() << "BreakExpr , NodeId = " << node.NodeId << endl;
    depth++;
    if (node.break_value) {
        cout << tab() << "Break Value : " << endl;
        node.break_value->accept(*this);
    }
    depth--;
}
void AST_Printer::visit(ContinueExpr &node) {
    cout << tab() << "ContinueExpr , NodeId = " << node.NodeId << endl;
    depth++;
    AST_Walker::visit(node);
    depth--;
}
void AST_Printer::visit(CastExpr &node) {
    cout << tab() << "CastExpr , NodeId = " << node.NodeId << endl;
    depth++;
    cout << tab() << "Expr : " << endl;
    node.expr->accept(*this);
    cout << tab() << "Target Type : " << endl;
    node.target_type->accept(*this);
    depth--;
}
void AST_Printer::visit(PathExpr &node) {
    cout << tab() << "PathExpr, name =  " << node.name << " , NodeId = " << node.NodeId << endl;
    depth++;
    cout << tab() << "Base : " << endl;
    node.base->accept(*this);
    depth--;
}
void AST_Printer::visit(SelfExpr &node) {
    cout << tab() << "SelfExpr , NodeId = " << node.NodeId << endl;
    depth++;
    AST_Walker::visit(node);
    depth--;
}
void AST_Printer::visit(UnitExpr &node) {
    cout << tab() << "UnitExpr , NodeId = " << node.NodeId << endl;
    depth++;
    AST_Walker::visit(node);
    depth--;
}
void AST_Printer::visit(ArrayExpr &node) {
    cout << tab() << "ArrayExpr, elements size =  " << node.elements.size() << " , NodeId = " << node.NodeId << endl;
    depth++;
    cout << tab() << "Elements : " << endl;
    for (auto &elem : node.elements) {
        elem->accept(*this);
    }
    depth--;
}
void AST_Printer::visit(RepeatArrayExpr &node) {
    cout << tab() << "RepeatArrayExpr , NodeId = " << node.NodeId << endl;
    depth++;
    cout << tab() << "Element : " << endl;
    node.element->accept(*this);
    cout << tab() << "Size : " << endl;
    node.size->accept(*this);
    depth--;
}
void AST_Printer::visit(FnItem &node) {
    cout << tab() << "FnItem, function name =  " << node.function_name <<
    ", receiver type = " << fn_reciever_type_to_string(node.receiver_type) << " , NodeId = " << node.NodeId << endl;
    depth++;
    cout << tab() << "Parameters : " << endl;
    for (auto &[name, type] : node.parameters) {
        cout << tab() << "Parameter name : " << endl;
        name->accept(*this);
        cout << tab() << "Parameter type : " << endl;
        type->accept(*this);
    }
    if (node.return_type) {
        cout << tab() << "Return Type : " << endl;
        node.return_type->accept(*this);
    }
    cout << tab() << "Body : " << endl;
    node.body->accept(*this);
    depth--;
}
void AST_Printer::visit(StructItem &node) {
    cout << tab() << "StructItem, struct name =  " << node.struct_name << " , NodeId = " << node.NodeId << endl;
    depth++;
    cout << tab() << "Fields : " << endl;
    for (auto &[name, type] : node.fields) {
        cout << tab() << "Field name = " << name << ", type : " << endl;
        type->accept(*this);
    }
    depth--;
}
void AST_Printer::visit(EnumItem &node) {
    cout << tab() << "EnumItem, enum name =  " << node.enum_name << " , NodeId = " << node.NodeId << endl;
    depth++;
    cout << tab() << "Variants : " << endl;
    for (auto &variant : node.variants) {
        cout << tab() << "Variant name = " << variant << endl;
    }
    AST_Walker::visit(node);
    depth--;
}
void AST_Printer::visit(ImplItem &node) {
    cout << tab() << "ImplItem, struct name =  " << node.struct_name << " , NodeId = " << node.NodeId << endl;
    depth++;
    cout << tab() << "Methods : " << endl;
    for (auto &item : node.methods) {
        item->accept(*this);
    }
    depth--;
}
void AST_Printer::visit(ConstItem &node) {
    cout << tab() << "ConstItem, const name =  " << node.const_name << " , NodeId = " << node.NodeId << endl;
    depth++;
    cout << tab() << "Const Type : " << endl;
    node.const_type->accept(*this);
    cout << tab() << "Value : " << endl;
    node.value->accept(*this);
    depth--;
}
void AST_Printer::visit(LetStmt &node) {
    cout << tab() << "LetStmt , NodeId = " << node.NodeId << endl;
    depth++;
    cout << tab() << "Pattern : " << endl;
    node.pattern->accept(*this);
    cout << tab() << "Type : " << endl;
    node.type->accept(*this);
    if (node.initializer) {
        cout << tab() << "Initializer : " << endl;
        node.initializer->accept(*this);
    }
    depth--;
}
void AST_Printer::visit(ExprStmt &node) {
    cout << tab() << "ExprStmt, is_semi =  " << (node.is_semi ? "true" : "false") << " , NodeId = " << node.NodeId << endl;
    depth++;
    cout << tab() << "Expr : " << endl;
    node.expr->accept(*this);
    depth--;
}
void AST_Printer::visit(ItemStmt &node) {
    cout << tab() << "ItemStmt , NodeId = " << node.NodeId << endl;
    depth++;
    cout << tab() << "Item : " << endl;
    node.item->accept(*this);
    depth--;
}
void AST_Printer::visit(PathType &node) {
    cout << tab() << "PathType, name =  " << node.name << " , NodeId = " << node.NodeId << endl;
    cout << tab() << ", Reference Type = " << reference_type_to_string(node.ref_type) << endl;
    depth++;
    AST_Walker::visit(node);
    depth--;
}
void AST_Printer::visit(ArrayType &node) {
    cout << tab() << "ArrayType , NodeId = " << node.NodeId << endl;
    cout << tab() << ", Reference Type = " << reference_type_to_string(node.ref_type) << endl;
    depth++;
    cout << tab() << "Element Type : " << endl;
    node.element_type->accept(*this);
    cout << tab() << "Size Expr : " << endl;
    node.size_expr->accept(*this);
    depth--;
}
void AST_Printer::visit(UnitType &node) {
    cout << tab() << "UnitType , NodeId = " << node.NodeId << endl;
    cout << tab() << ", Reference Type = " << reference_type_to_string(node.ref_type) << endl;
    depth++;
    AST_Walker::visit(node);
    depth--;
}
void AST_Printer::visit(SelfType &node) {
    cout << tab() << "SelfType , NodeId = " << node.NodeId << endl;
    cout << tab() << ", Reference Type = " << reference_type_to_string(node.ref_type) << endl;
    depth++;
    AST_Walker::visit(node);
    depth--;
}
void AST_Printer::visit(IdentifierPattern &node) {
    cout << tab() << "IdentifierPattern, name =  " << node.name << " , NodeId = " << node.NodeId << endl;
    cout << tab() << "Mutibility = " << mutibility_to_string(node.is_mut)
         << ", Reference Type = " << reference_type_to_string(node.is_ref) << endl;
    depth++;
    AST_Walker::visit(node);
    depth--;
}

void ASTIdGenerator::visit(LiteralExpr &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(IdentifierExpr &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(BinaryExpr &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(UnaryExpr &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(CallExpr &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(FieldExpr &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(StructExpr &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(IndexExpr &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(BlockExpr &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(IfExpr &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(WhileExpr &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(LoopExpr &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(ReturnExpr &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(ContinueExpr &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(BreakExpr &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(CastExpr &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(PathExpr &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(SelfExpr &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(UnitExpr &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(ArrayExpr &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(RepeatArrayExpr &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(FnItem &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(StructItem &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(EnumItem &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(ImplItem &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(ConstItem &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(LetStmt &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(ExprStmt &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(ItemStmt &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(PathType &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(ArrayType &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(UnitType &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(SelfType &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
void ASTIdGenerator::visit(IdentifierPattern &node) {
    node.NodeId = current_id++;
    AST_Walker::visit(node);
}
