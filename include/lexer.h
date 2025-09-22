#ifndef LEXER_H
#define LEXER_H

#include <cstddef>
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
    EQUAL_EQUAL,       // ==
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
    // TILDE,             // ~
    // remark : Rust 好像没有 ~
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


string token_type_to_string(Token_type type);

struct Token {
    Token_type type;
    string value;
    void show_token() const;
    Token(Token_type _type, string _value);
};

/*
分词的时候，只有 string 和 char 我会存真正的值（只会存内容的字符串）
其他的都只存原来的字符串（比如 114514_i32 我也会存 value = "114514_i32"）
*/
class Lexer {
    map<string, Token_type> keywords_map;
    map<string, Token_type> symbol_map;
    // 获得 / + ch 的转义字符
    string get_escape_character(char ch) const;
    // 判断是否不是符号(非字母，非数字，非 _ )
    bool is_not_symbol(char ch) const;
    // 添加一个标识符（需要判断是否是关键字）
    void add_identifier(string &now_str);
    // 添加一个数字（需要判断是否是合法的数字）
    void add_number(string &now_str);
    size_t current_token_index = 0; // 当前正在处理的 token 的下标
public:
    // 初始化 keywords_map
    Lexer();
    vector<Token> tokens;
    void read_and_get_tokens();
    // 看当前的第一个 token
    Token peek_token() const;
    // 消费掉当前的第一个 token
    Token consume_token();
    // 判断是否还有 token
    bool has_more_tokens() const;
    // 消费一个期望是 type 的 token，否则报错
    Token consume_expect_token(Token_type type);
};

#endif // LEXER_H