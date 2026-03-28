static int split_qualified_name(const char *name, char **left, char **right) {
    const char *dot = strchr(name, '.');
    if (!dot) {
        return 0;
    }
    *left = xstrndup(name, (size_t)(dot - name));
    *right = xstrdup(dot + 1);
    return 1;
}

static int find_object_field_index(ObjectValue *obj, const char *field_name) {
    for (int i = 0; i < obj->field_count; i++) {
        if (strcmp(obj->fields[i].name, field_name) == 0) {
            return i;
        }
    }
    return -1;
}

static int type_ref_equal(TypeRef a, TypeRef b) {
    if (a.kind != b.kind) {
        return 0;
    }
    if (a.kind == VT_LIST) {
        if (a.list_elem_type == NULL || b.list_elem_type == NULL) {
            return a.list_elem_type == b.list_elem_type;
        }
        return type_ref_equal(*a.list_elem_type, *b.list_elem_type);
    }
    if (a.kind == VT_MAP) {
        if (a.map_key_type == NULL || b.map_key_type == NULL) {
            if (a.map_key_type != b.map_key_type) {
                return 0;
            }
        } else if (!type_ref_equal(*a.map_key_type, *b.map_key_type)) {
            return 0;
        }
        if (a.map_value_type == NULL || b.map_value_type == NULL) {
            return a.map_value_type == b.map_value_type;
        }
        return type_ref_equal(*a.map_value_type, *b.map_value_type);
    }
    if (a.kind == VT_CHANNEL) {
        if (a.chan_elem_type == NULL || b.chan_elem_type == NULL) {
            return a.chan_elem_type == b.chan_elem_type;
        }
        return type_ref_equal(*a.chan_elem_type, *b.chan_elem_type);
    }
    if (a.kind == VT_MULTI) {
        if (a.multi_sig == NULL || b.multi_sig == NULL) {
            return a.multi_sig == b.multi_sig;
        }
        if (a.multi_sig->count != b.multi_sig->count) {
            return 0;
        }
        for (int i = 0; i < a.multi_sig->count; i++) {
            if (!type_ref_equal(a.multi_sig->items[i], b.multi_sig->items[i])) {
                return 0;
            }
        }
        return 1;
    }
    if (a.kind == VT_FUNCTION) {
        if (a.func_sig == NULL || b.func_sig == NULL) {
            return a.func_sig == b.func_sig;
        }
        if (!a.func_sig->return_type || !b.func_sig->return_type) {
            return a.func_sig->return_type == b.func_sig->return_type;
        }
        if (!type_ref_equal(*a.func_sig->return_type, *b.func_sig->return_type)) {
            return 0;
        }
        if (a.func_sig->param_count != b.func_sig->param_count) {
            return 0;
        }
        for (int i = 0; i < a.func_sig->param_count; i++) {
            if (!type_ref_equal(a.func_sig->params[i], b.func_sig->params[i])) {
                return 0;
            }
        }
        return 1;
    }
    if (a.kind == VT_OBJECT) {
        if (a.custom_name == NULL || b.custom_name == NULL) {
            return a.custom_name == b.custom_name;
        }
        return strcmp(a.custom_name, b.custom_name) == 0;
    }
    return 1;
}

static int function_matches_expected_type(Value v, TypeRef expected) {
    if (v.type != VT_FUNCTION || v.as.func == NULL || v.as.func->fn == NULL) {
        return 0;
    }
    if (expected.func_sig == NULL) {
        return 1;
    }
    FuncDecl *fn = v.as.func->fn;
    if (!expected.func_sig->return_type) {
        return 0;
    }
    if (!type_ref_equal(fn->return_type, *expected.func_sig->return_type)) {
        return 0;
    }
    if (fn->param_count != expected.func_sig->param_count) {
        return 0;
    }
    for (int i = 0; i < fn->param_count; i++) {
        if (!type_ref_equal(fn->params[i].type, expected.func_sig->params[i])) {
            return 0;
        }
    }
    return 1;
}

static InterfaceDecl *resolve_interface_decl(Runtime *rt, Module *current_module, const char *name, Module **out_owner) {
    if (current_module) {
        InterfaceDecl *local = find_interface_in_module(current_module, name);
        if (local) {
            if (out_owner) {
                *out_owner = current_module;
            }
            return local;
        }
    }
    InterfaceBinding *binding = find_interface_binding(rt, name);
    if (binding) {
        if (out_owner) {
            *out_owner = binding->owner;
        }
        return binding->iface_decl;
    }
    return NULL;
}

