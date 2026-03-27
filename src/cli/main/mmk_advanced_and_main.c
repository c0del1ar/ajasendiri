static int handle_mod_pack(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr,
                "usage: ./ajasendiri mmk pack <path/to/module.aja> --version x.y.z [--name module_name] [--out file.ajapkg]\n");
        return 1;
    }
    const char *src_arg = argv[3];
    const char *name_arg = NULL;
    const char *version_arg = NULL;
    const char *out_arg = NULL;
    for (int i = 4; i < argc; i++) {
        if (strcmp(argv[i], "--name") == 0 && i + 1 < argc) {
            if (name_arg) {
                fprintf(stderr, "mmk pack: --name provided more than once\n");
                return 1;
            }
            name_arg = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--version") == 0 && i + 1 < argc) {
            if (version_arg) {
                fprintf(stderr, "mmk pack: --version provided more than once\n");
                return 1;
            }
            version_arg = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            if (out_arg) {
                fprintf(stderr, "mmk pack: --out provided more than once\n");
                return 1;
            }
            out_arg = argv[++i];
            continue;
        }
        fprintf(stderr,
                "usage: ./ajasendiri mmk pack <path/to/module.aja> --version x.y.z [--name module_name] [--out file.ajapkg]\n");
        return 1;
    }
    if (!version_arg) {
        fprintf(stderr, "mmk pack: --version is required\n");
        return 1;
    }
    if (!valid_version(version_arg)) {
        fprintf(stderr, "mmk pack: invalid version '%s' (expected x.y.z)\n", version_arg);
        return 1;
    }

    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd))) {
        fprintf(stderr, "mmk pack: cannot get current directory\n");
        return 1;
    }
    char *src_abs = NULL;
    const char *src_use = src_arg;
    if (src_arg[0] != '/') {
        src_abs = join_path2(cwd, src_arg);
        src_use = src_abs;
    }
    if (!is_regular_file_path(src_use)) {
        fprintf(stderr, "mmk pack: source not found: %s\n", src_use);
        free(src_abs);
        return 1;
    }
    if (!has_aja_extension(src_use)) {
        fprintf(stderr, "mmk pack: source must be .aja file: %s\n", src_use);
        free(src_abs);
        return 1;
    }

    char *name = name_arg ? dup_s(name_arg) : module_name_from_path(src_use);
    if (!valid_module_name(name)) {
        fprintf(stderr, "mmk pack: invalid module name '%s'\n", name);
        free(name);
        free(src_abs);
        return 1;
    }

    char err[512];
    char hash_hex[17];
    if (!hash_file_fnv1a_hex16(src_use, hash_hex, err, sizeof(err))) {
        fprintf(stderr, "mmk pack: %s\n", err);
        free(name);
        free(src_abs);
        return 1;
    }

    char *src_content = read_file(src_use, err, sizeof(err));
    if (!src_content) {
        fprintf(stderr, "mmk pack: %s\n", err);
        free(name);
        free(src_abs);
        return 1;
    }
    char *exports_csv = extract_exports_csv(src_content);
    const char *sign_key = getenv("AJA_SIGN_KEY");
    char sig_hex[65];
    int has_sig = 0;
    if (sign_key && sign_key[0] != '\0') {
        if (!hmac_sha256_hex((const unsigned char *)src_content, strlen(src_content), sign_key, sig_hex, err,
                             sizeof(err))) {
            fprintf(stderr, "mmk pack: %s\n", err);
            free(exports_csv);
            free(src_content);
            free(name);
            free(src_abs);
            return 1;
        }
        has_sig = 1;
    }

    char *root = find_project_root(cwd);
    char *out_path = NULL;
    if (out_arg) {
        if (out_arg[0] == '/') {
            out_path = dup_s(out_arg);
        } else {
            out_path = join_path2(cwd, out_arg);
        }
    } else {
        char *pkg_root = join_path2(root, ".aja/pkgs");
        char *safe_name = sanitize_name_for_filename(name);
        TextBuf fname;
        tb_init(&fname);
        tb_push_n(&fname, safe_name, strlen(safe_name));
        tb_push_char(&fname, '-');
        tb_push_n(&fname, version_arg, strlen(version_arg));
        tb_push_n(&fname, ".ajapkg", strlen(".ajapkg"));
        out_path = join_path2(pkg_root, fname.buf);
        free(fname.buf);
        free(safe_name);
        free(pkg_root);
    }

    char *out_dir = dirname_from_path2(out_path);
    if (!ensure_dir_recursive(out_dir, err, sizeof(err))) {
        fprintf(stderr, "mmk pack: %s\n", err);
        free(out_dir);
        free(out_path);
        free(root);
        free(exports_csv);
        free(src_content);
        free(name);
        free(src_abs);
        return 1;
    }

    TextBuf pkg;
    tb_init(&pkg);
    tb_push_n(&pkg, "AJA_PKG_V1\n", strlen("AJA_PKG_V1\n"));
    tb_push_n(&pkg, "name=", strlen("name="));
    tb_push_n(&pkg, name, strlen(name));
    tb_push_char(&pkg, '\n');
    tb_push_n(&pkg, "version=", strlen("version="));
    tb_push_n(&pkg, version_arg, strlen(version_arg));
    tb_push_char(&pkg, '\n');
    tb_push_n(&pkg, "hash=", strlen("hash="));
    tb_push_n(&pkg, hash_hex, strlen(hash_hex));
    tb_push_char(&pkg, '\n');
    if (has_sig) {
        tb_push_n(&pkg, "sig=", strlen("sig="));
        tb_push_n(&pkg, sig_hex, strlen(sig_hex));
        tb_push_char(&pkg, '\n');
    }
    tb_push_n(&pkg, "exports=", strlen("exports="));
    tb_push_n(&pkg, exports_csv, strlen(exports_csv));
    tb_push_n(&pkg, "\n---\n", strlen("\n---\n"));
    tb_push_n(&pkg, src_content, strlen(src_content));
    if (pkg.len == 0 || pkg.buf[pkg.len - 1] != '\n') {
        tb_push_char(&pkg, '\n');
    }

    if (!write_file(out_path, pkg.buf, err, sizeof(err))) {
        fprintf(stderr, "mmk pack: %s\n", err);
        free(pkg.buf);
        free(out_dir);
        free(out_path);
        free(root);
        free(exports_csv);
        free(src_content);
        free(name);
        free(src_abs);
        return 1;
    }

    printf("packed %s==%s -> %s\n", name, version_arg, out_path);
    free(pkg.buf);
    free(out_dir);
    free(out_path);
    free(root);
    free(exports_csv);
    free(src_content);
    free(name);
    free(src_abs);
    return 0;
}

