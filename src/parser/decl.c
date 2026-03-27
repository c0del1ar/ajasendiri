static TypeDecl *parse_type_decl(Parser *p) {
    Token type_tok = parser_prev(p);
    Token name_tok = parser_expect(p, TOK_IDENT, "expected type name after 'type'");
    if (p->has_error) {
        return NULL;
    }
    parser_expect(p, TOK_COLON, "expected ':' after type name");
    if (p->has_error) {
        return NULL;
    }
    parser_expect(p, TOK_NEWLINE, "expected newline after type declaration");
    if (p->has_error) {
        return NULL;
    }
    parser_expect(p, TOK_INDENT, "expected indented type body");
    if (p->has_error) {
        return NULL;
    }

    TypeDecl *decl = (TypeDecl *)calloc(1, sizeof(TypeDecl));
    if (!decl) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    decl->name = xstrdup(name_tok.lexeme);
    decl->line = type_tok.line;

    while (!parser_check(p, TOK_DEDENT) && !parser_is_at_end(p)) {
        parser_skip_newlines(p);
        if (parser_check(p, TOK_DEDENT)) {
            break;
        }

        Token field_name = parser_expect(p, TOK_IDENT, "expected field name in type declaration");
        if (p->has_error) {
            return NULL;
        }
        parser_expect(p, TOK_COLON, "expected ':' after field name");
        if (p->has_error) {
            return NULL;
        }
        TypeRef parsed = parse_type_ref(p);
        if (p->has_error) {
            return NULL;
        }
        if (parsed.kind == VT_VOID || parsed.kind == VT_INVALID) {
            parser_error(p, parser_prev(p), "field '%s' cannot use void/invalid type", field_name.lexeme);
            return NULL;
        }
        parser_expect(p, TOK_NEWLINE, "expected newline after field declaration");
        if (p->has_error) {
            return NULL;
        }

        FieldDecl field;
        field.name = xstrdup(field_name.lexeme);
        field.type = parsed;
        field_list_push(&decl->fields, &decl->field_count, &decl->field_cap, field);
    }

    parser_expect(p, TOK_DEDENT, "expected dedent after type body");
    if (p->has_error) {
        return NULL;
    }
    if (decl->field_count == 0) {
        parser_error(p, name_tok, "type '%s' must declare at least one field", name_tok.lexeme);
        return NULL;
    }
    return decl;
}

static InterfaceDecl *parse_interface_decl(Parser *p) {
    Token iface_tok = parser_prev(p);
    Token name_tok = parser_expect(p, TOK_IDENT, "expected interface name after 'interface'");
    if (p->has_error) {
        return NULL;
    }
    parser_expect(p, TOK_COLON, "expected ':' after interface name");
    if (p->has_error) {
        return NULL;
    }
    parser_expect(p, TOK_NEWLINE, "expected newline after interface declaration");
    if (p->has_error) {
        return NULL;
    }
    parser_expect(p, TOK_INDENT, "expected indented interface body");
    if (p->has_error) {
        return NULL;
    }

    InterfaceDecl *decl = (InterfaceDecl *)calloc(1, sizeof(InterfaceDecl));
    if (!decl) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    decl->name = xstrdup(name_tok.lexeme);
    decl->line = iface_tok.line;

    while (!parser_check(p, TOK_DEDENT) && !parser_is_at_end(p)) {
        parser_skip_newlines(p);
        if (parser_check(p, TOK_DEDENT)) {
            break;
        }

        Token method_name = parser_expect(p, TOK_IDENT, "expected method signature in interface");
        if (p->has_error) {
            return NULL;
        }
        for (int i = 0; i < decl->method_count; i++) {
            if (strcmp(decl->methods[i].name, method_name.lexeme) == 0) {
                parser_error(p, method_name, "duplicate method '%s' in interface '%s'", method_name.lexeme,
                             decl->name);
                return NULL;
            }
        }

        InterfaceMethodSig method;
        memset(&method, 0, sizeof(method));
        method.name = xstrdup(method_name.lexeme);
        method.return_type = make_type_ref(VT_VOID, NULL);
        method.line = method_name.line;

        parser_expect(p, TOK_LPAREN, "expected '(' after interface method name");
        if (p->has_error) {
            return NULL;
        }
        if (!parser_check(p, TOK_RPAREN)) {
            while (1) {
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
                Param param;
                param.name = xstrdup(pname.lexeme);
                param.type = ptype;
                param.default_expr = NULL;
                param.is_kw_only = 0;
                param.line = pname.line;
                param_list_push(&method.params, &method.param_count, &method.param_cap, param);

                if (!parser_match(p, TOK_COMMA)) {
                    break;
                }
            }
        }
        parser_expect(p, TOK_RPAREN, "expected ')' after parameter list");
        if (p->has_error) {
            return NULL;
        }
        if (parser_match(p, TOK_ARROW)) {
            method.return_type = parse_return_type_ref(p);
            if (p->has_error) {
                return NULL;
            }
        }
        parser_expect(p, TOK_NEWLINE, "expected newline after interface method signature");
        if (p->has_error) {
            return NULL;
        }

        interface_method_list_push(&decl->methods, &decl->method_count, &decl->method_cap, method);
    }

    parser_expect(p, TOK_DEDENT, "expected dedent after interface body");
    if (p->has_error) {
        return NULL;
    }
    if (decl->method_count == 0) {
        parser_error(p, name_tok, "interface '%s' must declare at least one method", name_tok.lexeme);
        return NULL;
    }
    return decl;
}

