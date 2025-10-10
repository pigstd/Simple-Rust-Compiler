#include "ast.h"
#include "lexer.h"
#include "semantic_step1.h"
#include "semantic_step2.h"
#include "visitor.h"
#include "semantic_step3.h"
#include "tools.h"
#include <cstddef>
#include <cstdint>
#include <memory>

string const_value_kind_to_string(ConstValueKind kind) {
    switch (kind) {
        case ConstValueKind::ANYINT: return "ANYINT";
        case ConstValueKind::I32: return "I32";
        case ConstValueKind::U32: return "U32";
        case ConstValueKind::ISIZE: return "ISIZE";
        case ConstValueKind::USIZE: return "USIZE";
        case ConstValueKind::BOOL: return "BOOL";
        case ConstValueKind::CHAR: return "CHAR";
        case ConstValueKind::UNIT: return "UNIT";
        case ConstValueKind::ARRAY: return "ARRAY";
        default: return "UNKNOWN";
    }
}

ConstValue_ptr ConstItemVisitor::parse_literal_token_to_const_value(LiteralType type, string value) {
    if (type == LiteralType::NUMBER) {
        // ...i32, ...u32, ...isize, ...usize, ...
        if (value.size() >= 3 && value.substr(value.size() - 3) == "i32") {
            // i32
            int int_value = static_cast<int>(safe_stoll(value.substr(0, value.size() - 3), INT32_MAX));
            return std::make_shared<I32_ConstValue>(int_value);
        } else if (value.size() >= 3 && value.substr(value.size() - 3) == "u32") {
            // u32
            unsigned int uint_value = static_cast<unsigned int>(safe_stoll(value.substr(0, value.size() - 3), UINT32_MAX));
            return std::make_shared<U32_ConstValue>(uint_value);
        } else if (value.size() >= 5 && value.substr(value.size() - 5) == "isize") {
            // isize
            int isize_value = static_cast<int>(safe_stoll(value.substr(0, value.size() - 5), INT32_MAX));
            return std::make_shared<Isize_ConstValue>(isize_value);
        } else if (value.size() >= 5 && value.substr(value.size() - 5) == "usize") {
            // usize
            unsigned int usize_value = static_cast<unsigned int>(safe_stoll(value.substr(0, value.size() - 5), UINT32_MAX));
            return std::make_shared<Usize_ConstValue>(usize_value);
        } else {
            // anyint
            long long anyint_value = safe_stoll(value, LLONG_MAX);
            return std::make_shared<AnyInt_ConstValue>(anyint_value);
        }
    } else if (type == LiteralType::BOOL) {
        if (value == "true") {
            return std::make_shared<Bool_ConstValue>(true);
        } else if (value == "false") {
            return std::make_shared<Bool_ConstValue>(false);
        } else {
            throw string("CE, invalid boolean literal: ") + value;
        }
    } else if (type == LiteralType::CHAR) {
        if (value.size() != 1) {
            throw string("CE, char literal must be a single character");
        }
        return std::make_shared<Char_ConstValue>(value[0]);
    } else {
        // 不支持 string 等 type，投降（遇到了再说）
        throw string("CE, unexpected token type for literal type: ") + literal_type_to_string(type);
    }
}

ConstValue_ptr ConstItemVisitor::calc_const_unary_expr(Unary_Operator OP, ConstValue_ptr right) {
    // 只需要考虑 - 和 !
    if (OP == Unary_Operator::NEG) {
        if (right->kind == ConstValueKind::ANYINT) {
            auto anyint_value = std::dynamic_pointer_cast<AnyInt_ConstValue>(right);
            return std::make_shared<AnyInt_ConstValue>(-anyint_value->value);
        } else if (right->kind == ConstValueKind::I32) {
            auto i32_value = std::dynamic_pointer_cast<I32_ConstValue>(right);
            return std::make_shared<I32_ConstValue>(-i32_value->value);
        } else if (right->kind == ConstValueKind::U32) {
            // u32 不能为负数
            throw string("CE, unary - operator cannot be applied to u32 type");
        } else if (right->kind == ConstValueKind::ISIZE) {
            auto isize_value = std::dynamic_pointer_cast<Isize_ConstValue>(right);
            return std::make_shared<Isize_ConstValue>(-isize_value->value);
        } else if (right->kind == ConstValueKind::USIZE) {
            // usize 不能为负数
            throw string("CE, unary - operator cannot be applied to usize type");
        } else {
            throw string("CE, unary - operator requires integer type");
        }
    } else if (OP == Unary_Operator::NOT) {
        // ! 可以给 int 用，效果是 c++ 的 ~
        if (right->kind == ConstValueKind::BOOL) {
            auto bool_value = std::dynamic_pointer_cast<Bool_ConstValue>(right);
            return std::make_shared<Bool_ConstValue>(!bool_value->value);
        } else if (right->kind == ConstValueKind::ANYINT) {
            auto anyint_value = std::dynamic_pointer_cast<AnyInt_ConstValue>(right);
            // 直接全部取反
            // 不清楚会不会有问题，有问题再说
            return std::make_shared<AnyInt_ConstValue>(~anyint_value->value);
        } else if (right->kind == ConstValueKind::I32) {
            auto i32_value = std::dynamic_pointer_cast<I32_ConstValue>(right);
            return std::make_shared<I32_ConstValue>(~i32_value->value);
        } else if (right->kind == ConstValueKind::U32) {
            auto u32_value = std::dynamic_pointer_cast<U32_ConstValue>(right);
            return std::make_shared<U32_ConstValue>(~u32_value->value);
        } else if (right->kind == ConstValueKind::ISIZE) {
            auto isize_value = std::dynamic_pointer_cast<Isize_ConstValue>(right);
            return std::make_shared<Isize_ConstValue>(~isize_value->value);
        } else if (right->kind == ConstValueKind::USIZE) {
            auto usize_value = std::dynamic_pointer_cast<Usize_ConstValue>(right);
            return std::make_shared<Usize_ConstValue>(~usize_value->value);
        } else {
            throw string("CE, unary ! operator requires bool or integer type");
        }
    } else {
        throw string("CE, unexpected unary operator, type: ") + unary_operator_to_string(OP);
    }
}

