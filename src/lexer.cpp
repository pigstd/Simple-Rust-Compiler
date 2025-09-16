#include "lexer.h"
#include <cstddef>
#include <iostream>

Lexer::Lexer() {
    // 初始化关键字映射
    keywords_map = {
        {"let", Token_type::LET},
        {"mut", Token_type::MUT},
        {"fn", Token_type::FN},
        {"if", Token_type::IF},
        {"else", Token_type::ELSE},
        {"while", Token_type::WHILE},
        {"for", Token_type::FOR},
        {"in", Token_type::IN},
        {"return", Token_type::RETURN},
        {"break", Token_type::BREAK},
        {"continue", Token_type::CONTINUE},
        {"struct", Token_type::STRUCT},
        {"as", Token_type::AS},
        {"const", Token_type::CONST},
        {"enum", Token_type::ENUM},
        {"impl", Token_type::IMPL},
        {"loop", Token_type::LOOP},
        {"ref", Token_type::REF},
        {"use", Token_type::USE},
        {"self", Token_type::SELF},
        {"true", Token_type::TRUE},
        {"false", Token_type::FALSE}
    };
    // 初始化符号映射
    symbol_map = {
        {"(", Token_type::LEFT_PARENTHESIS},
        {")", Token_type::RIGHT_PARENTHESIS},
        {"{", Token_type::LEFT_BRACE},
        {"}", Token_type::RIGHT_BRACE},
        {"[", Token_type::LEFT_BRACKET},
        {"]", Token_type::RIGHT_BRACKET},
        {";", Token_type::SEMICOLON},
        {",", Token_type::COMMA},
        {"+", Token_type::PLUS},
        {"-", Token_type::MINUS},
        {"*", Token_type::STAR},
        {"/", Token_type::SLASH},
        {"%", Token_type::PERCENT},
        {"&", Token_type::AMPERSAND},
        {"|", Token_type::PIPE},
        {"^", Token_type::CARET},
        {"+=", Token_type::PLUS_EQUAL},
        {"-=", Token_type::MINUS_EQUAL},
        {"*=", Token_type::STAR_EQUAL},
        {"/=", Token_type::SLASH_EQUAL},
        {"%=", Token_type::PERCENT_EQUAL},
        {"&=", Token_type::AMPERSAND_EQUAL},
        {"|=", Token_type::PIPE_EQUAL},
        {"^=", Token_type::CARET_EQUAL},
        {"<<", Token_type::LEFT_SHIFT},
        {">>", Token_type::RIGHT_SHIFT},
        {"<<=", Token_type::LEFT_SHIFT_EQUAL},
        {">>=", Token_type::RIGHT_SHIFT_EQUAL},
        {".", Token_type::DOT},
        {"->", Token_type::ARROW},
        {":", Token_type::COLON},
        {"::", Token_type::COLON_COLON},
        {"~", Token_type::TILDE},
        {"!", Token_type::BANG},
        {"=", Token_type::EQUAL},
        {"!=", Token_type::NOT_EQUAL},
        {"<", Token_type::LESS},
        {">", Token_type::GREATER},
        {"<=", Token_type::LESS_EQUAL},
        {">=", Token_type::GREATER_EQUAL},
        {"&&", Token_type::AMPERSAND_AMPERSAND},
        {"||", Token_type::PIPE_PIPE}
    };
}