static InterfaceMethodSig *find_interface_method(InterfaceDecl *iface, const char *name) {
    if (!iface) {
        return NULL;
    }
    for (int i = 0; i < iface->method_count; i++) {
        if (strcmp(iface->methods[i].name, name) == 0) {
            return &iface->methods[i];
        }
    }
    return NULL;
}

static int method_matches_interface_signature(FuncDecl *fn, InterfaceMethodSig *sig) {
    if (!fn || !sig) {
        return 0;
    }
    if (fn->param_count != sig->param_count) {
        return 0;
    }
    if (!type_ref_equal(fn->return_type, sig->return_type)) {
        return 0;
    }
    for (int i = 0; i < fn->param_count; i++) {
        if (!type_ref_equal(fn->params[i].type, sig->params[i].type)) {
            return 0;
        }
    }
    return 1;
}

static int object_implements_interface(Runtime *rt, int line, Module *current_module, ObjectValue *obj,
                                       InterfaceDecl *iface) {
    for (int i = 0; i < iface->method_count; i++) {
        InterfaceMethodSig *sig = &iface->methods[i];
        FuncDecl *method = NULL;
        Env *method_globals = NULL;
        Module *method_owner = NULL;
        if (!resolve_method(rt, current_module, obj->type_name, sig->name, &method, &method_globals, &method_owner)) {
            runtime_error(rt, line, "type '%s' does not implement interface '%s': missing method '%s'", obj->type_name,
                          iface->name, sig->name);
            return 0;
        }
        if (!method_matches_interface_signature(method, sig)) {
            runtime_error(rt, line, "type '%s' does not implement interface '%s': method '%s' signature mismatch",
                          obj->type_name, iface->name, sig->name);
            return 0;
        }
    }
    return 1;
}

