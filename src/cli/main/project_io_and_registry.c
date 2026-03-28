        char *val = trim_ws(eq + 1);
        if (strcmp(key, "name") == 0) {
            free(spec->name);
            spec->name = dup_s(val);
        } else if (strcmp(key, "version") == 0) {
            free(spec->version);
            spec->version = dup_s(val);
        } else if (strcmp(key, "hash") == 0) {
            free(spec->hash);
            spec->hash = dup_s(val);
        } else if (strcmp(key, "sig") == 0) {
            free(spec->sig);
            spec->sig = dup_s(val);
        } else if (strcmp(key, "exports") == 0) {
            free(*out_exports);
            *out_exports = dup_s(val);
        }
    }

    if (!spec->name || !spec->version || !spec->hash) {
        free(spec->name);
        free(spec->version);
        free(spec->hash);
        free(spec->sig);
        spec->name = NULL;
        spec->version = NULL;
        spec->hash = NULL;
        spec->sig = NULL;
        free(header);
        free(source);
        free(*out_exports);
        *out_exports = NULL;
        snprintf(err, err_cap, "invalid package: missing one of name/version/hash");
        return 0;
    }
    if (!valid_module_name(spec->name)) {
        char *bad_name = dup_s(spec->name);
        free(spec->name);
        free(spec->version);
        free(spec->hash);
        free(spec->sig);
        spec->name = NULL;
        spec->version = NULL;
        spec->hash = NULL;
        spec->sig = NULL;
        free(header);
        free(source);
        free(*out_exports);
        *out_exports = NULL;
        snprintf(err, err_cap, "invalid package: bad module name '%s'", bad_name);
        free(bad_name);
        return 0;
    }
    if (!valid_version(spec->version)) {
        char *bad_version = dup_s(spec->version);
        free(spec->name);
        free(spec->version);
        free(spec->hash);
        free(spec->sig);
        spec->name = NULL;
        spec->version = NULL;
        spec->hash = NULL;
        spec->sig = NULL;
        free(header);
        free(source);
        free(*out_exports);
        *out_exports = NULL;
        snprintf(err, err_cap, "invalid package: bad version '%s'", bad_version);
        free(bad_version);
        return 0;
    }
    if (!valid_hash_hex(spec->hash)) {
        char *bad_hash = dup_s(spec->hash);
        free(spec->name);
        free(spec->version);
        free(spec->hash);
        free(spec->sig);
        spec->name = NULL;
        spec->version = NULL;
        spec->hash = NULL;
        spec->sig = NULL;
        free(header);
        free(source);
        free(*out_exports);
        *out_exports = NULL;
        snprintf(err, err_cap, "invalid package: bad hash '%s'", bad_hash);
        free(bad_hash);
        return 0;
    }
    if (spec->sig && spec->sig[0] != '\0' && !valid_hash_hex64(spec->sig)) {
        char *bad_sig = dup_s(spec->sig);
        free(spec->name);
        free(spec->version);
        free(spec->hash);
        free(spec->sig);
        spec->name = NULL;
        spec->version = NULL;
        spec->hash = NULL;
        spec->sig = NULL;
        free(header);
        free(source);
        free(*out_exports);
        *out_exports = NULL;
        snprintf(err, err_cap, "invalid package: bad signature '%s'", bad_sig);
        free(bad_sig);
        return 0;
    }

    *out_source = source;
    free(header);
    return 1;
}

static char *find_project_root(const char *start_dir) {
    char *cur = dup_s(start_dir);
    char *fallback = dup_s(start_dir);
    while (1) {
        char *a = join_path2(cur, "aja.toml");
        char *b = join_path2(cur, "requirements.txt");
        char *c = join_path2(cur, ".aja");
        int found = path_exists(a) || path_exists(b) || path_exists(c);
        free(a);
        free(b);
        free(c);
        if (found) {
            free(fallback);
            return cur;
        }
        char *parent = dirname_from_path2(cur);
        if (strcmp(parent, cur) == 0) {
            free(parent);
            free(cur);
            return fallback;
        }
        free(cur);
        cur = parent;
    }
}

