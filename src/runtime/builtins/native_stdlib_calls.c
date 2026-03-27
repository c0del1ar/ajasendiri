static Value native_fs_call(Runtime *rt, Module *current_module, Env *env, Expr *call_expr, const char *fn_name) {
    if (strcmp(fn_name, "read") == 0) {
        if (call_expr->as.call.arg_count != 1) {
            runtime_error(rt, call_expr->line, "function 'fs.read' expects 1 arg, got %d", call_expr->as.call.arg_count);
            return value_invalid();
        }
        const char *param_names[1] = {"path"};
        Value args_v[1];
        int args_set[1] = {0};
        if (!bind_named_call_args(rt, current_module, env, call_expr, "fs.read", param_names, 1, args_v, args_set)) {
            return value_invalid();
        }
        if (!ensure_required_call_args(rt, call_expr, "fs.read", param_names, 1, args_set)) {
            return value_invalid();
        }
        Value path_v = args_v[0];
        if (path_v.type != VT_STRING) {
            runtime_error(rt, call_expr->line, "fs.read(...) expects string path, got %s", value_type_name(path_v.type));
            return value_invalid();
        }
        if (rt->check_only) {
            return value_string("");
        }
        char io_err[256];
        char *txt = fs_read_text_file(path_v.as.s, io_err, sizeof(io_err));
        if (!txt) {
            runtime_error(rt, call_expr->line, "%s", io_err);
            return value_invalid();
        }
        Value out;
        out.type = VT_STRING;
        out.as.s = txt;
        return out;
    }

    if (strcmp(fn_name, "write") == 0) {
        if (call_expr->as.call.arg_count != 2) {
            runtime_error(rt, call_expr->line, "function 'fs.write' expects 2 args, got %d", call_expr->as.call.arg_count);
            return value_invalid();
        }
        const char *param_names[2] = {"path", "text"};
        Value args_v[2];
        int args_set[2] = {0, 0};
        if (!bind_named_call_args(rt, current_module, env, call_expr, "fs.write", param_names, 2, args_v, args_set)) {
            return value_invalid();
        }
        if (!ensure_required_call_args(rt, call_expr, "fs.write", param_names, 2, args_set)) {
            return value_invalid();
        }
        Value path_v = args_v[0];
        Value txt_v = args_v[1];
        if (path_v.type != VT_STRING) {
            runtime_error(rt, call_expr->line, "arg 1 for 'fs.write' expects string path, got %s",
                          value_type_name(path_v.type));
            return value_invalid();
        }
        if (txt_v.type != VT_STRING) {
            runtime_error(rt, call_expr->line, "arg 2 for 'fs.write' expects string text, got %s",
                          value_type_name(txt_v.type));
            return value_invalid();
        }
        if (rt->check_only) {
            return value_bool(1);
        }
        char io_err[256];
        if (!fs_write_text_file(path_v.as.s, txt_v.as.s, io_err, sizeof(io_err))) {
            runtime_error(rt, call_expr->line, "%s", io_err);
            return value_invalid();
        }
        return value_bool(1);
    }

    if (strcmp(fn_name, "append") == 0) {
        if (call_expr->as.call.arg_count != 2) {
            runtime_error(rt, call_expr->line, "function 'fs.append' expects 2 args, got %d",
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        const char *param_names[2] = {"path", "text"};
        Value args_v[2];
        int args_set[2] = {0, 0};
        if (!bind_named_call_args(rt, current_module, env, call_expr, "fs.append", param_names, 2, args_v, args_set)) {
            return value_invalid();
        }
        if (!ensure_required_call_args(rt, call_expr, "fs.append", param_names, 2, args_set)) {
            return value_invalid();
        }
        Value path_v = args_v[0];
        Value txt_v = args_v[1];
        if (path_v.type != VT_STRING) {
            runtime_error(rt, call_expr->line, "arg 1 for 'fs.append' expects string path, got %s",
                          value_type_name(path_v.type));
            return value_invalid();
        }
        if (txt_v.type != VT_STRING) {
            runtime_error(rt, call_expr->line, "arg 2 for 'fs.append' expects string text, got %s",
                          value_type_name(txt_v.type));
            return value_invalid();
        }
        if (rt->check_only) {
            return value_bool(1);
        }
        char io_err[256];
        if (!fs_append_text_file(path_v.as.s, txt_v.as.s, io_err, sizeof(io_err))) {
            runtime_error(rt, call_expr->line, "%s", io_err);
            return value_invalid();
        }
        return value_bool(1);
    }

    if (strcmp(fn_name, "exists") == 0) {
        if (call_expr->as.call.arg_count != 1) {
            runtime_error(rt, call_expr->line, "function 'fs.exists' expects 1 arg, got %d", call_expr->as.call.arg_count);
            return value_invalid();
        }
        const char *param_names[1] = {"path"};
        Value args_v[1];
        int args_set[1] = {0};
        if (!bind_named_call_args(rt, current_module, env, call_expr, "fs.exists", param_names, 1, args_v, args_set)) {
            return value_invalid();
        }
        if (!ensure_required_call_args(rt, call_expr, "fs.exists", param_names, 1, args_set)) {
            return value_invalid();
        }
        Value path_v = args_v[0];
        if (path_v.type != VT_STRING) {
            runtime_error(rt, call_expr->line, "fs.exists(...) expects string path, got %s", value_type_name(path_v.type));
            return value_invalid();
        }
        if (rt->check_only) {
            return value_bool(0);
        }
        return value_bool(fs_path_exists(path_v.as.s) ? 1 : 0);
    }

    if (strcmp(fn_name, "mkdir") == 0) {
        if (call_expr->as.call.arg_count != 1) {
            runtime_error(rt, call_expr->line, "function 'fs.mkdir' expects 1 arg, got %d", call_expr->as.call.arg_count);
            return value_invalid();
        }
        const char *param_names[1] = {"path"};
        Value args_v[1];
        int args_set[1] = {0};
        if (!bind_named_call_args(rt, current_module, env, call_expr, "fs.mkdir", param_names, 1, args_v, args_set)) {
            return value_invalid();
        }
        if (!ensure_required_call_args(rt, call_expr, "fs.mkdir", param_names, 1, args_set)) {
            return value_invalid();
        }
        Value path_v = args_v[0];
        if (path_v.type != VT_STRING) {
            runtime_error(rt, call_expr->line, "fs.mkdir(...) expects string path, got %s", value_type_name(path_v.type));
            return value_invalid();
        }
        if (rt->check_only) {
            return value_bool(1);
        }
        char io_err[256];
        if (!fs_make_dir(path_v.as.s, io_err, sizeof(io_err))) {
            runtime_error(rt, call_expr->line, "%s", io_err);
            return value_invalid();
        }
        return value_bool(1);
    }

    if (strcmp(fn_name, "remove") == 0) {
        if (call_expr->as.call.arg_count != 1) {
            runtime_error(rt, call_expr->line, "function 'fs.remove' expects 1 arg, got %d", call_expr->as.call.arg_count);
            return value_invalid();
        }
        const char *param_names[1] = {"path"};
        Value args_v[1];
        int args_set[1] = {0};
        if (!bind_named_call_args(rt, current_module, env, call_expr, "fs.remove", param_names, 1, args_v, args_set)) {
            return value_invalid();
        }
        if (!ensure_required_call_args(rt, call_expr, "fs.remove", param_names, 1, args_set)) {
            return value_invalid();
        }
        Value path_v = args_v[0];
        if (path_v.type != VT_STRING) {
            runtime_error(rt, call_expr->line, "fs.remove(...) expects string path, got %s", value_type_name(path_v.type));
            return value_invalid();
        }
        if (rt->check_only) {
            return value_bool(1);
        }
        char io_err[256];
        if (!fs_remove_path(path_v.as.s, io_err, sizeof(io_err))) {
            runtime_error(rt, call_expr->line, "%s", io_err);
            return value_invalid();
        }
        return value_bool(1);
    }

    runtime_error(rt, call_expr->line, "undefined function 'fs.%s'", fn_name);
    return value_invalid();
}

static Value native_os_call(Runtime *rt, Module *current_module, Env *env, Expr *call_expr, const char *fn_name) {
    if (strcmp(fn_name, "cwd") == 0) {
        if (call_expr->as.call.arg_count != 0) {
            runtime_error(rt, call_expr->line, "function 'os.cwd' expects 0 args, got %d", call_expr->as.call.arg_count);
            return value_invalid();
        }
        if (rt->check_only) {
            return value_string("");
        }
        char buf[4096];
        if (!getcwd(buf, sizeof(buf))) {
            runtime_error(rt, call_expr->line, "os.cwd() failed");
            return value_invalid();
        }
        return value_string(buf);
    }

    if (strcmp(fn_name, "chdir") == 0) {
        if (call_expr->as.call.arg_count != 1) {
            runtime_error(rt, call_expr->line, "function 'os.chdir' expects 1 arg, got %d",
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        Value path_v = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (path_v.type != VT_STRING) {
            runtime_error(rt, call_expr->line, "os.chdir(...) expects string path, got %s", value_type_name(path_v.type));
            return value_invalid();
        }
        if (rt->check_only) {
            return value_bool(1);
        }
        if (chdir(path_v.as.s) != 0) {
            runtime_error(rt, call_expr->line, "os.chdir() failed for path: %s", path_v.as.s);
            return value_invalid();
        }
        return value_bool(1);
    }

    if (strcmp(fn_name, "getenv") == 0) {
        if (call_expr->as.call.arg_count != 1) {
            runtime_error(rt, call_expr->line, "function 'os.getenv' expects 1 arg, got %d",
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        Value key_v = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (key_v.type != VT_STRING) {
            runtime_error(rt, call_expr->line, "os.getenv(...) expects string key, got %s", value_type_name(key_v.type));
            return value_invalid();
        }
        if (rt->check_only) {
            return value_string("");
        }
        const char *val = getenv(key_v.as.s);
        return value_string(val ? val : "");
    }

    if (strcmp(fn_name, "setenv") == 0) {
        if (call_expr->as.call.arg_count != 2) {
            runtime_error(rt, call_expr->line, "function 'os.setenv' expects 2 args, got %d",
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        Value key_v = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        Value val_v = eval_expr(rt, current_module, env, call_expr->as.call.args[1]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (key_v.type != VT_STRING) {
            runtime_error(rt, call_expr->line, "arg 1 for 'os.setenv' expects string key, got %s",
                          value_type_name(key_v.type));
            return value_invalid();
        }
        if (val_v.type != VT_STRING) {
            runtime_error(rt, call_expr->line, "arg 2 for 'os.setenv' expects string value, got %s",
                          value_type_name(val_v.type));
            return value_invalid();
        }
        if (rt->check_only) {
            return value_bool(1);
        }
        if (setenv(key_v.as.s, val_v.as.s, 1) != 0) {
            runtime_error(rt, call_expr->line, "os.setenv() failed for key: %s", key_v.as.s);
            return value_invalid();
        }
        return value_bool(1);
    }

    runtime_error(rt, call_expr->line, "undefined function 'os.%s'", fn_name);
    return value_invalid();
}

static Value native_path_call(Runtime *rt, Module *current_module, Env *env, Expr *call_expr, const char *fn_name) {
    if (strcmp(fn_name, "join") == 0) {
        if (call_expr->as.call.arg_count != 2) {
            runtime_error(rt, call_expr->line, "function 'path.join' expects 2 args, got %d",
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
        if (a.type != VT_STRING || b.type != VT_STRING) {
            runtime_error(rt, call_expr->line, "path.join(...) expects string args, got %s and %s",
                          value_type_name(a.type), value_type_name(b.type));
            return value_invalid();
        }
        char *out = path_join_dup(a.as.s, b.as.s);
        if (!out) {
            runtime_error(rt, call_expr->line, "out of memory");
            return value_invalid();
        }
        Value v;
        v.type = VT_STRING;
        v.as.s = out;
        return v;
    }

    if (strcmp(fn_name, "basename") == 0) {
        if (call_expr->as.call.arg_count != 1) {
            runtime_error(rt, call_expr->line, "function 'path.basename' expects 1 arg, got %d",
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        Value in = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (in.type != VT_STRING) {
            runtime_error(rt, call_expr->line, "path.basename(...) expects string path, got %s",
                          value_type_name(in.type));
            return value_invalid();
        }
        Value v;
        v.type = VT_STRING;
        v.as.s = path_basename_dup(in.as.s);
        return v;
    }

    if (strcmp(fn_name, "dirname") == 0) {
        if (call_expr->as.call.arg_count != 1) {
            runtime_error(rt, call_expr->line, "function 'path.dirname' expects 1 arg, got %d",
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        Value in = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (in.type != VT_STRING) {
            runtime_error(rt, call_expr->line, "path.dirname(...) expects string path, got %s", value_type_name(in.type));
            return value_invalid();
        }
        Value v;
        v.type = VT_STRING;
        v.as.s = path_dirname_dup(in.as.s);
        return v;
    }

    if (strcmp(fn_name, "ext") == 0) {
        if (call_expr->as.call.arg_count != 1) {
            runtime_error(rt, call_expr->line, "function 'path.ext' expects 1 arg, got %d", call_expr->as.call.arg_count);
            return value_invalid();
        }
        Value in = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (in.type != VT_STRING) {
            runtime_error(rt, call_expr->line, "path.ext(...) expects string path, got %s", value_type_name(in.type));
            return value_invalid();
        }
        Value v;
        v.type = VT_STRING;
        v.as.s = path_ext_dup(in.as.s);
        return v;
    }

    runtime_error(rt, call_expr->line, "undefined function 'path.%s'", fn_name);
    return value_invalid();
}

static Value native_http_call(Runtime *rt, Module *current_module, Env *env, Expr *call_expr, const char *fn_name) {
    const char *method = NULL;
    int has_body = 0;
    int has_method_arg = 0;
    Value method_v = value_invalid();
    Value url_v = value_invalid();
    Value body_v = value_invalid();
    const char *body_text = "";

    if (strcmp(fn_name, "get") == 0 || strcmp(fn_name, "delete") == 0) {
        if (call_expr->as.call.arg_count != 1) {
            runtime_error(rt, call_expr->line, "function 'http.%s' expects 1 arg, got %d", fn_name,
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        method = strcmp(fn_name, "get") == 0 ? "GET" : "DELETE";
        const char *param_names[1] = {"url"};
        const char *display_name = strcmp(fn_name, "get") == 0 ? "http.get" : "http.delete";
        Value args_v[1];
        int args_set[1] = {0};
        if (!bind_named_call_args(rt, current_module, env, call_expr, display_name, param_names, 1, args_v, args_set)) {
            return value_invalid();
        }
        if (!ensure_required_call_args(rt, call_expr, display_name, param_names, 1, args_set)) {
            return value_invalid();
        }
        url_v = args_v[0];
    } else if (strcmp(fn_name, "post") == 0 || strcmp(fn_name, "put") == 0) {
        if (call_expr->as.call.arg_count != 2) {
            runtime_error(rt, call_expr->line, "function 'http.%s' expects 2 args, got %d", fn_name,
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        method = strcmp(fn_name, "post") == 0 ? "POST" : "PUT";
        has_body = 1;
        const char *param_names[2] = {"url", "body"};
        Value args_v[2];
        int args_set[2] = {0, 0};
        if (!bind_named_call_args(rt, current_module, env, call_expr,
                                  strcmp(fn_name, "post") == 0 ? "http.post" : "http.put", param_names, 2, args_v,
                                  args_set)) {
            return value_invalid();
        }
        if (!ensure_required_call_args(rt, call_expr, strcmp(fn_name, "post") == 0 ? "http.post" : "http.put",
                                       param_names, 2, args_set)) {
            return value_invalid();
        }
        url_v = args_v[0];
        body_v = args_v[1];
        body_text = body_v.as.s;
    } else if (strcmp(fn_name, "request") == 0 || strcmp(fn_name, "requestEx") == 0) {
        if (call_expr->as.call.arg_count != 2 && call_expr->as.call.arg_count != 3) {
            runtime_error(rt, call_expr->line, "function 'http.%s' expects 2 or 3 args, got %d", fn_name,
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        const char *display_name = strcmp(fn_name, "request") == 0 ? "http.request" : "http.requestEx";
        const char *param_names[3] = {"method", "url", "body"};
        Value args_v[3];
        int args_set[3] = {0, 0, 0};
        if (!bind_named_call_args(rt, current_module, env, call_expr, display_name, param_names, 3, args_v, args_set)) {
            return value_invalid();
        }
        if (!ensure_required_call_args(rt, call_expr, display_name, param_names, 2, args_set)) {
            return value_invalid();
        }
        has_method_arg = 1;
        has_body = args_set[2] ? 1 : 0;
        method_v = args_v[0];
        url_v = args_v[1];
        body_v = args_v[2];
        if (has_body) {
            body_text = body_v.as.s;
        }
    } else {
        runtime_error(rt, call_expr->line, "undefined function 'http.%s'", fn_name);
        return value_invalid();
    }

    if (has_method_arg) {
        if (method_v.type != VT_STRING) {
            runtime_error(rt, call_expr->line, "arg 1 for 'http.%s' expects string method, got %s", fn_name,
                          value_type_name(method_v.type));
            return value_invalid();
        }
        method = method_v.as.s;
    }

    if (url_v.type != VT_STRING) {
        int arg_idx = has_method_arg ? 2 : 1;
        runtime_error(rt, call_expr->line, "arg %d for 'http.%s' expects string url, got %s", arg_idx, fn_name,
                      value_type_name(url_v.type));
        return value_invalid();
    }

    if (has_body) {
        if (body_v.type != VT_STRING) {
            int arg_idx = has_method_arg ? 3 : 2;
            runtime_error(rt, call_expr->line, "arg %d for 'http.%s' expects string body, got %s", arg_idx, fn_name,
                          value_type_name(body_v.type));
            return value_invalid();
        }
    }

    if (!http_method_is_safe(method)) {
        runtime_error(rt, call_expr->line, "http method must be uppercase letters");
        return value_invalid();
    }
    if (rt->check_only) {
        if (strcmp(fn_name, "requestEx") == 0) {
            HttpResponseParsed parsed;
            http_response_parsed_init(&parsed);
            parsed.status = 200;
            parsed.body = xstrdup("");
            parsed.headers = map_new(VT_STRING, NULL);
            if (!parsed.body || !parsed.headers) {
                runtime_error(rt, call_expr->line, "out of memory");
                http_response_parsed_cleanup(&parsed);
                return value_invalid();
            }
            return http_make_response_object(rt, call_expr->line, &parsed);
        }
        return value_string("");
    }

    if (strcmp(fn_name, "requestEx") == 0) {
        char io_err[256];
        HttpResponseParsed parsed;
        http_response_parsed_init(&parsed);
        if (!http_request_exec(method, url_v.as.s, body_text, has_body, 0, 1, &parsed, io_err, sizeof(io_err))) {
            runtime_error(rt, call_expr->line, "%s", io_err);
            http_response_parsed_cleanup(&parsed);
            return value_invalid();
        }
        return http_make_response_object(rt, call_expr->line, &parsed);
    }

    char io_err[256];
    char *body = http_request_text(method, url_v.as.s, body_text, has_body, io_err, sizeof(io_err));
    if (!body) {
        runtime_error(rt, call_expr->line, "%s", io_err);
        return value_invalid();
    }
    Value out;
    out.type = VT_STRING;
    out.as.s = body;
    return out;
}

static Value native_rand_call(Runtime *rt, Module *current_module, Env *env, Expr *call_expr, const char *fn_name) {
    if (strcmp(fn_name, "seed") == 0) {
        if (call_expr->as.call.arg_count != 1) {
            runtime_error(rt, call_expr->line, "function 'rand.seed' expects 1 arg, got %d", call_expr->as.call.arg_count);
            return value_invalid();
        }
        const char *param_names[1] = {"seed"};
        Value args_v[1];
        int args_set[1] = {0};
        if (!bind_named_call_args(rt, current_module, env, call_expr, "rand.seed", param_names, 1, args_v, args_set)) {
            return value_invalid();
        }
        if (!ensure_required_call_args(rt, call_expr, "rand.seed", param_names, 1, args_set)) {
            return value_invalid();
        }
        Value seed_v = args_v[0];
        if (seed_v.type != VT_INT) {
            runtime_error(rt, call_expr->line, "rand.seed(...) expects int seed, got %s", value_type_name(seed_v.type));
            return value_invalid();
        }
        unsigned long long s = (unsigned long long)seed_v.as.i;
        if (s == 0) {
            s = 0x9e3779b97f4a7c15ULL;
        }
        rt->rand_state = s;
        rt->rand_seeded = 1;
        return value_void();
    }

    if (strcmp(fn_name, "int") == 0) {
        if (call_expr->as.call.arg_count != 2) {
            runtime_error(rt, call_expr->line, "function 'rand.int' expects 2 args, got %d", call_expr->as.call.arg_count);
            return value_invalid();
        }
        const char *param_names[2] = {"min", "max"};
        Value args_v[2];
        int args_set[2] = {0, 0};
        if (!bind_named_call_args(rt, current_module, env, call_expr, "rand.int", param_names, 2, args_v, args_set)) {
            return value_invalid();
        }
        if (!ensure_required_call_args(rt, call_expr, "rand.int", param_names, 2, args_set)) {
            return value_invalid();
        }
        Value min_v = args_v[0];
        Value max_v = args_v[1];
        if (min_v.type != VT_INT || max_v.type != VT_INT) {
            runtime_error(rt, call_expr->line, "rand.int(...) expects int min/max, got %s/%s", value_type_name(min_v.type),
                          value_type_name(max_v.type));
            return value_invalid();
        }
        long long min = min_v.as.i;
        long long max = max_v.as.i;
        if (min > max) {
            runtime_error(rt, call_expr->line, "rand.int(...) expects min <= max, got %lld > %lld", min, max);
            return value_invalid();
        }
        unsigned long long span = ((unsigned long long)max - (unsigned long long)min) + 1ULL;
        unsigned long long r = rand_next_u64(rt);
        unsigned long long off = span == 0 ? r : (r % span);
        unsigned long long out_u = (unsigned long long)min + off;
        return value_int((long long)out_u);
    }

    if (strcmp(fn_name, "float") == 0) {
        if (call_expr->as.call.arg_count != 0) {
            runtime_error(rt, call_expr->line, "function 'rand.float' expects 0 args, got %d", call_expr->as.call.arg_count);
            return value_invalid();
        }
        unsigned long long r = rand_next_u64(rt);
        double d = (double)(r >> 11) * (1.0 / 9007199254740992.0);
        return value_float(d);
    }

    runtime_error(rt, call_expr->line, "undefined function 'rand.%s'", fn_name);
    return value_invalid();
}

static Value native_re_call(Runtime *rt, Module *current_module, Env *env, Expr *call_expr, const char *fn_name) {
    if (strcmp(fn_name, "compilePattern") == 0) {
        if (call_expr->as.call.arg_count != 1) {
            runtime_error(rt, call_expr->line, "function 'recore.compilePattern' expects 1 arg, got %d",
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        Value pattern_v = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (pattern_v.type != VT_STRING) {
            runtime_error(rt, call_expr->line, "recore.compilePattern(...) expects string pattern, got %s",
                          value_type_name(pattern_v.type));
            return value_invalid();
        }
        if (rt->check_only) {
            return value_int(1);
        }

        regex_t compiled;
        int rc = regcomp(&compiled, pattern_v.as.s, REG_EXTENDED);
        if (rc != 0) {
            re_runtime_error_from_code(rt, call_expr->line, "re.compile", rc, &compiled);
            return value_invalid();
        }
        long long handle = 0;
        if (!re_store_compiled(rt, &compiled, &handle)) {
            regfree(&compiled);
            runtime_error(rt, call_expr->line, "out of memory");
            return value_invalid();
        }
        return value_int(handle);
    }

    if (strcmp(fn_name, "freePattern") == 0) {
        if (call_expr->as.call.arg_count != 1) {
            runtime_error(rt, call_expr->line, "function 'recore.freePattern' expects 1 arg, got %d",
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        Value handle_v = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (handle_v.type != VT_INT) {
            runtime_error(rt, call_expr->line, "recore.freePattern(...) expects int handle, got %s",
                          value_type_name(handle_v.type));
            return value_invalid();
        }
        if (handle_v.as.i <= 0) {
            runtime_error(rt, call_expr->line, "re.freePattern: invalid handle %lld", handle_v.as.i);
            return value_invalid();
        }
        if (rt->check_only) {
            return value_bool(1);
        }
        if (!re_free_handle(rt, handle_v.as.i)) {
            runtime_error(rt, call_expr->line, "re.freePattern: invalid handle %lld", handle_v.as.i);
            return value_invalid();
        }
        return value_bool(1);
    }

    if (strcmp(fn_name, "isMatchHandle") == 0) {
        if (call_expr->as.call.arg_count != 2) {
            runtime_error(rt, call_expr->line, "function 'recore.isMatchHandle' expects 2 args, got %d",
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        Value handle_v = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        Value text_v = eval_expr(rt, current_module, env, call_expr->as.call.args[1]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (handle_v.type != VT_INT) {
            runtime_error(rt, call_expr->line, "arg 1 for 'recore.isMatchHandle' expects int handle, got %s",
                          value_type_name(handle_v.type));
            return value_invalid();
        }
        if (text_v.type != VT_STRING) {
            runtime_error(rt, call_expr->line, "arg 2 for 'recore.isMatchHandle' expects string text, got %s",
                          value_type_name(text_v.type));
            return value_invalid();
        }
        if (rt->check_only) {
            return value_bool(0);
        }
        RegexHandleEntry *entry = re_find_handle(rt, handle_v.as.i);
        if (!entry) {
            runtime_error(rt, call_expr->line, "re.isMatchHandle: invalid handle %lld", handle_v.as.i);
            return value_invalid();
        }

        int rc = regexec(&entry->compiled, text_v.as.s, 0, NULL, 0);
        if (rc == 0) {
            return value_bool(1);
        }
        if (rc == REG_NOMATCH) {
            return value_bool(0);
        }
        re_runtime_error_from_code(rt, call_expr->line, "re.isMatchHandle", rc, &entry->compiled);
        return value_invalid();
    }

    if (strcmp(fn_name, "searchHandle") == 0) {
        if (call_expr->as.call.arg_count != 2) {
            runtime_error(rt, call_expr->line, "function 'recore.searchHandle' expects 2 args, got %d",
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        Value handle_v = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        Value text_v = eval_expr(rt, current_module, env, call_expr->as.call.args[1]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (handle_v.type != VT_INT) {
            runtime_error(rt, call_expr->line, "arg 1 for 'recore.searchHandle' expects int handle, got %s",
                          value_type_name(handle_v.type));
            return value_invalid();
        }
        if (text_v.type != VT_STRING) {
            runtime_error(rt, call_expr->line, "arg 2 for 'recore.searchHandle' expects string text, got %s",
                          value_type_name(text_v.type));
            return value_invalid();
        }
        if (rt->check_only) {
            return re_new_no_match_map(rt, call_expr->line);
        }
        RegexHandleEntry *entry = re_find_handle(rt, handle_v.as.i);
        if (!entry) {
            runtime_error(rt, call_expr->line, "re.searchHandle: invalid handle %lld", handle_v.as.i);
            return value_invalid();
        }

        regmatch_t m[1];
        int rc = regexec(&entry->compiled, text_v.as.s, 1, m, 0);
        if (rc == REG_NOMATCH) {
            return re_new_no_match_map(rt, call_expr->line);
        }
        if (rc != 0) {
            re_runtime_error_from_code(rt, call_expr->line, "re.searchHandle", rc, &entry->compiled);
            return value_invalid();
        }
        if (m[0].rm_so < 0 || m[0].rm_eo < 0) {
            return re_new_no_match_map(rt, call_expr->line);
        }
        return re_new_search_map(rt, call_expr->line, (long long)m[0].rm_so, (long long)m[0].rm_eo);
    }

    if (strcmp(fn_name, "replaceAllHandle") == 0) {
        if (call_expr->as.call.arg_count != 3) {
            runtime_error(rt, call_expr->line, "function 'recore.replaceAllHandle' expects 3 args, got %d",
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        Value handle_v = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        Value text_v = eval_expr(rt, current_module, env, call_expr->as.call.args[1]);
        if (rt->has_error) {
            return value_invalid();
        }
        Value repl_v = eval_expr(rt, current_module, env, call_expr->as.call.args[2]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (handle_v.type != VT_INT) {
            runtime_error(rt, call_expr->line, "arg 1 for 'recore.replaceAllHandle' expects int handle, got %s",
                          value_type_name(handle_v.type));
            return value_invalid();
        }
        if (text_v.type != VT_STRING) {
            runtime_error(rt, call_expr->line, "arg 2 for 'recore.replaceAllHandle' expects string text, got %s",
                          value_type_name(text_v.type));
            return value_invalid();
        }
        if (repl_v.type != VT_STRING) {
            runtime_error(rt, call_expr->line, "arg 3 for 'recore.replaceAllHandle' expects string replacement, got %s",
                          value_type_name(repl_v.type));
            return value_invalid();
        }
        if (rt->check_only) {
            return value_string(text_v.as.s);
        }
        RegexHandleEntry *entry = re_find_handle(rt, handle_v.as.i);
        if (!entry) {
            runtime_error(rt, call_expr->line, "re.replaceAllHandle: invalid handle %lld", handle_v.as.i);
            return value_invalid();
        }

        ReTextBuf out;
        if (!re_text_buf_init(&out)) {
            runtime_error(rt, call_expr->line, "out of memory");
            return value_invalid();
        }

        size_t text_len = strlen(text_v.as.s);
        size_t pos = 0;
        while (pos <= text_len) {
            regmatch_t m[1];
            int rc = regexec(&entry->compiled, text_v.as.s + pos, 1, m, 0);
            if (rc == REG_NOMATCH) {
                if (!re_text_buf_push_n(&out, text_v.as.s + pos, text_len - pos)) {
                    free(out.buf);
                    runtime_error(rt, call_expr->line, "out of memory");
                    return value_invalid();
                }
                break;
            }
            if (rc != 0) {
                free(out.buf);
                re_runtime_error_from_code(rt, call_expr->line, "re.replaceAllHandle", rc, &entry->compiled);
                return value_invalid();
            }
            if (m[0].rm_so < 0 || m[0].rm_eo < 0) {
                free(out.buf);
                runtime_error(rt, call_expr->line, "re.replaceAllHandle: invalid regex match bounds");
                return value_invalid();
            }
            size_t start = pos + (size_t)m[0].rm_so;
            size_t end = pos + (size_t)m[0].rm_eo;
            if (end < start || end > text_len) {
                free(out.buf);
                runtime_error(rt, call_expr->line, "re.replaceAllHandle: invalid regex match bounds");
                return value_invalid();
            }
            if (start == end) {
                free(out.buf);
                runtime_error(rt, call_expr->line, "re.replaceAllHandle: zero-length matches are not supported");
                return value_invalid();
            }

            if (!re_text_buf_push_n(&out, text_v.as.s + pos, start - pos) || !re_text_buf_push_cstr(&out, repl_v.as.s)) {
                free(out.buf);
                runtime_error(rt, call_expr->line, "out of memory");
                return value_invalid();
            }
            pos = end;
        }

        Value replaced;
        replaced.type = VT_STRING;
        replaced.as.s = out.buf;
        return replaced;
    }

    if (strcmp(fn_name, "splitHandle") == 0) {
        if (call_expr->as.call.arg_count != 2) {
            runtime_error(rt, call_expr->line, "function 'recore.splitHandle' expects 2 args, got %d",
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        Value handle_v = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        Value text_v = eval_expr(rt, current_module, env, call_expr->as.call.args[1]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (handle_v.type != VT_INT) {
            runtime_error(rt, call_expr->line, "arg 1 for 'recore.splitHandle' expects int handle, got %s",
                          value_type_name(handle_v.type));
            return value_invalid();
        }
        if (text_v.type != VT_STRING) {
            runtime_error(rt, call_expr->line, "arg 2 for 'recore.splitHandle' expects string text, got %s",
                          value_type_name(text_v.type));
            return value_invalid();
        }
        RegexHandleEntry *entry = re_find_handle(rt, handle_v.as.i);
        if (!entry) {
            runtime_error(rt, call_expr->line, "re.splitHandle: invalid handle %lld", handle_v.as.i);
            return value_invalid();
        }
        ListValue *parts = list_new(VT_STRING, NULL);
        if (!parts) {
            runtime_error(rt, call_expr->line, "out of memory");
            return value_invalid();
        }
        size_t text_len = strlen(text_v.as.s);
        size_t pos = 0;
        while (pos <= text_len) {
            regmatch_t m[1];
            int rc = regexec(&entry->compiled, text_v.as.s + pos, 1, m, 0);
            if (rc == REG_NOMATCH) {
                Value part;
                part.type = VT_STRING;
                part.as.s = xstrndup(text_v.as.s + pos, text_len - pos);
                if (!list_push(parts, part)) {
                    runtime_error(rt, call_expr->line, "out of memory");
                    return value_invalid();
                }
                break;
            }
            if (rc != 0) {
                re_runtime_error_from_code(rt, call_expr->line, "re.splitHandle", rc, &entry->compiled);
                return value_invalid();
            }
            if (m[0].rm_so < 0 || m[0].rm_eo < 0) {
                runtime_error(rt, call_expr->line, "re.splitHandle: invalid regex match bounds");
                return value_invalid();
            }
            size_t start = pos + (size_t)m[0].rm_so;
            size_t end = pos + (size_t)m[0].rm_eo;
            if (end < start || end > text_len) {
                runtime_error(rt, call_expr->line, "re.splitHandle: invalid regex match bounds");
                return value_invalid();
            }
            if (start == end) {
                runtime_error(rt, call_expr->line, "re.splitHandle: zero-length matches are not supported");
                return value_invalid();
            }
            Value part;
            part.type = VT_STRING;
            part.as.s = xstrndup(text_v.as.s + pos, start - pos);
            if (!list_push(parts, part)) {
                runtime_error(rt, call_expr->line, "out of memory");
                return value_invalid();
            }
            pos = end;
        }
        return value_list(parts);
    }

    runtime_error(rt, call_expr->line, "undefined function 'recore.%s'", fn_name);
    return value_invalid();
}