static int check_value_against_type(Runtime *rt, Module *current_module, int line, Value v, TypeRef expected,
                                    const char *context_name, Value *out) {
    if (expected.kind == VT_MULTI) {
        if (v.type != VT_MULTI || v.as.multi == NULL || expected.multi_sig == NULL) {
            char exp_buf[128];
            char got_buf[128];
            runtime_error(rt, line, "type mismatch for %s: expected %s, got %s", context_name,
                          type_ref_name(expected, exp_buf, sizeof(exp_buf)), value_name(v, got_buf, sizeof(got_buf)));
            return 0;
        }
        if (v.as.multi->count != expected.multi_sig->count) {
            runtime_error(rt, line, "type mismatch for %s: expected %d return values, got %d", context_name,
                          expected.multi_sig->count, v.as.multi->count);
            return 0;
        }

        Value *checked_items = (Value *)calloc((size_t)v.as.multi->count, sizeof(Value));
        if (!checked_items) {
            runtime_error(rt, line, "out of memory");
            return 0;
        }
        for (int i = 0; i < v.as.multi->count; i++) {
            char nested_ctx[64];
            snprintf(nested_ctx, sizeof(nested_ctx), "%s item %d", context_name, i + 1);
            if (!check_value_against_type(rt, current_module, line, v.as.multi->items[i], expected.multi_sig->items[i],
                                          nested_ctx, &checked_items[i])) {
                free(checked_items);
                return 0;
            }
        }

        Value wrapped = value_multi(checked_items, v.as.multi->count);
        free(checked_items);
        if (wrapped.type == VT_INVALID) {
            runtime_error(rt, line, "out of memory");
            return 0;
        }
        *out = wrapped;
        return 1;
    }

    if (expected.kind == VT_FUNCTION) {
        if (function_matches_expected_type(v, expected)) {
            *out = v;
            return 1;
        }
        char exp_buf[128];
        char got_buf[128];
        runtime_error(rt, line, "type mismatch for %s: expected %s, got %s", context_name,
                      type_ref_name(expected, exp_buf, sizeof(exp_buf)), value_name(v, got_buf, sizeof(got_buf)));
        return 0;
    }

    if (expected.kind == VT_OBJECT && expected.custom_name != NULL) {
        Module *iface_owner = NULL;
        InterfaceDecl *iface = resolve_interface_decl(rt, current_module, expected.custom_name, &iface_owner);
        if (iface) {
            if (v.type == VT_INTERFACE && v.as.iface != NULL && v.as.iface->interface_name != NULL &&
                strcmp(v.as.iface->interface_name, iface->name) == 0) {
                *out = v;
                return 1;
            }

            ObjectValue *obj = NULL;
            if (v.type == VT_OBJECT && v.as.obj != NULL) {
                obj = v.as.obj;
            } else if (v.type == VT_INTERFACE && v.as.iface != NULL && v.as.iface->obj != NULL) {
                obj = v.as.iface->obj;
            }
            if (!obj) {
                char got_buf[128];
                runtime_error(rt, line, "type mismatch for %s: expected %s, got %s", context_name, iface->name,
                              value_name(v, got_buf, sizeof(got_buf)));
                return 0;
            }
            if (!object_implements_interface(rt, line, current_module, obj, iface)) {
                return 0;
            }
            Value wrapped = value_interface(iface, obj);
            if (wrapped.type == VT_INVALID) {
                runtime_error(rt, line, "out of memory");
                return 0;
            }
            *out = wrapped;
            return 1;
        }
    }

    if (expected.kind == VT_LIST && expected.list_elem_type != NULL && v.type == VT_LIST && v.as.list != NULL &&
        v.as.list->elem_type == VT_INVALID) {
        TypeRef elem_t = *expected.list_elem_type;
        v.as.list->elem_type = elem_t.kind;
        if (elem_t.kind == VT_OBJECT && elem_t.custom_name != NULL) {
            v.as.list->elem_object_type = xstrdup(elem_t.custom_name);
        }
    }
    if (expected.kind == VT_MAP && expected.map_value_type != NULL && v.type == VT_MAP && v.as.map != NULL &&
        v.as.map->value_type == VT_INVALID) {
        TypeRef value_t = *expected.map_value_type;
        v.as.map->value_type = value_t.kind;
        if (value_t.kind == VT_OBJECT && value_t.custom_name != NULL) {
            v.as.map->value_object_type = xstrdup(value_t.custom_name);
        }
    }
    if (expected.kind == VT_CHANNEL && expected.chan_elem_type != NULL && v.type == VT_CHANNEL && v.as.chan != NULL &&
        v.as.chan->elem_type == VT_INVALID) {
        TypeRef elem_t = *expected.chan_elem_type;
        v.as.chan->elem_type = elem_t.kind;
        if (elem_t.kind == VT_OBJECT && elem_t.custom_name != NULL) {
            v.as.chan->elem_object_type = xstrdup(elem_t.custom_name);
        }
    }

    if (value_matches_type(v, expected)) {
        *out = v;
        return 1;
    }
    char exp_buf[128];
    char got_buf[128];
    runtime_error(rt, line, "type mismatch for %s: expected %s, got %s", context_name,
                  type_ref_name(expected, exp_buf, sizeof(exp_buf)), value_name(v, got_buf, sizeof(got_buf)));
    return 0;
}

static TypeRef clone_type_ref(TypeRef src) {
    TypeRef out;
    memset(&out, 0, sizeof(out));
    out.kind = src.kind;
    out.custom_name = src.custom_name ? xstrdup(src.custom_name) : NULL;

    if (src.func_sig != NULL) {
        out.func_sig = (FuncTypeSig *)calloc(1, sizeof(FuncTypeSig));
        if (!out.func_sig) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        out.func_sig->param_count = src.func_sig->param_count;
        out.func_sig->param_cap = src.func_sig->param_count;
        if (src.func_sig->param_count > 0) {
            out.func_sig->params = (TypeRef *)calloc((size_t)src.func_sig->param_count, sizeof(TypeRef));
            if (!out.func_sig->params) {
                fprintf(stderr, "fatal: out of memory\n");
                exit(1);
            }
            for (int i = 0; i < src.func_sig->param_count; i++) {
                out.func_sig->params[i] = clone_type_ref(src.func_sig->params[i]);
            }
        }
        if (src.func_sig->return_type != NULL) {
            out.func_sig->return_type = (TypeRef *)calloc(1, sizeof(TypeRef));
            if (!out.func_sig->return_type) {
                fprintf(stderr, "fatal: out of memory\n");
                exit(1);
            }
            *out.func_sig->return_type = clone_type_ref(*src.func_sig->return_type);
        }
    }

    if (src.multi_sig != NULL) {
        out.multi_sig = (MultiTypeSig *)calloc(1, sizeof(MultiTypeSig));
        if (!out.multi_sig) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        out.multi_sig->count = src.multi_sig->count;
        out.multi_sig->cap = src.multi_sig->count;
        if (src.multi_sig->count > 0) {
            out.multi_sig->items = (TypeRef *)calloc((size_t)src.multi_sig->count, sizeof(TypeRef));
            if (!out.multi_sig->items) {
                fprintf(stderr, "fatal: out of memory\n");
                exit(1);
            }
            for (int i = 0; i < src.multi_sig->count; i++) {
                out.multi_sig->items[i] = clone_type_ref(src.multi_sig->items[i]);
            }
        }
    }

    if (src.list_elem_type != NULL) {
        out.list_elem_type = (TypeRef *)calloc(1, sizeof(TypeRef));
        if (!out.list_elem_type) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        *out.list_elem_type = clone_type_ref(*src.list_elem_type);
    }
    if (src.map_key_type != NULL) {
        out.map_key_type = (TypeRef *)calloc(1, sizeof(TypeRef));
        if (!out.map_key_type) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        *out.map_key_type = clone_type_ref(*src.map_key_type);
    }
    if (src.map_value_type != NULL) {
        out.map_value_type = (TypeRef *)calloc(1, sizeof(TypeRef));
        if (!out.map_value_type) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        *out.map_value_type = clone_type_ref(*src.map_value_type);
    }
    if (src.chan_elem_type != NULL) {
        out.chan_elem_type = (TypeRef *)calloc(1, sizeof(TypeRef));
        if (!out.chan_elem_type) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        *out.chan_elem_type = clone_type_ref(*src.chan_elem_type);
    }
    return out;
}

