static Stmt *parse_for_stmt(Parser *p) {
    Token for_tok = parser_prev(p);

    Token var_tok = parser_expect(p, TOK_IDENT, "expected loop variable after 'for'");
    if (p->has_error) {
        return NULL;
    }

    int has_second_var = 0;
    Token var2_tok = {0};
    if (parser_match(p, TOK_COMMA)) {
        var2_tok = parser_expect(p, TOK_IDENT, "expected second loop variable after ','");
        if (p->has_error) {
            return NULL;
        }
        has_second_var = 1;
    }

    parser_expect(p, TOK_IN, "expected 'in' after loop variable");
    if (p->has_error) {
        return NULL;
    }

    Stmt *s = new_stmt(ST_FOR, for_tok.line);
    s->as.for_stmt.var_name = xstrdup(var_tok.lexeme);
    s->as.for_stmt.has_second_var = has_second_var;
    if (has_second_var) {
        s->as.for_stmt.var_name2 = xstrdup(var2_tok.lexeme);
    }

    if (parser_check(p, TOK_IDENT) && parser_check_next(p, TOK_LPAREN) && strcmp(parser_peek(p).lexeme, "range") == 0) {
        Token range_tok = parser_advance(p);
        (void)range_tok;
        parser_expect(p, TOK_LPAREN, "expected '(' after range");
        if (p->has_error) {
            return NULL;
        }

        Expr *first = parse_expr(p);
        if (p->has_error || first == NULL) {
            return NULL;
        }

        Expr *start = NULL;
        Expr *end = NULL;
        Expr *step = NULL;

        if (parser_match(p, TOK_COMMA)) {
            start = first;
            end = parse_expr(p);
            if (p->has_error || end == NULL) {
                return NULL;
            }
            if (parser_match(p, TOK_COMMA)) {
                step = parse_expr(p);
                if (p->has_error || step == NULL) {
                    return NULL;
                }
            } else {
                step = new_expr(EX_INT, for_tok.line);
                step->as.int_val = 1;
            }
        } else {
            start = new_expr(EX_INT, for_tok.line);
            start->as.int_val = 0;
            end = first;
            step = new_expr(EX_INT, for_tok.line);
            step->as.int_val = 1;
        }

        parser_expect(p, TOK_RPAREN, "expected ')' after range arguments");
        if (p->has_error) {
            return NULL;
        }

        s->as.for_stmt.mode = FOR_MODE_RANGE;
        s->as.for_stmt.start = start;
        s->as.for_stmt.end = end;
        s->as.for_stmt.step = step;
        if (has_second_var) {
            parser_error(p, for_tok, "range loop supports one variable; got two");
            return NULL;
        }
    } else {
        Expr *iterable = parse_expr(p);
        if (p->has_error || iterable == NULL) {
            return NULL;
        }
        s->as.for_stmt.mode = FOR_MODE_EACH;
        s->as.for_stmt.iterable = iterable;
    }

    parser_expect(p, TOK_COLON, "expected ':' after for header");
    if (p->has_error) {
        return NULL;
    }
    parser_expect(p, TOK_NEWLINE, "expected newline after for loop");
    if (p->has_error) {
        return NULL;
    }
    parser_expect(p, TOK_INDENT, "expected indented for-loop body");
    if (p->has_error) {
        return NULL;
    }

    while (!parser_check(p, TOK_DEDENT) && !parser_is_at_end(p)) {
        parser_skip_newlines(p);
        if (parser_check(p, TOK_DEDENT)) {
            break;
        }
        Stmt *body_stmt = parse_stmt(p);
        if (p->has_error || body_stmt == NULL) {
            return NULL;
        }
        stmt_list_push(&s->as.for_stmt.body, &s->as.for_stmt.body_count, &s->as.for_stmt.body_cap, body_stmt);
    }

    parser_expect(p, TOK_DEDENT, "expected dedent after for-loop body");
    if (p->has_error) {
        return NULL;
    }

    return s;
}

