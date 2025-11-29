#include "ir/IRBuilder.h"
#include "ir/global_lowering.h"
#include "ir/type_lowering.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/scope.h"
#include "semantic/semantic_checker.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

void collect_structs(const Scope_ptr &scope,
                     std::vector<StructDecl_ptr> &out) {
    if (!scope) {
        return;
    }
    for (const auto &entry : scope->type_namespace) {
        if (!entry.second ||
            entry.second->kind != TypeDeclKind::Struct) {
            continue;
        }
        auto decl = std::dynamic_pointer_cast<StructDecl>(entry.second);
        if (decl) {
            out.push_back(decl);
        }
    }
    for (const auto &child : scope->children) {
        collect_structs(child, out);
    }
}

} // namespace

int main() {
    try {
        Lexer lexer;
        lexer.read_and_get_tokens();
        Parser parser(lexer);
        auto items = parser.parse();
        Semantic_Checker checker(items);
        checker.checker();

        ir::IRModule module("unknown-unknown-unknown", "");
        ir::IRBuilder builder(module);
        ir::TypeLowering type_lowering(module);
        type_lowering.declare_builtin_string_types();
        ir::GlobalLoweringDriver driver(module, builder, type_lowering,
                                        checker.const_value_map);
        driver.emit_scope_tree(checker.root_scope);

        std::vector<StructDecl_ptr> struct_decls;
        collect_structs(checker.root_scope, struct_decls);
        std::vector<std::pair<std::string, std::size_t>> results;
        std::unordered_set<std::string> seen;
        for (const auto &decl : struct_decls) {
            if (!decl || decl->name.empty() ||
                !seen.insert(decl->name).second) {
                continue;
            }
            auto real_type = std::make_shared<StructRealType>(
                decl->name, ReferenceType::NO_REF, decl);
            auto size = type_lowering.size_in_bytes(real_type);
            results.emplace_back(decl->name, size);
        }
        std::sort(results.begin(), results.end(),
                  [](const auto &lhs, const auto &rhs) {
                      return lhs.first < rhs.first;
                  });
        for (const auto &entry : results) {
            std::cout << entry.first << " " << entry.second << "\n";
        }
        return 0;
    } catch (const std::runtime_error &err) {
        std::cerr << err.what() << std::endl;
    } catch (const std::string &err) {
        std::cerr << err << std::endl;
    }
    return 1;
}