static char *read_file(const char *path, char *err, size_t err_cap) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(err, err_cap, "read error: cannot open %s", path);
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        snprintf(err, err_cap, "read error: cannot seek %s", path);
        return NULL;
    }
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        snprintf(err, err_cap, "read error: cannot tell size of %s", path);
        return NULL;
    }
    if ((size_t)size > ((size_t)-1) - 1) {
        fclose(f);
        snprintf(err, err_cap, "read error: file too large: %s", path);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        snprintf(err, err_cap, "read error: cannot seek %s", path);
        return NULL;
    }

    char *buf = (char *)malloc((size_t)size + 1);
    if (!buf) {
        fclose(f);
        snprintf(err, err_cap, "out of memory");
        return NULL;
    }

    size_t got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    if (got != (size_t)size) {
        free(buf);
        snprintf(err, err_cap, "read error: short read on %s", path);
        return NULL;
    }

    buf[size] = '\0';
    return buf;
}

static char *read_stdin_all(char *err, size_t err_cap) {
    size_t cap = 1024;
    size_t len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) {
        snprintf(err, err_cap, "out of memory");
        return NULL;
    }

    while (1) {
        if (len + 512 + 1 > cap) {
            cap *= 2;
            char *next = (char *)realloc(buf, cap);
            if (!next) {
                free(buf);
                snprintf(err, err_cap, "out of memory");
                return NULL;
            }
            buf = next;
        }
        size_t got = fread(buf + len, 1, 512, stdin);
        len += got;
        if (got < 512) {
            if (ferror(stdin)) {
                free(buf);
                snprintf(err, err_cap, "failed to read stdin");
                return NULL;
            }
            break;
        }
    }
    buf[len] = '\0';
    return buf;
}

static int hash_file_sha256_hex64(const char *path, char out_hex[65], char *err, size_t err_cap) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(err, err_cap, "cannot open source %s", path);
        return 0;
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        fclose(f);
        snprintf(err, err_cap, "failed to initialize SHA-256");
        return 0;
    }
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        fclose(f);
        snprintf(err, err_cap, "failed to initialize SHA-256");
        return 0;
    }
    unsigned char buf[4096];
    while (1) {
        size_t n = fread(buf, 1, sizeof(buf), f);
        if (n > 0) {
            if (EVP_DigestUpdate(ctx, buf, n) != 1) {
                EVP_MD_CTX_free(ctx);
                fclose(f);
                snprintf(err, err_cap, "failed to update SHA-256");
                return 0;
            }
        }
        if (n < sizeof(buf)) {
            if (ferror(f)) {
                EVP_MD_CTX_free(ctx);
                fclose(f);
                snprintf(err, err_cap, "failed reading %s", path);
                return 0;
            }
            break;
        }
    }
    fclose(f);

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    if (EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1 || digest_len != 32) {
        EVP_MD_CTX_free(ctx);
        snprintf(err, err_cap, "failed to finalize SHA-256");
        return 0;
    }
    EVP_MD_CTX_free(ctx);
    static const char *hex = "0123456789abcdef";
    for (unsigned int i = 0; i < digest_len; i++) {
        out_hex[i * 2] = hex[(digest[i] >> 4) & 0xF];
        out_hex[i * 2 + 1] = hex[digest[i] & 0xF];
    }
    out_hex[64] = '\0';
    return 1;
}

static int hash_bytes_sha256_hex64(const unsigned char *buf, size_t len, char out_hex[65]) {
    if (!buf) {
        return 0;
    }
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return 0;
    }
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        return 0;
    }
    if (EVP_DigestUpdate(ctx, buf, len) != 1) {
        EVP_MD_CTX_free(ctx);
        return 0;
    }
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    if (EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1 || digest_len != 32) {
        EVP_MD_CTX_free(ctx);
        return 0;
    }
    EVP_MD_CTX_free(ctx);
    static const char *hex = "0123456789abcdef";
    for (unsigned int i = 0; i < digest_len; i++) {
        out_hex[i * 2] = hex[(digest[i] >> 4) & 0xF];
        out_hex[i * 2 + 1] = hex[digest[i] & 0xF];
    }
    out_hex[64] = '\0';
    return 1;
}

