#if defined(__unix__) || defined(__APPLE__)
extern int setenv(const char *name, const char *value, int overwrite);
#endif

static void print_value(Value v) {
    if (v.type == VT_INT) {
        printf("%lld", v.as.i);
    } else if (v.type == VT_FLOAT) {
        printf("%g", v.as.f);
    } else if (v.type == VT_STRING) {
        printf("%s", v.as.s);
    } else if (v.type == VT_ERROR) {
        printf("error(%s)", v.as.s);
    } else if (v.type == VT_BOOL) {
        printf("%s", v.as.b ? "true" : "false");
    } else if (v.type == VT_MULTI) {
        printf("(");
        if (v.as.multi != NULL) {
            for (int i = 0; i < v.as.multi->count; i++) {
                if (i > 0) {
                    printf(", ");
                }
                print_value(v.as.multi->items[i]);
            }
        }
        printf(")");
    } else if (v.type == VT_LIST) {
        printf("[");
        for (int i = 0; i < v.as.list->count; i++) {
            if (i > 0) {
                printf(", ");
            }
            print_value(v.as.list->items[i]);
        }
        printf("]");
    } else if (v.type == VT_MAP) {
        printf("{");
        for (int i = 0; i < v.as.map->count; i++) {
            if (i > 0) {
                printf(", ");
            }
            printf("\"%s\": ", v.as.map->keys[i]);
            print_value(v.as.map->values[i]);
        }
        printf("}");
    } else if (v.type == VT_CHANNEL) {
        if (v.as.chan) {
            printf("<chan>");
        } else {
            printf("<invalid-chan>");
        }
    } else if (v.type == VT_OBJECT) {
        printf("%s{", v.as.obj->type_name);
        for (int i = 0; i < v.as.obj->field_count; i++) {
            if (i > 0) {
                printf(", ");
            }
            printf("%s: ", v.as.obj->fields[i].name);
            print_value(v.as.obj->values[i]);
        }
        printf("}");
    } else if (v.type == VT_INTERFACE) {
        if (v.as.iface && v.as.iface->obj) {
            Value concrete = value_object(v.as.iface->obj);
            print_value(concrete);
        } else {
            printf("<invalid-interface>");
        }
    } else if (v.type == VT_FUNCTION) {
        if (v.as.func && v.as.func->display_name) {
            printf("<func %s>", v.as.func->display_name);
        } else {
            printf("<func>");
        }
    } else if (v.type == VT_VOID) {
        printf("void");
    } else {
        printf("<invalid>");
    }
}

static char *read_input_line(char *err, size_t err_cap) {
    size_t cap = 64;
    size_t len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) {
        snprintf(err, err_cap, "out of memory");
        return NULL;
    }

    int ch = 0;
    while ((ch = fgetc(stdin)) != EOF && ch != '\n') {
        if (len + 1 >= cap) {
            cap *= 2;
            char *next = (char *)realloc(buf, cap);
            if (!next) {
                free(buf);
                snprintf(err, err_cap, "out of memory");
                return NULL;
            }
            buf = next;
        }
        buf[len++] = (char)ch;
    }

    if (ferror(stdin)) {
        free(buf);
        snprintf(err, err_cap, "failed to read from stdin");
        return NULL;
    }

    buf[len] = '\0';
    return buf;
}

static int parse_int_string(const char *s, long long *out) {
    while (*s != '\0' && isspace((unsigned char)*s)) {
        s++;
    }
    if (*s == '\0') {
        return 0;
    }

    errno = 0;
    char *end = NULL;
    long long v = strtoll(s, &end, 10);
    if (errno != 0 || end == NULL) {
        return 0;
    }
    while (*end != '\0' && isspace((unsigned char)*end)) {
        end++;
    }
    if (*end != '\0') {
        return 0;
    }

    *out = v;
    return 1;
}

static int parse_float_string(const char *s, double *out) {
    while (*s != '\0' && isspace((unsigned char)*s)) {
        s++;
    }
    if (*s == '\0') {
        return 0;
    }

    errno = 0;
    char *end = NULL;
    double v = strtod(s, &end);
    if (errno != 0 || end == NULL) {
        return 0;
    }
    while (*end != '\0' && isspace((unsigned char)*end)) {
        end++;
    }
    if (*end != '\0') {
        return 0;
    }

    *out = v;
    return 1;
}

