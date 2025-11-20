#include "ir/type_lowering.h"
#include <type_traits>

using namespace ir;

static_assert(std::is_constructible_v<TypeLowering, IRModule &>);
static_assert(std::is_same_v<IRType_ptr (TypeLowering::*)(RealType_ptr),
                             decltype(&TypeLowering::lower)>);
static_assert(std::is_same_v<std::shared_ptr<FunctionType> (TypeLowering::*)(FnDecl_ptr),
                             decltype(&TypeLowering::lower_function)>);
static_assert(std::is_same_v<std::shared_ptr<ConstantValue> (TypeLowering::*)(ConstValue_ptr, RealType_ptr),
                             decltype(&TypeLowering::lower_const)>);
static_assert(std::is_same_v<std::shared_ptr<StructType> (TypeLowering::*)(StructDecl_ptr),
                             decltype(&TypeLowering::declare_struct)>);
static_assert(std::is_same_v<void (TypeLowering::*)(),
                             decltype(&TypeLowering::declare_builtin_string_types)>);

int main() {
    return 0;
}