static Stmt *parse_while_stmt(Parser *p) {
    Token while_tok = parser_prev(p);

    Expr *cond = parse_expr(p);
    if (p->has_error || cond == NULL) {
        return NULL;
    }

    parser_expect(p, TOK_DO, "expected 'do' after while condition");
    if (p->has_error) {
        return NULL;
    }
    parser_expect(p, TOK_COLON, "expected ':' after while do");
    if (p->has_error) {
        return NULL;
    }
    parser_expect(p, TOK_NEWLINE, "expected newline after while header");
    if (p->has_error) {
        return NULL;
    }
    parser_expect(p, TOK_INDENT, "expected indented while body");
    if (p->has_error) {
        return NULL;
    }

    Stmt *s = new_stmt(ST_WHILE, while_tok.line);
    s->as.while_stmt.cond = cond;

    while (!parser_check(p, TOK_DEDENT) && !parser_is_at_end(p)) {
        parser_skip_newlines(p);
        if (parser_check(p, TOK_DEDENT)) {
            break;
        }
        Stmt *body_stmt = parse_stmt(p);
        if (p->has_error || body_stmt == NULL) {
            return NULL;
        }
        stmt_list_push(&s->as.while_stmt.body, &s->as.while_stmt.body_count, &s->as.while_stmt.body_cap, body_stmt);
    }

    parser_expect(p, TOK_DEDENT, "expected dedent after while body");
    if (p->has_error) {
        return NULL;
    }

    return s;
}

static Stmt *parse_do_while_stmt(Parser *p) {
    Token do_tok = parser_prev(p);

    parser_expect(p, TOK_COLON, "expected ':' after do");
    if (p->has_error) {
        return NULL;
    }
    parser_expect(p, TOK_NEWLINE, "expected newline after do:");
    if (p->has_error) {
        return NULL;
    }
    parser_expect(p, TOK_INDENT, "expected indented do body");
    if (p->has_error) {
        return NULL;
    }

    Stmt *s = new_stmt(ST_DO_WHILE, do_tok.line);

    while (!parser_check(p, TOK_DEDENT) && !parser_is_at_end(p)) {
        parser_skip_newlines(p);
        if (parser_check(p, TOK_DEDENT)) {
            break;
        }
        Stmt *body_stmt = parse_stmt(p);
        if (p->has_error || body_stmt == NULL) {
            return NULL;
        }
        stmt_list_push(&s->as.do_while_stmt.body, &s->as.do_while_stmt.body_count, &s->as.do_while_stmt.body_cap,
                       body_stmt);
    }

    parser_expect(p, TOK_DEDENT, "expected dedent after do body");
    if (p->has_error) {
        return NULL;
    }

    parser_expect(p, TOK_WHILE, "expected 'while' after do block");
    if (p->has_error) {
        return NULL;
    }

    Expr *cond = parse_expr(p);
    if (p->has_error || cond == NULL) {
        return NULL;
    }
    s->as.do_while_stmt.cond = cond;

    parser_expect(p, TOK_NEWLINE, "expected newline after do-while condition");
    if (p->has_error) {
        return NULL;
    }

    return s;
}

static int parse_indented_block(Parser *p, Stmt ***out_body, int *out_count, int *out_cap, const char *context) {
    parser_expect(p, TOK_COLON, "expected ':'");
    if (p->has_error) {
        return 0;
    }
    parser_expect(p, TOK_NEWLINE, "expected newline");
    if (p->has_error) {
        return 0;
    }
    parser_expect(p, TOK_INDENT, "expected indented block");
    if (p->has_error) {
        return 0;
    }

    while (!parser_check(p, TOK_DEDENT) && !parser_is_at_end(p)) {
        parser_skip_newlines(p);
        if (parser_check(p, TOK_DEDENT)) {
            break;
        }
        Stmt *body_stmt = parse_stmt(p);
        if (p->has_error || body_stmt == NULL) {
            return 0;
        }
        stmt_list_push(out_body, out_count, out_cap, body_stmt);
    }

    parser_expect(p, TOK_DEDENT, context);
    if (p->has_error) {
        return 0;
    }

    return 1;
}

