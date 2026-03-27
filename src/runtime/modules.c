static int register_namespaced_exports(Runtime *rt, Module *m) {
    for (int i = 0; i < m->prog->export_count; i++) {
        const char *sym = m->prog->exports[i];
        size_t n1 = strlen(m->namespace), n2 = strlen(sym);
        char *full = (char *)malloc(n1 + n2 + 2);
        if (!full) {
            return 0;
        }
        memcpy(full, m->namespace, n1);
        full[n1] = '.';
        memcpy(full + n1 + 1, sym, n2);
        full[n1 + n2 + 1] = '\0';
        FuncDecl *fn = find_func_in_module(m, sym);
        if (fn) {
            if (!add_binding(rt, full, fn, &m->globals, m)) {
                free(full);
                return 0;
            }
            free(full);
            continue;
        }

        TypeDecl *type_decl = find_type_in_module(m, sym);
        if (type_decl) {
            if (!add_type_binding(rt, full, type_decl, m)) {
                free(full);
                return 0;
            }
            free(full);
            continue;
        }

        InterfaceDecl *iface_decl = find_interface_in_module(m, sym);
        if (iface_decl) {
            if (!add_interface_binding(rt, full, iface_decl, m)) {
                free(full);
                return 0;
            }
            free(full);
            continue;
        }

        free(full);
    }
    return 1;
}

static Program *new_native_program(const char **exports, int export_count) {
    Program *p = (Program *)calloc(1, sizeof(Program));
    if (!p) {
        return NULL;
    }
    p->exports = (char **)calloc((size_t)export_count, sizeof(char *));
    if (!p->exports) {
        free(p);
        return NULL;
    }
    for (int i = 0; i < export_count; i++) {
        p->exports[i] = xstrdup(exports[i]);
    }
    p->export_count = export_count;
    p->export_cap = export_count;
    return p;
}

static Module *try_load_native_module(Runtime *rt, const char *base_dir, const char *mod_name) {
    char *ns = module_namespace(mod_name);
    int kind = NATIVE_NONE;
    const char *math_exports[] = {"pi", "abs", "sqrt", "min", "max"};
    const char *time_exports[] = {"now_unix", "now_ms", "sleep"};
    const char *json_exports[] = {"encode", "decode"};
    const char *fs_exports[] = {"read", "write", "append", "exists", "mkdir", "remove"};
    const char *http_exports[] = {"get", "post", "put", "delete", "request", "requestEx"};
    const char *rand_exports[] = {"seed", "int", "float"};
    const char *os_exports[] = {"cwd", "chdir", "getenv", "setenv"};
    const char *path_exports[] = {"join", "basename", "dirname", "ext"};
    const char *re_exports[] = {"compilePattern", "freePattern", "isMatchHandle", "searchHandle", "replaceAllHandle",
                                "splitHandle"};
    const char **exports = NULL;
    int export_count = 0;

    if (strcmp(ns, "math") == 0) {
        kind = NATIVE_MATH;
        exports = math_exports;
        export_count = 5;
    } else if (strcmp(ns, "time") == 0) {
        kind = NATIVE_TIME;
        exports = time_exports;
        export_count = 3;
    } else if (strcmp(ns, "json") == 0) {
        kind = NATIVE_JSON;
        exports = json_exports;
        export_count = 2;
    } else if (strcmp(ns, "fs") == 0) {
        kind = NATIVE_FS;
        exports = fs_exports;
        export_count = 6;
    } else if (strcmp(ns, "http") == 0) {
        kind = NATIVE_HTTP;
        exports = http_exports;
        export_count = 6;
    } else if (strcmp(ns, "rand") == 0) {
        kind = NATIVE_RAND;
        exports = rand_exports;
        export_count = 3;
    } else if (strcmp(ns, "os") == 0) {
        kind = NATIVE_OS;
        exports = os_exports;
        export_count = 4;
    } else if (strcmp(ns, "path") == 0) {
        kind = NATIVE_PATH;
        exports = path_exports;
        export_count = 4;
    } else if (strcmp(ns, "recore") == 0) {
        kind = NATIVE_RE;
        exports = re_exports;
        export_count = 6;
    }
    if (kind == NATIVE_NONE) {
        free(ns);
        return NULL;
    }

    Program *prog = new_native_program(exports, export_count);
    if (!prog) {
        free(ns);
        runtime_error(rt, 0, "out of memory while preparing native module '%s'", mod_name);
        return NULL;
    }
    Module *m = add_module(rt, mod_name, ns, base_dir, prog);
    free(ns);
    if (!m) {
        runtime_error(rt, 0, "out of memory while loading module '%s'", mod_name);
        return NULL;
    }
    m->native_kind = kind;
    m->loaded = 1;

    if (kind == NATIVE_MATH) {
        char env_err[256];
        if (!env_set(&m->globals, "pi", value_float(3.141592653589793), env_err, sizeof(env_err))) {
            runtime_error(rt, 0, "%s", env_err);
            return NULL;
        }
    }

    return m;
}

