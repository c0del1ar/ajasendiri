Architecture Guide
==================

Overview
--------

Ajasendiri is a C interpreter with four main layers:

1. Lexer: source text -> token stream
2. Parser: token stream -> AST
3. Runtime: executes AST and enforces runtime typing
4. CLI: entrypoints for run/check/test/fmt/repl/debug/mmk

Execution flow
--------------

Typical execution path:

1. ``src/cli/main.c`` reads command and source.
2. ``src/lexer/lexer.c`` tokenizes the source.
3. ``src/parser/parser.c`` builds AST (via parser submodules).
4. ``src/runtime/runtime.c`` executes using runtime submodules.
5. Errors return to CLI with context.

Source map
----------

Core headers
^^^^^^^^^^^^

- ``include/core/token.h``
- ``include/core/ast.h``
- ``include/core/api.h``
- ``include/ajasendiri.h``

Lexer
^^^^^

- ``src/lexer/lexer.c``

Parser
^^^^^^

- ``src/parser/parser.c`` (entrypoint)
- ``src/parser/base.c`` (shared parser utilities)
- ``src/parser/expr.c`` (expression grammar)
- ``src/parser/stmt.c`` (statement grammar)
- ``src/parser/decl.c`` (declarations, import/export)

Runtime composition wrappers
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- ``src/runtime/runtime.c``
- ``src/runtime/base.c``
- ``src/runtime/exec.c``
- ``src/runtime/builtins.c``

Runtime core modules
^^^^^^^^^^^^^^^^^^^^

- ``src/runtime/base/core_types_and_values.c``
- ``src/runtime/base/containers_env_debug.c``
- ``src/runtime/base/runtime_core_and_modules.c``
- ``src/runtime/modules.c``
- ``src/runtime/typecheck.c``

Runtime execution modules
^^^^^^^^^^^^^^^^^^^^^^^^^

- ``src/runtime/exec/eval_expr.c``
- ``src/runtime/exec/exec_stmt_and_runtime.c``
- ``src/runtime/exec/public_api.c``

Runtime builtins modules
^^^^^^^^^^^^^^^^^^^^^^^^

- ``src/runtime/builtins/value_cast_json.c``
- ``src/runtime/builtins/functions_kostroutine_math_time.c``
- ``src/runtime/builtins/native_json_fs_path_regex_http_parse.c``
- ``src/runtime/builtins/http_transport_and_rand.c``
- ``src/runtime/builtins/native_stdlib_calls.c``
- ``src/runtime/builtins/call_dispatch.c``

CLI modules
^^^^^^^^^^^

- ``src/cli/main/shared_utils_and_dep_parse.c``
- ``src/cli/main/project_io_and_registry.c``
- ``src/cli/main/repl_debug_format.c``
- ``src/cli/main/mmk_core_commands.c``
- ``src/cli/main/mmk_advanced_and_main.c``

Where to put new code
---------------------

- New syntax/tokens: lexer + parser + core headers.
- New runtime semantics: ``src/runtime/exec`` and ``src/runtime/base``.
- New builtin/native module behavior: ``src/runtime/builtins`` and ``src/runtime/modules.c``.
- New CLI/tooling behavior: ``src/cli/main``.

Contributor guidelines
----------------------

- Use descriptive filenames by responsibility.
- Keep composition wrappers (``base.c``, ``exec.c``, ``builtins.c``, ``main.c``) focused on includes/wiring.
- Add or update tests in ``tests/spec/pass`` and ``tests/spec/fail``.
- Update docs when behavior changes.

Builtin extension checklist
---------------------------

1. Register/export symbol in ``src/runtime/modules.c``.
2. Implement behavior in the matching runtime builtin module.
3. Add pass/fail tests under ``tests/spec``.
4. Update ``docs/stdlib.rst`` and ``DEV.rst``.