string Token::token_type_to_string() const {
    switch (type) {
        case Token_type::LEFT_PARENTHESIS: return "LEFT_PARENTHESIS";
        case Token_type::RIGHT_PARENTHESIS: return "RIGHT_PARENTHESIS";
        case Token_type::LEFT_BRACE: return "LEFT_BRACE";
        case Token_type::RIGHT_BRACE: return "RIGHT_BRACE";
        case Token_type::LEFT_BRACKET: return "LEFT_BRACKET";
        case Token_type::RIGHT_BRACKET: return "RIGHT_BRACKET";
        case Token_type::SEMICOLON: return "SEMICOLON";
        case Token_type::COMMA: return "COMMA";
        case Token_type::PLUS: return "PLUS";
        case Token_type::MINUS: return "MINUS";
        case Token_type::STAR: return "STAR";
        case Token_type::SLASH: return "SLASH";
        case Token_type::PERCENT: return "PERCENT";
        case Token_type::AMPERSAND: return "AMPERSAND";
        case Token_type::PIPE: return "PIPE";
        case Token_type::CARET: return "CARET";
        case Token_type::PLUS_EQUAL: return "PLUS_EQUAL";
        case Token_type::MINUS_EQUAL: return "MINUS_EQUAL";
        case Token_type::STAR_EQUAL: return "STAR_EQUAL";
        case Token_type::SLASH_EQUAL: return "SLASH_EQUAL";
        case Token_type::PERCENT_EQUAL: return "PERCENT_EQUAL";
        case Token_type::AMPERSAND_EQUAL: return "AMPERSAND_EQUAL";
        case Token_type::PIPE_EQUAL: return "PIPE_EQUAL";
        case Token_type::CARET_EQUAL: return "CARET_EQUAL";
        case Token_type::LEFT_SHIFT: return "LEFT_SHIFT";
        case Token_type::RIGHT_SHIFT: return "RIGHT_SHIFT";
        case Token_type::LEFT_SHIFT_EQUAL: return "LEFT_SHIFT_EQUAL";
        case Token_type::RIGHT_SHIFT_EQUAL: return "RIGHT_SHIFT_EQUAL";
        case Token_type::DOT: return "DOT";
        case Token_type::ARROW: return "ARROW";
        case Token_type::COLON: return "COLON";
        case Token_type::COLON_COLON: return "COLON_COLON";
        case Token_type::TILDE: return "TILDE";
        case Token_type::BANG: return "BANG";
        case Token_type::EQUAL: return "EQUAL";
        case Token_type::NOT_EQUAL: return "NOT_EQUAL";
        case Token_type::LESS: return "LESS";
        case Token_type::GREATER: return "GREATER";
        case Token_type::LESS_EQUAL: return "LESS_EQUAL";
        case Token_type::GREATER_EQUAL: return "GREATER_EQUAL";
        case Token_type::AMPERSAND_AMPERSAND: return "AMPERSAND_AMPERSAND";
        case Token_type::PIPE_PIPE: return "PIPE_PIPE";
        case Token_type::IDENTIFIER: return "IDENTIFIER";
        case Token_type::NUMBER: return "NUMBER";
        case Token_type::STRING: return "STRING";
        case Token_type::CHAR: return "CHAR";
        case Token_type::TRUE: return "TRUE";
        case Token_type::FALSE: return "FALSE";
        case Token_type::LET: return "LET";
        case Token_type::MUT: return "MUT";
        case Token_type::FN: return "FN";
        case Token_type::IF: return "IF";
        case Token_type::ELSE: return "ELSE";
        case Token_type::WHILE: return "WHILE";
        case Token_type::FOR: return "FOR";
        case Token_type::IN: return "IN";
        case Token_type::RETURN: return "RETURN";
        case Token_type::BREAK: return "BREAK";
        case Token_type::CONTINUE: return "CONTINUE";
        case Token_type::STRUCT: return "STRUCT";
        case Token_type::AS: return "AS";
        case Token_type::CONST: return "CONST";
        case Token_type::ENUM: return "ENUM";
        case Token_type::IMPL: return "IMPL";
        case Token_type::LOOP: return "LOOP";
        case Token_type::REF: return "REF";
        case Token_type::USE: return "USE";
        case Token_type::SELF: return "SELF";
        default: return "UNKNOWN";
    }
}

void Token::show_token() const {
    std::cerr << "[" << token_type_to_string() << ", " << value << "]" << std::endl;
}

Token::Token(Token_type type, string value) {
    this->type = type, this->value = value;
}

string Lexer::get_escape_character(char ch) const {
    switch (ch) {
        case 'n': return "\n";
        case 't': return "\t";
        case '0': return "\0";
        case '\\': return "\\";
        case '\'': return "\'";
        case '\"': return "\"";
        default: throw "CE, no such escape character";
    }
}
bool Lexer::is_not_symbol(char ch) const {
    if (ch <= '9' && ch >= '0') return true;
    if (ch <= 'z' && ch >= 'a') return true;
    if (ch <= 'Z' && ch >= 'A') return true;
    if (ch == '_') return true;
    return false;
}

void Lexer::add_identifier(string &now_str) {
    if (keywords_map.find(now_str) != keywords_map.end()) {
        tokens.push_back(Token(keywords_map[now_str], now_str));
    } else {
        tokens.push_back(Token(Token_type::IDENTIFIER, now_str));
    }
    now_str = "";
}

void Lexer::add_number(string &now_str) {
    bool is_valid = true;
    // to do : 判断是否合法的数字
    // 只需要考虑整数
    // 需要考虑：后缀（比如 _i32..）与前缀（0x.. 0o.. 0b..）
    // 暂时没有实现，先不急
    if (is_valid) {
        tokens.push_back(Token(Token_type::NUMBER, now_str));
    } else {
        throw "CE, invalid number";
    }
    now_str = "";
}