static int is_ident_start_char(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

static int is_ident_char2(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

static char *extract_exports_csv(const char *src) {
    TextBuf out;
    tb_init(&out);

    size_t i = 0;
    size_t n = strlen(src);
    while (i < n) {
        if (src[i] == '#') {
            while (i < n && src[i] != '\n') {
                i++;
            }
            continue;
        }
        if (src[i] == '"') {
            i++;
            while (i < n) {
                if (src[i] == '\\' && i + 1 < n) {
                    i += 2;
                    continue;
                }
                if (src[i] == '"') {
                    i++;
                    break;
                }
                i++;
            }
            continue;
        }
        if (!is_ident_start_char(src[i])) {
            i++;
            continue;
        }

        size_t start = i;
        i++;
        while (i < n && is_ident_char2(src[i])) {
            i++;
        }
        size_t len = i - start;
        if (!(len == 6 && strncmp(src + start, "export", 6) == 0)) {
            continue;
        }

        while (i < n && isspace((unsigned char)src[i])) {
            i++;
        }
        if (i >= n || src[i] != '(') {
            continue;
        }

        i++;
        int depth = 1;
        while (i < n && depth > 0) {
            if (src[i] == '#') {
                while (i < n && src[i] != '\n') {
                    i++;
                }
                continue;
            }
            if (src[i] == '"') {
                i++;
                while (i < n) {
                    if (src[i] == '\\' && i + 1 < n) {
                        i += 2;
                        continue;
                    }
                    if (src[i] == '"') {
                        i++;
                        break;
                    }
                    i++;
                }
                continue;
            }
            if (src[i] == '(') {
                depth++;
                i++;
                continue;
            }
            if (src[i] == ')') {
                depth--;
                i++;
                continue;
            }
            if (depth == 1 && is_ident_start_char(src[i])) {
                size_t sym_start = i;
                i++;
                while (i < n && is_ident_char2(src[i])) {
                    i++;
                }
                size_t sym_len = i - sym_start;
                if (sym_len == 0) {
                    continue;
                }
                if (out.len > 0) {
                    tb_push_char(&out, ',');
                }
                tb_push_n(&out, src + sym_start, sym_len);
                continue;
            }
            i++;
        }
    }
    return tb_take(&out);
}

static int write_dep_metadata(const char *site_root, const DepSpec *dep, const char *source_hash, const char *src_abs,
                              char *err, size_t err_cap) {
    char *src_content = read_file(src_abs, err, err_cap);
    if (!src_content) {
        return 0;
    }
    char *exports_csv = extract_exports_csv(src_content);
    free(src_content);

    size_t dep_name_len = strlen(dep->name);
    if (dep_name_len > ((size_t)-1) - 6) {
        free(exports_csv);
        snprintf(err, err_cap, "dependency name too long");
        return 0;
    }
    char *meta_rel = (char *)malloc(dep_name_len + 6);
    if (!meta_rel) {
        free(exports_csv);
        snprintf(err, err_cap, "out of memory");
        return 0;
    }
    memcpy(meta_rel, dep->name, dep_name_len);
    memcpy(meta_rel + dep_name_len, ".meta", 6);
    char *meta_path = join_path2(site_root, meta_rel);
    char *meta_dir = dirname_from_path2(meta_path);

    int ok = 0;
    if (!ensure_dir_recursive(meta_dir, err, err_cap)) {
        goto cleanup;
    }

    TextBuf meta;
    tb_init(&meta);
    tb_push_n(&meta, "name=", strlen("name="));
    tb_push_n(&meta, dep->name, strlen(dep->name));
    tb_push_char(&meta, '\n');

    tb_push_n(&meta, "version=", strlen("version="));
    tb_push_n(&meta, dep->version, strlen(dep->version));
    tb_push_char(&meta, '\n');

    tb_push_n(&meta, "source=", strlen("source="));
    tb_push_n(&meta, dep->source, strlen(dep->source));
    tb_push_char(&meta, '\n');

    tb_push_n(&meta, "hash=", strlen("hash="));
    tb_push_n(&meta, source_hash, strlen(source_hash));
    tb_push_char(&meta, '\n');

    tb_push_n(&meta, "exports=", strlen("exports="));
    tb_push_n(&meta, exports_csv, strlen(exports_csv));
    tb_push_char(&meta, '\n');

    ok = write_file(meta_path, meta.buf, err, err_cap);
    free(meta.buf);

cleanup:
    free(meta_dir);
    free(meta_path);
    free(meta_rel);
    free(exports_csv);
    return ok;
}

static int write_registry_entry(const char *root, const char *name, const char *version, const char *module_source,
                                const char *hash_hex, const char *sig_hex, const char *exports_csv,
                                const char *published_from, char *err, size_t err_cap) {
    char *registry_root = registry_root_from_project_root(root);
    char *mod_path = registry_module_path(registry_root, name, version);
    char *meta_path = registry_meta_path(registry_root, name, version);
    char *mod_dir = dirname_from_path2(mod_path);
    char *meta_dir = dirname_from_path2(meta_path);

    if (!ensure_dir_recursive(mod_dir, err, err_cap)) {
        free(registry_root);
        free(mod_path);
        free(meta_path);
        free(mod_dir);
        free(meta_dir);
        return 0;
    }
    if (!ensure_dir_recursive(meta_dir, err, err_cap)) {
        free(registry_root);
        free(mod_path);
        free(meta_path);
        free(mod_dir);
        free(meta_dir);
        return 0;
    }

    if (!write_file(mod_path, module_source, err, err_cap)) {
        free(registry_root);
        free(mod_path);
        free(meta_path);
        free(mod_dir);
        free(meta_dir);
        return 0;
    }

    TextBuf meta;
    tb_init(&meta);
    tb_push_n(&meta, "name=", strlen("name="));
    tb_push_n(&meta, name, strlen(name));
    tb_push_char(&meta, '\n');
    tb_push_n(&meta, "version=", strlen("version="));
    tb_push_n(&meta, version, strlen(version));
    tb_push_char(&meta, '\n');
    tb_push_n(&meta, "hash=", strlen("hash="));
    tb_push_n(&meta, hash_hex, strlen(hash_hex));
    tb_push_char(&meta, '\n');
    if (sig_hex && sig_hex[0] != '\0') {
        tb_push_n(&meta, "sig=", strlen("sig="));
        tb_push_n(&meta, sig_hex, strlen(sig_hex));
        tb_push_char(&meta, '\n');
    }
    tb_push_n(&meta, "exports=", strlen("exports="));
    if (exports_csv) {
        tb_push_n(&meta, exports_csv, strlen(exports_csv));
    }
    tb_push_char(&meta, '\n');
    if (published_from) {
        tb_push_n(&meta, "published_from=", strlen("published_from="));
        tb_push_n(&meta, published_from, strlen(published_from));
        tb_push_char(&meta, '\n');
    }

    int ok = write_file(meta_path, meta.buf, err, err_cap);
    free(meta.buf);
    free(registry_root);
    free(mod_path);
    free(meta_path);
    free(mod_dir);
    free(meta_dir);
    return ok;
}

static int read_registry_meta_integrity(const char *root, const char *name, const char *version, char **out_hash,
                                        char **out_sig, char *err, size_t err_cap) {
    *out_hash = NULL;
    *out_sig = NULL;

    char *registry_root = registry_root_from_project_root(root);
    char *meta_path = registry_meta_path(registry_root, name, version);
    free(registry_root);

    if (!is_regular_file_path(meta_path)) {
        snprintf(err, err_cap, "registry metadata not found for %s==%s: %s", name, version, meta_path);
        free(meta_path);
        return 0;
    }

    char *meta = read_file(meta_path, err, err_cap);
    free(meta_path);
    if (!meta) {
        return 0;
    }

    char *cur = meta;
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
        char *val = trim_ws(eq + 1);
        if (strcmp(key, "hash") == 0) {
            free(*out_hash);
            *out_hash = dup_s(val);
        } else if (strcmp(key, "sig") == 0) {
            free(*out_sig);
            *out_sig = dup_s(val);
        }
    }

    if (!*out_hash || !valid_hash_hex(*out_hash)) {
        snprintf(err, err_cap, "registry metadata has invalid hash for %s==%s", name, version);
        free(*out_hash);
        free(*out_sig);
        *out_hash = NULL;
        *out_sig = NULL;
        free(meta);
        return 0;
    }
    if (*out_sig && (*out_sig)[0] != '\0' && !valid_hash_hex64(*out_sig)) {
        snprintf(err, err_cap, "registry metadata has invalid signature for %s==%s", name, version);
        free(*out_hash);
        free(*out_sig);
        *out_hash = NULL;
        *out_sig = NULL;
        free(meta);
        return 0;
    }

    free(meta);
    return 1;
}

static int write_file_mode(const char *path, const char *content, mode_t mode, char *err, size_t err_cap) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (fd < 0) {
        snprintf(err, err_cap, "write error: cannot open %s", path);
        return 0;
    }
    size_t len = strlen(content);
    size_t off = 0;
    while (off < len) {
        ssize_t wrote = write(fd, content + off, len - off);
        if (wrote < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(fd);
            snprintf(err, err_cap, "write error: short write on %s", path);
            return 0;
        }
        off += (size_t)wrote;
    }
    if (close(fd) != 0) {
        snprintf(err, err_cap, "write error: cannot close %s", path);
        return 0;
    }
    return 1;
}

static int write_file(const char *path, const char *content, char *err, size_t err_cap) {
    return write_file_mode(path, content, 0644, err, err_cap);
}
