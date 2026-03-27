static char *normalize_named_call_spacing(const char *line) {
    TextBuf out;
    tb_init(&out);

    int in_string = 0;
    char quote = '\0';
    int escaped = 0;
    int paren_depth = 0;

    size_t i = 0;
    size_t n = strlen(line);
    while (i < n) {
        char c = line[i];

        if (in_string) {
            tb_push_char(&out, c);
            if (escaped) {
                escaped = 0;
            } else if (c == '\\') {
                escaped = 1;
            } else if (c == quote) {
                in_string = 0;
                quote = '\0';
            }
            i++;
            continue;
        }

        if (c == '"' || c == '\'') {
            in_string = 1;
            quote = c;
            tb_push_char(&out, c);
            i++;
            continue;
        }

        if (c == '#') {
            tb_push_n(&out, line + i, n - i);
            break;
        }

        if (c == '(') {
            paren_depth++;
            tb_push_char(&out, c);
            i++;
            continue;
        }
        if (c == ')') {
            if (paren_depth > 0) {
                paren_depth--;
            }
            tb_push_char(&out, c);
            i++;
            continue;
        }

        if (paren_depth > 0 && c == ',') {
            tb_push_char(&out, ',');
            i++;
            while (i < n && (line[i] == ' ' || line[i] == '\t')) {
                i++;
            }
            if (i < n && line[i] != ')' && line[i] != '#') {
                tb_push_char(&out, ' ');
            }
            continue;
        }

        if (paren_depth > 0 && c == '=' && (i == 0 || (line[i - 1] != '=' && line[i - 1] != '!' && line[i - 1] != '<' &&
                                                         line[i - 1] != '>')) &&
            (i + 1 >= n || line[i + 1] != '=')) {
            while (out.len > 0 && out.buf[out.len - 1] == ' ') {
                out.len--;
                out.buf[out.len] = '\0';
            }
            tb_push_n(&out, " = ", 3);
            i++;
            while (i < n && (line[i] == ' ' || line[i] == '\t')) {
                i++;
            }
            continue;
        }

        tb_push_char(&out, c);
        i++;
    }

    return tb_take(&out);
}

static void write_prefixed_error(char *err, size_t err_cap, const char *prefix, const char *detail) {
    if (!err || err_cap == 0) {
        return;
    }
    err[0] = '\0';
    if (!prefix) {
        prefix = "";
    }
    if (!detail) {
        detail = "";
    }

    size_t used = 0;
    size_t prefix_len = strlen(prefix);
    if (prefix_len >= err_cap) {
        prefix_len = err_cap - 1;
    }
    memcpy(err, prefix, prefix_len);
    err[prefix_len] = '\0';
    used = prefix_len;

    if (used + 1 >= err_cap) {
        return;
    }

    size_t remain = err_cap - used - 1;
    size_t detail_len = strlen(detail);
    if (detail_len > remain) {
        detail_len = remain;
    }
    memcpy(err + used, detail, detail_len);
    err[used + detail_len] = '\0';
}

static int validate_source_ast(const char *src, char *err, size_t err_cap) {
    TokenArray toks;
    token_array_init(&toks);

    char parse_err[512];
    if (!tokenize_source(src, &toks, parse_err, sizeof(parse_err))) {
        token_array_free(&toks);
        write_prefixed_error(err, err_cap, "lex error: ", parse_err);
        return 0;
    }

    Program *prog = parse_program(&toks, parse_err, sizeof(parse_err));
    token_array_free(&toks);
    if (!prog) {
        write_prefixed_error(err, err_cap, "parse error: ", parse_err);
        return 0;
    }
    return 1;
}

