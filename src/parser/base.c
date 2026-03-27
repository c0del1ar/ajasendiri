typedef struct {
    Token *tokens;
    int count;
    int pos;
    int has_error;
    char err[512];
} Parser;

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

static Expr *new_expr(ExprKind kind, int line) {
    Expr *e = (Expr *)calloc(1, sizeof(Expr));
    if (!e) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    e->kind = kind;
    e->line = line;
    return e;
}

static Stmt *new_stmt(StmtKind kind, int line) {
    Stmt *s = (Stmt *)calloc(1, sizeof(Stmt));
    if (!s) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    s->kind = kind;
    s->line = line;
    return s;
}

static FuncDecl *new_func(void) {
    FuncDecl *f = (FuncDecl *)calloc(1, sizeof(FuncDecl));
    if (!f) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    return f;
}

static void expr_list_push(Expr ***items, int *count, int *cap, Expr *expr) {
    if (*count + 1 > *cap) {
        *cap = *cap == 0 ? 4 : *cap * 2;
        Expr **next = (Expr **)realloc(*items, (size_t)*cap * sizeof(Expr *));
        if (!next) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        *items = next;
    }
    (*items)[(*count)++] = expr;
}

static void call_arg_list_push(Expr ***args, char ***arg_names, int *count, int *cap, Expr *arg, const char *arg_name) {
    if (*count + 1 > *cap) {
        *cap = *cap == 0 ? 4 : *cap * 2;
        Expr **next_args = (Expr **)realloc(*args, (size_t)*cap * sizeof(Expr *));
        char **next_names = (char **)realloc(*arg_names, (size_t)*cap * sizeof(char *));
        if (!next_args || !next_names) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        *args = next_args;
        *arg_names = next_names;
    }
    (*args)[*count] = arg;
    (*arg_names)[*count] = arg_name ? xstrdup(arg_name) : NULL;
    (*count)++;
}

static void map_item_list_push(char ***keys, Expr ***values, int *count, int *cap, const char *key, Expr *value) {
    if (*count + 1 > *cap) {
        *cap = *cap == 0 ? 4 : *cap * 2;
        char **next_keys = (char **)realloc(*keys, (size_t)*cap * sizeof(char *));
        Expr **next_values = (Expr **)realloc(*values, (size_t)*cap * sizeof(Expr *));
        if (!next_keys || !next_values) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        *keys = next_keys;
        *values = next_values;
    }
    (*keys)[*count] = xstrdup(key);
    (*values)[*count] = value;
    (*count)++;
}

static void stmt_list_push(Stmt ***items, int *count, int *cap, Stmt *stmt) {
    if (*count + 1 > *cap) {
        *cap = *cap == 0 ? 8 : *cap * 2;
        Stmt **next = (Stmt **)realloc(*items, (size_t)*cap * sizeof(Stmt *));
        if (!next) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        *items = next;
    }
    (*items)[(*count)++] = stmt;
}

static void func_list_push(FuncDecl ***items, int *count, int *cap, FuncDecl *fn) {
    if (*count + 1 > *cap) {
        *cap = *cap == 0 ? 4 : *cap * 2;
        FuncDecl **next = (FuncDecl **)realloc(*items, (size_t)*cap * sizeof(FuncDecl *));
        if (!next) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        *items = next;
    }
    (*items)[(*count)++] = fn;
}

static void type_list_push(TypeDecl ***items, int *count, int *cap, TypeDecl *type_decl) {
    if (*count + 1 > *cap) {
        *cap = *cap == 0 ? 4 : *cap * 2;
        TypeDecl **next = (TypeDecl **)realloc(*items, (size_t)*cap * sizeof(TypeDecl *));
        if (!next) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        *items = next;
    }
    (*items)[(*count)++] = type_decl;
}

static void interface_list_push(InterfaceDecl ***items, int *count, int *cap, InterfaceDecl *decl) {
    if (*count + 1 > *cap) {
        *cap = *cap == 0 ? 4 : *cap * 2;
        InterfaceDecl **next = (InterfaceDecl **)realloc(*items, (size_t)*cap * sizeof(InterfaceDecl *));
        if (!next) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        *items = next;
    }
    (*items)[(*count)++] = decl;
}

static void import_list_push(ImportDecl **items, int *count, int *cap, ImportDecl decl) {
    if (*count + 1 > *cap) {
        *cap = *cap == 0 ? 4 : *cap * 2;
        ImportDecl *next = (ImportDecl *)realloc(*items, (size_t)*cap * sizeof(ImportDecl));
        if (!next) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        *items = next;
    }
    (*items)[(*count)++] = decl;
}

static void export_list_push(char ***items, int *count, int *cap, char *name) {
    if (*count + 1 > *cap) {
        *cap = *cap == 0 ? 4 : *cap * 2;
        char **next = (char **)realloc(*items, (size_t)*cap * sizeof(char *));
        if (!next) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        *items = next;
    }
    (*items)[(*count)++] = name;
}

