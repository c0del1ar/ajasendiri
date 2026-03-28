static int handle_test_command(int argc, char **argv) {
    int cmd_argc = argc > 2 ? (argc - 1) : 1;
    char **cmd_argv = (char **)calloc((size_t)cmd_argc + 1, sizeof(char *));
    if (!cmd_argv) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    cmd_argv[0] = (char *)"./tests/run_tests.sh";
    for (int i = 2; i < argc; i++) {
        cmd_argv[i - 1] = argv[i];
    }
    cmd_argv[cmd_argc] = NULL;

    extern char **environ;
    pid_t pid = 0;
    int spawn_rc = posix_spawn(&pid, "./tests/run_tests.sh", NULL, NULL, cmd_argv, environ);
    if (spawn_rc != 0) {
        free(cmd_argv);
        fprintf(stderr, "failed to run tests\n");
        return 1;
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        free(cmd_argv);
        fprintf(stderr, "failed to wait for tests\n");
        return 1;
    }
    free(cmd_argv);

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return 1;
}

static int ensure_project_layout(const char *root, char *err, size_t err_cap) {
    char *dot_aja = join_path2(root, ".aja");
    char *site_packages = join_path2(dot_aja, "site-packages");
    char *pkgs = join_path2(dot_aja, "pkgs");
    char *registry = join_path2(dot_aja, "registry");
    int ok = ensure_dir_recursive(site_packages, err, err_cap);
    if (ok) {
        ok = ensure_dir_recursive(pkgs, err, err_cap);
    }
    if (ok) {
        ok = ensure_dir_recursive(registry, err, err_cap);
    }
    free(dot_aja);
    free(site_packages);
    free(pkgs);
    free(registry);
    return ok;
}

static int is_directory_path(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    return S_ISDIR(st.st_mode) ? 1 : 0;
}

typedef enum {
    STDLIB_TIER_CORE = 0,
    STDLIB_TIER_OPTIONAL = 1,
} StdlibTier;

typedef struct {
    const char *name;
    StdlibTier tier;
} StdlibModuleSpec;

static const StdlibModuleSpec k_stdlib_modules[] = {
    {"re", STDLIB_TIER_CORE},
    {"str", STDLIB_TIER_CORE},
    {"text", STDLIB_TIER_CORE},
    {"list", STDLIB_TIER_CORE},
    {"set", STDLIB_TIER_CORE},
    {"setutil", STDLIB_TIER_CORE},
    {"maputil", STDLIB_TIER_CORE},
    {"validate", STDLIB_TIER_CORE},
    {"assert", STDLIB_TIER_CORE},
    {"context", STDLIB_TIER_CORE},
    {"pathlib", STDLIB_TIER_CORE},
    {"httpx", STDLIB_TIER_OPTIONAL},
    {"collections", STDLIB_TIER_OPTIONAL},
    {"fileutil", STDLIB_TIER_OPTIONAL},
    {"env", STDLIB_TIER_OPTIONAL},
    {"log", STDLIB_TIER_OPTIONAL},
    {"retry", STDLIB_TIER_OPTIONAL},
    {"query", STDLIB_TIER_OPTIONAL},
    {"randutil", STDLIB_TIER_OPTIONAL},
    {"queue", STDLIB_TIER_OPTIONAL},
    {"stack", STDLIB_TIER_OPTIONAL},
    {"cache", STDLIB_TIER_OPTIONAL},
    {"kv", STDLIB_TIER_OPTIONAL},
    {"datetime", STDLIB_TIER_OPTIONAL},
};

static int stdlib_module_count(void) {
    return (int)(sizeof(k_stdlib_modules) / sizeof(k_stdlib_modules[0]));
}

static const StdlibModuleSpec *find_stdlib_module(const char *name) {
    int count = stdlib_module_count();
    for (int i = 0; i < count; i++) {
        if (strcmp(k_stdlib_modules[i].name, name) == 0) {
            return &k_stdlib_modules[i];
        }
    }
    return NULL;
}

