#include "ir/IRGen.h"
#include "ir/type_lowering.h"
#include <type_traits>

using namespace ir;

static_assert(std::is_base_of_v<AST_Walker, IRGenVisitor>);

static_assert(std::is_constructible_v<
              IRGenerator, IRModule &, IRBuilder &, TypeLowering &,
              std::map<size_t, Scope_ptr> &,
              std::map<Scope_ptr, Local_Variable_map> &,
              std::map<size_t, std::pair<RealType_ptr, PlaceKind>> &,
              std::map<size_t, OutcomeState> &,
              std::map<size_t, FnDecl_ptr> &,
              std::map<ConstDecl_ptr, ConstValue_ptr> &>);

static_assert(std::is_same_v<void (IRGenerator::*)(const std::vector<Item_ptr> &),
                             decltype(&IRGenerator::generate)>);

static_assert(std::is_constructible_v<
              IRGenVisitor, FunctionContext &, IRBuilder &, TypeLowering &,
              std::map<size_t, Scope_ptr> &,
              std::map<size_t, std::pair<RealType_ptr, PlaceKind>> &,
              std::map<size_t, OutcomeState> &,
              std::map<size_t, FnDecl_ptr> &,
              std::map<ConstDecl_ptr, ConstValue_ptr> &>);

static_assert(std::is_default_constructible_v<LoopContext>);
static_assert(std::is_default_constructible_v<FunctionContext>);

int main() {
    return 0;
}