static int format_source(const char *src, char **out, char *err, size_t err_cap) {
    if (!validate_source_ast(src, err, err_cap)) {
        return 0;
    }

    size_t len = strlen(src);
    size_t start = 0;
    int line_no = 1;

    int stack_cap = 16;
    int *indent_stack = (int *)malloc((size_t)stack_cap * sizeof(int));
    int stack_depth = 1;
    if (!indent_stack) {
        snprintf(err, err_cap, "out of memory");
        return 0;
    }
    indent_stack[0] = 0;

    TextBuf out_buf;
    tb_init(&out_buf);

    while (start < len) {
        size_t end = start;
        while (end < len && src[end] != '\n') {
            end++;
        }
        size_t raw_len = end - start;
        char *line = (char *)malloc(raw_len + 1);
        if (!line) {
            free(indent_stack);
            free(out_buf.buf);
            snprintf(err, err_cap, "out of memory");
            return 0;
        }
        if (raw_len > 0) {
            memcpy(line, src + start, raw_len);
        }
        line[raw_len] = '\0';

        if (strchr(line, '\t') != NULL) {
            free(line);
            free(indent_stack);
            free(out_buf.buf);
            snprintf(err, err_cap, "line %d: tabs are not allowed; use spaces", line_no);
            return 0;
        }

        if (raw_len > 0 && line[raw_len - 1] == '\r') {
            line[raw_len - 1] = '\0';
            raw_len--;
        }
        while (raw_len > 0 && (line[raw_len - 1] == ' ' || line[raw_len - 1] == '\t')) {
            line[raw_len - 1] = '\0';
            raw_len--;
        }

        int indent = count_leading_spaces(line);
        const char *trimmed = line + indent;
        if (*trimmed == '\0') {
            tb_push_char(&out_buf, '\n');
        } else if (*trimmed == '#') {
            int depth = stack_depth - 1;
            for (int i = 0; i < depth * 4; i++) {
                tb_push_char(&out_buf, ' ');
            }
            tb_push_n(&out_buf, trimmed, strlen(trimmed));
            tb_push_char(&out_buf, '\n');
        } else {
            int top = indent_stack[stack_depth - 1];
            if (indent > top) {
                if (!push_indent_level(&indent_stack, &stack_depth, &stack_cap, indent)) {
                    free(line);
                    free(indent_stack);
                    free(out_buf.buf);
                    snprintf(err, err_cap, "out of memory");
                    return 0;
                }
            } else if (indent < top) {
                while (stack_depth > 1 && indent_stack[stack_depth - 1] > indent) {
                    stack_depth--;
                }
                if (indent_stack[stack_depth - 1] != indent) {
                    free(line);
                    free(indent_stack);
                    free(out_buf.buf);
                    snprintf(err, err_cap, "line %d: invalid indentation", line_no);
                    return 0;
                }
            }

            int depth = stack_depth - 1;
            for (int i = 0; i < depth * 4; i++) {
                tb_push_char(&out_buf, ' ');
            }
            char *normalized = normalize_named_call_spacing(trimmed);
            if (!normalized) {
                free(line);
                free(indent_stack);
                free(out_buf.buf);
                snprintf(err, err_cap, "out of memory");
                return 0;
            }
            tb_push_n(&out_buf, normalized, strlen(normalized));
            free(normalized);
            tb_push_char(&out_buf, '\n');
        }

        free(line);
        start = end + 1;
        line_no++;
    }

    free(indent_stack);
    *out = tb_take(&out_buf);

    if (!validate_source_ast(*out, err, err_cap)) {
        free(*out);
        *out = NULL;
        return 0;
    }

    return 1;
}

static int parse_line_col_from_error(const char *msg, int *out_line, int *out_col) {
    const char *p = strstr(msg, "line ");
    while (p) {
        p += 5;
        if (!isdigit((unsigned char)*p)) {
            p = strstr(p, "line ");
            continue;
        }

        char *end_num = NULL;
        long line = strtol(p, &end_num, 10);
        if (end_num == NULL || line <= 0) {
            p = strstr(p, "line ");
            continue;
        }

        if (*end_num == ':') {
            const char *after_colon = end_num + 1;
            if (isdigit((unsigned char)*after_colon)) {
                char *end_col = NULL;
                long col = strtol(after_colon, &end_col, 10);
                if (end_col && *end_col == ':' && col > 0) {
                    *out_line = (int)line;
                    *out_col = (int)col;
                    return 1;
                }
            } else {
                *out_line = (int)line;
                *out_col = 1;
                return 1;
            }
        }

        p = strstr(p, "line ");
    }

    return 0;
}

