#ifndef DECL_H
#define DECL_H

#include "ast/ast.h"
#include <map>
#include <memory>
#include <utility>
#include <vector>

using std::map;
using std::vector;
using std::string;
using std::weak_ptr;
using std::shared_ptr;

struct TypeDecl;
struct ValueDecl;

struct StructDecl;
struct EnumDecl;
// 属于 TypeDecl

struct FnDecl;
struct ConstDecl;
struct ImplDecl;
struct LetDecl;
// 属于 ValueDecl

struct RealType;
// 与 AST 的 Type 区分，用来记录表达式的真正的类型

using TypeDecl_ptr = shared_ptr<TypeDecl>;
using ValueDecl_ptr = shared_ptr<ValueDecl>;
using StructDecl_ptr = shared_ptr<StructDecl>;
using EnumDecl_ptr = shared_ptr<EnumDecl>;
using FnDecl_ptr = shared_ptr<FnDecl>;
using ConstDecl_ptr = shared_ptr<ConstDecl>;
using ImplDecl_ptr = shared_ptr<ImplDecl>;
using LetDecl_ptr = shared_ptr<LetDecl>;

using RealType_ptr = shared_ptr<RealType>;

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
    string name;
    virtual ~TypeDecl() = default;
    TypeDecl(TypeDeclKind kind_, string name_) : kind(kind_), name(std::move(name_)) {}
};
struct ValueDecl {
    ValueDeclKind kind;
    string name;
    virtual ~ValueDecl() = default;
    ValueDecl(ValueDeclKind kind_, string name_) : kind(kind_), name(std::move(name_)) {}
};
// 作为基类

struct StructDecl : public TypeDecl {
    vector<string> field_order;
    StructItem_ptr ast_node;
    map<string, RealType_ptr> fields; // 字段名的类型，第二轮填
    map<string, FnDecl_ptr> methods, associated_func;
    // example : point.len() -> methods, point::len() -> associated_func
    map<string, ConstDecl_ptr> associated_const;
    // example : point::ZERO -> associated_const
    StructDecl(StructItem_ptr ast_node_, string name_)
        : TypeDecl(TypeDeclKind::Struct, std::move(name_)), ast_node(ast_node_) {}
    virtual ~StructDecl() = default;
};

struct EnumDecl : public TypeDecl {
    EnumItem_ptr ast_node;
    map<string, int> variants; // 变体名和对应的值，第二轮填
    EnumDecl(EnumItem_ptr ast_node_, string name_)
        : TypeDecl(TypeDeclKind::Enum, std::move(name_)), ast_node(ast_node_) {}
    virtual ~EnumDecl() = default;
};

struct Scope;
using Scope_ptr = shared_ptr<Scope>;

struct FnDecl : public ValueDecl {
    FnItem_ptr ast_node;
    weak_ptr<Scope> function_scope; // 函数的作用域
    vector<pair<Pattern_ptr, RealType_ptr>> parameters; // 参数名(pattern)和参数类型，第二轮填
    vector<LetDecl_ptr> parameter_let_decls; // 每个参数对应的 LetDecl，第四轮填
    RealType_ptr return_type; // 返回类型，第二轮填
    fn_reciever_type receiver_type; // 是否有 self 参数
    weak_ptr<StructDecl> self_struct; // 如果是 method，则存储这个 method 属于哪个 struct，第二轮填
    bool is_main, is_exit;
    bool is_builtin;
    bool is_array_len;
    FnDecl(FnItem_ptr ast_node_, Scope_ptr function_scope_, fn_reciever_type receiver_type_, string name_)
        : ValueDecl(ValueDeclKind::Function, std::move(name_)), ast_node(ast_node_), function_scope(function_scope_),
          receiver_type(receiver_type_), is_main(false), is_exit(false), is_builtin(false), is_array_len(false) {}
};

struct ConstDecl : public ValueDecl {
    ConstItem_ptr ast_node;
    RealType_ptr const_type; // 常量类型，第二轮填
    ConstDecl(ConstItem_ptr ast_node_, string name_)
        : ValueDecl(ValueDeclKind::Constant, std::move(name_)), ast_node(ast_node_) {}
};

// Let 语句引入一个局部变量
// 在 step 4 的时候会把 LetDecl 放入对应的 Scope 里面
struct LetDecl : public ValueDecl {
    RealType_ptr let_type;
    Mutibility is_mut;
    LetDecl(string name_, RealType_ptr let_type_, Mutibility is_mut_)
        : ValueDecl(ValueDeclKind::LetStmt, std::move(name_)), let_type(let_type_), is_mut(is_mut_) {}
};

// 找到 scope 里面的 const_decl
// 找不到返回 nullptr
ConstDecl_ptr find_const_decl(Scope_ptr NowScope, string name);

#endif // DECL_H
