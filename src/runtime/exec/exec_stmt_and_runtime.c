static ExecResult exec_stmt(Runtime *rt, Module *current_module, Env *env, Stmt *stmt) {
    ExecResult out;
    out.returned = 0;
    out.broke = 0;
    out.continued = 0;
    out.loop_label = NULL;
    out.value = value_void();

    if (!runtime_debug_before_stmt(rt, current_module, env, stmt)) {
        return out;
    }
    if (rt->has_error) {
        return out;
    }

    if (stmt->kind == ST_INC) {
        Value current;
        if (!env_get(env, stmt->as.inc.name, &current)) {
            runtime_error(rt, stmt->line, "undefined variable '%s'", stmt->as.inc.name);
            return out;
        }

        Value next;
        if (current.type == VT_INT) {
            next = value_int(current.as.i + 1);
        } else if (current.type == VT_FLOAT) {
            next = value_float(current.as.f + 1.0);
        } else {
            runtime_error(rt, stmt->line, "increment expects int or float, got %s", value_type_name(current.type));
            return out;
        }

        char env_err[256];
        if (!env_set(env, stmt->as.inc.name, next, env_err, sizeof(env_err))) {
            runtime_error(rt, stmt->line, "%s", env_err);
            return out;
        }
        return out;
    }

    if (stmt->kind == ST_CONST) {
        Value v = eval_expr(rt, current_module, env, stmt->as.const_assign.expr);
        if (rt->has_error) {
            return out;
        }
        char env_err[256];
        if (!env_set_const(env, stmt->as.const_assign.name, v, env_err, sizeof(env_err))) {
            runtime_error(rt, stmt->line, "%s", env_err);
            return out;
        }
        return out;
    }

    if (stmt->kind == ST_ASSIGN) {
        Value v = eval_expr(rt, current_module, env, stmt->as.assign.expr);
        if (rt->has_error) {
            return out;
        }

        Value current;
        if (env_get(env, stmt->as.assign.name, &current) && current.type == VT_INTERFACE && current.as.iface != NULL &&
            current.as.iface->interface_name != NULL) {
            TypeRef expected_iface;
            memset(&expected_iface, 0, sizeof(expected_iface));
            expected_iface.kind = VT_OBJECT;
            expected_iface.custom_name = current.as.iface->interface_name;
            expected_iface.func_sig = NULL;
            Value checked = value_invalid();
            if (!check_value_against_type(rt, current_module, stmt->line, v, expected_iface, stmt->as.assign.name,
                                          &checked)) {
                return out;
            }
            v = checked;
        }

        char env_err[256];
        if (!env_set(env, stmt->as.assign.name, v, env_err, sizeof(env_err))) {
            runtime_error(rt, stmt->line, "%s", env_err);
            return out;
        }
        return out;
    }

    if (stmt->kind == ST_MULTI_ASSIGN) {
        Value rhs = eval_expr(rt, current_module, env, stmt->as.multi_assign.expr);
        if (rt->has_error) {
            return out;
        }
        if (rhs.type != VT_MULTI || rhs.as.multi == NULL) {
            runtime_error(rt, stmt->line, "multi-assignment expects multi value, got %s", value_type_name(rhs.type));
            return out;
        }
        if (rhs.as.multi->count != stmt->as.multi_assign.name_count) {
            runtime_error(rt, stmt->line, "multi-assignment expects %d values, got %d", stmt->as.multi_assign.name_count,
                          rhs.as.multi->count);
            return out;
        }

        for (int i = 0; i < stmt->as.multi_assign.name_count; i++) {
            char env_err[256];
            if (!env_set(env, stmt->as.multi_assign.names[i], rhs.as.multi->items[i], env_err, sizeof(env_err))) {
                runtime_error(rt, stmt->line, "%s", env_err);
                return out;
            }
        }
        return out;
    }

    if (stmt->kind == ST_FIELD_ASSIGN) {
        Value obj;
        if (!env_get(env, stmt->as.field_assign.obj_name, &obj)) {
            runtime_error(rt, stmt->line, "undefined variable '%s'", stmt->as.field_assign.obj_name);
            return out;
        }
        if (obj.type == VT_INTERFACE) {
            runtime_error(rt, stmt->line, "field assignment is not allowed on interface value '%s'",
                          stmt->as.field_assign.obj_name);
            return out;
        }
        if (obj.type != VT_OBJECT) {
            runtime_error(rt, stmt->line, "field assignment target must be object, got %s", value_type_name(obj.type));
            return out;
        }

        int field_idx = find_object_field_index(obj.as.obj, stmt->as.field_assign.field_name);
        if (field_idx < 0) {
            runtime_error(rt, stmt->line, "type '%s' has no field '%s'", obj.as.obj->type_name,
                          stmt->as.field_assign.field_name);
            return out;
        }

        Value rhs = eval_expr(rt, current_module, env, stmt->as.field_assign.expr);
        if (rt->has_error) {
            return out;
        }
        Value checked_rhs = value_invalid();
        if (!check_value_against_type(rt, current_module, stmt->line, rhs, obj.as.obj->fields[field_idx].type,
                                      stmt->as.field_assign.field_name, &checked_rhs)) {
            return out;
        }

        obj.as.obj->values[field_idx] = checked_rhs;
        return out;
    }

    if (stmt->kind == ST_INDEX_ASSIGN) {
        Value *target = env_get_mut(env, stmt->as.index_assign.name);
        if (!target) {
            runtime_error(rt, stmt->line, "undefined variable '%s'", stmt->as.index_assign.name);
            return out;
        }

        Value idx = eval_expr(rt, current_module, env, stmt->as.index_assign.index);
        if (rt->has_error) {
            return out;
        }

        Value rhs = eval_expr(rt, current_module, env, stmt->as.index_assign.expr);
        if (rt->has_error) {
            return out;
        }
        if (target->type == VT_LIST) {
            if (idx.type != VT_INT) {
                runtime_error(rt, stmt->line, "list index must be int, got %s", value_type_name(idx.type));
                return out;
            }
            if (idx.as.i < 0 || idx.as.i >= target->as.list->count) {
                runtime_error(rt, stmt->line, "list index out of range: %lld", idx.as.i);
                return out;
            }
            if (target->as.list->elem_type != VT_INVALID && rhs.type != target->as.list->elem_type) {
                runtime_error(rt, stmt->line, "type mismatch for list '%s': expected %s, got %s",
                              stmt->as.index_assign.name, value_type_name(target->as.list->elem_type),
                              value_type_name(rhs.type));
                return out;
            }
            if (target->as.list->elem_type == VT_OBJECT && target->as.list->elem_object_type != NULL) {
                if (rhs.type != VT_OBJECT || rhs.as.obj == NULL ||
                    strcmp(rhs.as.obj->type_name, target->as.list->elem_object_type) != 0) {
                    runtime_error(rt, stmt->line, "type mismatch for list '%s': expected %s, got %s",
                                  stmt->as.index_assign.name, target->as.list->elem_object_type,
                                  rhs.type == VT_OBJECT && rhs.as.obj ? rhs.as.obj->type_name : value_type_name(rhs.type));
                    return out;
                }
            }

            target->as.list->items[idx.as.i] = rhs;
            return out;
        }

        if (target->type == VT_MAP) {
            if (idx.type != VT_STRING) {
                runtime_error(rt, stmt->line, "map index must be string, got %s", value_type_name(idx.type));
                return out;
            }
            if (rhs.type == VT_VOID || rhs.type == VT_INVALID) {
                runtime_error(rt, stmt->line, "map value cannot be %s", value_type_name(rhs.type));
                return out;
            }
            if (target->as.map->value_type == VT_INVALID) {
                target->as.map->value_type = rhs.type;
                if (rhs.type == VT_OBJECT && rhs.as.obj != NULL) {
                    target->as.map->value_object_type = xstrdup(rhs.as.obj->type_name);
                }
            } else if (target->as.map->value_type != rhs.type) {
                runtime_error(rt, stmt->line, "type mismatch for map '%s': expected %s, got %s",
                              stmt->as.index_assign.name, value_type_name(target->as.map->value_type),
                              value_type_name(rhs.type));
                return out;
            } else if (target->as.map->value_type == VT_OBJECT && target->as.map->value_object_type != NULL &&
                       (rhs.type != VT_OBJECT || rhs.as.obj == NULL ||
                        strcmp(rhs.as.obj->type_name, target->as.map->value_object_type) != 0)) {
                runtime_error(rt, stmt->line, "type mismatch for map '%s': expected %s, got %s",
                              stmt->as.index_assign.name, target->as.map->value_object_type,
                              rhs.type == VT_OBJECT && rhs.as.obj ? rhs.as.obj->type_name : value_type_name(rhs.type));
                return out;
            }
            if (!map_set(target->as.map, idx.as.s, rhs)) {
                runtime_error(rt, stmt->line, "out of memory");
                return out;
            }
            return out;
        }

        runtime_error(rt, stmt->line, "index assignment target must be list or map, got %s", value_type_name(target->type));
        return out;
    }

    if (stmt->kind == ST_BREAK) {
        out.broke = 1;
        if (stmt->as.break_stmt.has_label) {
            out.loop_label = stmt->as.break_stmt.label;
        }
        return out;
    }

    if (stmt->kind == ST_CONTINUE) {
        out.continued = 1;
        if (stmt->as.continue_stmt.has_label) {
            out.loop_label = stmt->as.continue_stmt.label;
        }
        return out;
    }

    if (stmt->kind == ST_DEFER) {
        DeferFrame *frame = current_defer_frame(rt);
        if (!frame) {
            runtime_error(rt, stmt->line, "defer is only allowed inside a function");
            return out;
        }
        if (!stmt->as.defer_stmt.expr || stmt->as.defer_stmt.expr->kind != EX_CALL) {
            runtime_error(rt, stmt->line, "defer expects function call");
            return out;
        }
        if (!defer_frame_add_call(frame, stmt->as.defer_stmt.expr)) {
            runtime_error(rt, stmt->line, "out of memory");
            return out;
        }
        return out;
    }

    if (stmt->kind == ST_KOSTROUTINE) {
        if (!schedule_kostroutine_call(rt, current_module, env, stmt->as.kostroutine_stmt.expr, stmt->line)) {
            return out;
        }
        return out;
    }

    if (stmt->kind == ST_RETURN) {
        out.returned = 1;
        if (stmt->as.ret.value_count == 0) {
            out.value = value_void();
            return out;
        }
        if (stmt->as.ret.value_count == 1) {
            out.value = eval_expr(rt, current_module, env, stmt->as.ret.values[0]);
            return out;
        }

        Value *items = (Value *)calloc((size_t)stmt->as.ret.value_count, sizeof(Value));
        if (!items) {
            runtime_error(rt, stmt->line, "out of memory");
            return out;
        }
        for (int i = 0; i < stmt->as.ret.value_count; i++) {
            items[i] = eval_expr(rt, current_module, env, stmt->as.ret.values[i]);
            if (rt->has_error) {
                free(items);
                return out;
            }
        }
        out.value = value_multi(items, stmt->as.ret.value_count);
        free(items);
        if (out.value.type == VT_INVALID) {
            runtime_error(rt, stmt->line, "out of memory");
            return out;
        }
        return out;
    }

    if (stmt->kind == ST_EXPR) {
        (void)eval_expr(rt, current_module, env, stmt->as.expr_stmt.expr);
        return out;
    }

    if (stmt->kind == ST_IF) {
        for (int i = 0; i < stmt->as.if_stmt.branch_count; i++) {
            IfBranch *branch = &stmt->as.if_stmt.branches[i];
            Value cond = eval_expr(rt, current_module, env, branch->cond);
            if (rt->has_error) {
                return out;
            }
            if (cond.type != VT_BOOL) {
                runtime_error(rt, branch->line, "if condition must be bool, got %s", value_type_name(cond.type));
                return out;
            }
            if (!cond.as.b) {
                continue;
            }

            for (int body_i = 0; body_i < branch->body_count; body_i++) {
                ExecResult r = exec_stmt(rt, current_module, env, branch->body[body_i]);
                if (rt->has_error) {
                    return out;
                }
                if (r.returned) {
                    return r;
                }
                if (r.broke || r.continued) {
                    return r;
                }
            }
            return out;
        }

        if (stmt->as.if_stmt.has_else) {
            for (int body_i = 0; body_i < stmt->as.if_stmt.else_count; body_i++) {
                ExecResult r = exec_stmt(rt, current_module, env, stmt->as.if_stmt.else_body[body_i]);
                if (rt->has_error) {
                    return out;
                }
                if (r.returned) {
                    return r;
                }
                if (r.broke || r.continued) {
                    return r;
                }
            }
        }
        return out;
    }

    if (stmt->kind == ST_MATCH) {
        Value target = eval_expr(rt, current_module, env, stmt->as.match_stmt.target);
        if (rt->has_error) {
            return out;
        }

        for (int i = 0; i < stmt->as.match_stmt.case_count; i++) {
            MatchCase *mc = &stmt->as.match_stmt.cases[i];
            Value pattern = eval_expr(rt, current_module, env, mc->pattern);
            if (rt->has_error) {
                return out;
            }

            int ok = 0;
            int eq = values_equal(target, pattern, &ok);
            if (!ok) {
                runtime_error(rt, mc->line, "match case expects same types or numeric mix, got %s and %s",
                              value_type_name(target.type), value_type_name(pattern.type));
                return out;
            }
            if (!eq) {
                continue;
            }

            for (int body_i = 0; body_i < mc->body_count; body_i++) {
                ExecResult r = exec_stmt(rt, current_module, env, mc->body[body_i]);
                if (rt->has_error) {
                    return out;
                }
                if (r.returned) {
                    return r;
                }
                if (r.broke || r.continued) {
                    return r;
                }
            }
            return out;
        }

        if (stmt->as.match_stmt.has_default) {
            for (int body_i = 0; body_i < stmt->as.match_stmt.default_count; body_i++) {
                ExecResult r = exec_stmt(rt, current_module, env, stmt->as.match_stmt.default_body[body_i]);
                if (rt->has_error) {
                    return out;
                }
                if (r.returned) {
                    return r;
                }
                if (r.broke || r.continued) {
                    return r;
                }
            }
        }
        return out;
    }

    if (stmt->kind == ST_SELECT) {
        int chosen = -1;
        int chosen_has_recv = 0;
        Value chosen_recv_value = value_invalid();
        int case_count = stmt->as.select_stmt.case_count;
        long long select_start_ms = runtime_now_ms();
        long long *timeout_ms = (long long *)calloc((size_t)case_count, sizeof(long long));
        int *timeout_init = (int *)calloc((size_t)case_count, sizeof(int));
        if (!timeout_ms || !timeout_init) {
            free(timeout_ms);
            free(timeout_init);
            runtime_error(rt, stmt->line, "out of memory");
            return out;
        }

        while (chosen < 0) {
            for (int i = 0; i < stmt->as.select_stmt.case_count; i++) {
                SelectCase *sc = &stmt->as.select_stmt.cases[i];
                if (!sc->op_call || sc->op_call->kind != EX_CALL || !sc->op_call->as.call.callee ||
                    sc->op_call->as.call.callee->kind != EX_IDENT) {
                    runtime_error(rt, sc->line, "invalid select case operation");
                    return out;
                }

                if (sc->kind == SELECT_CASE_RECV) {
                    if (sc->op_call->as.call.arg_count != 1) {
                        runtime_error(rt, sc->line, "select recv case expects recv(chan)");
                        return out;
                    }
                    Value chv = eval_expr(rt, current_module, env, sc->op_call->as.call.args[0]);
                    if (rt->has_error) {
                        return out;
                    }
                    if (chv.type != VT_CHANNEL || chv.as.chan == NULL) {
                        runtime_error(rt, sc->line, "select recv expects chan, got %s", value_type_name(chv.type));
                        return out;
                    }

                    Value got = value_invalid();
                    char ch_err[256];
                    int recv_ok = channel_recv(chv.as.chan, 0, &got, ch_err, sizeof(ch_err));
                    if (recv_ok == 1) {
                        chosen = i;
                        chosen_has_recv = 1;
                        chosen_recv_value = got;
                        break;
                    }
                    if (recv_ok == 0) {
                        runtime_error(rt, sc->line, "%s", ch_err);
                        return out;
                    }
                    continue;
                }

                if (sc->kind == SELECT_CASE_SEND) {
                    if (sc->op_call->as.call.arg_count != 2) {
                        runtime_error(rt, sc->line, "select send case expects send(chan, value)");
                        return out;
                    }
                    Value chv = eval_expr(rt, current_module, env, sc->op_call->as.call.args[0]);
                    if (rt->has_error) {
                        return out;
                    }
                    Value item = eval_expr(rt, current_module, env, sc->op_call->as.call.args[1]);
                    if (rt->has_error) {
                        return out;
                    }
                    if (chv.type != VT_CHANNEL || chv.as.chan == NULL) {
                        runtime_error(rt, sc->line, "select send expects chan, got %s", value_type_name(chv.type));
                        return out;
                    }
                    char ch_err[256];
                    if (!channel_send(chv.as.chan, item, 0, ch_err, sizeof(ch_err))) {
                        if (strcmp(ch_err, "channel buffer full") == 0) {
                            continue;
                        }
                        runtime_error(rt, sc->line, "%s", ch_err);
                        return out;
                    }
                    chosen = i;
                    break;
                }

                if (sc->kind == SELECT_CASE_TIMEOUT) {
                    if (sc->op_call->as.call.arg_count != 1) {
                        runtime_error(rt, sc->line, "select timeout case expects timeout(ms)");
                        free(timeout_ms);
                        free(timeout_init);
                        return out;
                    }
                    if (!timeout_init[i]) {
                        Value ms_val = eval_expr(rt, current_module, env, sc->op_call->as.call.args[0]);
                        if (rt->has_error) {
                            free(timeout_ms);
                            free(timeout_init);
                            return out;
                        }
                        if (ms_val.type != VT_INT) {
                            runtime_error(rt, sc->line, "timeout(ms) expects int ms, got %s",
                                          value_type_name(ms_val.type));
                            free(timeout_ms);
                            free(timeout_init);
                            return out;
                        }
                        if (ms_val.as.i < 0) {
                            runtime_error(rt, sc->line, "timeout(ms) expects non-negative ms, got %lld", ms_val.as.i);
                            free(timeout_ms);
                            free(timeout_init);
                            return out;
                        }
                        timeout_ms[i] = ms_val.as.i;
                        timeout_init[i] = 1;
                    }
                    long long elapsed = runtime_now_ms() - select_start_ms;
                    if (elapsed >= timeout_ms[i]) {
                        chosen = i;
                        break;
                    }
                    continue;
                }

                runtime_error(rt, sc->line, "unsupported select case kind");
                free(timeout_ms);
                free(timeout_init);
                return out;
            }

            if (chosen >= 0) {
                break;
            }

            if (stmt->as.select_stmt.has_default) {
                for (int body_i = 0; body_i < stmt->as.select_stmt.default_count; body_i++) {
                    ExecResult r = exec_stmt(rt, current_module, env, stmt->as.select_stmt.default_body[body_i]);
                    if (rt->has_error) {
                        free(timeout_ms);
                        free(timeout_init);
                        return out;
                    }
                    if (r.returned || r.broke || r.continued) {
                        free(timeout_ms);
                        free(timeout_init);
                        return r;
                    }
                }
                free(timeout_ms);
                free(timeout_init);
                return out;
            }
            runtime_sleep_ms(1);
        }

        free(timeout_ms);
        free(timeout_init);

        SelectCase *picked = &stmt->as.select_stmt.cases[chosen];
        if (chosen_has_recv && picked->has_bind) {
            char env_err[256];
            if (!env_set(env, picked->bind_name, chosen_recv_value, env_err, sizeof(env_err))) {
                runtime_error(rt, picked->line, "%s", env_err);
                return out;
            }
        }
        for (int body_i = 0; body_i < picked->body_count; body_i++) {
            ExecResult r = exec_stmt(rt, current_module, env, picked->body[body_i]);
            if (rt->has_error) {
                return out;
            }
            if (r.returned || r.broke || r.continued) {
                return r;
            }
        }
        return out;
    }

    if (stmt->kind == ST_FOR) {
        const char *loop_label = stmt->as.for_stmt.loop_label;

        if (stmt->as.for_stmt.mode == FOR_MODE_RANGE) {
            Value start_v = eval_expr(rt, current_module, env, stmt->as.for_stmt.start);
            if (rt->has_error) {
                return out;
            }
            Value end_v = eval_expr(rt, current_module, env, stmt->as.for_stmt.end);
            if (rt->has_error) {
                return out;
            }
            Value step_v = eval_expr(rt, current_module, env, stmt->as.for_stmt.step);
            if (rt->has_error) {
                return out;
            }

            if (start_v.type != VT_INT || end_v.type != VT_INT || step_v.type != VT_INT) {
                runtime_error(rt, stmt->line, "for range expects int start/end/step, got %s/%s/%s",
                              value_type_name(start_v.type), value_type_name(end_v.type), value_type_name(step_v.type));
                return out;
            }

            long long start = start_v.as.i;
            long long end = end_v.as.i;
            long long step = step_v.as.i;

            if (step == 0) {
                runtime_error(rt, stmt->line, "for loop step cannot be 0");
                return out;
            }

            for (long long i = start; (step > 0) ? (i < end) : (i > end); i += step) {
                char env_err[256];
                if (!env_set(env, stmt->as.for_stmt.var_name, value_int(i), env_err, sizeof(env_err))) {
                    runtime_error(rt, stmt->line, "%s", env_err);
                    return out;
                }

                for (int body_i = 0; body_i < stmt->as.for_stmt.body_count; body_i++) {
                    ExecResult loop_r = exec_stmt(rt, current_module, env, stmt->as.for_stmt.body[body_i]);
                    if (rt->has_error) {
                        return out;
                    }
                    if (loop_r.returned) {
                        return loop_r;
                    }
                    if (loop_r.broke) {
                        if (loop_r.loop_label && (!loop_label || strcmp(loop_r.loop_label, loop_label) != 0)) {
                            return loop_r;
                        }
                        return out;
                    }
                    if (loop_r.continued) {
                        if (loop_r.loop_label && (!loop_label || strcmp(loop_r.loop_label, loop_label) != 0)) {
                            return loop_r;
                        }
                        break;
                    }
                }
            }
            return out;
        }

        Value iterable = eval_expr(rt, current_module, env, stmt->as.for_stmt.iterable);
        if (rt->has_error) {
            return out;
        }

        if (iterable.type == VT_LIST) {
            for (int i = 0; i < iterable.as.list->count; i++) {
                char env_err[256];
                if (stmt->as.for_stmt.has_second_var) {
                    if (!env_set(env, stmt->as.for_stmt.var_name, value_int(i), env_err, sizeof(env_err))) {
                        runtime_error(rt, stmt->line, "%s", env_err);
                        return out;
                    }
                    if (!env_set(env, stmt->as.for_stmt.var_name2, iterable.as.list->items[i], env_err,
                                 sizeof(env_err))) {
                        runtime_error(rt, stmt->line, "%s", env_err);
                        return out;
                    }
                } else if (!env_set(env, stmt->as.for_stmt.var_name, iterable.as.list->items[i], env_err,
                                    sizeof(env_err))) {
                    runtime_error(rt, stmt->line, "%s", env_err);
                    return out;
                }

                for (int body_i = 0; body_i < stmt->as.for_stmt.body_count; body_i++) {
                    ExecResult loop_r = exec_stmt(rt, current_module, env, stmt->as.for_stmt.body[body_i]);
                    if (rt->has_error) {
                        return out;
                    }
                    if (loop_r.returned) {
                        return loop_r;
                    }
                    if (loop_r.broke) {
                        if (loop_r.loop_label && (!loop_label || strcmp(loop_r.loop_label, loop_label) != 0)) {
                            return loop_r;
                        }
                        return out;
                    }
                    if (loop_r.continued) {
                        if (loop_r.loop_label && (!loop_label || strcmp(loop_r.loop_label, loop_label) != 0)) {
                            return loop_r;
                        }
                        break;
                    }
                }
            }
            return out;
        }

        if (iterable.type == VT_MAP) {
            for (int i = 0; i < iterable.as.map->count; i++) {
                char env_err[256];
                Value key = value_string(iterable.as.map->keys[i]);
                if (stmt->as.for_stmt.has_second_var) {
                    if (!env_set(env, stmt->as.for_stmt.var_name, key, env_err, sizeof(env_err))) {
                        runtime_error(rt, stmt->line, "%s", env_err);
                        return out;
                    }
                    if (!env_set(env, stmt->as.for_stmt.var_name2, iterable.as.map->values[i], env_err,
                                 sizeof(env_err))) {
                        runtime_error(rt, stmt->line, "%s", env_err);
                        return out;
                    }
                } else if (!env_set(env, stmt->as.for_stmt.var_name, key, env_err, sizeof(env_err))) {
                    runtime_error(rt, stmt->line, "%s", env_err);
                    return out;
                }

                for (int body_i = 0; body_i < stmt->as.for_stmt.body_count; body_i++) {
                    ExecResult loop_r = exec_stmt(rt, current_module, env, stmt->as.for_stmt.body[body_i]);
                    if (rt->has_error) {
                        return out;
                    }
                    if (loop_r.returned) {
                        return loop_r;
                    }
                    if (loop_r.broke) {
                        if (loop_r.loop_label && (!loop_label || strcmp(loop_r.loop_label, loop_label) != 0)) {
                            return loop_r;
                        }
                        return out;
                    }
                    if (loop_r.continued) {
                        if (loop_r.loop_label && (!loop_label || strcmp(loop_r.loop_label, loop_label) != 0)) {
                            return loop_r;
                        }
                        break;
                    }
                }
            }
            return out;
        }

        runtime_error(rt, stmt->line, "for-in expects list or map, got %s", value_type_name(iterable.type));
        return out;
    }

    if (stmt->kind == ST_WHILE) {
        const char *loop_label = stmt->as.while_stmt.loop_label;
        while (1) {
            Value cond = eval_expr(rt, current_module, env, stmt->as.while_stmt.cond);
            if (rt->has_error) {
                return out;
            }
            if (cond.type != VT_BOOL) {
                runtime_error(rt, stmt->line, "while condition must be bool, got %s", value_type_name(cond.type));
                return out;
            }
            if (!cond.as.b) {
                break;
            }

            for (int body_i = 0; body_i < stmt->as.while_stmt.body_count; body_i++) {
                ExecResult loop_r = exec_stmt(rt, current_module, env, stmt->as.while_stmt.body[body_i]);
                if (rt->has_error) {
                    return out;
                }
                if (loop_r.returned) {
                    return loop_r;
                }
                if (loop_r.broke) {
                    if (loop_r.loop_label && (!loop_label || strcmp(loop_r.loop_label, loop_label) != 0)) {
                        return loop_r;
                    }
                    return out;
                }
                if (loop_r.continued) {
                    if (loop_r.loop_label && (!loop_label || strcmp(loop_r.loop_label, loop_label) != 0)) {
                        return loop_r;
                    }
                    break;
                }
            }
        }
        return out;
    }

    if (stmt->kind == ST_DO_WHILE) {
        const char *loop_label = stmt->as.do_while_stmt.loop_label;
        while (1) {
            for (int body_i = 0; body_i < stmt->as.do_while_stmt.body_count; body_i++) {
                ExecResult loop_r = exec_stmt(rt, current_module, env, stmt->as.do_while_stmt.body[body_i]);
                if (rt->has_error) {
                    return out;
                }
                if (loop_r.returned) {
                    return loop_r;
                }
                if (loop_r.broke) {
                    if (loop_r.loop_label && (!loop_label || strcmp(loop_r.loop_label, loop_label) != 0)) {
                        return loop_r;
                    }
                    return out;
                }
                if (loop_r.continued) {
                    if (loop_r.loop_label && (!loop_label || strcmp(loop_r.loop_label, loop_label) != 0)) {
                        return loop_r;
                    }
                    break;
                }
            }

            Value cond = eval_expr(rt, current_module, env, stmt->as.do_while_stmt.cond);
            if (rt->has_error) {
                return out;
            }
            if (cond.type != VT_BOOL) {
                runtime_error(rt, stmt->line, "do-while condition must be bool, got %s", value_type_name(cond.type));
                return out;
            }
            if (!cond.as.b) {
                break;
            }
        }
        return out;
    }

    runtime_error(rt, stmt->line, "unsupported statement");
    return out;
}

