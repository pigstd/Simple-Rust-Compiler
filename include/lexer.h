#ifndef LEXER_H
#define LEXER_H

#include <map>
#include <string>
#include <vector>
using std::string;
using std::vector;
using std::map;

// rust 语言的词法分析器
enum class Token_type {
    LEFT_PARENTHESIS,  // (
    RIGHT_PARENTHESIS, // )
    LEFT_BRACE,        // {
    RIGHT_BRACE,       // }
    LEFT_BRACKET,      // [
    RIGHT_BRACKET,     // ]
    SEMICOLON,         // ;
    COMMA,             // ,
    PLUS,              // +
    MINUS,             // -
    STAR,              // *
    SLASH,             // /
    PERCENT,           // %
    AMPERSAND,         // &
    PIPE,              // |
    CARET,             // ^
    PLUS_EQUAL,        // +=
    MINUS_EQUAL,       // -=
    STAR_EQUAL,        // *=
    SLASH_EQUAL,       // /=
    PERCENT_EQUAL,     // %=
    AMPERSAND_EQUAL,   // &=
    PIPE_EQUAL,        // |=
    CARET_EQUAL,       // ^=
    LEFT_SHIFT,        // <<
    RIGHT_SHIFT,       // >>
    LEFT_SHIFT_EQUAL,  // <<=
    RIGHT_SHIFT_EQUAL, // >>=
    DOT,               // .
    ARROW,             // ->
    COLON,             // :
    COLON_COLON,       // ::
    TILDE,             // ~
    BANG,              // !
    EQUAL,             // =
    NOT_EQUAL,         // !=
    LESS,              // <
    GREATER,           // >
    LESS_EQUAL,        // <=
    GREATER_EQUAL,     // >=
    AMPERSAND_AMPERSAND, // &&
    PIPE_PIPE,           // ||

    IDENTIFIER,        // 标识符
    NUMBER,            // 数字
    STRING,            // 字符串
    CHAR,              // 字符
    TRUE,              // true
    FALSE,             // false
    // 关键字
    LET,               // let
    MUT,               // mut
    FN,                // fn
    IF,                // if
    ELSE,              // else
    WHILE,             // while
    FOR,               // for
    IN,                // in
    RETURN,            // return
    BREAK,             // break
    CONTINUE,          // continue
    STRUCT,            // struct
    AS,                // as
    CONST,             // const
    ENUM,              // enum
    IMPL,              // impl
    LOOP,              // loop
    REF,               // ref
    USE,               // use
    SELF,              // self
    // 还有些不确定也不会搞的，后面再加
};

struct Token {
    Token_type type;
    string value;
    void show_token() const;
};

class Lexer {
    map<string, Token_type> keywords_map;
public:
    // 初始化 keywords_map
    Lexer();
    vector<Token> tokens;
    void read_and_get_tokens();
};

#endif // LEXER_H