static Value native_json_call(Runtime *rt, Module *current_module, Env *env, Expr *call_expr, const char *fn_name) {
    if (strcmp(fn_name, "encode") == 0) {
        if (call_expr->as.call.arg_count != 1) {
            runtime_error(rt, call_expr->line, "function 'json.encode' expects 1 arg, got %d",
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        Value in = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        JsonBuf jb;
        if (!json_buf_init(&jb)) {
            runtime_error(rt, call_expr->line, "out of memory");
            return value_invalid();
        }
        if (!json_encode_value(rt, call_expr->line, in, &jb)) {
            if (!rt->has_error) {
                runtime_error(rt, call_expr->line, "out of memory");
            }
            free(jb.buf);
            return value_invalid();
        }
        Value out;
        out.type = VT_STRING;
        out.as.s = jb.buf;
        return out;
    }
    if (strcmp(fn_name, "decode") == 0) {
        if (call_expr->as.call.arg_count != 1) {
            runtime_error(rt, call_expr->line, "function 'json.decode' expects 1 arg, got %d",
                          call_expr->as.call.arg_count);
            return value_invalid();
        }
        Value in = eval_expr(rt, current_module, env, call_expr->as.call.args[0]);
        if (rt->has_error) {
            return value_invalid();
        }
        if (in.type != VT_STRING) {
            runtime_error(rt, call_expr->line, "json.decode(...) expects string, got %s", value_type_name(in.type));
            return value_invalid();
        }
        JsonCursor c;
        c.src = in.as.s;
        c.len = (int)strlen(in.as.s);
        c.pos = 0;
        Value out = json_parse_value(rt, call_expr->line, &c);
        if (rt->has_error) {
            return value_invalid();
        }
        json_skip_ws(&c);
        if (c.pos != c.len) {
            runtime_error(rt, call_expr->line, "json.decode: trailing characters");
            return value_invalid();
        }
        return out;
    }
    runtime_error(rt, call_expr->line, "undefined function 'json.%s'", fn_name);
    return value_invalid();
}

static char *fs_read_text_file(const char *path, char *err, size_t err_cap) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(err, err_cap, "cannot open file: %s", path);
        return NULL;
    }

    size_t cap = 256;
    size_t len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) {
        fclose(f);
        snprintf(err, err_cap, "out of memory");
        return NULL;
    }

    while (1) {
        if (len + 256 + 1 > cap) {
            cap *= 2;
            char *next = (char *)realloc(buf, cap);
            if (!next) {
                free(buf);
                fclose(f);
                snprintf(err, err_cap, "out of memory");
                return NULL;
            }
            buf = next;
        }
        size_t n = fread(buf + len, 1, 256, f);
        len += n;
        if (n < 256) {
            if (ferror(f)) {
                free(buf);
                fclose(f);
                snprintf(err, err_cap, "failed reading file: %s", path);
                return NULL;
            }
            break;
        }
    }

    fclose(f);
    buf[len] = '\0';
    return buf;
}

static int fs_write_text_file(const char *path, const char *text, char *err, size_t err_cap) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        snprintf(err, err_cap, "cannot open file for write: %s", path);
        return 0;
    }
    size_t n = strlen(text);
    if (n > 0 && fwrite(text, 1, n, f) != n) {
        fclose(f);
        snprintf(err, err_cap, "failed writing file: %s", path);
        return 0;
    }
    if (fclose(f) != 0) {
        snprintf(err, err_cap, "failed closing file: %s", path);
        return 0;
    }
    return 1;
}

static int fs_append_text_file(const char *path, const char *text, char *err, size_t err_cap) {
    FILE *f = fopen(path, "ab");
    if (!f) {
        snprintf(err, err_cap, "cannot open file for append: %s", path);
        return 0;
    }
    size_t n = strlen(text);
    if (n > 0 && fwrite(text, 1, n, f) != n) {
        fclose(f);
        snprintf(err, err_cap, "failed appending file: %s", path);
        return 0;
    }
    if (fclose(f) != 0) {
        snprintf(err, err_cap, "failed closing file: %s", path);
        return 0;
    }
    return 1;
}

static int fs_path_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static int fs_make_dir(const char *path, char *err, size_t err_cap) {
    if (mkdir(path, 0777) == 0) {
        return 1;
    }
    if (errno == EEXIST) {
        struct stat st;
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
            return 1;
        }
    }
    snprintf(err, err_cap, "cannot create directory: %s", path);
    return 0;
}

static int fs_remove_path(const char *path, char *err, size_t err_cap) {
    if (remove(path) == 0) {
        return 1;
    }
    snprintf(err, err_cap, "cannot remove path: %s", path);
    return 0;
}

