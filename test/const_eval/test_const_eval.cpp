// 测试 semantic_step3 部分的 const eval 是否正确
#include "ast/ast.h"
#include "semantic/semantic_step3.h"
#include <cassert>
#include <cstddef>
#include <iostream>

using std::cout;
using std::endl;

void test_parse_literal_token_to_const_value(ConstItemVisitor &visitor) {
    {
        auto val = visitor.parse_literal_token_to_const_value(LiteralType::NUMBER, "123");
        assert(val->kind == ConstValueKind::ANYINT);
        auto anyint_val = std::dynamic_pointer_cast<AnyInt_ConstValue>(val);
        assert(anyint_val->value == 123);
    }
    {
        auto val = visitor.parse_literal_token_to_const_value(LiteralType::NUMBER, "456_i32");
        assert(val->kind == ConstValueKind::I32);
        auto i32_val = std::dynamic_pointer_cast<I32_ConstValue>(val);
        assert(i32_val->value == 456);
    }
    {
        auto val = visitor.parse_literal_token_to_const_value(LiteralType::NUMBER, "789_u32");
        assert(val->kind == ConstValueKind::U32);
        auto u32_val = std::dynamic_pointer_cast<U32_ConstValue>(val);
        assert(u32_val->value == 789);
    }
    {
        auto val = visitor.parse_literal_token_to_const_value(LiteralType::NUMBER, "100_isize");
        assert(val->kind == ConstValueKind::ISIZE);
        auto isize_val = std::dynamic_pointer_cast<Isize_ConstValue>(val);
        assert(isize_val->value == 100);
    }
    {
        auto val = visitor.parse_literal_token_to_const_value(LiteralType::NUMBER, "200_usize");
        assert(val->kind == ConstValueKind::USIZE);
        auto usize_val = std::dynamic_pointer_cast<Usize_ConstValue>(val);
        assert(usize_val->value == 200);
    }
    {
        auto val = visitor.parse_literal_token_to_const_value(LiteralType::BOOL, "true");
        assert(val->kind == ConstValueKind::BOOL);
        auto bool_val = std::dynamic_pointer_cast<Bool_ConstValue>(val);
        assert(bool_val->value == true);
    }
    {
        auto val = visitor.parse_literal_token_to_const_value(LiteralType::BOOL, "false");
        assert(val->kind == ConstValueKind::BOOL);
        auto bool_val = std::dynamic_pointer_cast<Bool_ConstValue>(val);
        assert(bool_val->value == false);
    }
    // 测试 CE
    {
        bool caught = false;
        try {
            auto val = visitor.parse_literal_token_to_const_value(LiteralType::BOOL, "notabool");
        } catch (const string &e) {
            caught = true;
            cout << "err info : " << e << endl;
        }
        assert(caught);
    }
    {
        bool caught = false;
        try {
            auto val = visitor.parse_literal_token_to_const_value(LiteralType::CHAR, "ab");
        } catch (const string &e) {
            caught = true;
            cout << "err info : " << e << endl;
        }
        assert(caught);
    }
    {
        bool caught = false;
        try {
            auto val = visitor.parse_literal_token_to_const_value(LiteralType::NUMBER, "114514nan");
        } catch (const string &e) {
            caught = true;
            cout << "err info : " << e << endl;
        }
        assert(caught);
    }
}

void test_calc_const_unary_expr(ConstItemVisitor &visitor) {
    {
        auto right = std::make_shared<AnyInt_ConstValue>(123);
        auto val = visitor.calc_const_unary_expr(Unary_Operator::NEG, right);
        assert(val->kind == ConstValueKind::ANYINT);
        auto anyint_val = std::dynamic_pointer_cast<AnyInt_ConstValue>(val);
        assert(anyint_val->value == -123);
    }
    {
        auto right = std::make_shared<I32_ConstValue>(456);
        auto val = visitor.calc_const_unary_expr(Unary_Operator::NEG, right);
        assert(val->kind == ConstValueKind::I32);
        auto i32_val = std::dynamic_pointer_cast<I32_ConstValue>(val);
        assert(i32_val->value == -456);
    }
    {
        bool caught = false;
        try {
            auto right = std::make_shared<U32_ConstValue>(789);
            auto val = visitor.calc_const_unary_expr(Unary_Operator::NEG, right);
        } catch (const string &e) {
            caught = true;
            cout << "err info : " << e << endl;
        }
        assert(caught);
    }
    {
        auto right = std::make_shared<Bool_ConstValue>(true);
        auto val = visitor.calc_const_unary_expr(Unary_Operator::NOT, right);
        assert(val->kind == ConstValueKind::BOOL);
        auto bool_val = std::dynamic_pointer_cast<Bool_ConstValue>(val);
        assert(bool_val->value == false);
    }
    {
        bool caught = false;
        try {
            auto right = std::make_shared<I32_ConstValue>(123);
            auto val = visitor.calc_const_unary_expr(Unary_Operator::NOT, right);
        } catch (const string &e) {
            caught = true;
            cout << "err info : " << e << endl;
        }
        assert(caught);
    }
}

void test_calc_const_binary_expr(ConstItemVisitor &visitor) {
    {
        auto left = std::make_shared<AnyInt_ConstValue>(100);
        auto right = std::make_shared<AnyInt_ConstValue>(200);
        auto val = visitor.calc_const_binary_expr(Binary_Operator::ADD, left, right);
        assert(val->kind == ConstValueKind::ANYINT);
        auto anyint_val = std::dynamic_pointer_cast<AnyInt_ConstValue>(val);
        assert(anyint_val->value == 300);
    }
    {
        auto left = std::make_shared<AnyInt_ConstValue>(300);
        auto right = std::make_shared<I32_ConstValue>(100);
        auto val = visitor.calc_const_binary_expr(Binary_Operator::SUB, left, right);
        assert(val->kind == ConstValueKind::I32);
        auto i32_val = std::dynamic_pointer_cast<I32_ConstValue>(val);
        assert(i32_val->value == 200);
    }
    {
        bool caught = false;
        try {
            auto left = std::make_shared<U32_ConstValue>(400);
            auto right = std::make_shared<I32_ConstValue>(100);
            auto val = visitor.calc_const_binary_expr(Binary_Operator::MUL, left, right);
        } catch (const string &e) {
            caught = true;
            cout << "err info : " << e << endl;
        }
        assert(caught);
    }
    {
        bool caught = false;
        try {
            auto left = std::make_shared<Bool_ConstValue>(true);
            auto right = std::make_shared<Bool_ConstValue>(false);
            auto val = visitor.calc_const_binary_expr(Binary_Operator::ADD, left, right);
        } catch (const string &e) {
            caught = true;
            cout << "err info : " << e << endl;
        }
        assert(caught);
    }
    // 很多没覆盖到
    // 我觉得问题不大
}

int main() {
    map<size_t, Scope_ptr> node_scope_map_;
    map<ConstDecl_ptr, ConstValue_ptr> const_value_map_;
    map<size_t, RealType_ptr> type_map_;
    map<size_t, size_t> const_expr_to_size_map_;
    ConstItemVisitor visitor(false, node_scope_map_, const_value_map_, type_map_, const_expr_to_size_map_);
    test_parse_literal_token_to_const_value(visitor);
    test_calc_const_unary_expr(visitor);
    test_calc_const_binary_expr(visitor);
    cout << "All tests passed!" << endl;
    return 0;
}