ConstValue_ptr ConstItemVisitor::calc_const_binary_expr(Binary_Operator OP, ConstValue_ptr left, ConstValue_ptr right) {
    // 需要考虑的： + - * / % ^ | & 
    // == != < > <= >=
    // && ||
    // << >>
    switch (OP) {
        // 处理 + - * / % ^ | &
        case Binary_Operator::ADD:
        case Binary_Operator::SUB:
        case Binary_Operator::MUL:
        case Binary_Operator::DIV:
        case Binary_Operator::MOD:
        case Binary_Operator::XOR:
        case Binary_Operator::OR:
        case Binary_Operator::AND: { 
            auto calc_result = [&](auto L, auto R) {
                if (OP == Binary_Operator::ADD) return L + R;
                if (OP == Binary_Operator::SUB) return L - R;
                if (OP == Binary_Operator::MUL) return L * R;
                if (OP == Binary_Operator::DIV) {
                    if (R == 0) throw string("CE, division by zero in constant expression");
                    return L / R;
                }
                if (OP == Binary_Operator::MOD) {
                    if (R == 0) throw string("CE, modulo by zero in constant expression");
                    return L % R;
                }
                if (OP == Binary_Operator::XOR) return L ^ R;
                if (OP == Binary_Operator::OR) return L | R;
                if (OP == Binary_Operator::AND) return L & R;
                throw string("CE, unknown binary operator");
            };
            if (left->kind == ConstValueKind::ANYINT && right->kind == ConstValueKind::ANYINT) {
                auto L = std::dynamic_pointer_cast<AnyInt_ConstValue>(left)->value;
                auto R = std::dynamic_pointer_cast<AnyInt_ConstValue>(right)->value;
                return std::make_shared<AnyInt_ConstValue>(calc_result(L, R));
            } else if (left->kind == ConstValueKind::ANYINT || right->kind == ConstValueKind::ANYINT) {
                // 一个是 anyint，另一个不是
                // 先把 anyint 转成另一个的类型
                if (left->kind == ConstValueKind::ANYINT) {
                    left = cast_anyint_const_to_target_type(std::dynamic_pointer_cast<AnyInt_ConstValue>(left), right->kind);
                } else {
                    right = cast_anyint_const_to_target_type(std::dynamic_pointer_cast<AnyInt_ConstValue>(right), left->kind);
                }
            }
            // 现在两个都不是 anyint
            if (left->kind != right->kind) {
                throw string("CE, binary operator requires both operands to be of the same type");
            }
            if (left->kind == ConstValueKind::I32) {
                auto L = std::dynamic_pointer_cast<I32_ConstValue>(left)->value;
                auto R = std::dynamic_pointer_cast<I32_ConstValue>(right)->value;
                return std::make_shared<I32_ConstValue>(calc_result(L, R));
            } else if (left->kind == ConstValueKind::U32) {
                auto L = std::dynamic_pointer_cast<U32_ConstValue>(left)->value;
                auto R = std::dynamic_pointer_cast<U32_ConstValue>(right)->value;
                return std::make_shared<U32_ConstValue>(calc_result(L, R));
            } else if (left->kind == ConstValueKind::ISIZE) {
                auto L = std::dynamic_pointer_cast<Isize_ConstValue>(left)->value;
                auto R = std::dynamic_pointer_cast<Isize_ConstValue>(right)->value;
                return std::make_shared<Isize_ConstValue>(calc_result(L, R));
            } else if (left->kind == ConstValueKind::USIZE) {
                auto L = std::dynamic_pointer_cast<Usize_ConstValue>(left)->value;
                auto R = std::dynamic_pointer_cast<Usize_ConstValue>(right)->value;
                return std::make_shared<Usize_ConstValue>(calc_result(L, R));
            } else if (left->kind == ConstValueKind::BOOL) {
                // BOOL 可以做 AND OR XOR
                if (OP == Binary_Operator::AND || OP == Binary_Operator::OR || OP == Binary_Operator::XOR) {
                    auto L = std::dynamic_pointer_cast<Bool_ConstValue>(left)->value;
                    auto R = std::dynamic_pointer_cast<Bool_ConstValue>(right)->value;
                    return std::make_shared<Bool_ConstValue>(calc_result(L, R));
                }
            }
            throw string("CE, ") + binary_operator_to_string(OP) + " operator don't support operands of type " + const_value_kind_to_string(left->kind);
            break;
        }
        // 处理 == != < > <= >=
        case Binary_Operator::EQ:
        case Binary_Operator::NEQ:
        case Binary_Operator::LT:
        case Binary_Operator::GT:
        case Binary_Operator::LEQ:
        case Binary_Operator::GEQ: {
            auto cmp_result = [&](auto L, auto R) -> bool {
                if (OP == Binary_Operator::EQ) return L == R;
                if (OP == Binary_Operator::NEQ) return L != R;
                if (OP == Binary_Operator::LT) return L < R;
                if (OP == Binary_Operator::GT) return L > R;
                if (OP == Binary_Operator::LEQ) return L <= R;
                if (OP == Binary_Operator::GEQ) return L >= R;
                throw string("CE, unknown binary operator");
            };
            if (left->kind == ConstValueKind::ANYINT && right->kind == ConstValueKind::ANYINT) {
                auto L = std::dynamic_pointer_cast<AnyInt_ConstValue>(left)->value;
                auto R = std::dynamic_pointer_cast<AnyInt_ConstValue>(right)->value;
                return std::make_shared<Bool_ConstValue>(cmp_result(L, R));
            } else if (left->kind == ConstValueKind::ANYINT || right->kind == ConstValueKind::ANYINT) {
                // 一个是 anyint，另一个不是
                // 先把 anyint 转成另一个的类型
                if (left->kind == ConstValueKind::ANYINT) {
                    left = cast_anyint_const_to_target_type(std::dynamic_pointer_cast<AnyInt_ConstValue>(left), right->kind);
                } else {
                    right = cast_anyint_const_to_target_type(std::dynamic_pointer_cast<AnyInt_ConstValue>(right), left->kind);
                }
            }
            // 现在两个都不是 anyint
            if (left->kind != right->kind) {
                throw string("CE, binary operator requires both operands to be of the same type");
            }
            if (left->kind == ConstValueKind::I32) {
                auto L = std::dynamic_pointer_cast<I32_ConstValue>(left)->value;
                auto R = std::dynamic_pointer_cast<I32_ConstValue>(right)->value;
                return std::make_shared<Bool_ConstValue>(cmp_result(L, R));
            } else if (left->kind == ConstValueKind::U32) {
                auto L = std::dynamic_pointer_cast<U32_ConstValue>(left)->value;
                auto R = std::dynamic_pointer_cast<U32_ConstValue>(right)->value;
                return std::make_shared<Bool_ConstValue>(cmp_result(L, R));
            } else if (left->kind == ConstValueKind::ISIZE) {
                auto L = std::dynamic_pointer_cast<Isize_ConstValue>(left)->value;
                auto R = std::dynamic_pointer_cast<Isize_ConstValue>(right)->value;
                return std::make_shared<Bool_ConstValue>(cmp_result(L, R));
            } else if (left->kind == ConstValueKind::USIZE) {
                auto L = std::dynamic_pointer_cast<Usize_ConstValue>(left)->value;
                auto R = std::dynamic_pointer_cast<Usize_ConstValue>(right)->value;
                return std::make_shared<Bool_ConstValue>(cmp_result(L, R));
            } else if (left->kind == ConstValueKind::BOOL) {
                auto L = std::dynamic_pointer_cast<Bool_ConstValue>(left)->value;
                auto R = std::dynamic_pointer_cast<Bool_ConstValue>(right)->value;
                return std::make_shared<Bool_ConstValue>(cmp_result(L, R));
            }
            throw string("CE, ") + binary_operator_to_string(OP) + " operator don't support operands of type " + const_value_kind_to_string(left->kind);
        }
        // 处理 && ||
        case Binary_Operator::AND_AND:
        case Binary_Operator::OR_OR: {
            if (left->kind != ConstValueKind::BOOL || right->kind != ConstValueKind::BOOL) {
                throw string("CE, logical operator requires both operands to be BOOL type");
            }
            auto L = std::dynamic_pointer_cast<Bool_ConstValue>(left)->value;
            auto R = std::dynamic_pointer_cast<Bool_ConstValue>(right)->value;
            if (OP == Binary_Operator::AND_AND) {
                return std::make_shared<Bool_ConstValue>(L && R);
            } else {
                return std::make_shared<Bool_ConstValue>(L || R);
            }
        }
        // 处理 << >>
        case Binary_Operator::SHL:
        case Binary_Operator::SHR: {
            auto shift_result = [&](auto L, size_t R) {
                if (OP == Binary_Operator::SHL) {
                    if (R >= 32) {
                        throw string("CE, shift count too large");
                    }
                    return L << R;
                }
                if (OP == Binary_Operator::SHR) return L >> R;
                throw string("CE, unknown binary operator");
            };
            size_t R;
            if (right->kind == ConstValueKind::ANYINT) {
                auto anyint_value = std::dynamic_pointer_cast<AnyInt_ConstValue>(right);
                if (anyint_value->value < 0) {
                    throw string("CE, shift count cannot be negative");
                }
                R = static_cast<size_t>(anyint_value->value);
            } else if (right->kind == ConstValueKind::I32) {
                auto i32_value = std::dynamic_pointer_cast<I32_ConstValue>(right);
                if (i32_value->value < 0) {
                    throw string("CE, shift count cannot be negative");
                }
                R = static_cast<size_t>(i32_value->value);
            } else if (right->kind == ConstValueKind::U32) {
                auto u32_value = std::dynamic_pointer_cast<U32_ConstValue>(right);
                R = static_cast<size_t>(u32_value->value);
            } else if (right->kind == ConstValueKind::ISIZE) {
                auto isize_value = std::dynamic_pointer_cast<Isize_ConstValue>(right);
                if (isize_value->value < 0) {
                    throw string("CE, shift count cannot be negative");
                }
                R = static_cast<size_t>(isize_value->value);
            } else if (right->kind == ConstValueKind::USIZE) {
                auto usize_value = std::dynamic_pointer_cast<Usize_ConstValue>(right);
                R = static_cast<size_t>(usize_value->value);
            } else {
                throw string("CE, shift count must be an integer type");
            }
            if (left->kind == ConstValueKind::ANYINT) {
                auto L = std::dynamic_pointer_cast<AnyInt_ConstValue>(left)->value;
                return std::make_shared<AnyInt_ConstValue>(shift_result(L, R));
            } else if (left->kind == ConstValueKind::I32) {
                auto L = std::dynamic_pointer_cast<I32_ConstValue>(left)->value;
                return std::make_shared<I32_ConstValue>(shift_result(L, R));
            } else if (left->kind == ConstValueKind::U32) {
                auto L = std::dynamic_pointer_cast<U32_ConstValue>(left)->value;
                return std::make_shared<U32_ConstValue>(shift_result(L, R));
            } else if (left->kind == ConstValueKind::ISIZE) {
                auto L = std::dynamic_pointer_cast<Isize_ConstValue>(left)->value;
                return std::make_shared<Isize_ConstValue>(shift_result(L, R));
            } else if (left->kind == ConstValueKind::USIZE) {
                auto L = std::dynamic_pointer_cast<Usize_ConstValue>(left)->value;
                return std::make_shared<Usize_ConstValue>(shift_result(L, R));
            } else {
                throw string("CE, shift operator requires left operand to be an integer type");
            }
        }
        default: {
            throw string("CE, unexpected binary operator in const expr, type: ") + binary_operator_to_string(OP);
        }
    }
}