static int handle_mod_publish(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr,
                "usage: ./ajasendiri mmk publish <path/to/module.aja|path/to/file.ajapkg> [--version x.y.z] [--name module_name]\n");
        return 1;
    }
    const char *input_arg = argv[3];
    const char *name_arg = NULL;
    const char *version_arg = NULL;
    for (int i = 4; i < argc; i++) {
        if (strcmp(argv[i], "--name") == 0 && i + 1 < argc) {
            if (name_arg) {
                fprintf(stderr, "mmk publish: --name provided more than once\n");
                return 1;
            }
            name_arg = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--version") == 0 && i + 1 < argc) {
            if (version_arg) {
                fprintf(stderr, "mmk publish: --version provided more than once\n");
                return 1;
            }
            version_arg = argv[++i];
            continue;
        }
        fprintf(stderr,
                "usage: ./ajasendiri mmk publish <path/to/module.aja|path/to/file.ajapkg> [--version x.y.z] [--name module_name]\n");
        return 1;
    }

    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd))) {
        fprintf(stderr, "mmk publish: cannot get current directory\n");
        return 1;
    }
    char *root = find_project_root(cwd);
    char err[512];

    char *input_abs = NULL;
    const char *input_use = input_arg;
    if (input_arg[0] != '/') {
        input_abs = join_path2(cwd, input_arg);
        input_use = input_abs;
    }
    if (!is_regular_file_path(input_use)) {
        fprintf(stderr, "mmk publish: input not found: %s\n", input_use);
        free(input_abs);
        free(root);
        return 1;
    }

    if (ends_with(input_use, ".ajapkg")) {
        if (name_arg || version_arg) {
            fprintf(stderr, "mmk publish: --name/--version are not used when publishing .ajapkg\n");
            free(input_abs);
            free(root);
            return 1;
        }
        char *pkg_text = read_file(input_use, err, sizeof(err));
        if (!pkg_text) {
            fprintf(stderr, "mmk publish: %s\n", err);
            free(input_abs);
            free(root);
            return 1;
        }
        DepSpec spec;
        char *exports_csv = NULL;
        char *module_source = NULL;
        if (!parse_package_text(pkg_text, &spec, &exports_csv, &module_source, err, sizeof(err))) {
            fprintf(stderr, "mmk publish: %s\n", err);
            free(pkg_text);
            free(input_abs);
            free(root);
            return 1;
        }
        char computed_hash[17];
        hash_bytes_fnv1a_hex16((const unsigned char *)module_source, strlen(module_source), computed_hash);
        if (strcmp(computed_hash, spec.hash) != 0) {
            fprintf(stderr, "mmk publish: package hash mismatch (declared=%s computed=%s)\n", spec.hash, computed_hash);
            free(spec.name);
            free(spec.version);
            free(spec.source);
            free(spec.hash);
            free(exports_csv);
            free(module_source);
            free(pkg_text);
            free(input_abs);
            free(root);
            return 1;
        }
        const char *require_sig = getenv("AJA_REQUIRE_SIGNATURE");
        int must_have_sig = require_sig && strcmp(require_sig, "1") == 0;
        const char *sign_key = getenv("AJA_SIGN_KEY");
        if (spec.sig && spec.sig[0] != '\0') {
            if (!sign_key || sign_key[0] == '\0') {
                fprintf(stderr,
                        "mmk publish: package is signed but AJA_SIGN_KEY is not set (required to verify signature)\n");
                free(spec.name);
                free(spec.version);
                free(spec.source);
                free(spec.hash);
                free(spec.sig);
                free(exports_csv);
                free(module_source);
                free(pkg_text);
                free(input_abs);
                free(root);
                return 1;
            }
            char sig_hex[65];
            if (!hmac_sha256_hex((const unsigned char *)module_source, strlen(module_source), sign_key, sig_hex, err,
                                 sizeof(err))) {
                fprintf(stderr, "mmk publish: %s\n", err);
                free(spec.name);
                free(spec.version);
                free(spec.source);
                free(spec.hash);
                free(spec.sig);
                free(exports_csv);
                free(module_source);
                free(pkg_text);
                free(input_abs);
                free(root);
                return 1;
            }
            if (strcmp(sig_hex, spec.sig) != 0) {
                fprintf(stderr, "mmk publish: package signature mismatch (declared=%s computed=%s)\n", spec.sig, sig_hex);
                free(spec.name);
                free(spec.version);
                free(spec.source);
                free(spec.hash);
                free(spec.sig);
                free(exports_csv);
                free(module_source);
                free(pkg_text);
                free(input_abs);
                free(root);
                return 1;
            }
        } else if (must_have_sig) {
            fprintf(stderr, "mmk publish: signature required (set AJA_SIGN_KEY when packing/signing)\n");
            free(spec.name);
            free(spec.version);
            free(spec.source);
            free(spec.hash);
            free(spec.sig);
            free(exports_csv);
            free(module_source);
            free(pkg_text);
            free(input_abs);
            free(root);
            return 1;
        }

        if (!write_registry_entry(root, spec.name, spec.version, module_source, spec.hash, spec.sig, exports_csv,
                                  input_arg, err, sizeof(err))) {
            fprintf(stderr, "mmk publish: %s\n", err);
            free(spec.name);
            free(spec.version);
            free(spec.source);
            free(spec.hash);
            free(spec.sig);
            free(exports_csv);
            free(module_source);
            free(pkg_text);
            free(input_abs);
            free(root);
            return 1;
        }
        printf("published %s==%s to local registry\n", spec.name, spec.version);
        free(spec.name);
        free(spec.version);
        free(spec.source);
        free(spec.hash);
        free(spec.sig);
        free(exports_csv);
        free(module_source);
        free(pkg_text);
        free(input_abs);
        free(root);
        return 0;
    }

    if (!has_aja_extension(input_use)) {
        fprintf(stderr, "mmk publish: input must be .aja or .ajapkg: %s\n", input_use);
        free(input_abs);
        free(root);
        return 1;
    }
    if (!version_arg) {
        fprintf(stderr, "mmk publish: --version is required for .aja input\n");
        free(input_abs);
        free(root);
        return 1;
    }
    if (!valid_version(version_arg)) {
        fprintf(stderr, "mmk publish: invalid version '%s' (expected x.y.z)\n", version_arg);
        free(input_abs);
        free(root);
        return 1;
    }
    char *name = name_arg ? dup_s(name_arg) : module_name_from_path(input_use);
    if (!valid_module_name(name)) {
        fprintf(stderr, "mmk publish: invalid module name '%s'\n", name);
        free(name);
        free(input_abs);
        free(root);
        return 1;
    }

    char hash_hex[17];
    if (!hash_file_fnv1a_hex16(input_use, hash_hex, err, sizeof(err))) {
        fprintf(stderr, "mmk publish: %s\n", err);
        free(name);
        free(input_abs);
        free(root);
        return 1;
    }
    char *module_source = read_file(input_use, err, sizeof(err));
    if (!module_source) {
        fprintf(stderr, "mmk publish: %s\n", err);
        free(name);
        free(input_abs);
        free(root);
        return 1;
    }
    char *exports_csv = extract_exports_csv(module_source);
    const char *sign_key = getenv("AJA_SIGN_KEY");
    const char *require_sig = getenv("AJA_REQUIRE_SIGNATURE");
    int must_have_sig = require_sig && strcmp(require_sig, "1") == 0;
    char sig_hex[65];
    const char *sig_ptr = NULL;
    if (sign_key && sign_key[0] != '\0') {
        if (!hmac_sha256_hex((const unsigned char *)module_source, strlen(module_source), sign_key, sig_hex, err,
                             sizeof(err))) {
            fprintf(stderr, "mmk publish: %s\n", err);
            free(exports_csv);
            free(module_source);
            free(name);
            free(input_abs);
            free(root);
            return 1;
        }
        sig_ptr = sig_hex;
    } else if (must_have_sig) {
        fprintf(stderr, "mmk publish: signature required but AJA_SIGN_KEY is not set\n");
        free(exports_csv);
        free(module_source);
        free(name);
        free(input_abs);
        free(root);
        return 1;
    }

    if (!write_registry_entry(root, name, version_arg, module_source, hash_hex, sig_ptr, exports_csv, input_arg, err,
                              sizeof(err))) {
        fprintf(stderr, "mmk publish: %s\n", err);
        free(exports_csv);
        free(module_source);
        free(name);
        free(input_abs);
        free(root);
        return 1;
    }

    printf("published %s==%s to local registry\n", name, version_arg);
    free(exports_csv);
    free(module_source);
    free(name);
    free(input_abs);
    free(root);
    return 0;
}