static int parse_import_decl(Parser *p, Program *prog) {
    parser_expect(p, TOK_LPAREN, "expected '(' after import");
    if (p->has_error) {
        return 0;
    }

    int imported_entries = 0;
    int last_entry_line = -1;
    parser_skip_layout(p);
    while (!parser_check(p, TOK_RPAREN) && !parser_is_at_end(p)) {
        parser_skip_layout(p);
        if (parser_check(p, TOK_RPAREN)) {
            break;
        }

        ImportDecl decl;
        int entry_line = -1;
        memset(&decl, 0, sizeof(decl));

        if (parser_match(p, TOK_STRING)) {
            Token mod = parser_prev(p);
            decl.mode = IMPORT_ALL;
            decl.module = xstrdup(mod.lexeme);
            if (parser_match(p, TOK_AS)) {
                Token alias_tok = parser_expect(p, TOK_IDENT, "expected alias name after 'as'");
                if (p->has_error) {
                    return 0;
                }
                decl.alias = xstrdup(alias_tok.lexeme);
            }
            import_list_push(&prog->imports, &prog->import_count, &prog->import_cap, decl);
            imported_entries++;
            entry_line = mod.line;
        } else if (parser_match(p, TOK_LBRACE)) {
            Token list_open = parser_prev(p);
            decl.mode = IMPORT_SELECTIVE;
            while (!parser_check(p, TOK_RBRACE) && !parser_is_at_end(p)) {
                parser_skip_layout(p);
                if (parser_check(p, TOK_RBRACE)) {
                    break;
                }
                Token name = parser_expect(p, TOK_IDENT, "expected identifier in import list");
                if (p->has_error) {
                    return 0;
                }
                export_list_push(&decl.names, &decl.name_count, &decl.name_cap, xstrdup(name.lexeme));
                if (parser_match(p, TOK_COMMA)) {
                    parser_error(p, parser_prev(p),
                                 "commas are not allowed in import lists; put each entry on its own line");
                    return 0;
                }
                parser_skip_layout(p);
            }

            parser_expect(p, TOK_RBRACE, "expected '}' in selective import");
            if (p->has_error) {
                return 0;
            }
            if (decl.name_count == 0) {
                parser_error(p, parser_prev(p), "selective import list cannot be empty");
                return 0;
            }
            parser_expect(p, TOK_FROM, "expected 'from' after selective import list");
            if (p->has_error) {
                return 0;
            }
            Token mod = parser_expect(p, TOK_STRING, "expected module string after from");
            if (p->has_error) {
                return 0;
            }
            decl.module = xstrdup(mod.lexeme);
            if (parser_match(p, TOK_AS)) {
                parser_error(p, parser_prev(p), "alias is only allowed for import-all entries");
                return 0;
            }
            import_list_push(&prog->imports, &prog->import_count, &prog->import_cap, decl);
            imported_entries++;
            entry_line = list_open.line;
        } else {
            parser_error(p, parser_peek(p), "invalid import entry");
            return 0;
        }

        if (imported_entries > 1 && entry_line == last_entry_line) {
            parser_error(p, parser_prev(p), "multiple import entries on one line are not allowed");
            return 0;
        }
        last_entry_line = entry_line;

        if (parser_match(p, TOK_COMMA)) {
            parser_error(p, parser_prev(p),
                         "commas are not allowed in import lists; put each module on its own line");
            return 0;
        }
        parser_skip_layout(p);
    }

    parser_expect(p, TOK_RPAREN, "expected ')' after import block");
    if (p->has_error) {
        return 0;
    }
    if (imported_entries == 0) {
        parser_error(p, parser_prev(p), "import list cannot be empty");
        return 0;
    }
    parser_match(p, TOK_NEWLINE);
    return 1;
}

