        errno = 0;
        double fv = strtod(num, &end);
        if (errno != 0 || !end || *end != '\0') {
            free(num);
            runtime_error(rt, line, "json.decode: invalid number");
            return value_invalid();
        }
        free(num);
        return value_float(fv);
    }

    runtime_error(rt, line, "json.decode: unexpected token");
    return value_invalid();
}

static ValueType g_sort_elem_type = VT_INVALID;

static int sort_value_cmp(const void *a, const void *b) {
    const Value *va = (const Value *)a;
    const Value *vb = (const Value *)b;

    if (g_sort_elem_type == VT_INT) {
        if (va->as.i < vb->as.i) {
            return -1;
        }
        if (va->as.i > vb->as.i) {
            return 1;
        }
        return 0;
    }
    if (g_sort_elem_type == VT_FLOAT) {
        if (va->as.f < vb->as.f) {
            return -1;
        }
        if (va->as.f > vb->as.f) {
            return 1;
        }
        return 0;
    }
    if (g_sort_elem_type == VT_STRING) {
        return strcmp(va->as.s, vb->as.s);
    }
    return 0;
}

static void run_deferred_calls(Runtime *rt, Module *current_module, Env *env, DeferFrame *frame) {
    if (!frame) {
        return;
    }
    int had_error = rt->has_error;
    char saved_err[2048];
    saved_err[0] = '\0';
    if (had_error) {
        snprintf(saved_err, sizeof(saved_err), "%s", rt->err);
    }

    for (int i = frame->count - 1; i >= 0; i--) {
        if (had_error) {
            rt->has_error = 0;
            rt->err[0] = '\0';
        }
        (void)eval_expr(rt, current_module, env, frame->calls[i]);
        if (!had_error && rt->has_error) {
            return;
        }
    }

    if (had_error) {
        rt->has_error = 1;
        snprintf(rt->err, sizeof(rt->err), "%s", saved_err);
    }
}

