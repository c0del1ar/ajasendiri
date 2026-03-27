#ifndef AJA_CORE_TOKEN_H
#define AJA_CORE_TOKEN_H

typedef enum {
    TOK_EOF = 0,
    TOK_NEWLINE,
    TOK_INDENT,
    TOK_DEDENT,
    TOK_IDENT,
    TOK_INT,
    TOK_FLOAT,
    TOK_STRING,
    TOK_BOOL,
    TOK_ASSIGN,
    TOK_PLUS,
    TOK_PLUSPLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_EQ,
    TOK_NEQ,
    TOK_LT,
    TOK_LTE,
    TOK_GT,
    TOK_GTE,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_COMMA,
    TOK_COLON,
    TOK_ARROW,
    TOK_DOT,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_FUC,
    TOK_RETURN,
    TOK_FOR,
    TOK_IN,
    TOK_NOTIN,
    TOK_WHILE,
    TOK_DO,
    TOK_IF,
    TOK_ELIF,
    TOK_ELSE,
    TOK_AND,
    TOK_OR,
    TOK_NOT,
    TOK_IMPORT,
    TOK_EXPORT,
    TOK_FROM,
    TOK_AS,
    TOK_TYPE,
    TOK_INTERFACE,
    TOK_CONST,
    TOK_BREAK,
    TOK_CONTINUE,
    TOK_DEFER,
    TOK_KOSTROUTINE,
    TOK_SELECT,
    TOK_MATCH,
    TOK_CASE,
    TOK_DEFAULT
} TokenType;

typedef struct {
    TokenType type;
    char *lexeme;
    int line;
    int col;
} Token;

typedef struct {
    Token *items;
    int count;
    int cap;
} TokenArray;

#endif