static int parse_export_decl(Parser *p, Program *prog) {
    parser_expect(p, TOK_LPAREN, "expected '(' after export");
    if (p->has_error) {
        return 0;
    }

    int exported_entries = 0;
    parser_skip_layout(p);
    while (!parser_check(p, TOK_RPAREN) && !parser_is_at_end(p)) {
        parser_skip_layout(p);
        if (parser_check(p, TOK_RPAREN)) {
            break;
        }
        Token name = parser_expect(p, TOK_IDENT, "expected identifier in export list");
        if (p->has_error) {
            return 0;
        }
        export_list_push(&prog->exports, &prog->export_count, &prog->export_cap, xstrdup(name.lexeme));
        exported_entries++;
        parser_match(p, TOK_COMMA);
        parser_skip_layout(p);
    }

    parser_expect(p, TOK_RPAREN, "expected ')' after export block");
    if (p->has_error) {
        return 0;
    }
    if (exported_entries == 0) {
        parser_error(p, parser_prev(p), "export list cannot be empty");
        return 0;
    }
    parser_match(p, TOK_NEWLINE);
    return 1;
}

static FuncDecl *parse_func_decl(Parser *p) {
    Token f_tok = parser_prev(p);

    FuncDecl *fn = new_func();
    fn->line = f_tok.line;
    fn->return_type = make_type_ref(VT_VOID, NULL);

    Token name_tok = {0};
    if (parser_match(p, TOK_LPAREN)) {
        Token recv_name = parser_expect(p, TOK_IDENT, "expected receiver name");
        if (p->has_error) {
            return NULL;
        }
        parser_expect(p, TOK_COLON, "expected ':' after receiver name");
        if (p->has_error) {
            return NULL;
        }
        Token recv_type_tok = parser_expect(p, TOK_IDENT, "expected receiver type");
        if (p->has_error) {
            return NULL;
        }
        TypeRef recv_type = parse_type_token(p, recv_type_tok);
        if (recv_type.kind != VT_OBJECT) {
            parser_error(p, recv_type_tok, "receiver type must be a custom type");
            return NULL;
        }
        parser_expect(p, TOK_RPAREN, "expected ')' after receiver");
        if (p->has_error) {
            return NULL;
        }
        name_tok = parser_expect(p, TOK_IDENT, "expected function name after receiver");
        if (p->has_error) {
            return NULL;
        }
        fn->has_receiver = 1;
        fn->receiver.name = xstrdup(recv_name.lexeme);
        fn->receiver.type = recv_type;
        fn->receiver.line = recv_name.line;
    } else {
        name_tok = parser_expect(p, TOK_IDENT, "expected function name after 'fuc'");
        if (p->has_error) {
            return NULL;
        }
    }
    fn->name = xstrdup(name_tok.lexeme);

    parser_expect(p, TOK_LPAREN, "expected '(' after function name");
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

    parser_expect(p, TOK_RPAREN, "expected ')' after parameter list");
    if (p->has_error) {
        return NULL;
    }

    if (parser_match(p, TOK_ARROW)) {
        fn->return_type = parse_return_type_ref(p);
        if (p->has_error) {
            return NULL;
        }
    }

    parser_expect(p, TOK_COLON, "expected ':' after function signature");
    if (p->has_error) {
        return NULL;
    }

    parser_expect(p, TOK_NEWLINE, "expected newline after function signature");
    if (p->has_error) {
        return NULL;
    }

    parser_expect(p, TOK_INDENT, "expected indented function body");
    if (p->has_error) {
        return NULL;
    }

    while (!parser_check(p, TOK_DEDENT) && !parser_is_at_end(p)) {
        parser_skip_newlines(p);
        if (parser_check(p, TOK_DEDENT)) {
            break;
        }

        Stmt *stmt = parse_stmt(p);
        if (p->has_error || stmt == NULL) {
            return NULL;
        }
        stmt_list_push(&fn->body, &fn->body_count, &fn->body_cap, stmt);
    }

    parser_expect(p, TOK_DEDENT, "expected dedent after function body");
    if (p->has_error) {
        return NULL;
    }

    return fn;
}

