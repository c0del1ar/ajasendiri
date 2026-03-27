#ifndef AJA_CORE_API_H
#define AJA_CORE_API_H

#include <stddef.h>

#include "core/ast.h"

void token_array_init(TokenArray *arr);
void token_array_free(TokenArray *arr);

int tokenize_source(const char *src, TokenArray *out, char *err, size_t err_cap);
Program *parse_program(TokenArray *tokens, char *err, size_t err_cap);
int run_program(Program *prog, const char *entry_path, int check_only, char *err, size_t err_cap);
int run_program_debug(Program *prog, const char *entry_path, const char *breakpoints_csv, int step_mode, char *err,
                      size_t err_cap);

#endif
