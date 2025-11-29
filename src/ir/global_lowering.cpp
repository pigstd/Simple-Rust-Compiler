#include "ir/global_lowering.h"

#include "ir/type_lowering.h"
#include "semantic/scope.h"
#include "semantic/type.h"

#include <cctype>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace ir {

GlobalLoweringDriver::GlobalLoweringDriver(
    IRModule &module, IRBuilder &builder, TypeLowering &type_lowering,
    std::map<ConstDecl_ptr, ConstValue_ptr> &const_values)
    : module_(module), builder_(builder), type_lowering_(type_lowering),
      const_values_(const_values) {}

void GlobalLoweringDriver::emit_scope_tree(Scope_ptr root_scope) {
    if (!root_scope) {
        throw std::runtime_error("GlobalLoweringDriver requires root scope");
    }
    scope_suffix_stack_.clear();
    globals_.clear();
    visit_scope(root_scope);
}

void GlobalLoweringDriver::visit_scope(Scope_ptr scope) {
    if (!scope) {
        return;
    }
    std::vector<StructDecl_ptr> struct_decls;
    struct_decls.reserve(scope->type_namespace.size());
    for (const auto &entry : scope->type_namespace) {
        if (!entry.second || entry.second->kind != TypeDeclKind::Struct) {
            continue;
        }
        auto struct_decl =
            std::dynamic_pointer_cast<StructDecl>(entry.second);
        if (!struct_decl) {
            throw std::runtime_error("Invalid StructDecl in scope");
        }
        struct_decls.push_back(struct_decl);
        type_lowering_.declare_struct_stub(struct_decl);
    }
    for (const auto &struct_decl : struct_decls) {
        type_lowering_.define_struct_fields(struct_decl);
    }
    for (const auto &struct_decl : struct_decls) {
        if (!struct_decl) {
            continue;
        }
        auto real_type = std::make_shared<StructRealType>(
            struct_decl->name, ReferenceType::NO_REF, struct_decl);
        (void)type_lowering_.size_in_bytes(real_type);
    }

    std::size_t local_counter = 0;
    const std::string scope_suffix = current_scope_suffix();
    for (const auto &entry : scope->value_namespace) {
        if (!entry.second) {
            continue;
        }
        if (entry.second->kind == ValueDeclKind::Function) {
            auto fn_decl = std::dynamic_pointer_cast<FnDecl>(entry.second);
            if (fn_decl && fn_decl->ast_node) {
                emit_function_decl(fn_decl, scope_suffix,
                                   /*define_body=*/false);
            }
        } else if (entry.second->kind == ValueDeclKind::Constant) {
            auto const_decl =
                std::dynamic_pointer_cast<ConstDecl>(entry.second);
            emit_const(const_decl, scope_suffix);
        }
    }

    for (const auto &child : scope->children) {
        if (!child) {
            continue;
        }
        std::string segment = "." + std::to_string(local_counter++);
        scope_suffix_stack_.push_back(segment);
        visit_scope(child);
        scope_suffix_stack_.pop_back();
    }
}

void GlobalLoweringDriver::emit_const(ConstDecl_ptr decl,
                                      const std::string &suffix) {
    if (!decl || !decl->const_type) {
        return;
    }
    auto array_type =
        std::dynamic_pointer_cast<ArrayRealType>(decl->const_type);
    if (!array_type) {
        return; // Only array consts are lowered at this stage.
    }
    auto value_iter = const_values_.find(decl);
    if (value_iter == const_values_.end() || !value_iter->second) {
        throw std::runtime_error("Const value missing for declaration");
    }
    auto ir_type = type_lowering_.lower(decl->const_type);
    auto ir_array_type = std::dynamic_pointer_cast<ArrayType>(ir_type);
    if (!ir_array_type) {
        throw std::runtime_error("Expected array IRType for const decl");
    }

    std::function<std::string(ConstValue_ptr, RealType_ptr, bool)> emit_value;
    emit_value = [&](ConstValue_ptr value, RealType_ptr type,
                     bool include_type) -> std::string {
        if (!value || !type) {
            throw std::runtime_error("Invalid const value or type");
        }
        if (type->kind == RealTypeKind::ARRAY) {
            auto nested_arr_type =
                std::dynamic_pointer_cast<ArrayRealType>(type);
            auto nested_value =
                std::dynamic_pointer_cast<Array_ConstValue>(value);
            if (!nested_arr_type || !nested_value) {
                throw std::runtime_error(
                    "Array const mismatch between value and type");
            }
            if (nested_arr_type->element_type == nullptr) {
                throw std::runtime_error("Array element type missing");
            }
            std::ostringstream oss;
            if (include_type) {
                auto lowered_type = type_lowering_.lower(type);
                auto lowered_array =
                    std::dynamic_pointer_cast<ArrayType>(lowered_type);
                if (!lowered_array) {
                    throw std::runtime_error(
                        "Expected IR array when lowering nested const");
                }
                oss << lowered_array->to_string() << " ";
            }
            oss << "[ ";
            for (std::size_t idx = 0; idx < nested_value->elements.size();
                 ++idx) {
                if (idx > 0) {
                    oss << ", ";
                }
                oss << emit_value(nested_value->elements[idx],
                                  nested_arr_type->element_type, true);
            }
            oss << " ]";
            return oss.str();
        }
        auto lowered_value = type_lowering_.lower_const(value, type);
        if (!lowered_value) {
            throw std::runtime_error("Unsupported const scalar lowering");
        }
        return lowered_value->typed_repr();
    };

    const std::string base_name = decl->name;
    const std::string symbol = "const." + base_name + suffix;
    decl->name = symbol;
    const std::string init_text =
        emit_value(value_iter->second, decl->const_type, false);
    auto global = module_.create_global(symbol, ir_array_type, init_text, true,
                                        "private");
    globals_[symbol] = global;
}

void GlobalLoweringDriver::emit_function_decl(FnDecl_ptr decl,
                                              const std::string &suffix,
                                              bool define_body) {
    if (!decl) {
        return;
    }
    const std::string symbol = decl->name + suffix;
    decl->name = symbol;
    auto fn_type = type_lowering_.lower_function(decl);
    IRFunction_ptr fn = nullptr;
    if (define_body) {
        fn = module_.define_function(symbol, fn_type);
    } else {
        fn = module_.declare_function(symbol, fn_type, false);
    }
    if (!fn) {
        return;
    }
    if (!fn->params().empty()) {
        return; // Parameters already named.
    }
    const auto &params = fn_type->param_types();
    for (std::size_t i = 0; i < params.size(); ++i) {
        fn->add_param("arg." + std::to_string(i), params[i]);
    }
}

std::string GlobalLoweringDriver::current_scope_suffix() const {
    std::string suffix;
    for (const auto &segment : scope_suffix_stack_) {
        suffix += segment;
    }
    return suffix;
}

} // namespace ir