static void param_list_push(Param **items, int *count, int *cap, Param p) {
    if (*count + 1 > *cap) {
        *cap = *cap == 0 ? 4 : *cap * 2;
        Param *next = (Param *)realloc(*items, (size_t)*cap * sizeof(Param));
        if (!next) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        *items = next;
    }
    (*items)[(*count)++] = p;
}

static void field_list_push(FieldDecl **items, int *count, int *cap, FieldDecl f) {
    if (*count + 1 > *cap) {
        *cap = *cap == 0 ? 4 : *cap * 2;
        FieldDecl *next = (FieldDecl *)realloc(*items, (size_t)*cap * sizeof(FieldDecl));
        if (!next) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        *items = next;
    }
    (*items)[(*count)++] = f;
}

static void interface_method_list_push(InterfaceMethodSig **items, int *count, int *cap, InterfaceMethodSig m) {
    if (*count + 1 > *cap) {
        *cap = *cap == 0 ? 4 : *cap * 2;
        InterfaceMethodSig *next = (InterfaceMethodSig *)realloc(*items, (size_t)*cap * sizeof(InterfaceMethodSig));
        if (!next) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        *items = next;
    }
    (*items)[(*count)++] = m;
}

static void if_branch_list_push(IfBranch **items, int *count, int *cap, IfBranch branch) {
    if (*count + 1 > *cap) {
        *cap = *cap == 0 ? 2 : *cap * 2;
        IfBranch *next = (IfBranch *)realloc(*items, (size_t)*cap * sizeof(IfBranch));
        if (!next) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        *items = next;
    }
    (*items)[(*count)++] = branch;
}

static void match_case_list_push(MatchCase **items, int *count, int *cap, MatchCase mc) {
    if (*count + 1 > *cap) {
        *cap = *cap == 0 ? 2 : *cap * 2;
        MatchCase *next = (MatchCase *)realloc(*items, (size_t)*cap * sizeof(MatchCase));
        if (!next) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        *items = next;
    }
    (*items)[(*count)++] = mc;
}

static void select_case_list_push(SelectCase **items, int *count, int *cap, SelectCase sc) {
    if (*count + 1 > *cap) {
        *cap = *cap == 0 ? 2 : *cap * 2;
        SelectCase *next = (SelectCase *)realloc(*items, (size_t)*cap * sizeof(SelectCase));
        if (!next) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        *items = next;
    }
    (*items)[(*count)++] = sc;
}

static Token parser_peek(Parser *p) {
    return p->tokens[p->pos];
}

static Token parser_prev(Parser *p) {
    return p->tokens[p->pos - 1];
}

static int parser_is_at_end(Parser *p) {
    return parser_peek(p).type == TOK_EOF;
}

static Token parser_advance(Parser *p) {
    if (!parser_is_at_end(p)) {
        p->pos++;
    }
    return parser_prev(p);
}

static int parser_check(Parser *p, TokenType tt) {
    if (parser_is_at_end(p)) {
        return tt == TOK_EOF;
    }
    return parser_peek(p).type == tt;
}

static int parser_check_next(Parser *p, TokenType tt) {
    if (p->pos + 1 >= p->count) {
        return 0;
    }
    return p->tokens[p->pos + 1].type == tt;
}

static int parser_check_next2(Parser *p, TokenType tt) {
    if (p->pos + 2 >= p->count) {
        return 0;
    }
    return p->tokens[p->pos + 2].type == tt;
}

static int parser_check_next3(Parser *p, TokenType tt) {
    if (p->pos + 3 >= p->count) {
        return 0;
    }
    return p->tokens[p->pos + 3].type == tt;
}

static int parser_match(Parser *p, TokenType tt) {
    if (parser_check(p, tt)) {
        parser_advance(p);
        return 1;
    }
    return 0;
}

static int parser_check_next_assign(Parser *p) {
    if (parser_check_next(p, TOK_ASSIGN)) {
        return 1;
    }
    if (p->pos + 1 >= p->count) {
        return 0;
    }
    return strcmp(p->tokens[p->pos + 1].lexeme, "=") == 0;
}

static int parser_match_assign(Parser *p) {
    if (parser_match(p, TOK_ASSIGN)) {
        return 1;
    }
    if (!parser_is_at_end(p) && strcmp(parser_peek(p).lexeme, "=") == 0) {
        parser_advance(p);
        return 1;
    }
    return 0;
}

static void parser_error(Parser *p, Token t, const char *fmt, ...) {
    if (p->has_error) {
        return;
    }
    p->has_error = 1;

    char msg[384];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    snprintf(p->err, sizeof(p->err), "line %d:%d: %s", t.line, t.col, msg);
}