static int contains_module_name(const char **names, int count, const char *name) {
    for (int i = 0; i < count; i++) {
        if (strcmp(names[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

static char *resolve_stdlib_source_root(const char *cwd, const char *from_arg) {
    if (from_arg && from_arg[0] != '\0') {
        if (from_arg[0] == '/') {
            return dup_s(from_arg);
        }
        return join_path2(cwd, from_arg);
    }

    const char *env_dir = getenv("AJA_STDLIB_DIR");
    if (env_dir && env_dir[0] != '\0') {
        return dup_s(env_dir);
    }

    char *from_cwd = join_path2(cwd, "libs");
    if (is_directory_path(from_cwd)) {
        return from_cwd;
    }
    free(from_cwd);

    if (is_directory_path("/usr/share/ajasendiri/libs")) {
        return dup_s("/usr/share/ajasendiri/libs");
    }
    if (is_directory_path("/usr/local/share/ajasendiri/libs")) {
        return dup_s("/usr/local/share/ajasendiri/libs");
    }

    return join_path2(cwd, "libs");
}

static int resolve_stdlib_target_site(int target_mode, const char *cwd, const char *cmd_name, char **out_target_site,
                                      char **out_project_root) {
    char err[512];
    *out_target_site = NULL;
    *out_project_root = NULL;

    if (target_mode == 1) {
        const char *home = getenv("HOME");
        if (!home || home[0] == '\0') {
            fprintf(stderr, "%s: HOME is not set\n", cmd_name);
            return 0;
        }
        char *dot_aja = join_path2(home, ".aja");
        char *target_site = join_path2(dot_aja, "site-packages");
        free(dot_aja);
        if (!ensure_dir_recursive(target_site, err, sizeof(err))) {
            fprintf(stderr, "%s: %s\n", cmd_name, err);
            free(target_site);
            return 0;
        }
        *out_target_site = target_site;
        return 1;
    }

    if (target_mode == 2) {
        const char *venv = getenv("AJA_VENV");
        if (!venv || venv[0] == '\0') {
            fprintf(stderr, "%s: AJA_VENV is not set\n", cmd_name);
            return 0;
        }
        char *venv_root = NULL;
        if (venv[0] == '/') {
            venv_root = dup_s(venv);
        } else {
            venv_root = join_path2(cwd, venv);
        }
        char *target_site = join_path2(venv_root, "site-packages");
        free(venv_root);
        if (!ensure_dir_recursive(target_site, err, sizeof(err))) {
            fprintf(stderr, "%s: %s\n", cmd_name, err);
            free(target_site);
            return 0;
        }
        *out_target_site = target_site;
        return 1;
    }

    char *project_root = find_project_root(cwd);
    if (!ensure_project_layout(project_root, err, sizeof(err))) {
        fprintf(stderr, "%s: %s\n", cmd_name, err);
        free(project_root);
        return 0;
    }
    *out_target_site = join_path2(project_root, ".aja/site-packages");
    *out_project_root = project_root;
    return 1;
}

static int install_stdlib_modules(const char *cmd_name, const char *source_root, const char *target_site,
                                  const char **module_names, int module_count, int *out_installed) {
    char err[512];
    *out_installed = 0;
    for (int i = 0; i < module_count; i++) {
        const char *name = module_names[i];
        size_t name_len = strlen(name);
        if (name_len > ((size_t)-1) - 5) {
            fprintf(stderr, "%s: module name too long\n", cmd_name);
            return 0;
        }
        char *rel = (char *)malloc(name_len + 5);
        if (!rel) {
            fprintf(stderr, "%s: out of memory\n", cmd_name);
            return 0;
        }
        memcpy(rel, name, name_len);
        memcpy(rel + name_len, ".aja", 5);

        char *src = join_path2(source_root, rel);
        if (!is_regular_file_path(src)) {
            fprintf(stderr, "%s: missing stdlib module: %s\n", cmd_name, src);
            free(src);
            free(rel);
            return 0;
        }

        char *dst = join_path2(target_site, rel);
        char *dst_dir = dirname_from_path2(dst);
        if (!ensure_dir_recursive(dst_dir, err, sizeof(err))) {
            fprintf(stderr, "%s: %s\n", cmd_name, err);
            free(dst_dir);
            free(dst);
            free(src);
            free(rel);
            return 0;
        }
        if (!copy_file_binary(src, dst, err, sizeof(err))) {
            fprintf(stderr, "%s: %s\n", cmd_name, err);
            free(dst_dir);
            free(dst);
            free(src);
            free(rel);
            return 0;
        }

        (*out_installed)++;
        free(dst_dir);
        free(dst);
        free(src);
        free(rel);
    }
    return 1;
}

static int handle_mod_install_coli(int argc, char **argv) {
    int target_mode = -1;  // -1=auto, 0=project, 1=global, 2=venv
    const char *from_arg = NULL;
    int install_optional = 0;
    int install_all = 0;

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--global") == 0) {
            target_mode = 1;
            continue;
        }
        if (strcmp(argv[i], "--project") == 0) {
            target_mode = 0;
            continue;
        }
        if (strcmp(argv[i], "--from") == 0 && i + 1 < argc) {
            from_arg = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--core") == 0) {
            install_optional = 0;
            install_all = 0;
            continue;
        }
        if (strcmp(argv[i], "--optional") == 0) {
            install_optional = 1;
            install_all = 0;
            continue;
        }
        if (strcmp(argv[i], "--all") == 0) {
            install_all = 1;
            continue;
        }
        fprintf(stderr,
                "usage: ./ajasendiri mmk install-coli [--global|--project] [--from <dir>] [--core|--optional|--all]\n");
        return 1;
    }

    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd))) {
        fprintf(stderr, "mmk install-coli: cannot get current directory\n");
        return 1;
    }

    if (target_mode < 0) {
        const char *venv = getenv("AJA_VENV");
        target_mode = (venv && venv[0] != '\0') ? 2 : 1;
    }

    char *target_site = NULL;
    char *project_root = NULL;
    if (!resolve_stdlib_target_site(target_mode, cwd, "mmk install-coli", &target_site, &project_root)) {
        return 1;
    }

    char *source_root = resolve_stdlib_source_root(cwd, from_arg);

    if (!is_directory_path(source_root)) {
        fprintf(stderr, "mmk install-coli: stdlib source directory not found: %s\n", source_root);
        free(source_root);
        free(target_site);
        free(project_root);
        return 1;
    }

    const char *selected[64];
    int selected_count = 0;
    int count = stdlib_module_count();
    for (int i = 0; i < count; i++) {
        const StdlibModuleSpec *spec = &k_stdlib_modules[i];
        if (install_all || (install_optional && spec->tier == STDLIB_TIER_OPTIONAL) ||
            (!install_optional && !install_all && spec->tier == STDLIB_TIER_CORE)) {
            selected[selected_count++] = spec->name;
        }
    }

    int installed = 0;
    if (!install_stdlib_modules("mmk install-coli", source_root, target_site, selected, selected_count, &installed)) {
        free(source_root);
        free(target_site);
        free(project_root);
        return 1;
    }

    const char *tier_label = install_all ? "all" : (install_optional ? "optional" : "core");
    if (target_mode == 1) {
        printf("installed %d %s stdlib module(s) into %s (global)\n", installed, tier_label, target_site);
    } else if (target_mode == 2) {
        printf("installed %d %s stdlib module(s) into %s (venv)\n", installed, tier_label, target_site);
    } else {
        printf("installed %d %s stdlib module(s) into %s (project)\n", installed, tier_label, target_site);
    }
    free(source_root);
    free(target_site);
    free(project_root);
    return 0;
}

