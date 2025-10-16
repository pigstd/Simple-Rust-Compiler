#include "lexer.h"
#include "parser.h"
#include "semantic_checker.h"
#include <iostream>


/*
semantic check
可以通过编译输出 0
出错输出 -1 并且打印错误信息到 stderr
*/

int main() {
    Lexer lexer;
    try {
        lexer.read_and_get_tokens();
        Parser parser(lexer);
        auto ast = parser.parse();
        Semantic_Checker checker(ast);
        checker.checker();
    } catch(string err_infomation) {
        std::cerr << err_infomation << std::endl;
        std::cout << -1 << std::endl;
        return 0;
    }
    std::cout << 0 << std::endl;
    return 0;
}