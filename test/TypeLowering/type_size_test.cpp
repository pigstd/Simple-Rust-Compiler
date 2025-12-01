#include "ir/type_lowering.h"
#include "semantic/decl.h"
#include "semantic/type.h"
#include <cassert>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

using namespace ir;

namespace {

StructDecl_ptr make_struct(const std::string &name,
                           const std::vector<std::pair<std::string, RealType_ptr>>
                               &fields) {
    auto ast_fields = std::vector<std::pair<std::string, Type_ptr>>();
    for (const auto &field : fields) {
        ast_fields.emplace_back(field.first, nullptr);
    }
    auto ast = std::make_shared<StructItem>(name, ast_fields);
    auto decl = std::make_shared<StructDecl>(ast, name);
    for (const auto &field : fields) {
        decl->field_order.push_back(field.first);
        decl->fields[field.first] = field.second;
    }
    return decl;
}

bool expect_throw(const std::function<void()> &fn) {
    try {
        fn();
    } catch (const std::runtime_error &) {
        return true;
    }
    return false;
}

} // namespace

int main() {
    IRModule module("unknown-unknown-unknown", "");
    TypeLowering lowering(module);
    lowering.declare_builtin_string_types();

    auto bool_rt = std::make_shared<BoolRealType>(ReferenceType::NO_REF);
    auto i32_rt = std::make_shared<I32RealType>(ReferenceType::NO_REF);
    auto char_rt = std::make_shared<CharRealType>(ReferenceType::NO_REF);
    auto unit_rt = std::make_shared<UnitRealType>(ReferenceType::NO_REF);
    auto str_rt = std::make_shared<StrRealType>(ReferenceType::NO_REF);
    auto string_rt = std::make_shared<StringRealType>(ReferenceType::NO_REF);
    auto ref_i32 = std::make_shared<I32RealType>(ReferenceType::REF);

    assert(lowering.size_in_bytes(bool_rt) == 1);
    assert(lowering.size_in_bytes(char_rt) == 1);
    assert(lowering.size_in_bytes(i32_rt) == 4);
    assert(lowering.size_in_bytes(unit_rt) == 0);
    assert(lowering.size_in_bytes(ref_i32) == 4);
    assert(lowering.size_in_bytes(str_rt) == 8);    // ptr + len
    assert(lowering.size_in_bytes(string_rt) == 12); // ptr + len + cap

    auto array_rt = std::make_shared<ArrayRealType>(i32_rt, nullptr,
                                                    ReferenceType::NO_REF, 5);
    assert(lowering.size_in_bytes(array_rt) == 20);

    auto nested_elem =
        std::make_shared<ArrayRealType>(bool_rt, nullptr, ReferenceType::NO_REF, 8);
    auto nested_arr = std::make_shared<ArrayRealType>(
        nested_elem, nullptr, ReferenceType::NO_REF, 3);
    nested_elem->size = 8;
    nested_arr->size = 3;
    assert(lowering.size_in_bytes(nested_arr) == 24); // 3 * (8 * 1)

    auto inner_decl = make_struct(
        "Inner", {{"values", std::make_shared<ArrayRealType>(
                                i32_rt, nullptr, ReferenceType::NO_REF, 2)},
                  {"flag", bool_rt}});
    auto inner_array =
        std::dynamic_pointer_cast<ArrayRealType>(inner_decl->fields["values"]);
    inner_array->size = 2;
    lowering.declare_struct_stub(inner_decl);
    lowering.define_struct_fields(inner_decl);
    auto inner_rt = std::make_shared<StructRealType>(
        "Inner", ReferenceType::NO_REF, inner_decl);
    assert(lowering.size_in_bytes(inner_rt) == 12);

    auto outer_decl =
        make_struct("Outer", {{"inner", inner_rt},
                              {"tail", std::make_shared<ArrayRealType>(
                                           bool_rt, nullptr, ReferenceType::NO_REF, 4)}});
    auto tail_arr =
        std::dynamic_pointer_cast<ArrayRealType>(outer_decl->fields["tail"]);
    tail_arr->size = 4;
    lowering.declare_struct_stub(outer_decl);
    lowering.define_struct_fields(outer_decl);
    auto outer_rt = std::make_shared<StructRealType>(
        "Outer", ReferenceType::NO_REF, outer_decl);
    assert(lowering.size_in_bytes(outer_rt) == 16);

    auto forward_decl = make_struct("Forward", {{"next", inner_rt}});
    auto pending_struct_rt = std::make_shared<StructRealType>(
        "Forward", ReferenceType::NO_REF, forward_decl);
    lowering.declare_struct_stub(forward_decl);
    assert(expect_throw(
        [&]() { (void)lowering.size_in_bytes(pending_struct_rt); }));

    lowering.define_struct_fields(forward_decl);
    assert(lowering.size_in_bytes(pending_struct_rt) == 12);

    auto bool_int_decl =
        make_struct("BoolInt", {{"flag", bool_rt}, {"value", i32_rt}});
    lowering.declare_struct_stub(bool_int_decl);
    lowering.define_struct_fields(bool_int_decl);
    auto bool_int_rt = std::make_shared<StructRealType>(
        "BoolInt", ReferenceType::NO_REF, bool_int_decl);
    assert(lowering.size_in_bytes(bool_int_rt) == 8);

    auto nested_mixed_decl = make_struct(
        "NestedMixed",
        {{"head", bool_int_rt},
         {"tail", std::make_shared<ArrayRealType>(bool_rt, nullptr,
                                                  ReferenceType::NO_REF, 3)}});
    auto tail_arr_nested =
        std::dynamic_pointer_cast<ArrayRealType>(nested_mixed_decl->fields["tail"]);
    tail_arr_nested->size = 3;
    lowering.declare_struct_stub(nested_mixed_decl);
    lowering.define_struct_fields(nested_mixed_decl);
    auto nested_mixed_rt = std::make_shared<StructRealType>(
        "NestedMixed", ReferenceType::NO_REF, nested_mixed_decl);
    assert(lowering.size_in_bytes(nested_mixed_rt) == 12);

    std::cout << "Type size tests passed\n";
    return 0;
}
