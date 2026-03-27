typedef struct Value Value;

typedef struct {
    char *name;
    TypeRef type;
} ObjectFieldDef;

typedef struct {
    char *type_name;
    ObjectFieldDef *fields;
    Value *values;
    int field_count;
} ObjectValue;

typedef struct {
    char *interface_name;
    InterfaceDecl *iface_decl;
    ObjectValue *obj;
} InterfaceValue;

typedef struct {
    FuncDecl *fn;
    struct Env *closure;
    struct Module *owner;
    char *display_name;
} FunctionValue;

typedef struct {
    ValueType elem_type;
    char *elem_object_type;
    Value *items;
    int count;
    int cap;
} ListValue;

typedef struct {
    ValueType value_type;
    char *value_object_type;
    char **keys;
    Value *values;
    int count;
    int cap;
} MapValue;

typedef struct {
    ValueType elem_type;
    char *elem_object_type;
    Value *items;
    int count;
    int cap;
    int max_buffer;
    int closed;
    pthread_mutex_t mu;
    pthread_cond_t cv;
} ChannelValue;

typedef struct {
    Value *items;
    int count;
} MultiValue;

typedef struct {
    long long handle;
    regex_t compiled;
    int active;
} RegexHandleEntry;

struct Value {
    ValueType type;
    union {
        long long i;
        double f;
        char *s;
        int b;
        ListValue *list;
        MapValue *map;
        ChannelValue *chan;
        MultiValue *multi;
        ObjectValue *obj;
        InterfaceValue *iface;
        FunctionValue *func;
    } as;
};

typedef struct {
    char *name;
    Value value;
    int is_const;
} VarEntry;

typedef struct Env {
    VarEntry *entries;
    int count;
    int cap;
    struct Env *parent;
} Env;

typedef struct {
    int returned;
    int broke;
    int continued;
    const char *loop_label;
    Value value;
} ExecResult;

typedef struct Module Module;

struct Module {
    char *name;
    char *namespace;
    char *base_dir;
    Program *prog;
    Env globals;
    int loaded;
    int loading;
    int native_kind;
};

typedef struct {
    char *name;
    FuncDecl *fn;
    Env *globals;
    Module *owner;
} FuncBinding;

typedef struct {
    char *name;
    TypeDecl *type_decl;
    Module *owner;
} TypeBinding;

typedef struct {
    char *name;
    InterfaceDecl *iface_decl;
    Module *owner;
} InterfaceBinding;

typedef struct {
    char *name;
    char *qualified_name;
} NativeAliasBinding;

typedef struct {
    char *alias;
    Module *module;
} ModuleAliasBinding;

typedef struct {
    char *name;
    int line;
} CallFrame;

typedef struct {
    Expr **calls;
    int count;
    int cap;
} DeferFrame;

typedef struct {
    char *display_name;
    FuncDecl *fn;
    Env *fn_parent_env;
    Module *fn_owner;
    Value *args;
    char **arg_names;
    int arg_count;
    int line;
} KostroutineTask;

typedef struct {
    Module *modules;
    int module_count;
    int module_cap;
    FuncBinding *bindings;
    int binding_count;
    int binding_cap;
    TypeBinding *type_bindings;
    int type_binding_count;
    int type_binding_cap;
    InterfaceBinding *interface_bindings;
    int interface_binding_count;
    int interface_binding_cap;
    NativeAliasBinding *native_aliases;
    int native_alias_count;
    int native_alias_cap;
    ModuleAliasBinding *module_aliases;
    int module_alias_count;
    int module_alias_cap;
    CallFrame *call_frames;
    int call_frame_count;
    int call_frame_cap;
    DeferFrame *defer_frames;
    int defer_frame_count;
    int defer_frame_cap;
    KostroutineTask *kostroutine_tasks;
    int kostroutine_count;
    int kostroutine_cap;
    pthread_t *kostroutine_threads;
    void **kostroutine_thread_ctxs;
    int kostroutine_thread_count;
    int kostroutine_thread_cap;
    int kostroutine_draining;
    int check_only;
    int debug_enabled;
    int debug_step_mode;
    int *debug_breakpoints;
    int debug_breakpoint_count;
    int debug_breakpoint_cap;
    unsigned long long rand_state;
    int rand_seeded;
    RegexHandleEntry *regex_entries;
    int regex_entry_count;
    int regex_entry_cap;
    long long regex_next_handle;
    int has_error;
    char err[2048];
} Runtime;