static Stmt *parse_if_stmt(Parser *p) {
    Token if_tok = parser_prev(p);
    Stmt *s = new_stmt(ST_IF, if_tok.line);

    IfBranch first;
    memset(&first, 0, sizeof(first));
    first.line = if_tok.line;
    first.cond = parse_expr(p);
    if (p->has_error || first.cond == NULL) {
        return NULL;
    }

    if (!parse_indented_block(p, &first.body, &first.body_count, &first.body_cap, "expected dedent after if body")) {
        return NULL;
    }
    if_branch_list_push(&s->as.if_stmt.branches, &s->as.if_stmt.branch_count, &s->as.if_stmt.branch_cap, first);

    while (parser_match(p, TOK_ELIF)) {
        Token elif_tok = parser_prev(p);
        IfBranch branch;
        memset(&branch, 0, sizeof(branch));
        branch.line = elif_tok.line;
        branch.cond = parse_expr(p);
        if (p->has_error || branch.cond == NULL) {
            return NULL;
        }
        if (!parse_indented_block(p, &branch.body, &branch.body_count, &branch.body_cap,
                                  "expected dedent after elif body")) {
            return NULL;
        }
        if_branch_list_push(&s->as.if_stmt.branches, &s->as.if_stmt.branch_count, &s->as.if_stmt.branch_cap, branch);
    }

    if (parser_match(p, TOK_ELSE)) {
        s->as.if_stmt.has_else = 1;
        if (!parse_indented_block(p, &s->as.if_stmt.else_body, &s->as.if_stmt.else_count, &s->as.if_stmt.else_cap,
                                  "expected dedent after else body")) {
            return NULL;
        }
    }

    return s;
}

static Stmt *parse_match_stmt(Parser *p) {
    Token match_tok = parser_prev(p);
    Stmt *s = new_stmt(ST_MATCH, match_tok.line);

    s->as.match_stmt.target = parse_expr(p);
    if (p->has_error || s->as.match_stmt.target == NULL) {
        return NULL;
    }

    parser_expect(p, TOK_COLON, "expected ':' after match expression");
    if (p->has_error) {
        return NULL;
    }
    parser_expect(p, TOK_NEWLINE, "expected newline after match header");
    if (p->has_error) {
        return NULL;
    }
    parser_expect(p, TOK_INDENT, "expected indented match block");
    if (p->has_error) {
        return NULL;
    }

    int saw_default = 0;
    while (!parser_check(p, TOK_DEDENT) && !parser_is_at_end(p)) {
        parser_skip_newlines(p);
        if (parser_check(p, TOK_DEDENT)) {
            break;
        }

        if (parser_match(p, TOK_CASE)) {
            if (saw_default) {
                parser_error(p, parser_prev(p), "default case must be last in match block");
                return NULL;
            }
            MatchCase mc;
            memset(&mc, 0, sizeof(mc));
            mc.line = parser_prev(p).line;
            mc.pattern = parse_expr(p);
            if (p->has_error || mc.pattern == NULL) {
                return NULL;
            }
            if (!parse_indented_block(p, &mc.body, &mc.body_count, &mc.body_cap, "expected dedent after case body")) {
                return NULL;
            }
            match_case_list_push(&s->as.match_stmt.cases, &s->as.match_stmt.case_count, &s->as.match_stmt.case_cap, mc);
            continue;
        }

        if (parser_match(p, TOK_DEFAULT)) {
            if (saw_default) {
                parser_error(p, parser_prev(p), "duplicate default case in match block");
                return NULL;
            }
            saw_default = 1;
            s->as.match_stmt.has_default = 1;
            if (!parse_indented_block(p, &s->as.match_stmt.default_body, &s->as.match_stmt.default_count,
                                      &s->as.match_stmt.default_cap, "expected dedent after default body")) {
                return NULL;
            }
            continue;
        }

        parser_error(p, parser_peek(p), "expected 'case' or 'default' in match block");
        return NULL;
    }

    parser_expect(p, TOK_DEDENT, "expected dedent after match block");
    if (p->has_error) {
        return NULL;
    }
    if (s->as.match_stmt.case_count == 0) {
        parser_error(p, match_tok, "match block must include at least one case");
        return NULL;
    }
    return s;
}

