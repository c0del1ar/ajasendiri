static char *join_qualified_name(char *base, const char *suffix) {
    size_t a = strlen(base);
    size_t b = strlen(suffix);
    char *next = (char *)malloc(a + b + 2);
    if (!next) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    memcpy(next, base, a);
    next[a] = '.';
    memcpy(next + a + 1, suffix, b);
    next[a + b + 1] = '\0';
    free(base);
    return next;
}

static Expr *parse_postfix_expr(Parser *p, Expr *base) {
    while (1) {
        if (parser_match(p, TOK_LBRACKET)) {
            Token open = parser_prev(p);
            Expr *e = NULL;

            if (parser_match(p, TOK_COLON)) {
                Expr *end = NULL;
                int has_end = 0;
                if (!parser_check(p, TOK_RBRACKET)) {
                    end = parse_expr(p);
                    if (p->has_error || end == NULL) {
                        return NULL;
                    }
                    has_end = 1;
                }
                parser_expect(p, TOK_RBRACKET, "expected ']' after slice expression");
                if (p->has_error) {
                    return NULL;
                }

                e = new_expr(EX_SLICE, open.line);
                e->as.slice.target = base;
                e->as.slice.start = NULL;
                e->as.slice.end = end;
                e->as.slice.has_start = 0;
                e->as.slice.has_end = has_end;
                base = e;
                continue;
            }

            Expr *start_or_idx = parse_expr(p);
            if (p->has_error || start_or_idx == NULL) {
                return NULL;
            }

            if (parser_match(p, TOK_COLON)) {
                Expr *end = NULL;
                int has_end = 0;
                if (!parser_check(p, TOK_RBRACKET)) {
                    end = parse_expr(p);
                    if (p->has_error || end == NULL) {
                        return NULL;
                    }
                    has_end = 1;
                }
                parser_expect(p, TOK_RBRACKET, "expected ']' after slice expression");
                if (p->has_error) {
                    return NULL;
                }

                e = new_expr(EX_SLICE, open.line);
                e->as.slice.target = base;
                e->as.slice.start = start_or_idx;
                e->as.slice.end = end;
                e->as.slice.has_start = 1;
                e->as.slice.has_end = has_end;
            } else {
                parser_expect(p, TOK_RBRACKET, "expected ']' after index expression");
                if (p->has_error) {
                    return NULL;
                }
                e = new_expr(EX_INDEX, open.line);
                e->as.index.target = base;
                e->as.index.index = start_or_idx;
            }

            base = e;
            continue;
        }

        if (parser_match(p, TOK_LPAREN)) {
            Token open = parser_prev(p);
            Expr *call = new_expr(EX_CALL, open.line);
            call->as.call.callee = base;
            int saw_named_arg = 0;
            if (!parser_check(p, TOK_RPAREN)) {
                while (1) {
                    Expr *arg = NULL;
                    const char *arg_name = NULL;
                    if (parser_check(p, TOK_IDENT) && parser_check_next_assign(p)) {
                        Token name_tok = parser_expect(p, TOK_IDENT, "expected argument name");
                        if (p->has_error) {
                            return NULL;
                        }
                        if (!parser_match_assign(p)) {
                            parser_error(p, parser_peek(p), "expected '=' after argument name");
                            return NULL;
                        }
                        arg = parse_expr(p);
                        if (p->has_error || arg == NULL) {
                            return NULL;
                        }
                        arg_name = name_tok.lexeme;
                        saw_named_arg = 1;
                    } else {
                        if (saw_named_arg) {
                            parser_error(p, parser_peek(p), "positional argument cannot follow named argument");
                            return NULL;
                        }
                        arg = parse_expr(p);
                    }
                    if (p->has_error || arg == NULL) {
                        return NULL;
                    }
                    call_arg_list_push(&call->as.call.args, &call->as.call.arg_names, &call->as.call.arg_count,
                                       &call->as.call.arg_cap, arg, arg_name);
                    if (!parser_match(p, TOK_COMMA)) {
                        break;
                    }
                }
            }
            parser_expect(p, TOK_RPAREN, "expected ')' after function arguments");
            if (p->has_error) {
                return NULL;
            }
            base = call;
            continue;
        }

        break;
    }
    return base;
}