static ObjectValue *new_object_from_type_decl(TypeDecl *type_decl) {
    ObjectValue *obj = (ObjectValue *)calloc(1, sizeof(ObjectValue));
    if (!obj) {
        return NULL;
    }
    runtime_note_alloc_object();
    obj->type_name = xstrdup(type_decl->name);
    if (!obj->type_name) {
        free(obj);
        return NULL;
    }
    obj->field_count = type_decl->field_count;
    obj->fields = (ObjectFieldDef *)calloc((size_t)obj->field_count, sizeof(ObjectFieldDef));
    obj->values = (Value *)calloc((size_t)obj->field_count, sizeof(Value));
    if (!obj->fields || !obj->values) {
        free(obj->fields);
        free(obj->values);
        free(obj->type_name);
        free(obj);
        return NULL;
    }
    for (int i = 0; i < obj->field_count; i++) {
        obj->fields[i].name = xstrdup(type_decl->fields[i].name);
        if (!obj->fields[i].name) {
            for (int j = 0; j < i; j++) {
                free(obj->fields[j].name);
            }
            free(obj->fields);
            free(obj->values);
            free(obj->type_name);
            free(obj);
            return NULL;
        }
        obj->fields[i].type = clone_type_ref(type_decl->fields[i].type);
        obj->values[i] = value_invalid();
    }
    return obj;
}

static int resolve_method(Runtime *rt, Module *current_module, const char *recv_type_name, const char *method_name,
                          FuncDecl **out_fn, Env **out_globals, Module **out_owner) {
    if (current_module) {
        FuncDecl *fn = find_method_in_module(current_module, recv_type_name, method_name);
        if (fn) {
            *out_fn = fn;
            *out_globals = &current_module->globals;
            *out_owner = current_module;
            return 1;
        }
    }
    for (int i = 0; i < rt->module_count; i++) {
        Module *m = &rt->modules[i];
        if (m == current_module) {
            continue;
        }
        FuncDecl *fn = find_method_in_module(m, recv_type_name, method_name);
        if (fn) {
            *out_fn = fn;
            *out_globals = &m->globals;
            *out_owner = m;
            return 1;
        }
    }
    return 0;
}

static Value eval_numeric_add_sub_mul(Runtime *rt, int line, Value left, Value right, TokenType op) {
    if (!is_number(left.type) || !is_number(right.type)) {
        if (op == TOK_PLUS) {
            runtime_error(rt, line, "'+' expects numbers or strings, got %s and %s", value_type_name(left.type),
                          value_type_name(right.type));
        } else {
            runtime_error(rt, line, "numeric operator expects numbers, got %s and %s", value_type_name(left.type),
                          value_type_name(right.type));
        }
        return value_invalid();
    }

    if (left.type == VT_INT && right.type == VT_INT) {
        if (op == TOK_PLUS) {
            return value_int(left.as.i + right.as.i);
        }
        if (op == TOK_MINUS) {
            return value_int(left.as.i - right.as.i);
        }
        if (op == TOK_STAR) {
            return value_int(left.as.i * right.as.i);
        }
    }

    double a = to_double(left);
    double b = to_double(right);
    if (op == TOK_PLUS) {
        return value_float(a + b);
    }
    if (op == TOK_MINUS) {
        return value_float(a - b);
    }
    if (op == TOK_STAR) {
        return value_float(a * b);
    }

    runtime_error(rt, line, "unsupported numeric operator");
    return value_invalid();
}

