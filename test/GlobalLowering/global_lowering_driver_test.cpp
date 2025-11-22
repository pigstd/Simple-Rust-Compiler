#include <iostream>
#include <sstream>
#include <string>

int main() {
    std::ostringstream source_buffer;
    source_buffer << std::cin.rdbuf();
    const std::string source = source_buffer.str();

#if 0
    // TODO: integrate Lexer -> Parser -> Semantic_Checker -> GlobalLoweringDriver
    // Example skeleton (same entry as test_semantic_check):
    // std::stringstream input(source);
    // Lexer lexer(input);
    // lexer.read_and_get_tokens();
    // Parser parser(lexer);
    // auto items = parser.parse();
    // Semantic_Checker checker(items);
    // checker.checker();
    // IRModule module;
    // IRBuilder builder(module);
    // TypeLowering lowering(module);
    // ir::GlobalLoweringDriver driver(module, builder, lowering,
    //                                 checker.const_value_map);
    // driver.emit_scope_tree(checker.root_scope);
    // std::cout << module.to_string();
    // return 0;
#endif

    (void)source; // silence unused warning before implementation
    std::cout << "GLOBAL_LOWERING_NOT_IMPLEMENTED" << std::endl;
    return 0;
}
