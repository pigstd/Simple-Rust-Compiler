#ifndef AST_H
#define AST_H

/*

定义 AST 相关的类

*/

#include "lexer.h"
#include <cfloat>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using std::string;
using std::vector;
using std::pair;
using std::unique_ptr;

struct AST_visitor;

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
struct IfExpr; // if 表达式 if (cond) { ... } else { ... }
struct WhileExpr; // while 表达式 while (cond) { ... }
struct LoopExpr; // loop 表达式 loop { ... }
struct ReturnExpr; // return 表达式 return expr;
struct BreakExpr; // break 表达式 break;
struct ContinueExpr; // continue 表达式 continue;
struct CastExpr; // 类型转换表达式 expr as Type
struct PathExpr; // 路径表达式 std::io::Result
struct SelfExpr; // self 表达式 只有 self 本身
struct UnitExpr; // 单元表达式 ()
struct ArrayExpr; // 数组表达式 [expr1, expr2, ...]
struct RepeatArrayExpr; // 重复数组表达式 [expr; n]

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
struct UnitType; // 单元类型 ()

// Patterns 只需要考虑 IdentifierPattern 即可
struct IdentifierPattern; // 标识符模式 let x = expr; 的 x

using LiteralExpr_ptr = unique_ptr<LiteralExpr>;
using IdentifierExpr_ptr = unique_ptr<IdentifierExpr>;
using BinaryExpr_ptr = unique_ptr<BinaryExpr>;
using UnaryExpr_ptr = unique_ptr<UnaryExpr>;
using CallExpr_ptr = unique_ptr<CallExpr>;
using FieldExpr_ptr = unique_ptr<FieldExpr>;
using StructExpr_ptr = unique_ptr<StructExpr>;
using IndexExpr_ptr = unique_ptr<IndexExpr>;
using BlockExpr_ptr = unique_ptr<BlockExpr>;
using IfExpr_ptr = unique_ptr<IfExpr>;
using WhileExpr_ptr = unique_ptr<WhileExpr>;
using LoopExpr_ptr = unique_ptr<LoopExpr>;
using ReturnExpr_ptr = unique_ptr<ReturnExpr>;
using BreakExpr_ptr = unique_ptr<BreakExpr>;
using ContinueExpr_ptr = unique_ptr<ContinueExpr>;
using CastExpr_ptr = unique_ptr<CastExpr>;
using PathExpr_ptr = unique_ptr<PathExpr>;
using SelfExpr_ptr = unique_ptr<SelfExpr>;
using UnitExpr_ptr = unique_ptr<UnitExpr>;
using ArrayExpr_ptr = unique_ptr<ArrayExpr>;
using RepeatArrayExpr_ptr = unique_ptr<RepeatArrayExpr>;
using FnItem_ptr = unique_ptr<FnItem>;
using StructItem_ptr = unique_ptr<StructItem>;
using EnumItem_ptr = unique_ptr<EnumItem>;
using ImplItem_ptr = unique_ptr<ImplItem>;
using ConstItem_ptr = unique_ptr<ConstItem>;
using LetStmt_ptr = unique_ptr<LetStmt>;
using ExprStmt_ptr = unique_ptr<ExprStmt>;
using ItemStmt_ptr = unique_ptr<ItemStmt>;
using PathType_ptr = unique_ptr<PathType>;
using ArrayType_ptr = unique_ptr<ArrayType>;
using UnitType_ptr = unique_ptr<UnitType>;
using IdentifierPattern_ptr = unique_ptr<IdentifierPattern>;

enum class LiteralType {
    NUMBER,
    STRING,
    CHAR,
    BOOL,
    // CSTRING
    // remark : 我前面忘记考虑 CSTRING 了
    // 先写完 Parser 再说，这个 CSTRING 后面在加，主要是要在 lexer 里面加内容
};
string literal_type_to_string(LiteralType type);

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
string binary_operator_to_string(Binary_Operator op);

// 妈的，要考虑引用
enum class Unary_Operator {
    NEG, NOT, REF, REF_MUT, DEREF
};
string unary_operator_to_string(Unary_Operator op);

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