static int handle_mod_verify(int argc, char **argv) {
    (void)argv;
    if (argc != 3) {
        fprintf(stderr, "usage: ./ajasendiri mmk verify\n");
        return 1;
    }
    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd))) {
        fprintf(stderr, "mmk verify: cannot get current directory\n");
        return 1;
    }
    char *root = find_project_root(cwd);
    char *req_path = join_path2(root, "requirements.txt");
    char *lock_path = join_path2(root, "requirements.lock");
    int using_venv = 0;
    char *site_root = resolve_site_packages_root(root, &using_venv);
    char err[512];

    if (!path_exists(req_path)) {
        fprintf(stderr, "mmk verify: requirements.txt not found\n");
        free(root);
        free(req_path);
        free(lock_path);
        free(site_root);
        return 1;
    }
    if (!path_exists(lock_path)) {
        fprintf(stderr, "mmk verify: requirements.lock not found\n");
        free(root);
        free(req_path);
        free(lock_path);
        free(site_root);
        return 1;
    }

    char *req_content = read_file(req_path, err, sizeof(err));
    char *lock_content = read_file(lock_path, err, sizeof(err));
    if (!req_content || !lock_content) {
        fprintf(stderr, "mmk verify: %s\n", err);
        free(req_content);
        free(lock_content);
        free(root);
        free(req_path);
        free(lock_path);
        free(site_root);
        return 1;
    }

    DepSpecList deps;
    DepSpecList locked;
    memset(&deps, 0, sizeof(deps));
    memset(&locked, 0, sizeof(locked));
    if (!parse_requirements_text(req_content, &deps, err, sizeof(err))) {
        fprintf(stderr, "mmk verify: %s\n", err);
        free(req_content);
        free(lock_content);
        dep_list_free(&deps);
        dep_list_free(&locked);
        free(root);
        free(req_path);
        free(lock_path);
        free(site_root);
        return 1;
    }
    if (!parse_lock_text(lock_content, &locked, err, sizeof(err))) {
        fprintf(stderr, "mmk verify: %s\n", err);
        free(req_content);
        free(lock_content);
        dep_list_free(&deps);
        dep_list_free(&locked);
        free(root);
        free(req_path);
        free(lock_path);
        free(site_root);
        return 1;
    }

    if (!validate_locked_requirements(&deps, &locked, err, sizeof(err))) {
        fprintf(stderr, "mmk verify: %s\n", err);
        free(req_content);
        free(lock_content);
        dep_list_free(&deps);
        dep_list_free(&locked);
        free(root);
        free(req_path);
        free(lock_path);
        free(site_root);
        return 1;
    }

    for (int i = 0; i < locked.count; i++) {
        DepSpec *d = &locked.items[i];
        char *src = NULL;
        if (!resolve_dep_source(root, d, &src, err, sizeof(err))) {
            fprintf(stderr, "mmk verify: %s\n", err);
            free(req_content);
            free(lock_content);
            dep_list_free(&deps);
            dep_list_free(&locked);
            free(root);
            free(req_path);
            free(lock_path);
            free(site_root);
            return 1;
        }
        if (!is_regular_file_path(src)) {
            fprintf(stderr, "mmk verify: source not found for %s: %s\n", d->name, src);
            free(src);
            free(req_content);
            free(lock_content);
            dep_list_free(&deps);
            dep_list_free(&locked);
            free(root);
            free(req_path);
            free(lock_path);
            free(site_root);
            return 1;
        }
        char src_hash[17];
        if (!hash_file_fnv1a_hex16(src, src_hash, err, sizeof(err))) {
            fprintf(stderr, "mmk verify: %s\n", err);
            free(src);
            free(req_content);
            free(lock_content);
            dep_list_free(&deps);
            dep_list_free(&locked);
            free(root);
            free(req_path);
            free(lock_path);
            free(site_root);
            return 1;
        }
        if (strcmp(src_hash, d->hash) != 0) {
            fprintf(stderr, "mmk verify: source hash mismatch for %s (lock=%s current=%s)\n", d->name, d->hash,
                    src_hash);
            free(src);
            free(req_content);
            free(lock_content);
            dep_list_free(&deps);
            dep_list_free(&locked);
            free(root);
            free(req_path);
            free(lock_path);
            free(site_root);
            return 1;
        }
        if (!verify_registry_dependency_signature(root, d, src, src_hash, err, sizeof(err))) {
            fprintf(stderr, "mmk verify: %s\n", err);
            free(src);
            free(req_content);
            free(lock_content);
            dep_list_free(&deps);
            dep_list_free(&locked);
            free(root);
            free(req_path);
            free(lock_path);
            free(site_root);
            return 1;
        }
        free(src);

        char *mod_file_rel = (char *)malloc(strlen(d->name) + 5);
        if (!mod_file_rel) {
            fprintf(stderr, "mmk verify: out of memory\n");
            free(req_content);
            free(lock_content);
            dep_list_free(&deps);
            dep_list_free(&locked);
            free(root);
            free(req_path);
            free(lock_path);
            free(site_root);
            return 1;
        }
        strcpy(mod_file_rel, d->name);
        strcat(mod_file_rel, ".aja");
        char *installed = join_path2(site_root, mod_file_rel);
        if (!is_regular_file_path(installed)) {
            fprintf(stderr, "mmk verify: installed module missing for %s: %s\n", d->name, installed);
            free(installed);
            free(mod_file_rel);
            free(req_content);
            free(lock_content);
            dep_list_free(&deps);
            dep_list_free(&locked);
            free(root);
            free(req_path);
            free(lock_path);
            free(site_root);
            return 1;
        }
        char installed_hash[17];
        if (!hash_file_fnv1a_hex16(installed, installed_hash, err, sizeof(err))) {
            fprintf(stderr, "mmk verify: %s\n", err);
            free(installed);
            free(mod_file_rel);
            free(req_content);
            free(lock_content);
            dep_list_free(&deps);
            dep_list_free(&locked);
            free(root);
            free(req_path);
            free(lock_path);
            free(site_root);
            return 1;
        }
        if (strcmp(installed_hash, d->hash) != 0) {
            fprintf(stderr, "mmk verify: installed file hash mismatch for %s (lock=%s current=%s)\n", d->name, d->hash,
                    installed_hash);
            free(installed);
            free(mod_file_rel);
            free(req_content);
            free(lock_content);
            dep_list_free(&deps);
            dep_list_free(&locked);
            free(root);
            free(req_path);
            free(lock_path);
            free(site_root);
            return 1;
        }
        free(installed);
        free(mod_file_rel);
    }

    if (using_venv) {
        printf("verified %d dependency(s) (AJA_VENV/site-packages)\n", locked.count);
    } else {
        printf("verified %d dependency(s)\n", locked.count);
    }
    free(req_content);
    free(lock_content);
    dep_list_free(&deps);
    dep_list_free(&locked);
    free(root);
    free(req_path);
    free(lock_path);
    free(site_root);
    return 0;
}