static int handle_mod_install_named_stdlib(int argc, char **argv) {
    int target_mode = -1;  // -1=auto, 0=project, 1=global, 2=venv
    const char *from_arg = NULL;
    const char *selected[64];
    int selected_count = 0;

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--global") == 0) {
            target_mode = 1;
            continue;
        }
        if (strcmp(argv[i], "--project") == 0) {
            target_mode = 0;
            continue;
        }
        if (strcmp(argv[i], "--from") == 0 && i + 1 < argc) {
            from_arg = argv[++i];
            continue;
        }
        if (argv[i][0] == '-') {
            fprintf(stderr, "usage: ./ajasendiri mmk install <module...> [--global|--project] [--from <dir>]\n");
            return 1;
        }
        if (!find_stdlib_module(argv[i])) {
            fprintf(stderr, "mmk install: unknown bundled module '%s'\n", argv[i]);
            return 1;
        }
        if (!contains_module_name(selected, selected_count, argv[i])) {
            if (selected_count >= (int)(sizeof(selected) / sizeof(selected[0]))) {
                fprintf(stderr, "mmk install: too many module names\n");
                return 1;
            }
            selected[selected_count++] = argv[i];
        }
    }

    if (selected_count == 0) {
        fprintf(stderr, "usage: ./ajasendiri mmk install <module...> [--global|--project] [--from <dir>]\n");
        return 1;
    }

    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd))) {
        fprintf(stderr, "mmk install: cannot get current directory\n");
        return 1;
    }

    if (target_mode < 0) {
        const char *venv = getenv("AJA_VENV");
        target_mode = (venv && venv[0] != '\0') ? 2 : 1;
    }

    char *target_site = NULL;
    char *project_root = NULL;
    if (!resolve_stdlib_target_site(target_mode, cwd, "mmk install", &target_site, &project_root)) {
        return 1;
    }

    char *source_root = resolve_stdlib_source_root(cwd, from_arg);
    if (!is_directory_path(source_root)) {
        fprintf(stderr, "mmk install: stdlib source directory not found: %s\n", source_root);
        free(source_root);
        free(target_site);
        free(project_root);
        return 1;
    }

    int installed = 0;
    if (!install_stdlib_modules("mmk install", source_root, target_site, selected, selected_count, &installed)) {
        free(source_root);
        free(target_site);
        free(project_root);
        return 1;
    }

    if (target_mode == 1) {
        printf("installed %d bundled module(s) into %s (global)\n", installed, target_site);
    } else if (target_mode == 2) {
        printf("installed %d bundled module(s) into %s (venv)\n", installed, target_site);
    } else {
        printf("installed %d bundled module(s) into %s (project)\n", installed, target_site);
    }
    free(source_root);
    free(target_site);
    free(project_root);
    return 0;
}