static int native_module_is_function_export(Module *m, const char *name) {
    if (m->native_kind == NATIVE_MATH) {
        return strcmp(name, "abs") == 0 || strcmp(name, "sqrt") == 0 || strcmp(name, "min") == 0 ||
               strcmp(name, "max") == 0;
    }
    if (m->native_kind == NATIVE_TIME) {
        return strcmp(name, "now_unix") == 0 || strcmp(name, "now_ms") == 0 || strcmp(name, "sleep") == 0;
    }
    if (m->native_kind == NATIVE_JSON) {
        return strcmp(name, "encode") == 0 || strcmp(name, "decode") == 0;
    }
    if (m->native_kind == NATIVE_FS) {
        return strcmp(name, "read") == 0 || strcmp(name, "write") == 0 || strcmp(name, "append") == 0 ||
               strcmp(name, "exists") == 0 || strcmp(name, "mkdir") == 0 || strcmp(name, "remove") == 0;
    }
    if (m->native_kind == NATIVE_HTTP) {
        return strcmp(name, "get") == 0 || strcmp(name, "post") == 0 || strcmp(name, "put") == 0 ||
               strcmp(name, "delete") == 0 || strcmp(name, "request") == 0 || strcmp(name, "requestEx") == 0;
    }
    if (m->native_kind == NATIVE_RAND) {
        return strcmp(name, "seed") == 0 || strcmp(name, "int") == 0 || strcmp(name, "float") == 0;
    }
    if (m->native_kind == NATIVE_OS) {
        return strcmp(name, "cwd") == 0 || strcmp(name, "chdir") == 0 || strcmp(name, "getenv") == 0 ||
               strcmp(name, "setenv") == 0;
    }
    if (m->native_kind == NATIVE_PATH) {
        return strcmp(name, "join") == 0 || strcmp(name, "basename") == 0 || strcmp(name, "dirname") == 0 ||
               strcmp(name, "ext") == 0;
    }
    if (m->native_kind == NATIVE_RE) {
        return strcmp(name, "compilePattern") == 0 || strcmp(name, "freePattern") == 0 ||
               strcmp(name, "isMatchHandle") == 0 || strcmp(name, "searchHandle") == 0 ||
               strcmp(name, "replaceAllHandle") == 0 || strcmp(name, "splitHandle") == 0;
    }
    return 0;
}

static Module *load_module(Runtime *rt, const char *base_dir, const char *mod_name) {
    Module *existing = find_module_by_name(rt, mod_name);
    if (existing) {
        if (existing->loading) {
            runtime_error(rt, 0, "circular import detected for module '%s'", mod_name);
            return NULL;
        }
        if (!existing->loaded) {
            if (!execute_module(rt, existing)) {
                return NULL;
            }
        }
        return existing;
    }

    Module *native = try_load_native_module(rt, base_dir, mod_name);
    if (rt->has_error) {
        return NULL;
    }
    if (native) {
        return native;
    }

    char *path = module_path_from_name(base_dir, mod_name);
    char read_err[512];
    char *src = read_file(path, read_err, sizeof(read_err));
    if (!src) {
        runtime_error(rt, 0, "%s", read_err);
        free(path);
        return NULL;
    }

    TokenArray toks;
    token_array_init(&toks);
    if (!tokenize_source(src, &toks, read_err, sizeof(read_err))) {
        runtime_error(rt, 0, "lex error in module '%s': %s", mod_name, read_err);
        free(src);
        free(path);
        token_array_free(&toks);
        return NULL;
    }

    Program *prog = parse_program(&toks, read_err, sizeof(read_err));
    if (!prog) {
        runtime_error(rt, 0, "parse error in module '%s': %s", mod_name, read_err);
        free(src);
        free(path);
        token_array_free(&toks);
        return NULL;
    }

    char *dir = dirname_from_path(path);
    char *ns = module_namespace(mod_name);
    Module *m = add_module(rt, mod_name, ns, dir, prog);
    free(ns);
    free(dir);
    free(src);
    free(path);
    token_array_free(&toks);

    if (!m) {
        runtime_error(rt, 0, "out of memory while loading module '%s'", mod_name);
        return NULL;
    }

    if (!execute_module(rt, m)) {
        return NULL;
    }
    return m;
}