void Lexer::read_and_get_tokens() {
    string line;
    // 是否在 /* ... */ 多行注释中
    bool is_in_block_comment = false;
    while (getline(std::cin, line)) {
        line = line + "  ";
        size_t line_len = line.size();
        // 单行注释可以直接 break
        bool is_in_common_string = false; // 是否在 "" 中
        bool is_in_raw_string = false;    // 是否在 r"" 或者 r#""# 中
        size_t raw_string_level = 0;         // raw string 的 # 数量
        bool is_in_char = false; // 是否在 '' 中
        bool is_in_number = false; // 是否在一个数字中
        bool is_in_identifier = false; // 是否在一个标识符中
        string now_str = "";
        for (size_t i = 0; i + 2 < line_len; i++) {
            char now_ch = line[i];
            char next_ch = line[i + 1];
            char next_next_ch = line[i + 2];
            // 处理多行注释 : /* ... */
            if (is_in_block_comment) {
                if (now_ch == '*' && next_ch == '/') {
                    is_in_block_comment = false;
                    i++;
                }
                continue;
            }
            if (is_in_char) {
                if (now_ch == '\'') {throw "CE, find '' ";}
                else if (now_ch == '\\') {
                    if (next_next_ch != '\'') {
                        throw "CE, find no '";
                    }
                    tokens.push_back(Token(Token_type::CHAR, get_escape_character(next_ch)));
                    i += 2;
                } else {
                    if (next_ch != '\'') {
                        throw "CE, find no '";
                    }
                    tokens.push_back(Token(Token_type::CHAR, string(1, now_ch)));
                    i += 1;
                }
                is_in_char = false;
                continue;
            }
            if (is_in_common_string) {
                if (now_ch == '\"') {
                    is_in_common_string = false;
                    tokens.push_back(Token(Token_type::STRING, now_str));
                    now_str = "";
                } else if (now_ch == '\\') {
                    now_str += get_escape_character(next_ch);
                    i++;
                } else {
                    now_str += now_ch;
                }
                continue;
            }
            if (is_in_raw_string) {
                if (now_ch == '\"') {
                    bool can_end = true;
                    for (size_t j = 1; j <= raw_string_level; j++) {
                        if (i + j >= line_len || line[i + j] != '#') {
                            can_end = false;
                            break;
                        }
                    }
                    if (can_end) {
                        is_in_raw_string = false;
                        tokens.push_back(Token(Token_type::STRING, now_str));
                        now_str = "";
                        i += raw_string_level;
                    } else {
                        now_str += now_ch;
                    }
                } else {
                    now_str += now_ch;
                }
                continue;
            }
            if (now_ch == '/' && next_ch == '*') {
                is_in_block_comment = true;
                i++;
                continue;
            }
            if (now_ch == '/' && next_ch == '/') {
                break;
            }
            if (is_not_symbol(now_ch)) {
                // 需要特殊考虑：raw string 的情况
                if (now_ch == 'r' && (next_ch == '\"' || next_ch == '#')) {
                    is_in_raw_string = true;
                    if (next_ch == '#') {
                        raw_string_level = 1;
                        size_t j = i + 2;
                        while (j < line_len && line[j] == '#') {
                            raw_string_level++;
                            j++;
                        }
                        if (j < line_len && line[j] == '\"') {
                            i = j;
                        } else {
                            throw "CE, no starting of raw string";
                        }
                    } else {
                        i++;
                        raw_string_level = 0;
                    }
                    if (is_in_number) {
                        add_number(now_str);
                        is_in_number = false;
                    }
                    if (is_in_identifier) {
                        add_identifier(now_str);
                        is_in_identifier = false;
                    }
                    continue;
                }
                if (is_in_number || is_in_identifier) {now_str += now_ch;}
                else {
                    now_str = string(1, now_ch);
                    if (now_ch <= '9' && now_ch >= '0') {is_in_number = true;}
                    else {is_in_identifier = true;}
                }
            }
            else {
                if (is_in_number) {
                    add_number(now_str);
                    is_in_number = false;
                }
                if (is_in_identifier) {
                    add_identifier(now_str);
                    is_in_identifier = false;
                }
                // 空白字符直接 continue
                if (now_ch == ' ' || now_ch == '\t' || now_ch == '\r' || now_ch == '\n') {
                    continue;
                }
                // 分成长度为 3 2 1 的符号来处理
                string three_str = string(1, now_ch) + string(1, next_ch) + string(1, next_next_ch);
                string two_str = string(1, now_ch) + string(1, next_ch);
                string one_str = string(1, now_ch);
                if (symbol_map.find(three_str) != symbol_map.end()) {
                    tokens.push_back(Token(symbol_map[three_str], three_str));
                    i += 2;
                } else if (symbol_map.find(two_str) != symbol_map.end()) {
                    tokens.push_back(Token(symbol_map[two_str], two_str));
                    i += 1;
                } else if (symbol_map.find(one_str) != symbol_map.end()) {
                    tokens.push_back(Token(symbol_map[one_str], one_str));
                } else {
                    // 考虑 " " 和 ' ' 的情况，这个时候是作为 string / char
                    if (now_ch == '\"') {
                        is_in_common_string = true;
                        continue;
                    }
                    if (now_ch == '\'') {
                        is_in_char = true;
                        continue;
                    }
                    throw "CE, no such symbol";
                }
            }
        }
        if (is_in_char || is_in_common_string || is_in_raw_string) {
            throw "CE, find no end char or string";
        }
        if (is_in_number) {
            add_number(now_str);
        } else if (is_in_identifier) {
            add_identifier(now_str);
        }
    }
    if (is_in_block_comment) {
        throw "CE, find no end of block comment";
    }
}