static char *path_basename_dup(const char *in) {
    if (in == NULL || in[0] == '\0') {
        return xstrdup("");
    }
    size_t len = strlen(in);
    while (len > 1 && in[len - 1] == '/') {
        len--;
    }
    const char *last = in;
    for (size_t i = 0; i < len; i++) {
        if (in[i] == '/') {
            last = in + i + 1;
        }
    }
    return xstrndup(last, len - (size_t)(last - in));
}

static char *path_dirname_dup(const char *in) {
    if (in == NULL || in[0] == '\0') {
        return xstrdup(".");
    }
    size_t len = strlen(in);
    while (len > 1 && in[len - 1] == '/') {
        len--;
    }
    ssize_t slash = -1;
    for (size_t i = 0; i < len; i++) {
        if (in[i] == '/') {
            slash = (ssize_t)i;
        }
    }
    if (slash < 0) {
        return xstrdup(".");
    }
    if (slash == 0) {
        return xstrdup("/");
    }
    return xstrndup(in, (size_t)slash);
}

static char *path_join_dup(const char *a, const char *b) {
    if (!a || a[0] == '\0') {
        return xstrdup(b ? b : "");
    }
    if (!b || b[0] == '\0') {
        return xstrdup(a);
    }
    size_t alen = strlen(a);
    size_t blen = strlen(b);
    int need_sep = 1;
    if (a[alen - 1] == '/' || b[0] == '/') {
        need_sep = 0;
    }
    char *out = (char *)malloc(alen + blen + (size_t)need_sep + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, a, alen);
    size_t pos = alen;
    if (need_sep) {
        out[pos++] = '/';
    }
    memcpy(out + pos, b, blen);
    pos += blen;
    out[pos] = '\0';
    return out;
}

static char *path_ext_dup(const char *in) {
    if (in == NULL || in[0] == '\0') {
        return xstrdup("");
    }
    const char *last_slash = strrchr(in, '/');
    const char *base = last_slash ? last_slash + 1 : in;
    const char *last_dot = strrchr(base, '.');
    if (!last_dot || last_dot == base) {
        return xstrdup("");
    }
    return xstrdup(last_dot);
}

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} ReTextBuf;

static int re_text_buf_init(ReTextBuf *b) {
    b->cap = 64;
    b->len = 0;
    b->buf = (char *)malloc(b->cap);
    if (!b->buf) {
        return 0;
    }
    b->buf[0] = '\0';
    return 1;
}

static int re_text_buf_push_n(ReTextBuf *b, const char *s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        while (b->len + n + 1 > b->cap) {
            b->cap *= 2;
        }
        char *next = (char *)realloc(b->buf, b->cap);
        if (!next) {
            return 0;
        }
        b->buf = next;
    }
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
    return 1;
}

static int re_text_buf_push_cstr(ReTextBuf *b, const char *s) {
    return re_text_buf_push_n(b, s, strlen(s));
}

static RegexHandleEntry *re_find_handle(Runtime *rt, long long handle) {
    for (int i = 0; i < rt->regex_entry_count; i++) {
        RegexHandleEntry *e = &rt->regex_entries[i];
        if (e->active && e->handle == handle) {
            return e;
        }
    }
    return NULL;
}

static int re_store_compiled(Runtime *rt, regex_t *compiled, long long *out_handle) {
    int slot = -1;
    for (int i = 0; i < rt->regex_entry_count; i++) {
        if (!rt->regex_entries[i].active) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        if (rt->regex_entry_count + 1 > rt->regex_entry_cap) {
            int next_cap = rt->regex_entry_cap == 0 ? 8 : rt->regex_entry_cap * 2;
            RegexHandleEntry *next =
                (RegexHandleEntry *)realloc(rt->regex_entries, (size_t)next_cap * sizeof(RegexHandleEntry));
            if (!next) {
                return 0;
            }
            for (int i = rt->regex_entry_cap; i < next_cap; i++) {
                next[i].active = 0;
                next[i].handle = 0;
            }
            rt->regex_entries = next;
            rt->regex_entry_cap = next_cap;
        }
        slot = rt->regex_entry_count++;
    }

    if (rt->regex_next_handle <= 0) {
        rt->regex_next_handle = 1;
    }
    RegexHandleEntry *entry = &rt->regex_entries[slot];
    entry->active = 1;
    entry->handle = rt->regex_next_handle++;
    entry->compiled = *compiled;
    *out_handle = entry->handle;
    return 1;
}

static int re_free_handle(Runtime *rt, long long handle) {
    RegexHandleEntry *entry = re_find_handle(rt, handle);
    if (!entry) {
        return 0;
    }
    regfree(&entry->compiled);
    entry->active = 0;
    entry->handle = 0;
    return 1;
}

