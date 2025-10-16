#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

class Parser {
public:
    // 将分词后的结果传给 Parser
    Parser(Lexer lexer_) : lexer(lexer_) {}
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

#endif // parser.h