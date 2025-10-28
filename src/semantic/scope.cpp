#include <memory>
#include "semantic/scope.h"

/*
实现 semantic check 的第一步的相关代码
step1 : 建作用域树 + 符号初收集
*/

using std::make_shared;

ScopeBuilder_Visitor::ScopeBuilder_Visitor(Scope_ptr root_scope, map<size_t, shared_ptr<Scope>> &node_scope_map_) :
    node_scope_map(node_scope_map_), block_is_in_function(false) {
    // 初始化作用域栈，加入根作用域
    scope_stack.push_back(root_scope);
}

void ScopeBuilder_Visitor::visit(LiteralExpr &node) {
    node_scope_map[node.NodeId] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(IdentifierExpr &node) {
    node_scope_map[node.NodeId] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(BinaryExpr &node) {
    node_scope_map[node.NodeId] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(UnaryExpr &node) {
    node_scope_map[node.NodeId] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(CallExpr &node) {
    node_scope_map[node.NodeId] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(FieldExpr &node) {
    node_scope_map[node.NodeId] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(StructExpr &node) {
    node_scope_map[node.NodeId] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(IndexExpr &node) {
    node_scope_map[node.NodeId] = current_scope();
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
    node_scope_map[node.NodeId] = current_scope();
    AST_Walker::visit(node);
    if (build_new_scope) {
        scope_stack.pop_back();
    }
}
void ScopeBuilder_Visitor::visit(IfExpr &node) {
    node_scope_map[node.NodeId] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(WhileExpr &node) {
    node_scope_map[node.NodeId] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(LoopExpr &node) {
    node_scope_map[node.NodeId] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(ReturnExpr &node) {
    node_scope_map[node.NodeId] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(BreakExpr &node) {
    node_scope_map[node.NodeId] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(ContinueExpr &node) {
    node_scope_map[node.NodeId] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(CastExpr &node) {
    node_scope_map[node.NodeId] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(PathExpr &node) {
    node_scope_map[node.NodeId] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(SelfExpr &node) {
    node_scope_map[node.NodeId] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(UnitExpr &node) {
    node_scope_map[node.NodeId] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(ArrayExpr &node) {
    node_scope_map[node.NodeId] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(RepeatArrayExpr &node) {
    node_scope_map[node.NodeId] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(FnItem &node) {
    node_scope_map[node.NodeId] = current_scope();
    Scope_ptr new_scope = make_shared<Scope>(current_scope(), ScopeKind::Function);

    // FnDecl 现在接收 FnItem_ptr
    FnItem_ptr fn_item_ptr = make_shared<FnItem>(node);
    FnDecl_ptr fn_decl = make_shared<FnDecl>(fn_item_ptr, new_scope, node.receiver_type);
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
    node_scope_map[node.NodeId] = current_scope();

    // StructDecl 现在接收 StructItem_ptr
    StructItem_ptr struct_item_ptr = make_shared<StructItem>(node);
    StructDecl_ptr struct_decl = make_shared<StructDecl>(struct_item_ptr);
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
    node_scope_map[node.NodeId] = current_scope();

    // EnumDecl 现在接收 EnumItem_ptr
    EnumItem_ptr enum_item_ptr = make_shared<EnumItem>(node);
    EnumDecl_ptr enum_decl = make_shared<EnumDecl>(enum_item_ptr);
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
    node_scope_map[node.NodeId] = current_scope();
    Scope_ptr new_scope = make_shared<Scope>(current_scope(), ScopeKind::Impl, node.struct_name);
    current_scope()->children.push_back(new_scope);
    scope_stack.push_back(new_scope);
    AST_Walker::visit(node);
    scope_stack.pop_back();
}
void ScopeBuilder_Visitor::visit(ConstItem &node) {
    node_scope_map[node.NodeId] = current_scope();

    // ConstDecl 现在接收 ConstItem_ptr
    ConstItem_ptr const_item_ptr = make_shared<ConstItem>(node);
    ConstDecl_ptr const_decl = make_shared<ConstDecl>(const_item_ptr);
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
    node_scope_map[node.NodeId] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(ExprStmt &node) {
    node_scope_map[node.NodeId] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(ItemStmt &node) {
    node_scope_map[node.NodeId] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(PathType &node) {
    node_scope_map[node.NodeId] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(ArrayType &node) {
    node_scope_map[node.NodeId] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(UnitType &node) {
    node_scope_map[node.NodeId] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(SelfType &node) {
    node_scope_map[node.NodeId] = current_scope();
    AST_Walker::visit(node);
}
void ScopeBuilder_Visitor::visit(IdentifierPattern &node) {
    node_scope_map[node.NodeId] = current_scope();
    AST_Walker::visit(node);
}
