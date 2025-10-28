#ifndef TYPE_H
#define TYPE_H

#include "ast/ast.h"
#include "ast/visitor.h"
#include "semantic/decl.h"
#include <cstddef>
#include <memory>

using std::shared_ptr;
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
    ANYINT, // 字面量数字，可以是任何类型的 int
    CHAR,
    STR,
    STRING,
    FUNCTION,
};

string real_type_kind_to_string(RealTypeKind kind);

struct RealType {
    RealTypeKind kind;
    ReferenceType is_ref;
    RealType(RealTypeKind k, ReferenceType ref) : kind(k), is_ref(ref) {}
    virtual ~RealType() = default;
    // 输出这个 type 的信息，方便调试
    virtual string show_real_type_info() = 0;
};
struct UnitRealType : public RealType {
    UnitRealType(ReferenceType ref) : RealType(RealTypeKind::UNIT, ref) {}
    ~UnitRealType() override = default;
    virtual string show_real_type_info() override;
};
struct NeverRealType : public RealType {
    NeverRealType(ReferenceType ref) : RealType(RealTypeKind::NEVER, ref) {}
    ~NeverRealType() override = default;
    virtual string show_real_type_info() override;
};
struct ArrayRealType : public RealType {
    RealType_ptr element_type;
    Expr_ptr size_expr; // 数组大小的表达式，在第二轮被解析出来，之后用 const_expr_to_size_map 来检查大小
    // 数组大小，对于 type，先记录 expr，第三轮计算 expr 的值，第四轮填回来
    // 对于字面量 [1, 2, 3] 这种，直接在这里填 size，不用记录 expr
    size_t size;
    ArrayRealType(RealType_ptr elem_type, Expr_ptr size_expr_, ReferenceType ref, size_t size_ = 0)
        : RealType(RealTypeKind::ARRAY, ref), element_type(elem_type), size_expr(size_expr_), size(size_) {}
    ~ArrayRealType() override = default;
    virtual string show_real_type_info() override;
};
struct StructRealType : public RealType {
    string name;
    weak_ptr<StructDecl> decl; // 具体的结构体类型
    StructRealType(const string &name_, ReferenceType ref, StructDecl_ptr struct_decl = nullptr)
        : RealType(RealTypeKind::STRUCT, ref), name(name_), decl(struct_decl) {}
    ~StructRealType() override = default;
    virtual string show_real_type_info() override;
};
struct EnumRealType : public RealType {
    string name;
    weak_ptr<EnumDecl> decl; // 具体的枚举类型
    EnumRealType(const string &name_, ReferenceType ref, EnumDecl_ptr enum_decl = nullptr)
        : RealType(RealTypeKind::ENUM, ref), name(name_), decl(enum_decl) {}
    ~EnumRealType() override = default;
    virtual string show_real_type_info() override;
};
struct BoolRealType : public RealType {
    BoolRealType(ReferenceType ref) : RealType(RealTypeKind::BOOL, ref) {}
    ~BoolRealType() override = default;
    virtual string show_real_type_info() override;
};
struct I32RealType : public RealType {
    I32RealType(ReferenceType ref) : RealType(RealTypeKind::I32, ref) {}
    ~I32RealType() override = default;
    virtual string show_real_type_info() override;
};
struct IsizeRealType : public RealType {
    IsizeRealType(ReferenceType ref) : RealType(RealTypeKind::ISIZE, ref) {}
    ~IsizeRealType() override = default;
    virtual string show_real_type_info() override;
};
struct U32RealType : public RealType {
    U32RealType(ReferenceType ref) : RealType(RealTypeKind::U32, ref) {}
    ~U32RealType() override = default;
    virtual string show_real_type_info() override;
};
struct UsizeRealType : public RealType {
    UsizeRealType(ReferenceType ref) : RealType(RealTypeKind::USIZE, ref) {}
    ~UsizeRealType() override = default;
    virtual string show_real_type_info() override;
};
struct AnyIntRealType : public RealType {
    AnyIntRealType(ReferenceType ref) : RealType(RealTypeKind::ANYINT, ref) {}
    ~AnyIntRealType() override = default;
    virtual string show_real_type_info() override;
};
struct CharRealType : public RealType {
    CharRealType(ReferenceType ref) : RealType(RealTypeKind::CHAR, ref) {}
    ~CharRealType() override = default;
    virtual string show_real_type_info() override;
};
struct StrRealType : public RealType {
    StrRealType(ReferenceType ref) : RealType(RealTypeKind::STR, ref) {}
    ~StrRealType() override = default;
    virtual string show_real_type_info() override;
};
struct StringRealType : public RealType {
    StringRealType(ReferenceType ref) : RealType(RealTypeKind::STRING, ref) {}
    ~StringRealType() override = default;
    virtual string show_real_type_info() override;
};
struct FunctionRealType : public RealType {
    // 函数的定义
    weak_ptr<FnDecl> decl;
    FunctionRealType(FnDecl_ptr decl_, ReferenceType ref) :
        RealType(RealTypeKind::FUNCTION, ref), decl(decl_) {}
    ~FunctionRealType() override = default;
    virtual string show_real_type_info() override;
};

// 根据 AST 的 Type 找到真正的类型 RealType，并且返回指针
// 存放在 map 中，这样后面 let 语句遇到的时候使用这个，直接 find_real_type 即可
RealType_ptr find_real_type(Scope_ptr current_scope, Type_ptr type_ast, map<size_t, RealType_ptr> &type_map, vector<Expr_ptr> &const_expr_queue);


// 遍历 AST 树，将其他类型的 type 解析出来
// 遇到 let 语句， As 语句，PathExpr 语句，StructExpr 语句
// 将 RealType 解析出来，并且存到 type_map 中，这样第四步直接查 type_map 即可 
// 遇到 type 和 RepeatArray 中的常量表达式，放入 const_expr_queue
struct OtherTypeAndRepeatArrayVisitor : public AST_Walker {
    map<size_t, Scope_ptr> &node_scope_map;
    map<size_t, RealType_ptr> &type_map;
    vector<Expr_ptr> &const_expr_queue;
    OtherTypeAndRepeatArrayVisitor(map<size_t, Scope_ptr> &node_scope_map_, map<size_t, RealType_ptr> &type_map_, vector<Expr_ptr> &const_expr_queue_)
        : node_scope_map(node_scope_map_), type_map(type_map_), const_expr_queue(const_expr_queue_) {}
    virtual ~OtherTypeAndRepeatArrayVisitor() = default;
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

#endif // TYPE_H