static Stmt *parse_select_stmt(Parser *p) {
    Token select_tok = parser_prev(p);
    Stmt *s = new_stmt(ST_SELECT, select_tok.line);

    parser_expect(p, TOK_COLON, "expected ':' after select");
    if (p->has_error) {
        return NULL;
    }
    parser_expect(p, TOK_NEWLINE, "expected newline after select");
    if (p->has_error) {
        return NULL;
    }
    parser_expect(p, TOK_INDENT, "expected indented select block");
    if (p->has_error) {
        return NULL;
    }

    int saw_default = 0;
    while (!parser_check(p, TOK_DEDENT) && !parser_is_at_end(p)) {
        parser_skip_newlines(p);
        if (parser_check(p, TOK_DEDENT)) {
            break;
        }

        if (parser_match(p, TOK_CASE)) {
            if (saw_default) {
                parser_error(p, parser_prev(p), "default case must be last in select block");
                return NULL;
            }
            SelectCase sc;
            memset(&sc, 0, sizeof(sc));
            sc.line = parser_prev(p).line;

            Expr *op = parse_expr(p);
            if (p->has_error || op == NULL) {
                return NULL;
            }
            if (op->kind != EX_CALL || op->as.call.callee == NULL || op->as.call.callee->kind != EX_IDENT) {
                parser_error(p, parser_prev(p), "select case expects recv(...) or send(...)");
                return NULL;
            }

            const char *op_name = op->as.call.callee->as.ident_name;
            if (strcmp(op_name, "recv") == 0) {
                sc.kind = SELECT_CASE_RECV;
                if (parser_match(p, TOK_AS)) {
                    Token name_tok = parser_expect(p, TOK_IDENT, "expected variable name after 'as'");
                    if (p->has_error) {
                        return NULL;
                    }
                    sc.bind_name = xstrdup(name_tok.lexeme);
                    sc.has_bind = 1;
                }
            } else if (strcmp(op_name, "send") == 0) {
                sc.kind = SELECT_CASE_SEND;
                if (parser_match(p, TOK_AS)) {
                    parser_error(p, parser_prev(p), "send case does not support 'as' binding");
                    return NULL;
                }
            } else if (strcmp(op_name, "timeout") == 0) {
                sc.kind = SELECT_CASE_TIMEOUT;
                if (parser_match(p, TOK_AS)) {
                    parser_error(p, parser_prev(p), "timeout case does not support 'as' binding");
                    return NULL;
                }
            } else {
                parser_error(p, parser_prev(p), "select case expects recv(...), send(...), or timeout(...)");
                return NULL;
            }
            sc.op_call = op;

            if (!parse_indented_block(p, &sc.body, &sc.body_count, &sc.body_cap, "expected dedent after select case body")) {
                return NULL;
            }
            select_case_list_push(&s->as.select_stmt.cases, &s->as.select_stmt.case_count, &s->as.select_stmt.case_cap, sc);
            continue;
        }

        if (parser_match(p, TOK_DEFAULT)) {
            if (saw_default) {
                parser_error(p, parser_prev(p), "duplicate default case in select block");
                return NULL;
            }
            saw_default = 1;
            s->as.select_stmt.has_default = 1;
            if (!parse_indented_block(p, &s->as.select_stmt.default_body, &s->as.select_stmt.default_count,
                                      &s->as.select_stmt.default_cap, "expected dedent after select default body")) {
                return NULL;
            }
            continue;
        }

        parser_error(p, parser_peek(p), "expected 'case' or 'default' in select block");
        return NULL;
    }

    parser_expect(p, TOK_DEDENT, "expected dedent after select block");
    if (p->has_error) {
        return NULL;
    }
    if (s->as.select_stmt.case_count == 0) {
        parser_error(p, select_tok, "select block must include at least one case");
        return NULL;
    }
    return s;
}

static int is_index_assign_start(Parser *p) {
    if (!parser_check(p, TOK_IDENT) || !parser_check_next(p, TOK_LBRACKET)) {
        return 0;
    }

    int i = p->pos + 1;
    int depth = 0;
    while (i < p->count) {
        TokenType tt = p->tokens[i].type;
        if (tt == TOK_LBRACKET) {
            depth++;
        } else if (tt == TOK_RBRACKET) {
            depth--;
            if (depth == 0) {
                break;
            }
        } else if ((tt == TOK_NEWLINE || tt == TOK_EOF) && depth > 0) {
            return 0;
        }
        i++;
    }

    if (i >= p->count || depth != 0) {
        return 0;
    }
    if (i + 1 >= p->count) {
        return 0;
    }
    return p->tokens[i + 1].type == TOK_ASSIGN;
}

