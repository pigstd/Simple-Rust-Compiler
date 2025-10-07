#include "semantic_step4.h"
#include "ast.h"
#include "semantic_step1.h"
#include "semantic_step2.h"
#include "visitor.h"
#include "tools.h"
#include <cassert>
#include <cstddef>
#include <memory>

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
    auto target_real_type = type_map[std::make_shared<ArrayType>(node)];
    auto array_real_type = std::dynamic_pointer_cast<ArrayRealType>(target_real_type);
    if (!array_real_type) {
        // 这个时候应该是前面代码写错了，不是 CE，报 Error 的错误
        throw "Error, should be array type";
    }
    size_t array_size = 0;
    if (const_expr_to_size_map.find(node.size_expr) != const_expr_to_size_map.end()) {
        array_size = const_expr_to_size_map[node.size_expr];
    } else {
        // 这个时候应该是前面代码写错了，不是 CE，报 Error 的错误
        throw "Error, cannot find array size";
    }
    array_real_type->size = array_size;
    AST_Walker::visit(node);
}
void ArrayTypeVisitor::visit(UnitType &node) { AST_Walker::visit(node); }
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

RealType_ptr type_merge(RealType_ptr left, RealType_ptr right) {
    assert(left != nullptr && right != nullptr);
    if (left->kind == RealTypeKind::NEVER) { return right; }
    if (right->kind == RealTypeKind::NEVER) { return left; }
    if (left->is_ref != right->is_ref) {
        throw "CE, cannot merge two different reference types";
    }
    if (left->kind == RealTypeKind::ANYINT) {
        if (type_is_number(right)) { return right; }
        else throw "CE, cannot merge AnyInt with non-number type";
    }
    if (right->kind == RealTypeKind::ANYINT) {
        if (type_is_number(left)) { return left; }
        else throw "CE, cannot merge AnyInt with non-number type";
    }
    // 其他情况的 type 必须完全相同
    if (left->kind != right->kind) {
        throw "CE, cannot merge two different types";
    }
    if (left->kind == RealTypeKind::ARRAY) {
        auto left_array = std::dynamic_pointer_cast<ArrayRealType>(left);
        auto right_array = std::dynamic_pointer_cast<ArrayRealType>(right);
        assert(left_array != nullptr && right_array != nullptr);
        if (left_array->size != right_array->size) {
            throw "CE, cannot merge two different array types with different size";
        }
        auto merged_element_type = type_merge(left_array->element_type, right_array->element_type);
        return std::make_shared<ArrayRealType>(merged_element_type, nullptr, left->is_ref, left_array->size);
    } else {
        return left;
    }
}

RealType_ptr type_of_literal(LiteralType type, string value) {
    if (type == LiteralType::STRING) {
        // 类型是 &str
        return std::make_shared<StrRealType>(ReferenceType::REF);
    } else if (type == LiteralType::CHAR) {
        return std::make_shared<RealType>(RealTypeKind::CHAR, ReferenceType::NO_REF);
    } else if (type == LiteralType::BOOL) {
        return std::make_shared<RealType>(RealTypeKind::BOOL, ReferenceType::NO_REF);
    } else if (type == LiteralType::NUMBER) {
        // 看后缀，以及要 check 是否是合法的数字
        if (value.size() >= 4 && value.substr(value.size() - 4) == "_i32") {
            // i32
            safe_stoll(value.substr(0, value.size() - 4), INT32_MAX);
            return std::make_shared<I32RealType>(ReferenceType::NO_REF);
        } else if (value.size() >= 4 && value.substr(value.size() - 4) == "_u32") {
            // u32
            safe_stoll(value.substr(0, value.size() - 4), UINT32_MAX);
            return std::make_shared<U32RealType>(ReferenceType::NO_REF);
        } else if (value.size() >= 6 && value.substr(value.size() - 6) == "_isize") {
            // isize
            safe_stoll(value.substr(0, value.size() - 6), INT32_MAX);
            return std::make_shared<IsizeRealType>(ReferenceType::NO_REF);
        } else if (value.size() >= 6 && value.substr(value.size() - 6) == "_usize") {
            // usize
            safe_stoll(value.substr(0, value.size() - 6), UINT32_MAX);
            return std::make_shared<UsizeRealType>(ReferenceType::NO_REF);
        } else {
            // anyint
            safe_stoll(value, LLONG_MAX);
            return std::make_shared<AnyIntRealType>(ReferenceType::NO_REF);
        }
    } else {
        throw "Error, unknown literal type";
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
        return std::make_shared<FunctionRealType>(fn_type->decl, type->is_ref);
    } else if (type->kind == RealTypeKind::STRUCT) {
        auto struct_type = std::dynamic_pointer_cast<StructRealType>(type);
        assert(struct_type != nullptr);
        return std::make_shared<StructRealType>(struct_type->decl, type->is_ref);
    } else if (type->kind == RealTypeKind::ENUM) {
        auto enum_type = std::dynamic_pointer_cast<EnumRealType>(type);
        assert(enum_type != nullptr);
        return std::make_shared<EnumRealType>(enum_type->decl, type->is_ref);
    } else if (type->kind == RealTypeKind::STR) {
        return std::make_shared<StrRealType>(type->is_ref);
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
        throw "Error, copy unknown real type";
    }
}