static int find_line_span(const char *src, int line, const char **out_start, const char **out_end) {
    if (!src || line <= 0) {
        return 0;
    }
    const char *start = src;
    int current = 1;
    while (*start != '\0' && current < line) {
        if (*start == '\n') {
            current++;
        }
        start++;
    }
    if (current != line) {
        return 0;
    }
    const char *end = start;
    while (*end != '\0' && *end != '\n') {
        end++;
    }
    *out_start = start;
    *out_end = end;
    return 1;
}

static void print_error_with_context(const char *label, const char *detail, const char *src) {
    fprintf(stderr, "%s: %s\n", label, detail);

    if (!src) {
        return;
    }

    int line = 0;
    int col = 0;
    if (!parse_line_col_from_error(detail, &line, &col)) {
        return;
    }

    const char *line_start = NULL;
    const char *line_end = NULL;
    if (!find_line_span(src, line, &line_start, &line_end)) {
        return;
    }

    int line_len = (int)(line_end - line_start);
    if (line_len < 0) {
        return;
    }
    fprintf(stderr, "%.*s\n", line_len, line_start);

    int caret_col = col > 0 ? col : 1;
    int max_col = line_len + 1;
    if (caret_col > max_col) {
        caret_col = max_col;
    }
    for (int i = 1; i < caret_col; i++) {
        fputc(' ', stderr);
    }
    fputc('^', stderr);
    fputc('\n', stderr);
}

static int run_source_text(const char *src, const char *label, int check_only) {
    char err[512];

    TokenArray toks;
    token_array_init(&toks);

    if (!tokenize_source(src, &toks, err, sizeof(err))) {
        print_error_with_context("lex error", err, src);
        token_array_free(&toks);
        return 1;
    }

    Program *prog = parse_program(&toks, err, sizeof(err));
    if (!prog) {
        print_error_with_context("parse error", err, src);
        token_array_free(&toks);
        return 1;
    }

    if (!run_program(prog, label, check_only, err, sizeof(err))) {
        print_error_with_context("runtime error", err, src);
        token_array_free(&toks);
        return 1;
    }

    token_array_free(&toks);
    return 0;
}

static int run_source_text_debug(const char *src, const char *label, const char *breakpoints_csv, int step_mode) {
    char err[512];

    TokenArray toks;
    token_array_init(&toks);

    if (!tokenize_source(src, &toks, err, sizeof(err))) {
        print_error_with_context("lex error", err, src);
        token_array_free(&toks);
        return 1;
    }

    Program *prog = parse_program(&toks, err, sizeof(err));
    if (!prog) {
        print_error_with_context("parse error", err, src);
        token_array_free(&toks);
        return 1;
    }

    if (!run_program_debug(prog, label, breakpoints_csv, step_mode, err, sizeof(err))) {
        print_error_with_context("runtime error", err, src);
        token_array_free(&toks);
        return 1;
    }

    token_array_free(&toks);
    return 0;
}

static int run_one_file(const char *path, int check_only) {
    if (!has_aja_extension(path)) {
        fprintf(stderr, "input file must use .aja extension\n");
        return 1;
    }

    char err[512];

    char *src = read_file(path, err, sizeof(err));
    if (!src) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    int rc = run_source_text(src, path, check_only);
    free(src);
    return rc;
}