static int handle_mod_init(int argc, char **argv) {
    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd))) {
        fprintf(stderr, "mmk init: cannot get current directory\n");
        return 1;
    }
    const char *project_name = argc >= 4 ? argv[3] : NULL;
    if (!project_name || project_name[0] == '\0') {
        const char *base = strrchr(cwd, '/');
        project_name = base ? base + 1 : cwd;
    }

    char err[512];
    if (!ensure_project_layout(cwd, err, sizeof(err))) {
        fprintf(stderr, "mmk init: %s\n", err);
        return 1;
    }

    char *toml = join_path2(cwd, "aja.toml");
    char *req = join_path2(cwd, "requirements.txt");
    char *lock = join_path2(cwd, "requirements.lock");

    if (!path_exists(toml)) {
        TextBuf buf;
        tb_init(&buf);
        tb_push_n(&buf, "[project]\n", strlen("[project]\n"));
        tb_push_n(&buf, "name = \"", strlen("name = \""));
        tb_push_n(&buf, project_name, strlen(project_name));
        tb_push_n(&buf, "\"\nversion = \"0.1.0\"\n", strlen("\"\nversion = \"0.1.0\"\n"));
        if (!write_file(toml, buf.buf, err, sizeof(err))) {
            fprintf(stderr, "mmk init: %s\n", err);
            free(buf.buf);
            free(toml);
            free(req);
            free(lock);
            return 1;
        }
        free(buf.buf);
        printf("created %s\n", toml);
    }

    if (!path_exists(req)) {
        if (!write_file(req, "# Ajasendiri dependencies\n", err, sizeof(err))) {
            fprintf(stderr, "mmk init: %s\n", err);
            free(toml);
            free(req);
            free(lock);
            return 1;
        }
        printf("created %s\n", req);
    }

    if (!path_exists(lock)) {
        if (!write_file(lock, "# generated by ajasendiri mmk install\n", err, sizeof(err))) {
            fprintf(stderr, "mmk init: %s\n", err);
            free(toml);
            free(req);
            free(lock);
            return 1;
        }
        printf("created %s\n", lock);
    }

    free(toml);
    free(req);
    free(lock);
    return 0;
}

static int rewrite_requirements_with_dep(const char *req_path, const char *module_name, const char *version,
                                         const char *source_path, char *err, size_t err_cap) {
    char *old = NULL;
    if (path_exists(req_path)) {
        old = read_file(req_path, err, err_cap);
        if (!old) {
            return 0;
        }
    } else {
        old = dup_s("");
    }

    TextBuf out;
    tb_init(&out);
    int replaced = 0;

    char *copy = dup_s(old);
    char *cur = copy;
    while (cur && *cur != '\0') {
        char *line = cur;
        char *nl = strchr(cur, '\n');
        if (nl) {
            *nl = '\0';
            cur = nl + 1;
        } else {
            cur = NULL;
        }

        char *work = dup_s(line);
        char *trimmed = trim_ws(work);
        if (trimmed[0] == '\0' || trimmed[0] == '#') {
            tb_push_n(&out, line, strlen(line));
            tb_push_char(&out, '\n');
            free(work);
            continue;
        }

        char *at = strchr(trimmed, '@');
        if (!at) {
            char *name = trimmed;
            char *eq = strstr(name, "==");
            if (eq) {
                *eq = '\0';
                name = trim_ws(name);
            }
            if (strcmp(name, module_name) == 0) {
                if (!replaced) {
                    tb_push_n(&out, module_name, strlen(module_name));
                    tb_push_n(&out, "==", strlen("=="));
                    tb_push_n(&out, version, strlen(version));
                    tb_push_n(&out, " @ ", strlen(" @ "));
                    tb_push_n(&out, source_path, strlen(source_path));
                    tb_push_char(&out, '\n');
                    replaced = 1;
                }
            } else {
                tb_push_n(&out, line, strlen(line));
                tb_push_char(&out, '\n');
            }
            free(work);
            continue;
        }
        *at = '\0';
        char *left = trim_ws(trimmed);
        char *eq = strstr(left, "==");
        char *name = left;
        if (eq) {
            *eq = '\0';
            name = trim_ws(left);
        }
        if (strcmp(name, module_name) == 0) {
            if (!replaced) {
                tb_push_n(&out, module_name, strlen(module_name));
                tb_push_n(&out, "==", strlen("=="));
                tb_push_n(&out, version, strlen(version));
                tb_push_n(&out, " @ ", strlen(" @ "));
                tb_push_n(&out, source_path, strlen(source_path));
                tb_push_char(&out, '\n');
                replaced = 1;
            }
        } else {
            tb_push_n(&out, line, strlen(line));
            tb_push_char(&out, '\n');
        }
        free(work);
    }
    free(copy);
    if (!replaced) {
        if (out.len > 0 && out.buf[out.len - 1] != '\n') {
            tb_push_char(&out, '\n');
        }
        tb_push_n(&out, module_name, strlen(module_name));
        tb_push_n(&out, "==", strlen("=="));
        tb_push_n(&out, version, strlen(version));
        tb_push_n(&out, " @ ", strlen(" @ "));
        tb_push_n(&out, source_path, strlen(source_path));
        tb_push_char(&out, '\n');
    }

    int ok = write_file(req_path, out.buf, err, err_cap);
    free(old);
    free(out.buf);
    return ok;
}

