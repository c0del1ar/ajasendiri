#include "cli/module.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <openssl/hmac.h>

static int has_aja_extension(const char *path) {
    size_t n = strlen(path);
    if (n < 4) {
        return 0;
    }
    return strcmp(path + (n - 4), ".aja") == 0;
}

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} TextBuf;

static int write_file(const char *path, const char *content, char *err, size_t err_cap);

static void tb_init(TextBuf *tb) {
    tb->cap = 128;
    tb->len = 0;
    tb->buf = (char *)malloc(tb->cap);
    if (!tb->buf) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    tb->buf[0] = '\0';
}

static void tb_ensure(TextBuf *tb, size_t extra) {
    if (tb->len + extra + 1 <= tb->cap) {
        return;
    }
    while (tb->len + extra + 1 > tb->cap) {
        tb->cap *= 2;
    }
    char *next = (char *)realloc(tb->buf, tb->cap);
    if (!next) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    tb->buf = next;
}

static void tb_push_n(TextBuf *tb, const char *s, size_t n) {
    tb_ensure(tb, n);
    memcpy(tb->buf + tb->len, s, n);
    tb->len += n;
    tb->buf[tb->len] = '\0';
}

static void tb_push_char(TextBuf *tb, char c) {
    tb_ensure(tb, 1);
    tb->buf[tb->len++] = c;
    tb->buf[tb->len] = '\0';
}

static char *tb_take(TextBuf *tb) {
    return tb->buf;
}

static int push_indent_level(int **stack, int *depth, int *cap, int value) {
    if (*depth + 1 > *cap) {
        *cap *= 2;
        int *next = (int *)realloc(*stack, (size_t)(*cap) * sizeof(int));
        if (!next) {
            return 0;
        }
        *stack = next;
    }
    (*stack)[(*depth)++] = value;
    return 1;
}

static int count_leading_spaces(const char *line) {
    int n = 0;
    while (line[n] == ' ') {
        n++;
    }
    return n;
}

static char *dup_n(const char *s, size_t n) {
    char *out = (char *)malloc(n + 1);
    if (!out) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static char *dup_s(const char *s) {
    return dup_n(s, strlen(s));
}

static char *join_path2(const char *a, const char *b) {
    size_t na = strlen(a);
    size_t nb = strlen(b);
    int need_slash = (na > 0 && a[na - 1] != '/');
    char *out = (char *)malloc(na + nb + (size_t)need_slash + 1);
    if (!out) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    memcpy(out, a, na);
    if (need_slash) {
        out[na++] = '/';
    }
    memcpy(out + na, b, nb);
    out[na + nb] = '\0';
    return out;
}

static char *resolve_site_packages_root(const char *project_root, int *out_using_venv) {
    const char *venv = getenv("AJA_VENV");
    if (venv && venv[0] != '\0') {
        char *venv_root = NULL;
        if (out_using_venv) {
            *out_using_venv = 1;
        }
        if (venv[0] == '/') {
            venv_root = dup_s(venv);
        } else {
            venv_root = join_path2(project_root, venv);
        }
        char *site_root = join_path2(venv_root, "site-packages");
        free(venv_root);
        return site_root;
    }
    if (out_using_venv) {
        *out_using_venv = 0;
    }
    return join_path2(project_root, ".aja/site-packages");
}

static char *dirname_from_path2(const char *path) {
    const char *slash = strrchr(path, '/');
    if (!slash) {
        return dup_s(".");
    }
    if (slash == path) {
        return dup_s("/");
    }
    return dup_n(path, (size_t)(slash - path));
}

static int path_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static int is_regular_file_path(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    return S_ISREG(st.st_mode) ? 1 : 0;
}

static int ensure_dir(const char *path, char *err, size_t err_cap) {
    if (mkdir(path, 0755) == 0) {
        return 1;
    }
    if (errno == EEXIST) {
        return 1;
    }
    snprintf(err, err_cap, "cannot create directory %s: %s", path, strerror(errno));
    return 0;
}

static int ensure_dir_recursive(const char *path, char *err, size_t err_cap) {
    size_t n = strlen(path);
    if (n == 0) {
        return 1;
    }
    char *tmp = dup_s(path);
    size_t start = (tmp[0] == '/') ? 1 : 0;
    for (size_t i = start; tmp[i] != '\0'; i++) {
        if (tmp[i] != '/') {
            continue;
        }
        tmp[i] = '\0';
        if (tmp[0] != '\0' && !ensure_dir(tmp, err, err_cap)) {
            free(tmp);
            return 0;
        }
        tmp[i] = '/';
    }
    if (!ensure_dir(tmp, err, err_cap)) {
        free(tmp);
        return 0;
    }
    free(tmp);
    return 1;
}

static int copy_file_binary(const char *src, const char *dst, char *err, size_t err_cap) {
    FILE *in = fopen(src, "rb");
    if (!in) {
        snprintf(err, err_cap, "cannot open source %s", src);
        return 0;
    }
    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        snprintf(err, err_cap, "cannot open destination %s", dst);
        return 0;
    }

    char buf[4096];
    while (1) {
        size_t got = fread(buf, 1, sizeof(buf), in);
        if (got > 0 && fwrite(buf, 1, got, out) != got) {
            fclose(in);
            fclose(out);
            snprintf(err, err_cap, "failed writing %s", dst);
            return 0;
        }
        if (got < sizeof(buf)) {
            if (ferror(in)) {
                fclose(in);
                fclose(out);
                snprintf(err, err_cap, "failed reading %s", src);
                return 0;
            }
            break;
        }
    }

    fclose(in);
    if (fclose(out) != 0) {
        snprintf(err, err_cap, "failed closing %s", dst);
        return 0;
    }
    return 1;
}

