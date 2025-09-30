#include "ast.h"
#include "lexer.h"
#include "semantic_step2.h"
#include "visitor.h"
#include <cstddef>
#include <memory>
#include "semantic_step3.h"

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
        default: return "UNKNOWN";
    }
}

ConstValue_ptr parse_literal_token_to_const_value(LiteralType type, string value) {
    if (type == LiteralType::NUMBER) {
        // ..._i32, ..._u32, ..._isize, ..._usize, ...
        if (value.size() >= 4 && value.substr(value.size() - 4) == "_i32") {
            // i32
            int int_value = std::stoi(value.substr(0, value.size() - 4));
            return std::make_shared<I32_ConstValue>(int_value);
        } else if (value.size() >= 4 && value.substr(value.size() - 4) == "_u32") {
            // u32
            unsigned int uint_value = static_cast<unsigned int>(std::stoul(value.substr(0, value.size() - 4)));
            return std::make_shared<U32_ConstValue>(uint_value);
        } else if (value.size() >= 6 && value.substr(value.size() - 6) == "_isize") {
            // isize
            int isize_value = std::stoi(value.substr(0, value.size() - 6));
            return std::make_shared<Isize_ConstValue>(isize_value);
        } else if (value.size() >= 6 && value.substr(value.size() - 6) == "_usize") {
            // usize
            unsigned int usize_value = static_cast<unsigned int>(std::stoul(value.substr(0, value.size() - 6)));
            return std::make_shared<Usize_ConstValue>(usize_value);
        } else {
            // anyint
            long long anyint_value = std::stoll(value);
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

ConstValue_ptr calc_const_unary_expr(Unary_Operator OP, ConstValue_ptr right) {
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
        if (right->kind != ConstValueKind::BOOL) {
            throw string("CE, unary ! operator requires BOOL type");
        }
        auto bool_value = std::dynamic_pointer_cast<Bool_ConstValue>(right);
        return std::make_shared<Bool_ConstValue>(!bool_value->value);
    } else {
        throw string("CE, unexpected unary operator, type: ") + unary_operator_to_string(OP);
    }
}

ConstValue_ptr calc_const_binary_expr(Binary_Operator OP, ConstValue_ptr left, ConstValue_ptr right) {
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

ConstValue_ptr cast_anyint_const_to_target_type(AnyInt_ConstValue_ptr anyint_value, ConstValueKind target_kind) {
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

ConstValue_ptr calc_const_cast_expr(ConstValue_ptr value, Type_ptr target_type) {
    // target_type 只能是 path type
    // 目前支持：i32 u32 isize usize bool char
    auto path_type = std::dynamic_pointer_cast<PathType>(target_type);
    if (path_type == nullptr) {
        // 支持不了，遇到了再说
        throw string("CE, cast target type must be a path type");
    }
    string type_name = path_type->name;
    if (type_name == "char") {
        // 数字可以转化为 char
        // 先不考虑截断的事情，直接强制转换
        size_t char_value;
        if (value->kind == ConstValueKind::ANYINT) {
            auto anyint_value = std::dynamic_pointer_cast<AnyInt_ConstValue>(value);
            char_value = static_cast<size_t>(anyint_value->value);
        } else if (value->kind == ConstValueKind::I32) {
            auto i32_value = std::dynamic_pointer_cast<I32_ConstValue>(value);
            char_value = static_cast<size_t>(i32_value->value);
        } else if (value->kind == ConstValueKind::U32) {
            auto u32_value = std::dynamic_pointer_cast<U32_ConstValue>(value);
            char_value = static_cast<size_t>(u32_value->value);
        } else if (value->kind == ConstValueKind::ISIZE) {
            auto isize_value = std::dynamic_pointer_cast<Isize_ConstValue>(value);
            char_value = static_cast<size_t>(isize_value->value);
        } else if (value->kind == ConstValueKind::USIZE) {
            auto usize_value = std::dynamic_pointer_cast<Usize_ConstValue>(value);
            char_value = static_cast<size_t>(usize_value->value);
        } else if (value->kind == ConstValueKind::CHAR) {
            auto ch_value = std::dynamic_pointer_cast<Char_ConstValue>(value);
            char_value = static_cast<size_t>(ch_value->value);
        } else {
            throw "CE, can't convert " + const_value_kind_to_string(value->kind) + " to char";
        }
        char ch = static_cast<char>(char_value); // 截断
        return std::make_shared<Char_ConstValue>(ch);
    } else if (type_name == "bool") {
        // 只能 bool 转化为 bool
        if (value->kind == ConstValueKind::BOOL) {
            return value;
        } else {
            throw "CE, can't convert " + const_value_kind_to_string(value->kind) + " to bool";
        }
    } else {
        // 一定是转化成数字
        // 先找到数字大小，再转化回去
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
            throw "CE, can't convert " + const_value_kind_to_string(value->kind) + " to " + type_name;
        }
        if (type_name == "i32") {
            if (num < INT32_MIN || num > INT32_MAX) {
                throw string("CE, value out of range for i32");
            }
            return std::make_shared<I32_ConstValue>(static_cast<int>(num));
        } else if (type_name == "u32") {
            if (num < 0 || num > UINT32_MAX) {
                throw string("CE, value out of range for u32");
            }
            return std::make_shared<U32_ConstValue>(static_cast<unsigned int>(num));
        } else if (type_name == "isize") {
            if (num < INT32_MIN || num > INT32_MAX) {
                throw string("CE, value out of range for isize");
            }
            return std::make_shared<Isize_ConstValue>(static_cast<int>(num));
        } else if (type_name == "usize") {
            if (num < 0 || num > UINT32_MAX) {
                throw string("CE, value out of range for usize");
            }
            return std::make_shared<Usize_ConstValue>(static_cast<unsigned int>(num));
        } else {
            throw "CE, can't convert to unknown type " + type_name;
        }
    }
}

void LetStmtAndRepeatArrayVisitor::visit(LiteralExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(IdentifierExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(BinaryExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(UnaryExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(CallExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(FieldExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(StructExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(IndexExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(BlockExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(IfExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(WhileExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(LoopExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(ReturnExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(BreakExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(ContinueExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(CastExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(PathExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(SelfExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(UnitExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(ArrayExpr &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(RepeatArrayExpr &node) {
    const_expr_queue.push_back(node.size);
    AST_Walker::visit(node);
}
void LetStmtAndRepeatArrayVisitor::visit(FnItem &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(StructItem &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(EnumItem &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(ImplItem &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(ConstItem &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(LetStmt &node) {
    if (node.type != nullptr) {
        find_real_type(node_scope_map[std::make_shared<LetStmt>(node)], node.type, type_map, const_expr_queue);
        // 这里不管返回值，因为 type_map 里面已经存了
    }
    AST_Walker::visit(node);
}
void LetStmtAndRepeatArrayVisitor::visit(ExprStmt &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(ItemStmt &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(PathType &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(ArrayType &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(UnitType &node) { AST_Walker::visit(node); }
void LetStmtAndRepeatArrayVisitor::visit(IdentifierPattern &node) { AST_Walker::visit(node); }

void ConstItemVisitor::visit(LiteralExpr &node) {
    if (is_need_to_calculate) {
        const_value = parse_literal_token_to_const_value(node.literal_type, node.value);
        is_need_to_calculate = false;
    }
    AST_Walker::visit(node);
}
void ConstItemVisitor::visit(IdentifierExpr &node) {
    if (is_need_to_calculate) {
        auto const_decl = find_const_decl(node_scope_map[std::make_shared<IdentifierExpr>(node)], node.name);
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
        throw string("CE, indexing not allowed in constant expression");
    }
    AST_Walker::visit(node); 
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
        const_value = calc_const_cast_expr(value, node.target_type);
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
    AST_Walker::visit(node); 
}
void ConstItemVisitor::visit(ArrayExpr &node) { 
    if (is_need_to_calculate) {
        throw string("CE, array expression not allowed in constant expression");
    }
    AST_Walker::visit(node); 
}
void ConstItemVisitor::visit(RepeatArrayExpr &node) { 
    if (is_need_to_calculate) {
        throw string("CE, repeat array expression not allowed in constant expression");
    }
    AST_Walker::visit(node); 
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
    node.const_type->accept(*this);
    auto const_decl = find_const_decl(node_scope_map[std::make_shared<IdentifierExpr>(node)], node.const_name);
    // 找到定义
    if (const_decl == nullptr) {
        throw string("CE, undefined constant: ") + node.const_name;
    }
    is_need_to_calculate = true;
    node.value->accept(*this);
    if (const_value == nullptr) {
        throw string("CE, failed to evaluate constant: ") + node.const_name;
    }
    const_value = calc_const_cast_expr(const_value, node.const_type);
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
        throw string("CE, type not allowed in constant expression");
    }
    AST_Walker::visit(node);
}
void ConstItemVisitor::visit(ArrayType &node) {
    if (is_need_to_calculate) {
        throw string("CE, type not allowed in constant expression");
    }
    AST_Walker::visit(node);
}
void ConstItemVisitor::visit(UnitType &node) {
    if (is_need_to_calculate) {
        throw string("CE, type not allowed in constant expression");
    }
    AST_Walker::visit(node);
}
void ConstItemVisitor::visit(IdentifierPattern &node) {
    if (is_need_to_calculate) {
        throw string("CE, pattern not allowed in constant expression");
    }
    AST_Walker::visit(node);
}