static int run_program_with_options(Program *prog, const char *entry_path, int check_only, int debug_enabled,
                                    const char *breakpoints_csv, int step_mode, char *err, size_t err_cap) {
    Runtime rt;
    memset(&rt, 0, sizeof(rt));
    runtime_set_current(&rt);
    rt.check_only = check_only ? 1 : 0;
    rt.debug_enabled = debug_enabled ? 1 : 0;
    rt.debug_step_mode = step_mode ? 1 : 0;
    if (rt.debug_enabled) {
        if (!debug_parse_breakpoints_csv(&rt, breakpoints_csv, err, err_cap)) {
            cleanup_regex_registry(&rt);
            runtime_set_current(NULL);
            return 0;
        }
    }

    char *entry_dir = dirname_from_path(entry_path);
    Module *main_mod = add_module(&rt, "__main__", "__main__", entry_dir, prog);
    free(entry_dir);
    if (!main_mod) {
        snprintf(err, err_cap, "out of memory");
        cleanup_regex_registry(&rt);
        runtime_set_current(NULL);
        return 0;
    }

    if (!execute_module(&rt, main_mod)) {
        snprintf(err, err_cap, "%s", rt.err);
        cleanup_regex_registry(&rt);
        runtime_set_current(NULL);
        return 0;
    }
    if (!run_kostroutines(&rt)) {
        snprintf(err, err_cap, "%s", rt.err);
        cleanup_regex_registry(&rt);
        runtime_set_current(NULL);
        return 0;
    }

    cleanup_regex_registry(&rt);
    runtime_set_current(NULL);
    return 1;
}
