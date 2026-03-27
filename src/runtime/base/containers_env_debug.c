static ListValue *list_new(ValueType elem_type, const char *elem_object_type) {
    ListValue *list = (ListValue *)calloc(1, sizeof(ListValue));
    if (!list) {
        return NULL;
    }
    list->elem_type = elem_type;
    list->elem_object_type = elem_object_type ? xstrdup(elem_object_type) : NULL;
    return list;
}

static int list_push(ListValue *list, Value v) {
    if (list->count + 1 > list->cap) {
        list->cap = list->cap == 0 ? 8 : list->cap * 2;
        Value *next = (Value *)realloc(list->items, (size_t)list->cap * sizeof(Value));
        if (!next) {
            return 0;
        }
        list->items = next;
    }
    list->items[list->count++] = v;
    return 1;
}

static ListValue *list_clone(ListValue *src) {
    ListValue *dst = list_new(src->elem_type, src->elem_object_type);
    if (!dst) {
        return NULL;
    }
    for (int i = 0; i < src->count; i++) {
        if (!list_push(dst, src->items[i])) {
            return NULL;
        }
    }
    return dst;
}

static MapValue *map_new(ValueType value_type, const char *value_object_type) {
    MapValue *map = (MapValue *)calloc(1, sizeof(MapValue));
    if (!map) {
        return NULL;
    }
    map->value_type = value_type;
    map->value_object_type = value_object_type ? xstrdup(value_object_type) : NULL;
    return map;
}

static ChannelValue *channel_new(int max_buffer) {
    ChannelValue *ch = (ChannelValue *)calloc(1, sizeof(ChannelValue));
    if (!ch) {
        return NULL;
    }
    ch->elem_type = VT_INVALID;
    ch->elem_object_type = NULL;
    ch->items = NULL;
    ch->count = 0;
    ch->cap = 0;
    ch->max_buffer = max_buffer;
    ch->closed = 0;
    if (pthread_mutex_init(&ch->mu, NULL) != 0) {
        free(ch);
        return NULL;
    }
    if (pthread_cond_init(&ch->cv, NULL) != 0) {
        pthread_mutex_destroy(&ch->mu);
        free(ch);
        return NULL;
    }
    return ch;
}

static int channel_push_unsafe(ChannelValue *ch, Value v) {
    if (ch->count + 1 > ch->cap) {
        ch->cap = ch->cap == 0 ? 8 : ch->cap * 2;
        Value *next = (Value *)realloc(ch->items, (size_t)ch->cap * sizeof(Value));
        if (!next) {
            return 0;
        }
        ch->items = next;
    }
    ch->items[ch->count++] = v;
    return 1;
}

static int channel_send(ChannelValue *ch, Value v, int block, char *err, size_t err_cap) {
    if (!ch) {
        snprintf(err, err_cap, "invalid channel");
        return 0;
    }
    pthread_mutex_lock(&ch->mu);
    if (ch->closed) {
        pthread_mutex_unlock(&ch->mu);
        snprintf(err, err_cap, "send on closed channel");
        return 0;
    }
    if (v.type == VT_VOID || v.type == VT_INVALID) {
        pthread_mutex_unlock(&ch->mu);
        snprintf(err, err_cap, "channel value cannot be %s", value_type_name(v.type));
        return 0;
    }
    if (ch->elem_type == VT_INVALID) {
        ch->elem_type = v.type;
        if (v.type == VT_OBJECT && v.as.obj != NULL) {
            ch->elem_object_type = xstrdup(v.as.obj->type_name);
        }
    } else if (ch->elem_type != v.type) {
        pthread_mutex_unlock(&ch->mu);
        snprintf(err, err_cap, "channel expects %s, got %s", value_type_name(ch->elem_type), value_type_name(v.type));
        return 0;
    } else if (ch->elem_type == VT_OBJECT && ch->elem_object_type != NULL &&
               (v.as.obj == NULL || strcmp(ch->elem_object_type, v.as.obj->type_name) != 0)) {
        pthread_mutex_unlock(&ch->mu);
        snprintf(err, err_cap, "channel expects %s, got %s", ch->elem_object_type,
                 v.as.obj ? v.as.obj->type_name : "<invalid>");
        return 0;
    }

    if (ch->max_buffer >= 0) {
        while (block && ch->count >= ch->max_buffer && !ch->closed) {
            pthread_cond_wait(&ch->cv, &ch->mu);
        }
        if (ch->closed) {
            pthread_mutex_unlock(&ch->mu);
            snprintf(err, err_cap, "send on closed channel");
            return 0;
        }
        if (ch->count >= ch->max_buffer) {
            pthread_mutex_unlock(&ch->mu);
            snprintf(err, err_cap, "channel buffer full");
            return 0;
        }
    }

    if (!channel_push_unsafe(ch, v)) {
        pthread_mutex_unlock(&ch->mu);
        snprintf(err, err_cap, "out of memory");
        return 0;
    }
    pthread_cond_broadcast(&ch->cv);
    pthread_mutex_unlock(&ch->mu);
    return 1;
}

