#include "semantic_step4.h"
#include "ast.h"
#include "semantic_step1.h"
#include "semantic_step2.h"
#include "visitor.h"
#include <cassert>
#include <cstddef>

// 只有遇到 ArrayType 的时候，才会用到这个 visitor
// 其他节点都直接调用 AST_Walker 的 visit 即可

void ArrayTypeVisitor::visit(LiteralExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(IdentifierExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(BinaryExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(UnaryExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(CallExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(FieldExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(StructExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(IndexExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(BlockExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(IfExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(WhileExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(LoopExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(ReturnExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(BreakExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(ContinueExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(CastExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(PathExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(SelfExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(UnitExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(ArrayExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(RepeatArrayExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(FnItem &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(StructItem &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(EnumItem &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(ImplItem &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(ConstItem &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(LetStmt &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(ExprStmt &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(ItemStmt &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(PathType &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(ArrayType &node) {
    auto target_real_type = type_map[std::make_shared<ArrayType>(node)];
    auto array_real_type = std::dynamic_pointer_cast<ArrayRealType>(target_real_type);
    if (!array_real_type) {
        // 这个时候应该是前面代码写错了，不是 CE，报 Error 的错误
        throw "Error, should be array type";
    }
    size_t array_size = 0;
    if (const_expr_to_size_map.find(node.size_expr) != const_expr_to_size_map.end()) {
        array_size = const_expr_to_size_map[node.size_expr];
    } else {
        // 这个时候应该是前面代码写错了，不是 CE，报 Error 的错误
        throw "Error, cannot find array size";
    }
    array_real_type->size = array_size;
    AST_Walker::visit(node);
}
void ArrayTypeVisitor::visit(UnitType &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(IdentifierPattern &node) { AST_Walker::visit(node); }

bool type_is_number(RealType_ptr checktype) {
    assert(checktype != nullptr);
    if (checktype->kind == RealTypeKind::NEVER ||
        checktype->kind == RealTypeKind::ANYINT || 
        checktype->kind == RealTypeKind::I32 || 
        checktype->kind == RealTypeKind::ISIZE ||
        checktype->kind == RealTypeKind::U32 ||
        checktype->kind == RealTypeKind::USIZE) { return true; }
    return false;
}

RealType_ptr type_merge(RealType_ptr left, RealType_ptr right) {
    assert(left != nullptr && right != nullptr);
    if (left->kind == RealTypeKind::NEVER) { return right; }
    if (right->kind == RealTypeKind::NEVER) { return left; }
    if (left->is_ref != right->is_ref) {
        throw "CE, cannot merge two different reference types";
    }
    if (left->kind == RealTypeKind::ANYINT) {
        if (type_is_number(right)) { return right; }
        else throw "CE, cannot merge AnyInt with non-number type";
    }
    if (right->kind == RealTypeKind::ANYINT) {
        if (type_is_number(left)) { return left; }
        else throw "CE, cannot merge AnyInt with non-number type";
    }
    // 其他情况的 type 必须完全相同
    if (left->kind != right->kind) {
        throw "CE, cannot merge two different types";
    }
    if (left->kind == RealTypeKind::ARRAY) {
        auto left_array = std::dynamic_pointer_cast<ArrayRealType>(left);
        auto right_array = std::dynamic_pointer_cast<ArrayRealType>(right);
        assert(left_array != nullptr && right_array != nullptr);
        if (left_array->size != right_array->size) {
            throw "CE, cannot merge two different array types with different size";
        }
        auto merged_element_type = type_merge(left_array->element_type, right_array->element_type);
        return std::make_shared<ArrayRealType>(merged_element_type, left_array->size, left->is_ref);
    } else {
        return left;
    }
}