static char *trim_ws(char *s) {
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

typedef struct {
    char *name;
    char *version;
    char *source;
    char *hash;
    char *sig;
    int line_no;
} DepSpec;

typedef struct {
    DepSpec *items;
    int count;
    int cap;
} DepSpecList;

static void dep_list_push(DepSpecList *list, DepSpec spec) {
    if (list->count + 1 > list->cap) {
        list->cap = list->cap == 0 ? 8 : list->cap * 2;
        DepSpec *next = (DepSpec *)realloc(list->items, (size_t)list->cap * sizeof(DepSpec));
        if (!next) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        list->items = next;
    }
    list->items[list->count++] = spec;
}

static void dep_list_free(DepSpecList *list) {
    for (int i = 0; i < list->count; i++) {
        free(list->items[i].name);
        free(list->items[i].version);
        free(list->items[i].source);
        free(list->items[i].hash);
        free(list->items[i].sig);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

static int valid_module_name(const char *name) {
    if (!name || name[0] == '\0') {
        return 0;
    }
    for (int i = 0; name[i] != '\0'; i++) {
        char c = name[i];
        if (isalnum((unsigned char)c) || c == '_' || c == '/') {
            continue;
        }
        return 0;
    }
    return 1;
}

static int valid_version(const char *version) {
    if (!version || version[0] == '\0') {
        return 0;
    }
    int dots = 0;
    int digit_run = 0;
    for (int i = 0; version[i] != '\0'; i++) {
        char c = version[i];
        if (isdigit((unsigned char)c)) {
            digit_run = 1;
            continue;
        }
        if (c == '.') {
            if (!digit_run) {
                return 0;
            }
            dots++;
            digit_run = 0;
            continue;
        }
        return 0;
    }
    if (!digit_run) {
        return 0;
    }
    return dots == 2;
}

typedef struct {
    int major;
    int minor;
    int patch;
} SemVer;

static int parse_semver3(const char *s, SemVer *out) {
    if (!s || !out) {
        return 0;
    }
    const char *p = s;
    char *end = NULL;
    long a = strtol(p, &end, 10);
    if (end == p || *end != '.' || a < 0) {
        return 0;
    }
    p = end + 1;
    long b = strtol(p, &end, 10);
    if (end == p || *end != '.' || b < 0) {
        return 0;
    }
    p = end + 1;
    long c = strtol(p, &end, 10);
    if (end == p || *end != '\0' || c < 0) {
        return 0;
    }
    out->major = (int)a;
    out->minor = (int)b;
    out->patch = (int)c;
    return 1;
}

static int parse_semver2_or_3(const char *s, SemVer *out, int *out_parts) {
    if (!s || !out || !out_parts) {
        return 0;
    }
    const char *p = s;
    char *end = NULL;
    long a = strtol(p, &end, 10);
    if (end == p || *end != '.' || a < 0) {
        return 0;
    }
    p = end + 1;
    long b = strtol(p, &end, 10);
    if (end == p || b < 0) {
        return 0;
    }
    if (*end == '\0') {
        out->major = (int)a;
        out->minor = (int)b;
        out->patch = 0;
        *out_parts = 2;
        return 1;
    }
    if (*end != '.') {
        return 0;
    }
    p = end + 1;
    long c = strtol(p, &end, 10);
    if (end == p || *end != '\0' || c < 0) {
        return 0;
    }
    out->major = (int)a;
    out->minor = (int)b;
    out->patch = (int)c;
    *out_parts = 3;
    return 1;
}

static int semver_cmp(const SemVer *a, const SemVer *b) {
    if (a->major != b->major) {
        return a->major < b->major ? -1 : 1;
    }
    if (a->minor != b->minor) {
        return a->minor < b->minor ? -1 : 1;
    }
    if (a->patch != b->patch) {
        return a->patch < b->patch ? -1 : 1;
    }
    return 0;
}

static int valid_version_selector(const char *selector) {
    if (!selector || selector[0] == '\0') {
        return 0;
    }
    if (valid_version(selector) || strcmp(selector, "latest") == 0 || strcmp(selector, "*") == 0) {
        return 1;
    }
    if (selector[0] == '^' || selector[0] == '~') {
        SemVer base;
        int parts = 0;
        return parse_semver2_or_3(selector + 1, &base, &parts);
    }
    return 0;
}

static int version_matches_selector(const char *version, const char *selector) {
    if (!valid_version(version) || !selector || selector[0] == '\0') {
        return 0;
    }
    if (strcmp(selector, "latest") == 0 || strcmp(selector, "*") == 0) {
        return 1;
    }
    if (valid_version(selector)) {
        return strcmp(version, selector) == 0;
    }

    SemVer v;
    if (!parse_semver3(version, &v)) {
        return 0;
    }

    if (selector[0] == '^') {
        SemVer base;
        SemVer upper;
        int parts = 0;
        if (!parse_semver2_or_3(selector + 1, &base, &parts)) {
            return 0;
        }
        upper.major = base.major + 1;
        upper.minor = 0;
        upper.patch = 0;
        return semver_cmp(&v, &base) >= 0 && semver_cmp(&v, &upper) < 0;
    }

    if (selector[0] == '~') {
        SemVer base;
        SemVer upper;
        int parts = 0;
        if (!parse_semver2_or_3(selector + 1, &base, &parts)) {
            return 0;
        }
        upper.major = base.major;
        upper.minor = base.minor + 1;
        upper.patch = 0;
        return semver_cmp(&v, &base) >= 0 && semver_cmp(&v, &upper) < 0;
    }

    return 0;
}

static int resolve_registry_version_selector(const char *registry_root, const char *module_name, const char *selector,
                                             char **out_version, char *err, size_t err_cap) {
    *out_version = NULL;
    if (!valid_version_selector(selector)) {
        snprintf(err, err_cap, "invalid version selector '%s' (use x.y.z, ^x.y(.z), ~x.y(.z), latest, or *)", selector);
        return 0;
    }

    char *module_dir = join_path2(registry_root, module_name);
    DIR *dir = opendir(module_dir);
    if (!dir) {
        snprintf(err, err_cap, "registry package not found: %s (%s)", module_name, module_dir);
        free(module_dir);
        return 0;
    }

    char *best = NULL;
    SemVer best_sv;
    int have_best = 0;
    struct dirent *ent = NULL;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }
        if (!valid_version(ent->d_name)) {
            continue;
        }
        if (!version_matches_selector(ent->d_name, selector)) {
            continue;
        }
        SemVer cur;
        if (!parse_semver3(ent->d_name, &cur)) {
            continue;
        }
        if (!have_best || semver_cmp(&cur, &best_sv) > 0) {
            free(best);
            best = dup_s(ent->d_name);
            best_sv = cur;
            have_best = 1;
        }
    }
    closedir(dir);
    free(module_dir);

    if (!have_best) {
        snprintf(err, err_cap, "registry package '%s' has no version matching selector '%s'", module_name, selector);
        free(best);
        return 0;
    }
    *out_version = best;
    return 1;
}

static char *module_name_from_path(const char *path) {
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    size_t n = strlen(base);
    if (n > 4 && strcmp(base + (n - 4), ".aja") == 0) {
        n -= 4;
    }
    return dup_n(base, n);
}

static int dep_list_contains_name(const DepSpecList *list, const char *name) {
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->items[i].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

static DepSpec *dep_list_find_by_name(const DepSpecList *list, const char *name) {
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->items[i].name, name) == 0) {
            return &list->items[i];
        }
    }
    return NULL;
}