static Expr *parse_lambda_expr(Parser *p, Token f_tok) {
    FuncDecl *fn = new_func();
    fn->line = f_tok.line;
    fn->name = xstrdup("<lambda>");

    parser_expect(p, TOK_LPAREN, "expected '(' after 'fuc' in lambda");
    if (p->has_error) {
        return NULL;
    }

    int saw_default_param = 0;
    int kw_only_mode = 0;
    int seen_kw_only_marker = 0;
    if (!parser_check(p, TOK_RPAREN)) {
        while (1) {
            if (parser_match(p, TOK_STAR)) {
                if (seen_kw_only_marker) {
                    parser_error(p, parser_prev(p), "duplicate '*' marker in parameter list");
                    return NULL;
                }
                if (parser_check(p, TOK_RPAREN)) {
                    parser_error(p, parser_prev(p), "keyword-only marker '*' must be followed by parameters");
                    return NULL;
                }
                parser_expect(p, TOK_COMMA, "expected ',' after '*' in parameter list");
                if (p->has_error) {
                    return NULL;
                }
                kw_only_mode = 1;
                seen_kw_only_marker = 1;
                continue;
            }
            Token pname = parser_expect(p, TOK_IDENT, "expected parameter name");
            if (p->has_error) {
                return NULL;
            }
            parser_expect(p, TOK_COLON, "expected ':' after parameter name");
            if (p->has_error) {
                return NULL;
            }
            TypeRef ptype = parse_type_ref(p);
            if (p->has_error) {
                return NULL;
            }
            if (ptype.kind == VT_VOID || ptype.kind == VT_INVALID) {
                parser_error(p, parser_prev(p), "parameter '%s' cannot use void/invalid type", pname.lexeme);
                return NULL;
            }

            Expr *default_expr = NULL;
            if (parser_match_assign(p)) {
                default_expr = parse_expr(p);
                if (p->has_error || default_expr == NULL) {
                    return NULL;
                }
                saw_default_param = 1;
            } else if (saw_default_param) {
                parser_error(p, pname, "non-default parameter '%s' cannot follow default parameter", pname.lexeme);
                return NULL;
            }

            Param param;
            param.name = xstrdup(pname.lexeme);
            param.type = ptype;
            param.default_expr = default_expr;
            param.is_kw_only = kw_only_mode;
            param.line = pname.line;
            param_list_push(&fn->params, &fn->param_count, &fn->param_cap, param);

            if (!parser_match(p, TOK_COMMA)) {
                break;
            }
        }
    }

    parser_expect(p, TOK_RPAREN, "expected ')' after lambda parameter list");
    if (p->has_error) {
        return NULL;
    }

    parser_expect(p, TOK_ARROW, "expected '->' in lambda");
    if (p->has_error) {
        return NULL;
    }
    fn->return_type = parse_return_type_ref(p);
    if (p->has_error) {
        return NULL;
    }
    if (fn->return_type.kind == VT_VOID || fn->return_type.kind == VT_INVALID) {
        parser_error(p, parser_prev(p), "lambda return type cannot be void/invalid");
        return NULL;
    }

    parser_expect(p, TOK_COLON, "expected ':' after lambda signature");
    if (p->has_error) {
        return NULL;
    }

    Expr *body_expr = parse_expr(p);
    if (p->has_error || body_expr == NULL) {
        return NULL;
    }

    Stmt *ret_stmt = new_stmt(ST_RETURN, f_tok.line);
    expr_list_push(&ret_stmt->as.ret.values, &ret_stmt->as.ret.value_count, &ret_stmt->as.ret.value_cap, body_expr);
    stmt_list_push(&fn->body, &fn->body_count, &fn->body_cap, ret_stmt);

    Expr *lambda_expr = new_expr(EX_LAMBDA, f_tok.line);
    lambda_expr->as.lambda.fn = fn;
    return parse_postfix_expr(p, lambda_expr);
}

