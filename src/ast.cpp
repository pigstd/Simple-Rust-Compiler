#include "ast.h"
#include "lexer.h"
#include <memory>

void LiteralExpr::accept(AST_visitor &v) { v.visit(*this); }
void IdentifierExpr::accept(AST_visitor &v) { v.visit(*this); }
void BinaryExpr::accept(AST_visitor &v) { v.visit(*this); }
void UnaryExpr::accept(AST_visitor &v) { v.visit(*this); }
void CallExpr::accept(AST_visitor &v) { v.visit(*this); }
void FieldExpr::accept(AST_visitor &v) { v.visit(*this); }
void StructExpr::accept(AST_visitor &v) { v.visit(*this); }
void IndexExpr::accept(AST_visitor &v) { v.visit(*this); }
void BlockExpr::accept(AST_visitor &v) { v.visit(*this); }
void IfExpr::accept(AST_visitor &v) { v.visit(*this); }
void WhileExpr::accept(AST_visitor &v) { v.visit(*this); }
void LoopExpr::accept(AST_visitor &v) { v.visit(*this); }
void ReturnExpr::accept(AST_visitor &v) { v.visit(*this); }
void BreakExpr::accept(AST_visitor &v) { v.visit(*this); }
void ContinueExpr::accept(AST_visitor &v) { v.visit(*this); }
void CastExpr::accept(AST_visitor &v) { v.visit(*this); }
void PathExpr::accept(AST_visitor &v) { v.visit(*this); }
void SelfExpr::accept(AST_visitor &v) { v.visit(*this); }

void FnItem::accept(AST_visitor &v) { v.visit(*this); }
void StructItem::accept(AST_visitor &v) { v.visit(*this); }
void EnumItem::accept(AST_visitor &v) { v.visit(*this); }
void ImplItem::accept(AST_visitor &v) { v.visit(*this); }
void ConstItem::accept(AST_visitor &v) { v.visit(*this); }

void LetStmt::accept(AST_visitor &v) { v.visit(*this); }
void ExprStmt::accept(AST_visitor &v) { v.visit(*this); }
void ItemStmt::accept(AST_visitor &v) { v.visit(*this); }

void PathType::accept(AST_visitor &v) { v.visit(*this); }
void ArrayType::accept(AST_visitor &v) { v.visit(*this); }
void UnitType::accept(AST_visitor &v) { v.visit(*this); }

void IdentifierPattern::accept(AST_visitor &v) { v.visit(*this); }

// AST_Walker 递归遍历子节点

