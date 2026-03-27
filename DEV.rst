Ajasendiri Developer Overview
=============================

Overview
--------

Ajasendiri is an interpreted language with Python-like block syntax and strict runtime typing.
It is implemented in C and split into lexer, parser, runtime, and CLI modules.

Language Features (Current)
---------------------------

Syntax and control flow:
- ``fuc`` function declaration
- indentation-based blocks (``:`` + indented body)
- ``if / elif / else``
- ``match / case / default``
- ``for`` with ``range`` and iterable forms
- ``while <cond> do:`` and ``do: ... while <cond>``
- ``break`` / ``continue`` (including labels)
- ``defer``
- ``n++`` increment sugar
- list/map comprehensions (single ``for`` + optional ``if``)
- membership operators: ``in`` and ``not in``

Types and typing rules:
- strict typing: first assignment locks variable type
- core types: ``int``, ``float``, ``str``/``string``, ``bool``, ``error``, ``void``
- containers: ``list``, ``map``, ``chan``
- generic annotations: ``list[T]``, ``map[str, V]``, ``chan[T]``
- user types with ``type`` and receiver methods
- Go-style ``interface`` with implicit implementation

Functions:
- first-class functions
- typed function signatures: ``func(...) -> ...``
- lambda-lite: ``fuc(...) -> T: expr``
- multi-return signatures and assignments: ``-> (T1, T2)``, ``a, b = fn()``
- default parameters and named arguments for user functions
- keyword-only user parameters via ``*`` marker in parameter lists

Modules:
- ``import (...)`` and ``export (...)``
- import aliases for import-all entries
- selective import syntax: ``{name ...} from "module"``
- module resolution: current dir, ``$AJA_VENV/site-packages`` (if set), parent ``.aja/site-packages``, global ``$HOME/.aja/site-packages``, then ``AJA_PATH``

Runtime Builtins
----------------

General builtins:
- ``print``
- ``input``
- ``int``, ``float``, ``str`` casts
- ``error`` and ``raiseErr``
- ``length`` and ``sort``

Concurrency builtins:
- ``kostroutine``
- ``waitAll``
- ``chan``, ``send``, ``recv``, ``close``
- ``trySend``, ``tryRecv``
- ``select`` + ``timeout(ms)``

Native stdlib modules:
- ``math``
- ``time``
- ``json``
- ``fs``
- ``http``
- ``rand``
- ``os``
- ``path``
- selected native module calls support named arguments (``fs``, ``http``, ``time.sleep``, ``rand.seed/int``)

Pure Ajasendiri libs:
- core (default install): ``re``, ``str``, ``text``, ``list``, ``set``, ``setutil``, ``maputil``, ``validate``, ``assert``
- optional: ``httpx``, ``fileutil``, ``env``, ``log``, ``retry``, ``query``, ``randutil``, ``queue``, ``stack``, ``cache``, ``kv``, ``datetime``

Project Layout
--------------

- ``include/core/``: token, AST, and public API headers
- ``include/{lexer,parser,runtime,cli}/``: module-level header entry points
- ``src/lexer/``: lexer implementation
- ``src/parser/``: parser implementation and parser submodules
- ``src/runtime/``: runtime implementation and runtime submodules
- ``src/cli/``: command-line interface
- ``libs/``: pure Ajasendiri libraries (for example ``httpx.aja``, ``text.aja``, ``str.aja``, ``list.aja``, ``set.aja``, ``setutil.aja``, ``maputil.aja``, ``fileutil.aja``, ``env.aja``, ``log.aja``, ``retry.aja``, ``query.aja``, ``randutil.aja``, ``queue.aja``, ``stack.aja``, ``cache.aja``, ``kv.aja``, ``validate.aja``, ``assert.aja``, ``datetime.aja``, ``re.aja``)
- ``tests/spec/``: language regression tests

Behavior Notes
--------------

- Conditions for ``if/while/do-while`` are bool-only.
- ``and/or`` short-circuit and require bool operands.
- ``match`` requires strict comparable types (numeric mix supported for int/float).
- ``raiseErr`` is fatal and still runs deferred calls in the current function frame.
- ``http`` module uses in-runtime networking for ``http://`` and ``https://`` and local reads for ``file://`` URLs.
- ``http.requestEx(method, url[, body])`` returns ``HttpResponse`` object with ``status``, ``headers``, and ``body``.

See ``docs/*.rst`` for detailed semantics and examples.