static Value cast_to_int(Runtime *rt, int line, Value in) {
    if (in.type == VT_INT) {
        return in;
    }
    if (in.type == VT_FLOAT) {
        if (in.as.f > (double)LLONG_MAX || in.as.f < (double)LLONG_MIN) {
            runtime_error(rt, line, "cannot convert float to int: out of range");
            return value_invalid();
        }
        return value_int((long long)in.as.f);
    }
    if (in.type == VT_BOOL) {
        return value_int(in.as.b ? 1 : 0);
    }
    if (in.type == VT_STRING) {
        long long parsed = 0;
        if (!parse_int_string(in.as.s, &parsed)) {
            runtime_error(rt, line, "cannot convert string to int: '%s'", in.as.s);
            return value_invalid();
        }
        return value_int(parsed);
    }
    runtime_error(rt, line, "int(...) does not support %s", value_type_name(in.type));
    return value_invalid();
}

static Value cast_to_float(Runtime *rt, int line, Value in) {
    if (in.type == VT_FLOAT) {
        return in;
    }
    if (in.type == VT_INT) {
        return value_float((double)in.as.i);
    }
    if (in.type == VT_BOOL) {
        return value_float(in.as.b ? 1.0 : 0.0);
    }
    if (in.type == VT_STRING) {
        double parsed = 0.0;
        if (!parse_float_string(in.as.s, &parsed)) {
            runtime_error(rt, line, "cannot convert string to float: '%s'", in.as.s);
            return value_invalid();
        }
        return value_float(parsed);
    }
    runtime_error(rt, line, "float(...) does not support %s", value_type_name(in.type));
    return value_invalid();
}

static Value cast_to_string(Runtime *rt, int line, Value in) {
    char buf[128];
    if (in.type == VT_STRING) {
        return value_string(in.as.s);
    }
    if (in.type == VT_INT) {
        snprintf(buf, sizeof(buf), "%lld", in.as.i);
        return value_string(buf);
    }
    if (in.type == VT_FLOAT) {
        snprintf(buf, sizeof(buf), "%g", in.as.f);
        return value_string(buf);
    }
    if (in.type == VT_BOOL) {
        return value_string(in.as.b ? "true" : "false");
    }
    if (in.type == VT_ERROR) {
        return value_string(in.as.s);
    }
    if (in.type == VT_VOID) {
        return value_string("void");
    }
    runtime_error(rt, line, "str(...) does not support %s", value_type_name(in.type));
    return value_invalid();
}

typedef struct {
    char *buf;
    int len;
    int cap;
} JsonBuf;

static int json_buf_init(JsonBuf *jb) {
    jb->cap = 128;
    jb->len = 0;
    jb->buf = (char *)malloc((size_t)jb->cap);
    if (!jb->buf) {
        return 0;
    }
    jb->buf[0] = '\0';
    return 1;
}

static int json_buf_push_char(JsonBuf *jb, char c) {
    if (jb->len + 2 > jb->cap) {
        jb->cap *= 2;
        char *next = (char *)realloc(jb->buf, (size_t)jb->cap);
        if (!next) {
            return 0;
        }
        jb->buf = next;
    }
    jb->buf[jb->len++] = c;
    jb->buf[jb->len] = '\0';
    return 1;
}

static int json_buf_push_str(JsonBuf *jb, const char *s) {
    size_t n = strlen(s);
    if (jb->len + (int)n + 1 > jb->cap) {
        while (jb->len + (int)n + 1 > jb->cap) {
            jb->cap *= 2;
        }
        char *next = (char *)realloc(jb->buf, (size_t)jb->cap);
        if (!next) {
            return 0;
        }
        jb->buf = next;
    }
    memcpy(jb->buf + jb->len, s, n);
    jb->len += (int)n;
    jb->buf[jb->len] = '\0';
    return 1;
}