static void cleanup_regex_registry(Runtime *rt) {
    if (!rt || !rt->regex_entries) {
        return;
    }
    for (int i = 0; i < rt->regex_entry_count; i++) {
        if (rt->regex_entries[i].active) {
            regfree(&rt->regex_entries[i].compiled);
            rt->regex_entries[i].active = 0;
        }
    }
    free(rt->regex_entries);
    rt->regex_entries = NULL;
    rt->regex_entry_count = 0;
    rt->regex_entry_cap = 0;
    rt->regex_next_handle = 0;
}

static void re_runtime_error_from_code(Runtime *rt, int line, const char *prefix, int code, regex_t *compiled) {
    char detail[256];
    detail[0] = '\0';
    regerror(code, compiled, detail, sizeof(detail));
    runtime_error(rt, line, "%s: %s", prefix, detail[0] == '\0' ? "regex operation failed" : detail);
}

static Value re_new_search_map(Runtime *rt, int line, long long start, long long end) {
    MapValue *m = map_new(VT_INT, NULL);
    if (!m) {
        runtime_error(rt, line, "out of memory");
        return value_invalid();
    }
    if (!map_set(m, "start", value_int(start)) || !map_set(m, "end", value_int(end)) ||
        !map_set(m, "length", value_int(end - start))) {
        runtime_error(rt, line, "out of memory");
        return value_invalid();
    }
    Value out;
    out.type = VT_MAP;
    out.as.map = m;
    return out;
}

static Value re_new_no_match_map(Runtime *rt, int line) {
    return re_new_search_map(rt, line, -1, -1);
}

static int http_url_is_safe(const char *url) {
    for (const unsigned char *p = (const unsigned char *)url; *p != '\0'; p++) {
        unsigned char c = *p;
        if (c < 0x20 || c >= 0x7f) {
            return 0;
        }
        if (c == '"' || c == '\\' || c == '`' || c == '$') {
            return 0;
        }
    }
    return 1;
}

static int http_method_is_safe(const char *method) {
    if (!method || method[0] == '\0') {
        return 0;
    }
    size_t n = strlen(method);
    if (n > 16) {
        return 0;
    }
    for (size_t i = 0; i < n; i++) {
        char c = method[i];
        if (c < 'A' || c > 'Z') {
            return 0;
        }
    }
    return 1;
}

typedef struct {
    int status;
    char *body;
    MapValue *headers;
} HttpResponseParsed;

static void http_response_parsed_init(HttpResponseParsed *out) {
    if (!out) {
        return;
    }
    out->status = 0;
    out->body = NULL;
    out->headers = NULL;
}

static void http_response_parsed_cleanup(HttpResponseParsed *out) {
    if (!out) {
        return;
    }
    if (out->body) {
        free(out->body);
        out->body = NULL;
    }
}

static int http_write_all_fd(int fd, const char *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = send(fd, buf + off, len - off, 0);
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n <= 0) {
            return 0;
        }
        off += (size_t)n;
    }
    return 1;
}

static int http_write_all_ssl(SSL *ssl, const char *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        int n = SSL_write(ssl, buf + off, (int)(len - off));
        if (n <= 0) {
            int ssl_err = SSL_get_error(ssl, n);
            if (ssl_err == SSL_ERROR_WANT_READ || ssl_err == SSL_ERROR_WANT_WRITE) {
                continue;
            }
            return 0;
        }
        off += (size_t)n;
    }
    return 1;
}

static int http_find_header_end(const char *buf, size_t len, size_t *out_end) {
    if (!buf || len < 2 || !out_end) {
        return 0;
    }
    for (size_t i = 0; i + 3 < len; i++) {
        if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n') {
            *out_end = i + 4;
            return 1;
        }
    }
    for (size_t i = 0; i + 1 < len; i++) {
        if (buf[i] == '\n' && buf[i + 1] == '\n') {
            *out_end = i + 2;
            return 1;
        }
    }
    return 0;
}

static int http_parse_status_code(const char *resp, size_t len, int *out_code) {
    if (!resp || len == 0 || !out_code) {
        return 0;
    }
    size_t line_end = 0;
    while (line_end < len && resp[line_end] != '\n') {
        line_end++;
    }
    if (line_end == len) {
        return 0;
    }
    size_t i = 0;
    while (i < line_end && resp[i] != ' ') {
        i++;
    }
    if (i == line_end) {
        return 0;
    }
    while (i < line_end && resp[i] == ' ') {
        i++;
    }
    if (i + 2 >= line_end) {
        return 0;
    }
    if (!isdigit((unsigned char)resp[i]) || !isdigit((unsigned char)resp[i + 1]) ||
        !isdigit((unsigned char)resp[i + 2])) {
        return 0;
    }
    *out_code = (resp[i] - '0') * 100 + (resp[i + 1] - '0') * 10 + (resp[i + 2] - '0');
    return 1;
}

