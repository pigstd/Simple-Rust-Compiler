#ifndef SEMANTIC_STEP2_H
#define SEMANTIC_STEP2_H
#include "ast.h"
#include "semantic_step1.h"
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

/*
dfs Scope tree
1. 解析所有的 struct 和 enum 的类型
2. 解析 fn const 的类型
3. 解析 impl 的 self_struct
let 的类型之后解析
*/
void Scope_dfs_and_build_type(Scope_ptr scope, map<size_t, RealType_ptr> &type_map, vector<Expr_ptr> &const_expr_queue);

#endif // SEMANTIC_STEP2_H