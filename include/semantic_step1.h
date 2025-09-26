#ifndef SEMANTIC_STEP1_H
#define SEMANTIC_STEP1_H

/*
实现 semantic check 的第一步的相关代码
step1 : 建作用域树 + 符号初收集
*/

#include "ast.h"
#include "visitor.h"
#include <cstddef>
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

struct UnitRealType;
struct NeverRealType;
struct ArrayRealType;
struct StructRealType;
struct EnumRealType;
struct BoolRealType;
struct I32RealType;
struct IsizeRealType;
struct U32RealType;
struct UsizeRealType;
// 具体的 RealType

using Scope_ptr = shared_ptr<Scope>;
using TypeDecl_ptr = shared_ptr<TypeDecl>;
using ValueDecl_ptr = shared_ptr<ValueDecl>;
using StructDecl_ptr = shared_ptr<StructDecl>;
using EnumDecl_ptr = shared_ptr<EnumDecl>;
using FnDecl_ptr = shared_ptr<FnDecl>;
using ConstDecl_ptr = shared_ptr<ConstDecl>;
using ImplDecl_ptr = shared_ptr<ImplDecl>;

using RealType_ptr = shared_ptr<RealType>;
using UnitRealType_ptr = shared_ptr<UnitRealType>;
using NeverRealType_ptr = shared_ptr<NeverRealType>;
using ArrayRealType_ptr = shared_ptr<ArrayRealType>;
using StructRealType_ptr = shared_ptr<StructRealType>;
using EnumRealType_ptr = shared_ptr<EnumRealType>;
using BoolRealType_ptr = shared_ptr<BoolRealType>;
using I32RealType_ptr = shared_ptr<I32RealType>;
using IsizeRealType_ptr = shared_ptr<IsizeRealType>;
using U32RealType_ptr = shared_ptr<U32RealType>;
using UsizeRealType_ptr = shared_ptr<UsizeRealType>;

enum class ScopeKind {
    Root, // 一开始的全局作用域
    Block,
    Function,
    Impl
};

struct Scope {
    // 我玉玉症大发作：shared_ptr 不能循环引用
    weak_ptr<Scope> parent;
    ScopeKind kind;
    vector<Scope_ptr> children;
    map<string, TypeDecl_ptr> type_namespace; // 类型命名空间
    map<string, ValueDecl_ptr> value_namespace; // 值命名空间

    // for impl
    Type_ptr impl_for_type; // impl 作用于 AST 树上的哪个类型
    RealType_ptr self_struct; // impl 作用的类型，在第二轮被解析出来

    Scope(Scope_ptr parent_, ScopeKind kind_, Type_ptr impl_for_type_ = nullptr) : parent(parent_), kind(kind_), impl_for_type(impl_for_type_), self_struct(nullptr) {}
};

struct TypeDecl { };
struct ValueDecl { };
// 作为基类

struct StructDecl : public TypeDecl {
    StructItem_ptr ast_node;
    map<string, RealType_ptr> fields; // 字段名的类型，第二轮填
    map<string, FnDecl_ptr> methods, associated_func;
    // example : point.len() -> methods, point::len() -> associated_func
    map<string, ConstDecl_ptr> associated_const;
    // example : point::ZERO -> associated_const
    StructDecl(StructItem_ptr ast_node_) : ast_node(ast_node_) {}
};

struct EnumDecl : public TypeDecl {
    EnumItem_ptr ast_node;
    map<string, int> variants; // 变体名和对应的值，第二轮填
    EnumDecl(EnumItem_ptr ast_node_) : ast_node(ast_node_) {}
};

struct FnDecl : public ValueDecl {
    FnItem_ptr ast_node;
    Scope_ptr function_scope; // 函数的作用域
    vector<pair<string, RealType_ptr>> parameters; // 参数名和参数类型，第二轮填
    RealType_ptr return_type; // 返回类型，第二轮填
    FnDecl(FnItem_ptr ast_node_, Scope_ptr function_scope_)
        : ast_node(ast_node_), function_scope(function_scope_) {}
};

