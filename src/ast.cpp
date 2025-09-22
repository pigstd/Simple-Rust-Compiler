#include "ast.h"
#include "lexer.h"
#include "visitor.h"
#include <memory>
#include <tuple>

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
void UnitExpr::accept(AST_visitor &v) { v.visit(*this); }
void ArrayExpr::accept(AST_visitor &v) { v.visit(*this); }
void RepeatArrayExpr::accept(AST_visitor &v) { v.visit(*this); }

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
Expr_ptr Parser::parse_expression(int rbp) {
    Token token = lexer.peek_token();
    Expr_ptr left = nud(token);
    while (lexer.has_more_tokens() && rbp < get_lbp(lexer.peek_token().type)) {
        token = lexer.consume_token();
        left = led(token, std::move(left));
    }
    return left;
}

Expr_ptr Parser::nud(Token token) {
    if (token.type == Token_type::IF) {
        return parse_if_expression();
    } else if (token.type == Token_type::WHILE) {
        return parse_while_expression();
    } else if (token.type == Token_type::LOOP) {
        return parse_loop_expression();
    } else if (token.type == Token_type::LEFT_BRACE) {
        return parse_block_expression();
    } else if (token.type == Token_type::LEFT_PARENTHESIS) {
        lexer.consume_expect_token(Token_type::LEFT_PARENTHESIS);
        if (lexer.peek_token().type == Token_type::RIGHT_PARENTHESIS) {
            // ()
            lexer.consume_expect_token(Token_type::RIGHT_PARENTHESIS);
            return std::make_unique<UnitExpr>();
        } else {
            // (expr)
            Expr_ptr expr = parse_expression();
            lexer.consume_expect_token(Token_type::RIGHT_PARENTHESIS);
            return expr;
        }
    } else if (token.type == Token_type::LEFT_BRACKET) {
        // [expr1, expr2, ...] or [expr; n]
        lexer.consume_expect_token(Token_type::LEFT_BRACKET);
        Expr_ptr first_expr = parse_expression();
        if (lexer.peek_token().type == Token_type::RIGHT_BRACKET) {
            // []
            lexer.consume_expect_token(Token_type::RIGHT_BRACKET);
            return std::make_unique<ArrayExpr>(std::vector<Expr_ptr>{});
        } else {
            if (lexer.peek_token().type == Token_type::SEMICOLON) {
                // repeat array [expr; n]
                lexer.consume_expect_token(Token_type::SEMICOLON);
                Expr_ptr size_expr = parse_expression();
                lexer.consume_expect_token(Token_type::RIGHT_BRACKET);
                return std::make_unique<RepeatArrayExpr>(std::move(first_expr), std::move(size_expr));
            } else {
                // normal array [expr1, expr2, ...(,)]
                std::vector<Expr_ptr> elements; elements.push_back(std::move(first_expr));
                while (lexer.peek_token().type == Token_type::COMMA) {
                    lexer.consume_expect_token(Token_type::COMMA);
                    if (lexer.peek_token().type == Token_type::RIGHT_BRACKET) {
                        break; // 允许最后一个元素后面有逗号
                    }
                    elements.push_back(parse_expression());
                }
                lexer.consume_expect_token(Token_type::RIGHT_BRACKET);
                return std::make_unique<ArrayExpr>(std::move(elements));
            }
        }
    } else if (token.type == Token_type::IDENTIFIER) {
        lexer.consume_expect_token(Token_type::IDENTIFIER);
        return std::make_unique<IdentifierExpr>(token.value);
    } else if (token.type == Token_type::SELF) {
        lexer.consume_expect_token(Token_type::SELF);
        return std::make_unique<SelfExpr>();
    } else if (token.type == Token_type::NUMBER) {
        lexer.consume_expect_token(Token_type::NUMBER);
        return std::make_unique<LiteralExpr>(LiteralType::NUMBER, token.value);
    } else if (token.type == Token_type::TRUE || 
               token.type == Token_type::FALSE) {
        lexer.consume_token();
        return std::make_unique<LiteralExpr>(LiteralType::BOOL, token.value);
    } else if (token.type == Token_type::STRING) {
        lexer.consume_expect_token(Token_type::STRING);
        return std::make_unique<LiteralExpr>(LiteralType::STRING, token.value);
    } else if (token.type == Token_type::CHAR) {
        lexer.consume_expect_token(Token_type::CHAR);
        return std::make_unique<LiteralExpr>(LiteralType::CHAR, token.value);
    } else if (token.type == Token_type::MINUS) {
        lexer.consume_expect_token(Token_type::MINUS);
        Expr_ptr right = parse_expression(get_nbp(Token_type::MINUS));
        return std::make_unique<UnaryExpr>(Unary_Operator::NEG, std::move(right));
    } else if (token.type == Token_type::BANG) {
        lexer.consume_expect_token(Token_type::BANG);
        Expr_ptr right = parse_expression(get_nbp(Token_type::BANG));
        return std::make_unique<UnaryExpr>(Unary_Operator::NOT, std::move(right));
    } else if (token.type == Token_type::AMPERSAND) {
        lexer.consume_expect_token(Token_type::AMPERSAND);
        if (lexer.peek_token().type == Token_type::MUT) {
            lexer.consume_expect_token(Token_type::MUT);
            Expr_ptr right = parse_expression(get_nbp(Token_type::AMPERSAND));
            return std::make_unique<UnaryExpr>(Unary_Operator::REF_MUT, std::move(right));
        } else {
            Expr_ptr right = parse_expression(get_nbp(Token_type::AMPERSAND));
            return std::make_unique<UnaryExpr>(Unary_Operator::REF, std::move(right));
        }
    } else if (token.type == Token_type::STAR) {
        lexer.consume_expect_token(Token_type::STAR);
        Expr_ptr right = parse_expression(get_nbp(Token_type::STAR));
        return std::make_unique<UnaryExpr>(Unary_Operator::DEREF, std::move(right));
    } else {
        throw string("CE in parser nud !!! unexpected token in expression: ") + token.value;
    }
}
// 处理中缀表达式
Expr_ptr Parser::led(Token token, Expr_ptr left) {
    if (token.type == Token_type::PLUS) {
        Expr_ptr right = parse_expression(get_rbp(Token_type::PLUS));
        return std::make_unique<BinaryExpr>(Binary_Operator::ADD, std::move(left), std::move(right));
    } else if (token.type == Token_type::MINUS) {
        Expr_ptr right = parse_expression(get_rbp(Token_type::MINUS));
        return std::make_unique<BinaryExpr>(Binary_Operator::SUB, std::move(left), std::move(right));
    } else if (token.type == Token_type::STAR) {
        Expr_ptr right = parse_expression(get_rbp(Token_type::STAR));
        return std::make_unique<BinaryExpr>(Binary_Operator::MUL, std::move(left), std::move(right));
    } else if (token.type == Token_type::SLASH) {
        Expr_ptr right = parse_expression(get_rbp(Token_type::SLASH));
        return std::make_unique<BinaryExpr>(Binary_Operator::DIV, std::move(left), std::move(right));
    } else if (token.type == Token_type::PERCENT) {
        Expr_ptr right = parse_expression(get_rbp(Token_type::PERCENT));
        return std::make_unique<BinaryExpr>(Binary_Operator::MOD, std::move(left), std::move(right));
    } else if (token.type == Token_type::AMPERSAND) {
        Expr_ptr right = parse_expression(get_rbp(Token_type::AMPERSAND));
        return std::make_unique<BinaryExpr>(Binary_Operator::AND, std::move(left), std::move(right));
    } else if (token.type == Token_type::PIPE) {
        Expr_ptr right = parse_expression(get_rbp(Token_type::PIPE));
        return std::make_unique<BinaryExpr>(Binary_Operator::OR, std::move(left), std::move(right));
    } else if (token.type == Token_type::CARET) {
        Expr_ptr right = parse_expression(get_rbp(Token_type::CARET));
        return std::make_unique<BinaryExpr>(Binary_Operator::XOR, std::move(left), std::move(right));
    } else if (token.type == Token_type::LEFT_SHIFT) {
        Expr_ptr right = parse_expression(get_rbp(Token_type::LEFT_SHIFT));
        return std::make_unique<BinaryExpr>(Binary_Operator::SHL, std::move(left), std::move(right));
    } else if (token.type == Token_type::RIGHT_SHIFT) {
        Expr_ptr right = parse_expression(get_rbp(Token_type::RIGHT_SHIFT));
        return std::make_unique<BinaryExpr>(Binary_Operator::SHR, std::move(left), std::move(right));
    } else if (token.type == Token_type::EQUAL_EQUAL) {
        Expr_ptr right = parse_expression(get_rbp(Token_type::EQUAL_EQUAL));
        return std::make_unique<BinaryExpr>(Binary_Operator::EQ, std::move(left), std::move(right));
    } else if (token.type == Token_type::NOT_EQUAL) {
        Expr_ptr right = parse_expression(get_rbp(Token_type::NOT_EQUAL));
        return std::make_unique<BinaryExpr>(Binary_Operator::NEQ, std::move(left), std::move(right));
    } else if (token.type == Token_type::LESS) {
        Expr_ptr right = parse_expression(get_rbp(Token_type::LESS));
        return std::make_unique<BinaryExpr>(Binary_Operator::LT, std::move(left), std::move(right));
    } else if (token.type == Token_type::LESS_EQUAL) {
        Expr_ptr right = parse_expression(get_rbp(Token_type::LESS_EQUAL));
        return std::make_unique<BinaryExpr>(Binary_Operator::LEQ, std::move(left), std::move(right));
    } else if (token.type == Token_type::GREATER) {
        Expr_ptr right = parse_expression(get_rbp(Token_type::GREATER));
        return std::make_unique<BinaryExpr>(Binary_Operator::GT, std::move(left), std::move(right));
    } else if (token.type == Token_type::GREATER_EQUAL) {
        Expr_ptr right = parse_expression(get_rbp(Token_type::GREATER_EQUAL));
        return std::make_unique<BinaryExpr>(Binary_Operator::GEQ, std::move(left), std::move(right));
    } else if (token.type == Token_type::AMPERSAND_AMPERSAND) {
        Expr_ptr right = parse_expression(get_rbp(Token_type::AMPERSAND_AMPERSAND));
        return std::make_unique<BinaryExpr>(Binary_Operator::AND_AND, std::move(left), std::move(right));
    } else if (token.type == Token_type::PIPE_PIPE) {
        Expr_ptr right = parse_expression(get_rbp(Token_type::PIPE_PIPE));
        return std::make_unique<BinaryExpr>(Binary_Operator::OR_OR, std::move(left), std::move(right));
    } else if (token.type == Token_type::EQUAL) {
        Expr_ptr right = parse_expression(get_rbp(Token_type::EQUAL));
        return std::make_unique<BinaryExpr>(Binary_Operator::ASSIGN, std::move(left), std::move(right));
    } else if (token.type == Token_type::PLUS_EQUAL) {
        Expr_ptr right = parse_expression(get_rbp(Token_type::PLUS_EQUAL));
        return std::make_unique<BinaryExpr>(Binary_Operator::ADD_ASSIGN, std::move(left), std::move(right));
    } else if (token.type == Token_type::MINUS_EQUAL) {
        Expr_ptr right = parse_expression(get_rbp(Token_type::MINUS_EQUAL));
        return std::make_unique<BinaryExpr>(Binary_Operator::SUB_ASSIGN, std::move(left), std::move(right));
    } else if (token.type == Token_type::STAR_EQUAL) {
        Expr_ptr right = parse_expression(get_rbp(Token_type::STAR_EQUAL));
        return std::make_unique<BinaryExpr>(Binary_Operator::MUL_ASSIGN, std::move(left), std::move(right));
    } else if (token.type == Token_type::SLASH_EQUAL) {
        Expr_ptr right = parse_expression(get_rbp(Token_type::SLASH_EQUAL));
        return std::make_unique<BinaryExpr>(Binary_Operator::DIV_ASSIGN, std::move(left), std::move(right));
    } else if (token.type == Token_type::PERCENT_EQUAL) {
        Expr_ptr right = parse_expression(get_rbp(Token_type::PERCENT_EQUAL));
        return std::make_unique<BinaryExpr>(Binary_Operator::MOD_ASSIGN, std::move(left), std::move(right));
    } else if (token.type == Token_type::CARET_EQUAL) {
        Expr_ptr right = parse_expression(get_rbp(Token_type::CARET_EQUAL));
        return std::make_unique<BinaryExpr>(Binary_Operator::XOR_ASSIGN, std::move(left), std::move(right));
    } else if (token.type == Token_type::AMPERSAND_EQUAL) {
        Expr_ptr right = parse_expression(get_rbp(Token_type::AMPERSAND_EQUAL));
        return std::make_unique<BinaryExpr>(Binary_Operator::AND_ASSIGN, std::move(left), std::move(right));
    } else if (token.type == Token_type::PIPE_EQUAL) {
        Expr_ptr right = parse_expression(get_rbp(Token_type::PIPE_EQUAL));
        return std::make_unique<BinaryExpr>(Binary_Operator::OR_ASSIGN, std::move(left), std::move(right));
    } else if (token.type == Token_type::LEFT_SHIFT_EQUAL) {
        Expr_ptr right = parse_expression(get_rbp(Token_type::LEFT_SHIFT_EQUAL));
        return std::make_unique<BinaryExpr>(Binary_Operator::SHL_ASSIGN, std::move(left), std::move(right));
    } else if (token.type == Token_type::RIGHT_SHIFT_EQUAL) {
        Expr_ptr right = parse_expression(get_rbp(Token_type::RIGHT_SHIFT_EQUAL));
        return std::make_unique<BinaryExpr>(Binary_Operator::SHR_ASSIGN, std::move(left), std::move(right));
    } else if (token.type == Token_type::LEFT_PARENTHESIS) {
        // 函数调用
        vector<Expr_ptr> arguments;
        while (lexer.peek_token().type != Token_type::RIGHT_PARENTHESIS) {
            Expr_ptr arg = parse_expression();
            arguments.push_back(std::move(arg));
            if (lexer.peek_token().type == Token_type::COMMA) {
                lexer.consume_expect_token(Token_type::COMMA);
            } else {
                break;
            }
        }
        lexer.consume_expect_token(Token_type::RIGHT_PARENTHESIS);
        return std::make_unique<CallExpr>(std::move(left), std::move(arguments));
    } else if (token.type == Token_type::DOT) {
        // 字段访问
        string field_name = lexer.consume_expect_token(Token_type::IDENTIFIER).value;
        return std::make_unique<FieldExpr>(std::move(left), field_name);
    } else if (token.type == Token_type::LEFT_BRACKET) {
        // 数组下标访问
        Expr_ptr index = parse_expression();
        lexer.consume_expect_token(Token_type::RIGHT_BRACKET);
        return std::make_unique<IndexExpr>(std::move(left), std::move(index));
    } else if (token.type == Token_type::LEFT_BRACE) {
        // struct 初始化
        // example: StructName { field1: value1, field2: value2, ... }
        // left 一定是 identifier expr
        string struct_name;
        if (auto id_expr = dynamic_cast<IdentifierExpr*>(left.get())) {
            struct_name = id_expr->name;
        }
        else {
            throw "CE, struct name must be a identifier!";
        }
        lexer.consume_expect_token(Token_type::LEFT_BRACE);
        vector<pair<string, Expr_ptr>> fields;
        while (lexer.peek_token().type != Token_type::RIGHT_BRACE) {
            string field_name = lexer.consume_expect_token(Token_type::IDENTIFIER).value;
            lexer.consume_expect_token(Token_type::COLON);
            Expr_ptr field_value = parse_expression();
            fields.emplace_back(field_name, std::move(field_value));
            if (lexer.peek_token().type == Token_type::COMMA) {
                lexer.consume_expect_token(Token_type::COMMA);
            } else {
                break;
            }
        }
        lexer.consume_expect_token(Token_type::RIGHT_BRACE);
        return std::make_unique<StructExpr>(struct_name, std::move(fields));

    } else if (token.type == Token_type::AS) {
        // 类型转换
        Type_ptr target_type = parse_type();
        return std::make_unique<CastExpr>(std::move(left), std::move(target_type));
    } else if (token.type == Token_type::COLON_COLON) {
        // 路径表达式
        string path_segment = lexer.consume_expect_token(Token_type::IDENTIFIER).value;
        return std::make_unique<PathExpr>(std::move(left), path_segment);
    } else {
        throw string("CE in parser led !!! unexpected token in expression: ") + token.value;
    }
}


