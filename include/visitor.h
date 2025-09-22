#ifndef VISITOR_H
#define VISITOR_H

#include "ast.h"

/*

定义 visitor 模式的基类 AST_visitor
以及一些子类：递归遍历 AST 的 AST_Walker

*/


struct AST_visitor {
public:
    virtual ~AST_visitor() = default;
    virtual void visit(LiteralExpr &node) = 0;
    virtual void visit(IdentifierExpr &node) = 0;
    virtual void visit(BinaryExpr &node) = 0;
    virtual void visit(UnaryExpr &node) = 0;
    virtual void visit(CallExpr &node) = 0;
    virtual void visit(FieldExpr &node) = 0;
    virtual void visit(StructExpr &node) = 0;
    virtual void visit(IndexExpr &node) = 0;
    virtual void visit(BlockExpr &node) = 0;
    virtual void visit(IfExpr &node) = 0;
    virtual void visit(WhileExpr &node) = 0;
    virtual void visit(LoopExpr &node) = 0;
    virtual void visit(ReturnExpr &node) = 0;
    virtual void visit(BreakExpr &node) = 0;
    virtual void visit(ContinueExpr &node) = 0;
    virtual void visit(CastExpr &node) = 0;
    virtual void visit(PathExpr &node) = 0;
    virtual void visit(SelfExpr &node) = 0;
    virtual void visit(UnitExpr &node) = 0;
    virtual void visit(ArrayExpr &node) = 0;
    virtual void visit(RepeatArrayExpr &node) = 0;
    virtual void visit(FnItem &node) = 0;
    virtual void visit(StructItem &node) = 0;
    virtual void visit(EnumItem &node) = 0;
    virtual void visit(ImplItem &node) = 0;
    virtual void visit(ConstItem &node) = 0;
    virtual void visit(LetStmt &node) = 0;
    virtual void visit(ExprStmt &node) = 0;
    virtual void visit(ItemStmt &node) = 0;
    virtual void visit(PathType &node) = 0;
    virtual void visit(ArrayType &node) = 0;
    virtual void visit(UnitType &node) = 0;
    virtual void visit(IdentifierPattern &node) = 0;
};


// AST_Walker: 遍历 AST 的基类
struct AST_Walker : public AST_visitor {
public:
    ~AST_Walker() override = default;
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
    virtual void visit(IdentifierPattern &node) override;
};

#endif // VISITOR_H