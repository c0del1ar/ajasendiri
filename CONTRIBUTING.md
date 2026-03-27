# Contributing to Ajasendiri

This guide explains how to contribute code in a way that keeps the project readable and maintainable.

## Development Setup

Requirements:

- C compiler (`cc`, `gcc`, or `clang`)
- `make`
- `python3` (for LSP helper)

Build:

```bash
make
```

Run full tests:

```bash
make test
```

## Project Structure (quick map)

- `src/lexer/`: tokenization
- `src/parser/`: AST building
- `src/runtime/`: execution engine, type checks, native modules
- `src/cli/`: command-line tools (`run`, `check`, `fmt`, `repl`, `debug`, `mmk`)
- `include/core/`: shared token/AST/API definitions
- `libs/`: pure Ajasendiri stdlib-style modules
- `tests/spec/pass`: programs that must pass
- `tests/spec/fail`: programs that must fail with expected errors
- `docs/`: user/dev docs

For detailed architecture, see `docs/architecture.rst`.

## Where to Put Changes

### Language syntax or grammar

- Tokens: `src/lexer/lexer.c`
- Parser and AST wiring: `src/parser/*.c` and `include/core/{token.h,ast.h}`

### Runtime behavior

- Expression/statement execution: `src/runtime/exec/*.c`
- Core values/types/env/channel/module registry: `src/runtime/base/*.c`
- Type checks and interface compatibility: `src/runtime/typecheck.c`

### Native builtins / stdlib modules

- Native export registration: `src/runtime/modules.c`
- Builtin implementation: `src/runtime/builtins/*.c`
- User docs for API changes: `docs/stdlib.rst`

### CLI or tooling

- CLI command handlers: `src/cli/main/*.c`
- Public command docs: `docs/tooling.rst`

## File Organization Rules

- Use descriptive filenames based on responsibility.
- Avoid generic split names like `part1.c`, `part2.c`.
- Keep wrapper files as composition-only include entrypoints:
  - `src/runtime/base.c`
  - `src/runtime/exec.c`
  - `src/runtime/builtins.c`
  - `src/cli/main.c`

## Testing Expectations

For behavior changes:

1. Add/adjust pass tests in `tests/spec/pass`.
2. Add/adjust fail tests in `tests/spec/fail` when errors are expected.
3. Run `make test` and ensure all suites pass.

For docs-only changes:

- No test change required, but docs must remain consistent with current behavior.

## Docs Expectations

When changing language/runtime features, update relevant docs:

- `docs/language.rst` for syntax/semantics
- `docs/stdlib.rst` for builtins/native modules
- `docs/concurrency.rst` for channel/kostroutine/select changes
- `docs/architecture.rst` for implementation layout changes
- `DEV.rst` for concise developer-facing feature overview

## Commit Quality Checklist

- Build succeeds (`make`)
- Full tests pass (`make test`) for code changes
- New behavior covered by tests
- Docs updated for user-visible changes
- Code placed in the correct module by concern
