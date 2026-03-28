static void runtime_error(Runtime *rt, int line, const char *fmt, ...) {
    if (rt->has_error) {
        return;
    }
    rt->has_error = 1;

    char msg[384];
    va_list ap;
    va_start(ap, fmt);
    /* Flawfinder: ignore */
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    if (line > 0) {
        snprintf(rt->err, sizeof(rt->err), "line %d: %s", line, msg);
    } else {
        snprintf(rt->err, sizeof(rt->err), "%s", msg);
    }

    if (rt->call_frame_count > 0) {
        size_t used = strlen(rt->err);
        int w = snprintf(rt->err + used, used < sizeof(rt->err) ? sizeof(rt->err) - used : 0,
                         "\nstack trace (most recent call last):");
        if (w > 0) {
            used += (size_t)w;
        }

        for (int i = rt->call_frame_count - 1; i >= 0; i--) {
            const char *fname = rt->call_frames[i].name ? rt->call_frames[i].name : "<unknown>";
            int fline = rt->call_frames[i].line;
            w = snprintf(rt->err + used, used < sizeof(rt->err) ? sizeof(rt->err) - used : 0, "\n  at %s", fname);
            if (w > 0) {
                used += (size_t)w;
            }
            if (fline > 0) {
                w = snprintf(rt->err + used, used < sizeof(rt->err) ? sizeof(rt->err) - used : 0, " (line %d)", fline);
                if (w > 0) {
                    used += (size_t)w;
                }
            }
        }
    }
}

static int is_number(ValueType t) {
    return t == VT_INT || t == VT_FLOAT;
}

static double to_double(Value v) {
    if (v.type == VT_FLOAT) {
        return v.as.f;
    }
    return (double)v.as.i;
}

static int has_suffix(const char *s, const char *suffix) {
    size_t n = strlen(s);
    size_t m = strlen(suffix);
    if (n < m) {
        return 0;
    }
    return strcmp(s + (n - m), suffix) == 0;
}

static int is_readable_file_path(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return 0;
    }
    fclose(f);
    return 1;
}

static char *dirname_from_path(const char *path) {
    const char *slash = strrchr(path, '/');
    if (!slash) {
        return xstrdup(".");
    }
    if (slash == path) {
        return xstrdup("/");
    }
    return xstrndup(path, (size_t)(slash - path));
}

static char *join_path(const char *a, const char *b) {
    size_t na = strlen(a);
    size_t nb = strlen(b);
    int need_slash = (na > 0 && a[na - 1] != '/');
    if (na > ((size_t)-1) - nb - (size_t)need_slash - 1) {
        fprintf(stderr, "fatal: path too long\n");
        exit(1);
    }
    char *out = (char *)malloc(na + nb + (size_t)need_slash + 1);
    if (!out) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    memcpy(out, a, na);
    if (need_slash) {
        out[na++] = '/';
    }
    memcpy(out + na, b, nb);
    out[na + nb] = '\0';
    return out;
}

static char *module_namespace(const char *mod_name) {
    const char *last = strrchr(mod_name, '/');
    const char *base = last ? last + 1 : mod_name;
    size_t n = strlen(base);
    if (n > 4 && strcmp(base + (n - 4), ".aja") == 0) {
        n -= 4;
    }
    return xstrndup(base, n);
}