Program *parse_program(TokenArray *tokens, char *err, size_t err_cap) {
    Parser p;
    p.tokens = tokens->items;
    p.count = tokens->count;
    p.pos = 0;
    p.has_error = 0;
    p.err[0] = '\0';

    Program *prog = (Program *)calloc(1, sizeof(Program));
    if (!prog) {
        snprintf(err, err_cap, "out of memory");
        return NULL;
    }

    while (!parser_is_at_end(&p)) {
        parser_skip_newlines(&p);
        if (parser_is_at_end(&p)) {
            break;
        }

        if (parser_match(&p, TOK_IMPORT)) {
            if (!parse_import_decl(&p, prog)) {
                snprintf(err, err_cap, "%s", p.err);
                return NULL;
            }
            continue;
        }

        if (parser_match(&p, TOK_EXPORT)) {
            if (!parse_export_decl(&p, prog)) {
                snprintf(err, err_cap, "%s", p.err);
                return NULL;
            }
            continue;
        }

        if (parser_match(&p, TOK_TYPE)) {
            TypeDecl *type_decl = parse_type_decl(&p);
            if (p.has_error || type_decl == NULL) {
                snprintf(err, err_cap, "%s", p.err);
                return NULL;
            }
            type_list_push(&prog->types, &prog->type_count, &prog->type_cap, type_decl);
            continue;
        }

        if (parser_match(&p, TOK_INTERFACE)) {
            InterfaceDecl *iface_decl = parse_interface_decl(&p);
            if (p.has_error || iface_decl == NULL) {
                snprintf(err, err_cap, "%s", p.err);
                return NULL;
            }
            interface_list_push(&prog->interfaces, &prog->interface_count, &prog->interface_cap, iface_decl);
            continue;
        }

        if (parser_match(&p, TOK_FUC)) {
            FuncDecl *fn = parse_func_decl(&p);
            if (p.has_error || fn == NULL) {
                snprintf(err, err_cap, "%s", p.err);
                return NULL;
            }
            func_list_push(&prog->funcs, &prog->func_count, &prog->func_cap, fn);
            continue;
        }

        Stmt *stmt = parse_stmt(&p);
        if (p.has_error || stmt == NULL) {
            snprintf(err, err_cap, "%s", p.err);
            return NULL;
        }
        stmt_list_push(&prog->stmts, &prog->stmt_count, &prog->stmt_cap, stmt);
    }

    return prog;
}
