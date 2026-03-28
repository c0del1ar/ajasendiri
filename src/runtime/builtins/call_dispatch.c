static Value call_native_module_function(Runtime *rt, Module *mod, Module *current_module, Env *env, Expr *call_expr,
                                         const char *fn_name) {
    if (mod->native_kind == NATIVE_MATH) {
        return native_math_call(rt, current_module, env, call_expr, fn_name);
    }
    if (mod->native_kind == NATIVE_TIME) {
        return native_time_call(rt, current_module, env, call_expr, fn_name);
    }
    if (mod->native_kind == NATIVE_JSON) {
        return native_json_call(rt, current_module, env, call_expr, fn_name);
    }
    if (mod->native_kind == NATIVE_FS) {
        return native_fs_call(rt, current_module, env, call_expr, fn_name);
    }
    if (mod->native_kind == NATIVE_HTTP) {
        return native_http_call(rt, current_module, env, call_expr, fn_name);
    }
    if (mod->native_kind == NATIVE_RAND) {
        return native_rand_call(rt, current_module, env, call_expr, fn_name);
    }
    if (mod->native_kind == NATIVE_OS) {
        return native_os_call(rt, current_module, env, call_expr, fn_name);
    }
    if (mod->native_kind == NATIVE_PATH) {
        return native_path_call(rt, current_module, env, call_expr, fn_name);
    }
    if (mod->native_kind == NATIVE_RE) {
        return native_re_call(rt, current_module, env, call_expr, fn_name);
    }
    runtime_error(rt, call_expr->line, "module '%s' is not native", mod->name);
    return value_invalid();
}

static Value runtime_mem_stats_value(Runtime *rt, int line) {
    MapValue *m = map_new(VT_INT, NULL);
    if (!m) {
        runtime_error(rt, line, "out of memory");
        return value_invalid();
    }

    if (!map_set(m, "strings", value_int(rt->alloc_string_count)) ||
        !map_set(m, "lists", value_int(rt->alloc_list_count)) ||
        !map_set(m, "maps", value_int(rt->alloc_map_count)) ||
        !map_set(m, "channels", value_int(rt->alloc_channel_count)) ||
        !map_set(m, "objects", value_int(rt->alloc_object_count)) ||
        !map_set(m, "interfaces", value_int(rt->alloc_interface_count)) ||
        !map_set(m, "functions", value_int(rt->alloc_function_count)) ||
        !map_set(m, "multis", value_int(rt->alloc_multi_count))) {
        runtime_error(rt, line, "out of memory");
        return value_invalid();
    }

    return value_map(m);
}

static int list_accepts_item(Runtime *rt, int line, ListValue *list, Value item, const char *method_name) {
    if (item.type == VT_VOID || item.type == VT_INVALID) {
        runtime_error(rt, line, "list.%s(...) does not support %s", method_name, value_type_name(item.type));
        return 0;
    }

    if (list->elem_type == VT_INVALID) {
        list->elem_type = item.type;
        if (item.type == VT_OBJECT && item.as.obj != NULL) {
            list->elem_object_type = xstrdup(item.as.obj->type_name);
        }
        return 1;
    }

    if (list->elem_type != item.type) {
        runtime_error(rt, line, "list.%s(...) expects %s, got %s", method_name, value_type_name(list->elem_type),
                      value_type_name(item.type));
        return 0;
    }

    if (list->elem_type == VT_OBJECT && list->elem_object_type != NULL &&
        (item.type != VT_OBJECT || item.as.obj == NULL || strcmp(list->elem_object_type, item.as.obj->type_name) != 0)) {
        runtime_error(rt, line, "list.%s(...) expects %s, got %s", method_name, list->elem_object_type,
                      item.type == VT_OBJECT && item.as.obj ? item.as.obj->type_name : value_type_name(item.type));
        return 0;
    }

    return 1;
}

