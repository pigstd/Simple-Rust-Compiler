#ifndef SIMPLE_RUST_COMPILER_IR_TYPE_LOWERING_H
#define SIMPLE_RUST_COMPILER_IR_TYPE_LOWERING_H

#include "ir/IRBuilder.h"
#include "semantic/consteval.h"
#include "semantic/decl.h"
#include "semantic/type.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace ir {

class TypeLowering {
  public:
    explicit TypeLowering(IRModule &module);

    IRType_ptr lower(RealType_ptr type);
    std::shared_ptr<FunctionType> lower_function(FnDecl_ptr decl);
    std::shared_ptr<ConstantValue> lower_const(ConstValue_ptr value,
                                               RealType_ptr expected_type);
    std::shared_ptr<StructType> declare_struct_stub(StructDecl_ptr decl);
    void define_struct_fields(StructDecl_ptr decl);
    void declare_builtin_string_types();

  private:
    IRModule &module_;
    VoidType_ptr void_type_;
    IntegerType_ptr i1_type_;
    IntegerType_ptr i8_type_;
    IntegerType_ptr i32_type_;
    std::unordered_map<std::string, std::shared_ptr<StructType>> struct_cache_;
    std::unordered_set<std::string> pending_struct_defs_;
};

} // namespace ir

#endif // SIMPLE_RUST_COMPILER_IR_TYPE_LOWERING_H