static int handle_mod_search(int argc, char **argv) {
    if (argc > 4) {
        fprintf(stderr, "usage: ./ajasendiri mmk search [query]\n");
        return 1;
    }
    const char *query = argc == 4 ? argv[3] : NULL;

    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd))) {
        fprintf(stderr, "mmk search: cannot get current directory\n");
        return 1;
    }
    char *root = find_project_root(cwd);
    char *registry_root = registry_root_from_project_root(root);

    if (!is_directory_path(registry_root)) {
        printf("no packages found in local registry (%s)\n", registry_root);
        free(registry_root);
        free(root);
        return 0;
    }

    DIR *dir = opendir(registry_root);
    if (!dir) {
        fprintf(stderr, "mmk search: cannot read registry directory: %s\n", registry_root);
        free(registry_root);
        free(root);
        return 1;
    }

    int found = 0;
    struct dirent *ent = NULL;
    char err[512];
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }
        if (query && query[0] != '\0' && strstr(ent->d_name, query) == NULL) {
            continue;
        }
        char *module_dir = join_path2(registry_root, ent->d_name);
        if (!is_directory_path(module_dir)) {
            free(module_dir);
            continue;
        }
        free(module_dir);
        if (!valid_module_name(ent->d_name)) {
            continue;
        }

        char *latest = NULL;
        if (!resolve_registry_version_selector(registry_root, ent->d_name, "latest", &latest, err, sizeof(err))) {
            continue;
        }
        printf("%s (latest %s)\n", ent->d_name, latest);
        free(latest);
        found++;
    }
    closedir(dir);

    if (found == 0) {
        if (query && query[0] != '\0') {
            printf("no packages found for query '%s'\n", query);
        } else {
            printf("no packages found in local registry\n");
        }
    }

    free(registry_root);
    free(root);
    return 0;
}

