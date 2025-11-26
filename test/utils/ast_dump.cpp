#include "ast/visitor.h"
#include "lexer/lexer.h"
#include "parser/parser.h"

#include <iostream>
#include <stdexcept>
#include <string>

int main() {
    try {
        Lexer lexer;
        lexer.read_and_get_tokens();
        Parser parser(lexer);
        auto items = parser.parse();

        AST_Printer printer;
        for (const auto &item : items) {
            if (item) {
                item->accept(printer);
            }
        }
        return 0;
    } catch (const std::runtime_error &err) {
        std::cerr << err.what() << std::endl;
    } catch (const std::string &err) {
        std::cerr << err << std::endl;
    }
    return 1;
}