void ExprTypeAndLetStmtVisitor::visit(LiteralExpr &node) {
    if (require_function) {
        throw "CE, literal is not function";
    }
    auto literal_type = type_of_literal(node.literal_type, node.value);
    node_type_and_place_kind_map[std::make_shared<LiteralExpr>(node)] = {literal_type, PlaceKind::NotPlace};
}
void ExprTypeAndLetStmtVisitor::visit(IdentifierExpr &node) {
    auto decl = find_value_decl(node_scope_map[std::make_shared<AST_Node>(node)], node.name);
    if (decl == nullptr) {
        throw "CE, cannot find identifier declaration: " + node.name;
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
            throw "CE, identifier is not function: " + node.name;
        }
    } else {
        if (decl->kind == ValueDeclKind::Constant) {
            auto const_decl = std::dynamic_pointer_cast<ConstDecl>(decl);
            assert(const_decl != nullptr);
            result_type = const_decl->const_type;
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
            throw "CE, identifier is not variable or constant : " + node.name;
        }
    }
    assert(result_type != nullptr);
    node_type_and_place_kind_map[std::make_shared<IdentifierExpr>(node)] = {result_type, place_kind};
}
void ExprTypeAndLetStmtVisitor::visit(BinaryExpr &node) {
    if (require_function) {
        throw "CE, binary expression is not function";
    }
    AST_Walker::visit(node);
    auto [left_type, left_place] = node_type_and_place_kind_map[node.left];
    auto [right_type, right_place] = node_type_and_place_kind_map[node.right];
    RealType_ptr result_type = nullptr;
    if (left_type->is_ref != ReferenceType::NO_REF ||
        right_type->is_ref != ReferenceType::NO_REF) {
        throw "CE, binary operator cannot have reference type operands";
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
                throw "CE, binary operator " + binary_operator_to_string(node.op) + " requires number types";
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
                throw "CE, binary operator " + binary_operator_to_string(node.op) + " requires number or bool types";
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
                throw "CE, binary operator " + binary_operator_to_string(node.op) + " requires bool types";
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
                result_type = std::make_shared<RealType>(RealTypeKind::BOOL, ReferenceType::NO_REF);
            } else {
                throw "CE, binary operator " + binary_operator_to_string(node.op) + " requires two same types";
            }
            break;
        }
        case Binary_Operator::SHL:
        case Binary_Operator::SHR: {
            // 只要左右都是数字就行
            if (type_is_number(left_type) && type_is_number(right_type)) {
                result_type = left_type;
            } else {
                throw "CE, binary operator " + binary_operator_to_string(node.op) + " requires number types";
            }
            break;
        }
        case Binary_Operator::ASSIGN: {
            if (left_place != PlaceKind::ReadWritePlace) {
                throw "CE, left side of assignment must be a mutable place";
            }
            // 如果类型不同，type_merge 的时候就直接会报错
            type_merge(left_type, right_type);
            result_type = std::make_shared<UnitRealType>(ReferenceType::NO_REF);
            break;
        }
        case Binary_Operator::ADD_ASSIGN:
        case Binary_Operator::SUB_ASSIGN:
        case Binary_Operator::MUL_ASSIGN:
        case Binary_Operator::DIV_ASSIGN:
        case Binary_Operator::MOD_ASSIGN: {
            if (left_place != PlaceKind::ReadWritePlace) {
                throw "CE, left side of assignment must be a mutable place";
            }
            if (type_is_number(left_type) && type_is_number(right_type)) {
                // 如果类型不同，type_merge 的时候就直接会报错
                type_merge(left_type, right_type);
                result_type = std::make_shared<UnitRealType>(ReferenceType::NO_REF);
            } else {
                throw "CE, binary operator " + binary_operator_to_string(node.op) + " requires number types";
            }
            break;
        }
        case Binary_Operator::AND_ASSIGN:
        case Binary_Operator::OR_ASSIGN:
        case Binary_Operator::XOR_ASSIGN: {
            if (left_place != PlaceKind::ReadWritePlace) {
                throw "CE, left side of assignment must be a mutable place";
            }
            if ((type_is_number(left_type) || left_type->kind == RealTypeKind::BOOL) &&
                (type_is_number(right_type) || right_type->kind == RealTypeKind::BOOL)) {
                // 如果类型不同，type_merge 的时候就直接会报错
                type_merge(left_type, right_type);
                result_type = std::make_shared<UnitRealType>(ReferenceType::NO_REF);
            } else {
                throw "CE, binary operator " + binary_operator_to_string(node.op) + " requires number or bool types";
            }
            break;
        }
        case Binary_Operator::SHL_ASSIGN:
        case Binary_Operator::SHR_ASSIGN: {
            if (left_place != PlaceKind::ReadWritePlace) {
                throw "CE, left side of assignment must be a mutable place";
            }
            if (type_is_number(left_type) && type_is_number(right_type)) {
                result_type = std::make_shared<UnitRealType>(ReferenceType::NO_REF);
            } else {
                throw "CE, binary operator " + binary_operator_to_string(node.op) + " requires number types";
            }
        }
    }
    assert(result_type != nullptr);
    node_type_and_place_kind_map[std::make_shared<BinaryExpr>(node)] =
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
        throw "CE, unary expression is not function";
    }
    AST_Walker::visit(node);
    auto [right_type, right_place] = node_type_and_place_kind_map[node.right];
    RealType_ptr result_type = nullptr;
    PlaceKind place_kind;
    if (node.op == Unary_Operator::NEG) {
        if (right_type->is_ref != ReferenceType::NO_REF) {
            throw "CE, unary operator - cannot have reference type operand";
        }
        if (type_is_number(right_type)) {
            result_type = right_type;
            place_kind = PlaceKind::NotPlace;
        } else {
            throw "CE, unary operator - requires number type";
        }
    } else if (node.op == Unary_Operator::NOT) {
        if (right_type->is_ref != ReferenceType::NO_REF) {
            throw "CE, unary operator ! cannot have reference type operand";
        }
        if (right_type->kind == RealTypeKind::BOOL) {
            result_type = right_type;
            place_kind = PlaceKind::NotPlace;
        } else {
            throw "CE, unary operator ! requires bool type";
        }
    } else if (node.op == Unary_Operator::REF) {
        if (right_place == PlaceKind::NotPlace) {
            throw "CE, unary operator & requires a place operand";
        }
        if (right_type->is_ref != ReferenceType::NO_REF) {
            throw "CE, cannot take reference of a reference type";
        }
        // 由于会修改，所以就深拷贝一下吧
        // 前面的地方直接赋值，没关系的
        result_type = copy(right_type);
        result_type->is_ref = ReferenceType::REF;
        place_kind = PlaceKind::NotPlace;
    } else if (node.op == Unary_Operator::REF_MUT) {
        if (right_place != PlaceKind::ReadWritePlace) {
            throw "CE, unary operator &mut requires a mutable place operand";
        }
        if (right_type->is_ref != ReferenceType::NO_REF) {
            throw "CE, cannot take reference of a reference type";
        }
        result_type = copy(right_type);
        result_type->is_ref = ReferenceType::REF_MUT;
        place_kind = PlaceKind::NotPlace;
    } else if (node.op == Unary_Operator::DEREF) {
        if (right_type->is_ref == ReferenceType::NO_REF) {
            throw "CE, unary operator * requires a reference type operand";
        }
        result_type = copy(right_type);
        result_type->is_ref = ReferenceType::NO_REF;
        if (right_place == PlaceKind::ReadWritePlace) {
            place_kind = PlaceKind::ReadWritePlace;
        } else {
            place_kind = PlaceKind::ReadOnlyPlace;
        }
    } else {
        throw "CE, unexpected unary operator: " + unary_operator_to_string(node.op);
    }
    assert(result_type != nullptr);
    node_type_and_place_kind_map[std::make_shared<UnaryExpr>(node)] =
        {result_type, place_kind};
}
void ExprTypeAndLetStmtVisitor::visit(CallExpr &node) {
    if (require_function) {
        throw "CE, call expression is not function";
    }
    require_function = true;
    node.callee->accept(*this);
    require_function = false;
    vector<pair<RealType_ptr, PlaceKind>> arg_types;
    for (auto &arg : node.arguments) {
        arg->accept(*this);
        arg_types.push_back(node_type_and_place_kind_map[arg]);
    }
    auto [callee_type, callee_place] = node_type_and_place_kind_map[node.callee];
    if (callee_type->kind != RealTypeKind::FUNCTION) {
        throw "CE, call expression requires a function type callee";
    }
    auto fn_type = std::dynamic_pointer_cast<FunctionRealType>(callee_type);
    assert(fn_type != nullptr);
    auto fn_decl = fn_type->decl.lock();
    if (fn_decl == nullptr) {
        throw "Error, function declaration is null";
    }
    if (fn_decl->parameters.size() != arg_types.size()) {
        throw "CE, function call argument number mismatch";
    }
    for (size_t i = 0; i < arg_types.size(); i++) {
        // 这里相当于 let 语句的类型检查
        auto [pattern, target_type] = fn_decl->parameters[i];
        auto [expr_type, expr_place] = arg_types[i];
        check_let_stmt(pattern, target_type, expr_type, expr_place);
    }
    node_type_and_place_kind_map[std::make_shared<CallExpr>(node)] =
        {fn_decl->return_type, PlaceKind::NotPlace};
}
void ExprTypeAndLetStmtVisitor::visit(FieldExpr &node) {
    if (require_function) {
        // 这个时候是返回一个结构体内的一个函数
        require_function = false;
        node.base->accept(*this);
        auto [base_type, base_place] = node_type_and_place_kind_map[node.base];
        node_type_and_place_kind_map[std::make_shared<FieldExpr>(node)] =
            {get_associated_func(base_type, node.field_name), PlaceKind::NotPlace};
    } else {
        // 这个时候是返回一个结构体内的一个字段
        node.base->accept(*this);
        auto [base_type, base_place] = node_type_and_place_kind_map[node.base];
        // base_type 必须是 struct 类型
        if (base_type->kind != RealTypeKind::STRUCT) {
            throw "CE, field access requires a struct type base";
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
            throw "CE, struct  has no field named " + node.field_name;
        }
        node_type_and_place_kind_map[std::make_shared<FieldExpr>(node)] =
            {field_type, place_kind};
    }
}
void ExprTypeAndLetStmtVisitor::visit(StructExpr &node) {
    if (require_function) {
        throw "CE, struct expression is not function";
    }
    AST_Walker::visit(node);
    auto decl = find_type_decl(node_scope_map[std::make_shared<AST_Node>(node)], node.struct_name);
    if (decl == nullptr || decl->kind != TypeDeclKind::Struct) {
        throw "CE, cannot find struct declaration: " + node.struct_name;
    }
    auto struct_decl = std::dynamic_pointer_cast<StructDecl>(decl);
    assert(struct_decl != nullptr);
    if (struct_decl->fields.size() != node.fields.size()) {
        throw "CE, struct expression field number mismatch";
    }
    for (auto &[fname, fexpr] : node.fields) {
        if (struct_decl->fields.find(fname) == struct_decl->fields.end()) {
            throw "CE, struct " + node.struct_name + " has no field named " + fname;
        }
        type_merge(struct_decl->fields[fname], node_type_and_place_kind_map[fexpr].first);
    }
    node_type_and_place_kind_map[std::make_shared<StructExpr>(node)] =
        {std::make_shared<StructRealType>(struct_decl, ReferenceType::NO_REF), PlaceKind::NotPlace};
}
// 后面的处理还要解引用啥的，有点烦
// NOT FINISHED !!!!
// TO DO !!!!