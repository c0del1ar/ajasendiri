#include "lexer/module.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *buf;
    int len;
    int cap;
} StrBuf;

static char *xstrndup(const char *s, size_t n) {
    char *out = (char *)malloc(n + 1);
    if (!out) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static char *xstrdup(const char *s) {
    return xstrndup(s, strlen(s));
}

static void sb_init(StrBuf *sb) {
    sb->cap = 32;
    sb->len = 0;
    sb->buf = (char *)malloc((size_t)sb->cap);
    if (!sb->buf) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    sb->buf[0] = '\0';
}

static void sb_push(StrBuf *sb, char c) {
    if (sb->len + 2 > sb->cap) {
        sb->cap *= 2;
        char *next = (char *)realloc(sb->buf, (size_t)sb->cap);
        if (!next) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        sb->buf = next;
    }
    sb->buf[sb->len++] = c;
    sb->buf[sb->len] = '\0';
}

static char *sb_take(StrBuf *sb) {
    return sb->buf;
}

void token_array_init(TokenArray *arr) {
    arr->items = NULL;
    arr->count = 0;
    arr->cap = 0;
}

void token_array_free(TokenArray *arr) {
    if (!arr || !arr->items) {
        return;
    }
    for (int i = 0; i < arr->count; i++) {
        free(arr->items[i].lexeme);
    }
    free(arr->items);
    arr->items = NULL;
    arr->count = 0;
    arr->cap = 0;
}

static void token_array_push(TokenArray *arr, Token t) {
    if (arr->count + 1 > arr->cap) {
        arr->cap = arr->cap == 0 ? 64 : arr->cap * 2;
        Token *next = (Token *)realloc(arr->items, (size_t)arr->cap * sizeof(Token));
        if (!next) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        arr->items = next;
    }
    arr->items[arr->count++] = t;
}

static void add_token(TokenArray *arr, TokenType type, const char *lexeme, int line, int col) {
    Token t;
    t.type = type;
    t.lexeme = xstrdup(lexeme);
    t.line = line;
    t.col = col;
    token_array_push(arr, t);
}

static int count_leading_spaces(const char *line) {
    int n = 0;
    while (line[n] == ' ') {
        n++;
    }
    return n;
}

static int is_ident_start(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

static int is_ident_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

static int push_indent(int **stack, int *depth, int *cap, int value) {
    if (*depth + 1 > *cap) {
        *cap *= 2;
        int *next = (int *)realloc(*stack, (size_t)*cap * sizeof(int));
        if (!next) {
            return 0;
        }
        *stack = next;
    }
    (*stack)[(*depth)++] = value;
    return 1;
}

static int tokenize_line(const char *line, int line_no, int base_col, TokenArray *out, char *err, size_t err_cap) {
    int i = 0;
    while (line[i] != '\0') {
        char ch = line[i];
        int col = base_col + i;

        if (ch == '#') {
            break;
        }

        if (isspace((unsigned char)ch)) {
            i++;
            continue;
        }

        if (ch == '=') {
            if (line[i + 1] == '=') {
                add_token(out, TOK_EQ, "==", line_no, col);
                i += 2;
            } else {
                add_token(out, TOK_ASSIGN, "=", line_no, col);
                i++;
            }
            continue;
        }
        if (ch == '!') {
            if (line[i + 1] == '=') {
                add_token(out, TOK_NEQ, "!=", line_no, col);
                i += 2;
                continue;
            }
            snprintf(err, err_cap, "line %d:%d: unexpected character '!'", line_no, col);
            return 0;
        }
        if (ch == '+') {
            if (line[i + 1] == '+') {
                add_token(out, TOK_PLUSPLUS, "++", line_no, col);
                i += 2;
            } else {
                add_token(out, TOK_PLUS, "+", line_no, col);
                i++;
            }
            continue;
        }
        if (ch == '-') {
            if (line[i + 1] == '>') {
                add_token(out, TOK_ARROW, "->", line_no, col);
                i += 2;
            } else {
                add_token(out, TOK_MINUS, "-", line_no, col);
                i++;
            }
            continue;
        }
        if (ch == '*') {
            add_token(out, TOK_STAR, "*", line_no, col);
            i++;
            continue;
        }
        if (ch == '/') {
            add_token(out, TOK_SLASH, "/", line_no, col);
            i++;
            continue;
        }
        if (ch == '<') {
            if (line[i + 1] == '=') {
                add_token(out, TOK_LTE, "<=", line_no, col);
                i += 2;
            } else {
                add_token(out, TOK_LT, "<", line_no, col);
                i++;
            }
            continue;
        }
        if (ch == '>') {
            if (line[i + 1] == '=') {
                add_token(out, TOK_GTE, ">=", line_no, col);
                i += 2;
            } else {
                add_token(out, TOK_GT, ">", line_no, col);
                i++;
            }
            continue;
        }
        if (ch == '(') {
            add_token(out, TOK_LPAREN, "(", line_no, col);
            i++;
            continue;
        }
        if (ch == '[') {
            add_token(out, TOK_LBRACKET, "[", line_no, col);
            i++;
            continue;
        }
        if (ch == ')') {
            add_token(out, TOK_RPAREN, ")", line_no, col);
            i++;
            continue;
        }
        if (ch == ']') {
            add_token(out, TOK_RBRACKET, "]", line_no, col);
            i++;
            continue;
        }
        if (ch == ',') {
            add_token(out, TOK_COMMA, ",", line_no, col);
            i++;
            continue;
        }
        if (ch == ':') {
            add_token(out, TOK_COLON, ":", line_no, col);
            i++;
            continue;
        }
        if (ch == '.') {
            add_token(out, TOK_DOT, ".", line_no, col);
            i++;
            continue;
        }
        if (ch == '{') {
            add_token(out, TOK_LBRACE, "{", line_no, col);
            i++;
            continue;
        }
        if (ch == '}') {
            add_token(out, TOK_RBRACE, "}", line_no, col);
            i++;
            continue;
        }

        if (ch == '"') {
            int start_col = col;
            i++;
            StrBuf sb;
            int closed = 0;
            sb_init(&sb);
            while (line[i] != '\0') {
                char c = line[i];
                if (c == '"') {
                    i++;
                    closed = 1;
                    break;
                }
                if (c == '\\') {
                    i++;
                    if (line[i] == '\0') {
                        snprintf(err, err_cap, "line %d:%d: unfinished string escape", line_no, start_col);
                        free(sb.buf);
                        return 0;
                    }
                    char e = line[i];
                    if (e == 'n') {
                        sb_push(&sb, '\n');
                    } else if (e == 't') {
                        sb_push(&sb, '\t');
                    } else if (e == '\\') {
                        sb_push(&sb, '\\');
                    } else if (e == '"') {
                        sb_push(&sb, '"');
                    } else {
                        snprintf(err, err_cap, "line %d:%d: invalid escape \\%c", line_no, start_col, e);
                        free(sb.buf);
                        return 0;
                    }
                    i++;
                    continue;
                }
                sb_push(&sb, c);
                i++;
            }
            if (!closed) {
                snprintf(err, err_cap, "line %d:%d: unterminated string", line_no, start_col);
                free(sb.buf);
                return 0;
            }
            Token t;
            t.type = TOK_STRING;
            t.lexeme = sb_take(&sb);
            t.line = line_no;
            t.col = start_col;
            token_array_push(out, t);
            continue;
        }

        if (isdigit((unsigned char)ch)) {
            int start = i;
            int has_dot = 0;
            while (line[i] != '\0') {
                if (line[i] == '.') {
                    if (has_dot) {
                        break;
                    }
                    has_dot = 1;
                    i++;
                    continue;
                }
                if (!isdigit((unsigned char)line[i])) {
                    break;
                }
                i++;
            }
            char *lex = xstrndup(line + start, (size_t)(i - start));
            if (has_dot) {
                char *end_ptr = NULL;
                errno = 0;
                (void)strtod(lex, &end_ptr);
                if (errno != 0 || end_ptr == NULL || *end_ptr != '\0') {
                    snprintf(err, err_cap, "line %d:%d: invalid float literal %s", line_no, base_col + start, lex);
                    free(lex);
                    return 0;
                }
                Token t = {TOK_FLOAT, lex, line_no, base_col + start};
                token_array_push(out, t);
            } else {
                Token t = {TOK_INT, lex, line_no, base_col + start};
                token_array_push(out, t);
            }
            continue;
        }

        if (is_ident_start(ch)) {
            int start = i;
            i++;
            while (line[i] != '\0' && is_ident_char(line[i])) {
                i++;
            }
            char *lex = xstrndup(line + start, (size_t)(i - start));
            TokenType tt = TOK_IDENT;
            if (strcmp(lex, "fuc") == 0) {
                tt = TOK_FUC;
            } else if (strcmp(lex, "return") == 0) {
                tt = TOK_RETURN;
            } else if (strcmp(lex, "for") == 0) {
                tt = TOK_FOR;
            } else if (strcmp(lex, "in") == 0) {
                tt = TOK_IN;
            } else if (strcmp(lex, "while") == 0) {
                tt = TOK_WHILE;
            } else if (strcmp(lex, "do") == 0) {
                tt = TOK_DO;
            } else if (strcmp(lex, "if") == 0) {
                tt = TOK_IF;
            } else if (strcmp(lex, "elif") == 0) {
                tt = TOK_ELIF;
            } else if (strcmp(lex, "else") == 0) {
                tt = TOK_ELSE;
            } else if (strcmp(lex, "and") == 0) {
                tt = TOK_AND;
            } else if (strcmp(lex, "or") == 0) {
                tt = TOK_OR;
            } else if (strcmp(lex, "not") == 0) {
                tt = TOK_NOT;
            } else if (strcmp(lex, "import") == 0) {
                tt = TOK_IMPORT;
            } else if (strcmp(lex, "export") == 0) {
                tt = TOK_EXPORT;
            } else if (strcmp(lex, "from") == 0) {
                tt = TOK_FROM;
            } else if (strcmp(lex, "as") == 0) {
                tt = TOK_AS;
            } else if (strcmp(lex, "type") == 0) {
                tt = TOK_TYPE;
            } else if (strcmp(lex, "interface") == 0) {
                tt = TOK_INTERFACE;
            } else if (strcmp(lex, "imut") == 0) {
                tt = TOK_CONST;
            } else if (strcmp(lex, "break") == 0) {
                tt = TOK_BREAK;
            } else if (strcmp(lex, "continue") == 0) {
                tt = TOK_CONTINUE;
            } else if (strcmp(lex, "defer") == 0) {
                tt = TOK_DEFER;
            } else if (strcmp(lex, "kostroutine") == 0) {
                tt = TOK_KOSTROUTINE;
            } else if (strcmp(lex, "select") == 0) {
                tt = TOK_SELECT;
            } else if (strcmp(lex, "match") == 0) {
                tt = TOK_MATCH;
            } else if (strcmp(lex, "case") == 0) {
                tt = TOK_CASE;
            } else if (strcmp(lex, "default") == 0) {
                tt = TOK_DEFAULT;
            } else if (strcmp(lex, "true") == 0 || strcmp(lex, "false") == 0 || strcmp(lex, "True") == 0 ||
                       strcmp(lex, "False") == 0) {
                tt = TOK_BOOL;
            }
            Token t = {tt, lex, line_no, base_col + start};
            token_array_push(out, t);
            continue;
        }

        snprintf(err, err_cap, "line %d:%d: unexpected character '%c'", line_no, col, ch);
        return 0;
    }

    return 1;
}

int tokenize_source(const char *src, TokenArray *out, char *err, size_t err_cap) {
    size_t len = strlen(src);
    size_t start = 0;
    int line_no = 1;

    int stack_cap = 16;
    int *indent_stack = (int *)malloc((size_t)stack_cap * sizeof(int));
    int stack_depth = 1;
    if (!indent_stack) {
        snprintf(err, err_cap, "out of memory");
        return 0;
    }
    indent_stack[0] = 0;

    while (start <= len) {
        size_t end = start;
        while (end < len && src[end] != '\n') {
            end++;
        }

        char *line = xstrndup(src + start, end - start);
        if (strchr(line, '\t') != NULL) {
            snprintf(err, err_cap, "line %d: tabs are not allowed; use spaces", line_no);
            free(line);
            free(indent_stack);
            return 0;
        }

        int indent = count_leading_spaces(line);
        const char *trimmed = line + indent;

        if (*trimmed == '\0' || *trimmed == '#') {
            add_token(out, TOK_NEWLINE, "\\n", line_no, 1);
        } else {
            int top = indent_stack[stack_depth - 1];
            if (indent > top) {
                if (!push_indent(&indent_stack, &stack_depth, &stack_cap, indent)) {
                    snprintf(err, err_cap, "out of memory");
                    free(line);
                    free(indent_stack);
                    return 0;
                }
                add_token(out, TOK_INDENT, "<indent>", line_no, 1);
            } else if (indent < top) {
                while (stack_depth > 1 && indent_stack[stack_depth - 1] > indent) {
                    stack_depth--;
                    add_token(out, TOK_DEDENT, "<dedent>", line_no, 1);
                }
                if (indent_stack[stack_depth - 1] != indent) {
                    snprintf(err, err_cap, "line %d: invalid indentation", line_no);
                    free(line);
                    free(indent_stack);
                    return 0;
                }
            }

            if (!tokenize_line(line + indent, line_no, indent + 1, out, err, err_cap)) {
                free(line);
                free(indent_stack);
                return 0;
            }
            add_token(out, TOK_NEWLINE, "\\n", line_no, (int)strlen(line) + 1);
        }

        free(line);

        if (end == len) {
            break;
        }
        start = end + 1;
        line_no++;
    }

    while (stack_depth > 1) {
        stack_depth--;
        add_token(out, TOK_DEDENT, "<dedent>", line_no, 1);
    }
    add_token(out, TOK_EOF, "", line_no + 1, 1);
    free(indent_stack);
    return 1;
}