static int is_multi_assign_start(Parser *p) {
    if (!parser_check(p, TOK_IDENT) || !parser_check_next(p, TOK_COMMA)) {
        return 0;
    }

    int i = p->pos;
    int name_count = 0;
    while (i < p->count && p->tokens[i].type == TOK_IDENT) {
        name_count++;
        i++;
        if (i >= p->count || p->tokens[i].type != TOK_COMMA) {
            break;
        }
        i++;
        if (i >= p->count || p->tokens[i].type != TOK_IDENT) {
            return 0;
        }
    }

    if (name_count < 2 || i >= p->count) {
        return 0;
    }
    return p->tokens[i].type == TOK_ASSIGN;
}

static int is_labeled_loop_start(Parser *p) {
    if (!parser_check(p, TOK_IDENT) || !parser_check_next(p, TOK_COLON)) {
        return 0;
    }
    return parser_check_next2(p, TOK_FOR) || parser_check_next2(p, TOK_WHILE) || parser_check_next2(p, TOK_DO);
}

static Stmt *parse_stmt(Parser *p) {
    if (is_labeled_loop_start(p)) {
        Token label_tok = parser_expect(p, TOK_IDENT, "expected loop label");
        if (p->has_error) {
            return NULL;
        }
        parser_expect(p, TOK_COLON, "expected ':' after loop label");
        if (p->has_error) {
            return NULL;
        }

        if (parser_match(p, TOK_FOR)) {
            Stmt *s = parse_for_stmt(p);
            if (p->has_error || s == NULL) {
                return NULL;
            }
            s->as.for_stmt.loop_label = xstrdup(label_tok.lexeme);
            return s;
        }
        if (parser_match(p, TOK_WHILE)) {
            Stmt *s = parse_while_stmt(p);
            if (p->has_error || s == NULL) {
                return NULL;
            }
            s->as.while_stmt.loop_label = xstrdup(label_tok.lexeme);
            return s;
        }
        if (parser_match(p, TOK_DO)) {
            Stmt *s = parse_do_while_stmt(p);
            if (p->has_error || s == NULL) {
                return NULL;
            }
            s->as.do_while_stmt.loop_label = xstrdup(label_tok.lexeme);
            return s;
        }

        parser_error(p, label_tok, "loop label must target for/while/do loop");
        return NULL;
    }

    if (parser_match(p, TOK_MATCH)) {
        return parse_match_stmt(p);
    }

    if (parser_match(p, TOK_SELECT)) {
        return parse_select_stmt(p);
    }

    if (parser_match(p, TOK_IF)) {
        return parse_if_stmt(p);
    }

    if (parser_match(p, TOK_WHILE)) {
        return parse_while_stmt(p);
    }

    if (parser_match(p, TOK_DO)) {
        return parse_do_while_stmt(p);
    }

    if (parser_match(p, TOK_FOR)) {
        return parse_for_stmt(p);
    }

    if (parser_match(p, TOK_BREAK)) {
        Token br = parser_prev(p);
        Stmt *s = new_stmt(ST_BREAK, br.line);
        if (parser_check(p, TOK_IDENT)) {
            Token label_tok = parser_advance(p);
            s->as.break_stmt.has_label = 1;
            s->as.break_stmt.label = xstrdup(label_tok.lexeme);
        }
        parser_expect(p, TOK_NEWLINE, "expected newline after break");
        if (p->has_error) {
            return NULL;
        }
        return s;
    }

    if (parser_match(p, TOK_CONTINUE)) {
        Token ct = parser_prev(p);
        Stmt *s = new_stmt(ST_CONTINUE, ct.line);
        if (parser_check(p, TOK_IDENT)) {
            Token label_tok = parser_advance(p);
            s->as.continue_stmt.has_label = 1;
            s->as.continue_stmt.label = xstrdup(label_tok.lexeme);
        }
        parser_expect(p, TOK_NEWLINE, "expected newline after continue");
        if (p->has_error) {
            return NULL;
        }
        return s;
    }

    if (parser_match(p, TOK_DEFER)) {
        Token df = parser_prev(p);
        Expr *call_expr = parse_expr(p);
        if (p->has_error || call_expr == NULL) {
            return NULL;
        }
        if (call_expr->kind != EX_CALL) {
            parser_error(p, df, "defer expects function call");
            return NULL;
        }
        parser_expect(p, TOK_NEWLINE, "expected newline after defer");
        if (p->has_error) {
            return NULL;
        }
        Stmt *s = new_stmt(ST_DEFER, df.line);
        s->as.defer_stmt.expr = call_expr;
        return s;
    }

    if (parser_match(p, TOK_KOSTROUTINE)) {
        Token kt = parser_prev(p);
        Expr *call_expr = parse_expr(p);
        if (p->has_error || call_expr == NULL) {
            return NULL;
        }
        if (call_expr->kind != EX_CALL || call_expr->as.call.callee == NULL || call_expr->as.call.callee->kind != EX_IDENT) {
            parser_error(p, kt, "kostroutine expects function call by name reference");
            return NULL;
        }
        parser_expect(p, TOK_NEWLINE, "expected newline after kostroutine");
        if (p->has_error) {
            return NULL;
        }
        Stmt *s = new_stmt(ST_KOSTROUTINE, kt.line);
        s->as.kostroutine_stmt.expr = call_expr;
        return s;
    }

    if (parser_match(p, TOK_CONST)) {
        Token name = parser_expect(p, TOK_IDENT, "expected variable name after imut");
        if (p->has_error) {
            return NULL;
        }
        parser_expect(p, TOK_ASSIGN, "expected '=' after imut variable name");
        if (p->has_error) {
            return NULL;
        }
        Expr *rhs = parse_expr(p);
        if (p->has_error || rhs == NULL) {
            return NULL;
        }
        parser_expect(p, TOK_NEWLINE, "expected newline after imut assignment");
        if (p->has_error) {
            return NULL;
        }
        Stmt *s = new_stmt(ST_CONST, name.line);
        s->as.const_assign.name = xstrdup(name.lexeme);
        s->as.const_assign.expr = rhs;
        return s;
    }

    if (is_multi_assign_start(p)) {
        Token first = parser_expect(p, TOK_IDENT, "expected variable name in multi-assignment");
        if (p->has_error) {
            return NULL;
        }

        Stmt *s = new_stmt(ST_MULTI_ASSIGN, first.line);
        export_list_push(&s->as.multi_assign.names, &s->as.multi_assign.name_count, &s->as.multi_assign.name_cap,
                         xstrdup(first.lexeme));

        while (parser_match(p, TOK_COMMA)) {
            Token name_tok = parser_expect(p, TOK_IDENT, "expected variable name after ',' in multi-assignment");
            if (p->has_error) {
                return NULL;
            }
            export_list_push(&s->as.multi_assign.names, &s->as.multi_assign.name_count, &s->as.multi_assign.name_cap,
                             xstrdup(name_tok.lexeme));
        }

        parser_expect(p, TOK_ASSIGN, "expected '=' in multi-assignment");
        if (p->has_error) {
            return NULL;
        }

        Expr *rhs = parse_expr(p);
        if (p->has_error || rhs == NULL) {
            return NULL;
        }
        s->as.multi_assign.expr = rhs;

        parser_expect(p, TOK_NEWLINE, "expected newline after multi-assignment");
        if (p->has_error) {
            return NULL;
        }
        return s;
    }

    if (parser_check(p, TOK_IDENT) && parser_check_next(p, TOK_PLUSPLUS) && parser_check_next2(p, TOK_NEWLINE)) {
        Token id = parser_advance(p);
        parser_advance(p);
        parser_expect(p, TOK_NEWLINE, "expected newline after increment");
        if (p->has_error) {
            return NULL;
        }

        Stmt *s = new_stmt(ST_INC, id.line);
        s->as.inc.name = xstrdup(id.lexeme);
        return s;
    }

    if (parser_check(p, TOK_IDENT) && parser_check_next(p, TOK_DOT) && parser_check_next2(p, TOK_IDENT) &&
        parser_check_next3(p, TOK_ASSIGN)) {
        Token obj = parser_advance(p);
        parser_expect(p, TOK_DOT, "expected '.' after object name");
        if (p->has_error) {
            return NULL;
        }
        Token field = parser_expect(p, TOK_IDENT, "expected field name after '.'");
        if (p->has_error) {
            return NULL;
        }
        parser_expect(p, TOK_ASSIGN, "expected '=' after field target");
        if (p->has_error) {
            return NULL;
        }

        Expr *rhs = parse_expr(p);
        if (p->has_error || rhs == NULL) {
            return NULL;
        }
        parser_expect(p, TOK_NEWLINE, "expected newline after assignment");
        if (p->has_error) {
            return NULL;
        }

        Stmt *s = new_stmt(ST_FIELD_ASSIGN, obj.line);
        s->as.field_assign.obj_name = xstrdup(obj.lexeme);
        s->as.field_assign.field_name = xstrdup(field.lexeme);
        s->as.field_assign.expr = rhs;
        return s;
    }

    if (is_index_assign_start(p)) {
        Token id = parser_expect(p, TOK_IDENT, "expected identifier before '['");
        if (p->has_error) {
            return NULL;
        }
        parser_expect(p, TOK_LBRACKET, "expected '[' after identifier");
        if (p->has_error) {
            return NULL;
        }

        Expr *index_expr = parse_expr(p);
        if (p->has_error || index_expr == NULL) {
            return NULL;
        }

        parser_expect(p, TOK_RBRACKET, "expected ']' after index expression");
        if (p->has_error) {
            return NULL;
        }
        parser_expect(p, TOK_ASSIGN, "expected '=' after index target");
        if (p->has_error) {
            return NULL;
        }

        Expr *rhs = parse_expr(p);
        if (p->has_error || rhs == NULL) {
            return NULL;
        }
        parser_expect(p, TOK_NEWLINE, "expected newline after assignment");
        if (p->has_error) {
            return NULL;
        }

        Stmt *s = new_stmt(ST_INDEX_ASSIGN, id.line);
        s->as.index_assign.name = xstrdup(id.lexeme);
        s->as.index_assign.index = index_expr;
        s->as.index_assign.expr = rhs;
        return s;
    }

    if (parser_match(p, TOK_RETURN)) {
        Token rt = parser_prev(p);
        Stmt *s = new_stmt(ST_RETURN, rt.line);

        if (parser_check(p, TOK_NEWLINE)) {
            parser_advance(p);
            return s;
        }

        while (1) {
            Expr *expr = parse_expr(p);
            if (p->has_error || expr == NULL) {
                return NULL;
            }
            expr_list_push(&s->as.ret.values, &s->as.ret.value_count, &s->as.ret.value_cap, expr);
            if (!parser_match(p, TOK_COMMA)) {
                break;
            }
        }
        parser_expect(p, TOK_NEWLINE, "expected newline after return statement");
        if (p->has_error) {
            return NULL;
        }
        return s;
    }

    if (parser_check(p, TOK_IDENT) && parser_check_next(p, TOK_ASSIGN)) {
        Token id = parser_advance(p);
        parser_advance(p);

        Expr *expr = parse_expr(p);
        if (p->has_error || expr == NULL) {
            return NULL;
        }

        parser_expect(p, TOK_NEWLINE, "expected newline after assignment");
        if (p->has_error) {
            return NULL;
        }

        Stmt *s = new_stmt(ST_ASSIGN, id.line);
        s->as.assign.name = xstrdup(id.lexeme);
        s->as.assign.expr = expr;
        return s;
    }

    Expr *expr = parse_expr(p);
    if (p->has_error || expr == NULL) {
        return NULL;
    }
    parser_expect(p, TOK_NEWLINE, "expected newline after expression");
    if (p->has_error) {
        return NULL;
    }

    Stmt *s = new_stmt(ST_EXPR, expr->line);
    s->as.expr_stmt.expr = expr;
    return s;
}