static int handle_mod_info(int argc, char **argv) {
    if (argc < 4 || argc > 6) {
        fprintf(stderr, "usage: ./ajasendiri mmk info <module_name> [--version <selector>]\n");
        return 1;
    }

    const char *name = argv[3];
    const char *selector = "latest";
    if (!valid_module_name(name)) {
        fprintf(stderr, "mmk info: invalid module name '%s'\n", name);
        return 1;
    }

    if (argc > 4) {
        if (argc != 6 || strcmp(argv[4], "--version") != 0) {
            fprintf(stderr, "usage: ./ajasendiri mmk info <module_name> [--version <selector>]\n");
            return 1;
        }
        selector = argv[5];
    }
    if (!valid_version_selector(selector)) {
        fprintf(stderr, "mmk info: invalid version selector '%s' (use x.y.z, ^x.y(.z), ~x.y(.z), latest, or *)\n",
                selector);
        return 1;
    }

    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd))) {
        fprintf(stderr, "mmk info: cannot get current directory\n");
        return 1;
    }
    char *root = find_project_root(cwd);
    char *registry_root = registry_root_from_project_root(root);
    char err[512];

    char *selected_version = NULL;
    if (!resolve_registry_version_selector(registry_root, name, selector, &selected_version, err, sizeof(err))) {
        fprintf(stderr, "mmk info: %s\n", err);
        free(registry_root);
        free(root);
        return 1;
    }
    char *latest_version = NULL;
    if (!resolve_registry_version_selector(registry_root, name, "latest", &latest_version, err, sizeof(err))) {
        fprintf(stderr, "mmk info: %s\n", err);
        free(selected_version);
        free(registry_root);
        free(root);
        return 1;
    }

    char *meta_hash = NULL;
    char *meta_sig = NULL;
    if (!read_registry_meta_integrity(root, name, selected_version, &meta_hash, &meta_sig, err, sizeof(err))) {
        fprintf(stderr, "mmk info: %s\n", err);
        free(latest_version);
        free(selected_version);
        free(registry_root);
        free(root);
        return 1;
    }

    char *module_path = registry_module_path(registry_root, name, selected_version);
    printf("name: %s\n", name);
    printf("selected: %s (selector: %s)\n", selected_version, selector);
    printf("latest: %s\n", latest_version);
    printf("source: %s\n", module_path);
    printf("hash: %s\n", meta_hash);
    if (meta_sig && meta_sig[0] != '\0') {
        printf("signature: %s\n", meta_sig);
    } else {
        printf("signature: <none>\n");
    }

    free(module_path);
    free(meta_hash);
    free(meta_sig);
    free(latest_version);
    free(selected_version);
    free(registry_root);
    free(root);
    return 0;
}

