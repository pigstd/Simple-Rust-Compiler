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

std::string strip_module_header(std::string text) {
    const auto header_sep = text.find("\n\n");
    if (header_sep != std::string::npos) {
        text = text.substr(header_sep + 2);
    }
    const auto first = text.find_first_not_of("\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = text.find_last_not_of("\r\n");
    return text.substr(first, last - first + 1);
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

        std::cout << strip_module_header(module.to_string()) << std::endl;
        return 0;
    } catch (const std::runtime_error &err) {
        std::cerr << err.what() << std::endl;
    } catch (const std::string &err) {
        std::cerr << err << std::endl;
    }
    return 1;
}