static int channel_recv(ChannelValue *ch, int block, Value *out, char *err, size_t err_cap) {
    if (!ch) {
        snprintf(err, err_cap, "invalid channel");
        return 0;
    }
    pthread_mutex_lock(&ch->mu);
    while (block && ch->count == 0 && !ch->closed) {
        pthread_cond_wait(&ch->cv, &ch->mu);
    }
    if (ch->count == 0) {
        if (ch->closed) {
            pthread_mutex_unlock(&ch->mu);
            snprintf(err, err_cap, "recv from closed channel");
            return 0;
        }
        pthread_mutex_unlock(&ch->mu);
        return -1;
    }

    *out = ch->items[0];
    if (ch->count > 1) {
        memmove(&ch->items[0], &ch->items[1], (size_t)(ch->count - 1) * sizeof(Value));
    }
    ch->count--;
    pthread_cond_broadcast(&ch->cv);
    pthread_mutex_unlock(&ch->mu);
    return 1;
}

static int channel_close(ChannelValue *ch, char *err, size_t err_cap) {
    if (!ch) {
        snprintf(err, err_cap, "invalid channel");
        return 0;
    }
    pthread_mutex_lock(&ch->mu);
    if (ch->closed) {
        pthread_mutex_unlock(&ch->mu);
        snprintf(err, err_cap, "close on closed channel");
        return 0;
    }
    ch->closed = 1;
    pthread_cond_broadcast(&ch->cv);
    pthread_mutex_unlock(&ch->mu);
    return 1;
}

static int map_find_key(MapValue *map, const char *key) {
    for (int i = 0; i < map->count; i++) {
        if (strcmp(map->keys[i], key) == 0) {
            return i;
        }
    }
    return -1;
}

static int map_set(MapValue *map, const char *key, Value value) {
    int idx = map_find_key(map, key);
    if (idx >= 0) {
        map->values[idx] = value;
        return 1;
    }
    if (map->count + 1 > map->cap) {
        map->cap = map->cap == 0 ? 8 : map->cap * 2;
        char **next_keys = (char **)realloc(map->keys, (size_t)map->cap * sizeof(char *));
        Value *next_values = (Value *)realloc(map->values, (size_t)map->cap * sizeof(Value));
        if (!next_keys || !next_values) {
            return 0;
        }
        map->keys = next_keys;
        map->values = next_values;
    }
    map->keys[map->count] = xstrdup(key);
    map->values[map->count] = value;
    map->count++;
    return 1;
}

static int map_delete(MapValue *map, const char *key) {
    int idx = map_find_key(map, key);
    if (idx < 0) {
        return 0;
    }
    free(map->keys[idx]);
    for (int i = idx; i + 1 < map->count; i++) {
        map->keys[i] = map->keys[i + 1];
        map->values[i] = map->values[i + 1];
    }
    map->count--;
    return 1;
}

