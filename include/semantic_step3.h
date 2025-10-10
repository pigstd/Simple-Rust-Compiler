#ifndef SEMANTIC_STEP3_H
#define SEMANTIC_STEP3_H

/*
第三步我想干的事情：
常量求值
处理出所有 let 的 type（除了数组大小）
常量表达式（数组大小，repeat array 的 size）
控制流分析
*/

#include "lexer.h"
#include "ast.h"
#include "semantic_step1.h"
#include "visitor.h"
#include <bitset>
#include <cstddef>
#include <string>
#include <memory>
using std::string;


struct ConstValue;

struct AnyInt_ConstValue;
struct I32_ConstValue;
struct U32_ConstValue;
struct Isize_ConstValue;
struct Usize_ConstValue;
struct Bool_ConstValue;
struct Char_ConstValue;
struct Unit_ConstValue;
// 只考虑这些 const value

using ConstValue_ptr = std::shared_ptr<ConstValue>;

using U32_ConstValue_ptr = std::shared_ptr<U32_ConstValue>;
using Usize_ConstValue_ptr = std::shared_ptr<Usize_ConstValue>;
using I32_ConstValue_ptr = std::shared_ptr<I32_ConstValue>;
using Isize_ConstValue_ptr = std::shared_ptr<Isize_ConstValue>;
using AnyInt_ConstValue_ptr = std::shared_ptr<AnyInt_ConstValue>;
using Bool_ConstValue_ptr = std::shared_ptr<Bool_ConstValue>;
using Char_ConstValue_ptr = std::shared_ptr<Char_ConstValue>;
using Unit_ConstValue_ptr = std::shared_ptr<Unit_ConstValue>;

enum class ConstValueKind {
    ANYINT,
    // ANYINT 的作用：字面量可以是任意整数
    I32,
    U32,
    ISIZE,
    USIZE,
    BOOL,
    CHAR,
    UNIT,
    ARRAY,
    // const 的 struct enum str string 好像数据没有，我自己也搞不好
    // 写起来也有点玉玉症的，那就不加了，哈哈
};

string const_value_kind_to_string(ConstValueKind kind);

struct ConstValue {
    ConstValueKind kind;
    virtual ~ConstValue() = default;
    ConstValue(ConstValueKind kind_) : kind(kind_) {}
};

struct AnyInt_ConstValue : public ConstValue {
    long long value; // 这里用 long long 来存储
    AnyInt_ConstValue(long long value_) : ConstValue(ConstValueKind::ANYINT), value(value_) {}
};
struct I32_ConstValue : public ConstValue {
    int value;
    I32_ConstValue(int value_) : ConstValue(ConstValueKind::I32), value(value_) {}
};
struct U32_ConstValue : public ConstValue {
    unsigned int value;
    U32_ConstValue(unsigned int value_) : ConstValue(ConstValueKind::U32), value(value_) {}
};
struct Isize_ConstValue : public ConstValue {
    int value;
    // 只考虑 32 位系统
    // 所以就是 int
    Isize_ConstValue(int value_) : ConstValue(ConstValueKind::ISIZE), value(value_) {}
};
struct Usize_ConstValue : public ConstValue {
    unsigned int value;
    // 同上
    Usize_ConstValue(unsigned int value_) : ConstValue(ConstValueKind::USIZE), value(value_) {}
};
struct Bool_ConstValue : public ConstValue {
    bool value;
    Bool_ConstValue(bool value_) : ConstValue(ConstValueKind::BOOL), value(value_) {}
};
struct Char_ConstValue : public ConstValue {
    char value;
    Char_ConstValue(char value_) : ConstValue(ConstValueKind::CHAR), value(value_) {}
};
struct Unit_ConstValue : public ConstValue {
    Unit_ConstValue() : ConstValue(ConstValueKind::UNIT) {}
};
struct Array_ConstValue : public ConstValue {
    vector<ConstValue_ptr> elements;
    Array_ConstValue(const vector<ConstValue_ptr> &elements_) : ConstValue(ConstValueKind::ARRAY), elements(elements_) {}
};

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