struct ConstDecl : public ValueDecl {
    ConstItem_ptr ast_node;
    RealType_ptr const_type; // 常量类型，第二轮填
    ConstDecl(ConstItem_ptr ast_node_) : ast_node(ast_node_) {}
};

struct ImplDecl : public ValueDecl {
    ImplItem_ptr ast_node;
    Scope_ptr impl_scope; // impl 的作用域
    Type_ptr impl_for_type; // impl 作用于哪个类型，存引用
    RealType_ptr self_struct; // impl 作用的类型，在第二轮被解析出来
    map<string, FnDecl_ptr> methods;
    ImplDecl(ImplItem_ptr ast_node_, Scope_ptr impl_scope_, Type_ptr impl_for_type_)
        : ast_node(ast_node_), impl_scope(impl_scope_), impl_for_type(impl_for_type_), self_struct(nullptr) {}

};

enum class RealTypeKind {
    UNIT,
    NEVER,
    ARRAY,
    STRUCT,
    ENUM,
    BOOL,
    I32,
    ISIZE,
    U32,
    USIZE,
};

struct RealType {
    RealTypeKind kind;
    Mutibility is_mut;
    ReferenceType is_ref;
    RealType(RealTypeKind k, Mutibility mut, ReferenceType ref) : kind(k), is_mut(mut), is_ref(ref) {}
};
struct UnitRealType : public RealType {
    UnitRealType(Mutibility mut, ReferenceType ref) : RealType(RealTypeKind::UNIT, mut, ref) {}
};
struct NeverRealType : public RealType {
    NeverRealType(Mutibility mut, ReferenceType ref) : RealType(RealTypeKind::NEVER, mut, ref) {}
};
struct ArrayRealType : public RealType {
    RealType_ptr element_type;
    size_t size; // 数组大小必须是一个常量，在第二轮被解析出来
    ArrayRealType(RealType_ptr elem_type, size_t sz, Mutibility mut, ReferenceType ref)
        : RealType(RealTypeKind::ARRAY, mut, ref), element_type(elem_type), size(sz) {}
};
struct StructRealType : public RealType {
    string name;
    StructDecl_ptr decl; // 具体的结构体类型
    StructRealType(const string &name_, Mutibility mut, ReferenceType ref, StructDecl_ptr struct_decl = nullptr)
        : RealType(RealTypeKind::STRUCT, mut, ref), name(name_), decl(struct_decl) {}
};
struct EnumRealType : public RealType {
    string name;
    EnumDecl_ptr decl; // 具体的枚举类型
    EnumRealType(const string &name_, Mutibility mut, ReferenceType ref, EnumDecl_ptr enum_decl = nullptr)
        : RealType(RealTypeKind::ENUM, mut, ref), name(name_), decl(enum_decl) {}
};
struct BoolRealType : public RealType {
    BoolRealType(Mutibility mut, ReferenceType ref) : RealType(RealTypeKind::BOOL, mut, ref) {}
};
struct I32RealType : public RealType {
    I32RealType(Mutibility mut, ReferenceType ref) : RealType(RealTypeKind::I32, mut, ref) {}
};
struct IsizeRealType : public RealType {
    IsizeRealType(Mutibility mut, ReferenceType ref) : RealType(RealTypeKind::ISIZE, mut, ref) {}
};
struct U32RealType : public RealType {
    U32RealType(Mutibility mut, ReferenceType ref) : RealType(RealTypeKind::U32, mut, ref) {}
};
struct UsizeRealType : public RealType {
    UsizeRealType(Mutibility mut, ReferenceType ref) : RealType(RealTypeKind::USIZE, mut, ref) {}
};

struct ScopeBuilder_Visitor : public AST_Walker {
    vector<Scope_ptr> scope_stack; // 作用域栈，最后一个即为当前作用域
    ScopeBuilder_Visitor() {
        // 初始化作用域栈，加入根作用域
        scope_stack.push_back(std::make_shared<Scope>(nullptr, ScopeKind::Root));
    }
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