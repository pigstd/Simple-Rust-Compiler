#include "ir/type_lowering.h"
#include "semantic/consteval.h"
#include "semantic/decl.h"
#include "semantic/type.h"
#include <cassert>
#include <memory>
#include <string>
#include <vector>

using namespace ir;

namespace {

StructDecl_ptr make_point_decl() {
    auto ast = std::make_shared<StructItem>(
        "Point", std::vector<std::pair<std::string, Type_ptr>>(
                      {{"x", nullptr}, {"y", nullptr}}));
    auto decl = std::make_shared<StructDecl>(ast);
    decl->fields["x"] = std::make_shared<I32RealType>(ReferenceType::NO_REF);
    decl->fields["y"] = std::make_shared<I32RealType>(ReferenceType::NO_REF);
    return decl;
}

void expect_type_string(const IRType_ptr &type, const std::string &expected) {
    assert(type && type->to_string() == expected);
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
    auto never_rt = std::make_shared<NeverRealType>(ReferenceType::NO_REF);
    auto str_rt = std::make_shared<StrRealType>(ReferenceType::NO_REF);
    auto string_rt = std::make_shared<StringRealType>(ReferenceType::NO_REF);

    expect_type_string(lowering.lower(bool_rt), "i1");
    expect_type_string(lowering.lower(i32_rt), "i32");
    expect_type_string(lowering.lower(char_rt), "i8");
    expect_type_string(lowering.lower(unit_rt), "void");
    expect_type_string(lowering.lower(never_rt), "void");

    auto array_rt =
        std::make_shared<ArrayRealType>(i32_rt, nullptr, ReferenceType::NO_REF, 4);
    expect_type_string(lowering.lower(array_rt), "[4 x i32]");

    expect_type_string(lowering.lower(str_rt), "%Str");
    expect_type_string(lowering.lower(string_rt), "%String");

    auto point_decl = make_point_decl();
    lowering.declare_struct(point_decl);
    auto point_rt =
        std::make_shared<StructRealType>("Point", ReferenceType::NO_REF, point_decl);
    auto point_ref_rt =
        std::make_shared<StructRealType>("Point", ReferenceType::REF, point_decl);

    expect_type_string(lowering.lower(point_rt), "%Point");
    expect_type_string(lowering.lower(point_ref_rt), "ptr");

    auto enum_decl = std::make_shared<EnumDecl>(nullptr);
    enum_decl->variants["A"] = 0;
    auto enum_rt =
        std::make_shared<EnumRealType>("Color", ReferenceType::NO_REF, enum_decl);
    expect_type_string(lowering.lower(enum_rt), "i32");

    auto bool_const =
        lowering.lower_const(std::make_shared<Bool_ConstValue>(true), bool_rt);
    assert(bool_const && bool_const->literal() == 1 &&
           bool_const->type()->to_string() == "i1");

    auto char_const =
        lowering.lower_const(std::make_shared<Char_ConstValue>('a'), char_rt);
    assert(char_const && char_const->literal() == 'a' &&
           char_const->type()->to_string() == "i8");

    auto array_const = std::make_shared<Array_ConstValue>(std::vector<ConstValue_ptr>{});
    assert(lowering.lower_const(array_const, array_rt) == nullptr);

    auto fn_decl =
        std::make_shared<FnDecl>(nullptr, nullptr, fn_reciever_type::NO_RECEIVER);
    fn_decl->parameters.push_back({nullptr, bool_rt});
    fn_decl->parameters.push_back({nullptr, point_rt});
    fn_decl->return_type = bool_rt;
    auto fn_type = lowering.lower_function(fn_decl);
    assert(fn_type);
    assert(fn_type->return_type()->to_string() == "i1");
    const auto &params = fn_type->param_types();
    assert(params.size() == 2);
    assert(params[0]->to_string() == "i1");
    assert(params[1]->to_string() == "%Point");

    return 0;
}