// 第一遍是遍历整个 ast 树，把所有 const item 的值求出来
// 如果 const item 是数组，那么将这个数组的常量表达式的值也求出来
// 并且存入 const_expr_to_size_map
// 之后对于所有的 const expr queue 里面的内容 (数组大小，repeat array 的 size)
// 每个用这个 Visitor 去 visitor 那个节点的子树即可。
// 这样就能把数组大小，repeat array 的 size 求出来
struct ConstItemVisitor : public AST_Walker {
    // 将字面量转化为 ConstValue
    ConstValue_ptr parse_literal_token_to_const_value(LiteralType type, string value);
    // 求 一元表达式的值
    ConstValue_ptr calc_const_unary_expr(Unary_Operator OP, ConstValue_ptr right);
    // 求 二元表达式的值
    ConstValue_ptr calc_const_binary_expr(Binary_Operator OP, ConstValue_ptr left, ConstValue_ptr right);
    // 将 anyint 转化为目标类型的 const value
    ConstValue_ptr cast_anyint_const_to_target_type(AnyInt_ConstValue_ptr anyint_value, ConstValueKind target_kind);
    // 将 value 转化为 target_type 的 const value
    ConstValue_ptr const_cast_to_realtype(ConstValue_ptr value, RealType_ptr target_type);
    // 解析 size 的大小，只支持 usize 和 anyint
    size_t calc_const_array_size(ConstValue_ptr size_value);
    // 是否需要计算
    // 如果是 const item 的值，把 is_need_to_calculate 设为 true 去计算
    bool is_need_to_calculate;
    // 如果 is_need_to_calculate 是 true
    // 那么 visit 完把求出来的值记录在 const_value 里面
    // 然后上一层从 const_value 里面取出来
    // 如果不是常量表达式，直接 CE
    ConstValue_ptr const_value;

    // 需要存 node_scope_map 来找 const_decl
    map<size_t, Scope_ptr> &node_scope_map;
    
    // 需要存 const_value_map 来找 const 的值
    map<ConstDecl_ptr, ConstValue_ptr> &const_value_map;

    map<size_t, RealType_ptr> &type_map;
    map<size_t, size_t> &const_expr_to_size_map;

    ConstItemVisitor(bool is_need_to_calculate_,
            map<size_t, Scope_ptr> &node_scope_map_,
            map<ConstDecl_ptr, ConstValue_ptr> &const_value_map_,
            map<size_t, RealType_ptr> &type_map_,
            map<size_t, size_t> &const_expr_to_size_map_) :
            is_need_to_calculate(is_need_to_calculate_),
            node_scope_map(node_scope_map_),
            const_value_map(const_value_map_),
            type_map(type_map_),
            const_expr_to_size_map(const_expr_to_size_map_) {}
    virtual ~ConstItemVisitor() = default;
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
// 先 visit 整个 ast 树求出 const item
// 然后对于数组里面要用到的常量表达式，每个用这个 Visitor 去 visitor 那个节点的子树即可。

// 找到 scope 里面的 const_decl
// 找不到返回 nullptr
ConstDecl_ptr find_const_decl(Scope_ptr NowScope, string name);

// 找到 Type_decl
TypeDecl_ptr find_type_decl(Scope_ptr NowScope, string name);

// 控制流分析
// 1. break continue 是否在循环里？
// 2. return break continue 的控制流分析（是否 diverge）

enum class OutcomeType {
    NEXT,
    RETURN,
    BREAK,
    CONTINUE,
    DIVERGE,
};
// OutcomeState: 可能的返回的集合
using OutcomeState = std::bitset<4>;

// 返回这些 status 对应的 OutcomeState
OutcomeState get_outcome_state(vector<OutcomeType> states);

bool has_state(const OutcomeState &states, OutcomeType state);

// 顺序执行的 OutcomeState
OutcomeState sequence_outcome_state(const OutcomeState &first, const OutcomeState &second);
// if else 的 OutcomeState
OutcomeState ifelse_outcome_state(const OutcomeState &Condition, const OutcomeState &if_branch, const OutcomeState &else_branch);
// while 的 OutcomeState
OutcomeState while_outcome_state(const OutcomeState &Condition, const OutcomeState &body);
// loop 的 OutcomeState
OutcomeState loop_outcome_state(const OutcomeState &body);

struct ControlFlowVisitor : public AST_Walker {
    // 这个 visitor 用来做控制流分析
    // 主要是分析 if while loop 的分支是否都返回
    // 以及 return break continue 的 diverge 情况
    size_t loop_depth;
    map<size_t, OutcomeState> &node_outcome_state_map;
    ControlFlowVisitor(map<size_t, OutcomeState> &node_outcome_state_map_) :
        loop_depth(0), node_outcome_state_map(node_outcome_state_map_) {}
    virtual ~ControlFlowVisitor() = default;
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


#endif // SEMANTIC_STEP3_H