void AST_Walker::visit([[maybe_unused]] LiteralExpr &node) { return; }
void AST_Walker::visit([[maybe_unused]] IdentifierExpr &node) { return; }
// LiteralExpr 和 IdentifierExpr 没有子节点
void AST_Walker::visit(BinaryExpr &node) {
    node.left->accept(*this);
    node.right->accept(*this);
}
void AST_Walker::visit(UnaryExpr &node) {node.right->accept(*this); }
void AST_Walker::visit(CallExpr &node) {
    node.callee->accept(*this);
    for (auto &arg : node.arguments) {
        arg->accept(*this);
    }
}
void AST_Walker::visit(FieldExpr &node) { node.base->accept(*this); }
void AST_Walker::visit(StructExpr &node) {
    for (auto &[name, expr] : node.fields) {
        expr->accept(*this);
    }
}
void AST_Walker::visit(IndexExpr &node) {
    node.base->accept(*this);
    node.index->accept(*this);
}
void AST_Walker::visit(BlockExpr &node) {
    for (auto &stmt : node.statements) {
        stmt->accept(*this);
    }
    if (node.tail_statement) {
        node.tail_statement->accept(*this);
    }
}
void AST_Walker::visit(IfExpr &node) {
    node.condition->accept(*this);
    node.then_branch->accept(*this);
    if (node.else_branch) {
        node.else_branch->accept(*this);
    }
}
void AST_Walker::visit(WhileExpr &node) {
    node.condition->accept(*this);
    node.body->accept(*this);
}
void AST_Walker::visit(LoopExpr &node) { node.body->accept(*this); }
void AST_Walker::visit(ReturnExpr &node) {
    if (node.return_value) {
        node.return_value->accept(*this);
    }
}
void AST_Walker::visit(BreakExpr &node) {
    if (node.break_value) {
        node.break_value->accept(*this);
    }
}
void AST_Walker::visit(ContinueExpr &node) {
    if (node.continue_value) {
        node.continue_value->accept(*this);
    }
}
void AST_Walker::visit(CastExpr &node) {
    node.expr->accept(*this);
    node.target_type->accept(*this);
}
void AST_Walker::visit(PathExpr &node) { node.base->accept(*this); }
void AST_Walker::visit([[maybe_unused]] SelfExpr &node) { return; }
void AST_Walker::visit(FnItem &node) {
    for (auto &[name, type] : node.parameters) {
        type->accept(*this);
    }
    if (node.return_type) {
        node.return_type->accept(*this);
    }
    node.body->accept(*this);
}
void AST_Walker::visit(StructItem &node) {
    for (auto &[name, type] : node.fields) {
        type->accept(*this);
    }
}
void AST_Walker::visit([[maybe_unused]] EnumItem &node) { return; }
void AST_Walker::visit(ImplItem &node) {
    for (auto &item : node.methods) {
        item->accept(*this);
    }
}
void AST_Walker::visit(ConstItem &node) {
    node.const_type->accept(*this);
    node.value->accept(*this);
}
void AST_Walker::visit(LetStmt &node) {
    node.pattern->accept(*this);
    node.initializer->accept(*this);
}
void AST_Walker::visit(ExprStmt &node) { node.expr->accept(*this); }
void AST_Walker::visit(ItemStmt &node) { node.item->accept(*this); }
void AST_Walker::visit([[maybe_unused]] PathType &node) { return; }
void AST_Walker::visit(ArrayType &node) {
    node.element_type->accept(*this);
    node.size_expr->accept(*this);
}
void AST_Walker::visit([[maybe_unused]] UnitType &node) { return; }
void AST_Walker::visit([[maybe_unused]] IdentifierPattern &node) { return; }

vector<Item_ptr> Parser::parse() {
    vector<Item_ptr> items;
    while (lexer.has_more_tokens()) {
        items.push_back(parse_item());
    }
    return items;
}

Item_ptr Parser::parse_item() {
    // 这里只需要考虑 fn, struct, enum, impl, const 五种 item
    Token token = lexer.peek_token();
    if (token.type == Token_type::FN) {
        return parse_fn_item();
    } else if (token.type == Token_type::STRUCT) {
        return parse_struct_item();
    } else if (token.type == Token_type::ENUM) {
        return parse_enum_item();
    } else if (token.type == Token_type::IMPL) {
        return parse_impl_item();
    } else if (token.type == Token_type::CONST) {
        return parse_const_item();
    } else {
        throw string("CE, expected item but got ") + token.value;
    }
}

FnItem_ptr Parser::parse_fn_item() {
    // example :
    // fn function_name(param1: Type1, param2: Type2, ...) -> ReturnType { body }
    lexer.consume_expect_token(Token_type::FN);
    string function_name = lexer.consume_expect_token(Token_type::IDENTIFIER).value;
    lexer.consume_expect_token(Token_type::LEFT_PARENTHESIS);
    fn_reciever_type receiver_type = fn_reciever_type::NO_RECEIVER;
    if (lexer.peek_token().type == Token_type::SELF) {
        receiver_type = fn_reciever_type::SELF;
        lexer.consume_expect_token(Token_type::SELF); // 消费掉 self
        lexer.consume_expect_token(Token_type::COMMA); // 消费掉逗号
    } else if (lexer.peek_token().type == Token_type::REF) {
        lexer.consume_expect_token(Token_type::REF); // 消费掉 &
        if (lexer.peek_token().type == Token_type::MUT) {
            receiver_type = fn_reciever_type::SELF_REF_MUT;
            lexer.consume_expect_token(Token_type::MUT); // 消费掉 mut
        } else {
            receiver_type = fn_reciever_type::SELF_REF;
        }
        lexer.consume_expect_token(Token_type::SELF); // 消费掉 self
        lexer.consume_expect_token(Token_type::COMMA); // 消费掉逗号
    }
    vector<pair<string, Type_ptr>> parameters;
    while(lexer.peek_token().type != Token_type::RIGHT_PARENTHESIS) {
        string param_name = lexer.consume_expect_token(Token_type::IDENTIFIER).value;
        lexer.consume_expect_token(Token_type::COLON);
        Type_ptr param_type = parse_type();
        parameters.emplace_back(param_name, std::move(param_type));
        if (lexer.peek_token().type == Token_type::COMMA) {
            lexer.consume_token(); // 消费掉逗号
        } else {
            break; // 没有逗号说明参数列表结束
        }
    }
    lexer.consume_expect_token(Token_type::RIGHT_PARENTHESIS);
    Type_ptr return_type = nullptr;
    if (lexer.peek_token().type == Token_type::ARROW) {
        lexer.consume_token();
        return_type = parse_type();
    }
    BlockExpr_ptr body = parse_block_expression();
    return std::make_unique<FnItem>(function_name, receiver_type,
        std::move(parameters), std::move(return_type), std::move(body));
}