// 返回 [npb, lbp, rbp]
// 从大到小考虑
// -1 表示不能作为前缀 / 中缀
// 左结合： lbp = rbp
// 右结合： rbp = lbp - 1
// 从高到低考虑每一个运算符
// TO DO!!!
std::tuple<int, int, int> get_binding_power(Token_type type) {
    switch (type) {
        case Token_type::LEFT_PARENTHESIS:  // 函数调用
        case Token_type::DOT:               // 字段访问
        case Token_type::LEFT_BRACKET:      // 数组下标访问
        case Token_type::LEFT_BRACE:        // struct 初始化
        case Token_type::AS:                // 类型转换
        case Token_type::COLON_COLON:       // 路径表达式
            return {-1, 100, 100};
        // ( [ 作为前缀的时候不会调用 get_binding_power，所以都可以返回 -1
        case Token_type::STAR:              //  解引用 / 乘法
            return {90, 80, 80};
        case Token_type::MINUS:             //  负号 / 减法
            return {90, 70, 70};
        case Token_type::AMPERSAND:         //  引用 / 按位与
            return {90, 60, 60};
        case Token_type::BANG:              //  逻辑非
            return {90, -1, -1};
        case Token_type::SLASH:            //  除法
        case Token_type::PERCENT:          //  取模
            return {-1, 80, 80};
        case Token_type::PLUS:             //  加法
            return {-1, 70, 70};
        case Token_type::LEFT_SHIFT:       //  左移
        case Token_type::RIGHT_SHIFT:      //  右移
            return {-1, 50, 50};
        case Token_type::CARET:            //  按位异或
            return {-1, 45, 45};
        case Token_type::PIPE:             //  按位或
            return {-1, 40, 40};
        case Token_type::EQUAL_EQUAL:      //  相等
        case Token_type::NOT_EQUAL:        //  不等
        case Token_type::LESS:             //  小于
        case Token_type::LESS_EQUAL:       //  小于等于
        case Token_type::GREATER:          //  大于
        case Token_type::GREATER_EQUAL:    //  大于等于
            return {-1, 35, 35};
        case Token_type::AMPERSAND_AMPERSAND: //  逻辑与
            return {-1, 30, 30};
        case Token_type::PIPE_PIPE:           //  逻辑或
            return {-1, 25, 25};
        case Token_type::EQUAL:               //  赋值
        case Token_type::PLUS_EQUAL:          //  加等
        case Token_type::MINUS_EQUAL:         //  减等
        case Token_type::STAR_EQUAL:          //  乘等
        case Token_type::SLASH_EQUAL:         //  除等
        case Token_type::PERCENT_EQUAL:       //  取模等
        case Token_type::CARET_EQUAL:         //  按位异或等
        case Token_type::AMPERSAND_EQUAL:     //  按位与等
        case Token_type::PIPE_EQUAL:          //  按位或等
        case Token_type::LEFT_SHIFT_EQUAL:    //  左移等
        case Token_type::RIGHT_SHIFT_EQUAL:   //  右移等
            return {-1, 20, 19}; // 右结合
        case Token_type::RIGHT_BRACE:         // }
        case Token_type::RIGHT_PARENTHESIS:   // )
        case Token_type::RIGHT_BRACKET:       // ]
        case Token_type::COMMA:               // ,
        case Token_type::SEMICOLON:           // ;
            return {-1, 0, -1};
            // 不能作为前缀，中缀遇到的时候说明表达式结束了，所以 lbp = 0
        default:
            return {-1, -1, -1};
    }
}

