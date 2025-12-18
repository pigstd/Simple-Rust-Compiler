#include "semantic/decl.h"
#include "semantic/scope.h"

ConstDecl_ptr find_const_decl(Scope_ptr NowScope, string name) {
    while (NowScope != nullptr) {
        for (auto decl : NowScope->value_namespace) {
            if (decl.first == name) {
                return std::dynamic_pointer_cast<ConstDecl>(decl.second);
            }
        }
        NowScope = NowScope->parent.lock();
    }
    return nullptr;
}

void FnDecl::set_builtin_method_self_type(RealType_ptr self_type) {
    builtin_method_self_type = self_type;
}