static int handle_debug_command(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: ./ajasendiri debug <file.aja> [--break line[,line...]] [--step]\n");
        return 1;
    }
    const char *path = argv[2];
    const char *break_csv = NULL;
    int step_mode = 0;
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--break") == 0 && i + 1 < argc) {
            if (break_csv) {
                fprintf(stderr, "debug: --break provided more than once\n");
                return 1;
            }
            break_csv = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--step") == 0) {
            step_mode = 1;
            continue;
        }
        fprintf(stderr, "usage: ./ajasendiri debug <file.aja> [--break line[,line...]] [--step]\n");
        return 1;
    }

    if (!has_aja_extension(path)) {
        fprintf(stderr, "input file must use .aja extension\n");
        return 1;
    }

    char err[512];
    char *src = read_file(path, err, sizeof(err));
    if (!src) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }

    if (!break_csv && !step_mode) {
        step_mode = 1;
    }
    int rc = run_source_text_debug(src, path, break_csv, step_mode);
    free(src);
    return rc;
}

static void print_repl_help(void) {
    printf("REPL commands:\n");
    printf("  .run   execute current buffer\n");
    printf("  .show  print current buffer\n");
    printf("  .clear clear current buffer\n");
    printf("  .exit  quit repl\n");
    printf("  .help  show this help\n");
}

static int handle_repl_command(int argc, char **argv) {
    (void)argv;
    if (argc != 2 && argc != 1) {
        fprintf(stderr, "usage: ./ajasendiri repl\n");
        return 1;
    }

    int interactive = isatty(0) && isatty(1);
    if (interactive) {
        printf("AjaSendiri Codeshell v%s\n", AJA_VERSION);
        printf("The modern-lightweight and ease-to-use language\n");
        printf("type code lines, then .run\n");
        printf("type .help for commands\n");
    }

    TextBuf program;
    tb_init(&program);

    char line[4096];
    while (1) {
        if (interactive) {
            printf("aja> ");
            fflush(stdout);
        }

        if (!fgets(line, sizeof(line), stdin)) {
            if (interactive) {
                printf("\n");
            }
            break;
        }

        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
            line[--n] = '\0';
        }

        char cmd_buf[4096];
        memcpy(cmd_buf, line, n + 1);
        char *cmd = trim_ws(cmd_buf);

        if (strcmp(cmd, ".exit") == 0 || strcmp(cmd, ".quit") == 0) {
            break;
        }
        if (strcmp(cmd, ".help") == 0) {
            print_repl_help();
            continue;
        }
        if (strcmp(cmd, ".clear") == 0) {
            program.len = 0;
            if (program.buf) {
                program.buf[0] = '\0';
            }
            continue;
        }
        if (strcmp(cmd, ".show") == 0) {
            if (program.len == 0) {
                printf("(empty)\n");
            } else {
                printf("%s", program.buf);
                if (program.buf[program.len - 1] != '\n') {
                    printf("\n");
                }
            }
            continue;
        }
        if (strcmp(cmd, ".run") == 0) {
            if (program.len == 0) {
                if (interactive) {
                    printf("(empty)\n");
                }
                continue;
            }
            (void)run_source_text(program.buf, "<repl>", 0);
            continue;
        }

        tb_push_n(&program, line, strlen(line));
        tb_push_char(&program, '\n');
    }

    free(program.buf);
    return 0;
}

static int format_one_file(const char *path, int check_only, int write_mode, int *changed) {
    char err[512];
    char *src = read_file(path, err, sizeof(err));
    if (!src) {
        fprintf(stderr, "%s\n", err);
        return 0;
    }

    char *formatted = NULL;
    if (!format_source(src, &formatted, err, sizeof(err))) {
        fprintf(stderr, "fmt error: %s: %s\n", path, err);
        free(src);
        return 0;
    }

    int is_changed = strcmp(src, formatted) != 0;
    if (is_changed) {
        *changed += 1;
        if (check_only) {
            printf("%s\n", path);
        } else {
            if (write_mode) {
                if (!write_file(path, formatted, err, sizeof(err))) {
                    fprintf(stderr, "%s\n", err);
                    free(src);
                    free(formatted);
                    return 0;
                }
                printf("formatted %s\n", path);
            } else {
                printf("%s", formatted);
            }
        }
    }

    free(src);
    free(formatted);
    return 1;
}

