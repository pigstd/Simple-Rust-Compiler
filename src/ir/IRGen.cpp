#include "ir/IRGen.h"

#include "ast/ast.h"
#include "ir/IRBuilder.h"
#include "ir/type_lowering.h"
#include "semantic/controlflow.h"
#include "semantic/scope.h"
#include "semantic/type.h"
#include "tools/tools.h"

#include <cassert>
#include <cassert>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <unordered_map>

namespace ir {

// IRGenerator 负责驱动 AST 遍历并交给 IRGenVisitor 处理。
IRGenerator::IRGenerator(
    IRModule &module, IRBuilder &builder, TypeLowering &type_lowering,
    std::map<size_t, Scope_ptr> &node_scope_map,
    std::map<Scope_ptr, Local_Variable_map> &scope_local_variable_map,
    std::map<size_t, RealType_ptr> &type_map,
    std::map<size_t, std::pair<RealType_ptr, PlaceKind>>
        &node_type_and_place_kind_map,
    std::map<size_t, OutcomeState> &node_outcome_state_map,
    std::map<size_t, FnDecl_ptr> &call_expr_to_decl_map,
    std::map<ConstDecl_ptr, ConstValue_ptr> &const_value_map,
    std::map<size_t, FnDecl_ptr> &fn_item_to_decl_map,
    std::map<size_t, ValueDecl_ptr> &identifier_expr_to_decl_map,
    std::map<size_t, LetDecl_ptr> &let_stmt_to_decl_map)
    : module_(module), builder_(builder), type_lowering_(type_lowering),
      node_scope_map_(node_scope_map),
      scope_local_variable_map_(scope_local_variable_map),
      type_map_(type_map),
      node_type_and_place_kind_map_(node_type_and_place_kind_map),
      node_outcome_state_map_(node_outcome_state_map),
      call_expr_to_decl_map_(call_expr_to_decl_map),
      const_value_map_(const_value_map),
      fn_item_to_decl_map_(fn_item_to_decl_map),
      identifier_expr_to_decl_map_(identifier_expr_to_decl_map),
      let_stmt_to_decl_map_(let_stmt_to_decl_map) {}

// 针对整个 AST 顺序执行 IR lowering。
void IRGenerator::generate(const std::vector<Item_ptr> &ast_items) {
    // 单个 visitor 维护全部上下文，顺序遍历 AST 即可。
    IRGenVisitor visitor(module_, builder_, type_lowering_, node_scope_map_,
                         type_map_,
                         node_type_and_place_kind_map_, node_outcome_state_map_,
                         call_expr_to_decl_map_, const_value_map_,
                         fn_item_to_decl_map_, identifier_expr_to_decl_map_,
                         let_stmt_to_decl_map_);
    for (const auto &item : ast_items) {
        if (item) {
            item->accept(visitor);
        }
    }
}

// IRGenVisitor 携带所有语义信息，在遍历时生成 IR。
IRGenVisitor::IRGenVisitor(
    IRModule &module, IRBuilder &builder, TypeLowering &type_lowering,
    std::map<size_t, Scope_ptr> &node_scope_map,
    std::map<size_t, RealType_ptr> &type_map,
    std::map<size_t, std::pair<RealType_ptr, PlaceKind>>
        &node_type_and_place_kind_map,
    std::map<size_t, OutcomeState> &node_outcome_state_map,
    std::map<size_t, FnDecl_ptr> &call_expr_to_decl_map,
    std::map<ConstDecl_ptr, ConstValue_ptr> &const_value_map,
    std::map<size_t, FnDecl_ptr> &fn_item_to_decl_map,
    std::map<size_t, ValueDecl_ptr> &identifier_expr_to_decl_map,
    std::map<size_t, LetDecl_ptr> &let_stmt_to_decl_map)
    : module_(module), builder_(builder), type_lowering_(type_lowering),
      node_scope_map_(node_scope_map),
      type_map_(type_map),
      node_type_and_place_kind_map_(node_type_and_place_kind_map),
      node_outcome_state_map_(node_outcome_state_map),
      call_expr_to_decl_map_(call_expr_to_decl_map),
      const_value_map_(const_value_map), fn_item_to_decl_map_(fn_item_to_decl_map),
      identifier_expr_to_decl_map_(identifier_expr_to_decl_map),
      let_stmt_to_decl_map_(let_stmt_to_decl_map) {}

// 处理函数节点：创建 IR 函数与上下文，并继续遍历函数体。
void IRGenVisitor::visit(FnItem &node) {
    auto decl_iter = fn_item_to_decl_map_.find(node.NodeId);
    if (decl_iter == fn_item_to_decl_map_.end() || !decl_iter->second) {
        return;
    }
    auto decl = decl_iter->second;
    if (!decl || !decl->ast_node) {
        return;
    }

    auto fn_type = type_lowering_.lower_function(decl);
    auto ir_function = module_.define_function(decl->name, fn_type);
    if (!ir_function) {
        throw std::runtime_error("Failed to define IR function");
    }
    if (!ir_function->blocks().empty()) {
        throw std::runtime_error("IR function already lowered");
    }

    // 每个函数节点单独创建 FunctionContext，避免跨函数污染状态。
    auto previous_fn_ctx = std::move(fn_ctx_);
    fn_ctx_ = std::make_unique<FunctionContext>();

    auto &ctx = current_fn();
    
    ctx.decl = decl;
    ctx.ir_function = ir_function;
    ctx.entry_block = ir_function->create_block("entry");
    ctx.return_block = ir_function->create_block("return");
    ctx.current_block = ctx.entry_block;
    ctx.block_sealed = false;
    
    auto previous_insertion_point = builder_.insertion_block();

    // 入口 block 一旦激活 builder 插入点，其余 helper 就能创建指令。
    builder_.set_insertion_point(ctx.entry_block);

    const auto &params = ir_function->params();
    size_t param_idx = 0;
    // 将 self 参数存入槽。
    if (node.receiver_type != fn_reciever_type::NO_RECEIVER) {
        param_idx = 1;
        auto [name, type] = params[0];
        auto slot = builder_.create_alloca(type, "self.slot");
        auto arg = std::make_shared<RegisterValue>(name, type);
        builder_.create_store(arg, slot);
        ctx.self_slot = slot;
    }
    for (std::size_t idx = 0; param_idx < params.size(); ++idx, ++param_idx) {
        auto let_decl = decl->parameter_let_decls[idx];
        auto slot = ensure_slot_for_decl(let_decl);
        auto [name, type] = params[param_idx];
        auto arg = std::make_shared<RegisterValue>(name, type);
        builder_.create_store(arg, slot);
    }

    bool needs_return_slot = false;
    if (decl->return_type &&
        decl->return_type->kind != RealTypeKind::UNIT &&
        decl->return_type->kind != RealTypeKind::NEVER) {
        needs_return_slot = true;
    }
    // 特判 main 函数总是需要返回槽。
    if (decl->is_main) {
        needs_return_slot = true;
        auto ir_type = type_lowering_.lower(
            std::make_shared<I32RealType>(ReferenceType::NO_REF));
        ctx.return_slot = builder_.create_temp_alloca(ir_type, "ret.slot");
    }
    else if (needs_return_slot) {
        auto ir_type = type_lowering_.lower(decl->return_type);
        ctx.return_slot = builder_.create_temp_alloca(ir_type, "ret.slot");
    } else {
        ctx.return_slot = nullptr;
    }

    if (node.body) {
        node.body->accept(*this);
    }

    if (needs_return_slot && !decl->is_main &&
        current_block_has_next(node.body->NodeId)) {
        // 有返回值且函数体非终止，那么这个 block 的值结尾就是返回值。
        builder_.create_store(get_rvalue(node.body->NodeId),
                             ctx.return_slot);
    }

    if (ctx.current_block && !ctx.block_sealed) {
        branch_if_needed(ctx.return_block);
    }
    builder_.set_insertion_point(ctx.return_block);
    if (!ctx.return_block->get_terminator()) {
        if (ctx.return_slot) {
            auto ret_value = builder_.create_load(ctx.return_slot, "ret.val");
            builder_.create_ret(ret_value);
        } else {
            builder_.create_ret();
        }
    }

    builder_.set_insertion_point(previous_insertion_point); // 函数结束，恢复插入点。
    fn_ctx_ = std::move(previous_fn_ctx);
}

// let 语句 lowering：分配槽并写入初始化值。
void IRGenVisitor::visit(LetStmt &node) {
    auto &ctx = current_fn();
    if (!ctx.current_block) {
        return; // 不可达
    }
    auto decl = let_stmt_to_decl_map_[node.NodeId];
    if (!decl) {
        throw std::runtime_error("LetStmt has no associated LetDecl");
    }
    auto slot = ensure_slot_for_decl(decl);
    if (!node.initializer) {
        return;
    }
    node.initializer->accept(*this);
    ensure_current_insertion();
    builder_.create_store(get_rvalue(node.initializer->NodeId), slot);
}

// 表达式语句，如果没有 ; 并且有值，则保存其值。
void IRGenVisitor::visit(ExprStmt &node) {
    if (!node.expr) {
        return;
    }
    node.expr->accept(*this);
    if (!node.is_semi) {
        auto type_iter =
            node_type_and_place_kind_map_.find(node.expr->NodeId);
        if (type_iter != node_type_and_place_kind_map_.end() &&
            type_iter->second.first &&
            type_iter->second.first->kind != RealTypeKind::UNIT &&
            type_iter->second.first->kind != RealTypeKind::NEVER) {
            expr_value_map_[node.NodeId] = get_rvalue(node.expr->NodeId);
        }
    }
}

// return。
void IRGenVisitor::visit(ReturnExpr &node) {
    if (!fn_ctx_) {
        throw std::runtime_error("ReturnExpr outside of function");
    }
    auto &ctx = current_fn();
    if (!ctx.current_block) {
        return; // 不可达
    }
    if (node.return_value) {
        node.return_value->accept(*this);
        if (ctx.return_slot) {
            auto value = get_rvalue(node.return_value->NodeId);
            ensure_current_insertion();
            builder_.create_store(value, ctx.return_slot);
        }
    }
    branch_if_needed(ctx.return_block);
    ctx.current_block = nullptr;
}

// break：若有 break 值则写入 loop break_slot。
void IRGenVisitor::visit(BreakExpr &node) {
    auto loop = current_loop();
    if (!loop) {
        throw std::runtime_error("BreakExpr outside of loop");
    }
    auto &ctx = current_fn();
    if (node.break_value && loop->break_slot && ctx.current_block) {
        node.break_value->accept(*this);
        ensure_current_insertion();
        builder_.create_store(get_rvalue(node.break_value->NodeId),
                             loop->break_slot);
    }
    branch_if_needed(loop->break_target);
    ctx.current_block = nullptr;
}

// continue：跳转到 loop continue_target。
void IRGenVisitor::visit(ContinueExpr &node) {
    auto loop = current_loop();
    if (!loop) {
        throw std::runtime_error("ContinueExpr outside of loop");
    }
    auto &ctx = current_fn();
    if (!ctx.current_block) {
        return;
    }
    branch_if_needed(loop->continue_target);
    ctx.current_block = nullptr;
}

// 二元表达式：算术、比较、赋值等。
void IRGenVisitor::visit(BinaryExpr &node) {
    if (!fn_ctx_) {
        return;
    }
    auto &ctx = current_fn();
    auto eval_operands = [&]() {
        if (node.left) {
            node.left->accept(*this);
        }
        if (node.right) {
            node.right->accept(*this);
        }
    };
    auto load_left_type = [&](RealTypeKind fallback = RealTypeKind::I32) {
        auto iter = node_type_and_place_kind_map_.find(node.left->NodeId);
        if (iter == node_type_and_place_kind_map_.end() || !iter->second.first) {
            return fallback;
        }
        return iter->second.first->kind;
    };
    auto is_unsigned_kind = [&](RealTypeKind kind) {
        return kind == RealTypeKind::U32 || kind == RealTypeKind::USIZE;
    };

    switch (node.op) {
    case Binary_Operator::ASSIGN:
    case Binary_Operator::ADD_ASSIGN:
    case Binary_Operator::SUB_ASSIGN:
    case Binary_Operator::MUL_ASSIGN:
    case Binary_Operator::DIV_ASSIGN:
    case Binary_Operator::MOD_ASSIGN:
    case Binary_Operator::AND_ASSIGN:
    case Binary_Operator::OR_ASSIGN:
    case Binary_Operator::XOR_ASSIGN:
    case Binary_Operator::SHL_ASSIGN:
    case Binary_Operator::SHR_ASSIGN: {
        eval_operands();
        auto addr = get_lvalue(node.left->NodeId);
        auto rhs = get_rvalue(node.right->NodeId);
        ensure_current_insertion();
        IRValue_ptr result = rhs;
        if (node.op != Binary_Operator::ASSIGN) {
            auto lhs_val = builder_.create_load(addr);
            const auto kind = load_left_type();
            const bool is_unsigned = is_unsigned_kind(kind);
            switch (node.op) {
            case Binary_Operator::ADD_ASSIGN:
                result = builder_.create_add(lhs_val, rhs);
                break;
            case Binary_Operator::SUB_ASSIGN:
                result = builder_.create_sub(lhs_val, rhs);
                break;
            case Binary_Operator::MUL_ASSIGN:
                result = builder_.create_mul(lhs_val, rhs);
                break;
            case Binary_Operator::DIV_ASSIGN:
                result = is_unsigned ? builder_.create_udiv(lhs_val, rhs)
                                     : builder_.create_sdiv(lhs_val, rhs);
                break;
            case Binary_Operator::MOD_ASSIGN:
                result = is_unsigned ? builder_.create_urem(lhs_val, rhs)
                                     : builder_.create_srem(lhs_val, rhs);
                break;
            case Binary_Operator::AND_ASSIGN:
                result = builder_.create_and(lhs_val, rhs);
                break;
            case Binary_Operator::OR_ASSIGN:
                result = builder_.create_or(lhs_val, rhs);
                break;
            case Binary_Operator::XOR_ASSIGN:
                result = builder_.create_xor(lhs_val, rhs);
                break;
            case Binary_Operator::SHL_ASSIGN:
                result = builder_.create_shl(lhs_val, rhs);
                break;
            case Binary_Operator::SHR_ASSIGN:
                result = is_unsigned ? builder_.create_lshr(lhs_val, rhs)
                                     : builder_.create_ashr(lhs_val, rhs);
                break;
            default:
                break;
            }
        }
        builder_.create_store(result, addr);
        expr_value_map_[node.NodeId] = result;
        return;
    }
    case Binary_Operator::ADD:
    case Binary_Operator::SUB:
    case Binary_Operator::MUL:
    case Binary_Operator::DIV:
    case Binary_Operator::MOD:
    case Binary_Operator::AND:
    case Binary_Operator::OR:
    case Binary_Operator::XOR:
    case Binary_Operator::SHL:
    case Binary_Operator::SHR: {
        eval_operands();
        auto lhs = get_rvalue(node.left->NodeId);
        auto rhs = get_rvalue(node.right->NodeId);
        ensure_current_insertion();
        IRValue_ptr result = nullptr;
        const auto kind = load_left_type();
        const bool is_unsigned = is_unsigned_kind(kind);
        switch (node.op) {
        case Binary_Operator::ADD:
            result = builder_.create_add(lhs, rhs);
            break;
        case Binary_Operator::SUB:
            result = builder_.create_sub(lhs, rhs);
            break;
        case Binary_Operator::MUL:
            result = builder_.create_mul(lhs, rhs);
            break;
        case Binary_Operator::DIV:
            result = is_unsigned ? builder_.create_udiv(lhs, rhs)
                                 : builder_.create_sdiv(lhs, rhs);
            break;
        case Binary_Operator::MOD:
            result = is_unsigned ? builder_.create_urem(lhs, rhs)
                                 : builder_.create_srem(lhs, rhs);
            break;
        case Binary_Operator::AND:
            result = builder_.create_and(lhs, rhs);
            break;
        case Binary_Operator::OR:
            result = builder_.create_or(lhs, rhs);
            break;
        case Binary_Operator::XOR:
            result = builder_.create_xor(lhs, rhs);
            break;
        case Binary_Operator::SHL:
            result = builder_.create_shl(lhs, rhs);
            break;
        case Binary_Operator::SHR:
            result = is_unsigned ? builder_.create_lshr(lhs, rhs)
                                 : builder_.create_ashr(lhs, rhs);
            break;
        default:
            throw std::runtime_error("Unsupported arithmetic operator in BinaryExpr");
        }
        // std::cerr << node.NodeId << ' ' << (result != nullptr) << '\n';
        if (result) {
            expr_value_map_[node.NodeId] = result;
        }
        return;
    }
    case Binary_Operator::EQ:
    case Binary_Operator::NEQ:
    case Binary_Operator::LT:
    case Binary_Operator::LEQ:
    case Binary_Operator::GT:
    case Binary_Operator::GEQ: {
        eval_operands();
        auto lhs = get_rvalue(node.left->NodeId);
        auto rhs = get_rvalue(node.right->NodeId);
        ensure_current_insertion();
        IRValue_ptr result = nullptr;
        const auto kind = load_left_type();
        const bool is_unsigned = is_unsigned_kind(kind);
        switch (node.op) {
        case Binary_Operator::EQ:
            result = builder_.create_icmp_eq(lhs, rhs);
            break;
        case Binary_Operator::NEQ:
            result = builder_.create_icmp_ne(lhs, rhs);
            break;
        case Binary_Operator::LT:
            result = is_unsigned ? builder_.create_icmp_ult(lhs, rhs)
                                 : builder_.create_icmp_slt(lhs, rhs);
            break;
        case Binary_Operator::LEQ:
            result = is_unsigned ? builder_.create_icmp_ule(lhs, rhs)
                                 : builder_.create_icmp_sle(lhs, rhs);
            break;
        case Binary_Operator::GT:
            result = is_unsigned ? builder_.create_icmp_ugt(lhs, rhs)
                                 : builder_.create_icmp_sgt(lhs, rhs);
            break;
        case Binary_Operator::GEQ:
            result = is_unsigned ? builder_.create_icmp_uge(lhs, rhs)
                                 : builder_.create_icmp_sge(lhs, rhs);
            break;
        default:
            throw std::runtime_error("Unsupported comparison operator in BinaryExpr");
        }
        if (result) {
        expr_value_map_[node.NodeId] = result;
        }
        return;
    }
    // && ||，要短路
    case Binary_Operator::AND_AND:
    case Binary_Operator::OR_OR: {
        auto result_slot = builder_.create_temp_alloca(
            type_lowering_.lower(
                std::make_shared<BoolRealType>(ReferenceType::NO_REF)));
        node.left->accept(*this);
        auto right_block = builder_.create_block("logical.rhs");
        auto merge_block = builder_.create_block("logical.merge");
        ensure_current_insertion();
        auto lhs_value = get_rvalue(node.left->NodeId);
        // && 先把结果填成 false, || 先把结果填成 true
        if (node.op == Binary_Operator::AND_AND) {
            builder_.create_store(
                std::make_shared<ConstantValue>(
                    type_lowering_.lower(
                        std::make_shared<BoolRealType>(ReferenceType::NO_REF)),
                    0),
                result_slot);
            builder_.create_cond_br(lhs_value, right_block, merge_block);
        } else {
            builder_.create_store(
                std::make_shared<ConstantValue>(
                    type_lowering_.lower(
                        std::make_shared<BoolRealType>(ReferenceType::NO_REF)),
                    1),
                result_slot);
            builder_.create_cond_br(lhs_value, merge_block, right_block);
        }
        ctx.current_block = right_block;
        builder_.set_insertion_point(right_block);
        ctx.block_sealed = false;
        node.right->accept(*this);
        ensure_current_insertion();
        auto rhs_value = get_rvalue(node.right->NodeId);
        builder_.create_store(rhs_value, result_slot);
        branch_if_needed(merge_block);
        ctx.current_block = merge_block;
        builder_.set_insertion_point(merge_block);
        ctx.block_sealed = false;
        expr_address_map_[node.NodeId] = result_slot;
        return;
    }
    default:
        throw std::runtime_error("Unsupported BinaryExpr operator");
    }
}

// 一元表达式：取负、取反、取引用、解引用。
void IRGenVisitor::visit(UnaryExpr &node) {
    if (!fn_ctx_ || !node.right) {
        return;
    }
    auto &ctx = current_fn();
    auto operand_type_kind = [&](size_t id) -> RealTypeKind {
        auto iter = node_type_and_place_kind_map_.find(id);
        if (iter == node_type_and_place_kind_map_.end() || !iter->second.first) {
            return RealTypeKind::I32;
        }
        return iter->second.first->kind;
    };

    node.right->accept(*this);
    switch (node.op) {
    case Unary_Operator::NEG: {
        auto rhs = get_rvalue(node.right->NodeId);
        ensure_current_insertion();
        auto [type, placekind] =
            node_type_and_place_kind_map_[node.right->NodeId];
        auto lowered_type = type_lowering_.lower(type);
        auto zero = std::make_shared<ConstantValue>(lowered_type, 0);
        auto result = builder_.create_sub(zero, rhs);
        expr_value_map_[node.NodeId] = result;
        return;
    }
    case Unary_Operator::NOT: {
        auto rhs = get_rvalue(node.right->NodeId);
        ensure_current_insertion();
        auto result = builder_.create_not(rhs);
        expr_value_map_[node.NodeId] = result;
        return;
    }
    case Unary_Operator::REF:
    case Unary_Operator::REF_MUT: {
        auto addr = get_lvalue(node.right->NodeId);
        expr_value_map_[node.NodeId] = addr;
        return;
    }
    case Unary_Operator::DEREF: {
        auto ptr = get_rvalue(node.right->NodeId);
        expr_address_map_[node.NodeId] = ptr;
        return;
    }
    default:
        break;
    }
    AST_Walker::visit(node);
}

// 调用：按绑定函数生成 call 指令。
void IRGenVisitor::visit(CallExpr &node) {
    if (!fn_ctx_) {
        throw std::runtime_error("CallExpr outside of function");
    }
    auto &ctx = current_fn();
    if (!ctx.current_block) {
        return;
    }
    auto decl_iter = call_expr_to_decl_map_.find(node.NodeId);
    if (decl_iter == call_expr_to_decl_map_.end() || !decl_iter->second) {
        throw std::runtime_error("CallExpr missing target FnDecl");
    }
    auto fn_decl = decl_iter->second;
    if (!fn_decl) {
        throw std::runtime_error("Invalid FnDecl for CallExpr");
    }

    // 特判：exit 其实就是 return
    if (fn_decl->is_exit) {
        if (node.arguments.size() != 1 || !node.arguments[0]) {
            throw std::runtime_error("exit function requires one argument");
        }
        node.arguments[0]->accept(*this);
        auto exit_code = get_rvalue(node.arguments[0]->NodeId);
        ensure_current_insertion();
        builder_.create_store(exit_code, ctx.return_slot);
        builder_.create_br(ctx.return_block);
        expr_value_map_[node.NodeId] = nullptr;
        ctx.block_sealed = true;
        return;
    }

    // 只有有 self 才需要 visit callee，否则我已经知道函数是什么了
    bool need_struct = false;
    if (fn_decl->receiver_type != fn_reciever_type::NO_RECEIVER) {
        need_struct = true;
    }
    if (need_struct) {
        node.callee->accept(*this);
    }
    for (const auto &arg : node.arguments) {
        if (arg) {
            arg->accept(*this);
        }
    }
    // array len 内建函数，直接返回一个值
    if (fn_decl->is_array_len) {
        size_t array_size = 0;
        auto callee_ptr = std::dynamic_pointer_cast<FieldExpr>(node.callee);
        if (!callee_ptr) {
            throw std::runtime_error("Array len callee is not FieldExpr");
        }
        auto array_type = node_type_and_place_kind_map_[callee_ptr->base->NodeId].first;
        if (!array_type || array_type->kind != RealTypeKind::ARRAY) {
            throw std::runtime_error("Array len callee base is not array type");
        }
        auto array_real_type = std::dynamic_pointer_cast<ArrayRealType>(array_type);
        if (!array_real_type) {
            throw std::runtime_error("Array len callee base is not ArrayRealType");
        }
        array_size = array_real_type->size;
        expr_value_map_[node.NodeId] = builder_.create_i32_constant(static_cast<int32_t>(array_size));
        return;
    }
    std::vector<IRValue_ptr> call_args;
    if (need_struct) {
        auto method_callee = std::dynamic_pointer_cast<FieldExpr>(node.callee);
        if (!method_callee || !method_callee->base) {
            throw std::runtime_error("Method call missing base expression");
        }
        IRValue_ptr self_operand = nullptr;
        switch (fn_decl->receiver_type) {
        case fn_reciever_type::SELF:
            self_operand = get_rvalue(method_callee->base->NodeId);
            break;
        case fn_reciever_type::SELF_REF:
        case fn_reciever_type::SELF_REF_MUT:
            self_operand = get_lvalue(method_callee->base->NodeId);
            break;
        default:
            break;
        }
        if (!self_operand) {
            throw std::runtime_error("Failed to prepare self argument");
        }
        call_args.push_back(self_operand);
    }

    for (const auto &arg : node.arguments) {
        if (!arg) {
            continue;
        }
        call_args.push_back(get_rvalue(arg->NodeId));
    }

    ensure_current_insertion();
    auto ret_real_type = fn_decl->return_type
                             ? fn_decl->return_type
                             : std::make_shared<UnitRealType>(
                                   ReferenceType::NO_REF);
    auto ret_ir_type = type_lowering_.lower(ret_real_type);
    if (fn_decl->is_builtin) {
        auto fn_type = type_lowering_.lower_function(fn_decl);
        module_.declare_function(fn_decl->name, fn_type, true);
    }
    auto call_result =
        builder_.create_call(fn_decl->name, call_args, ret_ir_type, "call.tmp");
    if (call_result) {
        expr_value_map_[node.NodeId] = call_result;
    }
}

// if：根据 OutcomeState 决定 then/else/merge 控制流。
void IRGenVisitor::visit(IfExpr &node) {
    if (!fn_ctx_ || !node.condition || !node.then_branch) {
        throw std::runtime_error("Invalid IfExpr: missing condition/then branch");
    }
    auto &ctx = current_fn();
    auto then_block = ctx.ir_function->create_block("if.then");
    auto else_block = ctx.ir_function->create_block("if.else");
    auto merge_block = ctx.ir_function->create_block("if.merge");

    bool need_result = false;

    // 如果分支有 NEXT 而且 if 的结果不是 () 就需要合并结果。
    if (current_block_has_next(node.NodeId) && 
        node_type_and_place_kind_map_[node.NodeId].first->kind != RealTypeKind::UNIT) {
        need_result = true;
    }

    IRValue_ptr result_slot = nullptr;
    if (need_result) {
        result_slot = builder_.create_temp_alloca(
            type_lowering_.lower(
                node_type_and_place_kind_map_[node.NodeId].first),
            "if.result.slot");
    }

    node.condition->accept(*this);
    auto cond_value = get_rvalue(node.condition->NodeId);
    ensure_current_insertion();
    builder_.create_cond_br(cond_value, then_block,
                            else_block);

    builder_.set_insertion_point(then_block);
    ctx.current_block = then_block;
    ctx.block_sealed = false;
    node.then_branch->accept(*this);
    if (ctx.current_block && !ctx.block_sealed) {
        if (need_result) {
            auto then_value = get_rvalue(node.then_branch->NodeId);
            builder_.create_store(then_value, result_slot);
        }
        builder_.create_br(merge_block);
    }
    
    builder_.set_insertion_point(else_block);
    ctx.block_sealed = false;
    if (node.else_branch) {
        ctx.current_block = else_block;
        node.else_branch->accept(*this);
        if (ctx.current_block && !ctx.block_sealed) {
            if (need_result) {
                auto else_value = get_rvalue(node.else_branch->NodeId);
                builder_.create_store(else_value, result_slot);
            }
            builder_.create_br(merge_block);
        }
    } else {
        // 没 else，else block 直接跳转 merge。
        builder_.create_br(merge_block);
    }

    builder_.set_insertion_point(merge_block);
    ctx.current_block = merge_block;
    ctx.block_sealed = false;

    if (need_result) {
        expr_address_map_[node.NodeId] = result_slot;
    }
}

// while。
void IRGenVisitor::visit(WhileExpr &node) {
    if (!fn_ctx_ || !node.condition || !node.body) {
        throw std::runtime_error("Invalid WhileExpr: missing condition/body");
    }
    auto &ctx = current_fn();
    auto cond_block = ctx.ir_function->create_block("while.cond");
    auto body_block = ctx.ir_function->create_block("while.body");
    auto exit_block = ctx.ir_function->create_block("while.exit");

    branch_if_needed(cond_block);
    builder_.set_insertion_point(cond_block);
    ctx.current_block = cond_block;
    ctx.block_sealed = false;
    node.condition->accept(*this);
    auto cond_value = get_rvalue(node.condition->NodeId);
    builder_.create_cond_br(cond_value, body_block, exit_block);

    LoopContext loop_ctx;
    loop_ctx.header_block = cond_block;
    loop_ctx.body_block = body_block;
    loop_ctx.continue_target = cond_block;
    loop_ctx.break_target = exit_block;
    ctx.loop_stack.push_back(loop_ctx);

    builder_.set_insertion_point(body_block);
    ctx.current_block = body_block;
    ctx.block_sealed = false;
    node.body->accept(*this);

    if (ctx.current_block && !ctx.block_sealed) {
        builder_.create_br(cond_block);
    }
    ctx.block_sealed = false;
    ctx.loop_stack.pop_back();

    builder_.set_insertion_point(exit_block);
    ctx.current_block = exit_block;
    ctx.block_sealed = false;
}

// loop。
void IRGenVisitor::visit(LoopExpr &node) {
    if (!fn_ctx_ || !node.body) {
        throw std::runtime_error("Invalid LoopExpr: missing body");
    }
    auto &ctx = current_fn();
    auto body_block = ctx.ir_function->create_block("loop.body");
    auto break_block = ctx.ir_function->create_block("loop.break");

    branch_if_needed(body_block);
    builder_.set_insertion_point(body_block);
    ctx.current_block = body_block;
    ctx.block_sealed = false;

    LoopContext loop_ctx;
    loop_ctx.header_block = body_block;
    loop_ctx.body_block = body_block;
    loop_ctx.continue_target = body_block;
    loop_ctx.break_target = break_block;
    auto [type, placekind] = 
        node_type_and_place_kind_map_[node.NodeId];
    if (!type) {
        throw std::runtime_error("LoopExpr has no associated type");
    }
    // 有返回值
    if (type->kind != RealTypeKind::UNIT
        && current_block_has_next(node.NodeId)) {
        auto ir_type = type_lowering_.lower(type);
        loop_ctx.break_slot =
            builder_.create_temp_alloca(ir_type, "loop.break.slot");
    }
    ctx.loop_stack.push_back(loop_ctx);

    node.body->accept(*this);
    if (ctx.current_block && !ctx.block_sealed) {
        builder_.set_insertion_point(ctx.current_block);
        builder_.create_br(body_block);
    }
    builder_.set_insertion_point(break_block);
    ctx.current_block = break_block;
    ctx.block_sealed = false;
    if (loop_ctx.break_slot) {
        expr_address_map_[node.NodeId] = loop_ctx.break_slot;
    }
    ctx.loop_stack.pop_back();
}

// block。
void IRGenVisitor::visit(BlockExpr &node) {
    if (!fn_ctx_) {
        return;
    }
    auto &ctx = current_fn();

    // std::cerr << "Visiting BlockExpr with NodeId = "
    //           << node.NodeId << " \n";

    for (const auto &stmt : node.statements) {
        if (!ctx.current_block || ctx.block_sealed) {
            // std::cerr << "break!\n";
            break;
        }
        // std::cerr << "visite stmt \n";
        stmt->accept(*this);
        if (!current_block_has_next(stmt->NodeId)) {
            ctx.current_block = nullptr;
            // std::cerr << "break! Node id = " << stmt->NodeId << "\n";
            break;
        }
    }
    if (node.tail_statement) {
        // std::cerr << "visit tail, NodeId = " << node.tail_statement->NodeId << "\n";
        node.tail_statement->accept(*this);
        // 如果返回值不是 void 才有值
        if (current_block_has_next(node.tail_statement->NodeId) &&
            node_type_and_place_kind_map_[node.tail_statement->NodeId].first->kind != RealTypeKind::UNIT) {
            // std::cerr << "visit tail, NodeId = " << node.tail_statement->NodeId << "\n";
            expr_value_map_[node.NodeId] =
                get_rvalue(node.tail_statement->NodeId);
        }
    }
}

// 结构体字面量。
void IRGenVisitor::visit(StructExpr &node) {
    if (!fn_ctx_) {
        throw std::runtime_error("StructExpr outside of function");
    }
    auto type_iter = node_type_and_place_kind_map_.find(node.NodeId);
    if (type_iter == node_type_and_place_kind_map_.end() ||
        !type_iter->second.first) {
        throw std::runtime_error("StructExpr missing inferred type");
    }
    auto struct_type =
        std::dynamic_pointer_cast<StructRealType>(type_iter->second.first);
    if (!struct_type) {
        throw std::runtime_error("StructExpr type is not a struct");
    }
    auto struct_decl = struct_type->decl.lock();
    if (!struct_decl) {
        throw std::runtime_error("StructExpr missing struct declaration");
    }

    auto ir_struct_type = type_lowering_.lower(struct_type);
    auto slot =
        builder_.create_temp_alloca(ir_struct_type, "struct.literal.slot");

    std::unordered_map<std::string, Expr_ptr> field_exprs;
    for (const auto &field_pair : node.fields) {
        field_exprs[field_pair.first] = field_pair.second;
    }
    auto zero = builder_.create_i32_constant(0);

    for (std::size_t idx = 0; idx < struct_decl->field_order.size(); ++idx) {
        const auto &field_name = struct_decl->field_order[idx];
        auto expr_iter = field_exprs.find(field_name);
        if (expr_iter == field_exprs.end()) {
            throw std::runtime_error("StructExpr missing field: " + field_name);
        }
        auto expr = expr_iter->second;
        if (!expr) {
            throw std::runtime_error("StructExpr field expression missing");
        }
        expr->accept(*this);
        ensure_current_insertion();
        auto field_gep = builder_.create_gep(
            slot, ir_struct_type,
            {zero, builder_.create_i32_constant(static_cast<int64_t>(idx))});
        builder_.create_store(get_rvalue(expr->NodeId), field_gep);
    }

    expr_address_map_[node.NodeId] = slot;
}

// 数组字面量。
void IRGenVisitor::visit(ArrayExpr &node) {
    if (!fn_ctx_) {
        throw std::runtime_error("ArrayExpr outside of function");
    }
    auto [type, placekind] =
        node_type_and_place_kind_map_[node.NodeId];
    if (!type) {
        throw std::runtime_error("ArrayExpr missing inferred type");
    }
    auto array_type =
        std::dynamic_pointer_cast<ArrayRealType>(type);
    if (!array_type) {
        throw std::runtime_error("ArrayExpr type is not array");
    }
    auto ir_array_type = type_lowering_.lower(array_type);
    auto slot = builder_.create_temp_alloca(ir_array_type, "array.literal.slot");

    auto zero = builder_.create_i32_constant(0);
    auto element_irs = type_lowering_.lower(array_type->element_type);

    for (std::size_t idx = 0; idx < node.elements.size(); ++idx) {
        auto &elem = node.elements[idx];
        if (!elem) {
            throw std::runtime_error("ArrayExpr element missing");
        }
        elem->accept(*this);
        ensure_current_insertion();
        auto element_addr =
            builder_.create_gep(slot, ir_array_type,
                                {zero, builder_.create_i32_constant(static_cast<int64_t>(idx))});
        builder_.create_store(get_rvalue(elem->NodeId), element_addr);
    }

    expr_address_map_[node.NodeId] = slot;
}

// repeat 数组。
void IRGenVisitor::visit(RepeatArrayExpr &node) {
    if (!fn_ctx_) {
        throw std::runtime_error("RepeatArrayExpr outside of function");
    }
    auto [type, placekind] =
        node_type_and_place_kind_map_[node.NodeId];
    if (!type) {
        throw std::runtime_error("RepeatArrayExpr missing inferred type");
    }
    auto array_type =
        std::dynamic_pointer_cast<ArrayRealType>(type);
    if (!array_type) {
        throw std::runtime_error("RepeatArrayExpr type is not array");
    }
    size_t arr_len = array_type->size;
    auto ir_array_type = type_lowering_.lower(array_type);
    auto slot = builder_.create_temp_alloca(
        ir_array_type, "repeat.array.literal.slot");
    node.element->accept(*this);
    auto element_value = get_rvalue(node.element->NodeId);
    auto zero = builder_.create_i32_constant(0);
    // 改成手动写一个 while 循环来赋值，避免生成过大的 IR。
    auto idx = builder_.create_temp_alloca(
        type_lowering_.lower(std::make_shared<UsizeRealType>(ReferenceType::NO_REF)),
        "repeat.array.idx");
    builder_.create_store(zero, idx);
    auto cond = builder_.create_block("repeatarray.while.cond");
    auto body = builder_.create_block("repeatarray.while.body");
    auto after = builder_.create_block("repeatarray.while.after");
    builder_.create_br(cond);
    current_fn().current_block = cond;
    builder_.set_insertion_point(cond);

    auto current_idx_cond = builder_.create_load(idx);
    auto len_const = builder_.create_i32_constant(static_cast<int64_t>(arr_len));
    auto cmp = builder_.create_icmp_slt(current_idx_cond, len_const);
    builder_.create_cond_br(cmp, body, after);

    current_fn().current_block = body;
    builder_.set_insertion_point(body);
    auto current_idx_body = builder_.create_load(idx);
    auto element_addr = builder_.create_gep(
        slot, ir_array_type,
        {zero, current_idx_body});;
    builder_.create_store(element_value, element_addr);
    auto one_const = builder_.create_i32_constant(1);
    auto next_idx = builder_.create_add(current_idx_body, one_const);
    builder_.create_store(next_idx, idx);
    builder_.create_br(cond);
    current_fn().current_block = after;
    builder_.set_insertion_point(after);

    expr_address_map_[node.NodeId] = slot;
}

// 字段访问。
// 分两种情况，如果是函数，那么直接遍历就好了，不用多管
// 否则就是结构体的字段访问
void IRGenVisitor::visit(FieldExpr &node) {
    if (!fn_ctx_) {
        throw std::runtime_error("FieldExpr outside of function");
    }
    auto [type, placekind] =
        node_type_and_place_kind_map_[node.NodeId];
    if (!type) {
        throw std::runtime_error("FieldExpr missing inferred type");
    }
    auto &ctx = current_fn();
    if (type->kind == RealTypeKind::FUNCTION) {
        node.base->accept(*this);
        auto value_it = expr_value_map_.find(node.base->NodeId);
        if (value_it != expr_value_map_.end()) {
            expr_value_map_[node.NodeId] = value_it->second;
            return;
        }
        auto addr_it = expr_address_map_.find(node.base->NodeId);
        if (addr_it != expr_address_map_.end()) {
            expr_address_map_[node.NodeId] = addr_it->second;
            return;
        }
        throw std::runtime_error("FieldExpr function base has no value");
    } else {
        node.base->accept(*this);
        auto base_addr = get_lvalue(node.base->NodeId);
        auto [base_type, base_placekind] =
            node_type_and_place_kind_map_[node.base->NodeId];
        auto struct_type =
            std::dynamic_pointer_cast<StructRealType>(base_type);
        if (!struct_type) {
            throw std::runtime_error("FieldExpr base is not a struct");
        }
        auto decl = struct_type->decl.lock();
        if (!decl) {
            throw std::runtime_error("FieldExpr base struct missing declaration");
        }
        size_t field_index = 0;
        bool field_found = false;
        for (size_t idx = 0; idx < decl->field_order.size(); ++idx) {
            if (decl->field_order[idx] == node.field_name) {
                field_index = idx;
                field_found = true;
                break;
            }
        }
        if (!field_found) {
            throw std::runtime_error("FieldExpr field not found: " + node.field_name);
        }
        ensure_current_insertion();
        // 需要考虑自动解引用
        // 如果 base 是引用类型，那么取它的底层类型
        auto ir_struct_type = type_lowering_.lower(struct_type);
        if (base_type->is_ref != ReferenceType::NO_REF) {
            // std::cerr << "auto deref in FieldExpr\n";
            base_addr = builder_.create_load(base_addr);
            ir_struct_type = std::dynamic_pointer_cast<PointerType>(
                ir_struct_type)->pointee_type();
        }
        auto zero = builder_.create_i32_constant(0);
        auto field_addr = builder_.create_gep(
            base_addr, ir_struct_type,
            {zero, builder_.create_i32_constant(static_cast<int64_t>(field_index))});
        expr_address_map_[node.NodeId] = field_addr;
    }
}

// 索引访问。
void IRGenVisitor::visit(IndexExpr &node) {
    if (!fn_ctx_) {
        throw std::runtime_error("IndexExpr outside of function");
    }
    auto [type, placekind] =
        node_type_and_place_kind_map_[node.NodeId];
    if (!type) {
        throw std::runtime_error("IndexExpr missing inferred type");
    }
    node.base->accept(*this);
    node.index->accept(*this);
    auto base_addr = get_lvalue(node.base->NodeId);
    // 如果 base 是引用类型，那么取它的底层类型
    auto [base_type, base_placekind] =
        node_type_and_place_kind_map_[node.base->NodeId];
    auto ir_base_type = type_lowering_.lower(base_type);
    if (base_type->is_ref != ReferenceType::NO_REF) {
        base_addr = builder_.create_load(base_addr);
        auto ir_base_ptr_type = std::dynamic_pointer_cast<PointerType>(ir_base_type);
        if (!ir_base_ptr_type) {
            throw std::runtime_error("Expected pointer type after dereferencing reference");
        }
        ir_base_type = ir_base_ptr_type->pointee_type();
    }
    auto &ctx = current_fn();
    ensure_current_insertion();
    auto zero = builder_.create_i32_constant(0);
    auto index_value = get_rvalue(node.index->NodeId);
    auto element_type =
        std::dynamic_pointer_cast<ArrayRealType>(type);
    auto element_addr = builder_.create_gep(
        base_addr, ir_base_type,
        {zero, index_value});
    expr_address_map_[node.NodeId] = element_addr;
}

void IRGenVisitor::visit(LiteralExpr &node) {
    IRValue_ptr constant = nullptr;
    auto &ctx = current_fn();
    switch (node.literal_type) {
    case LiteralType::NUMBER: {
        constant = builder_.create_i32_constant(safe_stoll(node.value));
        break;
    }
    case LiteralType::BOOL: {
        auto lowered_type = type_lowering_.lower(
            std::make_shared<BoolRealType>(ReferenceType::NO_REF));
        constant = std::make_shared<ConstantValue>(
            lowered_type, node.value == "true" ? 1 : 0);
        break;
    }
    case LiteralType::CHAR: {
        if (node.value.empty()) {
            throw std::runtime_error("empty char literal");
        }
        auto lowered_type = type_lowering_.lower(
            std::make_shared<CharRealType>(ReferenceType::NO_REF));
        constant = std::make_shared<ConstantValue>(
            lowered_type, static_cast<int>(node.value.front()));
        break;
    }
    case LiteralType::STRING: {
        auto str_type = type_lowering_.lower(
            std::make_shared<StrRealType>(ReferenceType::NO_REF));
        auto previous_block = builder_.insertion_block();
        builder_.set_insertion_point(ctx.entry_block);
        constant =
            builder_.create_temp_alloca(str_type, "str.literal.slot");
        builder_.set_insertion_point(previous_block);
        auto zero = builder_.create_i32_constant(0);
        auto one = builder_.create_i32_constant(1);
        ensure_current_insertion();
        auto ptr_field = 
            builder_.create_gep(constant, str_type,
            {zero, zero});
        auto len_field =
            builder_.create_gep(constant, str_type,
            {zero, one});
        auto data_global = builder_.create_string_literal(node.value);
        builder_.create_store(data_global, ptr_field);
        builder_.create_store(builder_.create_i32_constant(node.value.size()), len_field);
        break;
    }
    default:
        throw std::runtime_error("Unsupported LiteralType in LiteralExpr");
    }
    expr_value_map_[node.NodeId] = constant;
}

// 标识符访问：let -> 地址，const -> 常量或全局。
void IRGenVisitor::visit(IdentifierExpr &node) {
    auto decl_iter = identifier_expr_to_decl_map_.find(node.NodeId);
    if (decl_iter == identifier_expr_to_decl_map_.end() || !decl_iter->second) {
        return;
    }
    auto target = decl_iter->second;
    auto &ctx = current_fn();

    if (target->kind == ValueDeclKind::LetStmt) {
        auto let_decl = std::dynamic_pointer_cast<LetDecl>(target);
        if (!let_decl) {
            return;
        }
        auto slot = ensure_slot_for_decl(let_decl);
        expr_address_map_[node.NodeId] = slot;
        return;
    }

    if (target->kind == ValueDeclKind::Constant) {
        auto const_decl = std::dynamic_pointer_cast<ConstDecl>(target);
        if (!const_decl || !const_decl->const_type) {
            return;
        }
        if (const_decl->const_type->kind == RealTypeKind::ARRAY) {
            auto ir_type = type_lowering_.lower(const_decl->const_type);
            auto global =
                std::make_shared<GlobalValue>(const_decl->name, ir_type, "");
            expr_address_map_[node.NodeId] = global;
            return;
        }
        auto const_value = const_value_map_[const_decl];
        if (!const_value) {
            throw std::runtime_error("ConstDecl has no associated ConstValue");
        }
        auto lowered =
            type_lowering_.lower_const(const_value,
                                        const_decl->const_type);
        if (lowered) {
            expr_value_map_[node.NodeId] = lowered;
        }
    }
}

// 类型转换：当前仅支持整数/布尔/字符之间的转换。
void IRGenVisitor::visit(CastExpr &node) {
    if (!fn_ctx_ || !node.expr) {
        throw std::runtime_error("Invalid CastExpr");
    }
    node.expr->accept(*this);
    auto expr_iter = node_type_and_place_kind_map_.find(node.expr->NodeId);
    auto target_iter = node_type_and_place_kind_map_.find(node.NodeId);
    if (expr_iter == node_type_and_place_kind_map_.end() ||
        target_iter == node_type_and_place_kind_map_.end() ||
        !expr_iter->second.first || !target_iter->second.first) {
        throw std::runtime_error("CastExpr missing type information");
    }
    auto src_type = expr_iter->second.first;
    auto dst_type = target_iter->second.first;
    if (src_type->is_ref != ReferenceType::NO_REF ||
        dst_type->is_ref != ReferenceType::NO_REF) {
        throw std::runtime_error("CastExpr does not support references");
    }
    auto src_ir = type_lowering_.lower(src_type);
    auto dst_ir = type_lowering_.lower(dst_type);
    auto src_int = std::dynamic_pointer_cast<IntegerType>(src_ir);
    auto dst_int = std::dynamic_pointer_cast<IntegerType>(dst_ir);
    if (!src_int || !dst_int) {
        throw std::runtime_error("CastExpr requires integer-like types");
    }
    auto src_bits = src_int->bit_width();
    auto dst_bits = dst_int->bit_width();
    auto is_signed_integer = [](RealTypeKind kind) {
        switch (kind) {
        case RealTypeKind::I32:
        case RealTypeKind::ISIZE:
        case RealTypeKind::ANYINT:
            return true;
        default:
            return false;
        }
    };

    auto value = get_rvalue(node.expr->NodeId);
    IRValue_ptr result = nullptr;
    auto &ctx = current_fn();

    if (dst_bits == 1 && src_bits == 1) {
        result = value;
    } else if (dst_bits == 1) {
        ensure_current_insertion();
        auto zero = std::make_shared<ConstantValue>(src_ir, 0);
        result = builder_.create_icmp_ne(value, zero, "cast.to.bool");
    } else if (src_bits == dst_bits) {
        result = value;
    } else if (src_bits < dst_bits) {
        ensure_current_insertion();
        if (is_signed_integer(src_type->kind)) {
            result = builder_.create_sext(value, dst_ir, "cast.sext");
        } else {
            result = builder_.create_zext(value, dst_ir, "cast.zext");
        }
    } else if (src_bits > dst_bits) {
        ensure_current_insertion();
        result = builder_.create_trunc(value, dst_ir, "cast.trunc");
    } else {
        throw std::runtime_error("Unsupported CastExpr combination");
    }

    expr_value_map_[node.NodeId] = result;
}

// Path 表达式：处理结构体关联常量与枚举变体。
void IRGenVisitor::visit(PathExpr &node) {
    if (!fn_ctx_) {
        throw std::runtime_error("PathExpr outside of function");
    }
    auto type_decl = resolve_type_decl(node.base);
    if (!type_decl) {
        throw std::runtime_error("PathExpr base type not resolved");
    }
    auto &ctx = current_fn();
    if (type_decl->kind == TypeDeclKind::Struct) {
        auto struct_decl = std::dynamic_pointer_cast<StructDecl>(type_decl);
        if (!struct_decl) {
            throw std::runtime_error("StructDecl missing for PathExpr");
        }
        auto const_iter = struct_decl->associated_const.find(node.name);
        if (const_iter == struct_decl->associated_const.end()) {
            throw std::runtime_error("Struct has no associated const: " + node.name);
        }
        auto const_decl = const_iter->second;
        if (!const_decl || !const_decl->const_type) {
            throw std::runtime_error("Invalid associated const declaration");
        }
        if (const_decl->const_type->kind == RealTypeKind::ARRAY) {
            auto ir_type = type_lowering_.lower(const_decl->const_type);
            auto global =
                std::make_shared<GlobalValue>(const_decl->name, ir_type, "");
            expr_address_map_[node.NodeId] = global;
            return;
        }
        auto const_value_iter = const_value_map_.find(const_decl);
        if (const_value_iter == const_value_map_.end() || !const_value_iter->second) {
            throw std::runtime_error("Associated const has no evaluated value");
        }
        auto lowered =
            type_lowering_.lower_const(const_value_iter->second,
                                       const_decl->const_type);
        if (!lowered) {
            throw std::runtime_error("Failed to lower associated const value");
        }
        expr_value_map_[node.NodeId] = lowered;
        return;
    }
    if (type_decl->kind == TypeDeclKind::Enum) {
        auto enum_decl = std::dynamic_pointer_cast<EnumDecl>(type_decl);
        if (!enum_decl) {
            throw std::runtime_error("EnumDecl missing for PathExpr");
        }
        auto variant_iter = enum_decl->variants.find(node.name);
        if (variant_iter == enum_decl->variants.end()) {
            throw std::runtime_error("Enum has no variant named: " + node.name);
        }
        auto type_iter = node_type_and_place_kind_map_.find(node.NodeId);
        if (type_iter == node_type_and_place_kind_map_.end() ||
            !type_iter->second.first) {
            throw std::runtime_error("Enum PathExpr missing type info");
        }
        auto lowered_type = type_lowering_.lower(type_iter->second.first);
        auto constant = std::make_shared<ConstantValue>(
            lowered_type, static_cast<int64_t>(variant_iter->second));
        expr_value_map_[node.NodeId] = constant;
        return;
    }
    throw std::runtime_error("PathExpr supports only struct or enum bases");
}

// StructItem: 递归遍历子节点但本阶段无需处理。
void IRGenVisitor::visit(StructItem &node) { return; }

// EnumItem: 同上。
void IRGenVisitor::visit(EnumItem &node) { return; }

// ImplItem: 需要遍历内部方法/常量。
void IRGenVisitor::visit(ImplItem &node) { AST_Walker::visit(node); }

// ConstItem: 全局 lowering 已处理，IR 阶段无需重复。
void IRGenVisitor::visit(ConstItem &node) { return; }

// ItemStmt: 继续遍历内部 Item。
void IRGenVisitor::visit(ItemStmt &node) { AST_Walker::visit(node); }

// self。
void IRGenVisitor::visit(SelfExpr &node) {
    if (!fn_ctx_) {
        throw std::runtime_error("SelfExpr outside of function");
    }
    auto &ctx = current_fn();
    if (!ctx.self_slot) {
        throw std::runtime_error("SelfExpr missing self slot");
    }
    expr_address_map_[node.NodeId] = ctx.self_slot;
}

// ()。
// 因为 llvm ir 不能给 void 类型赋值，所以这里什么都不做。
// 真遇到了需要 () 的地方，再换个解决方案。
void IRGenVisitor::visit(UnitExpr &node) {
    return;
}

FunctionContext &IRGenVisitor::current_fn() {
    if (!fn_ctx_) {
        // 理论上只有 visit(FnItem) 及其子树会使用 current_fn。
        throw std::runtime_error("IRGenVisitor missing active function");
    }
    return *fn_ctx_;
}

const FunctionContext &IRGenVisitor::current_fn() const {
    if (!fn_ctx_) {
        throw std::runtime_error("IRGenVisitor missing active function");
    }
    return *fn_ctx_;
}

IRValue_ptr IRGenVisitor::ensure_slot_for_decl(LetDecl_ptr decl) {
    if (!decl) {
        throw std::runtime_error("LetDecl missing for slot allocation");
    }
    auto &ctx = current_fn();
    auto iter = ctx.local_slots.find(decl);
    if (iter != ctx.local_slots.end()) {
        return iter->second;
    }
    if (!decl->let_type) {
        throw std::runtime_error("LetDecl missing type information");
    }
    // 所有局部槽都放在入口块，便于后续做 mem2reg。
    auto ir_type = type_lowering_.lower(decl->let_type);
    auto slot = builder_.create_temp_alloca(ir_type, decl->name + ".slot");
    ctx.local_slots.emplace(decl, slot);
    return slot;
}

void IRGenVisitor::branch_if_needed(BasicBlock_ptr target) {
    if (!target || !fn_ctx_) {
        return;
    }
    auto &ctx = current_fn();
    if (ctx.block_sealed || !ctx.current_block) {
        return;
    }
    // 当前块尚未终结，则补一个 br 来弥补自动 fallthrough。
    builder_.set_insertion_point(ctx.current_block);
    builder_.create_br(target);
    ctx.block_sealed = true;
}

bool IRGenVisitor::current_block_has_next(size_t node_id) const {
    return has_state(node_outcome_state_map_[node_id], OutcomeType::NEXT);
}

void IRGenVisitor::ensure_current_insertion() {
    if (!fn_ctx_) {
        throw std::runtime_error("IRGenVisitor missing active function");
    }
    auto &ctx = current_fn();
    if (!ctx.current_block) {
        ctx.current_block = ctx.entry_block;
    }
    builder_.set_insertion_point(ctx.current_block);
}

IRValue_ptr IRGenVisitor::get_rvalue(size_t node_id) {
    auto iter = expr_value_map_.find(node_id);
    if (iter != expr_value_map_.end()) {
        return iter->second;
    }
    auto addr_iter = expr_address_map_.find(node_id);
    if (addr_iter == expr_address_map_.end()) {
        throw std::runtime_error(
            "IRGenVisitor::get_rvalue missing address for node " +
            std::to_string(node_id));
    }
    auto value = builder_.create_load(addr_iter->second);
    return value;
}

IRValue_ptr IRGenVisitor::get_lvalue(size_t node_id) {
    auto addr_iter = expr_address_map_.find(node_id);
    auto &ctx = current_fn();
    if (addr_iter != expr_address_map_.end()) {    
        return addr_iter->second;
    } else {
        auto value_iter = expr_value_map_.find(node_id);
        if (value_iter == expr_value_map_.end()) {
            throw std::runtime_error("IRGenVisitor::get_lvalue missing value");
        }
        auto previous_block = builder_.insertion_block();
        // 到 entry block 分配临时槽存放值。
        builder_.set_insertion_point(ctx.entry_block);
        auto address = builder_.create_alloca(value_iter->second->type());
        builder_.set_insertion_point(previous_block);
        builder_.create_store(value_iter->second, address);
        return address;
    }
}

LoopContext *IRGenVisitor::current_loop() {
    if (!fn_ctx_ || fn_ctx_->loop_stack.empty()) {
        return nullptr;
    }
    return &fn_ctx_->loop_stack.back();
}

const LoopContext *IRGenVisitor::current_loop() const {
    if (!fn_ctx_ || fn_ctx_->loop_stack.empty()) {
        return nullptr;
    }
    return &fn_ctx_->loop_stack.back();
}

TypeDecl_ptr IRGenVisitor::resolve_type_decl(Type_ptr type_node) const {
    if (!type_node) {
        return nullptr;
    }
    auto type_iter = type_map_.find(type_node->NodeId);
    if (type_iter == type_map_.end() || !type_iter->second) {
        return nullptr;
    }
    auto real_type = type_iter->second;
    if (auto struct_type = std::dynamic_pointer_cast<StructRealType>(real_type)) {
        return struct_type->decl.lock();
    }
    if (auto enum_type = std::dynamic_pointer_cast<EnumRealType>(real_type)) {
        return enum_type->decl.lock();
    }
    return nullptr;
}

} // namespace ir
