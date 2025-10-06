#ifndef SEMANTIC_STEP1_H
#define SEMANTIC_STEP1_H

/*
实现 semantic check 的第一步的相关代码
step1 : 建作用域树 + 符号初收集
*/

#include "ast.h"
#include "visitor.h"
#include <map>
#include <memory>
#include <vector>

using std::map;
using std::vector;
using std::string;
using std::weak_ptr;
using std::shared_ptr;

struct Scope;
struct TypeDecl;
struct ValueDecl;

struct StructDecl;
struct EnumDecl;
// 属于 TypeDecl

struct FnDecl;
struct ConstDecl;
struct ImplDecl;
// 属于 ValueDecl

struct RealType;
// 与 AST 的 Type 区分，用来记录表达式的真正的类型

using Scope_ptr = shared_ptr<Scope>;
using TypeDecl_ptr = shared_ptr<TypeDecl>;
using ValueDecl_ptr = shared_ptr<ValueDecl>;
using StructDecl_ptr = shared_ptr<StructDecl>;
using EnumDecl_ptr = shared_ptr<EnumDecl>;
using FnDecl_ptr = shared_ptr<FnDecl>;
using ConstDecl_ptr = shared_ptr<ConstDecl>;
using ImplDecl_ptr = shared_ptr<ImplDecl>;

using RealType_ptr = shared_ptr<RealType>;

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

    Scope(Scope_ptr parent_, ScopeKind kind_, string impl_struct_ = "") : parent(parent_), kind(kind_), impl_struct(impl_struct_), self_struct(nullptr) {}
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

enum class TypeDeclKind {
    Struct,
    Enum,
};
enum class ValueDeclKind {
    Function,
    Constant,
    LetStmt,
};

struct TypeDecl {
    TypeDeclKind kind;
    virtual ~TypeDecl() = default;
    TypeDecl(TypeDeclKind kind_) : kind(kind_) {}
};
struct ValueDecl {
    ValueDeclKind kind;
    virtual ~ValueDecl() = default;
    ValueDecl(ValueDeclKind kind_) : kind(kind_) {}
};
// 作为基类

struct StructDecl : public TypeDecl {
    StructItem &ast_node;
    map<string, RealType_ptr> fields; // 字段名的类型，第二轮填
    map<string, FnDecl_ptr> methods, associated_func;
    // example : point.len() -> methods, point::len() -> associated_func
    map<string, ConstDecl_ptr> associated_const;
    // example : point::ZERO -> associated_const
    StructDecl(StructItem &ast_node_) : TypeDecl(TypeDeclKind::Struct), ast_node(ast_node_) {}
    virtual ~StructDecl() = default;
};

struct EnumDecl : public TypeDecl {
    EnumItem &ast_node;
    map<string, int> variants; // 变体名和对应的值，第二轮填
    EnumDecl(EnumItem &ast_node_) : TypeDecl(TypeDeclKind::Enum), ast_node(ast_node_) {}
    virtual ~EnumDecl() = default;
};

struct FnDecl : public ValueDecl {
    FnItem &ast_node;
    weak_ptr<Scope> function_scope; // 函数的作用域
    vector<pair<Pattern_ptr, RealType_ptr>> parameters; // 参数名(pattern)和参数类型，第二轮填
    RealType_ptr return_type; // 返回类型，第二轮填
    FnDecl(FnItem &ast_node_, Scope_ptr function_scope_)
        : ValueDecl(ValueDeclKind::Function), ast_node(ast_node_), function_scope(function_scope_) {}
};

struct ConstDecl : public ValueDecl {
    ConstItem &ast_node;
    RealType_ptr const_type; // 常量类型，第二轮填
    ConstDecl(ConstItem &ast_node_) : ValueDecl(ValueDeclKind::Constant), ast_node(ast_node_) {}
};

// Let 语句引入一个局部变量
// 在 step 4 的时候会把 LetDecl 放入对应的 Scope 里面
struct LetDecl : public ValueDecl {
    RealType_ptr let_type;
    Mutibility is_mut;
    LetDecl(RealType_ptr let_type_, Mutibility is_mut_) : ValueDecl(ValueDeclKind::LetStmt), let_type(let_type_), is_mut(is_mut_) {}
};

struct ScopeBuilder_Visitor : public AST_Walker {
    vector<Scope_ptr> scope_stack; // 作用域栈，最后一个即为当前作用域
    map<AST_Node_ptr, Scope_ptr> &node_scope_map;
    bool block_is_in_function;
    // 下一个 block 是不是一定是 fn foo() {} 的 body
    // 如果是 block，那么在遍历的时候才加入，否则在遍历到 Fn 的时候就加入
    ScopeBuilder_Visitor(Scope_ptr root_scope, map<AST_Node_ptr, Scope_ptr> &node_scope_map_);
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
    virtual void visit(IdentifierPattern &node) override;
};

#endif // SEMANTIC_STEP1_H