static int handle_mod_add(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr,
                "usage: ./ajasendiri mmk add <module_name|path/to/module.aja> --version <x.y.z|^x.y|~x.y|latest|*> [--name module_name]\n");
        return 1;
    }

    const char *src_arg = argv[3];
    const char *name_arg = NULL;
    const char *version_arg = NULL;
    for (int i = 4; i < argc; i++) {
        if (strcmp(argv[i], "--name") == 0 && i + 1 < argc) {
            if (name_arg) {
                fprintf(stderr, "mmk add: --name provided more than once\n");
                return 1;
            }
            name_arg = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--version") == 0 && i + 1 < argc) {
            if (version_arg) {
                fprintf(stderr, "mmk add: --version provided more than once\n");
                return 1;
            }
            version_arg = argv[++i];
            continue;
        }
        fprintf(stderr,
                "usage: ./ajasendiri mmk add <module_name|path/to/module.aja> --version <x.y.z|^x.y|~x.y|latest|*> [--name module_name]\n");
        return 1;
    }

    if (!version_arg) {
        fprintf(stderr, "mmk add: --version is required\n");
        fprintf(stderr,
                "usage: ./ajasendiri mmk add <module_name|path/to/module.aja> --version <x.y.z|^x.y|~x.y|latest|*> [--name module_name]\n");
        return 1;
    }

    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd))) {
        fprintf(stderr, "mmk add: cannot get current directory\n");
        return 1;
    }
    char *root = find_project_root(cwd);
    char err[512];
    if (!ensure_project_layout(root, err, sizeof(err))) {
        fprintf(stderr, "mmk add: %s\n", err);
        free(root);
        return 1;
    }

    char *req_path = join_path2(root, "requirements.txt");
    int source_is_path = ends_with(src_arg, ".aja");
    const char *src_use = src_arg;
    char *src_abs = NULL;
    char *module_name = NULL;
    char *source_value = NULL;
    char *resolved_version_owned = NULL;
    const char *resolved_version = version_arg;

    if (source_is_path) {
        if (!valid_version(version_arg)) {
            fprintf(stderr, "mmk add: local path dependencies require exact version x.y.z (got '%s')\n", version_arg);
            free(req_path);
            free(root);
            return 1;
        }
        if (src_arg[0] != '/') {
            src_abs = join_path2(cwd, src_arg);
            src_use = src_abs;
        }
        if (!is_regular_file_path(src_use)) {
            fprintf(stderr, "mmk add: dependency source not found: %s\n", src_use);
            free(src_abs);
            free(req_path);
            free(root);
            return 1;
        }
        if (!has_aja_extension(src_use)) {
            fprintf(stderr, "mmk add: dependency source must be .aja file: %s\n", src_use);
            free(src_abs);
            free(req_path);
            free(root);
            return 1;
        }
        module_name = name_arg ? dup_s(name_arg) : module_name_from_path(src_use);
        source_value = dup_s(src_arg);
    } else {
        if (name_arg) {
            fprintf(stderr, "mmk add: --name is not allowed when adding from registry by module name\n");
            free(req_path);
            free(root);
            return 1;
        }
        if (!valid_version_selector(version_arg)) {
            fprintf(stderr, "mmk add: invalid version selector '%s' (use x.y.z, ^x.y(.z), ~x.y(.z), latest, or *)\n",
                    version_arg);
            free(req_path);
            free(root);
            return 1;
        }
        module_name = dup_s(src_arg);
        char *registry_root = registry_root_from_project_root(root);
        if (!resolve_registry_version_selector(registry_root, module_name, version_arg, &resolved_version_owned, err,
                                               sizeof(err))) {
            fprintf(stderr, "mmk add: %s\n", err);
            free(registry_root);
            free(module_name);
            free(req_path);
            free(root);
            return 1;
        }
        resolved_version = resolved_version_owned;
        char *registry_mod = registry_module_path(registry_root, module_name, resolved_version);
        if (!is_regular_file_path(registry_mod)) {
            fprintf(stderr, "mmk add: registry package not found: %s==%s\n", module_name, resolved_version);
            fprintf(stderr, "hint: run './ajasendiri mmk publish <module.aja> --version %s'\n", resolved_version);
            free(registry_mod);
            free(registry_root);
            free(module_name);
            free(resolved_version_owned);
            free(req_path);
            free(root);
            return 1;
        }
        source_value = registry_source_for(module_name, resolved_version);
        free(registry_mod);
        free(registry_root);
    }

    if (!valid_module_name(module_name)) {
        fprintf(stderr, "mmk add: invalid module name '%s'\n", module_name);
        free(module_name);
        free(src_abs);
        free(source_value);
        free(resolved_version_owned);
        free(req_path);
        free(root);
        return 1;
    }

    if (!rewrite_requirements_with_dep(req_path, module_name, resolved_version, source_value, err, sizeof(err))) {
        fprintf(stderr, "mmk add: %s\n", err);
        free(module_name);
        free(src_abs);
        free(source_value);
        free(resolved_version_owned);
        free(req_path);
        free(root);
        return 1;
    }

    printf("added dependency %s==%s @ %s\n", module_name, resolved_version, source_value);
    free(module_name);
    free(src_abs);
    free(source_value);
    free(resolved_version_owned);
    free(req_path);
    free(root);
    return 0;
}