static char *module_path_from_name(const char *base_dir, const char *mod_name) {
    char *file = NULL;
    if (has_suffix(mod_name, ".aja")) {
        file = xstrdup(mod_name);
    } else {
        size_t n = strlen(mod_name);
        if (n > ((size_t)-1) - 5) {
            fprintf(stderr, "fatal: module name too long\n");
            exit(1);
        }
        file = (char *)malloc(n + 5);
        if (!file) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        memcpy(file, mod_name, n);
        memcpy(file + n, ".aja", 5);
    }

    if (file[0] == '/') {
        return file;
    }

    char *fallback = join_path(base_dir, file);
    if (is_readable_file_path(fallback)) {
        free(file);
        return fallback;
    }

    const char *venv = getenv("AJA_VENV");
    if (venv && venv[0] != '\0') {
        char *venv_root = NULL;
        if (venv[0] == '/') {
            venv_root = xstrdup(venv);
        } else {
            venv_root = join_path(base_dir, venv);
        }
        char *venv_site = join_path(venv_root, "site-packages");
        char *venv_path = join_path(venv_site, file);
        free(venv_root);
        free(venv_site);
        if (is_readable_file_path(venv_path)) {
            free(file);
            free(fallback);
            return venv_path;
        }
        free(venv_path);
    }

    char *cur = xstrdup(base_dir);
    while (1) {
        char *site_root = join_path(cur, ".aja/site-packages");
        char *site_path = join_path(site_root, file);
        free(site_root);
        if (is_readable_file_path(site_path)) {
            free(cur);
            free(file);
            free(fallback);
            return site_path;
        }
        free(site_path);

        char *parent = dirname_from_path(cur);
        if (strcmp(parent, cur) == 0) {
            free(parent);
            break;
        }
        free(cur);
        cur = parent;
    }
    free(cur);

    const char *home = getenv("HOME");
    if (home && home[0] != '\0') {
        char *global_root = join_path(home, ".aja/site-packages");
        char *global_path = join_path(global_root, file);
        free(global_root);
        if (is_readable_file_path(global_path)) {
            free(file);
            free(fallback);
            return global_path;
        }
        free(global_path);
    }

    const char *env_path = getenv("AJA_PATH");
    if (env_path && env_path[0] != '\0') {
        const char *seg = env_path;
        while (*seg != '\0') {
            const char *end = seg;
            while (*end != '\0' && *end != ':') {
                end++;
            }
            if (end > seg) {
                char *dir = xstrndup(seg, (size_t)(end - seg));
                char *candidate = join_path(dir, file);
                free(dir);
                if (is_readable_file_path(candidate)) {
                    free(file);
                    free(fallback);
                    return candidate;
                }
                free(candidate);
            }
            seg = *end == ':' ? end + 1 : end;
        }
    }

    free(file);
    return fallback;
}

static char *read_file(const char *path, char *err, size_t err_cap) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(err, err_cap, "read error: cannot open %s", path);
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        snprintf(err, err_cap, "read error: cannot seek %s", path);
        return NULL;
    }
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        snprintf(err, err_cap, "read error: cannot tell size of %s", path);
        return NULL;
    }
    if ((size_t)size > ((size_t)-1) - 1) {
        fclose(f);
        snprintf(err, err_cap, "read error: file too large: %s", path);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        snprintf(err, err_cap, "read error: cannot seek %s", path);
        return NULL;
    }

    char *buf = (char *)malloc((size_t)size + 1);
    if (!buf) {
        fclose(f);
        snprintf(err, err_cap, "out of memory");
        return NULL;
    }

    size_t got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    if (got != (size_t)size) {
        free(buf);
        snprintf(err, err_cap, "read error: short read on %s", path);
        return NULL;
    }

    buf[size] = '\0';
    return buf;
}

static Module *find_module_by_name(Runtime *rt, const char *name) {
    for (int i = 0; i < rt->module_count; i++) {
        if (strcmp(rt->modules[i].name, name) == 0) {
            return &rt->modules[i];
        }
    }
    return NULL;
}

static Module *find_module_by_namespace(Runtime *rt, const char *ns) {
    for (int i = 0; i < rt->module_count; i++) {
        if (strcmp(rt->modules[i].namespace, ns) == 0) {
            return &rt->modules[i];
        }
    }
    for (int i = 0; i < rt->module_alias_count; i++) {
        if (strcmp(rt->module_aliases[i].alias, ns) == 0) {
            return rt->module_aliases[i].module;
        }
    }
    return NULL;
}

static Module *add_module(Runtime *rt, const char *name, const char *ns, const char *base_dir, Program *prog) {
    if (rt->module_count + 1 > rt->module_cap) {
        rt->module_cap = rt->module_cap == 0 ? 8 : rt->module_cap * 2;
        Module *next = (Module *)realloc(rt->modules, (size_t)rt->module_cap * sizeof(Module));
        if (!next) {
            return NULL;
        }
        rt->modules = next;
    }

    Module *m = &rt->modules[rt->module_count++];
    memset(m, 0, sizeof(*m));
    m->name = xstrdup(name);
    m->namespace = xstrdup(ns);
    m->base_dir = xstrdup(base_dir);
    m->prog = prog;
    env_init(&m->globals, NULL);
    return m;
}

static FuncDecl *find_func_in_module(Module *m, const char *name) {
    for (int i = 0; i < m->prog->func_count; i++) {
        if (!m->prog->funcs[i]->has_receiver && strcmp(m->prog->funcs[i]->name, name) == 0) {
            return m->prog->funcs[i];
        }
    }
    return NULL;
}