ConstValue_ptr ConstItemVisitor::cast_anyint_const_to_target_type(AnyInt_ConstValue_ptr anyint_value, ConstValueKind target_kind) {
    long long value = anyint_value->value;
    switch (target_kind) {
        case ConstValueKind::I32: {
            if (value < INT32_MIN || value > INT32_MAX) {
                throw string("CE, value out of range for i32");
            }
            return std::make_shared<I32_ConstValue>(static_cast<int>(value));
        }
        case ConstValueKind::U32: {
            if (value < 0 || value > UINT32_MAX) {
                throw string("CE, value out of range for u32");
            }
            return std::make_shared<U32_ConstValue>(static_cast<unsigned int>(value));
        }
        case ConstValueKind::ISIZE: {
            if (value < INT32_MIN || value > INT32_MAX) {
                throw string("CE, value out of range for isize");
            }
            return std::make_shared<Isize_ConstValue>(static_cast<int>(value));
        }
        case ConstValueKind::USIZE: {
            if (value < 0 || value > UINT32_MAX) {
                throw string("CE, value out of range for usize");
            }
            return std::make_shared<Usize_ConstValue>(static_cast<unsigned int>(value));
        }
        default:
            throw string("CE, cannot cast ANYINT to ") + const_value_kind_to_string(target_kind);
    }
}

