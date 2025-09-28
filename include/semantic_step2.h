#ifndef SEMANTIC_STEP2_H
#define SEMANTIC_STEP2_H
#include "ast.h"
#include "semantic_step1.h"
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
    weak_ptr<StructDecl> decl; // 具体的结构体类型
    StructRealType(const string &name_, Mutibility mut, ReferenceType ref, StructDecl_ptr struct_decl = nullptr)
        : RealType(RealTypeKind::STRUCT, mut, ref), name(name_), decl(struct_decl) {}
};
struct EnumRealType : public RealType {
    string name;
    weak_ptr<EnumDecl> decl; // 具体的枚举类型
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
struct AnyIntRealType : public RealType {
    AnyIntRealType(Mutibility mut, ReferenceType ref) : RealType(RealTypeKind::ANYINT, mut, ref) {}
};
struct CharRealType : public RealType {
    CharRealType(Mutibility mut, ReferenceType ref) : RealType(RealTypeKind::CHAR, mut, ref) {}
};
struct StrRealType : public RealType {
    StrRealType(Mutibility mut, ReferenceType ref) : RealType(RealTypeKind::STR, mut, ref) {}
};


// 根据 AST 的 Type 找到真正的类型 RealType，并且返回指针
RealType_ptr find_real_type(Scope_ptr current_scope, Type_ptr type_ast);

/*
dfs Scope tree
1. 解析所有的 struct 和 enum 的类型
2. 解析 fn const 的类型
3. 解析 impl 的 self_struct
let 的类型之后解析
*/
void Scope_dfs_and_build_type(Scope_ptr scope, map<Type_ptr, RealType_ptr> &type_map);

#endif // SEMANTIC_STEP2_H