static int valid_hash_hex16(const char *hash) {
    if (!hash || strlen(hash) != 16) {
        return 0;
    }
    for (int i = 0; i < 16; i++) {
        char c = hash[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            return 0;
        }
    }
    return 1;
}

static int valid_hash_hex64(const char *hash) {
    if (!hash || strlen(hash) != 64) {
        return 0;
    }
    for (int i = 0; i < 64; i++) {
        char c = hash[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            return 0;
        }
    }
    return 1;
}

static int hmac_sha256_hex(const unsigned char *data, size_t data_len, const char *key, char out_hex[65], char *err,
                           size_t err_cap) {
    if (!key || key[0] == '\0') {
        snprintf(err, err_cap, "empty signing key");
        return 0;
    }
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    unsigned char *ret = HMAC(EVP_sha256(), key, (int)strlen(key), data, data_len, digest, &digest_len);
    if (!ret || digest_len != 32) {
        snprintf(err, err_cap, "failed to compute HMAC-SHA256 signature");
        return 0;
    }
    static const char *hex = "0123456789abcdef";
    for (unsigned int i = 0; i < digest_len; i++) {
        out_hex[i * 2] = hex[(digest[i] >> 4) & 0xF];
        out_hex[i * 2 + 1] = hex[digest[i] & 0xF];
    }
    out_hex[64] = '\0';
    return 1;
}