static int resolve_dep_source(const char *root, const DepSpec *dep, char **out_path, char *err, size_t err_cap) {
    if (is_registry_source(dep->source)) {
        char *src_name = NULL;
        char *src_version = NULL;
        if (!parse_registry_source(dep->source, &src_name, &src_version)) {
            snprintf(err, err_cap, "invalid registry source '%s'", dep->source);
            return 0;
        }
        if (strcmp(src_name, dep->name) != 0 || strcmp(src_version, dep->version) != 0) {
            snprintf(err, err_cap, "registry source mismatch for '%s': expected %s/%s, got %s/%s", dep->name,
                     dep->name, dep->version, src_name, src_version);
            free(src_name);
            free(src_version);
            return 0;
        }
        char *registry_root = registry_root_from_project_root(root);
        *out_path = registry_module_path(registry_root, dep->name, dep->version);
        free(registry_root);
        free(src_name);
        free(src_version);
        return 1;
    }
    if (dep->source[0] == '/') {
        *out_path = dup_s(dep->source);
        return 1;
    }
    *out_path = join_path2(root, dep->source);
    return 1;
}

static int verify_registry_dependency_signature(const char *root, const DepSpec *dep, const char *src_path,
                                               const char *src_hash, char *err, size_t err_cap) {
    if (!is_registry_source(dep->source)) {
        return 1;
    }

    char *meta_hash = NULL;
    char *meta_sig = NULL;
    if (!read_registry_meta_integrity(root, dep->name, dep->version, &meta_hash, &meta_sig, err, err_cap)) {
        return 0;
    }

    if (strcmp(meta_hash, src_hash) != 0) {
        snprintf(err, err_cap, "registry hash mismatch for %s (meta=%s current=%s)", dep->name, meta_hash, src_hash);
        free(meta_hash);
        free(meta_sig);
        return 0;
    }

    const char *require_sig = getenv("AJA_REQUIRE_SIGNATURE");
    int must_have_sig = require_sig && strcmp(require_sig, "1") == 0;
    const char *sign_key = getenv("AJA_SIGN_KEY");

    if (meta_sig && meta_sig[0] != '\0') {
        if (!sign_key || sign_key[0] == '\0') {
            snprintf(err, err_cap, "dependency %s is signed but AJA_SIGN_KEY is not set", dep->name);
            free(meta_hash);
            free(meta_sig);
            return 0;
        }
        char *src_text = read_file(src_path, err, err_cap);
        if (!src_text) {
            free(meta_hash);
            free(meta_sig);
            return 0;
        }
        char sig_hex[65];
        int sig_ok = hmac_sha256_hex((const unsigned char *)src_text, strlen(src_text), sign_key, sig_hex, err, err_cap);
        free(src_text);
        if (!sig_ok) {
            free(meta_hash);
            free(meta_sig);
            return 0;
        }
        if (strcmp(sig_hex, meta_sig) != 0) {
            snprintf(err, err_cap, "registry signature mismatch for %s", dep->name);
            free(meta_hash);
            free(meta_sig);
            return 0;
        }
    } else if (must_have_sig) {
        snprintf(err, err_cap, "dependency %s is missing required signature", dep->name);
        free(meta_hash);
        free(meta_sig);
        return 0;
    }

    free(meta_hash);
    free(meta_sig);
    return 1;
}

static int validate_locked_requirements(const DepSpecList *deps, const DepSpecList *locks, char *err, size_t err_cap) {
    if (deps->count != locks->count) {
        snprintf(err, err_cap, "lock mismatch: requirements has %d deps, lock has %d deps", deps->count, locks->count);
        return 0;
    }
    for (int i = 0; i < deps->count; i++) {
        DepSpec *req = &deps->items[i];
        DepSpec *locked = dep_list_find_by_name(locks, req->name);
        if (!locked) {
            snprintf(err, err_cap, "lock mismatch: '%s' missing from requirements.lock", req->name);
            return 0;
        }
        if (strcmp(req->version, locked->version) != 0 || strcmp(req->source, locked->source) != 0) {
            snprintf(err, err_cap, "lock mismatch for '%s': requirements and lock differ", req->name);
            return 0;
        }
    }
    return 1;
}

