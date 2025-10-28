#ifndef SCOPE_H
#define SCOPE_H

#include "ast/visitor.h"
#include "semantic/decl.h"


struct Scope;
using Scope_ptr = shared_ptr<Scope>;

enum class ScopeKind {
    Root, // 一开始的全局作用域
    Block,
    Function,
    Impl
};

struct Scope {
    // 我玉玉症大发作：shared_ptr 不能循环引用
    // 所以 parent 用 weak_ptr
    // children 用 shared_ptr
    // 之后也要注意
    weak_ptr<Scope> parent;
    ScopeKind kind;
    vector<Scope_ptr> children;
    map<string, TypeDecl_ptr> type_namespace; // 类型命名空间
    map<string, ValueDecl_ptr> value_namespace; // 值命名空间

    // for impl
    string impl_struct; // impl 作用于 struct 的名字，若这个 Scope 不是 impl，则为空字符串
    RealType_ptr self_struct; // impl 作用的类型，在第二轮被解析出来

    // 是否是 main 函数的作用域
    bool is_main_scope;
    // main 有没有 exit，没有就是 CE
    bool has_exit;
    
    Scope(Scope_ptr parent_, ScopeKind kind_, string impl_struct_ = "") :
        parent(parent_), kind(kind_), impl_struct(impl_struct_), self_struct(nullptr),
        is_main_scope(false), has_exit(false) {}
};

/*
remark : 关于避免循环引用
1. AST 树不记录父亲，儿子就用 shared_ptr
2. Scope 的 parent 用 weak_ptr，children 用 shared_ptr
3. node_scope_map ： 因为相当于存了若干对 shared ptr，所以不会循环引用
4. Decl 里面存 AST 节点的引用，不存指针
5. Decl 里面存的 RealType 用 shared_ptr, RealType 里面存的 Decl 用 weak_ptr

shared_ptr 的顺序：
Scope -> Decl -> RealType -> AST_Node
如果不是这个顺序的，最好用 weak_ptr
*/

struct ScopeBuilder_Visitor : public AST_Walker {
    vector<Scope_ptr> scope_stack; // 作用域栈，最后一个即为当前作用域
    map<size_t, Scope_ptr> &node_scope_map;
    bool block_is_in_function;
    // 下一个 block 是不是一定是 fn foo() {} 的 body
    // 如果是 block，那么在遍历的时候才加入，否则在遍历到 Fn 的时候就加入
    ScopeBuilder_Visitor(Scope_ptr root_scope, map<size_t, Scope_ptr> &node_scope_map_);
    ~ScopeBuilder_Visitor() override = default;
    Scope_ptr current_scope() { return scope_stack.back(); }
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

// 找到 scope 里面的 const_decl
// 找不到返回 nullptr
ConstDecl_ptr find_const_decl(Scope_ptr NowScope, string name);

// 找到 Type_decl
TypeDecl_ptr find_type_decl(Scope_ptr NowScope, string name);

#endif // SCOPE_H