enum {
    NATIVE_NONE = 0,
    NATIVE_MATH = 1,
    NATIVE_TIME = 2,
    NATIVE_JSON = 3,
    NATIVE_FS = 4,
    NATIVE_HTTP = 5,
    NATIVE_RAND = 6,
    NATIVE_OS = 7,
    NATIVE_PATH = 8,
    NATIVE_RE = 9
};

static char *xstrndup(const char *s, size_t n) {
    char *out = (char *)malloc(n + 1);
    if (!out) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static char *xstrdup(const char *s) {
    return xstrndup(s, strlen(s));
}

static const char *func_signature_name(FuncDecl *fn, char *buf, size_t buf_cap);
static long long runtime_now_ms(void);
static void runtime_sleep_ms(long long ms);
static void runtime_error(Runtime *rt, int line, const char *fmt, ...);

static const char *value_type_name(ValueType t) {
    switch (t) {
    case VT_INT:
        return "int";
    case VT_FLOAT:
        return "float";
    case VT_STRING:
        return "string";
    case VT_ERROR:
        return "error";
    case VT_BOOL:
        return "bool";
    case VT_LIST:
        return "list";
    case VT_MAP:
        return "map";
    case VT_CHANNEL:
        return "chan";
    case VT_OBJECT:
        return "object";
    case VT_INTERFACE:
        return "interface";
    case VT_FUNCTION:
        return "func";
    case VT_MULTI:
        return "multi";
    case VT_VOID:
        return "void";
    default:
        return "invalid";
    }
}

static const char *type_ref_name(TypeRef t, char *buf, size_t buf_cap) {
    if (t.kind == VT_OBJECT && t.custom_name != NULL) {
        snprintf(buf, buf_cap, "%s", t.custom_name);
        return buf;
    }
    if (t.kind == VT_LIST && t.list_elem_type != NULL) {
        char ebuf[128];
        const char *ename = type_ref_name(*t.list_elem_type, ebuf, sizeof(ebuf));
        snprintf(buf, buf_cap, "list[%s]", ename);
        return buf;
    }
    if (t.kind == VT_MAP && t.map_value_type != NULL) {
        char kbuf[128];
        char vbuf[128];
        const char *kname = "string";
        if (t.map_key_type != NULL) {
            kname = type_ref_name(*t.map_key_type, kbuf, sizeof(kbuf));
        }
        const char *vname = type_ref_name(*t.map_value_type, vbuf, sizeof(vbuf));
        snprintf(buf, buf_cap, "map[%s, %s]", kname, vname);
        return buf;
    }
    if (t.kind == VT_CHANNEL && t.chan_elem_type != NULL) {
        char ebuf[128];
        const char *ename = type_ref_name(*t.chan_elem_type, ebuf, sizeof(ebuf));
        snprintf(buf, buf_cap, "chan[%s]", ename);
        return buf;
    }
    if (t.kind == VT_FUNCTION && t.func_sig != NULL) {
        size_t used = 0;
        int w = snprintf(buf, buf_cap, "func(");
        if (w < 0) {
            return "func";
        }
        used = (size_t)w < buf_cap ? (size_t)w : buf_cap;
        for (int i = 0; i < t.func_sig->param_count; i++) {
            char pbuf[128];
            const char *pname = type_ref_name(t.func_sig->params[i], pbuf, sizeof(pbuf));
            w = snprintf(buf + used, used < buf_cap ? buf_cap - used : 0, "%s%s", i > 0 ? ", " : "", pname);
            if (w < 0) {
                return "func";
            }
            used += (size_t)w;
        }
        const char *rname = "void";
        char rbuf[128];
        if (t.func_sig->return_type != NULL) {
            rname = type_ref_name(*t.func_sig->return_type, rbuf, sizeof(rbuf));
        }
        (void)snprintf(buf + (used < buf_cap ? used : buf_cap), used < buf_cap ? buf_cap - used : 0, ") -> %s", rname);
        return buf;
    }
    if (t.kind == VT_MULTI && t.multi_sig != NULL) {
        size_t used = 0;
        int w = snprintf(buf, buf_cap, "(");
        if (w < 0) {
            return "(...)";
        }
        used = (size_t)w < buf_cap ? (size_t)w : buf_cap;
        for (int i = 0; i < t.multi_sig->count; i++) {
            char tbuf[128];
            const char *item = type_ref_name(t.multi_sig->items[i], tbuf, sizeof(tbuf));
            w = snprintf(buf + used, used < buf_cap ? buf_cap - used : 0, "%s%s", i > 0 ? ", " : "", item);
            if (w < 0) {
                return "(...)";
            }
            used += (size_t)w;
        }
        (void)snprintf(buf + (used < buf_cap ? used : buf_cap), used < buf_cap ? buf_cap - used : 0, ")");
        return buf;
    }
    return value_type_name(t.kind);
}

static const char *value_name(Value v, char *buf, size_t buf_cap) {
    if (v.type == VT_OBJECT && v.as.obj != NULL && v.as.obj->type_name != NULL) {
        snprintf(buf, buf_cap, "%s", v.as.obj->type_name);
        return buf;
    }
    if (v.type == VT_INTERFACE && v.as.iface != NULL && v.as.iface->interface_name != NULL) {
        snprintf(buf, buf_cap, "%s", v.as.iface->interface_name);
        return buf;
    }
    if (v.type == VT_FUNCTION && v.as.func != NULL) {
        return func_signature_name(v.as.func->fn, buf, buf_cap);
    }
    if (v.type == VT_MULTI && v.as.multi != NULL) {
        size_t used = 0;
        int w = snprintf(buf, buf_cap, "(");
        if (w < 0) {
            return "(...)";
        }
        used = (size_t)w < buf_cap ? (size_t)w : buf_cap;
        for (int i = 0; i < v.as.multi->count; i++) {
            char item_buf[128];
            const char *item = value_name(v.as.multi->items[i], item_buf, sizeof(item_buf));
            w = snprintf(buf + used, used < buf_cap ? buf_cap - used : 0, "%s%s", i > 0 ? ", " : "", item);
            if (w < 0) {
                return "(...)";
            }
            used += (size_t)w;
        }
        (void)snprintf(buf + (used < buf_cap ? used : buf_cap), used < buf_cap ? buf_cap - used : 0, ")");
        return buf;
    }
    if (v.type == VT_LIST && v.as.list != NULL && v.as.list->elem_type == VT_OBJECT && v.as.list->elem_object_type) {
        snprintf(buf, buf_cap, "list of %s", v.as.list->elem_object_type);
        return buf;
    }
    if (v.type == VT_MAP && v.as.map != NULL && v.as.map->value_type == VT_OBJECT && v.as.map->value_object_type) {
        snprintf(buf, buf_cap, "map of %s", v.as.map->value_object_type);
        return buf;
    }
    if (v.type == VT_CHANNEL && v.as.chan != NULL && v.as.chan->elem_type == VT_OBJECT && v.as.chan->elem_object_type) {
        snprintf(buf, buf_cap, "chan of %s", v.as.chan->elem_object_type);
        return buf;
    }
    return value_type_name(v.type);
}

static int value_matches_type(Value v, TypeRef t) {
    if (t.kind == VT_MULTI) {
        if (v.type != VT_MULTI || v.as.multi == NULL || t.multi_sig == NULL) {
            return 0;
        }
        if (v.as.multi->count != t.multi_sig->count) {
            return 0;
        }
        for (int i = 0; i < t.multi_sig->count; i++) {
            if (!value_matches_type(v.as.multi->items[i], t.multi_sig->items[i])) {
                return 0;
            }
        }
        return 1;
    }
    if (t.kind == VT_OBJECT) {
        if (v.type != VT_OBJECT || v.as.obj == NULL || t.custom_name == NULL) {
            return 0;
        }
        return strcmp(v.as.obj->type_name, t.custom_name) == 0;
    }
    if (t.kind == VT_LIST) {
        if (v.type != VT_LIST || v.as.list == NULL) {
            return 0;
        }
        if (t.list_elem_type == NULL) {
            return 1;
        }
        TypeRef elem_t = *t.list_elem_type;
        if (v.as.list->elem_type == VT_INVALID) {
            return 1;
        }
        if (elem_t.kind == VT_OBJECT) {
            if (v.as.list->elem_type != VT_OBJECT) {
                return 0;
            }
            if (elem_t.custom_name != NULL && v.as.list->elem_object_type != NULL &&
                strcmp(v.as.list->elem_object_type, elem_t.custom_name) != 0) {
                return 0;
            }
        } else if (v.as.list->elem_type != elem_t.kind) {
            return 0;
        }
        for (int i = 0; i < v.as.list->count; i++) {
            if (!value_matches_type(v.as.list->items[i], elem_t)) {
                return 0;
            }
        }
        return 1;
    }
    if (t.kind == VT_MAP) {
        if (v.type != VT_MAP || v.as.map == NULL) {
            return 0;
        }
        if (t.map_value_type == NULL) {
            return 1;
        }
        TypeRef value_t = *t.map_value_type;
        if (v.as.map->value_type == VT_INVALID) {
            return 1;
        }
        if (value_t.kind == VT_OBJECT) {
            if (v.as.map->value_type != VT_OBJECT) {
                return 0;
            }
            if (value_t.custom_name != NULL && v.as.map->value_object_type != NULL &&
                strcmp(v.as.map->value_object_type, value_t.custom_name) != 0) {
                return 0;
            }
        } else if (v.as.map->value_type != value_t.kind) {
            return 0;
        }
        for (int i = 0; i < v.as.map->count; i++) {
            if (!value_matches_type(v.as.map->values[i], value_t)) {
                return 0;
            }
        }
        return 1;
    }
    if (t.kind == VT_CHANNEL) {
        if (v.type != VT_CHANNEL || v.as.chan == NULL) {
            return 0;
        }
        if (t.chan_elem_type == NULL) {
            return 1;
        }
        TypeRef elem_t = *t.chan_elem_type;
        if (v.as.chan->elem_type == VT_INVALID) {
            return 1;
        }
        if (elem_t.kind == VT_OBJECT) {
            if (v.as.chan->elem_type != VT_OBJECT) {
                return 0;
            }
            if (elem_t.custom_name != NULL && v.as.chan->elem_object_type != NULL &&
                strcmp(v.as.chan->elem_object_type, elem_t.custom_name) != 0) {
                return 0;
            }
        } else if (v.as.chan->elem_type != elem_t.kind) {
            return 0;
        }
        for (int i = 0; i < v.as.chan->count; i++) {
            if (!value_matches_type(v.as.chan->items[i], elem_t)) {
                return 0;
            }
        }
        return 1;
    }
    return v.type == t.kind;
}

static Value value_invalid(void) {
    Value v;
    v.type = VT_INVALID;
    return v;
}

static Value value_void(void) {
    Value v;
    v.type = VT_VOID;
    return v;
}

static Value value_int(long long x) {
    Value v;
    v.type = VT_INT;
    v.as.i = x;
    return v;
}

static Value value_float(double x) {
    Value v;
    v.type = VT_FLOAT;
    v.as.f = x;
    return v;
}

static Value value_string(const char *s) {
    Value v;
    v.type = VT_STRING;
    v.as.s = xstrdup(s);
    return v;
}

static Value value_error(const char *msg) {
    Value v;
    v.type = VT_ERROR;
    v.as.s = xstrdup(msg ? msg : "");
    return v;
}

static Value value_bool(int b) {
    Value v;
    v.type = VT_BOOL;
    v.as.b = b ? 1 : 0;
    return v;
}

static Value value_list(ListValue *list) {
    Value v;
    v.type = VT_LIST;
    v.as.list = list;
    return v;
}

static Value value_map(MapValue *map) {
    Value v;
    v.type = VT_MAP;
    v.as.map = map;
    return v;
}

static Value value_channel(ChannelValue *ch) {
    Value v;
    v.type = VT_CHANNEL;
    v.as.chan = ch;
    return v;
}

static Value value_multi(Value *items, int count) {
    Value v;
    v.type = VT_MULTI;
    v.as.multi = (MultiValue *)calloc(1, sizeof(MultiValue));
    if (!v.as.multi) {
        v.type = VT_INVALID;
        return v;
    }
    if (count <= 0) {
        v.as.multi->items = NULL;
        v.as.multi->count = 0;
        return v;
    }
    v.as.multi->items = (Value *)calloc((size_t)count, sizeof(Value));
    if (!v.as.multi->items) {
        v.type = VT_INVALID;
        return v;
    }
    memcpy(v.as.multi->items, items, (size_t)count * sizeof(Value));
    v.as.multi->count = count;
    return v;
}

static Value value_object(ObjectValue *obj) {
    Value v;
    v.type = VT_OBJECT;
    v.as.obj = obj;
    return v;
}

static Value value_interface(InterfaceDecl *iface, ObjectValue *obj) {
    Value v;
    v.type = VT_INTERFACE;
    v.as.iface = (InterfaceValue *)calloc(1, sizeof(InterfaceValue));
    if (!v.as.iface) {
        v.type = VT_INVALID;
        return v;
    }
    v.as.iface->interface_name = xstrdup(iface->name);
    v.as.iface->iface_decl = iface;
    v.as.iface->obj = obj;
    return v;
}

static Value value_function(FuncDecl *fn, Env *closure, Module *owner, const char *display_name) {
    Value v;
    v.type = VT_FUNCTION;
    v.as.func = (FunctionValue *)calloc(1, sizeof(FunctionValue));
    if (!v.as.func) {
        v.type = VT_INVALID;
        return v;
    }
    v.as.func->fn = fn;
    v.as.func->closure = closure;
    v.as.func->owner = owner;
    v.as.func->display_name = xstrdup(display_name ? display_name : "<lambda>");
    return v;
}

static int values_share_shape(Value a, Value b) {
    if (a.type != b.type) {
        return 0;
    }
    if (a.type == VT_OBJECT) {
        if (!a.as.obj || !b.as.obj) {
            return a.as.obj == b.as.obj;
        }
        return strcmp(a.as.obj->type_name, b.as.obj->type_name) == 0;
    }
    if (a.type == VT_INTERFACE) {
        const char *an = a.as.iface && a.as.iface->interface_name ? a.as.iface->interface_name : NULL;
        const char *bn = b.as.iface && b.as.iface->interface_name ? b.as.iface->interface_name : NULL;
        if (!an || !bn) {
            return an == bn;
        }
        return strcmp(an, bn) == 0;
    }
    if (a.type == VT_MULTI) {
        if (!a.as.multi || !b.as.multi) {
            return a.as.multi == b.as.multi;
        }
        if (a.as.multi->count != b.as.multi->count) {
            return 0;
        }
        for (int i = 0; i < a.as.multi->count; i++) {
            if (!values_share_shape(a.as.multi->items[i], b.as.multi->items[i])) {
                return 0;
            }
        }
        return 1;
    }
    return 1;
}

static int type_ref_equal_local(TypeRef a, TypeRef b) {
    if (a.kind != b.kind) {
        return 0;
    }
    if (a.kind == VT_LIST) {
        if (a.list_elem_type == NULL || b.list_elem_type == NULL) {
            return a.list_elem_type == b.list_elem_type;
        }
        return type_ref_equal_local(*a.list_elem_type, *b.list_elem_type);
    }
    if (a.kind == VT_MAP) {
        if (a.map_key_type == NULL || b.map_key_type == NULL) {
            if (a.map_key_type != b.map_key_type) {
                return 0;
            }
        } else if (!type_ref_equal_local(*a.map_key_type, *b.map_key_type)) {
            return 0;
        }
        if (a.map_value_type == NULL || b.map_value_type == NULL) {
            return a.map_value_type == b.map_value_type;
        }
        return type_ref_equal_local(*a.map_value_type, *b.map_value_type);
    }
    if (a.kind == VT_CHANNEL) {
        if (a.chan_elem_type == NULL || b.chan_elem_type == NULL) {
            return a.chan_elem_type == b.chan_elem_type;
        }
        return type_ref_equal_local(*a.chan_elem_type, *b.chan_elem_type);
    }
    if (a.kind == VT_MULTI) {
        if (a.multi_sig == NULL || b.multi_sig == NULL) {
            return a.multi_sig == b.multi_sig;
        }
        if (a.multi_sig->count != b.multi_sig->count) {
            return 0;
        }
        for (int i = 0; i < a.multi_sig->count; i++) {
            if (!type_ref_equal_local(a.multi_sig->items[i], b.multi_sig->items[i])) {
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
        if (!type_ref_equal_local(*a.func_sig->return_type, *b.func_sig->return_type)) {
            return 0;
        }
        if (a.func_sig->param_count != b.func_sig->param_count) {
            return 0;
        }
        for (int i = 0; i < a.func_sig->param_count; i++) {
            if (!type_ref_equal_local(a.func_sig->params[i], b.func_sig->params[i])) {
                return 0;
            }
        }
        return 1;
    }
    if (a.kind != VT_OBJECT) {
        return 1;
    }
    if (a.custom_name == NULL || b.custom_name == NULL) {
        return a.custom_name == b.custom_name;
    }
    return strcmp(a.custom_name, b.custom_name) == 0;
}

static int func_signature_equal(FuncDecl *a, FuncDecl *b) {
    if (!a || !b) {
        return 0;
    }
    if (a->param_count != b->param_count) {
        return 0;
    }
    if (!type_ref_equal_local(a->return_type, b->return_type)) {
        return 0;
    }
    for (int i = 0; i < a->param_count; i++) {
        if (!type_ref_equal_local(a->params[i].type, b->params[i].type)) {
            return 0;
        }
    }
    return 1;
}

static const char *func_signature_name(FuncDecl *fn, char *buf, size_t buf_cap) {
    size_t used = 0;
    int w = snprintf(buf, buf_cap, "func(");
    if (w < 0) {
        return "func(?)";
    }
    used = (size_t)w < buf_cap ? (size_t)w : buf_cap;
    for (int i = 0; fn && i < fn->param_count; i++) {
        char tbuf[128];
        const char *tname = type_ref_name(fn->params[i].type, tbuf, sizeof(tbuf));
        w = snprintf(buf + used, used < buf_cap ? buf_cap - used : 0, "%s%s", i > 0 ? ", " : "", tname);
        if (w < 0) {
            return "func(?)";
        }
        used += (size_t)w;
    }
    char rbuf[128];
    const char *rname = fn ? type_ref_name(fn->return_type, rbuf, sizeof(rbuf)) : "?";
    (void)snprintf(buf + (used < buf_cap ? used : buf_cap), used < buf_cap ? buf_cap - used : 0, ") -> %s", rname);
    return buf;
}
