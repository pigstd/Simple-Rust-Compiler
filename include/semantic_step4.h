#ifndef SEMANTIC_STEP4_H
#define SEMANTIC_STEP4_H


#include "ast.h"
#include "semantic_step1.h"
#include "visitor.h"
#include "semantic_step2.h"

/*
第四步我想干的事情：
求出所有表达式的 RealType
以及是否可读，是否可写的性质
引入 let 语句
这样就完成了全部的 semantic check
*/

/*
type visitor:

求出每个表达式节点的 RealType
不用考虑：类型的 RealType，Repeat Array 的 size
存入 map<AST_Node_ptr, RealType_ptr> node_type_map
*/

using LetDecl_ptr = shared_ptr<LetDecl>;
// 每个 Scope 应该对应一个 Local_Variable_map
// 即 scope_local_variable_map
// 这样在查找 identifier 的时候，先查 local 变量，再查 value_namespace
using Local_Variable_map = map<string, LetDecl_ptr>;

enum class PlaceKind {
    NotPlace, // 纯右值（字面量、算术、函数返回、结构体字面量等）
    ReadOnlyPlace, // 不可变的左值（不可变变量、字段访问、数组索引等）
    ReadWritePlace // 可变的左值（可变变量、可变引用解引用等）
};

struct ExprTypeAndLetStmtVisitor : public AST_Walker {
    /*
    遇到 A::B 和 A.B 有两种可能
    A::B 可能是 enum 的 variant，也可能是 struct 的关联函数
    A.B 可能是一个 struct 类型变量的字段，也可能是一个 struct 类型变量的关联函数
    因此用 require_function 来区分，如果是 true 则是后者，否则是前者

    遇到 identifier 的时候：

    有可能是 type_namespace 里面的 struct enum，这种情况应该在前面就判断掉
    有可能是 value_namespace 里面的 const fn 和 let 变量
    fn 是 require_function = true
    let 变量是 require_function = false
    每个 scope 有一个 local，先查 local 再查 value_namespace，local 允许重名，直接覆盖
    */
    // 是否要求是函数类型
    bool require_function;
    // 存放每个表达式节点的 RealType 和 PlaceKind
    map<AST_Node_ptr, pair<RealType_ptr, PlaceKind>> &node_type_and_place_kind_map;
    // 存放每个节点对应的作用域
    map<AST_Node_ptr, Scope_ptr> &node_scope_map;
    // 存放每个 AST 的 Type 对应的 RealType，直接复制过来即可
    map<Type_ptr, RealType_ptr> &type_map;
    // 记录每个 Scope 的局部变量
    map<Scope_ptr, Local_Variable_map> scope_local_variable_map;

    ExprTypeAndLetStmtVisitor(bool require_function_,
            map<AST_Node_ptr, pair<RealType_ptr, PlaceKind>> &node_type_and_place_kind_map_,
            map<AST_Node_ptr, Scope_ptr> &node_scope_map_,
            map<Type_ptr, RealType_ptr> &type_map_) :
            require_function(require_function_),
            node_type_and_place_kind_map(node_type_and_place_kind_map_),
            node_scope_map(node_scope_map_),
            type_map(type_map_) {}
    virtual ~ExprTypeAndLetStmtVisitor() = default;
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


#endif // SEMANTIC_STEP4_H