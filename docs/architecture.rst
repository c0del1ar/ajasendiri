Architecture Guide
==================

Overview
--------

Ajasendiri is a C interpreter with four major layers:

1. **Lexer**: source text -> token stream
2. **Parser**: token stream -> AST (Program, Stmt, Expr, TypeRef)
3. **Runtime**: AST execution + type checks + native module dispatch
4. **CLI**: commands (`run`, `check`, `test`, `fmt`, `repl`, `debug`, `mmk`)

Execution flow
--------------

Normal execution path:

1. ``src/cli/main.c`` accepts command and reads source.
2. ``src/lexer/lexer.c`` tokenizes the source.
3. ``src/parser/parser.c`` builds AST via parser submodules.
4. ``src/runtime/runtime.c`` executes by composing runtime submodules.
5. Errors bubble back to CLI with context.

Source map
----------

Core headers:

- ``include/core/token.h``: token model
- ``include/core/ast.h``: AST and type system model
- ``include/core/api.h``: public compile/run API
- ``include/ajasendiri.h``: umbrella header

Lexer:

- ``src/lexer/lexer.c``: tokenizer and indentation token handling

Parser:

- ``src/parser/parser.c``: parser entrypoint
- ``src/parser/base.c``: parser utilities, type parsing helpers
- ``src/parser/expr.c``: expression grammar and precedence
- ``src/parser/stmt.c``: statements and block parsing
- ``src/parser/decl.c``: declarations (`fuc`, `type`, `interface`, import/export)

Runtime entry wrappers:

- ``src/runtime/runtime.c``: runtime composition root
- ``src/runtime/base.c``: includes runtime core modules
- ``src/runtime/exec.c``: includes execution modules
- ``src/runtime/builtins.c``: includes builtin modules

Runtime core modules:

- ``src/runtime/base/core_types_and_values.c``: `Value`, type names, value constructors, type-shape checks
- ``src/runtime/base/containers_env_debug.c``: list/map/channel internals, env ops, debugger helpers
- ``src/runtime/base/runtime_core_and_modules.c``: errors, file/path helpers, module/binding registries, frames
- ``src/runtime/modules.c``: import resolution + native module loading/exports
- ``src/runtime/typecheck.c``: runtime type compatibility and interface checks

Runtime execution modules:

- ``src/runtime/exec/eval_expr.c``: expression evaluation
- ``src/runtime/exec/exec_stmt_and_runtime.c``: statement execution + runtime orchestration
- ``src/runtime/exec/public_api.c``: exported C API (`run_program`, `run_program_debug`)

Runtime builtin modules:

- ``src/runtime/builtins/value_cast_json.c``: print/input/casts/json parsing/encoding helpers
- ``src/runtime/builtins/functions_kostroutine_math_time.c``: function invocation, defer, kostroutine, math/time builtins
- ``src/runtime/builtins/native_json_fs_path_regex_http_parse.c``: json/fs/path/re helpers + HTTP parsing helpers
- ``src/runtime/builtins/http_transport_and_rand.c``: HTTP transport (http/https/file) + rand core
- ``src/runtime/builtins/native_stdlib_calls.c``: native stdlib call handlers (`fs/os/path/http/rand/re`)
- ``src/runtime/builtins/call_dispatch.c``: call dispatcher, list/map methods, builtin function routing

CLI modules:

- ``src/cli/main/shared_utils_and_dep_parse.c``: shared text/path/hash/dep parsing helpers
- ``src/cli/main/project_io_and_registry.c``: project IO, registry metadata helpers
- ``src/cli/main/repl_debug_format.c``: repl/debug/fmt command handlers
- ``src/cli/main/mmk_core_commands.c``: `mmk init/add/install` core
- ``src/cli/main/mmk_advanced_and_main.c``: `mmk pack/publish/verify/freeze` + `main()`

Where to add code
-----------------

Add by concern, not by file size:

- New syntax/token: ``src/lexer/lexer.c``, parser files, and ``include/core/{token,ast}.h``
- New statement/expr semantics: ``src/runtime/exec/*.c``
- New runtime data/type behavior: ``src/runtime/base/*.c`` or ``src/runtime/typecheck.c``
- New builtin/native module behavior: ``src/runtime/builtins/*.c`` and ``src/runtime/modules.c``
- New CLI commands/tooling: ``src/cli/main/*.c``

Contribution rules (structure)
------------------------------

- Prefer descriptive filenames by responsibility (avoid generic names like ``part*.c``).
- Keep wrapper files (``base.c``, ``exec.c``, ``builtins.c``, ``main.c``) as composition includes only.
- Place tests under:
  - ``tests/spec/pass`` for valid behavior
  - ``tests/spec/fail`` for expected runtime/parse/type failures
- Update docs when behavior changes:
  - user-facing API: ``docs/stdlib.rst`` / ``docs/language.rst``
  - implementation notes: ``docs/architecture.rst`` / ``DEV.rst``

Builtin extension checklist
---------------------------

1. Export/register symbol in ``src/runtime/modules.c``.
2. Implement behavior in the matching runtime builtin module.
3. Add pass/fail tests in ``tests/spec``.
4. Update docs in ``docs/stdlib.rst`` and ``DEV.rst`` when needed.
