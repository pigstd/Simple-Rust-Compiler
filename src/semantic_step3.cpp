#include "lexer.h"
#include "semantic_step2.h"
#include "visitor.h"
#include <memory>
#include "semantic_step3.h"

ConstValue_ptr parse_literal_token_to_const_value(const Token &token) {
    if (token.type == Token_type::NUMBER) {
        // ..._i32, ..._u32, ..._isize, ..._usize, ...
        string value = token.value;
        if (value.size() >= 4 && value.substr(value.size() - 4) == "_i32") {
            // i32
            int int_value = std::stoi(value.substr(0, value.size() - 4));
            return std::make_shared<I32_ConstValue>(int_value);
        } else if (value.size() >= 4 && value.substr(value.size() - 4) == "_u32") {
            // u32
            unsigned int uint_value = static_cast<unsigned int>(std::stoul(value.substr(0, value.size() - 4)));
            return std::make_shared<U32_ConstValue>(uint_value);
        } else if (value.size() >= 6 && value.substr(value.size() - 6) == "_isize") {
            // isize
            int isize_value = std::stoi(value.substr(0, value.size() - 6));
            return std::make_shared<Isize_ConstValue>(isize_value);
        } else if (value.size() >= 6 && value.substr(value.size() - 6) == "_usize") {
            // usize
            unsigned int usize_value = static_cast<unsigned int>(std::stoul(value.substr(0, value.size() - 6)));
            return std::make_shared<Usize_ConstValue>(usize_value);
        } else {
            // anyint
            long long anyint_value = std::stoll(value);
            return std::make_shared<AnyInt_ConstValue>(anyint_value);
        }
    } else if (token.type == Token_type::TRUE) {
        return std::make_shared<Bool_ConstValue>(true);
    } else if (token.type == Token_type::FALSE) {
        return std::make_shared<Bool_ConstValue>(false);
    } else if (token.type == Token_type::CHAR) {
        if (token.value.size() != 1) {
            throw string("CE, char literal must be a single character");
        }
        return std::make_shared<Char_ConstValue>(token.value[0]);
    } else {
        // 不支持 string 等 type，投降（遇到了再说）
        throw string("CE, unexpected token type for literal type: ") + token_type_to_string(token.type);
    }
}

void LetStmtAndRepeatArrayVisitor::visit(LiteralExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(IdentifierExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(BinaryExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(UnaryExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(CallExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(FieldExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(StructExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(IndexExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(BlockExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(IfExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(WhileExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(LoopExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(ReturnExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(BreakExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(ContinueExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(CastExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(PathExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(SelfExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(UnitExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(ArrayExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(RepeatArrayExpr &node) {
    const_expr_queue.push_back(node.size);
    AST_Walker::visit(node);
}
void LetStmtAndRepeatArrayVisitor::visit(FnItem &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(StructItem &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(EnumItem &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(ImplItem &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(ConstItem &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(LetStmt &node) {
    if (node.type != nullptr) {
        find_real_type(node_scope_map[std::make_shared<LetStmt>(node)], node.type, type_map, const_expr_queue);
        // 这里不管返回值，因为 type_map 里面已经存了
    }
    AST_Walker::visit(node);
}
void LetStmtAndRepeatArrayVisitor::visit(ExprStmt &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(ItemStmt &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(PathType &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(ArrayType &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(UnitType &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(IdentifierPattern &node) { AST_Walker::visit(node); }