static int values_equal(Value left, Value right, int *ok) {
    *ok = 1;

    if (is_number(left.type) && is_number(right.type)) {
        return to_double(left) == to_double(right);
    }

    if (left.type != right.type) {
        *ok = 0;
        return 0;
    }

    if (left.type == VT_INT) {
        return left.as.i == right.as.i;
    }
    if (left.type == VT_FLOAT) {
        return left.as.f == right.as.f;
    }
    if (left.type == VT_STRING) {
        return strcmp(left.as.s, right.as.s) == 0;
    }
    if (left.type == VT_ERROR) {
        return strcmp(left.as.s, right.as.s) == 0;
    }
    if (left.type == VT_BOOL) {
        return left.as.b == right.as.b;
    }
    if (left.type == VT_VOID) {
        return 1;
    }

    *ok = 0;
    return 0;
}

static int list_contains_value(Runtime *rt, int line, ListValue *list, Value needle, int *out_contains) {
    if (list->elem_type == VT_INVALID) {
        *out_contains = 0;
        return 1;
    }

    if (list->elem_type != needle.type) {
        runtime_error(rt, line, "membership for list expects %s, got %s", value_type_name(list->elem_type),
                      value_type_name(needle.type));
        return 0;
    }
    if (list->elem_type == VT_OBJECT && list->elem_object_type != NULL &&
        (needle.as.obj == NULL || strcmp(list->elem_object_type, needle.as.obj->type_name) != 0)) {
        runtime_error(rt, line, "membership for list expects %s, got %s", list->elem_object_type,
                      needle.as.obj ? needle.as.obj->type_name : "<invalid>");
        return 0;
    }

    for (int i = 0; i < list->count; i++) {
        int ok = 0;
        int eq = values_equal(list->items[i], needle, &ok);
        if (!ok) {
            runtime_error(rt, line, "membership for list does not support element type %s",
                          value_type_name(list->elem_type));
            return 0;
        }
        if (eq) {
            *out_contains = 1;
            return 1;
        }
    }
    *out_contains = 0;
    return 1;
}