static int ends_with(const char *s, const char *suffix) {
    size_t ns = strlen(s);
    size_t nx = strlen(suffix);
    if (nx > ns) {
        return 0;
    }
    return strcmp(s + (ns - nx), suffix) == 0;
}

static int is_registry_source(const char *source) {
    return strncmp(source, "registry://", strlen("registry://")) == 0;
}

static int parse_registry_source(const char *source, char **out_name, char **out_version) {
    const char *prefix = "registry://";
    if (strncmp(source, prefix, strlen(prefix)) != 0) {
        return 0;
    }
    const char *rest = source + strlen(prefix);
    const char *slash = strrchr(rest, '/');
    if (!slash || slash == rest || slash[1] == '\0') {
        return 0;
    }
    char *name = dup_n(rest, (size_t)(slash - rest));
    char *version = dup_s(slash + 1);
    if (!valid_module_name(name) || !valid_version(version)) {
        free(name);
        free(version);
        return 0;
    }
    *out_name = name;
    *out_version = version;
    return 1;
}

static char *registry_source_for(const char *name, const char *version) {
    TextBuf tb;
    tb_init(&tb);
    tb_push_n(&tb, "registry://", strlen("registry://"));
    tb_push_n(&tb, name, strlen(name));
    tb_push_char(&tb, '/');
    tb_push_n(&tb, version, strlen(version));
    return tb_take(&tb);
}