// 中缀表达式的左结合力
int get_lbp(Token_type type) {
    int res = std::get<1>(get_binding_power(type));
    if (res == -1) {
        throw string("CE, unexpected token in infix expression: ") + token_type_to_string(type);
    }
    return res;
}

// 中缀表达式的右结合力
int get_rbp(Token_type type) {
    int res = std::get<2>(get_binding_power(type));
    if (res == -1) {
        throw string("CE, unexpected token in infix expression: ") + token_type_to_string(type);
    }
    return res;
}

// 前缀表达式的结合力
int get_nbp(Token_type type) {
    int res = std::get<0>(get_binding_power(type));
    if (res == -1) {
        throw string("CE, unexpected token in prefix expression: ") + token_type_to_string(type);
    }
    return res;
}

string literal_type_to_string(LiteralType type) {
    switch (type) {
        case LiteralType::NUMBER: return "number";
        case LiteralType::STRING: return "string";
        case LiteralType::CHAR: return "char";
        case LiteralType::BOOL: return "bool";
    }
}

string binary_operator_to_string(Binary_Operator op) {
    switch (op) {
        case Binary_Operator::ADD: return "+";
        case Binary_Operator::SUB: return "-";
        case Binary_Operator::MUL: return "*";
        case Binary_Operator::DIV: return "/";
        case Binary_Operator::MOD: return "%";
        case Binary_Operator::AND: return "&";
        case Binary_Operator::OR: return "|";
        case Binary_Operator::XOR: return "^";
        case Binary_Operator::AND_AND: return "&&";
        case Binary_Operator::OR_OR: return "||";
        case Binary_Operator::EQ: return "==";
        case Binary_Operator::NEQ: return "!=";
        case Binary_Operator::LT: return "<";
        case Binary_Operator::GT: return ">";
        case Binary_Operator::LEQ: return "<=";
        case Binary_Operator::GEQ: return ">=";
        case Binary_Operator::SHL: return "<<";
        case Binary_Operator::SHR: return ">>";
        case Binary_Operator::ASSIGN: return "=";
        case Binary_Operator::ADD_ASSIGN: return "+=";
        case Binary_Operator::SUB_ASSIGN: return "-=";
        case Binary_Operator::MUL_ASSIGN: return "*=";
        case Binary_Operator::DIV_ASSIGN: return "/=";
        case Binary_Operator::MOD_ASSIGN: return "%=";
        case Binary_Operator::AND_ASSIGN: return "&=";
        case Binary_Operator::OR_ASSIGN: return "|=";
        case Binary_Operator::XOR_ASSIGN: return "^=";
        case Binary_Operator::SHL_ASSIGN: return "<<=";
        case Binary_Operator::SHR_ASSIGN: return ">>=";
    }
}

string unary_operator_to_string(Unary_Operator op) {
    switch (op) {
        case Unary_Operator::NEG: return "-";
        case Unary_Operator::NOT: return "!";
        case Unary_Operator::REF: return "&";
        case Unary_Operator::REF_MUT: return "&mut";
        case Unary_Operator::DEREF: return "*";
    }
}