static int handle_mod_freeze(int argc, char **argv) {
    (void)argv;
    if (argc != 3) {
        fprintf(stderr, "usage: ./ajasendiri mmk freeze\n");
        return 1;
    }
    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd))) {
        fprintf(stderr, "mmk freeze: cannot get current directory\n");
        return 1;
    }
    char *root = find_project_root(cwd);
    char *lock_path = join_path2(root, "requirements.lock");
    char err[512];
    if (!path_exists(lock_path)) {
        fprintf(stderr, "mmk freeze: requirements.lock not found (run 'ajasendiri mmk install')\n");
        free(root);
        free(lock_path);
        return 1;
    }
    char *content = read_file(lock_path, err, sizeof(err));
    if (!content) {
        fprintf(stderr, "mmk freeze: %s\n", err);
        free(root);
        free(lock_path);
        return 1;
    }
    printf("%s", content);
    free(content);
    free(root);
    free(lock_path);
    return 0;
}

static char *resolve_executable_path(const char *argv0) {
    if (!argv0 || argv0[0] == '\0') {
        return NULL;
    }
    if (argv0[0] == '/') {
        return dup_s(argv0);
    }
    if (strchr(argv0, '/')) {
        char cwd[4096];
        if (!getcwd(cwd, sizeof(cwd))) {
            return NULL;
        }
        return join_path2(cwd, argv0);
    }

    const char *path = getenv("PATH");
    if (!path || path[0] == '\0') {
        return NULL;
    }

    const char *seg = path;
    while (*seg != '\0') {
        const char *end = seg;
        while (*end != '\0' && *end != ':') {
            end++;
        }
        if (end > seg) {
            char *dir = dup_n(seg, (size_t)(end - seg));
            char *candidate = join_path2(dir, argv0);
            free(dir);
            if (access(candidate, X_OK) == 0) {
                return candidate;
            }
            free(candidate);
        }
        seg = *end == ':' ? end + 1 : end;
    }
    return NULL;
}

static char *escape_shell_dquote(const char *s) {
    TextBuf out;
    tb_init(&out);
    for (size_t i = 0; s[i] != '\0'; i++) {
        char c = s[i];
        if (c == '\\' || c == '"' || c == '$' || c == '`') {
            tb_push_char(&out, '\\');
        }
        tb_push_char(&out, c);
    }
    return tb_take(&out);
}

static int write_executable_script(const char *path, const char *content, char *err, size_t err_cap) {
    if (!write_file(path, content, err, err_cap)) {
        return 0;
    }
    if (chmod(path, 0755) != 0) {
        snprintf(err, err_cap, "cannot chmod executable %s: %s", path, strerror(errno));
        return 0;
    }
    return 1;
}