static FuncDecl *find_method_in_module(Module *m, const char *recv_type_name, const char *method_name) {
    for (int i = 0; i < m->prog->func_count; i++) {
        FuncDecl *fn = m->prog->funcs[i];
        if (!fn->has_receiver) {
            continue;
        }
        if (fn->receiver.type.kind != VT_OBJECT || fn->receiver.type.custom_name == NULL) {
            continue;
        }
        if (strcmp(fn->receiver.type.custom_name, recv_type_name) != 0) {
            continue;
        }
        if (strcmp(fn->name, method_name) == 0) {
            return fn;
        }
    }
    return NULL;
}

static TypeDecl *find_type_in_module(Module *m, const char *name) {
    for (int i = 0; i < m->prog->type_count; i++) {
        if (strcmp(m->prog->types[i]->name, name) == 0) {
            return m->prog->types[i];
        }
    }
    return NULL;
}

static InterfaceDecl *find_interface_in_module(Module *m, const char *name) {
    for (int i = 0; i < m->prog->interface_count; i++) {
        if (strcmp(m->prog->interfaces[i]->name, name) == 0) {
            return m->prog->interfaces[i];
        }
    }
    return NULL;
}

static int module_is_exported(Module *m, const char *name) {
    for (int i = 0; i < m->prog->export_count; i++) {
        if (strcmp(m->prog->exports[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

static char *alias_to_namespaced_name(Runtime *rt, const char *name) {
    const char *dot = strchr(name, '.');
    if (!dot) {
        return NULL;
    }
    size_t alias_len = (size_t)(dot - name);
    const char *suffix = dot + 1;
    if (*suffix == '\0') {
        return NULL;
    }

    for (int i = 0; i < rt->module_alias_count; i++) {
        ModuleAliasBinding *a = &rt->module_aliases[i];
        if (strlen(a->alias) != alias_len || strncmp(a->alias, name, alias_len) != 0) {
            continue;
        }
        size_t n1 = strlen(a->module->namespace);
        size_t n2 = strlen(suffix);
        if (n1 > ((size_t)-1) - n2 - 2) {
            return NULL;
        }
        char *full = (char *)malloc(n1 + n2 + 2);
        if (!full) {
            return NULL;
        }
        memcpy(full, a->module->namespace, n1);
        full[n1] = '.';
        memcpy(full + n1 + 1, suffix, n2);
        full[n1 + n2 + 1] = '\0';
        return full;
    }
    return NULL;
}

static FuncBinding *find_binding(Runtime *rt, const char *name) {
    for (int i = 0; i < rt->binding_count; i++) {
        if (strcmp(rt->bindings[i].name, name) == 0) {
            return &rt->bindings[i];
        }
    }

    char *aliased = alias_to_namespaced_name(rt, name);
    if (aliased) {
        for (int i = 0; i < rt->binding_count; i++) {
            if (strcmp(rt->bindings[i].name, aliased) == 0) {
                free(aliased);
                return &rt->bindings[i];
            }
        }
        free(aliased);
    }
    return NULL;
}

static TypeBinding *find_type_binding(Runtime *rt, const char *name) {
    for (int i = 0; i < rt->type_binding_count; i++) {
        if (strcmp(rt->type_bindings[i].name, name) == 0) {
            return &rt->type_bindings[i];
        }
    }

    char *aliased = alias_to_namespaced_name(rt, name);
    if (aliased) {
        for (int i = 0; i < rt->type_binding_count; i++) {
            if (strcmp(rt->type_bindings[i].name, aliased) == 0) {
                free(aliased);
                return &rt->type_bindings[i];
            }
        }
        free(aliased);
    }
    return NULL;
}

static InterfaceBinding *find_interface_binding(Runtime *rt, const char *name) {
    for (int i = 0; i < rt->interface_binding_count; i++) {
        if (strcmp(rt->interface_bindings[i].name, name) == 0) {
            return &rt->interface_bindings[i];
        }
    }

    char *aliased = alias_to_namespaced_name(rt, name);
    if (aliased) {
        for (int i = 0; i < rt->interface_binding_count; i++) {
            if (strcmp(rt->interface_bindings[i].name, aliased) == 0) {
                free(aliased);
                return &rt->interface_bindings[i];
            }
        }
        free(aliased);
    }
    return NULL;
}

static int add_binding(Runtime *rt, const char *name, FuncDecl *fn, Env *globals, Module *owner) {
    if (find_binding(rt, name)) {
        return 1;
    }
    if (rt->binding_count + 1 > rt->binding_cap) {
        rt->binding_cap = rt->binding_cap == 0 ? 16 : rt->binding_cap * 2;
        FuncBinding *next = (FuncBinding *)realloc(rt->bindings, (size_t)rt->binding_cap * sizeof(FuncBinding));
        if (!next) {
            return 0;
        }
        rt->bindings = next;
    }

    FuncBinding *b = &rt->bindings[rt->binding_count++];
    b->name = xstrdup(name);
    b->fn = fn;
    b->globals = globals;
    b->owner = owner;
    return 1;
}

static int add_type_binding(Runtime *rt, const char *name, TypeDecl *type_decl, Module *owner) {
    if (find_type_binding(rt, name)) {
        return 1;
    }
    if (rt->type_binding_count + 1 > rt->type_binding_cap) {
        rt->type_binding_cap = rt->type_binding_cap == 0 ? 16 : rt->type_binding_cap * 2;
        TypeBinding *next = (TypeBinding *)realloc(rt->type_bindings, (size_t)rt->type_binding_cap * sizeof(TypeBinding));
        if (!next) {
            return 0;
        }
        rt->type_bindings = next;
    }

    TypeBinding *b = &rt->type_bindings[rt->type_binding_count++];
    b->name = xstrdup(name);
    b->type_decl = type_decl;
    b->owner = owner;
    return 1;
}

static int add_interface_binding(Runtime *rt, const char *name, InterfaceDecl *iface_decl, Module *owner) {
    if (find_interface_binding(rt, name)) {
        return 1;
    }
    if (rt->interface_binding_count + 1 > rt->interface_binding_cap) {
        rt->interface_binding_cap = rt->interface_binding_cap == 0 ? 16 : rt->interface_binding_cap * 2;
        InterfaceBinding *next =
            (InterfaceBinding *)realloc(rt->interface_bindings, (size_t)rt->interface_binding_cap * sizeof(InterfaceBinding));
        if (!next) {
            return 0;
        }
        rt->interface_bindings = next;
    }

    InterfaceBinding *b = &rt->interface_bindings[rt->interface_binding_count++];
    b->name = xstrdup(name);
    b->iface_decl = iface_decl;
    b->owner = owner;
    return 1;
}

static NativeAliasBinding *find_native_alias(Runtime *rt, const char *name) {
    for (int i = 0; i < rt->native_alias_count; i++) {
        if (strcmp(rt->native_aliases[i].name, name) == 0) {
            return &rt->native_aliases[i];
        }
    }
    return NULL;
}

static ModuleAliasBinding *find_module_alias(Runtime *rt, const char *alias) {
    for (int i = 0; i < rt->module_alias_count; i++) {
        if (strcmp(rt->module_aliases[i].alias, alias) == 0) {
            return &rt->module_aliases[i];
        }
    }
    return NULL;
}

static int add_module_alias(Runtime *rt, const char *alias, Module *module) {
    ModuleAliasBinding *existing = find_module_alias(rt, alias);
    if (existing) {
        return existing->module == module;
    }

    if (rt->module_alias_count + 1 > rt->module_alias_cap) {
        rt->module_alias_cap = rt->module_alias_cap == 0 ? 16 : rt->module_alias_cap * 2;
        ModuleAliasBinding *next =
            (ModuleAliasBinding *)realloc(rt->module_aliases, (size_t)rt->module_alias_cap * sizeof(ModuleAliasBinding));
        if (!next) {
            return 0;
        }
        rt->module_aliases = next;
    }

    ModuleAliasBinding *b = &rt->module_aliases[rt->module_alias_count++];
    b->alias = xstrdup(alias);
    b->module = module;
    return 1;
}

static int add_native_alias(Runtime *rt, const char *name, const char *qualified_name) {
    NativeAliasBinding *existing = find_native_alias(rt, name);
    if (existing) {
        return strcmp(existing->qualified_name, qualified_name) == 0;
    }
    if (rt->native_alias_count + 1 > rt->native_alias_cap) {
        rt->native_alias_cap = rt->native_alias_cap == 0 ? 16 : rt->native_alias_cap * 2;
        NativeAliasBinding *next =
            (NativeAliasBinding *)realloc(rt->native_aliases, (size_t)rt->native_alias_cap * sizeof(NativeAliasBinding));
        if (!next) {
            return 0;
        }
        rt->native_aliases = next;
    }
    NativeAliasBinding *a = &rt->native_aliases[rt->native_alias_count++];
    a->name = xstrdup(name);
    a->qualified_name = xstrdup(qualified_name);
    return 1;
}

static int push_call_frame(Runtime *rt, const char *name, int line) {
    if (rt->call_frame_count + 1 > rt->call_frame_cap) {
        rt->call_frame_cap = rt->call_frame_cap == 0 ? 8 : rt->call_frame_cap * 2;
        CallFrame *next = (CallFrame *)realloc(rt->call_frames, (size_t)rt->call_frame_cap * sizeof(CallFrame));
        if (!next) {
            return 0;
        }
        rt->call_frames = next;
    }

    CallFrame *f = &rt->call_frames[rt->call_frame_count++];
    f->name = xstrdup(name ? name : "<unknown>");
    f->line = line;
    return 1;
}

static void pop_call_frame(Runtime *rt) {
    if (rt->call_frame_count <= 0) {
        return;
    }
    rt->call_frame_count--;
    free(rt->call_frames[rt->call_frame_count].name);
    rt->call_frames[rt->call_frame_count].name = NULL;
}

static int push_defer_frame(Runtime *rt) {
    if (rt->defer_frame_count + 1 > rt->defer_frame_cap) {
        rt->defer_frame_cap = rt->defer_frame_cap == 0 ? 8 : rt->defer_frame_cap * 2;
        DeferFrame *next = (DeferFrame *)realloc(rt->defer_frames, (size_t)rt->defer_frame_cap * sizeof(DeferFrame));
        if (!next) {
            return 0;
        }
        rt->defer_frames = next;
    }
    DeferFrame *f = &rt->defer_frames[rt->defer_frame_count++];
    memset(f, 0, sizeof(*f));
    return 1;
}

static DeferFrame *current_defer_frame(Runtime *rt) {
    if (rt->defer_frame_count <= 0) {
        return NULL;
    }
    return &rt->defer_frames[rt->defer_frame_count - 1];
}

static int defer_frame_add_call(DeferFrame *f, Expr *call_expr) {
    if (!f) {
        return 0;
    }
    if (f->count + 1 > f->cap) {
        f->cap = f->cap == 0 ? 4 : f->cap * 2;
        Expr **next = (Expr **)realloc(f->calls, (size_t)f->cap * sizeof(Expr *));
        if (!next) {
            return 0;
        }
        f->calls = next;
    }
    f->calls[f->count++] = call_expr;
    return 1;
}

static void pop_defer_frame(Runtime *rt) {
    if (rt->defer_frame_count <= 0) {
        return;
    }
    rt->defer_frame_count--;
    DeferFrame *f = &rt->defer_frames[rt->defer_frame_count];
    free(f->calls);
    f->calls = NULL;
    f->count = 0;
    f->cap = 0;
}

static void free_kostroutine_task(KostroutineTask *task) {
    if (!task) {
        return;
    }
    free(task->display_name);
    free(task->args);
    if (task->arg_names) {
        for (int i = 0; i < task->arg_count; i++) {
            free(task->arg_names[i]);
        }
        free(task->arg_names);
    }
    task->display_name = NULL;
    task->args = NULL;
    task->arg_names = NULL;
    task->arg_count = 0;
}

static int push_kostroutine_thread(Runtime *rt, pthread_t thread, void *ctx) {
    if (rt->kostroutine_thread_count + 1 > rt->kostroutine_thread_cap) {
        int next_cap = rt->kostroutine_thread_cap == 0 ? 8 : rt->kostroutine_thread_cap * 2;
        pthread_t *next_threads = (pthread_t *)malloc((size_t)next_cap * sizeof(pthread_t));
        void **next_ctxs = (void **)malloc((size_t)next_cap * sizeof(void *));
        if (!next_threads || !next_ctxs) {
            free(next_threads);
            free(next_ctxs);
            return 0;
        }
        for (int i = 0; i < rt->kostroutine_thread_count; i++) {
            next_threads[i] = rt->kostroutine_threads[i];
            next_ctxs[i] = rt->kostroutine_thread_ctxs[i];
        }
        free(rt->kostroutine_threads);
        free(rt->kostroutine_thread_ctxs);
        rt->kostroutine_threads = next_threads;
        rt->kostroutine_thread_ctxs = next_ctxs;
        rt->kostroutine_thread_cap = next_cap;
    }
    rt->kostroutine_threads[rt->kostroutine_thread_count] = thread;
    rt->kostroutine_thread_ctxs[rt->kostroutine_thread_count] = ctx;
    rt->kostroutine_thread_count++;
    return 1;
}

static Value eval_expr(Runtime *rt, Module *current_module, Env *env, Expr *expr);
static ExecResult exec_stmt(Runtime *rt, Module *current_module, Env *env, Stmt *stmt);
static int execute_module(Runtime *rt, Module *m);
static int schedule_kostroutine_call(Runtime *rt, Module *current_module, Env *env, Expr *call_expr, int line);
static int run_kostroutines(Runtime *rt);
static int resolve_method(Runtime *rt, Module *current_module, const char *recv_type_name, const char *method_name,
                          FuncDecl **out_fn, Env **out_globals, Module **out_owner);