static Value call_list_method(Runtime *rt, Module *current_module, Env *env, Expr *call_expr, Value list_value,
                              const char *method_name) {
    ListValue *list = list_value.as.list;
    if (strcmp(method_name, "append") == 0) {
        if (call_expr->as.call.arg_count != 1) {
            runtime_error(rt, call_expr->line, "list.append(...) expects 1 arg, got %d", call_expr->as.call.arg_count);
            return value_invalid();
        }
        Value item = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (!list_accepts_item(rt, call_expr->line, list, item, "append")) {
            return value_invalid();
        }
        if (!list_push(list, item)) {
            runtime_error(rt, call_expr->line, "out of memory");
            return value_invalid();
        }
        return value_void();
    }

    if (strcmp(method_name, "pop") == 0) {
        if (call_expr->as.call.arg_count != 0) {
            runtime_error(rt, call_expr->line, "list.pop(...) expects 0 args, got %d", call_expr->as.call.arg_count);
            return value_invalid();
        }
        if (list->count == 0) {
            runtime_error(rt, call_expr->line, "list.pop(...) from empty list");
            return value_invalid();
        }
        Value out = list->items[list->count - 1];
        list->count--;
        return out;
    }

    if (strcmp(method_name, "has") == 0) {
        if (call_expr->as.call.arg_count != 1) {
            runtime_error(rt, call_expr->line, "list.has(...) expects 1 arg, got %d", call_expr->as.call.arg_count);
            return value_invalid();
        }
        Value needle = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (list->elem_type == VT_INVALID) {
            return value_bool(0);
        }
        if (needle.type != list->elem_type) {
            runtime_error(rt, call_expr->line, "list.has(...) expects %s, got %s", value_type_name(list->elem_type),
                          value_type_name(needle.type));
            return value_invalid();
        }
        if (list->elem_type == VT_OBJECT && list->elem_object_type != NULL &&
            (needle.as.obj == NULL || strcmp(list->elem_object_type, needle.as.obj->type_name) != 0)) {
            runtime_error(rt, call_expr->line, "list.has(...) expects %s, got %s", list->elem_object_type,
                          needle.as.obj ? needle.as.obj->type_name : "<invalid>");
            return value_invalid();
        }

        for (int i = 0; i < list->count; i++) {
            int ok = 0;
            int eq = values_equal(list->items[i], needle, &ok);
            if (!ok) {
                runtime_error(rt, call_expr->line, "list.has(...) does not support element type %s",
                              value_type_name(list->elem_type));
                return value_invalid();
            }
            if (eq) {
                return value_bool(1);
            }
        }
        return value_bool(0);
    }

    if (strcmp(method_name, "extend") == 0) {
        if (call_expr->as.call.arg_count != 1) {
            runtime_error(rt, call_expr->line, "list.extend(...) expects 1 arg, got %d", call_expr->as.call.arg_count);
            return value_invalid();
        }
        Value other_val = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (other_val.type != VT_LIST || other_val.as.list == NULL) {
            runtime_error(rt, call_expr->line, "list.extend(...) expects list arg, got %s",
                          value_type_name(other_val.type));
            return value_invalid();
        }

        ListValue *other = other_val.as.list;
        if (other->elem_type != VT_INVALID && list->elem_type == VT_INVALID) {
            list->elem_type = other->elem_type;
            if (other->elem_type == VT_OBJECT && other->elem_object_type != NULL) {
                list->elem_object_type = xstrdup(other->elem_object_type);
            }
        }

        if (list->elem_type != VT_INVALID && other->elem_type != VT_INVALID && list->elem_type != other->elem_type) {
            runtime_error(rt, call_expr->line, "list.extend(...) expects %s, got %s", value_type_name(list->elem_type),
                          value_type_name(other->elem_type));
            return value_invalid();
        }
        if (list->elem_type == VT_OBJECT && list->elem_object_type != NULL && other->elem_type == VT_OBJECT &&
            (other->elem_object_type == NULL || strcmp(list->elem_object_type, other->elem_object_type) != 0)) {
            runtime_error(rt, call_expr->line, "list.extend(...) expects %s, got %s", list->elem_object_type,
                          other->elem_object_type ? other->elem_object_type : "object");
            return value_invalid();
        }

        for (int i = 0; i < other->count; i++) {
            if (!list_push(list, other->items[i])) {
                runtime_error(rt, call_expr->line, "out of memory");
                return value_invalid();
            }
        }
        return value_void();
    }

    if (strcmp(method_name, "insert") == 0) {
        if (call_expr->as.call.arg_count != 2) {
            runtime_error(rt, call_expr->line, "list.insert(...) expects 2 args, got %d", call_expr->as.call.arg_count);
            return value_invalid();
        }
        Value idx = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (idx.type != VT_INT) {
            runtime_error(rt, call_expr->line, "list.insert(...) expects int index, got %s", value_type_name(idx.type));
            return value_invalid();
        }
        if (idx.as.i < 0 || idx.as.i > list->count) {
            runtime_error(rt, call_expr->line, "list.insert(...) index out of range: %lld", idx.as.i);
            return value_invalid();
        }

        Value item = eval_expr(rt, current_module, env, call_expr->as.call.args[1]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (!list_accepts_item(rt, call_expr->line, list, item, "insert")) {
            return value_invalid();
        }

        if (idx.as.i == list->count) {
            if (!list_push(list, item)) {
                runtime_error(rt, call_expr->line, "out of memory");
                return value_invalid();
            }
            return value_void();
        }

        if (list->count == 0) {
            if (!list_push(list, item)) {
                runtime_error(rt, call_expr->line, "out of memory");
                return value_invalid();
            }
            return value_void();
        }

        if (!list_push(list, list->items[list->count - 1])) {
            runtime_error(rt, call_expr->line, "out of memory");
            return value_invalid();
        }
        for (int i = list->count - 1; i > idx.as.i; i--) {
            list->items[i] = list->items[i - 1];
        }
        list->items[idx.as.i] = item;
        return value_void();
    }

    runtime_error(rt, call_expr->line, "undefined list method '%s'", method_name);
    return value_invalid();
}

static Value call_map_method(Runtime *rt, Module *current_module, Env *env, Expr *call_expr, Value map_value,
                             const char *method_name) {
    MapValue *map = map_value.as.map;

    if (strcmp(method_name, "get") == 0 || strcmp(method_name, "pop") == 0) {
        int is_pop = strcmp(method_name, "pop") == 0;
        if (call_expr->as.call.arg_count != 1 && call_expr->as.call.arg_count != 2) {
            runtime_error(rt, call_expr->line, "map.%s(...) expects 1 or 2 args, got %d", method_name,
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        Value key = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (key.type != VT_STRING) {
            runtime_error(rt, call_expr->line, "map.%s(...) expects string key, got %s", method_name,
                          value_type_name(key.type));
            return value_invalid();
        }

        int idx = map_find_key(map, key.as.s);
        if (idx >= 0) {
            Value out = map->values[idx];
            if (is_pop) {
                (void)map_delete(map, key.as.s);
            }
            return out;
        }

        if (call_expr->as.call.arg_count == 1) {
            runtime_error(rt, call_expr->line, "map.%s(...) key '%s' not found", method_name, key.as.s);
            return value_invalid();
        }

        Value fallback = eval_expr(rt, current_module, env, call_expr->as.call.args[1]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (fallback.type == VT_VOID || fallback.type == VT_INVALID) {
            runtime_error(rt, call_expr->line, "map.%s(...) fallback does not support %s", method_name,
                          value_type_name(fallback.type));
            return value_invalid();
        }
        if (map->value_type != VT_INVALID && fallback.type != map->value_type) {
            runtime_error(rt, call_expr->line, "map.%s(...) fallback expects %s, got %s", method_name,
                          value_type_name(map->value_type), value_type_name(fallback.type));
            return value_invalid();
        }
        if (map->value_type == VT_OBJECT && map->value_object_type != NULL &&
            (fallback.type != VT_OBJECT || fallback.as.obj == NULL ||
             strcmp(map->value_object_type, fallback.as.obj->type_name) != 0)) {
            runtime_error(rt, call_expr->line, "map.%s(...) fallback expects %s, got %s", method_name,
                          map->value_object_type,
                          fallback.type == VT_OBJECT && fallback.as.obj ? fallback.as.obj->type_name
                                                                         : value_type_name(fallback.type));
            return value_invalid();
        }
        return fallback;
    }

    if (strcmp(method_name, "has") == 0) {
        if (call_expr->as.call.arg_count != 1) {
            runtime_error(rt, call_expr->line, "map.has(...) expects 1 arg, got %d", call_expr->as.call.arg_count);
            return value_invalid();
        }
        Value key = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (key.type != VT_STRING) {
            runtime_error(rt, call_expr->line, "map.has(...) expects string key, got %s", value_type_name(key.type));
            return value_invalid();
        }
        return value_bool(map_find_key(map, key.as.s) >= 0);
    }

    if (strcmp(method_name, "delete") == 0) {
        if (call_expr->as.call.arg_count != 1) {
            runtime_error(rt, call_expr->line, "map.delete(...) expects 1 arg, got %d", call_expr->as.call.arg_count);
            return value_invalid();
        }
        Value key = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (key.type != VT_STRING) {
            runtime_error(rt, call_expr->line, "map.delete(...) expects string key, got %s", value_type_name(key.type));
            return value_invalid();
        }
        (void)map_delete(map, key.as.s);
        return value_void();
    }

    if (strcmp(method_name, "keys") == 0) {
        if (call_expr->as.call.arg_count != 0) {
            runtime_error(rt, call_expr->line, "map.keys(...) expects 0 args, got %d", call_expr->as.call.arg_count);
            return value_invalid();
        }
        ListValue *out = list_new(VT_STRING, NULL);
        if (!out) {
            runtime_error(rt, call_expr->line, "out of memory");
            return value_invalid();
        }
        for (int i = 0; i < map->count; i++) {
            Value key = value_string(map->keys[i]);
            if (!list_push(out, key)) {
                runtime_error(rt, call_expr->line, "out of memory");
                return value_invalid();
            }
        }
        return value_list(out);
    }

    if (strcmp(method_name, "values") == 0) {
        if (call_expr->as.call.arg_count != 0) {
            runtime_error(rt, call_expr->line, "map.values(...) expects 0 args, got %d", call_expr->as.call.arg_count);
            return value_invalid();
        }
        ListValue *out = list_new(map->value_type, map->value_object_type);
        if (!out) {
            runtime_error(rt, call_expr->line, "out of memory");
            return value_invalid();
        }
        for (int i = 0; i < map->count; i++) {
            if (!list_push(out, map->values[i])) {
                runtime_error(rt, call_expr->line, "out of memory");
                return value_invalid();
            }
        }
        return value_list(out);
    }

    runtime_error(rt, call_expr->line, "undefined map method '%s'", method_name);
    return value_invalid();
}

static Value invoke_function_value(Runtime *rt, Module *current_module, Env *env, Expr *call_expr, Value callee) {
    if (callee.type != VT_FUNCTION || callee.as.func == NULL || callee.as.func->fn == NULL) {
        runtime_error(rt, call_expr->line, "call target must be function, got %s", value_type_name(callee.type));
        return value_invalid();
    }
    const char *display_name = callee.as.func->display_name ? callee.as.func->display_name : "<lambda>";
    Env *parent_env = callee.as.func->closure ? callee.as.func->closure : env;
    Module *owner = callee.as.func->owner ? callee.as.func->owner : current_module;
    return invoke_user_function(rt, current_module, env, call_expr, display_name, callee.as.func->fn, parent_env, owner,
                                NULL);
}

static Value call_function(Runtime *rt, Module *current_module, Env *env, Expr *call_expr) {
    if (call_expr->as.call.callee == NULL) {
        runtime_error(rt, call_expr->line, "invalid call expression");
        return value_invalid();
    }

    if (call_expr->as.call.callee->kind != EX_IDENT) {
        Value callee = eval_expr(rt, current_module, env, call_expr->as.call.callee);
        if (rt->has_error) {
            return value_invalid();
        }
        return invoke_function_value(rt, current_module, env, call_expr, callee);
    }

    const char *name = call_expr->as.call.callee->as.ident_name;
    if (strchr(name, '.') == NULL) {
        Value local_callee;
        if (env_get(env, name, &local_callee)) {
            return invoke_function_value(rt, current_module, env, call_expr, local_callee);
        }
    }

    const char *resolved_name = name;
    const char *lookup_name = NULL;
    char lookup_buf[512];
    lookup_buf[0] = '\0';
    NativeAliasBinding *alias = find_native_alias(rt, name);
    if (alias) {
        resolved_name = alias->qualified_name;
    }
    lookup_name = resolved_name;

    if (strcmp(resolved_name, "print") == 0) {
        for (int i = 0; i < call_expr->as.call.arg_count; i++) {
            Value v = eval_expr(rt, current_module, env, call_expr->as.call.args[i]);
            if (rt->has_error) {
                return value_invalid();
            }
            if (!rt->check_only) {
                if (i > 0) {
                    printf(" ");
                }
                print_value(v);
            }
        }
        if (!rt->check_only) {
            printf("\n");
        }
        return value_void();
    }

    if (strcmp(resolved_name, "input") == 0) {
        if (call_expr->as.call.arg_count > 1) {
            runtime_error(rt, call_expr->line, "function 'input' expects 0 or 1 args, got %d",
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        if (call_expr->as.call.arg_count == 1) {
            Value prompt = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
            if (rt->has_error) {
                return value_invalid();
            }
            if (prompt.type != VT_STRING) {
                runtime_error(rt, call_expr->line, "arg 1 for 'input' expects string, got %s",
                              value_type_name(prompt.type));
                return value_invalid();
            }
            if (!rt->check_only) {
                printf("%s", prompt.as.s);
                fflush(stdout);
            }
        }

        if (rt->check_only) {
            return value_string("");
        }

        char read_err[256];
        char *line = read_input_line(read_err, sizeof(read_err));
        if (!line) {
            runtime_error(rt, call_expr->line, "%s", read_err);
            return value_invalid();
        }
        Value out;
        out.type = VT_STRING;
        out.as.s = line;
        return out;
    }

    if (strcmp(resolved_name, "error") == 0) {
        if (call_expr->as.call.arg_count > 1) {
            runtime_error(rt, call_expr->line, "function 'error' expects 0 or 1 args, got %d",
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        if (call_expr->as.call.arg_count == 0) {
            return value_error("");
        }

        Value in = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (in.type != VT_STRING) {
            runtime_error(rt, call_expr->line, "arg 1 for 'error' expects string, got %s", value_type_name(in.type));
            return value_invalid();
        }
        return value_error(in.as.s);
    }

    if (strcmp(resolved_name, "raiseErr") == 0) {
        if (call_expr->as.call.arg_count != 1) {
            runtime_error(rt, call_expr->line, "function 'raiseErr' expects 1 arg, got %d",
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        Value in = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (in.type != VT_STRING) {
            runtime_error(rt, call_expr->line, "arg 1 for 'raiseErr' expects string, got %s", value_type_name(in.type));
            return value_invalid();
        }
        runtime_error(rt, call_expr->line, "%s", in.as.s);
        return value_invalid();
    }

    if (strcmp(resolved_name, "waitAll") == 0) {
        if (call_expr->as.call.arg_count != 0) {
            runtime_error(rt, call_expr->line, "function 'waitAll' expects 0 args, got %d",
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        if (!run_kostroutines(rt)) {
            return value_invalid();
        }
        return value_void();
    }

    if (strcmp(resolved_name, "chan") == 0) {
        int max_buffer = -1;
        if (call_expr->as.call.arg_count > 1) {
            runtime_error(rt, call_expr->line, "function 'chan' expects 0 or 1 args, got %d",
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        if (call_expr->as.call.arg_count == 1) {
            Value capv = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
            if (rt->has_error) {
                return value_invalid();
            }
            if (capv.type != VT_INT) {
                runtime_error(rt, call_expr->line, "arg 1 for 'chan' expects int, got %s", value_type_name(capv.type));
                return value_invalid();
            }
            if (capv.as.i <= 0) {
                runtime_error(rt, call_expr->line, "chan capacity must be > 0, got %lld", capv.as.i);
                return value_invalid();
            }
            max_buffer = (int)capv.as.i;
        }
        ChannelValue *ch = channel_new(max_buffer);
        if (!ch) {
            runtime_error(rt, call_expr->line, "out of memory");
            return value_invalid();
        }
        return value_channel(ch);
    }

    if (strcmp(resolved_name, "send") == 0) {
        if (call_expr->as.call.arg_count != 2) {
            runtime_error(rt, call_expr->line, "function 'send' expects 2 args, got %d", call_expr->as.call.arg_count);
            return value_invalid();
        }
        Value chv = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        Value item = eval_expr(rt, current_module, env, call_expr->as.call.args[1]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (chv.type != VT_CHANNEL || chv.as.chan == NULL) {
            runtime_error(rt, call_expr->line, "arg 1 for 'send' expects chan, got %s", value_type_name(chv.type));
            return value_invalid();
        }
        char ch_err[256];
        if (!channel_send(chv.as.chan, item, 1, ch_err, sizeof(ch_err))) {
            runtime_error(rt, call_expr->line, "%s", ch_err);
            return value_invalid();
        }
        return value_void();
    }

    if (strcmp(resolved_name, "recv") == 0) {
        if (call_expr->as.call.arg_count != 1) {
            runtime_error(rt, call_expr->line, "function 'recv' expects 1 arg, got %d", call_expr->as.call.arg_count);
            return value_invalid();
        }
        Value chv = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (chv.type != VT_CHANNEL || chv.as.chan == NULL) {
            runtime_error(rt, call_expr->line, "arg 1 for 'recv' expects chan, got %s", value_type_name(chv.type));
            return value_invalid();
        }
        Value out = value_invalid();
        char ch_err[256];
        int ok = channel_recv(chv.as.chan, 1, &out, ch_err, sizeof(ch_err));
        if (ok != 1) {
            runtime_error(rt, call_expr->line, "%s", ch_err);
            return value_invalid();
        }
        return out;
    }

    if (strcmp(resolved_name, "close") == 0) {
        if (call_expr->as.call.arg_count != 1) {
            runtime_error(rt, call_expr->line, "function 'close' expects 1 arg, got %d", call_expr->as.call.arg_count);
            return value_invalid();
        }
        Value chv = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (chv.type != VT_CHANNEL || chv.as.chan == NULL) {
            runtime_error(rt, call_expr->line, "arg 1 for 'close' expects chan, got %s", value_type_name(chv.type));
            return value_invalid();
        }
        char ch_err[256];
        if (!channel_close(chv.as.chan, ch_err, sizeof(ch_err))) {
            runtime_error(rt, call_expr->line, "%s", ch_err);
            return value_invalid();
        }
        return value_void();
    }

    if (strcmp(resolved_name, "trySend") == 0) {
        if (call_expr->as.call.arg_count != 2) {
            runtime_error(rt, call_expr->line, "function 'trySend' expects 2 args, got %d",
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        Value chv = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        Value item = eval_expr(rt, current_module, env, call_expr->as.call.args[1]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (chv.type != VT_CHANNEL || chv.as.chan == NULL) {
            runtime_error(rt, call_expr->line, "arg 1 for 'trySend' expects chan, got %s", value_type_name(chv.type));
            return value_invalid();
        }
        char ch_err[256];
        if (!channel_send(chv.as.chan, item, 0, ch_err, sizeof(ch_err))) {
            if (strcmp(ch_err, "send on closed channel") == 0 || strcmp(ch_err, "channel buffer full") == 0) {
                return value_bool(0);
            }
            runtime_error(rt, call_expr->line, "%s", ch_err);
            return value_invalid();
        }
        return value_bool(1);
    }

    if (strcmp(resolved_name, "tryRecv") == 0) {
        if (call_expr->as.call.arg_count != 2) {
            runtime_error(rt, call_expr->line, "function 'tryRecv' expects 2 args, got %d",
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        Value chv = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        Value fallback = eval_expr(rt, current_module, env, call_expr->as.call.args[1]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (chv.type != VT_CHANNEL || chv.as.chan == NULL) {
            runtime_error(rt, call_expr->line, "arg 1 for 'tryRecv' expects chan, got %s", value_type_name(chv.type));
            return value_invalid();
        }
        if (fallback.type == VT_VOID || fallback.type == VT_INVALID) {
            runtime_error(rt, call_expr->line, "fallback for 'tryRecv' cannot be %s", value_type_name(fallback.type));
            return value_invalid();
        }
        ChannelValue *ch = chv.as.chan;
        if (ch->elem_type != VT_INVALID && fallback.type != ch->elem_type) {
            runtime_error(rt, call_expr->line, "fallback for 'tryRecv' expects %s, got %s",
                          value_type_name(ch->elem_type), value_type_name(fallback.type));
            return value_invalid();
        }
        if (ch->elem_type == VT_OBJECT && ch->elem_object_type != NULL) {
            if (fallback.type != VT_OBJECT || fallback.as.obj == NULL ||
                strcmp(ch->elem_object_type, fallback.as.obj->type_name) != 0) {
                runtime_error(rt, call_expr->line, "fallback for 'tryRecv' expects %s, got %s", ch->elem_object_type,
                              fallback.type == VT_OBJECT && fallback.as.obj ? fallback.as.obj->type_name
                                                                             : value_type_name(fallback.type));
                return value_invalid();
            }
        }

        Value out = value_invalid();
        char ch_err[256];
        int ok = channel_recv(ch, 0, &out, ch_err, sizeof(ch_err));
        if (ok == 1) {
            return out;
        }
        if (ok == -1) {
            return fallback;
        }
        if (strcmp(ch_err, "recv from closed channel") == 0) {
            return fallback;
        }
        runtime_error(rt, call_expr->line, "%s", ch_err);
        return value_invalid();
    }

    if (strcmp(resolved_name, "int") == 0 || strcmp(resolved_name, "float") == 0 || strcmp(resolved_name, "str") == 0) {
        if (call_expr->as.call.arg_count != 1) {
            runtime_error(rt, call_expr->line, "function '%s' expects 1 arg, got %d", resolved_name,
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        Value in = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }

        if (strcmp(resolved_name, "int") == 0) {
            return cast_to_int(rt, call_expr->line, in);
        }
        if (strcmp(resolved_name, "float") == 0) {
            return cast_to_float(rt, call_expr->line, in);
        }
        return cast_to_string(rt, call_expr->line, in);
    }

    if (strcmp(resolved_name, "sort") == 0) {
        if (call_expr->as.call.arg_count != 1) {
            runtime_error(rt, call_expr->line, "function 'sort' expects 1 arg, got %d", call_expr->as.call.arg_count);
            return value_invalid();
        }

        Value in = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (in.type != VT_LIST) {
            runtime_error(rt, call_expr->line, "sort(...) expects list, got %s", value_type_name(in.type));
            return value_invalid();
        }

        ListValue *sorted = list_clone(in.as.list);
        if (!sorted) {
            runtime_error(rt, call_expr->line, "out of memory");
            return value_invalid();
        }
        if (sorted->count < 2 || sorted->elem_type == VT_INVALID) {
            return value_list(sorted);
        }

        if (sorted->elem_type != VT_INT && sorted->elem_type != VT_FLOAT && sorted->elem_type != VT_STRING) {
            runtime_error(rt, call_expr->line, "sort(...) supports list of int, float, or string; got list of %s",
                          value_type_name(sorted->elem_type));
            return value_invalid();
        }

        g_sort_elem_type = sorted->elem_type;
        qsort(sorted->items, (size_t)sorted->count, sizeof(Value), sort_value_cmp);
        g_sort_elem_type = VT_INVALID;
        return value_list(sorted);
    }

    if (strcmp(resolved_name, "length") == 0) {
        if (call_expr->as.call.arg_count != 1) {
            runtime_error(rt, call_expr->line, "function 'length' expects 1 arg, got %d", call_expr->as.call.arg_count);
            return value_invalid();
        }

        Value in = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }

        if (in.type == VT_LIST) {
            return value_int((long long)in.as.list->count);
        }
        if (in.type == VT_MAP) {
            return value_int((long long)in.as.map->count);
        }
        if (in.type == VT_STRING) {
            return value_int((long long)strlen(in.as.s));
        }

        runtime_error(rt, call_expr->line, "length(...) expects list, map, or string, got %s", value_type_name(in.type));
        return value_invalid();
    }

    if (strcmp(resolved_name, "memStats") == 0) {
        if (call_expr->as.call.arg_count != 0) {
            runtime_error(rt, call_expr->line, "function 'memStats' expects 0 args, got %d",
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        return runtime_mem_stats_value(rt, call_expr->line);
    }

    if (strcmp(resolved_name, "memCollect") == 0) {
        if (call_expr->as.call.arg_count != 0) {
            runtime_error(rt, call_expr->line, "function 'memCollect' expects 0 args, got %d",
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        return runtime_mem_stats_value(rt, call_expr->line);
    }

    char *left = NULL;
    char *right = NULL;
    if (split_qualified_name(resolved_name, &left, &right)) {
        Value recv;
        if (env_get(env, left, &recv)) {
            if (recv.type == VT_LIST && recv.as.list != NULL) {
                Value out = call_list_method(rt, current_module, env, call_expr, recv, right);
                free(left);
                free(right);
                return out;
            }
            if (recv.type == VT_MAP && recv.as.map != NULL) {
                Value out = call_map_method(rt, current_module, env, call_expr, recv, right);
                free(left);
                free(right);
                return out;
            }
            if (recv.type == VT_OBJECT) {
                FuncDecl *method = NULL;
                Env *method_globals = NULL;
                Module *method_owner = NULL;
                if (!resolve_method(rt, current_module, recv.as.obj->type_name, right, &method, &method_globals,
                                    &method_owner)) {
                    runtime_error(rt, call_expr->line, "undefined method '%s' for type %s", right,
                                  recv.as.obj->type_name);
                    free(left);
                    free(right);
                    return value_invalid();
                }
                Value out = invoke_user_function(rt, current_module, env, call_expr, resolved_name, method, method_globals,
                                                 method_owner, &recv);
                free(left);
                free(right);
                return out;
            }
            if (recv.type == VT_INTERFACE && recv.as.iface != NULL && recv.as.iface->obj != NULL) {
                InterfaceDecl *iface = recv.as.iface->iface_decl;
                if (!iface) {
                    Module *iface_owner = NULL;
                    iface = resolve_interface_decl(rt, current_module, recv.as.iface->interface_name, &iface_owner);
                }
                if (!iface) {
                    runtime_error(rt, call_expr->line, "unknown interface '%s' in runtime value",
                                  recv.as.iface->interface_name);
                    free(left);
                    free(right);
                    return value_invalid();
                }
                InterfaceMethodSig *sig = find_interface_method(iface, right);
                if (!sig) {
                    runtime_error(rt, call_expr->line, "method '%s' is not in interface '%s'", right, iface->name);
                    free(left);
                    free(right);
                    return value_invalid();
                }

                FuncDecl *method = NULL;
                Env *method_globals = NULL;
                Module *method_owner = NULL;
                if (!resolve_method(rt, current_module, recv.as.iface->obj->type_name, right, &method, &method_globals,
                                    &method_owner)) {
                    runtime_error(rt, call_expr->line, "type '%s' does not implement interface '%s': missing method '%s'",
                                  recv.as.iface->obj->type_name, iface->name, right);
                    free(left);
                    free(right);
                    return value_invalid();
                }
                if (!method_matches_interface_signature(method, sig)) {
                    runtime_error(rt, call_expr->line,
                                  "type '%s' does not implement interface '%s': method '%s' signature mismatch",
                                  recv.as.iface->obj->type_name, iface->name, right);
                    free(left);
                    free(right);
                    return value_invalid();
                }

                Value concrete_recv = value_object(recv.as.iface->obj);
                Value out = invoke_user_function(rt, current_module, env, call_expr, resolved_name, method, method_globals,
                                                 method_owner, &concrete_recv);
                free(left);
                free(right);
                return out;
            }
        }
        Module *mod = find_module_by_namespace(rt, left);
        if (mod && mod->native_kind != NATIVE_NONE) {
            Value out = call_native_module_function(rt, mod, current_module, env, call_expr, right);
            free(left);
            free(right);
            return out;
        }
        if (mod && mod->native_kind == NATIVE_NONE) {
            int w = snprintf(lookup_buf, sizeof(lookup_buf), "%s.%s", mod->namespace, right);
            if (w > 0 && (size_t)w < sizeof(lookup_buf)) {
                lookup_name = lookup_buf;
            }
        }
        free(left);
        free(right);
    }

    Module *iface_owner = NULL;
    InterfaceDecl *iface_ctor = resolve_interface_decl(rt, current_module, lookup_name, &iface_owner);
    if (iface_ctor) {
        runtime_error(rt, call_expr->line, "cannot call interface '%s' like constructor", resolved_name);
        return value_invalid();
    }

    TypeDecl *ctor_type = NULL;
    Module *ctor_owner = NULL;
    if (current_module) {
        ctor_type = find_type_in_module(current_module, lookup_name);
        if (ctor_type) {
            ctor_owner = current_module;
        }
    }
    if (!ctor_type) {
        TypeBinding *tb = find_type_binding(rt, lookup_name);
        if (tb) {
            ctor_type = tb->type_decl;
            ctor_owner = tb->owner;
        }
    }
    if (ctor_type) {
        if (call_expr->as.call.arg_count != ctor_type->field_count) {
            runtime_error(rt, call_expr->line, "constructor '%s' expects %d args, got %d", resolved_name,
                          ctor_type->field_count,
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        ObjectValue *obj = new_object_from_type_decl(ctor_type);
        if (!obj) {
            runtime_error(rt, call_expr->line, "out of memory");
            return value_invalid();
        }
        for (int i = 0; i < ctor_type->field_count; i++) {
            Value arg = eval_expr(rt, current_module, env, call_expr->as.call.args[i]);
            if (rt->has_error) {
                return value_invalid();
            }
            Value checked_arg = value_invalid();
            if (!check_value_against_type(rt, ctor_owner, call_expr->line, arg, ctor_type->fields[i].type,
                                          ctor_type->fields[i].name, &checked_arg)) {
                return value_invalid();
            }
            obj->values[i] = checked_arg;
        }
        return value_object(obj);
    }

    FuncBinding *binding = find_binding(rt, lookup_name);
    FuncDecl *fn = NULL;
    Env *fn_globals = NULL;
    Module *fn_owner = NULL;

    if (binding) {
        fn = binding->fn;
        fn_globals = binding->globals;
        fn_owner = binding->owner;
    } else if (current_module) {
        fn = find_func_in_module(current_module, lookup_name);
        if (fn) {
            fn_globals = &current_module->globals;
            fn_owner = current_module;
        }
    }

    if (!fn || !fn_globals || !fn_owner) {
        runtime_error(rt, call_expr->line, "undefined function '%s'", resolved_name);
        return value_invalid();
    }
    return invoke_user_function(rt, current_module, env, call_expr, resolved_name, fn, fn_globals, fn_owner, NULL);
}