// If While Loop 全部当成表达式，返回值可以是值也可以是 ()
// 有个特殊情况，如果返回值是 () 的时候，后面可以不加分号，但是如果返回值不是 () 的时候，必须加分号
// 因此，如果 if/While/Loop 后面没有分号，那么一定得返回 ()
// 特别的 While 一定返回的是 ()，所以没关系
// 这个在 ast 里面需要记录，是否一定返回 ()
struct IfExpr : public Expr_Node {
    Expr_ptr condition;
    // if 分支的大括号内可以是语句
    Expr_ptr then_branch;
    Expr_ptr else_branch; // 如果没有 else 分支则为 nullptr
    bool must_return_unit;
    IfExpr(Expr_ptr cond, Expr_ptr then_br, Expr_ptr else_br = nullptr)
        : condition(std::move(cond)), then_branch(std::move(then_br)), else_branch(std::move(else_br)), must_return_unit(false) {}
    void accept(AST_visitor &v) override;
};
struct WhileExpr : public Expr_Node {
    Expr_ptr condition;
    Expr_ptr body;
    WhileExpr(Expr_ptr cond, Expr_ptr body)
        : condition(std::move(cond)), body(std::move(body)) {}
    void accept(AST_visitor &v) override;
};
struct LoopExpr : public Expr_Node {
    Expr_ptr body;
    bool must_return_unit;
    LoopExpr(Expr_ptr body) : body(std::move(body)), must_return_unit(false) {}
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
// base::name
struct PathExpr : public Expr_Node {
    Expr_ptr base; // 可以是另一个 PathExpr
    string name;
    PathExpr(Expr_ptr b, const string &n) : base(std::move(b)), name(n) {}
    void accept(AST_visitor &v) override;
};
struct SelfExpr : public Expr_Node {
    void accept(AST_visitor &v) override;
};
struct UnitExpr : public Expr_Node {
    void accept(AST_visitor &v) override;
};
struct RepeatArrayExpr : public Expr_Node {
    Expr_ptr element;
    Expr_ptr size; // 数组大小必须是一个常量 但是也可以是常量表达式
    RepeatArrayExpr(Expr_ptr elem, Expr_ptr sz) : element(std::move(elem)), size(std::move(sz)) {}
    void accept(AST_visitor &v) override;
};
struct ArrayExpr : public Expr_Node {
    vector<Expr_ptr> elements;
    ArrayExpr(vector<Expr_ptr> elems) : elements(std::move(elems)) {}
    void accept(AST_visitor &v) override;
};

enum class fn_reciever_type {
    NO_RECEIVER,
    SELF,
    SELF_REF,
    SELF_REF_MUT
};
string fn_reciever_type_to_string(fn_reciever_type type);

struct FnItem : public Item_Node {
    string function_name;
    fn_reciever_type receiver_type;
    vector<pair<Pattern_ptr, Type_ptr>> parameters; // 参数名和参数类型
    Type_ptr return_type; // 返回类型，若是 nullptr 则说明返回 ()
    Expr_ptr body;
    FnItem(const string &name, fn_reciever_type recv_type, vector<pair<Pattern_ptr, Type_ptr>> params, Type_ptr ret_type, Expr_ptr body)
        : function_name(name), receiver_type(recv_type), parameters(std::move(params)), return_type(std::move(ret_type)), body(std::move(body)) {}
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
    vector<Item_ptr> methods; // 只考虑方法
    ImplItem(const string &name, vector<Item_ptr> mets)
        : struct_name(name), methods(std::move(mets)) {}
    void accept(AST_visitor &v) override;
};
struct ConstItem : public Item_Node {
    string const_name;
    // remark : 貌似不用口考虑 const 的类型推导
    // 直接要求必须有类型
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

enum class Mutibility {
    IMMUTABLE,
    MUTABLE
};
enum class ReferenceType {
    NO_REF,
    REF
};
string mutibility_to_string(Mutibility mut);
string reference_type_to_string(ReferenceType ref);

struct PathType : public Type_Node {
    string name;
    Mutibility is_mut;
    ReferenceType is_ref;
    PathType(const string &name, Mutibility mut, ReferenceType ref) : name(name), is_mut(mut), is_ref(ref) {}
    void accept(AST_visitor &v) override;
};
struct ArrayType : public Type_Node {
    Type_ptr element_type;
    Expr_ptr size_expr; // 数组大小必须是一个常量 但是也可以是常量表达式
    Mutibility is_mut;
    ReferenceType is_ref;
    ArrayType(Type_ptr elem_type, Expr_ptr sz, Mutibility mut, ReferenceType ref)
        : element_type(std::move(elem_type)), size_expr(std::move(sz)), is_mut(mut), is_ref(ref) {}
    void accept(AST_visitor &v) override;
};
struct UnitType : public Type_Node {
    Mutibility is_mut;
    ReferenceType is_ref;
    UnitType(Mutibility mut, ReferenceType ref) : is_mut(mut), is_ref(ref) {}
    void accept(AST_visitor &v) override;
};

struct IdentifierPattern : public Pattern_Node {
    string name;
    Mutibility is_mut;
    ReferenceType is_ref;
    IdentifierPattern(const string &name, Mutibility mut, ReferenceType ref) : name(name), is_mut(mut), is_ref(ref) {}
    void accept(AST_visitor &v) override;
};

class Parser {
public:
    // 将分词后的结果传给 Parser
    Parser(Lexer lexer) : lexer(lexer) {}
    ~Parser() = default;
    vector<Item_ptr> parse();
private:
    Lexer lexer;
    Item_ptr parse_item();
    FnItem_ptr parse_fn_item();
    StructItem_ptr parse_struct_item();
    EnumItem_ptr parse_enum_item();
    ImplItem_ptr parse_impl_item();
    ConstItem_ptr parse_const_item();
    Stmt_ptr parse_statement();
    LetStmt_ptr parse_let_statement();
    ExprStmt_ptr parse_expr_statement();
    ItemStmt_ptr parse_item_statement();
    Pattern_ptr parse_pattern();
    IfExpr_ptr parse_if_expression();
    WhileExpr_ptr parse_while_expression();
    LoopExpr_ptr parse_loop_expression();
    BlockExpr_ptr parse_block_expression();
    Type_ptr parse_type();
    Expr_ptr parse_expression(int rbp = 0);
    Expr_ptr nud(Token token);
    Expr_ptr led(Token token, Expr_ptr left);
    int get_lbp(Token_type type);
    int get_rbp(Token_type type);
    int get_nbp(Token_type type);
    // nbp lbp rbp
    std::tuple<int, int, int> get_binding_power(Token_type type);
    // 处理表达式
};

#endif // AST_H