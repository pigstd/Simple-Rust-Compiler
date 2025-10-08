#ifndef SEMANTIC_STEP4_H
#define SEMANTIC_STEP4_H


#include "ast.h"
#include "semantic_step1.h"
#include "semantic_step3.h"
#include "visitor.h"
#include "semantic_step2.h"
#include <cstddef>

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

/*
合并两个类型，要求类型相同，用于赋值，if else 分支等操作
特殊情况：
Never 可以和任何类型合并，结果为另一个类型
AnyInt 可以和任何整数类型合并，结果为另一个整数类型
*/
RealType_ptr type_merge(RealType_ptr left, RealType_ptr right);

/*
Anyint 可以合并的内容：
Never, Anyint, 各种 int
*/
bool type_is_number(RealType_ptr checktype);

RealType_ptr type_of_literal(LiteralType type, string value);

// 深拷贝一个 RealType
RealType_ptr copy(RealType_ptr type);

// 第二轮处理出了所有 Type，但是 Array Type 只存了 ast 节点，没存真正大小
// 遍历一遍所有的 Array Type，利用 const_expr_to_size_map 把大小填回去
struct ArrayTypeVisitor : public AST_Walker {
    // 记录 AST 的 Type 对应的真正的类型 RealType
    map<size_t, RealType_ptr> &type_map;
    // 记录 Expr 对应的 size
    map<size_t, size_t> &const_expr_to_size_map;

    ArrayTypeVisitor(map<size_t, RealType_ptr> &type_map_,
            map<size_t, size_t> &const_expr_to_size_map_) :
            type_map(type_map_),
            const_expr_to_size_map(const_expr_to_size_map_) {}
    virtual ~ArrayTypeVisitor() = default;
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
    map<size_t, pair<RealType_ptr, PlaceKind>> &node_type_and_place_kind_map;
    // 存放每个节点对应的作用域
    map<size_t, Scope_ptr> &node_scope_map;
    // 存放每个 AST 的 Type 对应的 RealType，直接复制过来即可
    map<size_t, RealType_ptr> &type_map;
    // 记录每个 Scope 的局部变量
    map<Scope_ptr, Local_Variable_map> &scope_local_variable_map;

    // 遇到数组 type 的时候，将 size 从这个 map 里面取出来
    map<size_t, size_t> &const_expr_to_size_map;

    // 对于循环，break 会返回一个值
    // 为了确定循环的返回值，需要一个栈维护当前在哪个循环，这个循环的返回值是什么
    // 遇到循环的时候，新加一个，然后遇到 break 的时候和这个栈顶的返回值合并
    vector<RealType_ptr> loop_type_stack;

    // 记录每个 AST 树节点的 OutComeState
    map<size_t, OutcomeState> &node_outcome_state_map;

    // 内置方法 e.g. array.len()
    // (类型, 方法名, 方法的 FnDecl)
    vector<std::tuple<RealTypeKind, string, FnDecl_ptr>> &builtin_method_funcs;

    // 内置关联函数 e.g. String::from()
    vector<std::tuple<RealTypeKind, string, FnDecl_ptr>> &builtin_associated_funcs;

    // 是否在函数内，如果在存储该函数的定义
    FnDecl_ptr now_func_decl;

    // 找到该作用域下的 ValueDecl
    ValueDecl_ptr find_value_decl(Scope_ptr now_scope, const string &name);
    // 找到该作用域下的 TypeDecl
    TypeDecl_ptr find_type_decl(Scope_ptr now_scope, const string &name);    
    // 获取 (base_type, placekind) 的 method_name 方法，返回一个函数类型
    // e.g. point.len()
    // 如果没有找到则返回 nullptr
    // 除了结构体，还需要考虑内置的函数
    RealType_ptr get_method_func(RealType_ptr base_type, PlaceKind place_kind, const string &method_name);
    // 获取 base_type 的 func_name 关联函数，返回一个函数类型
    // e.g. point::len()
    // 如果没有找到则返回 nullptr
    // 除了结构体，还需要考虑内置的函数
    RealType_ptr get_associated_func(RealType_ptr base_type, const string &func_name);

    // 检查 let 语句：
    // let pattern : target_type = (expr_type, expr_place)
    // 是否合法，用于 let 和函数传参
    // 如果不合法 throw CE
    void check_let_stmt(Pattern_ptr let_pattern, RealType_ptr target_type, RealType_ptr expr_type, PlaceKind expr_place);

    // 将 let 语句加入到当前作用域
    // 同样，用于 let 语句和函数参数
    void intro_let_stmt(Scope_ptr current_scope, Pattern_ptr let_pattern, RealType_ptr let_type);

    // 检查 cast 是否合法
    // 如果不合法 throw CE
    void check_cast(RealType_ptr expr_type, RealType_ptr target_type);

    ExprTypeAndLetStmtVisitor(bool require_function_,
            map<size_t, pair<RealType_ptr, PlaceKind>> &node_type_and_place_kind_map_,
            map<size_t, Scope_ptr> &node_scope_map_,
            map<size_t, RealType_ptr> &type_map_,
            map<Scope_ptr, Local_Variable_map> &scope_local_variable_map_,
            map<size_t, size_t> &const_expr_to_size_map_,
            map<size_t, OutcomeState> &node_outcome_state_map_,
            vector<std::tuple<RealTypeKind, string, FnDecl_ptr>> &builtin_method_funcs_,
            vector<std::tuple<RealTypeKind, string, FnDecl_ptr>> &builtin_associated_funcs_) :
            require_function(require_function_),
            node_type_and_place_kind_map(node_type_and_place_kind_map_),
            node_scope_map(node_scope_map_),
            type_map(type_map_),
            scope_local_variable_map(scope_local_variable_map_),
            const_expr_to_size_map(const_expr_to_size_map_),
            node_outcome_state_map(node_outcome_state_map_),
            builtin_method_funcs(builtin_method_funcs_),
            builtin_associated_funcs(builtin_associated_funcs_),
            now_func_decl(nullptr) {}
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