static int format_path_recursive(const char *path, int from_dir, int check_only, int write_mode, int *changed,
                                 int *seen_aja_files) {
    struct stat st;
    if (stat(path, &st) != 0) {
        fprintf(stderr, "fmt error: cannot stat %s: %s\n", path, strerror(errno));
        return 0;
    }

    if (S_ISDIR(st.st_mode)) {
        DIR *dir = opendir(path);
        if (!dir) {
            fprintf(stderr, "fmt error: cannot open directory %s: %s\n", path, strerror(errno));
            return 0;
        }
        struct dirent *ent = NULL;
        while ((ent = readdir(dir)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
                continue;
            }
            size_t path_len = strlen(path);
            size_t name_len = strlen(ent->d_name);
            char *child = (char *)malloc(path_len + 1 + name_len + 1);
            if (!child) {
                closedir(dir);
                fprintf(stderr, "fmt error: out of memory\n");
                return 0;
            }
            memcpy(child, path, path_len);
            child[path_len] = '/';
            memcpy(child + path_len + 1, ent->d_name, name_len);
            child[path_len + 1 + name_len] = '\0';
            if (!format_path_recursive(child, 1, check_only, write_mode, changed, seen_aja_files)) {
                free(child);
                closedir(dir);
                return 0;
            }
            free(child);
        }
        closedir(dir);
        return 1;
    }

    if (!S_ISREG(st.st_mode)) {
        return 1;
    }

    if (!has_aja_extension(path)) {
        if (!from_dir) {
            fprintf(stderr, "fmt error: input file must use .aja extension: %s\n", path);
            return 0;
        }
        return 1;
    }

    *seen_aja_files += 1;
    return format_one_file(path, check_only, write_mode, changed);
}

static int handle_fmt_command(int argc, char **argv) {
    int check_only = 0;
    int write_mode = 1;
    int stdin_mode = 0;
    int first_path = 2;

    while (first_path < argc) {
        const char *arg = argv[first_path];
        if (strcmp(arg, "--check") == 0) {
            check_only = 1;
        } else if (strcmp(arg, "--write") == 0) {
            write_mode = 1;
        } else if (strcmp(arg, "--stdin") == 0) {
            stdin_mode = 1;
        } else if (arg[0] == '-') {
            fprintf(stderr, "fmt: unknown option '%s'\n", arg);
            return 1;
        } else {
            break;
        }
        first_path++;
    }

    if (stdin_mode) {
        if (first_path < argc) {
            fprintf(stderr, "fmt: --stdin does not accept file paths\n");
            return 1;
        }
        char err[512];
        char *src = read_stdin_all(err, sizeof(err));
        if (!src) {
            fprintf(stderr, "fmt error: %s\n", err);
            return 1;
        }
        char *formatted = NULL;
        if (!format_source(src, &formatted, err, sizeof(err))) {
            fprintf(stderr, "fmt error: %s\n", err);
            free(src);
            return 1;
        }
        int changed = strcmp(src, formatted) != 0;
        if (check_only && changed) {
            fprintf(stderr, "fmt: stdin needs formatting\n");
            free(src);
            free(formatted);
            return 1;
        }
        fputs(formatted, stdout);
        free(src);
        free(formatted);
        return 0;
    }

    int changed = 0;
    int seen_aja_files = 0;

    if (first_path >= argc) {
        if (!format_path_recursive(".", 0, check_only, write_mode, &changed, &seen_aja_files)) {
            return 1;
        }
    } else {
        for (int i = first_path; i < argc; i++) {
            if (!format_path_recursive(argv[i], 0, check_only, write_mode, &changed, &seen_aja_files)) {
                return 1;
            }
        }
    }

    if (seen_aja_files == 0) {
        fprintf(stderr, "fmt: no .aja files found\n");
        return 1;
    }
    if (check_only && changed > 0) {
        fprintf(stderr, "fmt: %d file(s) need formatting\n", changed);
        return 1;
    }
    return 0;
}