static char *registry_root_from_project_root(const char *root) {
    const char *env = getenv("AJA_REGISTRY");
    if (env && env[0] != '\0') {
        if (env[0] == '/') {
            return dup_s(env);
        }
        return join_path2(root, env);
    }
    return join_path2(root, ".aja/registry");
}

static char *registry_module_path(const char *registry_root, const char *name, const char *version) {
    char *name_dir = join_path2(registry_root, name);
    char *version_dir = join_path2(name_dir, version);
    char *module_path = join_path2(version_dir, "module.aja");
    free(name_dir);
    free(version_dir);
    return module_path;
}

static char *registry_meta_path(const char *registry_root, const char *name, const char *version) {
    char *name_dir = join_path2(registry_root, name);
    char *version_dir = join_path2(name_dir, version);
    char *meta_path = join_path2(version_dir, "module.meta");
    free(name_dir);
    free(version_dir);
    return meta_path;
}

static char *sanitize_name_for_filename(const char *name) {
    size_t n = strlen(name);
    char *out = dup_s(name);
    for (size_t i = 0; i < n; i++) {
        if (out[i] == '/' || out[i] == '\\') {
            out[i] = '_';
        }
    }
    return out;
}

static int parse_dep_spec(const char *ctx, char *trimmed, int line_no, DepSpec *spec, char *err, size_t err_cap) {
    memset(spec, 0, sizeof(*spec));
    spec->line_no = line_no;

    char *at = strchr(trimmed, '@');
    if (!at) {
        snprintf(err, err_cap, "%s line %d: expected 'name==version @ path'", ctx, line_no);
        return 0;
    }
    *at = '\0';
    char *left = trim_ws(trimmed);
    char *src = trim_ws(at + 1);
    if (left[0] == '\0' || src[0] == '\0') {
        snprintf(err, err_cap, "%s line %d: expected 'name==version @ path'", ctx, line_no);
        return 0;
    }

    char *eq = strstr(left, "==");
    if (!eq) {
        snprintf(err, err_cap, "%s line %d: expected version pin like 'name==1.2.3 @ path'", ctx, line_no);
        return 0;
    }
    *eq = '\0';
    char *name = trim_ws(left);
    char *version = trim_ws(eq + 2);
    if (name[0] == '\0' || version[0] == '\0' || strstr(version, "==")) {
        snprintf(err, err_cap, "%s line %d: invalid 'name==version' segment", ctx, line_no);
        return 0;
    }
    if (!valid_module_name(name)) {
        snprintf(err, err_cap, "%s line %d: invalid module name '%s'", ctx, line_no, name);
        return 0;
    }
    if (!valid_version(version)) {
        snprintf(err, err_cap, "%s line %d: invalid version '%s' (expected x.y.z)", ctx, line_no, version);
        return 0;
    }
    spec->name = dup_s(name);
    spec->version = dup_s(version);
    spec->source = dup_s(src);
    return 1;
}

static int parse_requirements_text(const char *content, DepSpecList *out, char *err, size_t err_cap) {
    char *copy = dup_s(content);
    char *cur = copy;
    int line_no = 1;
    while (cur && *cur != '\0') {
        char *line = cur;
        char *nl = strchr(cur, '\n');
        if (nl) {
            *nl = '\0';
            cur = nl + 1;
        } else {
            cur = NULL;
        }
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\r') {
            line[len - 1] = '\0';
        }

        char *trimmed = trim_ws(line);
        if (trimmed[0] == '\0' || trimmed[0] == '#') {
            line_no++;
            continue;
        }

        DepSpec spec;
        if (!parse_dep_spec("requirements", trimmed, line_no, &spec, err, err_cap)) {
            free(copy);
            dep_list_free(out);
            return 0;
        }
        if (dep_list_contains_name(out, spec.name)) {
            free(spec.name);
            free(spec.version);
            free(spec.source);
            snprintf(err, err_cap, "requirements line %d: duplicate dependency '%s'", line_no, trimmed);
            free(copy);
            dep_list_free(out);
            return 0;
        }
        dep_list_push(out, spec);
        line_no++;
    }
    free(copy);
    return 1;
}