ConstValue_ptr ConstItemVisitor::const_cast_to_realtype(ConstValue_ptr value, RealType_ptr target_type) {
    switch (target_type->kind) {
        case RealTypeKind::BOOL: {
            // 数字转化为 BOOL : 先不写
            if (value->kind != ConstValueKind::BOOL) {
                throw string("CE, cannot cast non-bool type to bool");
            }
            return value;
        }
        case RealTypeKind::CHAR: {
            // 数字转化为 CHAR : 先不写
            if (value->kind != ConstValueKind::CHAR) {
                throw string("CE, cannot cast non-char type to char");
            }
            return value;
        }
        case RealTypeKind::UNIT: {
            if (value->kind != ConstValueKind::UNIT) {
                throw string("CE, cannot cast non-unit type to unit");
            }
            return value;
        }
        case RealTypeKind::ARRAY: {
            if (value->kind != ConstValueKind::ARRAY) {
                throw string("CE, cannot cast non-array type to array");
            }
            auto array_value = std::dynamic_pointer_cast<Array_ConstValue>(value);
            auto target_array_type = std::dynamic_pointer_cast<ArrayRealType>(target_type);
            size_t target_size = const_expr_to_size_map[target_array_type->size_expr->NodeId];
            if (array_value->elements.size() != target_size) {
                throw string("CE, array size mismatch in const cast");
            }
            vector<ConstValue_ptr> casted_elements;
            for (auto &elem : array_value->elements) {
                casted_elements.push_back(const_cast_to_realtype(elem, target_array_type->element_type));
            }
            return std::make_shared<Array_ConstValue>(casted_elements);
        }
        case RealTypeKind::I32:
        case RealTypeKind::U32:
        case RealTypeKind::ISIZE:
        case RealTypeKind::USIZE: {
            // 先转化到数字，然后再转换回去
            long long num;
            if (value->kind == ConstValueKind::ANYINT) {
                auto anyint_value = std::dynamic_pointer_cast<AnyInt_ConstValue>(value);
                num = anyint_value->value;
            } else if (value->kind == ConstValueKind::I32) {
                auto i32_value = std::dynamic_pointer_cast<I32_ConstValue>(value);
                num = i32_value->value;
            } else if (value->kind == ConstValueKind::U32) {
                auto u32_value = std::dynamic_pointer_cast<U32_ConstValue>(value);
                num = u32_value->value;
            } else if (value->kind == ConstValueKind::ISIZE) {
                auto isize_value = std::dynamic_pointer_cast<Isize_ConstValue>(value);
                num = isize_value->value;
            } else if (value->kind == ConstValueKind::USIZE) {
                auto usize_value = std::dynamic_pointer_cast<Usize_ConstValue>(value);
                num = usize_value->value;
            } else if (value->kind == ConstValueKind::CHAR) {
                auto ch_value = std::dynamic_pointer_cast<Char_ConstValue>(value);
                num = ch_value->value;
            } else if (value->kind == ConstValueKind::BOOL) {
                auto bool_value = std::dynamic_pointer_cast<Bool_ConstValue>(value);
                num = bool_value->value ? 1 : 0;
            } else {
                throw string("CE, can't convert ") + const_value_kind_to_string(value->kind)
                + " to " + real_type_kind_to_string(target_type->kind);
            }
            if (target_type->kind == RealTypeKind::I32) {
                if (num < INT32_MIN || num > INT32_MAX) {
                    throw string("CE, value out of range for i32");
                }
                return std::make_shared<I32_ConstValue>(static_cast<int>(num));
            } else if (target_type->kind == RealTypeKind::U32) {
                if (num < 0 || num > UINT32_MAX) {
                    throw string("CE, value out of range for u32");
                }
                return std::make_shared<U32_ConstValue>(static_cast<unsigned int>(num));
            } else if (target_type->kind == RealTypeKind::ISIZE) {
                if (num < INT32_MIN || num > INT32_MAX) {
                    throw string("CE, value out of range for isize");
                }
                return std::make_shared<Isize_ConstValue>(static_cast<int>(num));
            } else if (target_type->kind == RealTypeKind::USIZE) {
                if (num < 0 || num > UINT32_MAX) {
                    throw string("CE, value out of range for usize");
                }
                return std::make_shared<Usize_ConstValue>(static_cast<unsigned int>(num));
            } else {
                throw string("CE, can't convert number to unknown type ");
            }
        }
        default: {
            throw string("CE, unsupported target type for const cast: ") + real_type_kind_to_string(target_type->kind);
        }
    }
}

