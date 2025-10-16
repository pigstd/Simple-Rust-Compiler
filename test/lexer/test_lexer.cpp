#include "lexer/lexer.h"
#include <iostream>

int main() {
    Lexer lexer;
    try {
        lexer.read_and_get_tokens();
    } catch(string err_infomation) {
        std::cerr << err_infomation << std::endl;
        return 0;
    }
    for (const auto &token : lexer.tokens)
        token.show_token();
    return 0;
}
