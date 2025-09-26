#include "semantic_step1.h"
#include "ast.h"
#include "visitor.h"
#include <memory>

/*
实现 semantic check 的第一步的相关代码
step1 : 建作用域树 + 符号初收集
*/

using std::make_shared;

ScopeBuilder_Visitor::ScopeBuilder_Visitor(Scope_ptr root_scope, map<AST_Node_ptr, shared_ptr<Scope>> &node_scope_map_) :
    node_scope_map(node_scope_map_), block_is_in_function(false) {
    // 初始化作用域栈，加入根作用域
    scope_stack.push_back(root_scope);
}

void ScopeBuilder_Visitor::visit(LiteralExpr &node) {
    node_scope_map[make_shared<LiteralExpr>(node)] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(IdentifierExpr &node) {
    node_scope_map[make_shared<IdentifierExpr>(node)] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(BinaryExpr &node) {
    node_scope_map[make_shared<BinaryExpr>(node)] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(UnaryExpr &node) {
    node_scope_map[make_shared<UnaryExpr>(node)] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(CallExpr &node) {
    node_scope_map[make_shared<CallExpr>(node)] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(FieldExpr &node) {
    node_scope_map[make_shared<FieldExpr>(node)] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(StructExpr &node) {
    node_scope_map[make_shared<StructExpr>(node)] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(IndexExpr &node) {
    node_scope_map[make_shared<IndexExpr>(node)] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(BlockExpr &node) {
    bool build_new_scope = false;
    if (block_is_in_function) {
        block_is_in_function = false;
    } else {
        // 新建作用域
        build_new_scope = true;
        Scope_ptr new_scope = make_shared<Scope>(current_scope(), ScopeKind::Block);
        current_scope()->children.push_back(new_scope);
        scope_stack.push_back(new_scope);
    }
    node_scope_map[make_shared<BlockExpr>(node)] = current_scope();
    AST_Walker::visit(node);
    if (build_new_scope) {
        scope_stack.pop_back();
    }
}
void ScopeBuilder_Visitor::visit(IfExpr &node) {
    node_scope_map[make_shared<IfExpr>(node)] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(WhileExpr &node) {
    node_scope_map[make_shared<WhileExpr>(node)] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(LoopExpr &node) {
    node_scope_map[make_shared<LoopExpr>(node)] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(ReturnExpr &node) {
    node_scope_map[make_shared<ReturnExpr>(node)] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(BreakExpr &node) {
    node_scope_map[make_shared<BreakExpr>(node)] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(ContinueExpr &node) {
    node_scope_map[make_shared<ContinueExpr>(node)] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(CastExpr &node) {
    node_scope_map[make_shared<CastExpr>(node)] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(PathExpr &node) {
    node_scope_map[make_shared<PathExpr>(node)] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(SelfExpr &node) {
    node_scope_map[make_shared<SelfExpr>(node)] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(UnitExpr &node) {
    node_scope_map[make_shared<UnitExpr>(node)] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(ArrayExpr &node) {
    node_scope_map[make_shared<ArrayExpr>(node)] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(RepeatArrayExpr &node) {
    node_scope_map[make_shared<RepeatArrayExpr>(node)] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(FnItem &node) {
    node_scope_map[make_shared<FnItem>(node)] = current_scope();
    Scope_ptr new_scope = make_shared<Scope>(current_scope(), ScopeKind::Function);

    FnDecl_ptr fn_decl = make_shared<FnDecl>(node, new_scope);
    // 关联函数的事情，第二轮再搞
    // 加入 value_namespace
    if (current_scope()->value_namespace.find(node.function_name) != 
        current_scope()->value_namespace.end()) {
        throw string("CE, function name ") + node.function_name + " redefined";
    } else {
        current_scope()->value_namespace[node.function_name] = fn_decl;
    }

    current_scope()->children.push_back(new_scope);
    scope_stack.push_back(new_scope);
    block_is_in_function = true;
    // 接下来遍历到 Pattern，这个时候不用管
    // 下一个是 BlockExpr，这个时候会被 block_is_in_function 标记，所以不会新建作用域
    AST_Walker::visit(node);
    scope_stack.pop_back();
}
void ScopeBuilder_Visitor::visit(StructItem &node) {
    node_scope_map[make_shared<StructItem>(node)] = current_scope();

    StructDecl_ptr struct_decl = make_shared<StructDecl>(node);
    // 加入 type_namespace
    if (current_scope()->type_namespace.find(node.struct_name) != 
        current_scope()->type_namespace.end()) {
        throw string("CE, struct name ") + node.struct_name + " redefined";
    } else {
        current_scope()->type_namespace[node.struct_name] = struct_decl;
    }
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(EnumItem &node) {
    node_scope_map[make_shared<EnumItem>(node)] = current_scope();

    EnumDecl_ptr enum_decl = make_shared<EnumDecl>(node);
    // 加入 type_namespace
    if (current_scope()->type_namespace.find(node.enum_name) != 
        current_scope()->type_namespace.end()) {
        throw string("CE, enum name ") + node.enum_name + " redefined";
    } else {
        current_scope()->type_namespace[node.enum_name] = enum_decl;
    }
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(ImplItem &node) {
    node_scope_map[make_shared<ImplItem>(node)] = current_scope();
    Scope_ptr new_scope = make_shared<Scope>(current_scope(), ScopeKind::Impl, node.struct_name);
    current_scope()->children.push_back(new_scope);
    scope_stack.push_back(new_scope);
    AST_Walker::visit(node);
    scope_stack.pop_back();
}
void ScopeBuilder_Visitor::visit(ConstItem &node) {
    node_scope_map[make_shared<ConstItem>(node)] = current_scope();

    ConstDecl_ptr const_decl = make_shared<ConstDecl>(node);
    // 加入 value_namespace
    if (current_scope()->value_namespace.find(node.const_name) != 
        current_scope()->value_namespace.end()) {
        throw string("CE, const name ") + node.const_name + " redefined";
    } else {
        current_scope()->value_namespace[node.const_name] = const_decl;
    }
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(LetStmt &node) {
    node_scope_map[make_shared<LetStmt>(node)] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(ExprStmt &node) {
    node_scope_map[make_shared<ExprStmt>(node)] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(ItemStmt &node) {
    node_scope_map[make_shared<ItemStmt>(node)] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(PathType &node) {
    node_scope_map[make_shared<PathType>(node)] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(ArrayType &node) {
    node_scope_map[make_shared<ArrayType>(node)] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(UnitType &node) {
    node_scope_map[make_shared<UnitType>(node)] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(IdentifierPattern &node) {
    node_scope_map[make_shared<IdentifierPattern>(node)] = current_scope();
    AST_Walker::visit(node);
}

Semantic_Checker::Semantic_Checker() :
    root_scope(std::make_shared<Scope>(nullptr, ScopeKind::Root)) {}

void Semantic_Checker::step1_build_scopes_and_collect_symbols(vector<Item_ptr> &items) {
    ScopeBuilder_Visitor visitor(root_scope, node_scope_map);
    for (auto &item : items) {
        item->accept(visitor);
    }
}