size_t ConstItemVisitor::calc_const_array_size(ConstValue_ptr size_value) {
    size_t size;
    if (size_value->kind == ConstValueKind::ANYINT) {
        auto anyint_value = std::dynamic_pointer_cast<AnyInt_ConstValue>(size_value);
        if (anyint_value->value < 0) {
            throw string("CE, array size cannot be negative");
        }
        size = static_cast<size_t>(anyint_value->value);
    } else if (size_value->kind == ConstValueKind::USIZE) {
        auto usize_value = std::dynamic_pointer_cast<Usize_ConstValue>(size_value);
        size = static_cast<size_t>(usize_value->value);
    } else {
        throw string("CE, array size must be usize or anyint type");
    }
    return size;
}

void OtherTypeAndRepeatArrayVisitor::visit(LiteralExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(IdentifierExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(BinaryExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(UnaryExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(CallExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(FieldExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(StructExpr &node) {
    AST_Walker::visit(node);    
    // 解析 node.struct_name
    find_real_type(node_scope_map[node.struct_name->NodeId], node.struct_name, type_map, const_expr_queue);
}
void OtherTypeAndRepeatArrayVisitor::visit(IndexExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(BlockExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(IfExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(WhileExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(LoopExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(ReturnExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(BreakExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(ContinueExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(CastExpr &node) {
    // 将 as 后面的类型解析出来
    AST_Walker::visit(node);
    find_real_type(node_scope_map[node.target_type->NodeId], node.target_type, type_map, const_expr_queue);
}
void OtherTypeAndRepeatArrayVisitor::visit(PathExpr &node) {
    AST_Walker::visit(node);
    // 解析 node.base
    find_real_type(node_scope_map[node.base->NodeId], node.base, type_map, const_expr_queue);
}
void OtherTypeAndRepeatArrayVisitor::visit(SelfExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(UnitExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(ArrayExpr &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(RepeatArrayExpr &node) {
    const_expr_queue.push_back(node.size);
    AST_Walker::visit(node);
}
void OtherTypeAndRepeatArrayVisitor::visit(FnItem &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(StructItem &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(EnumItem &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(ImplItem &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(ConstItem &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(LetStmt &node) {
    if (node.type != nullptr) {
        find_real_type(node_scope_map[node.NodeId], node.type, type_map, const_expr_queue);
        // 这里不管返回值，因为 type_map 里面已经存了
    }
    AST_Walker::visit(node);
}
void OtherTypeAndRepeatArrayVisitor::visit(ExprStmt &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(ItemStmt &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(PathType &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(ArrayType &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(UnitType &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(SelfType &node) { AST_Walker::visit(node); }
void OtherTypeAndRepeatArrayVisitor::visit(IdentifierPattern &node) { AST_Walker::visit(node); }

void ConstItemVisitor::visit(LiteralExpr &node) {
    if (is_need_to_calculate) {
        const_value = parse_literal_token_to_const_value(node.literal_type, node.value);
        is_need_to_calculate = false;
    }
    AST_Walker::visit(node);
}
void ConstItemVisitor::visit(IdentifierExpr &node) {
    if (is_need_to_calculate) {
        auto const_decl = find_const_decl(node_scope_map[node.NodeId], node.name);
        if (const_decl == nullptr) {
            throw string("CE, undefined constant: ") + node.name;
        }
        if (const_value_map.find(const_decl) == const_value_map.end()) {
            throw string("CE, constant not evaluated yet: ") + node.name;
        }
        const_value = const_value_map[const_decl];
        is_need_to_calculate = false;
    }
    AST_Walker::visit(node);
}
void ConstItemVisitor::visit(BinaryExpr &node) {
    if (is_need_to_calculate) {
        // 先计算左右
        ConstValue_ptr left_value;
        ConstValue_ptr right_value;
        // calc left
        is_need_to_calculate = true;
        node.left->accept(*this);
        left_value = const_value;
        const_value = nullptr;
        // calc right
        is_need_to_calculate = true;
        node.right->accept(*this);
        right_value = const_value;
        const_value = nullptr;
        // 计算结果
        const_value = calc_const_binary_expr(node.op, left_value, right_value);
        is_need_to_calculate = false;
    }
    else { AST_Walker::visit(node); }
}
void ConstItemVisitor::visit(UnaryExpr &node) {
    if (is_need_to_calculate) {
        // 先计算右边
        ConstValue_ptr right_value;
        // calc right
        is_need_to_calculate = true;
        node.right->accept(*this);
        right_value = const_value;
        const_value = nullptr;
        // 计算结果
        const_value = calc_const_unary_expr(node.op, right_value);
        is_need_to_calculate = false;
    }
    else { AST_Walker::visit(node); }
}
void ConstItemVisitor::visit(CallExpr &node) { 
    if (is_need_to_calculate) {
        throw string("CE, function call not allowed in constant expression");
    }
    AST_Walker::visit(node);
}
void ConstItemVisitor::visit(FieldExpr &node) { 
    if (is_need_to_calculate) {
        throw string("CE, field access not allowed in constant expression");
    }
    AST_Walker::visit(node); 
}
void ConstItemVisitor::visit(StructExpr &node) { 
    if (is_need_to_calculate) {
        throw string("CE, struct construction not allowed in constant expression");
    }
    AST_Walker::visit(node); 
}
void ConstItemVisitor::visit(IndexExpr &node) { 
    if (is_need_to_calculate) {
        is_need_to_calculate = true;
        node.base->accept(*this);
        ConstValue_ptr base_value = const_value;
        const_value = nullptr;
        is_need_to_calculate = true;
        node.index->accept(*this);
        ConstValue_ptr index_value = const_value;
        const_value = nullptr;
        size_t index = calc_const_array_size(index_value);
        // base_value 一定要是数组
        if (base_value->kind != ConstValueKind::ARRAY) {
            throw string("CE, base of index expression must be an array in constant expression");
        }
        auto array_value = std::dynamic_pointer_cast<Array_ConstValue>(base_value);
        if (array_value->elements.size() <= index) {
            throw string("CE, array index out of bounds in constant expression");
        }
        const_value = array_value->elements[index];
        is_need_to_calculate = false;
    }
    else { AST_Walker::visit(node); }
}
void ConstItemVisitor::visit(BlockExpr &node) { 
    if (is_need_to_calculate) {
        throw string("CE, block not allowed in constant expression");
    }
    AST_Walker::visit(node); 
}
void ConstItemVisitor::visit(IfExpr &node) { 
    if (is_need_to_calculate) {
        throw string("CE, if expression not allowed in constant expression");
    }
    AST_Walker::visit(node); 
}
void ConstItemVisitor::visit(WhileExpr &node) { 
    if (is_need_to_calculate) {
        throw string("CE, while loop not allowed in constant expression");
    }
    AST_Walker::visit(node); 
}
void ConstItemVisitor::visit(LoopExpr &node) { 
    if (is_need_to_calculate) {
        throw string("CE, loop not allowed in constant expression");
    }
    AST_Walker::visit(node); 
}
void ConstItemVisitor::visit(ReturnExpr &node) { 
    if (is_need_to_calculate) {
        throw string("CE, return not allowed in constant expression");
    }
    AST_Walker::visit(node); 
}
void ConstItemVisitor::visit(BreakExpr &node) { 
    if (is_need_to_calculate) {
        throw string("CE, break not allowed in constant expression");
    }
    AST_Walker::visit(node); 
}
void ConstItemVisitor::visit(ContinueExpr &node) { 
    if (is_need_to_calculate) {
        throw string("CE, continue not allowed in constant expression");
    }
    AST_Walker::visit(node); 
}
void ConstItemVisitor::visit(CastExpr &node) {
    if (is_need_to_calculate) {
        // 先计算值
        ConstValue_ptr value;
        // calc value
        is_need_to_calculate = true;
        node.expr->accept(*this);
        value = const_value;
        const_value = nullptr;
        // 计算结果
        const_value = const_cast_to_realtype(value, type_map[node.target_type->NodeId]);
        is_need_to_calculate = false;
    }
    else { AST_Walker::visit(node); }
}
void ConstItemVisitor::visit(PathExpr &node) { 
    if (is_need_to_calculate) {
        throw string("CE, path expression not allowed in constant expression");
    }
    AST_Walker::visit(node); 
}
void ConstItemVisitor::visit(SelfExpr &node) { 
    if (is_need_to_calculate) {
        throw string("CE, self expression not allowed in constant expression");
    }
    AST_Walker::visit(node); 
}
void ConstItemVisitor::visit(UnitExpr &node) { 
    if (is_need_to_calculate) {
        const_value = std::make_shared<Unit_ConstValue>();
        is_need_to_calculate = false;
    }
    else { AST_Walker::visit(node); }
}
void ConstItemVisitor::visit(ArrayExpr &node) {
    if (is_need_to_calculate) {
        vector<ConstValue_ptr> elem;
        for (auto expr : node.elements) {
            is_need_to_calculate = true;
            expr->accept(*this);
            elem.push_back(const_value);
            const_value = nullptr;
        }
        is_need_to_calculate = false;
        const_value = std::make_shared<Array_ConstValue>(elem);
    }
    else { AST_Walker::visit(node); }
}
void ConstItemVisitor::visit(RepeatArrayExpr &node) {
    if (is_need_to_calculate) {
        is_need_to_calculate = true;
        node.element->accept(*this);
        ConstValue_ptr element_value = const_value;
        const_value = nullptr;
        is_need_to_calculate = true;
        node.size->accept(*this);
        ConstValue_ptr size_value = const_value;
        const_value = nullptr;

        size_t array_size = calc_const_array_size(size_value);
        vector<ConstValue_ptr> elem(array_size, element_value);
        const_value = std::make_shared<Array_ConstValue>(elem);
        is_need_to_calculate = false;
    }
    else { AST_Walker::visit(node); }
}
void ConstItemVisitor::visit(FnItem &node) {
    if (is_need_to_calculate) {
        throw string("CE, function definition not allowed in constant expression");
    }
    AST_Walker::visit(node);
}
void ConstItemVisitor::visit(StructItem &node) {
    if (is_need_to_calculate) {
        throw string("CE, struct definition not allowed in constant expression");
    }
    AST_Walker::visit(node);
}
void ConstItemVisitor::visit(EnumItem &node) {
    if (is_need_to_calculate) {
        throw string("CE, enum definition not allowed in constant expression");
    }
    AST_Walker::visit(node);
}
void ConstItemVisitor::visit(ImplItem &node) {
    if (is_need_to_calculate) {
        throw string("CE, impl definition not allowed in constant expression");
    }
    AST_Walker::visit(node);
}
void ConstItemVisitor::visit(ConstItem &node) {
    if (is_need_to_calculate) {
        throw string("CE, const definition not allowed in constant expression");
    }
    // 需要计算这个 const item 的值
    // 先递归 type
    // 如果是数组，就要把表达式也给算出来，并且存在 const_expr_to_size_map 里面
    is_need_to_calculate = true;
    node.const_type->accept(*this);
    auto const_decl = find_const_decl(node_scope_map[node.NodeId], node.const_name);
    // 找到定义
    if (const_decl == nullptr) {
        throw string("CE, undefined constant: ") + node.const_name;
    }
    is_need_to_calculate = true;
    node.value->accept(*this);
    if (const_value == nullptr) {
        // 这个情况不应该发生，发生了说明代码写错了没找到
        throw string("CE, failed to evaluate constant: ") + node.const_name;
    }
    const_value = const_cast_to_realtype(const_value, type_map[node.const_type->NodeId]);
    const_value_map[const_decl] = const_value;
    const_value = nullptr;
    is_need_to_calculate = false;
}
void ConstItemVisitor::visit(LetStmt &node) {
    if (is_need_to_calculate) {
        throw string("CE, let statement not allowed in constant expression");
    }
    AST_Walker::visit(node); 
}
void ConstItemVisitor::visit(ExprStmt &node) {
    if (is_need_to_calculate) {
        throw string("CE, expression statement not allowed in constant expression");
    }
    AST_Walker::visit(node); 
}
void ConstItemVisitor::visit(ItemStmt &node) {
    if (is_need_to_calculate) {
        throw string("CE, item statement not allowed in constant expression");
    }
    AST_Walker::visit(node); 
}
void ConstItemVisitor::visit(PathType &node) {
    if (is_need_to_calculate) {
        is_need_to_calculate = false;
    }
    AST_Walker::visit(node);
}
void ConstItemVisitor::visit(ArrayType &node) {
    if (is_need_to_calculate) {
        node.element_type->accept(*this);
        is_need_to_calculate = true;
        node.size_expr->accept(*this);
        size_t array_size = calc_const_array_size(const_value);
        const_value = nullptr;
        is_need_to_calculate = false;
        const_expr_to_size_map[node.size_expr->NodeId] = array_size;
    }
    else { AST_Walker::visit(node); }
}
void ConstItemVisitor::visit(UnitType &node) {
    if (is_need_to_calculate) {
        is_need_to_calculate = false;
    }
    AST_Walker::visit(node);
}
void ConstItemVisitor::visit(SelfType &node) {
    if (is_need_to_calculate) {
        throw string("CE, self type not allowed in constant expression");
    }
    AST_Walker::visit(node);
}
void ConstItemVisitor::visit(IdentifierPattern &node) {
    if (is_need_to_calculate) {
        throw string("CE, pattern not allowed in constant expression");
    }
    AST_Walker::visit(node);
}

ConstDecl_ptr find_const_decl(Scope_ptr NowScope, string name) {
    while (NowScope != nullptr) {
        for (auto decl : NowScope->value_namespace) {
            if (decl.first == name) {
                return std::dynamic_pointer_cast<ConstDecl>(decl.second);
            }
        }
        NowScope = NowScope->parent.lock();
    }
    return nullptr;
}

TypeDecl_ptr find_type_decl(Scope_ptr NowScope, string name) {
    while (NowScope != nullptr) {
        for (auto decl : NowScope->type_namespace) {
            if (decl.first == name) {
                return decl.second;
            }
        }
        NowScope = NowScope->parent.lock();
    }
    return nullptr;
}

OutcomeState get_outcome_state(vector<OutcomeType> states) {
    OutcomeState result_state;
    for (auto state : states) {
        size_t stateid = static_cast<size_t>(state);
        result_state[stateid] = true;
    }
    return result_state;
}

bool has_state(const OutcomeState &states, OutcomeType state) {
    size_t stateid = static_cast<size_t>(state);
    return states[stateid];
}

OutcomeState sequence_outcome_state(const OutcomeState &first, const OutcomeState &second) {
    if (has_state(first, OutcomeType::NEXT)) {
        auto result = first;
        result[static_cast<size_t>(OutcomeType::NEXT)] = false;
        result |= second;
        return result;
    } else {
        return first;
    }
}

OutcomeState ifelse_outcome_state(const OutcomeState &Condition, const OutcomeState &if_branch, const OutcomeState &else_branch) {
    if (!has_state(Condition, OutcomeType::NEXT)) {
        return Condition;
    }
    auto result = Condition;
    result[static_cast<size_t>(OutcomeType::NEXT)] = false;
    result |= if_branch | else_branch;
    return result;
}

OutcomeState while_outcome_state(const OutcomeState &Condition, const OutcomeState &body) {
    if (!has_state(Condition, OutcomeType::NEXT)) {
        return Condition;
    }    
    // Condition 一定有 NEXT，body 有可能不运行，所以还是可以有 NEXT
    auto result = Condition;
    if (has_state(body, OutcomeType::DIVERGE)) {
        result[static_cast<size_t>(OutcomeType::DIVERGE)] = true;
    }
    if (has_state(body, OutcomeType::RETURN)) {
        result[static_cast<size_t>(OutcomeType::RETURN)] = true;
    }
    return result;
}

OutcomeState loop_outcome_state(const OutcomeState &body) {
    if (has_state(body, OutcomeType::BREAK)) {
        auto result = get_outcome_state({OutcomeType::NEXT});
        if (has_state(body, OutcomeType::DIVERGE)) {
            result[static_cast<size_t>(OutcomeType::DIVERGE)] = true;
        }
        if (has_state(body, OutcomeType::RETURN)) {
            result[static_cast<size_t>(OutcomeType::RETURN)] = true;
        }
        return result;
    } else {
        // 有可能死循环或者 return 了
        auto result = get_outcome_state({OutcomeType::DIVERGE});
        if (has_state(body, OutcomeType::RETURN)) {
            result[static_cast<size_t>(OutcomeType::RETURN)] = true;
        }
        return result;
    }
}

void ControlFlowVisitor::visit(LiteralExpr &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}

void ControlFlowVisitor::visit(IdentifierExpr &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}

void ControlFlowVisitor::visit(BinaryExpr &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = sequence_outcome_state(
        node_outcome_state_map[node.left->NodeId],
        node_outcome_state_map[node.right->NodeId]
    );
}
void ControlFlowVisitor::visit(UnaryExpr &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}
void ControlFlowVisitor::visit(CallExpr &node) {
    AST_Walker::visit(node);
    OutcomeState state = get_outcome_state({OutcomeType::NEXT});
    for (auto arg : node.arguments) {
        state = sequence_outcome_state(state, node_outcome_state_map[arg->NodeId]);
    }
    node_outcome_state_map[node.NodeId] = state;
}
void ControlFlowVisitor::visit(FieldExpr &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = node_outcome_state_map[node.base->NodeId];
}
void ControlFlowVisitor::visit(StructExpr &node) {
    AST_Walker::visit(node);
    OutcomeState state = get_outcome_state({OutcomeType::NEXT});
    for (auto &[type, expr] : node.fields) {
        state = sequence_outcome_state(state, node_outcome_state_map[expr->NodeId]);
    }
    node_outcome_state_map[node.NodeId] = state;
}
void ControlFlowVisitor::visit(IndexExpr &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = sequence_outcome_state(
        node_outcome_state_map[node.base->NodeId],
        node_outcome_state_map[node.index->NodeId]
    );
}
void ControlFlowVisitor::visit(BlockExpr &node) {
    AST_Walker::visit(node);
    OutcomeState state = get_outcome_state({OutcomeType::NEXT});
    for (auto stmt : node.statements) {
        state = sequence_outcome_state(state, node_outcome_state_map[stmt->NodeId]);
    }
    if (node.tail_statement != nullptr) {
        state = sequence_outcome_state(state, node_outcome_state_map[node.tail_statement->NodeId]);
    }
    node_outcome_state_map[node.NodeId] = state;
}
void ControlFlowVisitor::visit(IfExpr &node) {
    AST_Walker::visit(node);
    OutcomeState state = ifelse_outcome_state(
        node_outcome_state_map[node.condition->NodeId],
        node_outcome_state_map[node.then_branch->NodeId],
        node.else_branch == nullptr ? get_outcome_state({OutcomeType::NEXT}) : node_outcome_state_map[node.else_branch->NodeId]
    );
    node_outcome_state_map[node.NodeId] = state;
}
void ControlFlowVisitor::visit(WhileExpr &node) {
    loop_depth++;
    AST_Walker::visit(node);
    loop_depth--;
    OutcomeState state = while_outcome_state(
        node_outcome_state_map[node.condition->NodeId],
        node_outcome_state_map[node.body->NodeId]
    );
    node_outcome_state_map[node.NodeId] = state;
}
void ControlFlowVisitor::visit(LoopExpr &node) {
    loop_depth++;
    AST_Walker::visit(node);
    loop_depth--;
    OutcomeState state = loop_outcome_state(
        node_outcome_state_map[node.body->NodeId]
    );
    node_outcome_state_map[node.NodeId] = state;
}
void ControlFlowVisitor::visit(ReturnExpr &node) {
    // 需要检查是否在函数内 (?)
    // 其实（应该）不需要检查，因为在 const 里面会报错，其他地方的表达式一定是在函数内 (?)
    // 真的遇到怪情况了再说
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::RETURN});
    // 有没有可能 return 里面嵌套了一些怪东西（嵌套 break 等等）？
    // 先不管
}
void ControlFlowVisitor::visit(BreakExpr &node) {
    if (loop_depth == 0) {
        throw string("CE, break statement not in loop");
    }
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::BREAK});
    // 同上，break 里面套了怪东西也不管
}
void ControlFlowVisitor::visit(ContinueExpr &node) {
    if (loop_depth == 0) {
        throw string("CE, continue statement not in loop");
    }
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::BREAK});
    // 同上，continue 里面套了怪东西也不管
}
void ControlFlowVisitor::visit(CastExpr &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = node_outcome_state_map[node.expr->NodeId];
}
void ControlFlowVisitor::visit(PathExpr &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}
void ControlFlowVisitor::visit(SelfExpr &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}
void ControlFlowVisitor::visit(UnitExpr &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}
void ControlFlowVisitor::visit(ArrayExpr &node) {
    AST_Walker::visit(node);
    OutcomeState state = get_outcome_state({OutcomeType::NEXT});
    for (auto expr : node.elements) {
        state = sequence_outcome_state(state, node_outcome_state_map[expr->NodeId]);
    }
    node_outcome_state_map[node.NodeId] = state;
}
void ControlFlowVisitor::visit(RepeatArrayExpr &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = node_outcome_state_map[node.element->NodeId];
}

void ControlFlowVisitor::visit(FnItem &node) {
    // 需要把当前的 loop_depth 归零
    auto prev_loop_depth = loop_depth;
    loop_depth = 0;
    AST_Walker::visit(node);
    loop_depth = prev_loop_depth;
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}
void ControlFlowVisitor::visit(StructItem &node) { 
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}
void ControlFlowVisitor::visit(EnumItem &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}
void ControlFlowVisitor::visit(ImplItem &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}
void ControlFlowVisitor::visit(ConstItem &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}

void ControlFlowVisitor::visit(LetStmt &node) {
    AST_Walker::visit(node);
    // let 好像不能没有 initializer
    // 先这么写了，到时候再看看
    if (node.initializer == nullptr) {
        // let x: i32;
        node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
    } else {
        node_outcome_state_map[node.NodeId] = node_outcome_state_map[node.initializer->NodeId];
    }
}
void ControlFlowVisitor::visit(ExprStmt &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = node_outcome_state_map[node.expr->NodeId];
}
void ControlFlowVisitor::visit(ItemStmt &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}
void ControlFlowVisitor::visit(PathType &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}
void ControlFlowVisitor::visit(ArrayType &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}
void ControlFlowVisitor::visit(UnitType &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}
void ControlFlowVisitor::visit(SelfType &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}
void ControlFlowVisitor::visit(IdentifierPattern &node) {
    AST_Walker::visit(node);
    node_outcome_state_map[node.NodeId] = get_outcome_state({OutcomeType::NEXT});
}