static int handle_venv_command(int argc, char **argv) {
    if (argc > 3) {
        fprintf(stderr, "usage: ./ajasendiri venv [path]\n");
        return 1;
    }

    const char *path_arg = argc == 3 ? argv[2] : ".ajaenv";
    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd))) {
        fprintf(stderr, "venv: cannot get current directory\n");
        return 1;
    }

    char *venv_root = NULL;
    if (path_arg[0] == '/') {
        venv_root = dup_s(path_arg);
    } else {
        venv_root = join_path2(cwd, path_arg);
    }
    char *bin_dir = join_path2(venv_root, "bin");
    char *site_dir = join_path2(venv_root, "site-packages");
    char *cfg_path = join_path2(venv_root, "aja-venv.cfg");
    char *activate_path = join_path2(bin_dir, "activate");
    char *runner_path = join_path2(bin_dir, "ajasendiri");

    char err[512];
    if (!ensure_dir_recursive(bin_dir, err, sizeof(err)) || !ensure_dir_recursive(site_dir, err, sizeof(err))) {
        fprintf(stderr, "venv: %s\n", err);
        free(venv_root);
        free(bin_dir);
        free(site_dir);
        free(cfg_path);
        free(activate_path);
        free(runner_path);
        return 1;
    }

    char *exe_abs = resolve_executable_path(argv[0]);
    if (!exe_abs) {
        fprintf(stderr, "venv: cannot resolve ajasendiri executable path from argv[0]=%s\n", argv[0]);
        free(venv_root);
        free(bin_dir);
        free(site_dir);
        free(cfg_path);
        free(activate_path);
        free(runner_path);
        return 1;
    }

    char *venv_esc = escape_shell_dquote(venv_root);
    char *exe_esc = escape_shell_dquote(exe_abs);

    TextBuf activate;
    tb_init(&activate);
    tb_push_n(&activate, "#!/usr/bin/env sh\n", strlen("#!/usr/bin/env sh\n"));
    tb_push_n(&activate, "_AJA_OLD_PATH=\"$PATH\"\n", strlen("_AJA_OLD_PATH=\"$PATH\"\n"));
    tb_push_n(&activate, "_AJA_OLD_AJA_VENV=\"${AJA_VENV-}\"\n", strlen("_AJA_OLD_AJA_VENV=\"${AJA_VENV-}\"\n"));
    tb_push_n(&activate, "export AJA_VENV=\"", strlen("export AJA_VENV=\""));
    tb_push_n(&activate, venv_esc, strlen(venv_esc));
    tb_push_n(&activate, "\"\n", strlen("\"\n"));
    tb_push_n(&activate, "export PATH=\"$AJA_VENV/bin:$PATH\"\n", strlen("export PATH=\"$AJA_VENV/bin:$PATH\"\n"));
    tb_push_n(&activate, "\n", 1);
    tb_push_n(&activate, "deactivate() {\n", strlen("deactivate() {\n"));
    tb_push_n(&activate, "    PATH=\"$_AJA_OLD_PATH\"\n", strlen("    PATH=\"$_AJA_OLD_PATH\"\n"));
    tb_push_n(&activate, "    export PATH\n", strlen("    export PATH\n"));
    tb_push_n(&activate, "    if [ -n \"${_AJA_OLD_AJA_VENV-}\" ]; then\n",
              strlen("    if [ -n \"${_AJA_OLD_AJA_VENV-}\" ]; then\n"));
    tb_push_n(&activate, "        export AJA_VENV=\"$_AJA_OLD_AJA_VENV\"\n",
              strlen("        export AJA_VENV=\"$_AJA_OLD_AJA_VENV\"\n"));
    tb_push_n(&activate, "    else\n", strlen("    else\n"));
    tb_push_n(&activate, "        unset AJA_VENV\n", strlen("        unset AJA_VENV\n"));
    tb_push_n(&activate, "    fi\n", strlen("    fi\n"));
    tb_push_n(&activate, "    unset _AJA_OLD_PATH _AJA_OLD_AJA_VENV\n",
              strlen("    unset _AJA_OLD_PATH _AJA_OLD_AJA_VENV\n"));
    tb_push_n(&activate, "    unset -f deactivate 2>/dev/null || true\n",
              strlen("    unset -f deactivate 2>/dev/null || true\n"));
    tb_push_n(&activate, "}\n", strlen("}\n"));

    if (!write_executable_script(activate_path, activate.buf, err, sizeof(err))) {
        fprintf(stderr, "venv: %s\n", err);
        free(activate.buf);
        free(venv_esc);
        free(exe_esc);
        free(exe_abs);
        free(venv_root);
        free(bin_dir);
        free(site_dir);
        free(cfg_path);
        free(activate_path);
        free(runner_path);
        return 1;
    }
    free(activate.buf);

    TextBuf runner;
    tb_init(&runner);
    tb_push_n(&runner, "#!/usr/bin/env sh\n", strlen("#!/usr/bin/env sh\n"));
    tb_push_n(&runner, "export AJA_VENV=\"", strlen("export AJA_VENV=\""));
    tb_push_n(&runner, venv_esc, strlen(venv_esc));
    tb_push_n(&runner, "\"\n", strlen("\"\n"));
    tb_push_n(&runner, "exec \"", strlen("exec \""));
    tb_push_n(&runner, exe_esc, strlen(exe_esc));
    tb_push_n(&runner, "\" \"$@\"\n", strlen("\" \"$@\"\n"));
    if (!write_executable_script(runner_path, runner.buf, err, sizeof(err))) {
        fprintf(stderr, "venv: %s\n", err);
        free(runner.buf);
        free(venv_esc);
        free(exe_esc);
        free(exe_abs);
        free(venv_root);
        free(bin_dir);
        free(site_dir);
        free(cfg_path);
        free(activate_path);
        free(runner_path);
        return 1;
    }
    free(runner.buf);

    TextBuf cfg;
    tb_init(&cfg);
    tb_push_n(&cfg, "venv=", strlen("venv="));
    tb_push_n(&cfg, venv_root, strlen(venv_root));
    tb_push_n(&cfg, "\n", 1);
    tb_push_n(&cfg, "binary=", strlen("binary="));
    tb_push_n(&cfg, exe_abs, strlen(exe_abs));
    tb_push_n(&cfg, "\n", 1);
    if (!write_file(cfg_path, cfg.buf, err, sizeof(err))) {
        fprintf(stderr, "venv: %s\n", err);
        free(cfg.buf);
        free(venv_esc);
        free(exe_esc);
        free(exe_abs);
        free(venv_root);
        free(bin_dir);
        free(site_dir);
        free(cfg_path);
        free(activate_path);
        free(runner_path);
        return 1;
    }
    free(cfg.buf);

    printf("created venv at %s\n", venv_root);
    printf("activate with: . %s\n", activate_path);

    free(venv_esc);
    free(exe_esc);
    free(exe_abs);
    free(venv_root);
    free(bin_dir);
    free(site_dir);
    free(cfg_path);
    free(activate_path);
    free(runner_path);
    return 0;
}