static Expr *parse_list_literal(Parser *p, Token open_tok) {
    Expr *list = new_expr(EX_LIST, open_tok.line);
    if (parser_check(p, TOK_RBRACKET)) {
        parser_expect(p, TOK_RBRACKET, "expected ']' after list literal");
        if (p->has_error) {
            return NULL;
        }
        return parse_postfix_expr(p, list);
    }

    Expr *first = parse_expr(p);
    if (p->has_error || first == NULL) {
        return NULL;
    }

    if (parser_match(p, TOK_FOR)) {
        Token var_tok = parser_expect(p, TOK_IDENT, "expected loop variable after 'for' in list comprehension");
        if (p->has_error) {
            return NULL;
        }
        parser_expect(p, TOK_IN, "expected 'in' after loop variable in list comprehension");
        if (p->has_error) {
            return NULL;
        }
        Expr *iterable = parse_expr(p);
        if (p->has_error || iterable == NULL) {
            return NULL;
        }

        Expr *cond = NULL;
        int has_cond = 0;
        if (parser_match(p, TOK_IF)) {
            cond = parse_expr(p);
            if (p->has_error || cond == NULL) {
                return NULL;
            }
            has_cond = 1;
        }

        parser_expect(p, TOK_RBRACKET, "expected ']' after list comprehension");
        if (p->has_error) {
            return NULL;
        }

        Expr *comp = new_expr(EX_LIST_COMP, open_tok.line);
        comp->as.list_comp.item_expr = first;
        comp->as.list_comp.var_name = xstrdup(var_tok.lexeme);
        comp->as.list_comp.iterable = iterable;
        comp->as.list_comp.cond = cond;
        comp->as.list_comp.has_cond = has_cond;
        return parse_postfix_expr(p, comp);
    }

    expr_list_push(&list->as.list_lit.items, &list->as.list_lit.count, &list->as.list_lit.cap, first);
    while (parser_match(p, TOK_COMMA)) {
        Expr *item = parse_expr(p);
        if (p->has_error || item == NULL) {
            return NULL;
        }
        expr_list_push(&list->as.list_lit.items, &list->as.list_lit.count, &list->as.list_lit.cap, item);
    }

    parser_expect(p, TOK_RBRACKET, "expected ']' after list literal");
    if (p->has_error) {
        return NULL;
    }
    return parse_postfix_expr(p, list);
}

static Expr *parse_map_literal(Parser *p, Token open_tok) {
    Expr *map = new_expr(EX_MAP, open_tok.line);
    if (parser_check(p, TOK_RBRACE)) {
        parser_expect(p, TOK_RBRACE, "expected '}' after map literal");
        if (p->has_error) {
            return NULL;
        }
        return parse_postfix_expr(p, map);
    }

    Expr *first_key = parse_expr(p);
    if (p->has_error || first_key == NULL) {
        return NULL;
    }
    parser_expect(p, TOK_COLON, "expected ':' after map key");
    if (p->has_error) {
        return NULL;
    }
    Expr *first_value = parse_expr(p);
    if (p->has_error || first_value == NULL) {
        return NULL;
    }

    if (parser_match(p, TOK_FOR)) {
        Token var_tok = parser_expect(p, TOK_IDENT, "expected loop variable after 'for' in map comprehension");
        if (p->has_error) {
            return NULL;
        }
        parser_expect(p, TOK_IN, "expected 'in' after loop variable in map comprehension");
        if (p->has_error) {
            return NULL;
        }
        Expr *iterable = parse_expr(p);
        if (p->has_error || iterable == NULL) {
            return NULL;
        }

        Expr *cond = NULL;
        int has_cond = 0;
        if (parser_match(p, TOK_IF)) {
            cond = parse_expr(p);
            if (p->has_error || cond == NULL) {
                return NULL;
            }
            has_cond = 1;
        }

        parser_expect(p, TOK_RBRACE, "expected '}' after map comprehension");
        if (p->has_error) {
            return NULL;
        }

        Expr *comp = new_expr(EX_MAP_COMP, open_tok.line);
        comp->as.map_comp.key_expr = first_key;
        comp->as.map_comp.value_expr = first_value;
        comp->as.map_comp.var_name = xstrdup(var_tok.lexeme);
        comp->as.map_comp.iterable = iterable;
        comp->as.map_comp.cond = cond;
        comp->as.map_comp.has_cond = has_cond;
        return parse_postfix_expr(p, comp);
    }

    if (first_key->kind != EX_STRING) {
        parser_error(p, parser_prev(p), "expected string key in map literal");
        return NULL;
    }
    map_item_list_push(&map->as.map_lit.keys, &map->as.map_lit.values, &map->as.map_lit.count, &map->as.map_lit.cap,
                       first_key->as.string_val, first_value);

    while (parser_match(p, TOK_COMMA)) {
        Token key_tok = parser_expect(p, TOK_STRING, "expected string key in map literal");
        if (p->has_error) {
            return NULL;
        }
        parser_expect(p, TOK_COLON, "expected ':' after map key");
        if (p->has_error) {
            return NULL;
        }
        Expr *value = parse_expr(p);
        if (p->has_error || value == NULL) {
            return NULL;
        }
        map_item_list_push(&map->as.map_lit.keys, &map->as.map_lit.values, &map->as.map_lit.count,
                           &map->as.map_lit.cap, key_tok.lexeme, value);
    }

    parser_expect(p, TOK_RBRACE, "expected '}' after map literal");
    if (p->has_error) {
        return NULL;
    }
    return parse_postfix_expr(p, map);
}

