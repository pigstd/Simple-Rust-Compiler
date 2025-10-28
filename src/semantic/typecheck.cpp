#include "ast/ast.h"
#include "semantic/decl.h"
#include "semantic/scope.h"
#include "semantic/type.h"
#include "semantic/typecheck.h"
#include "ast/visitor.h"
#include "tools/tools.h"
#include <cassert>
#include <cstddef>
// #include <iostream>
#include <memory>
// #include <ostream>

// 只有遇到 ArrayType 的时候，才会用到这个 visitor
// 其他节点都直接调用 AST_Walker 的 visit 即可

void ArrayTypeVisitor::visit(LiteralExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(IdentifierExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(BinaryExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(UnaryExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(CallExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(FieldExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(StructExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(IndexExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(BlockExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(IfExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(WhileExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(LoopExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(ReturnExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(BreakExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(ContinueExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(CastExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(PathExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(SelfExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(UnitExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(ArrayExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(RepeatArrayExpr &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(FnItem &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(StructItem &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(EnumItem &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(ImplItem &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(ConstItem &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(LetStmt &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(ExprStmt &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(ItemStmt &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(PathType &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(ArrayType &node) {
    auto target_real_type = type_map[node.NodeId];
    assert(target_real_type != nullptr);
    assert(target_real_type->kind == RealTypeKind::ARRAY);
    auto array_real_type = std::dynamic_pointer_cast<ArrayRealType>(target_real_type);
    if (!array_real_type) {
        // 这个时候应该是前面代码写错了，不是 CE，报 Error 的错误
        throw string("Error, should be array type");
    }
    size_t array_size = 0;
    if (const_expr_to_size_map.find(node.size_expr->NodeId) != const_expr_to_size_map.end()) {
        array_size = const_expr_to_size_map[node.size_expr->NodeId];
    } else {
        // 这个时候应该是前面代码写错了，不是 CE，报 Error 的错误
        throw string("Error, cannot find array size");
    }
    array_real_type->size = array_size;
    AST_Walker::visit(node);
}
void ArrayTypeVisitor::visit(UnitType &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(SelfType &node) { AST_Walker::visit(node); }
void ArrayTypeVisitor::visit(IdentifierPattern &node) { AST_Walker::visit(node); }

bool type_is_number(RealType_ptr checktype) {
    assert(checktype != nullptr);
    if (checktype->kind == RealTypeKind::NEVER ||
        checktype->kind == RealTypeKind::ANYINT || 
        checktype->kind == RealTypeKind::I32 || 
        checktype->kind == RealTypeKind::ISIZE ||
        checktype->kind == RealTypeKind::U32 ||
        checktype->kind == RealTypeKind::USIZE) { return true; }
    return false;
}

// rust 支持 &mut -> & 的隐式转换
// 但是赋值的时候要注意只能从 &mut T 赋值给 &T，不能反过来
// 如果是 赋值操作，则 is_assignment = true
RealType_ptr type_merge(RealType_ptr left, RealType_ptr right, bool is_assignment) {
    assert(left != nullptr && right != nullptr);
    if (left->kind == RealTypeKind::NEVER) { return right; }
    if (right->kind == RealTypeKind::NEVER) { return left; }
    ReferenceType result_ref;
    RealType_ptr result_type;
    if (left->is_ref == right->is_ref) {
        result_ref = left->is_ref;
    } else if (left->is_ref == ReferenceType::REF_MUT && right->is_ref == ReferenceType::REF) {
        if (is_assignment) {
            throw string("CE, cannot assign &T to &mut T\n type info:")
            + left->show_real_type_info() + " and " + right->show_real_type_info();
        } else {
            result_ref = ReferenceType::REF;
        }
    } else if (left->is_ref == ReferenceType::REF && right->is_ref == ReferenceType::REF_MUT) {
        result_ref = ReferenceType::REF;
    } else {
        // std::cerr << "!!!" << reference_type_to_string(left->is_ref) << ' '
        // << reference_type_to_string(right->is_ref) << std::endl;
        throw string("CE, cannot merge two different reference types\n type info:")
            + left->show_real_type_info() + " and " + right->show_real_type_info();
    }
    if (left->kind == RealTypeKind::ANYINT) {
        if (type_is_number(right)) { result_type = copy(right); }
        else {
            throw string("CE, cannot merge AnyInt with non-number type\n type info:")
            + left->show_real_type_info() + " and " + right->show_real_type_info();
        }
    } else if (right->kind == RealTypeKind::ANYINT) {
        if (type_is_number(left)) { result_type = copy(left); }
        else {
            throw string("CE, cannot merge AnyInt with non-number type\n type info:")
            + left->show_real_type_info() + " and " + right->show_real_type_info();
        }
    } else {
        // 其他情况的 type 必须完全相同
        if (left->kind != right->kind) {
            throw string("CE, cannot merge two different types\n type info:")
                + left->show_real_type_info() + " and " + right->show_real_type_info();
        }
        if (left->kind == RealTypeKind::ARRAY) {
            auto left_array = std::dynamic_pointer_cast<ArrayRealType>(left);
            auto right_array = std::dynamic_pointer_cast<ArrayRealType>(right);
            assert(left_array != nullptr && right_array != nullptr);
            if (left_array->size != right_array->size) {
                throw string("CE, cannot merge two different array types with size\n type info:")
                + left->show_real_type_info() + " and " + right->show_real_type_info();
            }
            auto merged_element_type = type_merge(left_array->element_type, right_array->element_type, is_assignment);
            result_type = std::make_shared<ArrayRealType>(merged_element_type, nullptr, left->is_ref, left_array->size);
        } else {
            if (left->kind == RealTypeKind::STRUCT) {
                auto left_struct = std::dynamic_pointer_cast<StructRealType>(left);
                auto right_struct = std::dynamic_pointer_cast<StructRealType>(right);
                assert(left_struct != nullptr && right_struct != nullptr);
                if (left_struct->decl.lock() != right_struct->decl.lock()) {
                    throw string("CE, cannot merge two different struct types\n type info:")
                    + left->show_real_type_info() + " and " + right->show_real_type_info();
                }
            } else if (left->kind == RealTypeKind::ENUM) {
                auto left_enum = std::dynamic_pointer_cast<EnumRealType>(left);
                auto right_enum = std::dynamic_pointer_cast<EnumRealType>(right);
                assert(left_enum != nullptr && right_enum != nullptr);
                if (left_enum->decl.lock() != right_enum->decl.lock()) {
                    throw string("CE, cannot merge two different enum types\n type info:")
                    + left->show_real_type_info() + " and " + right->show_real_type_info();
                }
            }
            result_type = copy(left);
        }
    }
    result_type->is_ref = result_ref;
    return result_type;
}

RealType_ptr type_of_literal(LiteralType type, string value) {
    if (type == LiteralType::STRING) {
        // 类型是 &str
        return std::make_shared<StrRealType>(ReferenceType::REF);
    } else if (type == LiteralType::CHAR) {
        return std::make_shared<CharRealType>(ReferenceType::NO_REF);
    } else if (type == LiteralType::BOOL) {
        return std::make_shared<BoolRealType>(ReferenceType::NO_REF);
    } else if (type == LiteralType::NUMBER) {
        // 看后缀，以及要 check 是否是合法的数字
        if (value.size() >= 3 && value.substr(value.size() - 3) == "i32") {
            // i32
            safe_stoll(value.substr(0, value.size() - 3), INT32_MAX);
            return std::make_shared<I32RealType>(ReferenceType::NO_REF);
        } else if (value.size() >= 3 && value.substr(value.size() - 3) == "u32") {
            // u32
            safe_stoll(value.substr(0, value.size() - 3), UINT32_MAX);
            return std::make_shared<U32RealType>(ReferenceType::NO_REF);
        } else if (value.size() >= 5 && value.substr(value.size() - 5) == "isize") {
            // isize
            safe_stoll(value.substr(0, value.size() - 5), INT32_MAX);
            return std::make_shared<IsizeRealType>(ReferenceType::NO_REF);
        } else if (value.size() >= 5 && value.substr(value.size() - 5) == "usize") {
            // usize
            safe_stoll(value.substr(0, value.size() - 5), UINT32_MAX);
            return std::make_shared<UsizeRealType>(ReferenceType::NO_REF);
        } else {
            // anyint
            safe_stoll(value, LLONG_MAX);
            return std::make_shared<AnyIntRealType>(ReferenceType::NO_REF);
        }
    } else {
        throw string("Error, unknown literal type");
    }
}

RealType_ptr copy(RealType_ptr type) {
    assert(type != nullptr);
    if (type->kind == RealTypeKind::ARRAY) {
        auto array_type = std::dynamic_pointer_cast<ArrayRealType>(type);
        assert(array_type != nullptr);
        return std::make_shared<ArrayRealType>(copy(array_type->element_type), nullptr, type->is_ref, array_type->size);
    } else if (type->kind == RealTypeKind::FUNCTION) {
        auto fn_type = std::dynamic_pointer_cast<FunctionRealType>(type);
        assert(fn_type != nullptr);
        return std::make_shared<FunctionRealType>(fn_type->decl.lock(), type->is_ref);
    } else if (type->kind == RealTypeKind::STRUCT) {
        auto struct_type = std::dynamic_pointer_cast<StructRealType>(type);
        assert(struct_type != nullptr);
        return std::make_shared<StructRealType>(struct_type->name, type->is_ref, struct_type->decl.lock());
    } else if (type->kind == RealTypeKind::ENUM) {
        auto enum_type = std::dynamic_pointer_cast<EnumRealType>(type);
        assert(enum_type != nullptr);
        return std::make_shared<EnumRealType>(enum_type->name, type->is_ref, enum_type->decl.lock());
    } else if (type->kind == RealTypeKind::STR) {
        return std::make_shared<StrRealType>(type->is_ref);
    } else if (type->kind == RealTypeKind::STRING) {
        return std::make_shared<StringRealType>(type->is_ref);
    } else if (type->kind == RealTypeKind::I32) {
        return std::make_shared<I32RealType>(type->is_ref);
    } else if (type->kind == RealTypeKind::U32) {
        return std::make_shared<U32RealType>(type->is_ref);
    } else if (type->kind == RealTypeKind::ISIZE) {
        return std::make_shared<IsizeRealType>(type->is_ref);
    } else if (type->kind == RealTypeKind::USIZE) {
        return std::make_shared<UsizeRealType>(type->is_ref);
    } else if (type->kind == RealTypeKind::ANYINT) {
        return std::make_shared<AnyIntRealType>(type->is_ref);
    } else if (type->kind == RealTypeKind::BOOL) {
        return std::make_shared<BoolRealType>(type->is_ref);
    } else if (type->kind == RealTypeKind::CHAR) {
        return std::make_shared<CharRealType>(type->is_ref);
    } else if (type->kind == RealTypeKind::UNIT) {
        return std::make_shared<UnitRealType>(type->is_ref);
    } else if (type->kind == RealTypeKind::NEVER) {
        return std::make_shared<NeverRealType>(type->is_ref);
    } else {
        throw string("Error, copy unknown real type");
    }
}

void ExprTypeAndLetStmtVisitor::visit(LiteralExpr &node) {
    if (require_function) {
        throw string("CE, literal is not function");
    }
    auto literal_type = type_of_literal(node.literal_type, node.value);
    node_type_and_place_kind_map[node.NodeId] = {literal_type, PlaceKind::NotPlace};
}
void ExprTypeAndLetStmtVisitor::visit(IdentifierExpr &node) {
    // std::cerr << "start identifier expr visit: " << node.name << "\n";
    auto decl = find_value_decl(node_scope_map[node.NodeId], node.name);
    if (decl == nullptr) {
        throw string("CE, cannot find identifier declaration: ") + node.name;
    }
    RealType_ptr result_type = nullptr;
    PlaceKind place_kind = PlaceKind::NotPlace;
    if (require_function) {
        if (decl->kind == ValueDeclKind::Function) {
            auto fn_decl = std::dynamic_pointer_cast<FnDecl>(decl);
            assert(fn_decl != nullptr);
            result_type = std::make_shared<FunctionRealType>(fn_decl, ReferenceType::NO_REF);
        }
        else {
            throw string("CE, identifier is not function: ") + node.name;
        }
    } else {
        if (decl->kind == ValueDeclKind::Constant) {
            auto const_decl = std::dynamic_pointer_cast<ConstDecl>(decl);
            assert(const_decl != nullptr);
            result_type = const_decl->const_type;
            place_kind = PlaceKind::NotPlace;
        } else if (decl->kind == ValueDeclKind::LetStmt) {
            auto let_decl = std::dynamic_pointer_cast<LetDecl>(decl);
            assert(let_decl != nullptr);
            result_type = let_decl->let_type;
            if (let_decl->is_mut == Mutibility::MUTABLE) {
                place_kind = PlaceKind::ReadWritePlace;
            } else {
                place_kind = PlaceKind::ReadOnlyPlace;
            }
        } else {
            throw string("CE, identifier is not variable or constant : ") + node.name;
        }
    }
    assert(result_type != nullptr);
    node_type_and_place_kind_map[node.NodeId] = {result_type, place_kind};
    // std::cerr << "end identifier expr visit: " << node.name << "\n";
}
void ExprTypeAndLetStmtVisitor::visit(BinaryExpr &node) {
    if (require_function) {
        throw string("CE, binary expression is not function");
    }
    AST_Walker::visit(node);
    auto [left_type, left_place] = node_type_and_place_kind_map[node.left->NodeId];
    auto [right_type, right_place] = node_type_and_place_kind_map[node.right->NodeId];
    RealType_ptr result_type = nullptr;
    if (left_type->is_ref != ReferenceType::NO_REF ||
        right_type->is_ref != ReferenceType::NO_REF) {
        throw string("CE, binary operator cannot have reference type operands");
    }
    switch (node.op) {
        // 只能是数字的
        case Binary_Operator::ADD:
        case Binary_Operator::SUB:
        case Binary_Operator::MUL:
        case Binary_Operator::DIV:
        case Binary_Operator::MOD: {
            if (type_is_number(left_type) && type_is_number(right_type)) {
                result_type = type_merge(left_type, right_type);
            } else {
                throw string("CE, binary operator ") + binary_operator_to_string(node.op) + " requires number types";
            }
            break;
        }
        // 可以是数字或者 bool 的
        case Binary_Operator::AND:
        case Binary_Operator::OR:
        case Binary_Operator::XOR: {
            if ((type_is_number(left_type) || left_type->kind == RealTypeKind::BOOL) &&
                (type_is_number(right_type) || right_type->kind == RealTypeKind::BOOL)) {
                result_type = type_merge(left_type, right_type);
            } else {
                throw string("CE, binary operator ") + binary_operator_to_string(node.op) + " requires number or bool types";
            }
            break;
        }
        // 两边都是 bool
        case Binary_Operator::AND_AND:
        case Binary_Operator::OR_OR: {
            if (left_type->kind == RealTypeKind::BOOL &&
                right_type->kind == RealTypeKind::BOOL) {
                result_type = type_merge(left_type, right_type);
            } else {
                throw string("CE, binary operator ") + binary_operator_to_string(node.op) + " requires bool types";
            }
            break;
        }
        case Binary_Operator::EQ:
        case Binary_Operator::NEQ:
        case Binary_Operator::LT:
        case Binary_Operator::GT:
        case Binary_Operator::LEQ:
        case Binary_Operator::GEQ: {
            // 必须要类型相同
            // 不确定一些奇怪的类型支不支持比较（比如 struct）
            // 先假设支持
            if (type_merge(left_type, right_type) != nullptr) {
                result_type = std::make_shared<BoolRealType>(ReferenceType::NO_REF);
            } else {
                throw string("CE, binary operator ") + binary_operator_to_string(node.op) + " requires two same types";
            }
            break;
        }
        case Binary_Operator::SHL:
        case Binary_Operator::SHR: {
            // 只要左右都是数字就行
            if (type_is_number(left_type) && type_is_number(right_type)) {
                result_type = left_type;
            } else {
                throw string("CE, binary operator ") + binary_operator_to_string(node.op) + " requires number types";
            }
            break;
        }
        case Binary_Operator::ASSIGN: {
            if (left_place != PlaceKind::ReadWritePlace) {
                throw string("CE, left side of assignment must be a mutable place");
            }
            // 如果类型不同，type_merge 的时候就直接会报错
            type_merge(left_type, right_type, true);
            result_type = std::make_shared<UnitRealType>(ReferenceType::NO_REF);
            break;
        }
        case Binary_Operator::ADD_ASSIGN:
        case Binary_Operator::SUB_ASSIGN:
        case Binary_Operator::MUL_ASSIGN:
        case Binary_Operator::DIV_ASSIGN:
        case Binary_Operator::MOD_ASSIGN: {
            if (left_place != PlaceKind::ReadWritePlace) {
                throw string("CE, left side of assignment must be a mutable place");
            }
            if (type_is_number(left_type) && type_is_number(right_type)) {
                // 如果类型不同，type_merge 的时候就直接会报错
                type_merge(left_type, right_type, true);
                result_type = std::make_shared<UnitRealType>(ReferenceType::NO_REF);
            } else {
                throw string("CE, binary operator ") + binary_operator_to_string(node.op) + " requires number types";
            }
            break;
        }
        case Binary_Operator::AND_ASSIGN:
        case Binary_Operator::OR_ASSIGN:
        case Binary_Operator::XOR_ASSIGN: {
            if (left_place != PlaceKind::ReadWritePlace) {
                throw string("CE, left side of assignment must be a mutable place");
            }
            if ((type_is_number(left_type) || left_type->kind == RealTypeKind::BOOL) &&
                (type_is_number(right_type) || right_type->kind == RealTypeKind::BOOL)) {
                // 如果类型不同，type_merge 的时候就直接会报错
                type_merge(left_type, right_type, true);
                result_type = std::make_shared<UnitRealType>(ReferenceType::NO_REF);
            } else {
                throw string("CE, binary operator ") + binary_operator_to_string(node.op) + " requires number or bool types";
            }
            break;
        }
        case Binary_Operator::SHL_ASSIGN:
        case Binary_Operator::SHR_ASSIGN: {
            if (left_place != PlaceKind::ReadWritePlace) {
                throw string("CE, left side of assignment must be a mutable place");
            }
            if (type_is_number(left_type) && type_is_number(right_type)) {
                result_type = std::make_shared<UnitRealType>(ReferenceType::NO_REF);
            } else {
                throw string("CE, binary operator ") + binary_operator_to_string(node.op) + " requires number types";
            }
        }
    }
    assert(result_type != nullptr);
    node_type_and_place_kind_map[node.NodeId] =
        {result_type, PlaceKind::NotPlace};
}
void ExprTypeAndLetStmtVisitor::visit(UnaryExpr &node) {
    /*
    !!!! remark:
    关于 & 和 &mut，rust 的语法和 c++ 有区别
    尤其是右值好像是可以引用的
    我需要再确认一下，这里的代码可能需要修改
    !!!!
    */
    if (require_function) {
        throw string("CE, unary expression is not function");
    }
    AST_Walker::visit(node);
    auto [right_type, right_place] = node_type_and_place_kind_map[node.right->NodeId];
    RealType_ptr result_type = nullptr;
    PlaceKind place_kind;
    if (node.op == Unary_Operator::NEG) {
        if (right_type->is_ref != ReferenceType::NO_REF) {
            throw string("CE, unary operator - cannot have reference type operand");
        }
        if (type_is_number(right_type)) {
            result_type = right_type;
            place_kind = PlaceKind::NotPlace;
        } else {
            throw string("CE, unary operator - requires number type");
        }
    } else if (node.op == Unary_Operator::NOT) {
        if (right_type->is_ref != ReferenceType::NO_REF) {
            throw string("CE, unary operator ! cannot have reference type operand");
        }
        // ! 也可以用于 整数
        if (right_type->kind == RealTypeKind::BOOL || type_is_number(right_type)) {
            result_type = right_type;
            place_kind = PlaceKind::NotPlace;
        } else {
            throw string("CE, unary operator ! requires bool type");
        }
    } else if (node.op == Unary_Operator::REF) {
        // 右值也可以被引用，所以都可以被引用
        if (right_type->is_ref != ReferenceType::NO_REF) {
            throw string("CE, cannot take reference of a reference type");
        }
        // 由于会修改，所以就深拷贝一下吧
        // 前面的地方直接赋值，没关系的
        result_type = copy(right_type);
        result_type->is_ref = ReferenceType::REF;
        place_kind = PlaceKind::NotPlace;
    } else if (node.op == Unary_Operator::REF_MUT) {
        // 右值和 mut 的左值都可以被 &mut 引用
        // 但是不可变的左值不行
        if (right_place == PlaceKind::ReadOnlyPlace) {
            throw string("CE, unary operator &mut requires a mutable place operand");
        }
        if (right_type->is_ref != ReferenceType::NO_REF) {
            throw string("CE, cannot take reference of a reference type");
        }
        result_type = copy(right_type);
        result_type->is_ref = ReferenceType::REF_MUT;
        place_kind = PlaceKind::NotPlace;
    } else if (node.op == Unary_Operator::DEREF) {
        if (right_type->is_ref == ReferenceType::NO_REF) {
            throw string("CE, unary operator * requires a reference type operand");
        }
        result_type = copy(right_type);
        result_type->is_ref = ReferenceType::NO_REF;
        if (right_type->is_ref == ReferenceType::REF) {
            place_kind = PlaceKind::ReadOnlyPlace;
        } else {
            place_kind = PlaceKind::ReadWritePlace;
        }
    } else {
        throw string("CE, unexpected unary operator: ") + unary_operator_to_string(node.op);
    }
    assert(result_type != nullptr);
    node_type_and_place_kind_map[node.NodeId] =
        {result_type, place_kind};
}
void ExprTypeAndLetStmtVisitor::visit(CallExpr &node) {
    if (require_function) {
        throw string("CE, call expression is not function");
    }
    require_function = true;
    node.callee->accept(*this);
    require_function = false;
    vector<pair<RealType_ptr, PlaceKind>> arg_types;
    auto [callee_type, callee_place] = node_type_and_place_kind_map[node.callee->NodeId];
    if (callee_type->kind != RealTypeKind::FUNCTION) {
        throw string("CE, call expression requires a function type callee");
    }
    for (auto &arg : node.arguments) {
        arg->accept(*this);
        arg_types.push_back(node_type_and_place_kind_map[arg->NodeId]);
    }
    auto fn_type = std::dynamic_pointer_cast<FunctionRealType>(callee_type);
    assert(fn_type != nullptr);
    auto fn_decl = fn_type->decl.lock();
    if (fn_decl == nullptr) {
        throw string("Error, function declaration is null");
    }
    if (fn_decl->parameters.size() != arg_types.size()) {
        throw string("CE, function call argument number mismatch");
    }
    for (size_t i = 0; i < arg_types.size(); i++) {
        // 这里相当于 let 语句的类型检查
        auto [pattern, target_type] = fn_decl->parameters[i];
        auto [expr_type, expr_place] = arg_types[i];
        check_let_stmt(pattern, target_type, expr_type, expr_place, node.arguments[i]);
    }
    // 特殊情况： exit 一定要在 main 里面
    if (fn_decl->is_exit) {
        if (now_func_decl == nullptr) {
            throw string("CE, exit function can only be called inside main");
        } else if (!now_func_decl->is_main) {
            throw string("CE, exit function can only be called inside main");
        }
        now_func_decl->function_scope.lock()->has_exit = true;
    }
    node_type_and_place_kind_map[node.NodeId] =
        {fn_decl->return_type, PlaceKind::NotPlace};
}
void ExprTypeAndLetStmtVisitor::visit(FieldExpr &node) {
    if (require_function) {
        // 这个时候是返回一个结构体内的一个函数
        require_function = false;
        node.base->accept(*this);
        auto [base_type, base_place] = node_type_and_place_kind_map[node.base->NodeId];
        auto func = get_method_func(base_type, base_place, node.field_name);
        if (func == nullptr) {
            throw string("CE, struct has no method named ") + node.field_name;
        }
        node_type_and_place_kind_map[node.NodeId] =
            {func, PlaceKind::NotPlace};
    } else {
        // 这个时候是返回一个结构体内的一个字段
        node.base->accept(*this);
        auto [base_type, base_place] = node_type_and_place_kind_map[node.base->NodeId];
        // base_type 必须是 struct 类型
        if (base_type->kind != RealTypeKind::STRUCT) {
            throw string("CE, field access requires a struct type base");
        }
        auto struct_type = std::dynamic_pointer_cast<StructRealType>(base_type);
        assert(struct_type != nullptr);
        auto struct_decl = struct_type->decl.lock();
        assert(struct_decl != nullptr);
        RealType_ptr field_type = nullptr;
        PlaceKind place_kind;
        for (auto &[fname, ftype] : struct_decl->fields) {
            if (fname == node.field_name) {
                field_type = ftype;
                // 自动解引用
                if (base_type->is_ref == ReferenceType::NO_REF) {
                    place_kind = base_place;
                } else if (base_type->is_ref == ReferenceType::REF) {
                    place_kind = PlaceKind::ReadOnlyPlace;
                } else {
                    place_kind = PlaceKind::ReadWritePlace;
                }
                break;
            }
        }
        if (field_type == nullptr) {
            throw string("CE, struct  has no field named ") + node.field_name;
        }
        node_type_and_place_kind_map[node.NodeId] =
            {field_type, place_kind};
    }
}
void ExprTypeAndLetStmtVisitor::visit(StructExpr &node) {
    if (require_function) {
        throw string("CE, struct expression is not function");
    }
    AST_Walker::visit(node);
    auto type = type_map[node.struct_name->NodeId];
    if (type == nullptr) {
        throw string("Error, cannot find struct type, NodeId = ") + std::to_string(node.struct_name->NodeId);
    }
    auto struct_type = std::dynamic_pointer_cast<StructRealType>(type);
    if (struct_type == nullptr) {
        throw string("CE, struct expression requires a struct type");
    }
    auto struct_decl = struct_type->decl.lock();
    assert(struct_decl != nullptr);
    if (struct_decl->fields.size() != node.fields.size()) {
        throw string("CE, struct expression field number mismatch");
    }
    for (auto &[fname, fexpr] : node.fields) {
        if (struct_decl->fields.find(fname) == struct_decl->fields.end()) {
            throw string("CE, struct ") + struct_type->name + " has no field named " + fname;
        }
        type_merge(struct_decl->fields[fname], node_type_and_place_kind_map[fexpr->NodeId].first, true);
    }
    node_type_and_place_kind_map[node.NodeId] =
        {std::make_shared<StructRealType>(struct_type->name, ReferenceType::NO_REF, struct_decl), PlaceKind::NotPlace};
}
void ExprTypeAndLetStmtVisitor::visit(IndexExpr &node) {
    if (require_function) {
        throw string("CE, index expression is not function");
    }
    node.base->accept(*this);
    node.index->accept(*this);
    auto [base_type, base_place] = node_type_and_place_kind_map[node.base->NodeId];
    auto [index_type, index_place] = node_type_and_place_kind_map[node.index->NodeId];
    if (index_type->is_ref != ReferenceType::NO_REF ||
        (index_type->kind != RealTypeKind::USIZE && index_type->kind != RealTypeKind::ANYINT)) {
        throw string("CE, index expression requires usize type index");
    }
    if (base_type->kind != RealTypeKind::ARRAY) {
        throw string("CE, index expression requires array type base");
    }
    auto array_type = std::dynamic_pointer_cast<ArrayRealType>(base_type);
    PlaceKind place_kind;
    if (base_type->is_ref == ReferenceType::NO_REF) {
        // 不可变 -> 不可变
        // 可变 -> 可变
        // 右值 -> 自动引用 -> 可变 (?)
        if (base_place == PlaceKind::ReadOnlyPlace) {
            place_kind = PlaceKind::ReadOnlyPlace;
        } else {
            place_kind = PlaceKind::ReadWritePlace;
        }
    } else if (base_type->is_ref == ReferenceType::REF) {
        place_kind = PlaceKind::ReadOnlyPlace;
    } else {
        place_kind = PlaceKind::ReadWritePlace;
    }
    node_type_and_place_kind_map[node.NodeId] =
        {array_type->element_type, place_kind};
}
void ExprTypeAndLetStmtVisitor::visit(BlockExpr &node) {
    if (require_function) {
        throw string("CE, block expression is not function");
    }
    auto now_scope = node_scope_map[node.NodeId];
    for (auto &stmt : node.statements) {
        if (now_scope->is_main_scope && now_scope->has_exit) {
            throw string("CE, unreachable statement after exit in main function");
        }
        stmt->accept(*this);
    }
    if (node.tail_statement != nullptr) {
        if (now_scope->is_main_scope && now_scope->has_exit) {
            throw string("CE, unreachable statement after exit in main function");
        }
        node.tail_statement->accept(*this);
    }
    if (now_scope->is_main_scope && !now_scope->has_exit) {
        throw string("CE, main function must have an exit call");
    }
    RealType_ptr result_type;
    if (!has_state(node_outcome_state_map[node.NodeId], OutcomeType::NEXT)) {
        result_type = std::make_shared<NeverRealType>(ReferenceType::NO_REF);
    } else if (node.tail_statement != nullptr) {
        auto [tail_type, tail_place] = node_type_and_place_kind_map[node.tail_statement->NodeId];
        result_type = tail_type;
    } else {
        result_type = std::make_shared<UnitRealType>(ReferenceType::NO_REF);
    }
    if (node.must_return_unit && result_type->kind != RealTypeKind::UNIT && result_type->kind != RealTypeKind::NEVER) {
        throw string("CE, block expression must return unit type");
    }
    node_type_and_place_kind_map[node.NodeId] =
        {result_type, PlaceKind::NotPlace};
}
void ExprTypeAndLetStmtVisitor::visit(IfExpr &node) {
    if (require_function) {
        throw string("CE, if expression is not function");
    }
    AST_Walker::visit(node);
    if (!has_state(node_outcome_state_map[node.NodeId], OutcomeType::NEXT)) {
        node_type_and_place_kind_map[node.NodeId] =
            {std::make_shared<NeverRealType>(ReferenceType::NO_REF), PlaceKind::NotPlace};
        return;
    }
    auto [cond_type, cond_place] = node_type_and_place_kind_map[node.condition->NodeId];
    if (cond_type->is_ref != ReferenceType::NO_REF || cond_type->kind != RealTypeKind::BOOL) {
        throw string("CE, if expression requires bool type condition");
    }
    RealType_ptr then_type = node_type_and_place_kind_map[node.then_branch->NodeId].first;
    RealType_ptr else_type = std::make_shared<UnitRealType>(ReferenceType::NO_REF);
    if (node.else_branch != nullptr) {
        else_type = node_type_and_place_kind_map[node.else_branch->NodeId].first;
    }
    node_type_and_place_kind_map[node.NodeId] =
        {type_merge(then_type, else_type), PlaceKind::NotPlace};
}
void ExprTypeAndLetStmtVisitor::visit(WhileExpr &node) {
    if (require_function) {
        throw string("CE, while expression is not function");
    }
    // 放一个 UnitType 在栈里，表示 while 的结果类型
    // 这样 break 的时候，一定得是 Unit Type，否则就不能合并
    // 最后结果一定是 Unit Type
    loop_type_stack.push_back(std::make_shared<UnitRealType>(ReferenceType::NO_REF));
    AST_Walker::visit(node);
    auto [cond_type, cond_place] = node_type_and_place_kind_map[node.condition->NodeId];
    if (cond_type->is_ref != ReferenceType::NO_REF || cond_type->kind != RealTypeKind::BOOL) {
        throw string("CE, while expression requires bool type condition");
    }
    node_type_and_place_kind_map[node.NodeId] =
        {loop_type_stack.back(), PlaceKind::NotPlace};
    loop_type_stack.pop_back();
}
void ExprTypeAndLetStmtVisitor::visit(LoopExpr &node) {
    if (require_function) {
        throw string("CE, loop expression is not function");
    }
    // 放一个 NeverType 在栈里，表示 loop 的结果类型
    // break 有任何 type 都可以和他合并
    loop_type_stack.push_back(std::make_shared<NeverRealType>(ReferenceType::NO_REF));
    AST_Walker::visit(node);
    node_type_and_place_kind_map[node.NodeId] =
        {loop_type_stack.back(), PlaceKind::NotPlace};
    loop_type_stack.pop_back();
}
void ExprTypeAndLetStmtVisitor::visit(ReturnExpr &node) {
    if (require_function) {
        throw string("CE, return expression is not function");
    }
    if (now_func_decl == nullptr) {
        throw string("CE, return expression is not in a function");
    }
    AST_Walker::visit(node);
    RealType_ptr return_type;
    if (node.return_value != nullptr) {
        auto [value_type, value_place] = node_type_and_place_kind_map[node.return_value->NodeId];
        return_type = value_type;
    } else {
        return_type = std::make_shared<UnitRealType>(ReferenceType::NO_REF);
    }
    type_merge(now_func_decl->return_type, return_type);
    node_type_and_place_kind_map[node.NodeId] =
        {std::make_shared<NeverRealType>(ReferenceType::NO_REF), PlaceKind::NotPlace};
}
void ExprTypeAndLetStmtVisitor::visit(BreakExpr &node) {
    if (require_function) {
        throw string("CE, break expression is not function");
    }
    if (loop_type_stack.empty()) {
        throw string("CE, break expression is not in a loop");
    }
    AST_Walker::visit(node);
    RealType_ptr break_type;
    if (node.break_value != nullptr) {
        auto [value_type, value_place] = node_type_and_place_kind_map[node.break_value->NodeId];
        break_type = value_type;
    } else {
        break_type = std::make_shared<UnitRealType>(ReferenceType::NO_REF);
    }
    loop_type_stack.back() = type_merge(loop_type_stack.back(), break_type);
    node_type_and_place_kind_map[node.NodeId] =
        {std::make_shared<NeverRealType>(ReferenceType::NO_REF), PlaceKind::NotPlace};
}
void ExprTypeAndLetStmtVisitor::visit(ContinueExpr &node) {
    if (require_function) {
        throw string("CE, continue expression is not function");
    }
    if (loop_type_stack.empty()) {
        throw string("CE, continue expression is not in a loop");
    }
    AST_Walker::visit(node);
    node_type_and_place_kind_map[node.NodeId] =
        {std::make_shared<NeverRealType>(ReferenceType::NO_REF), PlaceKind::NotPlace};
}
void ExprTypeAndLetStmtVisitor::visit(CastExpr &node) {
    if (require_function) {
        throw string("CE, cast expression is not function");
    }
    node.expr->accept(*this);
    // target_type 已经在前面被求出来过了
    auto target_type = type_map[node.target_type->NodeId];
    assert(target_type != nullptr);
    check_cast(node_type_and_place_kind_map[node.expr->NodeId].first, target_type);
    node_type_and_place_kind_map[node.NodeId] =
        {target_type, PlaceKind::NotPlace};
}
void ExprTypeAndLetStmtVisitor::visit(PathExpr &node) {
    // 有两种可能
    // 结构体的关联函数
    // 或者枚举的字段和结构体的 const 字段
    auto base_type = type_map[node.base->NodeId];
    if (require_function) {
        require_function = false;
        node_type_and_place_kind_map[node.NodeId] =
            {get_associated_func(base_type, node.name), PlaceKind::NotPlace};
    } else {
        if (base_type->kind == RealTypeKind::STRUCT) {
            auto struct_type = std::dynamic_pointer_cast<StructRealType>(base_type);
            auto struct_decl = struct_type->decl.lock();
            assert(struct_decl != nullptr);
            if (struct_decl->associated_const.find(node.name) == struct_decl->associated_const.end()) {
                throw string("CE, struct has no associated constant named ") + node.name;
            } else {
                auto const_type = struct_decl->associated_const[node.name];
                node_type_and_place_kind_map[node.NodeId] =
                    {const_type->const_type, PlaceKind::NotPlace};
            }
        } else if (base_type->kind == RealTypeKind::ENUM) {
            auto enum_type = std::dynamic_pointer_cast<EnumRealType>(base_type);
            auto enum_decl = enum_type->decl.lock();
            assert(enum_decl != nullptr);
            if (enum_decl->variants.find(node.name) == enum_decl->variants.end()) {
                throw string("CE, enum has no variant named ") + node.name;
            } else {
                node_type_and_place_kind_map[node.NodeId] =
                    {enum_type, PlaceKind::NotPlace};
            }
        } else {
            throw string("CE, path expression requires a struct or enum type base");
        }
    }
}
void ExprTypeAndLetStmtVisitor::visit(SelfExpr &node) {
    // 一定得是函数内的 self.
    if (require_function) {
        throw string("CE, self expression is not function");
    }
    if (now_func_decl == nullptr) {
        throw string("CE, self expression is not in a function");
    }
    if (now_func_decl->receiver_type == fn_reciever_type::NO_RECEIVER) {
        throw string("CE, self expression is in a function without self receiver");
    } else if (now_func_decl->receiver_type == fn_reciever_type::SELF) {
        // Struct 的 name：无所谓了，就当作 Self 吧
        // PlaceKind ：暂时应该是 ReadWritePlace，不是特别确定
        node_type_and_place_kind_map[node.NodeId] =
            {std::make_shared<StructRealType>("Self", ReferenceType::NO_REF, now_func_decl->self_struct.lock()), PlaceKind::ReadWritePlace};
    } else if (now_func_decl->receiver_type == fn_reciever_type::SELF_REF) {
        node_type_and_place_kind_map[node.NodeId] =
            {std::make_shared<StructRealType>("Self", ReferenceType::REF, now_func_decl->self_struct.lock()), PlaceKind::NotPlace};
    } else if (now_func_decl->receiver_type == fn_reciever_type::SELF_REF_MUT) {
        node_type_and_place_kind_map[node.NodeId] =
            {std::make_shared<StructRealType>("Self", ReferenceType::REF_MUT, now_func_decl->self_struct.lock()), PlaceKind::NotPlace};
    } else {
        throw string("Error, unknown self receiver type");
    }
}
void ExprTypeAndLetStmtVisitor::visit(UnitExpr &node) {
    if (require_function) {
        throw string("CE, unit expression is not function");
    }
    node_type_and_place_kind_map[node.NodeId] =
        {std::make_shared<UnitRealType>(ReferenceType::NO_REF), PlaceKind::NotPlace};
}
void ExprTypeAndLetStmtVisitor::visit(ArrayExpr &node) {
    if (require_function) {
        throw string("CE, array expression is not function");
    }
    AST_Walker::visit(node);
    // 因为 never 和任何的都可以合并，所以一开始就放一个 never
    RealType_ptr element_type = std::make_shared<NeverRealType>(ReferenceType::NO_REF);
    for (auto &elem : node.elements) {
        auto [etype, eplace] = node_type_and_place_kind_map[elem->NodeId];
        element_type = type_merge(element_type, etype);
    }
    node_type_and_place_kind_map[node.NodeId] =
        {std::make_shared<ArrayRealType>(element_type, nullptr, ReferenceType::NO_REF, node.elements.size()), PlaceKind::NotPlace};
}
void ExprTypeAndLetStmtVisitor::visit(RepeatArrayExpr &node) {
    if (require_function) {
        throw string("CE, repeat array expression is not function");
    }
    node.element->accept(*this);
    auto [elem_type, elem_place] = node_type_and_place_kind_map[node.element->NodeId];
    size_t size = const_expr_to_size_map[node.size->NodeId];
    node_type_and_place_kind_map[node.NodeId] =
        {std::make_shared<ArrayRealType>(elem_type, nullptr, ReferenceType::NO_REF, size), PlaceKind::NotPlace};
}

void ExprTypeAndLetStmtVisitor::visit(FnItem &node) {
    if (require_function) {
        throw string("CE, function item is not function");
    }
    auto prev_func_decl = now_func_decl;
    auto now_scope = node_scope_map[node.NodeId];
    if (now_scope->value_namespace.find(node.function_name) == now_scope->value_namespace.end()) {
        // 不可能，所以是 Error
        throw string("Error, function declaration not found in scope");
    }
    auto decl = now_scope->value_namespace[node.function_name];
    if (decl->kind != ValueDeclKind::Function) {
        throw string("Error, function declaration is not a function");
    }
    now_func_decl = std::dynamic_pointer_cast<FnDecl>(decl);
    // 函数的参数可以认为在下一个定义域里面加了这些局部变量
    for (auto &[pattern, type] : now_func_decl->parameters) {
        intro_let_stmt(now_func_decl->function_scope.lock(), pattern, type);
    }
    node.body->accept(*this);
    /*
    检查返回类型：首先，return 的类型在 return 会判断
    所以就是 body 的类型和 return type 合并
    如果 body 是 never 在前面应该就会判断
    */
    type_merge(node_type_and_place_kind_map[node.body->NodeId].first, now_func_decl->return_type);
    now_func_decl = prev_func_decl;
}
void ExprTypeAndLetStmtVisitor::visit([[maybe_unused]]StructItem &node) {
    // struct item 不用管
    if (require_function) {
        throw string("CE, struct item is not function");
    }
    return;
}
void ExprTypeAndLetStmtVisitor::visit([[maybe_unused]]EnumItem &node) {
    // enum item 不用管
    if (require_function) {
        throw string("CE, enum item is not function");
    }
    return;    
}
void ExprTypeAndLetStmtVisitor::visit(ImplItem &node) {
    if (require_function) {
        throw string("CE, impl item is not function");
    }
    // 这个时候要进去递归函数
    AST_Walker::visit(node);
}
void ExprTypeAndLetStmtVisitor::visit([[maybe_unused]]ConstItem &node) {
    if (require_function) {
        throw string("CE, const item is not function");
    }
    // 之前检查过了，不用管
    return;
}
void ExprTypeAndLetStmtVisitor::visit(LetStmt &node) {
    if (require_function) {
        throw string("CE, let statement is not function");
    }
    if (node.initializer == nullptr) {
        throw string("CE, let statement must have an initializer");
    }
    node.initializer->accept(*this);
    auto [init_type, init_place] = node_type_and_place_kind_map[node.initializer->NodeId];
    check_let_stmt(node.pattern, type_map[node.type->NodeId], init_type, init_place, node.initializer);
    intro_let_stmt(node_scope_map[node.NodeId], node.pattern, type_map[node.type->NodeId]);
}
void ExprTypeAndLetStmtVisitor::visit(ExprStmt &node) {
    AST_Walker::visit(node);
    node_type_and_place_kind_map[node.NodeId] =
        node_type_and_place_kind_map[node.expr->NodeId];
}
void ExprTypeAndLetStmtVisitor::visit(ItemStmt &node) {
    AST_Walker::visit(node);
}
void ExprTypeAndLetStmtVisitor::visit([[maybe_unused]]PathType &node) { return; }
void ExprTypeAndLetStmtVisitor::visit([[maybe_unused]]ArrayType &node) { return; }
void ExprTypeAndLetStmtVisitor::visit([[maybe_unused]]UnitType &node) { return; }
void ExprTypeAndLetStmtVisitor::visit([[maybe_unused]]SelfType &node) { return; }
void ExprTypeAndLetStmtVisitor::visit([[maybe_unused]]IdentifierPattern &node) { return; }

ValueDecl_ptr ExprTypeAndLetStmtVisitor::find_value_decl(Scope_ptr now_scope, const string &name) {
    while (now_scope != nullptr) {
        if (scope_local_variable_map[now_scope].find(name) != scope_local_variable_map[now_scope].end()) {
            return scope_local_variable_map[now_scope][name];
        }
        if (now_scope->value_namespace.find(name) != now_scope->value_namespace.end()) {
            return now_scope->value_namespace[name];
        }
        now_scope = now_scope->parent.lock();
    }
    return nullptr;
}

TypeDecl_ptr ExprTypeAndLetStmtVisitor::find_type_decl(Scope_ptr now_scope, const string &name) {
    while (now_scope != nullptr) {
        if (now_scope->type_namespace.find(name) != now_scope->type_namespace.end()) {
            return now_scope->type_namespace[name];
        }
        now_scope = now_scope->parent.lock();
    }
    return nullptr;
}

// expr_place 好像没啥用
// 我纳闷了，为啥我当时觉得有用呢？
// 先打个 maybe_unused 再说
void ExprTypeAndLetStmtVisitor::check_let_stmt(Pattern_ptr let_pattern, RealType_ptr target_type, RealType_ptr expr_type, [[maybe_unused]]PlaceKind expr_place, Expr_ptr initializer) {
    // 如果 initializer 是 LiteralExpr，检查是否越界
    if (auto literal_expr = std::dynamic_pointer_cast<LiteralExpr>(initializer)) {
        auto literal_type = type_of_literal(literal_expr->literal_type, literal_expr->value);
        // 只需要考虑是 anyint 的情况，其他情况早已被检查掉了
        if (literal_type->kind == RealTypeKind::ANYINT) {
            if (target_type->kind == RealTypeKind::USIZE ||
                target_type->kind == RealTypeKind::U32) {
                safe_stoll(literal_expr->value, UINT32_MAX);
            } else if (target_type->kind == RealTypeKind::ISIZE ||
                       target_type->kind == RealTypeKind::I32) {
                safe_stoll(literal_expr->value, INT32_MAX);
            } else {
                throw string("CE, cannot assign anyint literal to non-integer type");
            }
        }
    }
    // Pattern 目前只有 IdentifierPattern
    auto ident_pattern = std::dynamic_pointer_cast<IdentifierPattern>(let_pattern);
    // 考虑：let x, let mut x, let &mut x, let &x
    if (ident_pattern->is_mut == Mutibility::IMMUTABLE && ident_pattern->is_ref == ReferenceType::NO_REF) {
        // let x
        type_merge(target_type, expr_type, true);
    } else if (ident_pattern->is_mut == Mutibility::MUTABLE && ident_pattern->is_ref == ReferenceType::NO_REF) {
        // let mut x
        type_merge(target_type, expr_type, true);
    } else if (ident_pattern->is_mut == Mutibility::IMMUTABLE && ident_pattern->is_ref == ReferenceType::REF) {
        // let &x
        if (expr_type->is_ref == ReferenceType::NO_REF) {
            throw string("CE, let & requires a reference type initializer");
        }
        auto real_type = copy(expr_type);
        real_type->is_ref = ReferenceType::NO_REF;
        type_merge(target_type, real_type, true);
    } else if (ident_pattern->is_mut == Mutibility::MUTABLE && ident_pattern->is_ref == ReferenceType::REF) {
        // let &mut x
        if (expr_type->is_ref != ReferenceType::REF_MUT) {
            throw string("CE, let &mut requires a mutable reference type initializer");
        }
        if (expr_type->is_ref == ReferenceType::NO_REF) {
            throw string("CE, let & requires a reference type initializer");
        }
        auto real_type = copy(expr_type);
        real_type->is_ref = ReferenceType::NO_REF;
        type_merge(target_type, real_type, true);
    } else {
        throw string("Error, unknown pattern mutibility and reference type");
    }
}

void ExprTypeAndLetStmtVisitor::intro_let_stmt(Scope_ptr current_scope, Pattern_ptr let_pattern, RealType_ptr let_type) {
    // Pattern 目前只有 IdentifierPattern
    auto ident_pattern = std::dynamic_pointer_cast<IdentifierPattern>(let_pattern);
    // 只要考虑是否 mut
    if (ident_pattern->is_mut == Mutibility::MUTABLE) {
        scope_local_variable_map[current_scope][ident_pattern->name] =
            std::make_shared<LetDecl>(let_type, Mutibility::MUTABLE);
    } else {
        scope_local_variable_map[current_scope][ident_pattern->name] =
            std::make_shared<LetDecl>(let_type, Mutibility::IMMUTABLE);
    }
}

void ExprTypeAndLetStmtVisitor::check_cast(RealType_ptr expr_type, RealType_ptr target_type) {
    switch (expr_type->is_ref) {
        case ReferenceType::NO_REF: {
            if (target_type->is_ref != ReferenceType::NO_REF) {
                throw string("CE, cannot cast non-reference type to reference type");
            }
            break;
        }
        case ReferenceType::REF: {
            if (target_type->is_ref == ReferenceType::REF_MUT) {
                throw string("CE, cannot cast &T to &mut T");
            }
            break;
        }
        case ReferenceType::REF_MUT: {
            // &mut T 可以转 &T
            if (target_type->is_ref == ReferenceType::NO_REF) {
                throw string("CE, cannot cast &mut T to T");
            }
            break;
        }
    }
    if (type_is_number(target_type)) {
        if (type_is_number(expr_type) ||
            expr_type->kind == RealTypeKind::CHAR ||
            expr_type->kind == RealTypeKind::BOOL ||
            expr_type->kind == RealTypeKind::ENUM) {
            return;
        }
        throw string("CE, cannot cast type ") + real_type_kind_to_string(expr_type->kind) + " to number type " + real_type_kind_to_string(target_type->kind);
    }
    if (target_type->kind == RealTypeKind::CHAR) {
        if (type_is_number(expr_type) ||
            expr_type->kind == RealTypeKind::CHAR) {
            return;
        }
        throw string("CE, cannot cast type ") + real_type_kind_to_string(expr_type->kind) + " to char type";
    }
    if (target_type->kind == RealTypeKind::ARRAY) {
        if (expr_type->kind == RealTypeKind::ARRAY) {
            auto expr_array_type = std::dynamic_pointer_cast<ArrayRealType>(expr_type);
            auto target_array_type = std::dynamic_pointer_cast<ArrayRealType>(target_type);
            if (expr_array_type->size != target_array_type->size) {
                throw string("CE, cannot cast array type with different size");
            }
            check_cast(expr_array_type->element_type, target_array_type->element_type);
            return;
        } else {
            throw string("CE, cannot cast type ") + real_type_kind_to_string(expr_type->kind) + " to array type";
        }
    }
    // 其他情况必须全部相同了
    // 直接用 type_merge 检查
    type_merge(expr_type, target_type);
}

// 需要考虑 RecieverType
RealType_ptr ExprTypeAndLetStmtVisitor::get_method_func(RealType_ptr base_type, PlaceKind place_kind, const string &method_name) {
    FnDecl_ptr method_decl = nullptr;
    if (base_type->kind == RealTypeKind::STRUCT) {
        auto struct_type = std::dynamic_pointer_cast<StructRealType>(base_type);
        auto struct_decl = struct_type->decl.lock();
        if (struct_decl->methods.find(method_name) == struct_decl->methods.end()) {
            throw string("CE, struct has no method named ") + method_name;
        }
        method_decl = struct_decl->methods[method_name];
    } else {
        for (auto &[kind, name, decl] : builtin_method_funcs) {
            if (kind == base_type->kind && name == method_name) {
                method_decl = decl;
                break;
            }
        }
    }
    if (method_decl == nullptr) {
        throw string("CE, type ") + real_type_kind_to_string(base_type->kind) + " has no method named " + method_name;
    }
    // 检查 self receiver
    if (method_decl->receiver_type == fn_reciever_type::SELF) {
        // 这个时候自动解引用
        return std::make_shared<FunctionRealType>(method_decl, ReferenceType::NO_REF);
    } else if (method_decl->receiver_type == fn_reciever_type::SELF_REF) {
        // 这个时候自动加引用
        return std::make_shared<FunctionRealType>(method_decl, ReferenceType::NO_REF);
    } else if (method_decl->receiver_type == fn_reciever_type::SELF_REF_MUT) {
        if (base_type->is_ref == ReferenceType::NO_REF) {
            if (place_kind != PlaceKind::ReadOnlyPlace) {
                // 可变的左值 和右值应该都可以
                return std::make_shared<FunctionRealType>(method_decl, ReferenceType::NO_REF);
            } else {
                throw string("CE, method ") + method_name + " requires a mutable self receiver";
            }
        } else if (base_type->is_ref == ReferenceType::REF) {
            throw string("CE, method ") + method_name + " requires a mutable self receiver";
        } else {
            // REF_MUT
            return std::make_shared<FunctionRealType>(method_decl, ReferenceType::NO_REF);
        }
    } else {
        // no receiver 的情况应该前面已经检查掉了
        throw string("Error, unknown self receiver type");
    }
}

RealType_ptr ExprTypeAndLetStmtVisitor::get_associated_func(RealType_ptr base_type, const string &func_name) {
    FnDecl_ptr func_decl = nullptr;
    if (base_type->kind == RealTypeKind::STRUCT) {
        auto struct_type = std::dynamic_pointer_cast<StructRealType>(base_type);
        auto struct_decl = struct_type->decl.lock();
        if (struct_decl->associated_func.find(func_name) == struct_decl->associated_func.end()) {
            throw string("CE, struct has no associated function named ") + func_name;
        }
        func_decl = struct_decl->associated_func[func_name];
    } else {
        for (auto &[kind, name, decl] : builtin_associated_funcs) {
            if (kind == base_type->kind && name == func_name) {
                func_decl = decl;
                break;
            }
        }
    }
    if (func_decl == nullptr) {
        throw string("CE, type ") + real_type_kind_to_string(base_type->kind) + " has no associated function named " + func_name;
    }
    return std::make_shared<FunctionRealType>(func_decl, ReferenceType::NO_REF);
}