StructItem_ptr Parser::parse_struct_item() {
    // example :
    // struct StructName { field1: Type1, field2: Type2, ... }
    lexer.consume_expect_token(Token_type::STRUCT);
    string struct_name = lexer.consume_expect_token(Token_type::IDENTIFIER).value;
    lexer.consume_expect_token(Token_type::LEFT_BRACE);
    vector<pair<string, Type_ptr>> fields;
    while(lexer.peek_token().type != Token_type::RIGHT_BRACE) {
        string field_name = lexer.consume_expect_token(Token_type::IDENTIFIER).value;
        lexer.consume_expect_token(Token_type::COLON);
        Type_ptr field_type = parse_type();
        fields.emplace_back(field_name, std::move(field_type));
        if (lexer.peek_token().type == Token_type::COMMA) {
            lexer.consume_token(); // 消费掉逗号
        } else {
            break; // 没有逗号说明字段列表结束
        }
    }
    lexer.consume_expect_token(Token_type::RIGHT_BRACE);
    return std::make_unique<StructItem>(struct_name, std::move(fields));
}
EnumItem_ptr Parser::parse_enum_item() {
    // example :
    // enum EnumName { Variant1, Variant2, ... }
    lexer.consume_expect_token(Token_type::ENUM);
    string enum_name = lexer.consume_expect_token(Token_type::IDENTIFIER).value;
    lexer.consume_expect_token(Token_type::LEFT_BRACE);
    vector<string> variants;
    while(lexer.peek_token().type != Token_type::RIGHT_BRACE) {
        string variant_name = lexer.consume_expect_token(Token_type::IDENTIFIER).value;
        variants.push_back(variant_name);
        if (lexer.peek_token().type == Token_type::COMMA) {
            lexer.consume_token(); // 消费掉逗号
        } else {
            break; // 没有逗号说明变体列表结束
        }
    }
    lexer.consume_expect_token(Token_type::RIGHT_BRACE);
    return std::make_unique<EnumItem>(enum_name, std::move(variants));
}
ImplItem_ptr Parser::parse_impl_item() {
    // example :
    // impl StructName { fn method1(...) { ... } fn method2(...) { ... } ... }
    lexer.consume_expect_token(Token_type::IMPL);
    string struct_name = lexer.consume_expect_token(Token_type::IDENTIFIER).value;
    lexer.consume_expect_token(Token_type::LEFT_BRACE);
    vector<Item_ptr> methods;
    while(lexer.peek_token().type != Token_type::RIGHT_BRACE) {
        // 只能有函数方法
        methods.push_back(parse_fn_item());
    }
    lexer.consume_expect_token(Token_type::RIGHT_BRACE);
    return std::make_unique<ImplItem>(struct_name, std::move(methods));
}

ConstItem_ptr Parser::parse_const_item() {
    // example :
    // const CONST_NAME: Type = value;
    lexer.consume_expect_token(Token_type::CONST);
    string const_name = lexer.consume_expect_token(Token_type::IDENTIFIER).value;
    lexer.consume_expect_token(Token_type::COLON);
    Type_ptr const_type = parse_type();
    lexer.consume_expect_token(Token_type::EQUAL);
    Expr_ptr value = parse_expression();
    lexer.consume_expect_token(Token_type::SEMICOLON);
    return std::make_unique<ConstItem>(const_name, std::move(const_type), std::move(value));
}

