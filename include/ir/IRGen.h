#ifndef SIMPLE_RUST_COMPILER_IR_IRGEN_H
#define SIMPLE_RUST_COMPILER_IR_IRGEN_H

#include "ast/visitor.h"
#include "ir/IRBuilder.h"
#include "semantic/consteval.h"
#include "semantic/controlflow.h"
#include "semantic/decl.h"
#include "semantic/typecheck.h"
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

namespace ir {

class TypeLowering;

struct LoopContext {
    BasicBlock_ptr header_block = nullptr;
    BasicBlock_ptr body_block = nullptr;
    BasicBlock_ptr continue_target = nullptr;
    BasicBlock_ptr break_target = nullptr;
    IRValue_ptr break_slot = nullptr;
    RealType_ptr result_type = nullptr;
};

struct FunctionContext {
    FnDecl_ptr decl = nullptr;
    IRFunction_ptr ir_function = nullptr;
    BasicBlock_ptr entry_block = nullptr;
    BasicBlock_ptr current_block = nullptr;
    BasicBlock_ptr return_block = nullptr;
    IRValue_ptr return_slot = nullptr;
    IRValue_ptr self_slot = nullptr;
    bool block_sealed = false;

    std::unordered_map<LetDecl_ptr, IRValue_ptr> local_slots;
    std::vector<LoopContext> loop_stack;
    std::unordered_map<size_t, IRValue_ptr> expr_value_map;
    std::unordered_map<size_t, IRValue_ptr> expr_address_map;
};

class IRGenVisitor : public AST_Walker {
  public:
    IRGenVisitor(IRModule &module, IRBuilder &builder,
                 TypeLowering &type_lowering,
                 std::map<size_t, Scope_ptr> &node_scope_map,
                 std::map<size_t, RealType_ptr> &type_map,
                 std::map<size_t, std::pair<RealType_ptr, PlaceKind>>
                     &node_type_and_place_kind_map,
                 std::map<size_t, OutcomeState> &node_outcome_state_map,
                 std::map<size_t, FnDecl_ptr> &call_expr_to_decl_map,
                 std::map<ConstDecl_ptr, ConstValue_ptr> &const_value_map,
                 std::map<size_t, FnDecl_ptr> &fn_item_to_decl_map,
                 std::map<size_t, ValueDecl_ptr> &identifier_expr_to_decl_map,
                 std::map<size_t, LetDecl_ptr> &let_stmt_to_decl_map);

    void visit(FnItem &node) override;
    void visit(StructItem &node) override;
    void visit(EnumItem &node) override;
    void visit(ImplItem &node) override;
    void visit(ConstItem &node) override;
    void visit(ItemStmt &node) override;
    void visit(LetStmt &node) override;
    void visit(ExprStmt &node) override;
    void visit(ReturnExpr &node) override;
    void visit(BreakExpr &node) override;
    void visit(ContinueExpr &node) override;
    void visit(BinaryExpr &node) override;
    void visit(UnaryExpr &node) override;
    void visit(CallExpr &node) override;
    void visit(IfExpr &node) override;
    void visit(WhileExpr &node) override;
    void visit(LoopExpr &node) override;
    void visit(BlockExpr &node) override;
    void visit(StructExpr &node) override;
    void visit(ArrayExpr &node) override;
    void visit(RepeatArrayExpr &node) override;
    void visit(FieldExpr &node) override;
    void visit(IndexExpr &node) override;
    void visit(IdentifierExpr &node) override;
    void visit(LiteralExpr &node) override;
    void visit(CastExpr &node) override;
    void visit(PathExpr &node) override;
    void visit(SelfExpr &node) override;
    void visit(UnitExpr &node) override;

  private:
    IRValue_ptr ensure_slot_for_decl(LetDecl_ptr decl);
    void branch_if_needed(BasicBlock_ptr target);
    bool current_block_has_next(size_t node_id) const;
    IRValue_ptr get_rvalue(size_t node_id);
    IRValue_ptr get_lvalue(size_t node_id);
    LoopContext *current_loop();
    const LoopContext *current_loop() const;
    void ensure_current_insertion();
    FunctionContext &current_fn();
    const FunctionContext &current_fn() const;
    TypeDecl_ptr resolve_type_decl(Type_ptr type_node) const;

    IRModule &module_;
    IRBuilder &builder_;
    TypeLowering &type_lowering_;
    std::map<size_t, Scope_ptr> &node_scope_map_;
    std::map<size_t, RealType_ptr> &type_map_;
    std::map<size_t, std::pair<RealType_ptr, PlaceKind>>
        &node_type_and_place_kind_map_;
    std::map<size_t, OutcomeState> &node_outcome_state_map_;
    std::map<size_t, FnDecl_ptr> &call_expr_to_decl_map_;
    std::map<ConstDecl_ptr, ConstValue_ptr> &const_value_map_;
    std::map<size_t, FnDecl_ptr> &fn_item_to_decl_map_;
    std::map<size_t, ValueDecl_ptr> &identifier_expr_to_decl_map_;
    std::map<size_t, LetDecl_ptr> &let_stmt_to_decl_map_;
    std::unique_ptr<FunctionContext> fn_ctx_;
};

class IRGenerator {
  public:
    IRGenerator(IRModule &module, IRBuilder &builder,
                TypeLowering &type_lowering,
                std::map<size_t, Scope_ptr> &node_scope_map,
                std::map<Scope_ptr, Local_Variable_map>
                    &scope_local_variable_map,
                std::map<size_t, RealType_ptr> &type_map,
                std::map<size_t, std::pair<RealType_ptr, PlaceKind>>
                    &node_type_and_place_kind_map,
                std::map<size_t, OutcomeState> &node_outcome_state_map,
                std::map<size_t, FnDecl_ptr> &call_expr_to_decl_map,
                std::map<ConstDecl_ptr, ConstValue_ptr> &const_value_map,
                std::map<size_t, FnDecl_ptr> &fn_item_to_decl_map,
                std::map<size_t, ValueDecl_ptr> &identifier_expr_to_decl_map,
                std::map<size_t, LetDecl_ptr> &let_stmt_to_decl_map);

    void generate(const std::vector<Item_ptr> &ast_items);

  private:
    IRModule &module_;
    IRBuilder &builder_;
    TypeLowering &type_lowering_;
    std::map<size_t, Scope_ptr> &node_scope_map_;
    std::map<Scope_ptr, Local_Variable_map> &scope_local_variable_map_;
    std::map<size_t, RealType_ptr> &type_map_;
    std::map<size_t, std::pair<RealType_ptr, PlaceKind>>
        &node_type_and_place_kind_map_;
    std::map<size_t, OutcomeState> &node_outcome_state_map_;
    std::map<size_t, FnDecl_ptr> &call_expr_to_decl_map_;
    std::map<ConstDecl_ptr, ConstValue_ptr> &const_value_map_;
    std::map<size_t, FnDecl_ptr> &fn_item_to_decl_map_;
    std::map<size_t, ValueDecl_ptr> &identifier_expr_to_decl_map_;
    std::map<size_t, LetDecl_ptr> &let_stmt_to_decl_map_;
};

} // namespace ir

#endif // SIMPLE_RUST_COMPILER_IR_IRGEN_H
