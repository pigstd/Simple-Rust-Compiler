#ifndef AST_H
#define AST_H

/*

定义 AST 相关的类，以及 visitor 模式的基类

*/

#include "lexer.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

using std::string;
using std::vector;
using std::pair;
using std::unique_ptr;

struct AST_visitor {
    virtual ~AST_visitor() = default;
};

struct AST_Node {
    virtual ~AST_Node() = default;
    virtual void accept(AST_visitor &v) = 0;
};
struct Expr_Node;
struct Stmt_Node;
struct Item_Node;
struct Type_Node;
struct Pattern_Node;

using Expr_ptr = unique_ptr<Expr_Node>;
using Stmt_ptr = unique_ptr<Stmt_Node>;
using Item_ptr = unique_ptr<Item_Node>;
using Type_ptr = unique_ptr<Type_Node>;
using Pattern_ptr = unique_ptr<Pattern_Node>;

struct Expr_Node : public AST_Node {
    virtual ~Expr_Node() = default;
    virtual void accept(AST_visitor &v) override = 0;
};
struct Stmt_Node : public AST_Node {
    virtual ~Stmt_Node() = default;
    virtual void accept(AST_visitor &v) override = 0;
};
struct Item_Node : public AST_Node {
    virtual ~Item_Node() = default;
    virtual void accept(AST_visitor &v) override = 0;
};
struct Type_Node : public AST_Node {
    virtual ~Type_Node() = default;
    virtual void accept(AST_visitor &v) override = 0;
};
struct Pattern_Node : public AST_Node {
    virtual ~Pattern_Node() = default;
    virtual void accept(AST_visitor &v) override = 0;
};

struct LiteralExpr; // 字面量 1, "hello", 'c', true
struct IdentifierExpr; // 标识符 x, y, foo
struct BinaryExpr; // 二元表达式 a + b
struct UnaryExpr; // 一元表达式 !x
struct CallExpr; // 函数调用表达式 f(x)
struct FieldExpr; // 结构体字段访问表达式 point.x
struct StructExpr; // 结构体构造表达式 point { x: 1, y: 2 }
struct IndexExpr; // 数组索引表达式 arr[0]
struct BlockExpr; // 代码块 { ... }
struct IfExpr; // if 表达式 if cond { ... } else { ... }
struct WhileExpr; // while 表达式 while cond { ... }
struct LoopExpr; // loop 表达式 loop { ... }
struct ReturnExpr; // return 表达式 return expr;
struct BreakExpr; // break 表达式 break;
struct ContinueExpr; // continue 表达式 continue;
struct CastExpr; // 类型转换表达式 expr as Type

struct FnItem; // 函数项 fn foo() { ... }
struct StructItem; // 结构体项 struct Point { x: i32, y: i32 }
struct EnumItem; // 枚举项 enum Color { Red, Green, Blue }
// Enum 只需要考虑最简单的常量枚举即可
struct ImplItem; // impl 块 impl Point { fn new() -> Self { ... } }
struct ConstItem; // 常量项 const MAX: i32 = 100;

struct LetStmt; // let 语句 let x = expr;
struct ExprStmt; // 表达式语句 expr;
struct ItemStmt; // 项目语句 item;

struct PathType; // 类型路径 i32, String, MyStruct ()
struct ArrayType; // 数组类型 [T; n]

// Patterns 只需要考虑 IdentifierPattern 即可
struct IdentifierPattern; // 标识符模式 let x = expr; 的 x


enum class LiteralType {
    NUMBER,
    STRING,
    CHAR,
    BOOL,
    CSTRING
};

struct LiteralExpr : public Expr_Node {
    LiteralType literal_type;
    string value;
    LiteralExpr(LiteralType type, const string &val) : literal_type(type), value(val) {}
    void accept(AST_visitor &v) override;
};

struct IdentifierExpr : public Expr_Node {
    string name;
    IdentifierExpr(const string &name) : name(name) {}
    void accept(AST_visitor &v) override;
};


enum class Binary_Operator {
    ADD, SUB, MUL, DIV, MOD,
    AND, OR, XOR,
    AND_AND, OR_OR,
    EQ, NEQ, LT, GT, LEQ, GEQ,
    SHL, SHR,
    ASSIGN,
    ADD_ASSIGN, SUB_ASSIGN, MUL_ASSIGN, DIV_ASSIGN, MOD_ASSIGN,
    AND_ASSIGN, OR_ASSIGN, XOR_ASSIGN,
    SHL_ASSIGN, SHR_ASSIGN
};
// 不用考虑引用，哈哈
enum class Unary_Operator {
    NEG, NOT
};

struct BinaryExpr : public Expr_Node {
    Binary_Operator op;
    Expr_ptr left;
    Expr_ptr right;
    BinaryExpr(Binary_Operator oper, Expr_ptr lhs, Expr_ptr rhs)
        : op(oper), left(std::move(lhs)), right(std::move(rhs)) {}
    void accept(AST_visitor &v) override;
};

struct UnaryExpr : public Expr_Node {
    Unary_Operator op;
    Expr_ptr right;
    UnaryExpr(Unary_Operator oper, Expr_ptr expr)
        : op(oper), right(std::move(expr)) {}
    void accept(AST_visitor &v) override;
};

struct CallExpr : public Expr_Node {
    Expr_ptr callee;
    vector<Expr_ptr> arguments;
    CallExpr(Expr_ptr callee, vector<Expr_ptr> args)
        : callee(std::move(callee)), arguments(std::move(args)) {}
    void accept(AST_visitor &v) override;
};

struct FieldExpr : public Expr_Node {
    Expr_ptr base;
    string field_name;
    FieldExpr(Expr_ptr base, const string &field)
        : base(std::move(base)), field_name(field) {}
    void accept(AST_visitor &v) override;
};

