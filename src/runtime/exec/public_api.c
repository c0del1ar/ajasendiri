int run_program(Program *prog, const char *entry_path, int check_only, char *err, size_t err_cap) {
    return run_program_with_options(prog, entry_path, check_only, 0, NULL, 0, err, err_cap);
}

int run_program_debug(Program *prog, const char *entry_path, const char *breakpoints_csv, int step_mode, char *err,
                      size_t err_cap) {
    return run_program_with_options(prog, entry_path, 0, 1, breakpoints_csv, step_mode, err, err_cap);
}