static int handle_mod_command(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr,
                "usage: ./ajasendiri mmk <init|add|install|install-stdlib|freeze|pack|publish|verify|search|info> ...\n");
        fprintf(stderr, "  install deps:    ./ajasendiri mmk install [--locked]\n");
        fprintf(stderr, "  install module:  ./ajasendiri mmk install <module...> [--global|--project] [--from <dir>]\n");
        fprintf(stderr, "  install stdlib:  ./ajasendiri mmk install-stdlib [--global|--project] [--from <dir>] [--core|--optional|--all]\n");
        return 1;
    }
    if (strcmp(argv[2], "init") == 0) {
        return handle_mod_init(argc, argv);
    }
    if (strcmp(argv[2], "add") == 0) {
        return handle_mod_add(argc, argv);
    }
    if (strcmp(argv[2], "install") == 0) {
        return handle_mod_install(argc, argv);
    }
    if (strcmp(argv[2], "install-stdlib") == 0) {
        return handle_mod_install_stdlib(argc, argv);
    }
    if (strcmp(argv[2], "pack") == 0) {
        return handle_mod_pack(argc, argv);
    }
    if (strcmp(argv[2], "publish") == 0) {
        return handle_mod_publish(argc, argv);
    }
    if (strcmp(argv[2], "verify") == 0) {
        return handle_mod_verify(argc, argv);
    }
    if (strcmp(argv[2], "freeze") == 0) {
        return handle_mod_freeze(argc, argv);
    }
    if (strcmp(argv[2], "search") == 0) {
        return handle_mod_search(argc, argv);
    }
    if (strcmp(argv[2], "info") == 0) {
        return handle_mod_info(argc, argv);
    }
    fprintf(stderr, "unknown mmk command: %s\n", argv[2]);
    return 1;
}

static void print_usage(void) {
    fprintf(stderr, "usage: ./ajasendiri <file.aja>\n");
    fprintf(stderr, "   or: ./ajasendiri venv [path]\n");
    fprintf(stderr, "   or: ./ajasendiri check <file.aja>\n");
    fprintf(stderr, "   or: ./ajasendiri debug <file.aja> [--break line[,line...]] [--step]\n");
    fprintf(stderr, "   or: ./ajasendiri repl\n");
    fprintf(stderr, "   or: ./ajasendiri fmt [--check] [--write] [--stdin] <file_or_dir...>\n");
    fprintf(stderr, "   or: ./ajasendiri test [test_file_or_dir...]\n");
    fprintf(stderr,
            "   or: ./ajasendiri mmk <init|add|install|install-stdlib|freeze|pack|publish|verify|search|info> ...\n");
    fprintf(stderr, "      deps:    ./ajasendiri mmk install [--locked]\n");
    fprintf(stderr, "      module:  ./ajasendiri mmk install <module...> [--global|--project] [--from <dir>]\n");
    fprintf(stderr, "      stdlib:  ./ajasendiri mmk install-stdlib [--global|--project] [--from <dir>] [--core|--optional|--all]\n");
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return handle_repl_command(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "mmk") == 0) {
        return handle_mod_command(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "venv") == 0) {
        return handle_venv_command(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "test") == 0) {
        return handle_test_command(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "fmt") == 0) {
        return handle_fmt_command(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "debug") == 0) {
        return handle_debug_command(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "repl") == 0) {
        return handle_repl_command(argc, argv);
    }
    if (argc == 3 && strcmp(argv[1], "check") == 0) {
        return run_one_file(argv[2], 1);
    }
    if (argc == 2) {
        return run_one_file(argv[1], 0);
    }
    print_usage();
    return 1;
}