static Expr *parse_primary(Parser *p) {
    if (parser_match(p, TOK_INT)) {
        Token tok = parser_prev(p);
        char *end_ptr = NULL;
        errno = 0;
        long long v = strtoll(tok.lexeme, &end_ptr, 10);
        if (errno != 0 || end_ptr == NULL || *end_ptr != '\0') {
            parser_error(p, tok, "invalid int literal '%s'", tok.lexeme);
            return NULL;
        }
        Expr *e = new_expr(EX_INT, tok.line);
        e->as.int_val = v;
        return parse_postfix_expr(p, e);
    }

    if (parser_match(p, TOK_FLOAT)) {
        Token tok = parser_prev(p);
        char *end_ptr = NULL;
        errno = 0;
        double v = strtod(tok.lexeme, &end_ptr);
        if (errno != 0 || end_ptr == NULL || *end_ptr != '\0') {
            parser_error(p, tok, "invalid float literal '%s'", tok.lexeme);
            return NULL;
        }
        Expr *e = new_expr(EX_FLOAT, tok.line);
        e->as.float_val = v;
        return parse_postfix_expr(p, e);
    }

    if (parser_match(p, TOK_STRING)) {
        Token tok = parser_prev(p);
        Expr *e = new_expr(EX_STRING, tok.line);
        e->as.string_val = xstrdup(tok.lexeme);
        return parse_postfix_expr(p, e);
    }

    if (parser_match(p, TOK_BOOL)) {
        Token tok = parser_prev(p);
        Expr *e = new_expr(EX_BOOL, tok.line);
        e->as.bool_val = strcmp(tok.lexeme, "true") == 0 || strcmp(tok.lexeme, "True") == 0;
        return parse_postfix_expr(p, e);
    }

    if (parser_match(p, TOK_LBRACKET)) {
        Token open = parser_prev(p);
        return parse_list_literal(p, open);
    }

    if (parser_match(p, TOK_LBRACE)) {
        Token open = parser_prev(p);
        return parse_map_literal(p, open);
    }

    if (parser_match(p, TOK_FUC)) {
        Token f_tok = parser_prev(p);
        return parse_lambda_expr(p, f_tok);
    }

    if (parser_match(p, TOK_IDENT)) {
        Token id = parser_prev(p);
        char *qualified = xstrdup(id.lexeme);
        while (parser_match(p, TOK_DOT)) {
            Token part = parser_expect(p, TOK_IDENT, "expected identifier after '.'");
            if (p->has_error) {
                free(qualified);
                return NULL;
            }
            qualified = join_qualified_name(qualified, part.lexeme);
        }

        Expr *base = new_expr(EX_IDENT, id.line);
        base->as.ident_name = qualified;
        return parse_postfix_expr(p, base);
    }

    if (parser_match(p, TOK_LPAREN)) {
        Expr *e = parse_expr(p);
        if (p->has_error || e == NULL) {
            return NULL;
        }
        parser_expect(p, TOK_RPAREN, "expected ')' after expression");
        if (p->has_error) {
            return NULL;
        }
        return parse_postfix_expr(p, e);
    }

    Token t = parser_peek(p);
    parser_error(p, t, "unexpected token '%s'", t.lexeme);
    return NULL;
}