static int http_header_map_set(MapValue *m, const char *key_src, size_t key_len, const char *val_src, size_t val_len) {
    while (key_len > 0 && isspace((unsigned char)*key_src)) {
        key_src++;
        key_len--;
    }
    while (key_len > 0 && isspace((unsigned char)key_src[key_len - 1])) {
        key_len--;
    }
    while (val_len > 0 && isspace((unsigned char)*val_src)) {
        val_src++;
        val_len--;
    }
    while (val_len > 0 && isspace((unsigned char)val_src[val_len - 1])) {
        val_len--;
    }

    if (key_len == 0) {
        return 1;
    }

    char *k = xstrndup(key_src, key_len);
    char *v = xstrndup(val_src, val_len);
    if (!k || !v) {
        free(k);
        free(v);
        return 0;
    }
    Value vv = value_string(v);
    free(v);
    if (!vv.as.s) {
        free(k);
        return 0;
    }
    if (!map_set(m, k, vv)) {
        free(k);
        return 0;
    }
    free(k);
    return 1;
}

static int http_parse_headers_map(const char *resp, size_t header_end, MapValue *headers, char *err, size_t err_cap) {
    if (header_end == 0) {
        return 1;
    }

    size_t i = 0;
    while (i < header_end && resp[i] != '\n') {
        i++;
    }
    if (i < header_end && resp[i] == '\n') {
        i++;
    }

    while (i < header_end) {
        size_t line_start = i;
        while (i < header_end && resp[i] != '\n') {
            i++;
        }
        size_t line_end = i;
        if (i < header_end && resp[i] == '\n') {
            i++;
        }
        if (line_end > line_start && resp[line_end - 1] == '\r') {
            line_end--;
        }
        if (line_end <= line_start) {
            continue;
        }

        size_t colon = line_start;
        while (colon < line_end && resp[colon] != ':') {
            colon++;
        }
        if (colon >= line_end) {
            continue;
        }
        if (!http_header_map_set(headers, resp + line_start, colon - line_start, resp + colon + 1,
                                 line_end - (colon + 1))) {
            snprintf(err, err_cap, "out of memory");
            return 0;
        }
    }
    return 1;
}

static int http_parse_response(const char *method, const char *resp, size_t resp_len, int require_2xx, int with_headers,
                               HttpResponseParsed *out, char *err, size_t err_cap) {
    size_t header_end = 0;
    if (!http_find_header_end(resp, resp_len, &header_end)) {
        snprintf(err, err_cap, "http invalid response");
        return 0;
    }

    int status = 0;
    if (!http_parse_status_code(resp, resp_len, &status)) {
        snprintf(err, err_cap, "http invalid response");
        return 0;
    }
    if (require_2xx && (status < 200 || status >= 300)) {
        snprintf(err, err_cap, "http %s request failed", method);
        return 0;
    }
    if (header_end > resp_len) {
        snprintf(err, err_cap, "http invalid response");
        return 0;
    }

    if (with_headers) {
        MapValue *headers = map_new(VT_STRING, NULL);
        if (!headers) {
            snprintf(err, err_cap, "out of memory");
            return 0;
        }
        if (!http_parse_headers_map(resp, header_end, headers, err, err_cap)) {
            return 0;
        }
        out->headers = headers;
    }

    size_t body_len = resp_len - header_end;
    char *body = (char *)malloc(body_len + 1);
    if (!body) {
        snprintf(err, err_cap, "out of memory");
        return 0;
    }
    memcpy(body, resp + header_end, body_len);
    body[body_len] = '\0';

    out->status = status;
    out->body = body;
    return 1;
}

static char *http_file_url_to_path(const char *url, char *err, size_t err_cap) {
    const char *rest = url + 7; // after file://
    const char *path_start = NULL;
    if (*rest == '/') {
        path_start = rest;
    } else {
        const char *slash = strchr(rest, '/');
        if (!slash) {
            snprintf(err, err_cap, "invalid file url");
            return NULL;
        }
        size_t host_len = (size_t)(slash - rest);
        if (host_len != strlen("localhost") || strncmp(rest, "localhost", host_len) != 0) {
            snprintf(err, err_cap, "unsupported file url host");
            return NULL;
        }
        path_start = slash;
    }
    size_t n = 0;
    while (path_start[n] != '\0' && path_start[n] != '?' && path_start[n] != '#') {
        n++;
    }
    if (n == 0) {
        snprintf(err, err_cap, "invalid file url path");
        return NULL;
    }
    return xstrndup(path_start, n);
}

static int http_parse_network_url(const char *url, const char *scheme, int default_port, char **out_host, int *out_port,
                                  char **out_path, char *err, size_t err_cap) {
    char prefix[16];
    snprintf(prefix, sizeof(prefix), "%s://", scheme);
    const char *rest = url + strlen(prefix);