static int process_imports(Runtime *rt, Module *m) {
    for (int i = 0; i < m->prog->import_count; i++) {
        ImportDecl *decl = &m->prog->imports[i];
        Module *imported = load_module(rt, m->base_dir, decl->module);
        if (rt->has_error || !imported) {
            return 0;
        }

        if (decl->mode == IMPORT_ALL) {
            if (decl->alias && decl->alias[0] != '\0') {
                Module *conflict = find_module_by_namespace(rt, decl->alias);
                if (conflict && conflict != imported) {
                    runtime_error(rt, 0, "cannot alias module '%s' as '%s': alias is already used", decl->module,
                                  decl->alias);
                    return 0;
                }
                if (!add_module_alias(rt, decl->alias, imported)) {
                    runtime_error(rt, 0, "cannot alias module '%s' as '%s': alias is already used", decl->module,
                                  decl->alias);
                    return 0;
                }
            }
            continue;
        }

        for (int j = 0; j < decl->name_count; j++) {
            const char *name = decl->names[j];
            if (!module_is_exported(imported, name)) {
                runtime_error(rt, 0, "module '%s' does not export '%s'", decl->module, name);
                return 0;
            }

            FuncDecl *fn = find_func_in_module(imported, name);
            if (fn) {
                if (!add_binding(rt, name, fn, &imported->globals, imported)) {
                    runtime_error(rt, 0, "out of memory while importing '%s'", name);
                    return 0;
                }
                continue;
            }

            TypeDecl *type_decl = find_type_in_module(imported, name);
            if (type_decl) {
                if (!add_type_binding(rt, name, type_decl, imported)) {
                    runtime_error(rt, 0, "out of memory while importing type '%s'", name);
                    return 0;
                }
                continue;
            }

            InterfaceDecl *iface_decl = find_interface_in_module(imported, name);
            if (iface_decl) {
                if (!add_interface_binding(rt, name, iface_decl, imported)) {
                    runtime_error(rt, 0, "out of memory while importing interface '%s'", name);
                    return 0;
                }
                continue;
            }

            if (imported->native_kind != NATIVE_NONE && native_module_is_function_export(imported, name)) {
                size_t n1 = strlen(imported->namespace);
                size_t n2 = strlen(name);
                char *qualified = (char *)malloc(n1 + n2 + 2);
                if (!qualified) {
                    runtime_error(rt, 0, "out of memory while importing '%s'", name);
                    return 0;
                }
                memcpy(qualified, imported->namespace, n1);
                qualified[n1] = '.';
                memcpy(qualified + n1 + 1, name, n2);
                qualified[n1 + n2 + 1] = '\0';
                int ok = add_native_alias(rt, name, qualified);
                free(qualified);
                if (!ok) {
                    runtime_error(rt, 0, "cannot import '%s': conflicting function alias", name);
                    return 0;
                }
                continue;
            }

            Value v;
            if (!env_get(&imported->globals, name, &v)) {
                runtime_error(rt, 0, "export '%s' from module '%s' is not defined", name, decl->module);
                return 0;
            }
            char env_err[256];
            if (!env_set(&m->globals, name, v, env_err, sizeof(env_err))) {
                runtime_error(rt, 0, "%s", env_err);
                return 0;
            }
        }
    }
    return 1;
}

static int execute_module(Runtime *rt, Module *m) {
    if (m->loaded) {
        return 1;
    }
    if (m->loading) {
        runtime_error(rt, 0, "circular import detected for module '%s'", m->name);
        return 0;
    }

    m->loading = 1;

    if (!register_namespaced_exports(rt, m)) {
        runtime_error(rt, 0, "out of memory while preparing module '%s'", m->name);
        m->loading = 0;
        return 0;
    }

    if (!process_imports(rt, m)) {
        m->loading = 0;
        return 0;
    }

    for (int i = 0; i < m->prog->stmt_count; i++) {
        ExecResult r = exec_stmt(rt, m, &m->globals, m->prog->stmts[i]);
        if (rt->has_error) {
            m->loading = 0;
            return 0;
        }
        if (r.returned) {
            runtime_error(rt, m->prog->stmts[i]->line, "return is only allowed inside a function");
            m->loading = 0;
            return 0;
        }
        if (r.broke) {
            if (r.loop_label) {
                runtime_error(rt, m->prog->stmts[i]->line, "unknown loop label '%s' for break", r.loop_label);
            } else {
                runtime_error(rt, m->prog->stmts[i]->line, "break is only allowed inside a loop");
            }
            m->loading = 0;
            return 0;
        }
        if (r.continued) {
            if (r.loop_label) {
                runtime_error(rt, m->prog->stmts[i]->line, "unknown loop label '%s' for continue", r.loop_label);
            } else {
                runtime_error(rt, m->prog->stmts[i]->line, "continue is only allowed inside a loop");
            }
            m->loading = 0;
            return 0;
        }
    }

    m->loading = 0;
    m->loaded = 1;
    return 1;
}