static Expr *parse_unary(Parser *p) {
    if (parser_match(p, TOK_MINUS) || parser_match(p, TOK_NOT)) {
        Token op = parser_prev(p);
        Expr *right = parse_unary(p);
        if (p->has_error || right == NULL) {
            return NULL;
        }
        Expr *e = new_expr(EX_UNARY, op.line);
        e->as.unary.op = op.type;
        e->as.unary.expr = right;
        return e;
    }
    return parse_primary(p);
}

static Expr *parse_mul_div(Parser *p) {
    Expr *left = parse_unary(p);
    if (p->has_error || left == NULL) {
        return NULL;
    }

    while (parser_match(p, TOK_STAR) || parser_match(p, TOK_SLASH)) {
        Token op = parser_prev(p);
        Expr *right = parse_unary(p);
        if (p->has_error || right == NULL) {
            return NULL;
        }
        Expr *bin = new_expr(EX_BINARY, op.line);
        bin->as.binary.left = left;
        bin->as.binary.op = op.type;
        bin->as.binary.right = right;
        left = bin;
    }
    return left;
}

static Expr *parse_add_sub(Parser *p) {
    Expr *left = parse_mul_div(p);
    if (p->has_error || left == NULL) {
        return NULL;
    }

    while (parser_match(p, TOK_PLUS) || parser_match(p, TOK_MINUS)) {
        Token op = parser_prev(p);
        Expr *right = parse_mul_div(p);
        if (p->has_error || right == NULL) {
            return NULL;
        }
        Expr *bin = new_expr(EX_BINARY, op.line);
        bin->as.binary.left = left;
        bin->as.binary.op = op.type;
        bin->as.binary.right = right;
        left = bin;
    }
    return left;
}

static int is_comparison_op(TokenType tt) {
    return tt == TOK_EQ || tt == TOK_NEQ || tt == TOK_LT || tt == TOK_LTE || tt == TOK_GT || tt == TOK_GTE ||
           tt == TOK_IN;
}

static int is_not_in_op(Parser *p) {
    return parser_check(p, TOK_NOT) && parser_check_next(p, TOK_IN);
}

static Expr *parse_comparison(Parser *p) {
    Expr *left = parse_add_sub(p);
    if (p->has_error || left == NULL) {
        return NULL;
    }

    while (is_comparison_op(parser_peek(p).type) || is_not_in_op(p)) {
        Token op;
        if (is_not_in_op(p)) {
            op = parser_advance(p); // 'not'
            (void)parser_expect(p, TOK_IN, "expected 'in' after 'not'");
            if (p->has_error) {
                return NULL;
            }
            op.type = TOK_NOTIN;
            op.lexeme = "not in";
        } else {
            op = parser_advance(p);
        }

        Expr *right = parse_add_sub(p);
        if (p->has_error || right == NULL) {
            return NULL;
        }
        Expr *bin = new_expr(EX_BINARY, op.line);
        bin->as.binary.left = left;
        bin->as.binary.op = op.type;
        bin->as.binary.right = right;
        left = bin;
    }
    return left;
}

static Expr *parse_and_expr(Parser *p) {
    Expr *left = parse_comparison(p);
    if (p->has_error || left == NULL) {
        return NULL;
    }

    while (parser_match(p, TOK_AND)) {
        Token op = parser_prev(p);
        Expr *right = parse_comparison(p);
        if (p->has_error || right == NULL) {
            return NULL;
        }
        Expr *bin = new_expr(EX_BINARY, op.line);
        bin->as.binary.left = left;
        bin->as.binary.op = op.type;
        bin->as.binary.right = right;
        left = bin;
    }
    return left;
}

static Expr *parse_or_expr(Parser *p) {
    Expr *left = parse_and_expr(p);
    if (p->has_error || left == NULL) {
        return NULL;
    }

    while (parser_match(p, TOK_OR)) {
        Token op = parser_prev(p);
        Expr *right = parse_and_expr(p);
        if (p->has_error || right == NULL) {
            return NULL;
        }
        Expr *bin = new_expr(EX_BINARY, op.line);
        bin->as.binary.left = left;
        bin->as.binary.op = op.type;
        bin->as.binary.right = right;
        left = bin;
    }
    return left;
}

static Expr *parse_expr(Parser *p) {
    return parse_or_expr(p);
}
