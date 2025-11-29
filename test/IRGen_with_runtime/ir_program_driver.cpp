#include "ir/IRBuilder.h"
#include "ir/IRGen.h"
#include "ir/global_lowering.h"
#include "ir/type_lowering.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/semantic_checker.h"

#include <iostream>
#include <stdexcept>
#include <string>

std::string generate_ir() {
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
    ir::GlobalLoweringDriver global_driver(module, builder, type_lowering,
                                           checker.const_value_map);
    global_driver.emit_scope_tree(checker.root_scope);

    ir::IRGenerator generator(
        module, builder, type_lowering, checker.node_scope_map,
        checker.scope_local_variable_map, checker.type_map,
        checker.node_type_and_place_kind_map, checker.node_outcome_state_map,
        checker.call_expr_to_decl_map, checker.const_value_map,
        checker.fn_item_to_decl_map, checker.identifier_expr_to_decl_map,
        checker.let_stmt_to_decl_map);
    generator.generate(items);
    return module.to_string();
}

int main() {
    try {
        std::cout << generate_ir();
        return 0;
    } catch (const std::runtime_error &err) {
        std::cerr << err.what() << std::endl;
    } catch (const std::string &err) {
        std::cerr << err << std::endl;
    }
    return 1;
}
