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
    return std::make_unique<FnItem>(function_name,
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
