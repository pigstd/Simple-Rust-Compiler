#include "ast.h"
#include "lexer.h"
#include "visitor.h"
// #include <iostream>
#include <cassert>

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
void PathExpr::accept(AST_visitor &v) { v.visit(*this); }
void SelfExpr::accept(AST_visitor &v) { v.visit(*this); }
void UnitExpr::accept(AST_visitor &v) { v.visit(*this); }
void ArrayExpr::accept(AST_visitor &v) { v.visit(*this); }
void RepeatArrayExpr::accept(AST_visitor &v) { v.visit(*this); }

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
void UnitType::accept(AST_visitor &v) { v.visit(*this); }
void SelfType::accept(AST_visitor &v) { v.visit(*this); }

void IdentifierPattern::accept(AST_visitor &v) { v.visit(*this); }


string literal_type_to_string(LiteralType type) {
    switch (type) {
        case LiteralType::NUMBER: return "number";
        case LiteralType::STRING: return "string";
        case LiteralType::CHAR: return "char";
        case LiteralType::BOOL: return "bool";
        default: return "unknown_literal_type";
    }
}

string binary_operator_to_string(Binary_Operator op) {
    switch (op) {
        case Binary_Operator::ADD: return "+";
        case Binary_Operator::SUB: return "-";
        case Binary_Operator::MUL: return "*";
        case Binary_Operator::DIV: return "/";
        case Binary_Operator::MOD: return "%";
        case Binary_Operator::AND: return "&";
        case Binary_Operator::OR: return "|";
        case Binary_Operator::XOR: return "^";
        case Binary_Operator::AND_AND: return "&&";
        case Binary_Operator::OR_OR: return "||";
        case Binary_Operator::EQ: return "==";
        case Binary_Operator::NEQ: return "!=";
        case Binary_Operator::LT: return "<";
        case Binary_Operator::GT: return ">";
        case Binary_Operator::LEQ: return "<=";
        case Binary_Operator::GEQ: return ">=";
        case Binary_Operator::SHL: return "<<";
        case Binary_Operator::SHR: return ">>";
        case Binary_Operator::ASSIGN: return "=";
        case Binary_Operator::ADD_ASSIGN: return "+=";
        case Binary_Operator::SUB_ASSIGN: return "-=";
        case Binary_Operator::MUL_ASSIGN: return "*=";
        case Binary_Operator::DIV_ASSIGN: return "/=";
        case Binary_Operator::MOD_ASSIGN: return "%=";
        case Binary_Operator::AND_ASSIGN: return "&=";
        case Binary_Operator::OR_ASSIGN: return "|=";
        case Binary_Operator::XOR_ASSIGN: return "^=";
        case Binary_Operator::SHL_ASSIGN: return "<<=";
        case Binary_Operator::SHR_ASSIGN: return ">>=";
        default: return "unknown_binary_operator";
    }
}

string unary_operator_to_string(Unary_Operator op) {
    switch (op) {
        case Unary_Operator::NEG: return "-";
        case Unary_Operator::NOT: return "!";
        case Unary_Operator::REF: return "&";
        case Unary_Operator::REF_MUT: return "&mut";
        case Unary_Operator::DEREF: return "*";
        default: return "unknown_unary_operator";
    }
}

string fn_reciever_type_to_string(fn_reciever_type type) {
    switch (type) {
        case fn_reciever_type::NO_RECEIVER: return "no receiver";
        case fn_reciever_type::SELF: return "self";
        case fn_reciever_type::SELF_REF: return "&self";
        case fn_reciever_type::SELF_REF_MUT: return "&mut self";
        default: return "unknown receiver type";
    }
}

string mutibility_to_string(Mutibility mut) {
    switch (mut) {
        case Mutibility::IMMUTABLE: return "immutable";
        case Mutibility::MUTABLE: return "mutable";
        default: return "unknown_mutibility";
    }
}

string reference_type_to_string(ReferenceType ref) {
    switch (ref) {
        case ReferenceType::NO_REF: return "no_ref";
        case ReferenceType::REF: return "ref";
        case ReferenceType::REF_MUT: return "ref_mut";
        default: return "unknown_reference_type";
    }
}