static Value eval_binary(Runtime *rt, Module *current_module, Env *env, Expr *expr) {
    if (expr->as.binary.op == TOK_AND) {
        Value left = eval_expr(rt, current_module, env, expr->as.binary.left);
        if (rt->has_error) {
            return value_invalid();
        }
        if (left.type != VT_BOOL) {
            runtime_error(rt, expr->line, "logical operator expects bool operands, got %s", value_type_name(left.type));
            return value_invalid();
        }
        if (!left.as.b) {
            return value_bool(0);
        }

        Value right = eval_expr(rt, current_module, env, expr->as.binary.right);
        if (rt->has_error) {
            return value_invalid();
        }
        if (right.type != VT_BOOL) {
            runtime_error(rt, expr->line, "logical operator expects bool operands, got %s", value_type_name(right.type));
            return value_invalid();
        }
        return value_bool(right.as.b);
    }

    if (expr->as.binary.op == TOK_OR) {
        Value left = eval_expr(rt, current_module, env, expr->as.binary.left);
        if (rt->has_error) {
            return value_invalid();
        }
        if (left.type != VT_BOOL) {
            runtime_error(rt, expr->line, "logical operator expects bool operands, got %s", value_type_name(left.type));
            return value_invalid();
        }
        if (left.as.b) {
            return value_bool(1);
        }

        Value right = eval_expr(rt, current_module, env, expr->as.binary.right);
        if (rt->has_error) {
            return value_invalid();
        }
        if (right.type != VT_BOOL) {
            runtime_error(rt, expr->line, "logical operator expects bool operands, got %s", value_type_name(right.type));
            return value_invalid();
        }
        return value_bool(right.as.b);
    }

    Value left = eval_expr(rt, current_module, env, expr->as.binary.left);
    if (rt->has_error) {
        return value_invalid();
    }
    Value right = eval_expr(rt, current_module, env, expr->as.binary.right);
    if (rt->has_error) {
        return value_invalid();
    }

    if (expr->as.binary.op == TOK_IN || expr->as.binary.op == TOK_NOTIN) {
        int contains = 0;
        if (right.type == VT_LIST) {
            if (!list_contains_value(rt, expr->line, right.as.list, left, &contains)) {
                return value_invalid();
            }
        } else if (right.type == VT_MAP) {
            if (left.type != VT_STRING) {
                runtime_error(rt, expr->line, "membership for map expects string key, got %s",
                              value_type_name(left.type));
                return value_invalid();
            }
            contains = map_find_key(right.as.map, left.as.s) >= 0;
        } else if (right.type == VT_STRING) {
            if (left.type != VT_STRING) {
                runtime_error(rt, expr->line, "membership for string expects string needle, got %s",
                              value_type_name(left.type));
                return value_invalid();
            }
            contains = strstr(right.as.s, left.as.s) != NULL;
        } else {
            runtime_error(rt, expr->line, "operator 'in' expects right operand list, map, or string, got %s",
                          value_type_name(right.type));
            return value_invalid();
        }
        if (expr->as.binary.op == TOK_NOTIN) {
            return value_bool(!contains);
        }
        return value_bool(contains);
    }

    if (expr->as.binary.op == TOK_PLUS) {
        if (left.type == VT_STRING && right.type == VT_STRING) {
            size_t l1 = strlen(left.as.s);
            size_t l2 = strlen(right.as.s);
            if (l1 > ((size_t)-1) - l2 - 1) {
                runtime_error(rt, expr->line, "string too large");
                return value_invalid();
            }
            char *cat = (char *)malloc(l1 + l2 + 1);
            if (!cat) {
                runtime_error(rt, expr->line, "out of memory");
                return value_invalid();
            }
            memcpy(cat, left.as.s, l1);
            memcpy(cat + l1, right.as.s, l2);
            cat[l1 + l2] = '\0';
            Value v;
            v.type = VT_STRING;
            v.as.s = cat;
            return v;
        }
        return eval_numeric_add_sub_mul(rt, expr->line, left, right, TOK_PLUS);
    }

    if (expr->as.binary.op == TOK_MINUS) {
        return eval_numeric_add_sub_mul(rt, expr->line, left, right, TOK_MINUS);
    }

    if (expr->as.binary.op == TOK_STAR) {
        return eval_numeric_add_sub_mul(rt, expr->line, left, right, TOK_STAR);
    }

    if (expr->as.binary.op == TOK_SLASH) {
        if (!is_number(left.type) || !is_number(right.type)) {
            runtime_error(rt, expr->line, "numeric operator expects numbers, got %s and %s", value_type_name(left.type),
                          value_type_name(right.type));
            return value_invalid();
        }
        if ((right.type == VT_INT && right.as.i == 0) || (right.type == VT_FLOAT && right.as.f == 0.0)) {
            runtime_error(rt, expr->line, "division by zero");
            return value_invalid();
        }
        return value_float(to_double(left) / to_double(right));
    }

    if (expr->as.binary.op == TOK_EQ || expr->as.binary.op == TOK_NEQ) {
        int ok = 0;
        int eq = values_equal(left, right, &ok);
        if (!ok) {
            runtime_error(rt, expr->line, "equality expects same types or numeric mix, got %s and %s",
                          value_type_name(left.type), value_type_name(right.type));
            return value_invalid();
        }
        return value_bool(expr->as.binary.op == TOK_EQ ? eq : !eq);
    }

    if (expr->as.binary.op == TOK_LT || expr->as.binary.op == TOK_LTE || expr->as.binary.op == TOK_GT ||
        expr->as.binary.op == TOK_GTE) {
        if (!is_number(left.type) || !is_number(right.type)) {
            runtime_error(rt, expr->line, "ordering comparison expects numbers, got %s and %s",
                          value_type_name(left.type), value_type_name(right.type));
            return value_invalid();
        }

        double a = to_double(left);
        double b = to_double(right);
        if (expr->as.binary.op == TOK_LT) {
            return value_bool(a < b);
        }
        if (expr->as.binary.op == TOK_LTE) {
            return value_bool(a <= b);
        }
        if (expr->as.binary.op == TOK_GT) {
            return value_bool(a > b);
        }
        return value_bool(a >= b);
    }

    runtime_error(rt, expr->line, "unsupported binary operator");
    return value_invalid();
}