Pattern_ptr Parser::parse_pattern() {
    // example :
    // identifier, mut identifier, &mut identifier, &identifier
    ReferenceType ref_type = ReferenceType::NO_REF;
    if (lexer.peek_token().type == Token_type::AMPERSAND) {
        lexer.consume_expect_token(Token_type::AMPERSAND);
        ref_type = ReferenceType::REF;
    }
    Mutibility mut_type = Mutibility::IMMUTABLE;
    if (lexer.peek_token().type == Token_type::MUT) {
        lexer.consume_expect_token(Token_type::MUT);
        mut_type = Mutibility::MUTABLE;
    }
    string identifier_name = lexer.consume_expect_token(Token_type::IDENTIFIER).value;
    return std::make_unique<IdentifierPattern>(identifier_name, mut_type, ref_type);
}

LetStmt_ptr Parser::parse_let_statement() {
    // example :
    // let pattern : Type (= initializer);
    // 可以没有定义
    lexer.consume_expect_token(Token_type::LET);
    Pattern_ptr pattern = parse_pattern();
    lexer.consume_expect_token(Token_type::COLON);
    Type_ptr type = parse_type();
    Expr_ptr initializer = nullptr;
    if (lexer.peek_token().type == Token_type::EQUAL) {
        lexer.consume_expect_token(Token_type::EQUAL);
        initializer = parse_expression();
    }
    lexer.consume_expect_token(Token_type::SEMICOLON);
    return std::make_unique<LetStmt>(std::move(pattern), std::move(initializer));
}
ExprStmt_ptr Parser::parse_expr_statement() {
    // example :
    // expression;
    Expr_ptr expr = parse_expression();
    bool is_semi = false;
    if (lexer.peek_token().type == Token_type::SEMICOLON) {
        lexer.consume_expect_token(Token_type::SEMICOLON);
        is_semi = true;
    }
    return std::make_unique<ExprStmt>(std::move(expr), is_semi);
}
ItemStmt_ptr Parser::parse_item_statement() {
    // example :
    // item
    Item_ptr item = parse_item();
    return std::make_unique<ItemStmt>(std::move(item));
}

Stmt_ptr Parser::parse_statement() {
    // 这里只需要考虑 let, expression, item
    Token token = lexer.peek_token();
    if (token.type == Token_type::LET) {
        return parse_let_statement();
    } else if (token.type == Token_type::FN ||
               token.type == Token_type::STRUCT ||
               token.type == Token_type::ENUM ||
               token.type == Token_type::IMPL ||
               token.type == Token_type::CONST) {
        return parse_item_statement();
    } else {
        return parse_expr_statement();
    }
}

BlockExpr_ptr Parser::parse_block_expression() {
    // example :
    // { statement1; statement2; ... tail_statement }
    lexer.consume_expect_token(Token_type::LEFT_BRACE);
    vector<Stmt_ptr> statements;
    Stmt_ptr tail_statement = nullptr;
    bool tail_statement_has_semi = true;
    while (lexer.peek_token().type != Token_type::RIGHT_BRACE) {
        // 跳过多余的分号
        if (lexer.peek_token().type == Token_type::SEMICOLON) {
            lexer.consume_expect_token(Token_type::SEMICOLON);
            continue;
        }
        // 只有 expression 需要考虑有没有分号
        bool has_semi = true;
        Stmt_ptr stmt = parse_statement();
        if (auto expr_stmt = dynamic_cast<ExprStmt*>(stmt.get())) {
            if (!expr_stmt->is_semi) { has_semi = false; }
        }
        if (tail_statement) {
            if (!tail_statement_has_semi) {
                // 如果是 if, loop，那么打上标记，返回值一定是 ()
                // 如果 While，不用管
                // 否则 CE
                if (auto if_expr = dynamic_cast<IfExpr*>(tail_statement.get())) {
                    if_expr->must_return_unit = true;
                } else if (auto loop_expr = dynamic_cast<LoopExpr*>(tail_statement.get())) {
                    loop_expr->must_return_unit = true;
                } else if (auto while_expr = dynamic_cast<WhileExpr*>(tail_statement.get())) {
                    // do nothing
                    // while 一定返回 ()
                } else {
                    throw string("CE, unexpected statement after expression without semicolon");
                }
            }
            statements.push_back(std::move(tail_statement));
        }
        tail_statement = std::move(stmt);
        tail_statement_has_semi = has_semi;
    }
    lexer.consume_expect_token(Token_type::RIGHT_BRACE);
    if (tail_statement_has_semi) {
        if (tail_statement) {
            statements.push_back(std::move(tail_statement));
        }
        tail_statement = nullptr;
        return std::make_unique<BlockExpr>(std::move(statements), nullptr);
    }
    else {
        return std::make_unique<BlockExpr>(std::move(statements), std::move(tail_statement));
    }
}