struct StructExpr : public Expr_Node {
    string struct_name;
    vector<pair<string, Expr_ptr>> fields;
    StructExpr(const string &name, vector<pair<string, Expr_ptr>> flds)
        : struct_name(name), fields(std::move(flds)) {}
    void accept(AST_visitor &v) override;
};

struct IndexExpr : public Expr_Node {
    Expr_ptr base;
    Expr_ptr index;
    IndexExpr(Expr_ptr base, Expr_ptr index)
        : base(std::move(base)), index(std::move(index)) {}
    void accept(AST_visitor &v) override;
};

struct BlockExpr : public Expr_Node {
    vector<Stmt_ptr> statements;
    Stmt_ptr tail_statement; // 可选的尾随表达式，若最后有 ; 那么就是 nullptr
    BlockExpr(vector<Stmt_ptr> stmts, Stmt_ptr tail = nullptr) : statements(std::move(stmts)), tail_statement(std::move(tail)) {}
    void accept(AST_visitor &v) override;
};

struct IfExpr : public Expr_Node {
    Expr_ptr condition;
    // if 分支的大括号内可以是语句
    BlockExpr then_branch;
    BlockExpr else_branch; // 如果没有 else 分支则为 nullptr
    IfExpr(Expr_ptr cond, BlockExpr then_br, BlockExpr else_br = BlockExpr({}))
        : condition(std::move(cond)), then_branch(std::move(then_br)), else_branch(std::move(else_br)) {}
    void accept(AST_visitor &v) override;
};
struct WhileExpr : public Expr_Node {
    Expr_ptr condition;
    BlockExpr body;
    WhileExpr(Expr_ptr cond, BlockExpr body)
        : condition(std::move(cond)), body(std::move(body)) {}
    void accept(AST_visitor &v) override;
};
struct LoopExpr : public Expr_Node {
    BlockExpr body;
    LoopExpr(BlockExpr body) : body(std::move(body)) {}
    void accept(AST_visitor &v) override;
};
struct ReturnExpr : public Expr_Node {
    Expr_ptr return_value; // return 后面可以没有表达式，若没有则为 nullptr
    ReturnExpr(Expr_ptr val = nullptr) : return_value(std::move(val)) {}
    void accept(AST_visitor &v) override;
};
struct BreakExpr : public Expr_Node {
    Expr_ptr break_value; // break 后面可以没有表达式，若没有则为 nullptr
    BreakExpr(Expr_ptr val = nullptr) : break_value(std::move(val)) {}
    void accept(AST_visitor &v) override;
};
struct ContinueExpr : public Expr_Node {
    Expr_ptr continue_value; // continue 后面可以没有表达式，若没有则为 nullptr
    ContinueExpr(Expr_ptr val = nullptr) : continue_value(std::move(val)) {}
    void accept(AST_visitor &v) override;
};
struct CastExpr : public Expr_Node {
    Expr_ptr expr;
    Type_ptr target_type;
    CastExpr(Expr_ptr e, Type_ptr t) : expr(std::move(e)), target_type(std::move(t)) {}
    void accept(AST_visitor &v) override;
};

struct FnItem : public Item_Node {
    string function_name;
    vector<pair<string, Type_ptr>> parameters; // 参数名和参数类型
    Type_ptr return_type; // 返回类型，若是 nullptr 则说明返回 ()
    BlockExpr body;
    FnItem(const string &name, vector<pair<string, Type_ptr>> params, Type_ptr ret_type, BlockExpr body)
        : function_name(name), parameters(std::move(params)), return_type(std::move(ret_type)), body(std::move(body)) {}
    void accept(AST_visitor &v) override;
};
struct StructItem : public Item_Node {
    string struct_name;
    vector<pair<string, Type_ptr>> fields; // 字段名和字段类型
    StructItem(const string &name, vector<pair<string, Type_ptr>> flds)
        : struct_name(name), fields(std::move(flds)) {}
    void accept(AST_visitor &v) override;
};
struct EnumItem : public Item_Node {
    string enum_name;
    vector<string> variants; // 只考虑简单的常量枚举
    EnumItem(const string &name, vector<string> vars)
        : enum_name(name), variants(std::move(vars)) {}
    void accept(AST_visitor &v) override;
};
struct ImplItem : public Item_Node {
    string struct_name; // impl 后面的结构体名
    vector<FnItem> methods; // 只考虑方法
    ImplItem(const string &name, vector<FnItem> mets)
        : struct_name(name), methods(std::move(mets)) {}
    void accept(AST_visitor &v) override;
};
struct ConstItem : public Item_Node {
    string const_name;
    Type_ptr const_type;
    Expr_ptr value;
    ConstItem(const string &name, Type_ptr type, Expr_ptr val)
        : const_name(name), const_type(std::move(type)), value(std::move(val)) {}
    void accept(AST_visitor &v) override;
};

struct LetStmt : public Stmt_Node {
    Pattern_ptr pattern;
    Expr_ptr initializer; // 可以没有初始化表达式，若没有则为 nullptr
    LetStmt(Pattern_ptr pat, Expr_ptr init = nullptr)
        : pattern(std::move(pat)), initializer(std::move(init)) {}
    void accept(AST_visitor &v) override;
};
struct ExprStmt : public Stmt_Node {
    Expr_ptr expr;
    bool is_semi; // 是否以分号结尾
    ExprStmt(Expr_ptr e, bool semi = false) : expr(std::move(e)), is_semi(semi) {}
    void accept(AST_visitor &v) override;
};
struct ItemStmt : public Stmt_Node {
    Item_ptr item;
    ItemStmt(Item_ptr it) : item(std::move(it)) {}
    void accept(AST_visitor &v) override;
};


#endif // AST_H