static int parse_lock_text(const char *content, DepSpecList *out, char *err, size_t err_cap) {
    char *copy = dup_s(content);
    char *cur = copy;
    int line_no = 1;
    while (cur && *cur != '\0') {
        char *line = cur;
        char *nl = strchr(cur, '\n');
        if (nl) {
            *nl = '\0';
            cur = nl + 1;
        } else {
            cur = NULL;
        }
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\r') {
            line[len - 1] = '\0';
        }

        char *trimmed = trim_ws(line);
        if (trimmed[0] == '\0' || trimmed[0] == '#') {
            line_no++;
            continue;
        }

        char *pipe = strstr(trimmed, "|");
        if (!pipe) {
            snprintf(err, err_cap, "lock line %d: expected '| hash=<hex16>' suffix", line_no);
            free(copy);
            dep_list_free(out);
            return 0;
        }
        *pipe = '\0';
        char *left = trim_ws(trimmed);
        char *right = trim_ws(pipe + 1);
        if (strncmp(right, "hash=", 5) != 0) {
            snprintf(err, err_cap, "lock line %d: expected 'hash=<hex16>'", line_no);
            free(copy);
            dep_list_free(out);
            return 0;
        }
        char *hash = trim_ws(right + 5);
        if (!valid_hash_hex16(hash)) {
            snprintf(err, err_cap, "lock line %d: invalid hash '%s'", line_no, hash);
            free(copy);
            dep_list_free(out);
            return 0;
        }

        DepSpec spec;
        if (!parse_dep_spec("lock", left, line_no, &spec, err, err_cap)) {
            free(copy);
            dep_list_free(out);
            return 0;
        }
        if (dep_list_contains_name(out, spec.name)) {
            free(spec.name);
            free(spec.version);
            free(spec.source);
            snprintf(err, err_cap, "lock line %d: duplicate dependency '%s'", line_no, left);
            free(copy);
            dep_list_free(out);
            return 0;
        }
        spec.hash = dup_s(hash);
        dep_list_push(out, spec);
        line_no++;
    }
    free(copy);
    return 1;
}

static int parse_package_text(const char *content, DepSpec *spec, char **out_exports, char **out_source, char *err,
                              size_t err_cap) {
    memset(spec, 0, sizeof(*spec));
    *out_exports = NULL;
    *out_source = NULL;

    const char *magic = "AJA_PKG_V1\n";
    size_t magic_len = strlen(magic);
    if (strncmp(content, magic, magic_len) != 0) {
        snprintf(err, err_cap, "invalid package: missing AJA_PKG_V1 header");
        return 0;
    }

    const char *sep = strstr(content, "\n---\n");
    if (!sep) {
        snprintf(err, err_cap, "invalid package: missing body separator");
        return 0;
    }

    char *header = dup_n(content + magic_len, (size_t)(sep - (content + magic_len)));
    char *source = dup_s(sep + 5);
    char *cur = header;
    while (cur && *cur != '\0') {
        char *line = cur;
        char *nl = strchr(cur, '\n');
        if (nl) {
            *nl = '\0';
            cur = nl + 1;
        } else {
            cur = NULL;
        }
        char *trimmed = trim_ws(line);
        if (trimmed[0] == '\0' || trimmed[0] == '#') {
            continue;
        }
        char *eq = strchr(trimmed, '=');
        if (!eq) {
            continue;
        }
        *eq = '\0';
        char *key = trim_ws(trimmed);