IfExpr_ptr Parser::parse_if_expression() {
    // example :
    // if (condition) { then_branch } (else { else_branch })?
    // 有一个特殊情况： else 后面如果跟的是 if 就不用大括号
    lexer.consume_expect_token(Token_type::IF);
    lexer.consume_expect_token(Token_type::LEFT_PARENTHESIS);
    Expr_ptr condition = parse_expression();
    lexer.consume_expect_token(Token_type::RIGHT_PARENTHESIS);
    Expr_ptr then_branch = parse_block_expression();
    Expr_ptr else_branch = nullptr;
    if (lexer.peek_token().type == Token_type::ELSE) {
        lexer.consume_expect_token(Token_type::ELSE);
        // else 可能后面跟 if 也可能跟 {
        if (lexer.peek_token().type == Token_type::IF) { else_branch = parse_if_expression(); }
        else  {else_branch = parse_block_expression(); }
    }
    return std::make_unique<IfExpr>(std::move(condition), std::move(then_branch), std::move(else_branch));
}
WhileExpr_ptr Parser::parse_while_expression() {
    // example :
    // while (condition) { body }
    lexer.consume_expect_token(Token_type::WHILE);
    lexer.consume_expect_token(Token_type::LEFT_PARENTHESIS);
    Expr_ptr condition = parse_expression();
    lexer.consume_expect_token(Token_type::RIGHT_PARENTHESIS);
    Expr_ptr body = parse_block_expression();
    return std::make_unique<WhileExpr>(std::move(condition), std::move(body));
}
LoopExpr_ptr Parser::parse_loop_expression() {
    // example :
    // loop { body }
    lexer.consume_expect_token(Token_type::LOOP);
    Expr_ptr body = parse_block_expression();
    return std::make_unique<LoopExpr>(std::move(body));
}
Type_ptr Parser::parse_type() {
    // example :
    // identifier, [Type; size_expr], (), &Type, &mut Type
    ReferenceType ref_type = ReferenceType::NO_REF;
    if (lexer.peek_token().type == Token_type::AMPERSAND) {
        lexer.consume_expect_token(Token_type::AMPERSAND);
        ref_type = ReferenceType::REF;
    }
    Mutibility mut_type = Mutibility::IMMUTABLE;
    if (lexer.peek_token().type == Token_type::MUT) {
        lexer.consume_expect_token(Token_type::MUT);
        mut_type = Mutibility::MUTABLE;
    }
    if (lexer.peek_token().type == Token_type::LEFT_BRACKET) {
        // [Type, size_expr]
        lexer.consume_expect_token(Token_type::LEFT_BRACKET);
        Type_ptr element_type = parse_type();
        lexer.consume_expect_token(Token_type::SEMICOLON);
        Expr_ptr size_expr = parse_expression();
        lexer.consume_expect_token(Token_type::RIGHT_BRACKET);
        return std::make_unique<ArrayType>(std::move(element_type), std::move(size_expr), mut_type, ref_type);
    }
    else if (lexer.peek_token().type == Token_type::LEFT_PARENTHESIS) {
        // ()
        lexer.consume_expect_token(Token_type::LEFT_PARENTHESIS);
        lexer.consume_expect_token(Token_type::RIGHT_PARENTHESIS);
        return std::make_unique<UnitType>(mut_type, ref_type);
    }
    else {
        // identifier
        string type_name = lexer.consume_expect_token(Token_type::IDENTIFIER).value;
        return std::make_unique<PathType>(type_name, mut_type, ref_type);
    }
}

// 递归下降解析表达式，基于 Pratt 解析器
// 先考虑掉 loop if while return break continue 这些表达式，然后再使用 Pratt 解析器
// TO DO !!!
Expr_ptr Parser::parse_expression(int rbp) {

}

// 处理前缀表达式
// TO DO !!!
Expr_ptr Parser::nud(Token token) {

}
// 处理中缀表达式
// TO DO !!!
Expr_ptr Parser::led(Token token, Expr_ptr left) {

}
