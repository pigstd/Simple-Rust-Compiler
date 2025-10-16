#include "ast/ast.h"
#include "ast/visitor.h"
#include "parser/parser.h"
#include <iostream>

/*
输出 AST 结构，方便调试
*/

int main() {
    Lexer lexer;
    try {
        lexer.read_and_get_tokens();
    } catch(string err_infomation) {
        std::cerr << err_infomation << std::endl;
        return 0;
    }
    Parser parser(lexer);
    try {
        auto ast = parser.parse();
        AST_Printer printer;
        for (const auto &item : ast) {
            item->accept(printer);
        }
    } catch(string err_infomation) {
        std::cerr << err_infomation << std::endl;
        return 0;
    }
    return 0;
}