static int find_param_index(FuncDecl *fn, const char *name) {
    if (!fn || !name) {
        return -1;
    }
    for (int i = 0; i < fn->param_count; i++) {
        if (strcmp(fn->params[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_call_param_index(const char **param_names, int param_count, const char *name) {
    if (!param_names || !name) {
        return -1;
    }
    for (int i = 0; i < param_count; i++) {
        if (strcmp(param_names[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

static int bind_named_call_args(Runtime *rt, Module *current_module, Env *env, Expr *call_expr, const char *display_name,
                                const char **param_names, int param_count, Value *out_values, int *out_set) {
    int next_positional = 0;
    for (int i = 0; i < call_expr->as.call.arg_count; i++) {
        Value v = eval_expr(rt, current_module, env, call_expr->as.call.args[i]);
        if (rt->has_error) {
            return 0;
        }

        const char *arg_name = (call_expr->as.call.arg_names && call_expr->as.call.arg_names[i])
                                   ? call_expr->as.call.arg_names[i]
                                   : NULL;
        if (arg_name && arg_name[0] != '\0') {
            int idx = find_call_param_index(param_names, param_count, arg_name);
            if (idx < 0) {
                runtime_error(rt, call_expr->line, "function '%s' got unexpected named argument '%s'", display_name,
                              arg_name);
                return 0;
            }
            if (out_set[idx]) {
                runtime_error(rt, call_expr->line, "function '%s' got multiple values for argument '%s'", display_name,
                              arg_name);
                return 0;
            }
            out_values[idx] = v;
            out_set[idx] = 1;
            continue;
        }

        while (next_positional < param_count && out_set[next_positional]) {
            next_positional++;
        }
        if (next_positional >= param_count) {
            runtime_error(rt, call_expr->line, "function '%s' got too many positional arguments", display_name);
            return 0;
        }
        out_values[next_positional] = v;
        out_set[next_positional] = 1;
        next_positional++;
    }
    return 1;
}

static int ensure_required_call_args(Runtime *rt, Expr *call_expr, const char *display_name, const char **param_names,
                                     int required_count, int *arg_set) {
    for (int i = 0; i < required_count; i++) {
        if (arg_set[i]) {
            continue;
        }
        runtime_error(rt, call_expr->line, "function '%s' missing required argument '%s'", display_name, param_names[i]);
        return 0;
    }
    return 1;
}

static Value invoke_user_function_with_args(Runtime *rt, const char *display_name, FuncDecl *fn, Env *fn_parent_env,
                                            Module *fn_owner, Value *receiver_or_null, Value *args, char **arg_names,
                                            int arg_count, int call_line) {
    int frame_pushed = 0;
    int defer_pushed = 0;
    Value final_value = value_invalid();
    Value *bound_values = NULL;
    int *bound_set = NULL;

    if (!push_call_frame(rt, display_name, call_line)) {
        runtime_error(rt, call_line, "out of memory");
        return value_invalid();
    }
    frame_pushed = 1;

    if (arg_count > fn->param_count) {
        runtime_error(rt, call_line, "function '%s' expects at most %d args, got %d", display_name, fn->param_count,
                      arg_count);
        goto done;
    }

    if (fn->param_count > 0) {
        bound_values = (Value *)calloc((size_t)fn->param_count, sizeof(Value));
        bound_set = (int *)calloc((size_t)fn->param_count, sizeof(int));
        if (!bound_values || !bound_set) {
            runtime_error(rt, call_line, "out of memory");
            goto done;
        }
    }

    Env local;
    env_init(&local, fn_parent_env);

    if (receiver_or_null) {
        char env_err[256];
        if (!env_set(&local, fn->receiver.name, *receiver_or_null, env_err, sizeof(env_err))) {
            runtime_error(rt, call_line, "%s", env_err);
            goto done;
        }
    }

    int next_positional = 0;
    for (int i = 0; i < arg_count; i++) {
        const char *arg_name = arg_names ? arg_names[i] : NULL;
        if (arg_name && arg_name[0] != '\0') {
            int param_idx = find_param_index(fn, arg_name);
            if (param_idx < 0) {
                runtime_error(rt, call_line, "function '%s' got unexpected named argument '%s'", display_name, arg_name);
                goto done;
            }
            if (bound_set[param_idx]) {
                runtime_error(rt, call_line, "function '%s' got multiple values for argument '%s'", display_name,
                              arg_name);
                goto done;
            }
            bound_values[param_idx] = args[i];
            bound_set[param_idx] = 1;
            continue;
        }

        while (next_positional < fn->param_count && bound_set[next_positional]) {
            next_positional++;
        }
        if (next_positional >= fn->param_count) {
            runtime_error(rt, call_line, "function '%s' got too many positional arguments", display_name);
            goto done;
        }
        if (fn->params[next_positional].is_kw_only) {
            runtime_error(rt, call_line, "function '%s' got positional argument for keyword-only parameter '%s'",
                          display_name, fn->params[next_positional].name);
            goto done;
        }
        bound_values[next_positional] = args[i];
        bound_set[next_positional] = 1;
        next_positional++;
    }

    for (int i = 0; i < fn->param_count; i++) {
        Value arg_value = value_invalid();
        if (bound_set[i]) {
            arg_value = bound_values[i];
        } else if (fn->params[i].default_expr) {
            arg_value = eval_expr(rt, fn_owner, &local, fn->params[i].default_expr);
            if (rt->has_error) {
                goto done;
            }
        } else {
            runtime_error(rt, call_line, "function '%s' missing required argument '%s'", display_name,
                          fn->params[i].name);
            goto done;
        }

        Value checked_arg = value_invalid();
        if (!check_value_against_type(rt, fn_owner, call_line, arg_value, fn->params[i].type, fn->params[i].name,
                                      &checked_arg)) {
            goto done;
        }
        char env_err[256];
        if (!env_set(&local, fn->params[i].name, checked_arg, env_err, sizeof(env_err))) {
            runtime_error(rt, call_line, "%s", env_err);
            goto done;
        }
    }

    if (!push_defer_frame(rt)) {
        runtime_error(rt, call_line, "out of memory");
        goto done;
    }
    defer_pushed = 1;

    for (int i = 0; i < fn->body_count; i++) {
        ExecResult r = exec_stmt(rt, fn_owner, &local, fn->body[i]);
        if (rt->has_error) {
            goto done;
        }
        if (r.returned) {
            Value checked_ret = value_invalid();
            if (!check_value_against_type(rt, fn_owner, fn->body[i]->line, r.value, fn->return_type, "return value",
                                          &checked_ret)) {
                goto done;
            }
            final_value = checked_ret;
            goto done;
        }
        if (r.broke) {
            if (r.loop_label) {
                runtime_error(rt, fn->body[i]->line, "unknown loop label '%s' for break", r.loop_label);
            } else {
                runtime_error(rt, fn->body[i]->line, "break is only allowed inside a loop");
            }
            goto done;
        }
        if (r.continued) {
            if (r.loop_label) {
                runtime_error(rt, fn->body[i]->line, "unknown loop label '%s' for continue", r.loop_label);
            } else {
                runtime_error(rt, fn->body[i]->line, "continue is only allowed inside a loop");
            }
            goto done;
        }
    }

    if (fn->return_type.kind != VT_VOID) {
        char exp_buf[128];
        runtime_error(rt, fn->line, "function '%s' must return %s", display_name,
                      type_ref_name(fn->return_type, exp_buf, sizeof(exp_buf)));
        goto done;
    }

    final_value = value_void();

done:
    free(bound_values);
    free(bound_set);
    if (defer_pushed) {
        DeferFrame *frame = current_defer_frame(rt);
        run_deferred_calls(rt, fn_owner, &local, frame);
        pop_defer_frame(rt);
    }
    if (frame_pushed) {
        pop_call_frame(rt);
    }
    if (rt->has_error) {
        return value_invalid();
    }
    return final_value;
}

static Value invoke_user_function(Runtime *rt, Module *current_module, Env *caller_env, Expr *call_expr,
                                  const char *display_name, FuncDecl *fn, Env *fn_parent_env, Module *fn_owner,
                                  Value *receiver_or_null) {
    int call_line = call_expr ? call_expr->line : fn->line;
    Value *args = NULL;
    char **arg_names = NULL;
    int arg_count = call_expr ? call_expr->as.call.arg_count : 0;
    if (arg_count > 0) {
        args = (Value *)calloc((size_t)arg_count, sizeof(Value));
        if (!args) {
            runtime_error(rt, call_line, "out of memory");
            return value_invalid();
        }
        arg_names = call_expr->as.call.arg_names;
    }
    for (int i = 0; i < arg_count; i++) {
        args[i] = eval_expr(rt, current_module, caller_env, call_expr->as.call.args[i]);
        if (rt->has_error) {
            free(args);
            return value_invalid();
        }
    }

    Value out = invoke_user_function_with_args(rt, display_name, fn, fn_parent_env, fn_owner, receiver_or_null, args,
                                               arg_names, arg_count, call_line);
    free(args);
    return out;
}

typedef struct {
    Runtime seed;
    KostroutineTask task;
    int failed;
    char err[2048];
} KostroutineThreadCtx;

static void *kostroutine_worker(void *arg);

static int schedule_kostroutine_call(Runtime *rt, Module *current_module, Env *env, Expr *call_expr, int line) {
    if (!call_expr || call_expr->kind != EX_CALL || !call_expr->as.call.callee ||
        call_expr->as.call.callee->kind != EX_IDENT) {
        runtime_error(rt, line, "kostroutine expects function call by name reference");
        return 0;
    }

    const char *name = call_expr->as.call.callee->as.ident_name;
    Value callee = value_invalid();
    int found = 0;

    if (strchr(name, '.') == NULL) {
        Value local_callee;
        if (env_get(env, name, &local_callee)) {
            callee = local_callee;
            found = 1;
        }
    }

    if (!found) {
        FuncBinding *binding = find_binding(rt, name);
        if (binding) {
            callee = value_function(binding->fn, binding->globals, binding->owner, name);
            if (callee.type == VT_INVALID) {
                runtime_error(rt, line, "out of memory");
                return 0;
            }
            found = 1;
        }
    }

    if (!found && current_module && strchr(name, '.') == NULL) {
        FuncDecl *local_fn = find_func_in_module(current_module, name);
        if (local_fn) {
            callee = value_function(local_fn, &current_module->globals, current_module, name);
            if (callee.type == VT_INVALID) {
                runtime_error(rt, line, "out of memory");
                return 0;
            }
            found = 1;
        }
    }

    if (!found) {
        runtime_error(rt, line, "undefined function '%s'", name);
        return 0;
    }
    if (callee.type != VT_FUNCTION || callee.as.func == NULL || callee.as.func->fn == NULL) {
        runtime_error(rt, line, "kostroutine target must be function, got %s", value_type_name(callee.type));
        return 0;
    }

    int arg_count = call_expr->as.call.arg_count;
    Value *args = NULL;
    char **arg_names = NULL;
    if (arg_count > 0) {
        args = (Value *)calloc((size_t)arg_count, sizeof(Value));
        if (!args) {
            runtime_error(rt, line, "out of memory");
            return 0;
        }
        arg_names = (char **)calloc((size_t)arg_count, sizeof(char *));
        if (!arg_names) {
            free(args);
            runtime_error(rt, line, "out of memory");
            return 0;
        }
    }
    for (int i = 0; i < arg_count; i++) {
        args[i] = eval_expr(rt, current_module, env, call_expr->as.call.args[i]);
        if (rt->has_error) {
            if (arg_names) {
                for (int j = 0; j < arg_count; j++) {
                    free(arg_names[j]);
                }
            }
            free(arg_names);
            free(args);
            return 0;
        }
        if (arg_names && call_expr->as.call.arg_names && call_expr->as.call.arg_names[i]) {
            arg_names[i] = xstrdup(call_expr->as.call.arg_names[i]);
            if (!arg_names[i]) {
                for (int j = 0; j < i; j++) {
                    free(arg_names[j]);
                }
                free(arg_names);
                free(args);
                runtime_error(rt, line, "out of memory");
                return 0;
            }
        }
    }

    KostroutineTask task;
    memset(&task, 0, sizeof(task));
    task.display_name = xstrdup(callee.as.func->display_name ? callee.as.func->display_name : name);
    task.fn = callee.as.func->fn;
    task.fn_parent_env =
        callee.as.func->closure ? callee.as.func->closure : (callee.as.func->owner ? &callee.as.func->owner->globals : env);
    task.fn_owner = callee.as.func->owner ? callee.as.func->owner : current_module;
    task.args = args;
    task.arg_names = arg_names;
    task.arg_count = arg_count;
    task.line = line;

    if (!task.fn_owner) {
        free_kostroutine_task(&task);
        runtime_error(rt, line, "cannot resolve function owner for kostroutine '%s'", name);
        return 0;
    }

    KostroutineThreadCtx *ctx = (KostroutineThreadCtx *)calloc(1, sizeof(KostroutineThreadCtx));
    if (!ctx) {
        free_kostroutine_task(&task);
        runtime_error(rt, line, "out of memory");
        return 0;
    }
    ctx->seed = *rt;
    ctx->task = task;

    pthread_t th;
    if (pthread_create(&th, NULL, kostroutine_worker, ctx) != 0) {
        free_kostroutine_task(&ctx->task);
        free(ctx);
        runtime_error(rt, line, "failed to start kostroutine thread");
        return 0;
    }
    if (!push_kostroutine_thread(rt, th, ctx)) {
        pthread_join(th, NULL);
        free_kostroutine_task(&ctx->task);
        free(ctx);
        runtime_error(rt, line, "out of memory");
        return 0;
    }
    return 1;
}

static void *kostroutine_worker(void *arg) {
    KostroutineThreadCtx *ctx = (KostroutineThreadCtx *)arg;
    Runtime rt_local = ctx->seed;
    runtime_set_current(&rt_local);
    rt_local.has_error = 0;
    rt_local.err[0] = '\0';
    rt_local.call_frames = NULL;
    rt_local.call_frame_count = 0;
    rt_local.call_frame_cap = 0;
    rt_local.defer_frames = NULL;
    rt_local.defer_frame_count = 0;
    rt_local.defer_frame_cap = 0;
    rt_local.kostroutine_tasks = NULL;
    rt_local.kostroutine_count = 0;
    rt_local.kostroutine_cap = 0;
    rt_local.kostroutine_threads = NULL;
    rt_local.kostroutine_thread_ctxs = NULL;
    rt_local.kostroutine_thread_count = 0;
    rt_local.kostroutine_thread_cap = 0;
    rt_local.kostroutine_draining = 0;

    (void)invoke_user_function_with_args(&rt_local, ctx->task.display_name ? ctx->task.display_name : "<kostroutine>",
                                         ctx->task.fn, ctx->task.fn_parent_env, ctx->task.fn_owner, NULL, ctx->task.args,
                                         ctx->task.arg_names, ctx->task.arg_count, ctx->task.line);
    if (!rt_local.has_error) {
        (void)run_kostroutines(&rt_local);
    }
    if (rt_local.has_error) {
        ctx->failed = 1;
        snprintf(ctx->err, sizeof(ctx->err), "%s", rt_local.err);
    }
    runtime_set_current(NULL);
    return NULL;
}

static int run_kostroutines(Runtime *rt) {
    if (rt->kostroutine_draining) {
        return 1;
    }
    rt->kostroutine_draining = 1;

    int total_threads = rt->kostroutine_thread_count;
    if (total_threads <= 0) {
        rt->kostroutine_draining = 0;
        return 1;
    }
    for (int i = 0; i < total_threads; i++) {
        pthread_join(rt->kostroutine_threads[i], NULL);
    }

    if (!rt->has_error) {
        for (int i = 0; i < total_threads; i++) {
            KostroutineThreadCtx *ctx = (KostroutineThreadCtx *)rt->kostroutine_thread_ctxs[i];
            if (ctx && ctx->failed) {
                rt->has_error = 1;
                snprintf(rt->err, sizeof(rt->err), "%s", ctx->err);
                break;
            }
        }
    }

    for (int i = 0; i < total_threads; i++) {
        KostroutineThreadCtx *ctx = (KostroutineThreadCtx *)rt->kostroutine_thread_ctxs[i];
        if (ctx) {
            free_kostroutine_task(&ctx->task);
            free(ctx);
            rt->kostroutine_thread_ctxs[i] = NULL;
        }
    }
    rt->kostroutine_thread_count = 0;

    rt->kostroutine_draining = 0;
    return !rt->has_error;
}

typedef struct {
    ChannelValue *ch;
    long long delay_ms;
} TimeAfterTask;

static void *time_after_worker(void *arg) {
    TimeAfterTask *task = (TimeAfterTask *)arg;
    if (task->delay_ms > 0) {
        runtime_sleep_ms(task->delay_ms);
    }
    char ch_err[128];
    (void)channel_send(task->ch, value_bool(1), 0, ch_err, sizeof(ch_err));
    (void)channel_close(task->ch, ch_err, sizeof(ch_err));
    free(task);
    return NULL;
}

static double approx_sqrt(double x) {
    if (x <= 0.0) {
        return 0.0;
    }
    double guess = x > 1.0 ? x : 1.0;
    for (int i = 0; i < 32; i++) {
        guess = 0.5 * (guess + x / guess);
    }
    return guess;
}

static Value native_math_call(Runtime *rt, Module *current_module, Env *env, Expr *call_expr, const char *fn_name) {
    if (strcmp(fn_name, "abs") == 0) {
        if (call_expr->as.call.arg_count != 1) {
            runtime_error(rt, call_expr->line, "function 'math.abs' expects 1 arg, got %d", call_expr->as.call.arg_count);
            return value_invalid();
        }
        Value in = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (in.type == VT_INT) {
            return value_int(in.as.i < 0 ? -in.as.i : in.as.i);
        }
        if (in.type == VT_FLOAT) {
            return value_float(in.as.f < 0 ? -in.as.f : in.as.f);
        }
        runtime_error(rt, call_expr->line, "math.abs(...) expects int or float, got %s", value_type_name(in.type));
        return value_invalid();
    }
    if (strcmp(fn_name, "sqrt") == 0) {
        if (call_expr->as.call.arg_count != 1) {
            runtime_error(rt, call_expr->line, "function 'math.sqrt' expects 1 arg, got %d", call_expr->as.call.arg_count);
            return value_invalid();
        }
        Value in = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (!is_number(in.type)) {
            runtime_error(rt, call_expr->line, "math.sqrt(...) expects int or float, got %s", value_type_name(in.type));
            return value_invalid();
        }
        double d = to_double(in);
        if (d < 0) {
            runtime_error(rt, call_expr->line, "math.sqrt(...) expects non-negative number");
            return value_invalid();
        }
        return value_float(approx_sqrt(d));
    }
    if (strcmp(fn_name, "min") == 0 || strcmp(fn_name, "max") == 0) {
        if (call_expr->as.call.arg_count != 2) {
            runtime_error(rt, call_expr->line, "function 'math.%s' expects 2 args, got %d", fn_name,
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        Value a = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        Value b = eval_expr(rt, current_module, env, call_expr->as.call.args[1]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (!is_number(a.type) || !is_number(b.type)) {
            runtime_error(rt, call_expr->line, "math.%s(...) expects numeric args, got %s and %s", fn_name,
                          value_type_name(a.type), value_type_name(b.type));
            return value_invalid();
        }
        double da = to_double(a);
        double db = to_double(b);
        double out = strcmp(fn_name, "min") == 0 ? (da < db ? da : db) : (da > db ? da : db);
        if (a.type == VT_INT && b.type == VT_INT) {
            return value_int((long long)out);
        }
        return value_float(out);
    }
    runtime_error(rt, call_expr->line, "undefined function 'math.%s'", fn_name);
    return value_invalid();
}

static Value native_time_call(Runtime *rt, Module *current_module, Env *env, Expr *call_expr, const char *fn_name) {
    if (strcmp(fn_name, "now_unix") == 0) {
        if (call_expr->as.call.arg_count != 0) {
            runtime_error(rt, call_expr->line, "function 'time.now_unix' expects 0 args, got %d",
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        if (rt->check_only) {
            return value_int(0);
        }
        time_t now = time(NULL);
        return value_int((long long)now);
    }
    if (strcmp(fn_name, "now_ms") == 0) {
        if (call_expr->as.call.arg_count != 0) {
            runtime_error(rt, call_expr->line, "function 'time.now_ms' expects 0 args, got %d",
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        if (rt->check_only) {
            return value_int(0);
        }
        return value_int(runtime_now_ms());
    }
    if (strcmp(fn_name, "sleep") == 0) {
        if (call_expr->as.call.arg_count != 1) {
            runtime_error(rt, call_expr->line, "function 'time.sleep' expects 1 arg, got %d",
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        const char *param_names[1] = {"ms"};
        Value args_v[1];
        int args_set[1] = {0};
        if (!bind_named_call_args(rt, current_module, env, call_expr, "time.sleep", param_names, 1, args_v, args_set)) {
            return value_invalid();
        }
        if (!ensure_required_call_args(rt, call_expr, "time.sleep", param_names, 1, args_set)) {
            return value_invalid();
        }
        Value ms_v = args_v[0];
        if (ms_v.type != VT_INT) {
            runtime_error(rt, call_expr->line, "time.sleep(...) expects int milliseconds, got %s",
                          value_type_name(ms_v.type));
            return value_invalid();
        }
        if (ms_v.as.i < 0) {
            runtime_error(rt, call_expr->line, "time.sleep(...) expects non-negative milliseconds, got %lld", ms_v.as.i);
            return value_invalid();
        }
        if (!rt->check_only) {
            runtime_sleep_ms(ms_v.as.i);
        }
        return value_void();
    }
    if (strcmp(fn_name, "after") == 0) {
        if (call_expr->as.call.arg_count != 1) {
            runtime_error(rt, call_expr->line, "function 'time.after' expects 1 arg, got %d",
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        const char *param_names[1] = {"ms"};
        Value args_v[1];
        int args_set[1] = {0};
        if (!bind_named_call_args(rt, current_module, env, call_expr, "time.after", param_names, 1, args_v, args_set)) {
            return value_invalid();
        }
        if (!ensure_required_call_args(rt, call_expr, "time.after", param_names, 1, args_set)) {
            return value_invalid();
        }
        Value ms_v = args_v[0];
        if (ms_v.type != VT_INT) {
            runtime_error(rt, call_expr->line, "time.after(...) expects int milliseconds, got %s",
                          value_type_name(ms_v.type));
            return value_invalid();
        }
        if (ms_v.as.i < 0) {
            runtime_error(rt, call_expr->line, "time.after(...) expects non-negative milliseconds, got %lld", ms_v.as.i);
            return value_invalid();
        }

        ChannelValue *ch = channel_new(1);
        if (!ch) {
            runtime_error(rt, call_expr->line, "out of memory");
            return value_invalid();
        }

        if (rt->check_only) {
            return value_channel(ch);
        }

        TimeAfterTask *task = (TimeAfterTask *)calloc(1, sizeof(TimeAfterTask));
        if (!task) {
            runtime_error(rt, call_expr->line, "out of memory");
            return value_invalid();
        }
        task->ch = ch;
        task->delay_ms = ms_v.as.i;

        pthread_t th;
        if (pthread_create(&th, NULL, time_after_worker, task) != 0) {
            free(task);
            runtime_error(rt, call_expr->line, "failed to start timer thread");
            return value_invalid();
        }
        pthread_detach(th);
        return value_channel(ch);
    }
    runtime_error(rt, call_expr->line, "undefined function 'time.%s'", fn_name);
    return value_invalid();
}