static int json_push_escaped_string(JsonBuf *jb, const char *s) {
    if (!json_buf_push_char(jb, '"')) {
        return 0;
    }
    for (const unsigned char *p = (const unsigned char *)s; *p != '\0'; p++) {
        unsigned char c = *p;
        if (c == '"' || c == '\\') {
            if (!json_buf_push_char(jb, '\\') || !json_buf_push_char(jb, (char)c)) {
                return 0;
            }
            continue;
        }
        if (c == '\n') {
            if (!json_buf_push_str(jb, "\\n")) {
                return 0;
            }
            continue;
        }
        if (c == '\t') {
            if (!json_buf_push_str(jb, "\\t")) {
                return 0;
            }
            continue;
        }
        if (c < 0x20) {
            char esc[7];
            snprintf(esc, sizeof(esc), "\\u%04x", c);
            if (!json_buf_push_str(jb, esc)) {
                return 0;
            }
            continue;
        }
        if (!json_buf_push_char(jb, (char)c)) {
            return 0;
        }
    }
    return json_buf_push_char(jb, '"');
}

static int json_encode_value(Runtime *rt, int line, Value v, JsonBuf *jb) {
    if (v.type == VT_INT) {
        char num[64];
        snprintf(num, sizeof(num), "%lld", v.as.i);
        return json_buf_push_str(jb, num);
    }
    if (v.type == VT_FLOAT) {
        char num[64];
        snprintf(num, sizeof(num), "%.17g", v.as.f);
        return json_buf_push_str(jb, num);
    }
    if (v.type == VT_STRING) {
        return json_push_escaped_string(jb, v.as.s);
    }
    if (v.type == VT_BOOL) {
        return json_buf_push_str(jb, v.as.b ? "true" : "false");
    }
    if (v.type == VT_LIST) {
        if (!json_buf_push_char(jb, '[')) {
            return 0;
        }
        for (int i = 0; i < v.as.list->count; i++) {
            if (i > 0 && !json_buf_push_char(jb, ',')) {
                return 0;
            }
            if (!json_encode_value(rt, line, v.as.list->items[i], jb)) {
                return 0;
            }
        }
        return json_buf_push_char(jb, ']');
    }
    if (v.type == VT_MAP) {
        if (!json_buf_push_char(jb, '{')) {
            return 0;
        }
        for (int i = 0; i < v.as.map->count; i++) {
            if (i > 0 && !json_buf_push_char(jb, ',')) {
                return 0;
            }
            if (!json_push_escaped_string(jb, v.as.map->keys[i])) {
                return 0;
            }
            if (!json_buf_push_char(jb, ':')) {
                return 0;
            }
            if (!json_encode_value(rt, line, v.as.map->values[i], jb)) {
                return 0;
            }
        }
        return json_buf_push_char(jb, '}');
    }

    runtime_error(rt, line, "json.encode(...) does not support %s", value_type_name(v.type));
    return 0;
}

typedef struct {
    const char *src;
    int len;
    int pos;
} JsonCursor;

static void json_skip_ws(JsonCursor *c) {
    while (c->pos < c->len && isspace((unsigned char)c->src[c->pos])) {
        c->pos++;
    }
}

static int json_peek(JsonCursor *c) {
    if (c->pos >= c->len) {
        return -1;
    }
    return (unsigned char)c->src[c->pos];
}

static int json_match_literal(JsonCursor *c, const char *lit) {
    int n = (int)strlen(lit);
    if (c->pos + n > c->len) {
        return 0;
    }
    if (strncmp(c->src + c->pos, lit, (size_t)n) != 0) {
        return 0;
    }
    c->pos += n;
    return 1;
}

