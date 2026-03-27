static long long runtime_now_ms(void) {
    struct timespec ts;
    (void)timespec_get(&ts, TIME_UTC);
    return (long long)ts.tv_sec * 1000LL + (long long)(ts.tv_nsec / 1000000L);
}

static void runtime_sleep_ms(long long ms) {
    if (ms <= 0) {
        return;
    }
    long long until = runtime_now_ms() + ms;
    while (runtime_now_ms() < until) {
    }
}

static Value eval_expr(Runtime *rt, Module *current_module, Env *env, Expr *expr) {
    if (expr->kind == EX_INT) {
        return value_int(expr->as.int_val);
    }
    if (expr->kind == EX_FLOAT) {
        return value_float(expr->as.float_val);
    }
    if (expr->kind == EX_STRING) {
        return value_string(expr->as.string_val);
    }
    if (expr->kind == EX_BOOL) {
        return value_bool(expr->as.bool_val);
    }
    if (expr->kind == EX_LIST) {
        ListValue *list = list_new(VT_INVALID, NULL);
        if (!list) {
            runtime_error(rt, expr->line, "out of memory");
            return value_invalid();
        }
        for (int i = 0; i < expr->as.list_lit.count; i++) {
            Value item = eval_expr(rt, current_module, env, expr->as.list_lit.items[i]);
            if (rt->has_error) {
                return value_invalid();
            }
            if (item.type == VT_VOID || item.type == VT_INVALID) {
                runtime_error(rt, expr->line, "list item cannot be %s", value_type_name(item.type));
                return value_invalid();
            }
            if (list->elem_type == VT_INVALID) {
                list->elem_type = item.type;
                if (item.type == VT_OBJECT) {
                    list->elem_object_type = xstrdup(item.as.obj->type_name);
                }
            } else if (list->elem_type != item.type) {
                runtime_error(rt, expr->line, "list items must have the same type: expected %s, got %s",
                              value_type_name(list->elem_type), value_type_name(item.type));
                return value_invalid();
            } else if (list->elem_type == VT_OBJECT && list->elem_object_type != NULL &&
                       strcmp(list->elem_object_type, item.as.obj->type_name) != 0) {
                runtime_error(rt, expr->line, "list items must have the same type: expected %s, got %s",
                              list->elem_object_type, item.as.obj->type_name);
                return value_invalid();
            }
            if (!list_push(list, item)) {
                runtime_error(rt, expr->line, "out of memory");
                return value_invalid();
            }
        }
        return value_list(list);
    }
    if (expr->kind == EX_MAP) {
        MapValue *map = map_new(VT_INVALID, NULL);
        if (!map) {
            runtime_error(rt, expr->line, "out of memory");
            return value_invalid();
        }
        for (int i = 0; i < expr->as.map_lit.count; i++) {
            Value item = eval_expr(rt, current_module, env, expr->as.map_lit.values[i]);
            if (rt->has_error) {
                return value_invalid();
            }
            if (item.type == VT_VOID || item.type == VT_INVALID) {
                runtime_error(rt, expr->line, "map value cannot be %s", value_type_name(item.type));
                return value_invalid();
            }
            if (map->value_type == VT_INVALID) {
                map->value_type = item.type;
                if (item.type == VT_OBJECT && item.as.obj != NULL) {
                    map->value_object_type = xstrdup(item.as.obj->type_name);
                }
            } else if (map->value_type != item.type) {
                runtime_error(rt, expr->line, "map values must have the same type: expected %s, got %s",
                              value_type_name(map->value_type), value_type_name(item.type));
                return value_invalid();
            } else if (map->value_type == VT_OBJECT && map->value_object_type != NULL &&
                       (item.as.obj == NULL || strcmp(map->value_object_type, item.as.obj->type_name) != 0)) {
                runtime_error(rt, expr->line, "map values must have the same type: expected %s, got %s",
                              map->value_object_type, item.as.obj ? item.as.obj->type_name : "<invalid>");
                return value_invalid();
            }

            if (!map_set(map, expr->as.map_lit.keys[i], item)) {
                runtime_error(rt, expr->line, "out of memory");
                return value_invalid();
            }
        }
        return value_map(map);
    }
    if (expr->kind == EX_LIST_COMP) {
        ListValue *out = list_new(VT_INVALID, NULL);
        if (!out) {
            runtime_error(rt, expr->line, "out of memory");
            return value_invalid();
        }
        Value iterable = eval_expr(rt, current_module, env, expr->as.list_comp.iterable);
        if (rt->has_error) {
            return value_invalid();
        }

        char env_err[256];
        if (iterable.type == VT_LIST) {
            for (int i = 0; i < iterable.as.list->count; i++) {
                if (!env_set(env, expr->as.list_comp.var_name, iterable.as.list->items[i], env_err, sizeof(env_err))) {
                    runtime_error(rt, expr->line, "%s", env_err);
                    return value_invalid();
                }
                if (expr->as.list_comp.has_cond) {
                    Value cond = eval_expr(rt, current_module, env, expr->as.list_comp.cond);
                    if (rt->has_error) {
                        return value_invalid();
                    }
                    if (cond.type != VT_BOOL) {
                        runtime_error(rt, expr->line, "list comprehension filter expects bool, got %s",
                                      value_type_name(cond.type));
                        return value_invalid();
                    }
                    if (!cond.as.b) {
                        continue;
                    }
                }
                Value item = eval_expr(rt, current_module, env, expr->as.list_comp.item_expr);
                if (rt->has_error) {
                    return value_invalid();
                }
                if (item.type == VT_VOID || item.type == VT_INVALID) {
                    runtime_error(rt, expr->line, "list item cannot be %s", value_type_name(item.type));
                    return value_invalid();
                }
                if (out->elem_type == VT_INVALID) {
                    out->elem_type = item.type;
                    if (item.type == VT_OBJECT && item.as.obj != NULL) {
                        out->elem_object_type = xstrdup(item.as.obj->type_name);
                    }
                } else if (out->elem_type != item.type) {
                    runtime_error(rt, expr->line, "list items must have the same type: expected %s, got %s",
                                  value_type_name(out->elem_type), value_type_name(item.type));
                    return value_invalid();
                } else if (out->elem_type == VT_OBJECT && out->elem_object_type != NULL &&
                           (item.as.obj == NULL || strcmp(out->elem_object_type, item.as.obj->type_name) != 0)) {
                    runtime_error(rt, expr->line, "list items must have the same type: expected %s, got %s",
                                  out->elem_object_type, item.as.obj ? item.as.obj->type_name : "<invalid>");
                    return value_invalid();
                }
                if (!list_push(out, item)) {
                    runtime_error(rt, expr->line, "out of memory");
                    return value_invalid();
                }
            }
            return value_list(out);
        }
        if (iterable.type == VT_MAP) {
            for (int i = 0; i < iterable.as.map->count; i++) {
                Value key = value_string(iterable.as.map->keys[i]);
                if (!env_set(env, expr->as.list_comp.var_name, key, env_err, sizeof(env_err))) {
                    runtime_error(rt, expr->line, "%s", env_err);
                    return value_invalid();
                }
                if (expr->as.list_comp.has_cond) {
                    Value cond = eval_expr(rt, current_module, env, expr->as.list_comp.cond);
                    if (rt->has_error) {
                        return value_invalid();
                    }
                    if (cond.type != VT_BOOL) {
                        runtime_error(rt, expr->line, "list comprehension filter expects bool, got %s",
                                      value_type_name(cond.type));
                        return value_invalid();
                    }
                    if (!cond.as.b) {
                        continue;
                    }
                }
                Value item = eval_expr(rt, current_module, env, expr->as.list_comp.item_expr);
                if (rt->has_error) {
                    return value_invalid();
                }
                if (item.type == VT_VOID || item.type == VT_INVALID) {
                    runtime_error(rt, expr->line, "list item cannot be %s", value_type_name(item.type));
                    return value_invalid();
                }
                if (out->elem_type == VT_INVALID) {
                    out->elem_type = item.type;
                    if (item.type == VT_OBJECT && item.as.obj != NULL) {
                        out->elem_object_type = xstrdup(item.as.obj->type_name);
                    }
                } else if (out->elem_type != item.type) {
                    runtime_error(rt, expr->line, "list items must have the same type: expected %s, got %s",
                                  value_type_name(out->elem_type), value_type_name(item.type));
                    return value_invalid();
                } else if (out->elem_type == VT_OBJECT && out->elem_object_type != NULL &&
                           (item.as.obj == NULL || strcmp(out->elem_object_type, item.as.obj->type_name) != 0)) {
                    runtime_error(rt, expr->line, "list items must have the same type: expected %s, got %s",
                                  out->elem_object_type, item.as.obj ? item.as.obj->type_name : "<invalid>");
                    return value_invalid();
                }
                if (!list_push(out, item)) {
                    runtime_error(rt, expr->line, "out of memory");
                    return value_invalid();
                }
            }
            return value_list(out);
        }
        runtime_error(rt, expr->line, "list comprehension expects list or map, got %s", value_type_name(iterable.type));
        return value_invalid();
    }
    if (expr->kind == EX_MAP_COMP) {
        MapValue *out = map_new(VT_INVALID, NULL);
        if (!out) {
            runtime_error(rt, expr->line, "out of memory");
            return value_invalid();
        }
        Value iterable = eval_expr(rt, current_module, env, expr->as.map_comp.iterable);
        if (rt->has_error) {
            return value_invalid();
        }

        char env_err[256];
        if (iterable.type == VT_LIST) {
            for (int i = 0; i < iterable.as.list->count; i++) {
                if (!env_set(env, expr->as.map_comp.var_name, iterable.as.list->items[i], env_err, sizeof(env_err))) {
                    runtime_error(rt, expr->line, "%s", env_err);
                    return value_invalid();
                }
                if (expr->as.map_comp.has_cond) {
                    Value cond = eval_expr(rt, current_module, env, expr->as.map_comp.cond);
                    if (rt->has_error) {
                        return value_invalid();
                    }
                    if (cond.type != VT_BOOL) {
                        runtime_error(rt, expr->line, "map comprehension filter expects bool, got %s",
                                      value_type_name(cond.type));
                        return value_invalid();
                    }
                    if (!cond.as.b) {
                        continue;
                    }
                }
                Value key_v = eval_expr(rt, current_module, env, expr->as.map_comp.key_expr);
                if (rt->has_error) {
                    return value_invalid();
                }
                if (key_v.type != VT_STRING) {
                    runtime_error(rt, expr->line, "map comprehension key must be string, got %s",
                                  value_type_name(key_v.type));
                    return value_invalid();
                }
                Value val_v = eval_expr(rt, current_module, env, expr->as.map_comp.value_expr);
                if (rt->has_error) {
                    return value_invalid();
                }
                if (val_v.type == VT_VOID || val_v.type == VT_INVALID) {
                    runtime_error(rt, expr->line, "map value cannot be %s", value_type_name(val_v.type));
                    return value_invalid();
                }
                if (out->value_type == VT_INVALID) {
                    out->value_type = val_v.type;
                    if (val_v.type == VT_OBJECT && val_v.as.obj != NULL) {
                        out->value_object_type = xstrdup(val_v.as.obj->type_name);
                    }
                } else if (out->value_type != val_v.type) {
                    runtime_error(rt, expr->line, "map values must have the same type: expected %s, got %s",
                                  value_type_name(out->value_type), value_type_name(val_v.type));
                    return value_invalid();
                } else if (out->value_type == VT_OBJECT && out->value_object_type != NULL &&
                           (val_v.as.obj == NULL || strcmp(out->value_object_type, val_v.as.obj->type_name) != 0)) {
                    runtime_error(rt, expr->line, "map values must have the same type: expected %s, got %s",
                                  out->value_object_type, val_v.as.obj ? val_v.as.obj->type_name : "<invalid>");
                    return value_invalid();
                }
                if (!map_set(out, key_v.as.s, val_v)) {
                    runtime_error(rt, expr->line, "out of memory");
                    return value_invalid();
                }
            }
            return value_map(out);
        }
        if (iterable.type == VT_MAP) {
            for (int i = 0; i < iterable.as.map->count; i++) {
                Value key = value_string(iterable.as.map->keys[i]);
                if (!env_set(env, expr->as.map_comp.var_name, key, env_err, sizeof(env_err))) {
                    runtime_error(rt, expr->line, "%s", env_err);
                    return value_invalid();
                }
                if (expr->as.map_comp.has_cond) {
                    Value cond = eval_expr(rt, current_module, env, expr->as.map_comp.cond);
                    if (rt->has_error) {
                        return value_invalid();
                    }
                    if (cond.type != VT_BOOL) {
                        runtime_error(rt, expr->line, "map comprehension filter expects bool, got %s",
                                      value_type_name(cond.type));
                        return value_invalid();
                    }
                    if (!cond.as.b) {
                        continue;
                    }
                }
                Value key_v = eval_expr(rt, current_module, env, expr->as.map_comp.key_expr);
                if (rt->has_error) {
                    return value_invalid();
                }
                if (key_v.type != VT_STRING) {
                    runtime_error(rt, expr->line, "map comprehension key must be string, got %s",
                                  value_type_name(key_v.type));
                    return value_invalid();
                }
                Value val_v = eval_expr(rt, current_module, env, expr->as.map_comp.value_expr);
                if (rt->has_error) {
                    return value_invalid();
                }
                if (val_v.type == VT_VOID || val_v.type == VT_INVALID) {
                    runtime_error(rt, expr->line, "map value cannot be %s", value_type_name(val_v.type));
                    return value_invalid();
                }
                if (out->value_type == VT_INVALID) {
                    out->value_type = val_v.type;
                    if (val_v.type == VT_OBJECT && val_v.as.obj != NULL) {
                        out->value_object_type = xstrdup(val_v.as.obj->type_name);
                    }
                } else if (out->value_type != val_v.type) {
                    runtime_error(rt, expr->line, "map values must have the same type: expected %s, got %s",
                                  value_type_name(out->value_type), value_type_name(val_v.type));
                    return value_invalid();
                } else if (out->value_type == VT_OBJECT && out->value_object_type != NULL &&
                           (val_v.as.obj == NULL || strcmp(out->value_object_type, val_v.as.obj->type_name) != 0)) {
                    runtime_error(rt, expr->line, "map values must have the same type: expected %s, got %s",
                                  out->value_object_type, val_v.as.obj ? val_v.as.obj->type_name : "<invalid>");
                    return value_invalid();
                }
                if (!map_set(out, key_v.as.s, val_v)) {
                    runtime_error(rt, expr->line, "out of memory");
                    return value_invalid();
                }
            }
            return value_map(out);
        }
        runtime_error(rt, expr->line, "map comprehension expects list or map, got %s", value_type_name(iterable.type));
        return value_invalid();
    }
    if (expr->kind == EX_INDEX) {
        Value target = eval_expr(rt, current_module, env, expr->as.index.target);
        if (rt->has_error) {
            return value_invalid();
        }

        Value idx = eval_expr(rt, current_module, env, expr->as.index.index);
        if (rt->has_error) {
            return value_invalid();
        }

        if (target.type == VT_LIST) {
            if (idx.type != VT_INT) {
                runtime_error(rt, expr->line, "list index must be int, got %s", value_type_name(idx.type));
                return value_invalid();
            }
            if (idx.as.i < 0 || idx.as.i >= target.as.list->count) {
                runtime_error(rt, expr->line, "list index out of range: %lld", idx.as.i);
                return value_invalid();
            }
            return target.as.list->items[idx.as.i];
        }
        if (target.type == VT_MAP) {
            if (idx.type != VT_STRING) {
                runtime_error(rt, expr->line, "map index must be string, got %s", value_type_name(idx.type));
                return value_invalid();
            }
            int key_idx = map_find_key(target.as.map, idx.as.s);
            if (key_idx < 0) {
                runtime_error(rt, expr->line, "map key '%s' not found", idx.as.s);
                return value_invalid();
            }
            return target.as.map->values[key_idx];
        }

        runtime_error(rt, expr->line, "index target must be list or map, got %s", value_type_name(target.type));
        return value_invalid();
    }
    if (expr->kind == EX_SLICE) {
        Value target = eval_expr(rt, current_module, env, expr->as.slice.target);
        if (rt->has_error) {
            return value_invalid();
        }

        long long start = 0;
        long long end = 0;
        long long max_len = 0;

        if (target.type == VT_LIST) {
            max_len = target.as.list->count;
        } else if (target.type == VT_STRING) {
            max_len = (long long)strlen(target.as.s);
        } else {
            runtime_error(rt, expr->line, "slice target must be list or string, got %s", value_type_name(target.type));
            return value_invalid();
        }

        end = max_len;
        if (expr->as.slice.has_start) {
            Value sv = eval_expr(rt, current_module, env, expr->as.slice.start);
            if (rt->has_error) {
                return value_invalid();
            }
            if (sv.type != VT_INT) {
                runtime_error(rt, expr->line, "slice start must be int, got %s", value_type_name(sv.type));
                return value_invalid();
            }
            start = sv.as.i;
        }
        if (expr->as.slice.has_end) {
            Value ev = eval_expr(rt, current_module, env, expr->as.slice.end);
            if (rt->has_error) {
                return value_invalid();
            }
            if (ev.type != VT_INT) {
                runtime_error(rt, expr->line, "slice end must be int, got %s", value_type_name(ev.type));
                return value_invalid();
            }
            end = ev.as.i;
        }

        if (start < 0 || start > max_len) {
            runtime_error(rt, expr->line, "slice start out of range: %lld", start);
            return value_invalid();
        }
        if (end < 0 || end > max_len) {
            runtime_error(rt, expr->line, "slice end out of range: %lld", end);
            return value_invalid();
        }
        if (start > end) {
            runtime_error(rt, expr->line, "slice start cannot be greater than end");
            return value_invalid();
        }

        if (target.type == VT_LIST) {
            ListValue *out = list_new(target.as.list->elem_type, target.as.list->elem_object_type);
            if (!out) {
                runtime_error(rt, expr->line, "out of memory");
                return value_invalid();
            }
            for (long long i = start; i < end; i++) {
                if (!list_push(out, target.as.list->items[i])) {
                    runtime_error(rt, expr->line, "out of memory");
                    return value_invalid();
                }
            }
            return value_list(out);
        }

        size_t out_len = (size_t)(end - start);
        char *buf = (char *)malloc(out_len + 1);
        if (!buf) {
            runtime_error(rt, expr->line, "out of memory");
            return value_invalid();
        }
        if (out_len > 0) {
            memcpy(buf, target.as.s + start, out_len);
        }
        buf[out_len] = '\0';
        Value out = value_string(buf);
        free(buf);
        return out;
    }
    if (expr->kind == EX_IDENT) {
        char *left = NULL;
        char *right = NULL;
        if (split_qualified_name(expr->as.ident_name, &left, &right)) {
            Value obj;
            if (env_get(env, left, &obj)) {
                if (obj.type == VT_OBJECT) {
                    int field_idx = find_object_field_index(obj.as.obj, right);
                    if (field_idx < 0) {
                        runtime_error(rt, expr->line, "type '%s' has no field '%s'", obj.as.obj->type_name, right);
                        free(left);
                        free(right);
                        return value_invalid();
                    }
                    Value out = obj.as.obj->values[field_idx];
                    free(left);
                    free(right);
                    return out;
                }
                if (obj.type == VT_INTERFACE) {
                    runtime_error(rt, expr->line, "field access is not allowed on interface value '%s'", left);
                    free(left);
                    free(right);
                    return value_invalid();
                }
            }

            Module *m = find_module_by_namespace(rt, left);
            if (!m) {
                runtime_error(rt, expr->line, "undefined module namespace '%s'", left);
                free(left);
                free(right);
                return value_invalid();
            }
            if (!module_is_exported(m, right)) {
                runtime_error(rt, expr->line, "'%s' is not exported by module '%s'", right, m->name);
                free(left);
                free(right);
                return value_invalid();
            }

            size_t n1 = strlen(m->namespace);
            size_t n2 = strlen(right);
            char *qualified = (char *)malloc(n1 + n2 + 2);
            if (!qualified) {
                runtime_error(rt, expr->line, "out of memory");
                free(left);
                free(right);
                return value_invalid();
            }
            memcpy(qualified, m->namespace, n1);
            qualified[n1] = '.';
            memcpy(qualified + n1 + 1, right, n2);
            qualified[n1 + n2 + 1] = '\0';

            FuncBinding *fb = find_binding(rt, qualified);
            if (!fb) {
                fb = find_binding(rt, expr->as.ident_name);
            }
            if (fb) {
                Value fv = value_function(fb->fn, fb->globals, fb->owner, expr->as.ident_name);
                if (fv.type == VT_INVALID) {
                    runtime_error(rt, expr->line, "out of memory");
                }
                free(qualified);
                free(left);
                free(right);
                return fv;
            }

            Value v;
            if (!env_get(&m->globals, right, &v)) {
                runtime_error(rt, expr->line, "undefined variable '%s' in module '%s'", right, m->name);
                free(qualified);
                free(left);
                free(right);
                return value_invalid();
            }
            free(qualified);
            free(left);
            free(right);
            return v;
        }

        Value v;
        if (env_get(env, expr->as.ident_name, &v)) {
            return v;
        }

        FuncBinding *binding = find_binding(rt, expr->as.ident_name);
        if (binding) {
            Value fv = value_function(binding->fn, binding->globals, binding->owner, expr->as.ident_name);
            if (fv.type == VT_INVALID) {
                runtime_error(rt, expr->line, "out of memory");
            }
            return fv;
        }

        if (current_module) {
            FuncDecl *local_fn = find_func_in_module(current_module, expr->as.ident_name);
            if (local_fn) {
                Value fv = value_function(local_fn, &current_module->globals, current_module, expr->as.ident_name);
                if (fv.type == VT_INVALID) {
                    runtime_error(rt, expr->line, "out of memory");
                }
                return fv;
            }
        }

        runtime_error(rt, expr->line, "undefined variable '%s'", expr->as.ident_name);
        return value_invalid();
    }
    if (expr->kind == EX_LAMBDA) {
        Env *closure = env_snapshot_visible(env);
        if (!closure) {
            runtime_error(rt, expr->line, "out of memory");
            return value_invalid();
        }
        Value fv = value_function(expr->as.lambda.fn, closure, current_module, "<lambda>");
        if (fv.type == VT_INVALID) {
            runtime_error(rt, expr->line, "out of memory");
            return value_invalid();
        }
        return fv;
    }
    if (expr->kind == EX_UNARY) {
        Value right = eval_expr(rt, current_module, env, expr->as.unary.expr);
        if (rt->has_error) {
            return value_invalid();
        }
        if (expr->as.unary.op == TOK_MINUS) {
            if (right.type == VT_INT) {
                return value_int(-right.as.i);
            }
            if (right.type == VT_FLOAT) {
                return value_float(-right.as.f);
            }
            runtime_error(rt, expr->line, "unary '-' expects int or float, got %s", value_type_name(right.type));
            return value_invalid();
        }
        if (expr->as.unary.op == TOK_NOT) {
            if (right.type != VT_BOOL) {
                runtime_error(rt, expr->line, "'not' expects bool, got %s", value_type_name(right.type));
                return value_invalid();
            }
            return value_bool(!right.as.b);
        }
        runtime_error(rt, expr->line, "unsupported unary operator");
        return value_invalid();
    }
    if (expr->kind == EX_BINARY) {
        return eval_binary(rt, current_module, env, expr);
    }
    if (expr->kind == EX_CALL) {
        return call_function(rt, current_module, env, expr);
    }

    runtime_error(rt, expr->line, "unsupported expression");
    return value_invalid();
}