static int handle_mod_install(int argc, char **argv) {
    if (argc >= 4 && argv[3][0] != '-') {
        return handle_mod_install_named_stdlib(argc, argv);
    }

    int locked_mode = 0;
    if (argc == 4 && strcmp(argv[3], "--locked") == 0) {
        locked_mode = 1;
    } else if (argc != 3) {
        fprintf(stderr, "usage: ./ajasendiri mmk install [--locked]\n");
        fprintf(stderr, "   or: ./ajasendiri mmk install <module...> [--global|--project] [--from <dir>]\n");
        return 1;
    }

    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd))) {
        fprintf(stderr, "mmk install: cannot get current directory\n");
        return 1;
    }
    char *root = find_project_root(cwd);
    char *req_path = join_path2(root, "requirements.txt");
    char *lock_path = join_path2(root, "requirements.lock");
    int using_venv = 0;
    char *site_root = resolve_site_packages_root(root, &using_venv);
    char err[512];

    if (!path_exists(req_path)) {
        fprintf(stderr, "mmk install: requirements.txt not found (run 'ajasendiri mmk init')\n");
        free(root);
        free(req_path);
        free(lock_path);
        free(site_root);
        return 1;
    }
    if (!ensure_project_layout(root, err, sizeof(err))) {
        fprintf(stderr, "mmk install: %s\n", err);
        free(root);
        free(req_path);
        free(lock_path);
        free(site_root);
        return 1;
    }

    if (!ensure_dir_recursive(site_root, err, sizeof(err))) {
        fprintf(stderr, "mmk install: %s\n", err);
        free(root);
        free(req_path);
        free(lock_path);
        free(site_root);
        return 1;
    }

    char *req_content = read_file(req_path, err, sizeof(err));
    if (!req_content) {
        fprintf(stderr, "mmk install: %s\n", err);
        free(root);
        free(req_path);
        free(lock_path);
        free(site_root);
        return 1;
    }

    DepSpecList deps;
    memset(&deps, 0, sizeof(deps));
    if (!parse_requirements_text(req_content, &deps, err, sizeof(err))) {
        fprintf(stderr, "mmk install: %s\n", err);
        free(req_content);
        dep_list_free(&deps);
        free(root);
        free(req_path);
        free(lock_path);
        free(site_root);
        return 1;
    }

    DepSpecList locked;
    memset(&locked, 0, sizeof(locked));
    if (locked_mode) {
        if (!path_exists(lock_path)) {
            fprintf(stderr, "mmk install: requirements.lock not found (run 'ajasendiri mmk install')\n");
            free(req_content);
            dep_list_free(&deps);
            free(root);
            free(req_path);
            free(lock_path);
            free(site_root);
            return 1;
        }
        char *lock_content = read_file(lock_path, err, sizeof(err));
        if (!lock_content) {
            fprintf(stderr, "mmk install: %s\n", err);
            free(req_content);
            dep_list_free(&deps);
            free(root);
            free(req_path);
            free(lock_path);
            return 1;
        }
        if (!parse_lock_text(lock_content, &locked, err, sizeof(err))) {
            fprintf(stderr, "mmk install: %s\n", err);
            free(lock_content);
            free(req_content);
            dep_list_free(&deps);
            dep_list_free(&locked);
            free(root);
            free(req_path);
            free(lock_path);
            free(site_root);
            return 1;
        }
        free(lock_content);
        if (!validate_locked_requirements(&deps, &locked, err, sizeof(err))) {
            fprintf(stderr, "mmk install: %s\n", err);
            free(req_content);
            dep_list_free(&deps);
            dep_list_free(&locked);
            free(root);
            free(req_path);
            free(lock_path);
            free(site_root);
            return 1;
        }
    }

    TextBuf lock_buf;
    tb_init(&lock_buf);
    tb_push_n(&lock_buf, "# generated by ajasendiri mmk install\n",
              strlen("# generated by ajasendiri mmk install\n"));

    int installed = 0;
    for (int i = 0; i < deps.count; i++) {
        DepSpec *d = &deps.items[i];
        char *src = NULL;
        if (!resolve_dep_source(root, d, &src, err, sizeof(err))) {
            fprintf(stderr, "mmk install: %s\n", err);
            free(req_content);
            dep_list_free(&deps);
            dep_list_free(&locked);
            free(lock_buf.buf);
            free(root);
            free(req_path);
            free(lock_path);
            free(site_root);
            return 1;
        }
        if (!is_regular_file_path(src)) {
            fprintf(stderr, "mmk install: dependency source not found for %s: %s\n", d->name, src);
            free(src);
            free(req_content);
            dep_list_free(&deps);
            dep_list_free(&locked);
            free(lock_buf.buf);
            free(root);
            free(req_path);
            free(lock_path);
            free(site_root);
            return 1;
        }
        if (!has_aja_extension(src)) {
            fprintf(stderr, "mmk install: dependency source for %s must be .aja file: %s\n", d->name, src);
            free(src);
            free(req_content);
            dep_list_free(&deps);
            dep_list_free(&locked);
            free(lock_buf.buf);
            free(root);
            free(req_path);
            free(lock_path);
            free(site_root);
            return 1;
        }

        char hash_hex[65];
        if (!hash_file_sha256_hex64(src, hash_hex, err, sizeof(err))) {
            fprintf(stderr, "mmk install: %s\n", err);
            free(src);
            free(req_content);
            dep_list_free(&deps);
            dep_list_free(&locked);
            free(lock_buf.buf);
            free(root);
            free(req_path);
            free(lock_path);
            return 1;
        }
        if (!verify_registry_dependency_signature(root, d, src, hash_hex, err, sizeof(err))) {
            fprintf(stderr, "mmk install: %s\n", err);
            free(src);
            free(req_content);
            dep_list_free(&deps);
            dep_list_free(&locked);
            free(lock_buf.buf);
            free(root);
            free(req_path);
            free(lock_path);
            free(site_root);
            return 1;
        }
        if (locked_mode) {
            DepSpec *locked_dep = dep_list_find_by_name(&locked, d->name);
            if (!locked_dep || strcmp(hash_hex, locked_dep->hash) != 0) {
                fprintf(stderr,
                        "mmk install: hash mismatch for %s (locked=%s current=%s). run 'ajasendiri mmk install' to refresh lock.\n",
                        d->name, locked_dep ? locked_dep->hash : "<missing>", hash_hex);
                free(src);
                free(req_content);
                dep_list_free(&deps);
                dep_list_free(&locked);
                free(lock_buf.buf);
                free(root);
                free(req_path);
                free(lock_path);
                free(site_root);
                return 1;
            }
        }

        size_t dep_name_len = strlen(d->name);
        if (dep_name_len > ((size_t)-1) - 5) {
            fprintf(stderr, "mmk install: dependency name too long\n");
            free(src);
            free(req_content);
            dep_list_free(&deps);
            dep_list_free(&locked);
            free(lock_buf.buf);
            free(root);
            free(req_path);
            free(lock_path);
            free(site_root);
            return 1;
        }
        char *mod_file_rel = (char *)malloc(dep_name_len + 5);
        if (!mod_file_rel) {
            fprintf(stderr, "mmk install: out of memory\n");
            free(src);
            free(req_content);
            dep_list_free(&deps);
            dep_list_free(&locked);
            free(lock_buf.buf);
            free(root);
            free(req_path);
            free(lock_path);
            free(site_root);
            return 1;
        }
        memcpy(mod_file_rel, d->name, dep_name_len);
        memcpy(mod_file_rel + dep_name_len, ".aja", 5);

        char *dst = join_path2(site_root, mod_file_rel);
        char *dst_dir = dirname_from_path2(dst);
        if (!ensure_dir_recursive(dst_dir, err, sizeof(err))) {
            fprintf(stderr, "mmk install: %s\n", err);
            free(dst_dir);
            free(dst);
            free(mod_file_rel);
            free(src);
            free(req_content);
            dep_list_free(&deps);
            dep_list_free(&locked);
            free(lock_buf.buf);
            free(root);
            free(req_path);
            free(lock_path);
            free(site_root);
            return 1;
        }

        if (!copy_file_binary(src, dst, err, sizeof(err))) {
            fprintf(stderr, "mmk install: %s\n", err);
            free(dst_dir);
            free(dst);
            free(mod_file_rel);
            free(src);
            free(req_content);
            dep_list_free(&deps);
            dep_list_free(&locked);
            free(lock_buf.buf);
            free(root);
            free(req_path);
            free(lock_path);
            free(site_root);
            return 1;
        }

        if (!write_dep_metadata(site_root, d, hash_hex, src, err, sizeof(err))) {
            fprintf(stderr, "mmk install: %s\n", err);
            free(dst_dir);
            free(dst);
            free(mod_file_rel);
            free(src);
            free(req_content);
            dep_list_free(&deps);
            dep_list_free(&locked);
            free(lock_buf.buf);
            free(root);
            free(req_path);
            free(lock_path);
            free(site_root);
            return 1;
        }

        tb_push_n(&lock_buf, d->name, strlen(d->name));
        tb_push_n(&lock_buf, "==", strlen("=="));
        tb_push_n(&lock_buf, d->version, strlen(d->version));
        tb_push_n(&lock_buf, " @ ", strlen(" @ "));
        tb_push_n(&lock_buf, d->source, strlen(d->source));
        tb_push_n(&lock_buf, " | hash=", strlen(" | hash="));
        tb_push_n(&lock_buf, hash_hex, strlen(hash_hex));
        tb_push_char(&lock_buf, '\n');

        installed++;
        free(dst_dir);
        free(dst);
        free(mod_file_rel);
        free(src);
    }

    if (!locked_mode) {
        if (!write_file(lock_path, lock_buf.buf, err, sizeof(err))) {
            fprintf(stderr, "mmk install: %s\n", err);
            free(req_content);
            dep_list_free(&deps);
            dep_list_free(&locked);
            free(lock_buf.buf);
            free(root);
            free(req_path);
            free(lock_path);
            free(site_root);
            return 1;
        }
    }

    const char *target_label = using_venv ? "AJA_VENV/site-packages" : ".aja/site-packages";
    if (locked_mode) {
        printf("installed %d dependency(s) into %s (locked)\n", installed, target_label);
    } else {
        printf("installed %d dependency(s) into %s\n", installed, target_label);
    }
    free(req_content);
    dep_list_free(&deps);
    dep_list_free(&locked);
    free(lock_buf.buf);
    free(root);
    free(req_path);
    free(lock_path);
    free(site_root);
    return 0;
}
