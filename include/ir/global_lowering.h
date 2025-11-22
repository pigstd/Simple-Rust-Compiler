#ifndef SIMPLE_RUST_COMPILER_IR_GLOBALS_H
#define SIMPLE_RUST_COMPILER_IR_GLOBALS_H

#include "ir/IRBuilder.h"
#include "semantic/consteval.h"
#include "semantic/decl.h"
#include "semantic/scope.h"
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ir {

class TypeLowering;

class GlobalLoweringDriver {
  public:
    GlobalLoweringDriver(IRModule &module, IRBuilder &builder,
                         TypeLowering &type_lowering,
                         std::map<ConstDecl_ptr, ConstValue_ptr> &const_values);

    void emit_scope_tree(Scope_ptr root_scope);

  private:
    void visit_scope(Scope_ptr scope);
    void emit_const(ConstDecl_ptr decl);
    void emit_function_decl(FnDecl_ptr decl, bool define_body);
    GlobalValue_ptr ensure_global_bytes(std::string symbol,
                                        const std::vector<uint8_t> &bytes,
                                        bool is_constant);
    std::string allocate_symbol(std::string_view base);

    IRModule &module_;
    IRBuilder &builder_;
    TypeLowering &type_lowering_;
    std::map<ConstDecl_ptr, ConstValue_ptr> &const_values_;
    std::unordered_map<std::string, GlobalValue_ptr> globals_;
    std::vector<std::string> scope_suffix_stack_;
};

} // namespace ir

#endif // SIMPLE_RUST_COMPILER_IR_GLOBALS_H