static char *json_parse_string_raw(Runtime *rt, int line, JsonCursor *c) {
    if (json_peek(c) != '"') {
        runtime_error(rt, line, "json.decode: expected string");
        return NULL;
    }
    c->pos++;

    JsonBuf jb;
    if (!json_buf_init(&jb)) {
        runtime_error(rt, line, "out of memory");
        return NULL;
    }

    while (c->pos < c->len) {
        char ch = c->src[c->pos++];
        if (ch == '"') {
            return jb.buf;
        }
        if (ch == '\\') {
            if (c->pos >= c->len) {
                free(jb.buf);
                runtime_error(rt, line, "json.decode: invalid escape");
                return NULL;
            }
            char esc = c->src[c->pos++];
            if (esc == '"' || esc == '\\' || esc == '/') {
                if (!json_buf_push_char(&jb, esc)) {
                    free(jb.buf);
                    runtime_error(rt, line, "out of memory");
                    return NULL;
                }
            } else if (esc == 'n') {
                if (!json_buf_push_char(&jb, '\n')) {
                    free(jb.buf);
                    runtime_error(rt, line, "out of memory");
                    return NULL;
                }
            } else if (esc == 't') {
                if (!json_buf_push_char(&jb, '\t')) {
                    free(jb.buf);
                    runtime_error(rt, line, "out of memory");
                    return NULL;
                }
            } else {
                free(jb.buf);
                runtime_error(rt, line, "json.decode: unsupported escape");
                return NULL;
            }
            continue;
        }
        if ((unsigned char)ch < 0x20) {
            free(jb.buf);
            runtime_error(rt, line, "json.decode: invalid control character in string");
            return NULL;
        }
        if (!json_buf_push_char(&jb, ch)) {
            free(jb.buf);
            runtime_error(rt, line, "out of memory");
            return NULL;
        }
    }

    free(jb.buf);
    runtime_error(rt, line, "json.decode: unterminated string");
    return NULL;
}

static Value json_parse_value(Runtime *rt, int line, JsonCursor *c);

static Value json_parse_array(Runtime *rt, int line, JsonCursor *c) {
    c->pos++;
    json_skip_ws(c);

    ListValue *list = list_new(VT_INVALID, NULL);
    if (!list) {
        runtime_error(rt, line, "out of memory");
        return value_invalid();
    }

    if (json_peek(c) == ']') {
        c->pos++;
        return value_list(list);
    }

    while (1) {
        Value item = json_parse_value(rt, line, c);
        if (rt->has_error) {
            return value_invalid();
        }
        if (item.type == VT_VOID || item.type == VT_INVALID) {
            runtime_error(rt, line, "json.decode: array item cannot be %s", value_type_name(item.type));
            return value_invalid();
        }
        if (list->elem_type == VT_INVALID) {
            list->elem_type = item.type;
            if (item.type == VT_OBJECT && item.as.obj != NULL) {
                list->elem_object_type = xstrdup(item.as.obj->type_name);
            }
        } else if (list->elem_type != item.type) {
            runtime_error(rt, line, "json.decode: array items must have the same type");
            return value_invalid();
        } else if (list->elem_type == VT_OBJECT && list->elem_object_type != NULL &&
                   (item.as.obj == NULL || strcmp(list->elem_object_type, item.as.obj->type_name) != 0)) {
            runtime_error(rt, line, "json.decode: array object items must have the same type");
            return value_invalid();
        }
        if (!list_push(list, item)) {
            runtime_error(rt, line, "out of memory");
            return value_invalid();
        }

        json_skip_ws(c);
        int ch = json_peek(c);
        if (ch == ',') {
            c->pos++;
            json_skip_ws(c);
            continue;
        }
        if (ch == ']') {
            c->pos++;
            return value_list(list);
        }
        runtime_error(rt, line, "json.decode: expected ',' or ']'");
        return value_invalid();
    }
}