static void env_init(Env *env, Env *parent) {
    env->entries = NULL;
    env->count = 0;
    env->cap = 0;
    env->parent = parent;
}

static int env_find_local(Env *env, const char *name) {
    for (int i = 0; i < env->count; i++) {
        if (strcmp(env->entries[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int env_define_raw(Env *env, const char *name, Value v, int is_const) {
    if (env->count + 1 > env->cap) {
        env->cap = env->cap == 0 ? 8 : env->cap * 2;
        VarEntry *next = (VarEntry *)realloc(env->entries, (size_t)env->cap * sizeof(VarEntry));
        if (!next) {
            return 0;
        }
        env->entries = next;
    }
    env->entries[env->count].name = xstrdup(name);
    env->entries[env->count].value = v;
    env->entries[env->count].is_const = is_const ? 1 : 0;
    env->count++;
    return 1;
}

static Env *env_snapshot_visible(Env *env) {
    Env *snap = (Env *)calloc(1, sizeof(Env));
    if (!snap) {
        return NULL;
    }
    env_init(snap, NULL);

    for (Env *cur = env; cur != NULL; cur = cur->parent) {
        for (int i = 0; i < cur->count; i++) {
            const char *name = cur->entries[i].name;
            if (env_find_local(snap, name) >= 0) {
                continue;
            }
            if (!env_define_raw(snap, name, cur->entries[i].value, cur->entries[i].is_const)) {
                return NULL;
            }
        }
    }
    return snap;
}

static int env_get(Env *env, const char *name, Value *out) {
    for (Env *cur = env; cur != NULL; cur = cur->parent) {
        int idx = env_find_local(cur, name);
        if (idx >= 0) {
            *out = cur->entries[idx].value;
            return 1;
        }
    }
    return 0;
}

static char *dbg_trim(char *s) {
    while (*s != '\0' && isspace((unsigned char)*s)) {
        s++;
    }
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) {
        s[n - 1] = '\0';
        n--;
    }
    return s;
}

static int debug_has_breakpoint(Runtime *rt, int line) {
    if (!rt || line <= 0) {
        return 0;
    }
    for (int i = 0; i < rt->debug_breakpoint_count; i++) {
        if (rt->debug_breakpoints[i] == line) {
            return 1;
        }
    }
    return 0;
}

static int debug_add_breakpoint(Runtime *rt, int line) {
    if (!rt || line <= 0) {
        return 0;
    }
    if (debug_has_breakpoint(rt, line)) {
        return 1;
    }
    if (rt->debug_breakpoint_count + 1 > rt->debug_breakpoint_cap) {
        int next_cap = rt->debug_breakpoint_cap == 0 ? 8 : rt->debug_breakpoint_cap * 2;
        int *next = (int *)realloc(rt->debug_breakpoints, (size_t)next_cap * sizeof(int));
        if (!next) {
            return 0;
        }
        rt->debug_breakpoints = next;
        rt->debug_breakpoint_cap = next_cap;
    }
    rt->debug_breakpoints[rt->debug_breakpoint_count++] = line;
    return 1;
}

static int debug_parse_breakpoints_csv(Runtime *rt, const char *csv, char *err, size_t err_cap) {
    if (!csv || csv[0] == '\0') {
        return 1;
    }
    char *copy = xstrdup(csv);
    char *cur = copy;
    while (cur && *cur != '\0') {
        char *tok = cur;
        char *comma = strchr(cur, ',');
        if (comma) {
            *comma = '\0';
            cur = comma + 1;
        } else {
            cur = NULL;
        }
        tok = dbg_trim(tok);
        if (tok[0] == '\0') {
            continue;
        }
        char *end = NULL;
        long line = strtol(tok, &end, 10);
        if (!end || *dbg_trim(end) != '\0' || line <= 0) {
            snprintf(err, err_cap, "invalid breakpoint '%s' in --break list", tok);
            free(copy);
            return 0;
        }
        if (!debug_add_breakpoint(rt, (int)line)) {
            snprintf(err, err_cap, "out of memory");
            free(copy);
            return 0;
        }
    }
    free(copy);
    return 1;
}

static void debug_print_value(Value v) {
    char tbuf[128];
    const char *tname = value_name(v, tbuf, sizeof(tbuf));
    switch (v.type) {
    case VT_INT:
        fprintf(stderr, "%lld (%s)\n", v.as.i, tname);
        break;
    case VT_FLOAT:
        fprintf(stderr, "%g (%s)\n", v.as.f, tname);
        break;
    case VT_BOOL:
        fprintf(stderr, "%s (%s)\n", v.as.b ? "true" : "false", tname);
        break;
    case VT_STRING:
        fprintf(stderr, "\"%s\" (%s)\n", v.as.s ? v.as.s : "", tname);
        break;
    case VT_ERROR:
        fprintf(stderr, "error(\"%s\") (%s)\n", v.as.s ? v.as.s : "", tname);
        break;
    default:
        fprintf(stderr, "<%s>\n", tname);
        break;
    }
}

static void debug_print_help(void) {
    fprintf(stderr, "debug commands:\n");
    fprintf(stderr, "  c | continue   run until next breakpoint\n");
    fprintf(stderr, "  s | step       stop at next statement\n");
    fprintf(stderr, "  b <line>       add breakpoint line\n");
    fprintf(stderr, "  bl             list breakpoints\n");
    fprintf(stderr, "  p <name>       print variable in current scope\n");
    fprintf(stderr, "  h | help       show this help\n");
    fprintf(stderr, "  q | quit       stop program with debugger error\n");
}

static int runtime_debug_before_stmt(Runtime *rt, Module *current_module, Env *env, Stmt *stmt) {
    if (!rt || !rt->debug_enabled || !stmt) {
        return 1;
    }
    if (!current_module || !current_module->namespace || strcmp(current_module->namespace, "__main__") != 0) {
        return 1;
    }
    if (!rt->debug_step_mode && !debug_has_breakpoint(rt, stmt->line)) {
        return 1;
    }

    fprintf(stderr, "[debug] break at line %d\n", stmt->line);
    while (1) {
        fprintf(stderr, "(dbg) ");
        fflush(stderr);
        char input[256];
        if (!fgets(input, sizeof(input), stdin)) {
            runtime_error(rt, stmt->line, "debugger input closed");
            return 0;
        }
        char *cmd = dbg_trim(input);
        if (cmd[0] == '\0' || strcmp(cmd, "s") == 0 || strcmp(cmd, "step") == 0) {
            rt->debug_step_mode = 1;
            return 1;
        }
        if (strcmp(cmd, "c") == 0 || strcmp(cmd, "continue") == 0) {
            rt->debug_step_mode = 0;
            return 1;
        }
        if (strcmp(cmd, "h") == 0 || strcmp(cmd, "help") == 0) {
            debug_print_help();
            continue;
        }
        if (strcmp(cmd, "bl") == 0) {
            if (rt->debug_breakpoint_count == 0) {
                fprintf(stderr, "no breakpoints\n");
            } else {
                fprintf(stderr, "breakpoints:");
                for (int i = 0; i < rt->debug_breakpoint_count; i++) {
                    fprintf(stderr, " %d", rt->debug_breakpoints[i]);
                }
                fprintf(stderr, "\n");
            }
            continue;
        }
        if (strncmp(cmd, "b ", 2) == 0) {
            char *num = dbg_trim(cmd + 2);
            char *end = NULL;
            long line = strtol(num, &end, 10);
            if (!end || *dbg_trim(end) != '\0' || line <= 0) {
                fprintf(stderr, "invalid breakpoint line: %s\n", num);
                continue;
            }
            if (!debug_add_breakpoint(rt, (int)line)) {
                runtime_error(rt, stmt->line, "out of memory");
                return 0;
            }
            fprintf(stderr, "added breakpoint %ld\n", line);
            continue;
        }
        if (strncmp(cmd, "p ", 2) == 0) {
            char *name = dbg_trim(cmd + 2);
            if (name[0] == '\0') {
                fprintf(stderr, "usage: p <name>\n");
                continue;
            }
            Value v;
            if (!env_get(env, name, &v)) {
                fprintf(stderr, "undefined variable '%s'\n", name);
                continue;
            }
            debug_print_value(v);
            continue;
        }
        if (strcmp(cmd, "q") == 0 || strcmp(cmd, "quit") == 0) {
            runtime_error(rt, stmt->line, "debugger terminated by user");
            return 0;
        }
        fprintf(stderr, "unknown debug command '%s' (use 'help')\n", cmd);
    }
}

static int env_set(Env *env, const char *name, Value v, char *err, size_t err_cap) {
    int idx = env_find_local(env, name);
    if (idx >= 0) {
        Value old = env->entries[idx].value;
        if (env->entries[idx].is_const) {
            snprintf(err, err_cap, "cannot assign to imut '%s'", name);
            return 0;
        }
        if (old.type != v.type) {
            char exp_buf[128];
            char got_buf[128];
            snprintf(err, err_cap, "type mismatch for '%s': expected %s, got %s", name, value_name(old, exp_buf, sizeof(exp_buf)),
                     value_name(v, got_buf, sizeof(got_buf)));
            return 0;
        }
        if (old.type == VT_OBJECT && v.type == VT_OBJECT &&
            strcmp(old.as.obj->type_name, v.as.obj->type_name) != 0) {
            snprintf(err, err_cap, "type mismatch for '%s': expected %s, got %s", name, old.as.obj->type_name,
                     v.as.obj->type_name);
            return 0;
        }
        if (old.type == VT_INTERFACE && v.type == VT_INTERFACE) {
            const char *a = old.as.iface && old.as.iface->interface_name ? old.as.iface->interface_name : "<unknown>";
            const char *b = v.as.iface && v.as.iface->interface_name ? v.as.iface->interface_name : "<unknown>";
            if (strcmp(a, b) != 0) {
                snprintf(err, err_cap, "type mismatch for '%s': expected %s, got %s", name, a, b);
                return 0;
            }
        }
        if (old.type == VT_LIST && v.type == VT_LIST && old.as.list->elem_type != v.as.list->elem_type) {
            snprintf(err, err_cap, "type mismatch for '%s': expected list of %s, got list of %s", name,
                     value_type_name(old.as.list->elem_type), value_type_name(v.as.list->elem_type));
            return 0;
        }
        if (old.type == VT_LIST && v.type == VT_LIST && old.as.list->elem_type == VT_OBJECT) {
            const char *a = old.as.list->elem_object_type ? old.as.list->elem_object_type : "<unknown>";
            const char *b = v.as.list->elem_object_type ? v.as.list->elem_object_type : "<unknown>";
            if (strcmp(a, b) != 0) {
                snprintf(err, err_cap, "type mismatch for '%s': expected list of %s, got list of %s", name, a, b);
                return 0;
            }
        }
        if (old.type == VT_LIST && v.type == VT_LIST && old.as.list->elem_type == VT_INVALID &&
            v.as.list->elem_type == VT_OBJECT && old.as.list->elem_object_type != NULL && v.as.list->elem_object_type != NULL &&
            strcmp(old.as.list->elem_object_type, v.as.list->elem_object_type) != 0) {
            snprintf(err, err_cap, "type mismatch for '%s': expected list of %s, got list of %s", name,
                     old.as.list->elem_object_type, v.as.list->elem_object_type);
            return 0;
        }
        if (old.type == VT_MAP && v.type == VT_MAP && old.as.map->value_type != v.as.map->value_type) {
            snprintf(err, err_cap, "type mismatch for '%s': expected map of %s, got map of %s", name,
                     value_type_name(old.as.map->value_type), value_type_name(v.as.map->value_type));
            return 0;
        }
        if (old.type == VT_MAP && v.type == VT_MAP && old.as.map->value_type == VT_OBJECT) {
            const char *a = old.as.map->value_object_type ? old.as.map->value_object_type : "<unknown>";
            const char *b = v.as.map->value_object_type ? v.as.map->value_object_type : "<unknown>";
            if (strcmp(a, b) != 0) {
                snprintf(err, err_cap, "type mismatch for '%s': expected map of %s, got map of %s", name, a, b);
                return 0;
            }
        }
        if (old.type == VT_CHANNEL && v.type == VT_CHANNEL && old.as.chan->elem_type != v.as.chan->elem_type) {
            snprintf(err, err_cap, "type mismatch for '%s': expected chan of %s, got chan of %s", name,
                     value_type_name(old.as.chan->elem_type), value_type_name(v.as.chan->elem_type));
            return 0;
        }
        if (old.type == VT_CHANNEL && v.type == VT_CHANNEL && old.as.chan->elem_type == VT_OBJECT) {
            const char *a = old.as.chan->elem_object_type ? old.as.chan->elem_object_type : "<unknown>";
            const char *b = v.as.chan->elem_object_type ? v.as.chan->elem_object_type : "<unknown>";
            if (strcmp(a, b) != 0) {
                snprintf(err, err_cap, "type mismatch for '%s': expected chan of %s, got chan of %s", name, a, b);
                return 0;
            }
        }
        if (old.type == VT_FUNCTION && v.type == VT_FUNCTION) {
            FuncDecl *a = old.as.func ? old.as.func->fn : NULL;
            FuncDecl *b = v.as.func ? v.as.func->fn : NULL;
            if (!func_signature_equal(a, b)) {
                char exp[192];
                char got[192];
                snprintf(err, err_cap, "type mismatch for '%s': expected %s, got %s", name,
                         func_signature_name(a, exp, sizeof(exp)), func_signature_name(b, got, sizeof(got)));
                return 0;
            }
        }
        if (old.type == VT_MULTI && v.type == VT_MULTI) {
            if (!values_share_shape(old, v)) {
                char exp[192];
                char got[192];
                snprintf(err, err_cap, "type mismatch for '%s': expected %s, got %s", name,
                         value_name(old, exp, sizeof(exp)), value_name(v, got, sizeof(got)));
                return 0;
            }
        }
        env->entries[idx].value = v;
        return 1;
    }

    if (env->count + 1 > env->cap) {
        env->cap = env->cap == 0 ? 8 : env->cap * 2;
        VarEntry *next = (VarEntry *)realloc(env->entries, (size_t)env->cap * sizeof(VarEntry));
        if (!next) {
            snprintf(err, err_cap, "out of memory");
            return 0;
        }
        env->entries = next;
    }

    env->entries[env->count].name = xstrdup(name);
    env->entries[env->count].value = v;
    env->entries[env->count].is_const = 0;
    env->count++;
    return 1;
}

static int env_set_const(Env *env, const char *name, Value v, char *err, size_t err_cap) {
    int idx = env_find_local(env, name);
    if (idx >= 0) {
        snprintf(err, err_cap, "imut '%s' already defined", name);
        return 0;
    }

    if (env->count + 1 > env->cap) {
        env->cap = env->cap == 0 ? 8 : env->cap * 2;
        VarEntry *next = (VarEntry *)realloc(env->entries, (size_t)env->cap * sizeof(VarEntry));
        if (!next) {
            snprintf(err, err_cap, "out of memory");
            return 0;
        }
        env->entries = next;
    }

    env->entries[env->count].name = xstrdup(name);
    env->entries[env->count].value = v;
    env->entries[env->count].is_const = 1;
    env->count++;
    return 1;
}

