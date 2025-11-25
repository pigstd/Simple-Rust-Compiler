#include "ir/IRBuilder.h"
#include "ir/global_lowering.h"
#include "ir/type_lowering.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/semantic_checker.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

constexpr const char *kNotImplementedSentinel = "IRGEN_NOT_IMPLEMENTED";

void drain_stdin_and_run_semantic_checks() {
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
}

} // namespace

int main() {
    try {
        drain_stdin_and_run_semantic_checks();
        std::cout << kNotImplementedSentinel << std::endl;
        return 0;
    } catch (const std::runtime_error &err) {
        std::cerr << err.what() << std::endl;
    } catch (const std::string &err) {
        std::cerr << err << std::endl;
    }
    return 1;
}