static Value json_parse_object(Runtime *rt, int line, JsonCursor *c) {
    c->pos++;
    json_skip_ws(c);

    MapValue *map = map_new(VT_INVALID, NULL);
    if (!map) {
        runtime_error(rt, line, "out of memory");
        return value_invalid();
    }

    if (json_peek(c) == '}') {
        c->pos++;
        return value_map(map);
    }

    while (1) {
        if (json_peek(c) != '"') {
            runtime_error(rt, line, "json.decode: expected string key");
            return value_invalid();
        }
        char *key = json_parse_string_raw(rt, line, c);
        if (rt->has_error || !key) {
            return value_invalid();
        }

        json_skip_ws(c);
        if (json_peek(c) != ':') {
            free(key);
            runtime_error(rt, line, "json.decode: expected ':' after key");
            return value_invalid();
        }
        c->pos++;
        json_skip_ws(c);

        Value item = json_parse_value(rt, line, c);
        if (rt->has_error) {
            free(key);
            return value_invalid();
        }
        if (item.type == VT_VOID || item.type == VT_INVALID) {
            free(key);
            runtime_error(rt, line, "json.decode: object value cannot be %s", value_type_name(item.type));
            return value_invalid();
        }
        if (map->value_type == VT_INVALID) {
            map->value_type = item.type;
            if (item.type == VT_OBJECT && item.as.obj != NULL) {
                map->value_object_type = xstrdup(item.as.obj->type_name);
            }
        } else if (map->value_type != item.type) {
            free(key);
            runtime_error(rt, line, "json.decode: object values must have the same type");
            return value_invalid();
        } else if (map->value_type == VT_OBJECT && map->value_object_type != NULL &&
                   (item.as.obj == NULL || strcmp(map->value_object_type, item.as.obj->type_name) != 0)) {
            free(key);
            runtime_error(rt, line, "json.decode: object values must have the same object type");
            return value_invalid();
        }
        if (!map_set(map, key, item)) {
            free(key);
            runtime_error(rt, line, "out of memory");
            return value_invalid();
        }
        free(key);

        json_skip_ws(c);
        int ch = json_peek(c);
        if (ch == ',') {
            c->pos++;
            json_skip_ws(c);
            continue;
        }
        if (ch == '}') {
            c->pos++;
            return value_map(map);
        }
        runtime_error(rt, line, "json.decode: expected ',' or '}'");
        return value_invalid();
    }
}

static Value json_parse_value(Runtime *rt, int line, JsonCursor *c) {
    json_skip_ws(c);
    int ch = json_peek(c);
    if (ch < 0) {
        runtime_error(rt, line, "json.decode: unexpected end of input");
        return value_invalid();
    }
    if (ch == '"') {
        char *s = json_parse_string_raw(rt, line, c);
        if (!s) {
            return value_invalid();
        }
        Value out = value_string(s);
        free(s);
        return out;
    }
    if (ch == '[') {
        return json_parse_array(rt, line, c);
    }
    if (ch == '{') {
        return json_parse_object(rt, line, c);
    }
    if (ch == 't') {
        if (json_match_literal(c, "true")) {
            return value_bool(1);
        }
        runtime_error(rt, line, "json.decode: invalid token");
        return value_invalid();
    }
    if (ch == 'f') {
        if (json_match_literal(c, "false")) {
            return value_bool(0);
        }
        runtime_error(rt, line, "json.decode: invalid token");
        return value_invalid();
    }
    if (ch == 'n') {
        runtime_error(rt, line, "json.decode: null is not supported in strict mode");
        return value_invalid();
    }
    if (ch == '-' || isdigit((unsigned char)ch)) {
        int start = c->pos;
        if (c->src[c->pos] == '-') {
            c->pos++;
        }
        while (c->pos < c->len && isdigit((unsigned char)c->src[c->pos])) {
            c->pos++;
        }
        int has_dot = 0;
        if (c->pos < c->len && c->src[c->pos] == '.') {
            has_dot = 1;
            c->pos++;
            while (c->pos < c->len && isdigit((unsigned char)c->src[c->pos])) {
                c->pos++;
            }
        }
        if (c->pos < c->len && (c->src[c->pos] == 'e' || c->src[c->pos] == 'E')) {
            has_dot = 1;
            c->pos++;
            if (c->pos < c->len && (c->src[c->pos] == '+' || c->src[c->pos] == '-')) {
                c->pos++;
            }
            while (c->pos < c->len && isdigit((unsigned char)c->src[c->pos])) {
                c->pos++;
            }
        }
        char *num = xstrndup(c->src + start, (size_t)(c->pos - start));
        if (!has_dot) {
            char *end = NULL;
            errno = 0;
            long long iv = strtoll(num, &end, 10);
            if (errno == 0 && end && *end == '\0') {
                free(num);
                return value_int(iv);
            }
        }
        char *end = NULL;