static Token parser_expect(Parser *p, TokenType tt, const char *msg) {
    if (parser_check(p, tt)) {
        return parser_advance(p);
    }
    parser_error(p, parser_peek(p), "%s", msg);
    Token t = {0};
    return t;
}

static void parser_skip_newlines(Parser *p) {
    while (parser_check(p, TOK_NEWLINE)) {
        parser_advance(p);
    }
}

static void parser_skip_layout(Parser *p) {
    while (parser_check(p, TOK_NEWLINE) || parser_check(p, TOK_INDENT) || parser_check(p, TOK_DEDENT)) {
        parser_advance(p);
    }
}

static TypeRef make_type_ref(ValueType kind, const char *custom_name) {
    TypeRef out;
    out.kind = kind;
    out.custom_name = custom_name ? xstrdup(custom_name) : NULL;
    out.func_sig = NULL;
    out.multi_sig = NULL;
    out.list_elem_type = NULL;
    out.map_key_type = NULL;
    out.map_value_type = NULL;
    out.chan_elem_type = NULL;
    return out;
}

static TypeRef parse_type_token(Parser *p, Token t) {
    (void)p;
    if (strcmp(t.lexeme, "int") == 0) {
        return make_type_ref(VT_INT, NULL);
    }
    if (strcmp(t.lexeme, "float") == 0) {
        return make_type_ref(VT_FLOAT, NULL);
    }
    if (strcmp(t.lexeme, "string") == 0) {
        return make_type_ref(VT_STRING, NULL);
    }
    if (strcmp(t.lexeme, "str") == 0) {
        return make_type_ref(VT_STRING, NULL);
    }
    if (strcmp(t.lexeme, "error") == 0) {
        return make_type_ref(VT_ERROR, NULL);
    }
    if (strcmp(t.lexeme, "bool") == 0) {
        return make_type_ref(VT_BOOL, NULL);
    }
    if (strcmp(t.lexeme, "func") == 0) {
        return make_type_ref(VT_FUNCTION, NULL);
    }
    if (strcmp(t.lexeme, "list") == 0) {
        return make_type_ref(VT_LIST, NULL);
    }
    if (strcmp(t.lexeme, "map") == 0 || strcmp(t.lexeme, "dict") == 0) {
        return make_type_ref(VT_MAP, NULL);
    }
    if (strcmp(t.lexeme, "chan") == 0) {
        return make_type_ref(VT_CHANNEL, NULL);
    }
    if (strcmp(t.lexeme, "void") == 0) {
        return make_type_ref(VT_VOID, NULL);
    }
    return make_type_ref(VT_OBJECT, t.lexeme);
}

static void type_ref_list_push(TypeRef **items, int *count, int *cap, TypeRef t) {
    if (*count + 1 > *cap) {
        *cap = *cap == 0 ? 4 : *cap * 2;
        TypeRef *next = (TypeRef *)realloc(*items, (size_t)*cap * sizeof(TypeRef));
        if (!next) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        *items = next;
    }
    (*items)[(*count)++] = t;
}

static TypeRef *type_ref_box(TypeRef t) {
    TypeRef *boxed = (TypeRef *)calloc(1, sizeof(TypeRef));
    if (!boxed) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    *boxed = t;
    return boxed;
}

static TypeRef parse_return_type_ref(Parser *p);

static TypeRef parse_type_ref(Parser *p) {
    Token t = parser_expect(p, TOK_IDENT, "expected type");
    if (p->has_error) {
        return make_type_ref(VT_INVALID, NULL);
    }

    if (strcmp(t.lexeme, "func") != 0 || !parser_check(p, TOK_LPAREN)) {
        TypeRef out = parse_type_token(p, t);
        if (!parser_match(p, TOK_LBRACKET)) {
            return out;
        }

        if (out.kind != VT_LIST && out.kind != VT_MAP && out.kind != VT_CHANNEL) {
            parser_error(p, t, "type '%s' does not support generic parameters", t.lexeme);
            return make_type_ref(VT_INVALID, NULL);
        }

        if (out.kind == VT_LIST || out.kind == VT_CHANNEL) {
            TypeRef elem_t = parse_type_ref(p);
            if (p->has_error) {
                return make_type_ref(VT_INVALID, NULL);
            }
            if (elem_t.kind == VT_VOID || elem_t.kind == VT_INVALID) {
                parser_error(p, parser_prev(p), "generic element type cannot be void/invalid");
                return make_type_ref(VT_INVALID, NULL);
            }
            if (parser_match(p, TOK_COMMA)) {
                parser_error(p, parser_prev(p), "%s generic expects exactly one type argument",
                             out.kind == VT_LIST ? "list" : "chan");
                return make_type_ref(VT_INVALID, NULL);
            }
            parser_expect(p, TOK_RBRACKET, "expected ']' after generic type arguments");
            if (p->has_error) {
                return make_type_ref(VT_INVALID, NULL);
            }
            if (out.kind == VT_LIST) {
                out.list_elem_type = type_ref_box(elem_t);
            } else {
                out.chan_elem_type = type_ref_box(elem_t);
            }
            return out;
        }

        TypeRef key_t = parse_type_ref(p);
        if (p->has_error) {
            return make_type_ref(VT_INVALID, NULL);
        }
        parser_expect(p, TOK_COMMA, "map generic expects two type arguments: map[key, value]");
        if (p->has_error) {
            return make_type_ref(VT_INVALID, NULL);
        }
        TypeRef value_t = parse_type_ref(p);
        if (p->has_error) {
            return make_type_ref(VT_INVALID, NULL);
        }
        if (parser_match(p, TOK_COMMA)) {
            parser_error(p, parser_prev(p), "map generic expects exactly two type arguments");
            return make_type_ref(VT_INVALID, NULL);
        }
        parser_expect(p, TOK_RBRACKET, "expected ']' after generic type arguments");
        if (p->has_error) {
            return make_type_ref(VT_INVALID, NULL);
        }
        if (key_t.kind != VT_STRING) {
            parser_error(p, t, "map key type must be string/str");
            return make_type_ref(VT_INVALID, NULL);
        }
        if (value_t.kind == VT_VOID || value_t.kind == VT_INVALID) {
            parser_error(p, parser_prev(p), "map value type cannot be void/invalid");
            return make_type_ref(VT_INVALID, NULL);
        }
        out.map_key_type = type_ref_box(key_t);
        out.map_value_type = type_ref_box(value_t);
        return out;
    }

    TypeRef out = make_type_ref(VT_FUNCTION, NULL);
    out.func_sig = (FuncTypeSig *)calloc(1, sizeof(FuncTypeSig));
    if (!out.func_sig) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }

    parser_expect(p, TOK_LPAREN, "expected '(' in function type");
    if (p->has_error) {
        return make_type_ref(VT_INVALID, NULL);
    }

    if (!parser_check(p, TOK_RPAREN)) {
        while (1) {
            TypeRef param_t = parse_type_ref(p);
            if (p->has_error) {
                return make_type_ref(VT_INVALID, NULL);
            }
            if (param_t.kind == VT_VOID || param_t.kind == VT_INVALID) {
                parser_error(p, parser_prev(p), "function type parameter cannot be void");
                return make_type_ref(VT_INVALID, NULL);
            }
            type_ref_list_push(&out.func_sig->params, &out.func_sig->param_count, &out.func_sig->param_cap, param_t);
            if (!parser_match(p, TOK_COMMA)) {
                break;
            }
        }
    }

    parser_expect(p, TOK_RPAREN, "expected ')' after function type parameters");
    if (p->has_error) {
        return make_type_ref(VT_INVALID, NULL);
    }
    parser_expect(p, TOK_ARROW, "expected '->' after function type parameters");
    if (p->has_error) {
        return make_type_ref(VT_INVALID, NULL);
    }

    TypeRef ret_t = parse_return_type_ref(p);
    if (p->has_error) {
        return make_type_ref(VT_INVALID, NULL);
    }
    out.func_sig->return_type = (TypeRef *)calloc(1, sizeof(TypeRef));
    if (!out.func_sig->return_type) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    *out.func_sig->return_type = ret_t;
    return out;
}

static TypeRef parse_return_type_ref(Parser *p) {
    if (!parser_match(p, TOK_LPAREN)) {
        return parse_type_ref(p);
    }

    TypeRef out = make_type_ref(VT_MULTI, NULL);
    out.multi_sig = (MultiTypeSig *)calloc(1, sizeof(MultiTypeSig));
    if (!out.multi_sig) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }

    while (1) {
        TypeRef item_t = parse_type_ref(p);
        if (p->has_error) {
            return make_type_ref(VT_INVALID, NULL);
        }
        if (item_t.kind == VT_VOID || item_t.kind == VT_INVALID) {
            parser_error(p, parser_prev(p), "return type item cannot be void/invalid");
            return make_type_ref(VT_INVALID, NULL);
        }
        type_ref_list_push(&out.multi_sig->items, &out.multi_sig->count, &out.multi_sig->cap, item_t);
        if (!parser_match(p, TOK_COMMA)) {
            break;
        }
    }

    parser_expect(p, TOK_RPAREN, "expected ')' after return type list");
    if (p->has_error) {
        return make_type_ref(VT_INVALID, NULL);
    }
    if (out.multi_sig->count < 2) {
        parser_error(p, parser_prev(p), "multi return type requires at least two items");
        return make_type_ref(VT_INVALID, NULL);
    }
    return out;
}

static Expr *parse_expr(Parser *p);
static Stmt *parse_stmt(Parser *p);
