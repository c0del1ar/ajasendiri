Ajasendiri Developer Overview
=============================

Purpose
-------

This document is a quick contributor map: what the language supports today,
how the codebase is organized, and where to place new work.

Current language scope
----------------------

Syntax and flow control
^^^^^^^^^^^^^^^^^^^^^^^

- ``fuc`` function declarations
- indentation-based blocks (``:`` + indented body)
- ``if / elif / else``
- ``match / case / default``
- ``for`` loops (range and iterable forms)
- ``while <cond> do:`` and ``do: ... while <cond>``
- ``break`` / ``continue`` with optional labels
- ``defer``
- ``n++`` increment sugar
- list/map comprehensions (single ``for``, optional ``if``)
- membership operators: ``in`` and ``not in``

Types and typing
^^^^^^^^^^^^^^^^

- strict typing: first assignment locks variable type
- core types: ``int``, ``float``, ``str``/``string``, ``bool``, ``error``, ``void``
- containers: ``list``, ``map``, ``chan``
- generic annotations: ``list[T]``, ``map[str, V]``, ``chan[T]``
- custom object types via ``type`` and receiver methods
- Go-style ``interface`` with implicit implementation

Functions
^^^^^^^^^

- first-class functions
- typed signatures
- lambda-lite syntax
- multi-return signatures and assignment
- default parameters and named arguments
- keyword-only parameters via ``*``

Modules
^^^^^^^

- ``import (...)`` and ``export (...)``
- import alias support for import-all entries
- selective import syntax: ``{name ...} from "module"``
- resolution order: current dir -> ``$AJA_VENV/site-packages`` -> parent ``.aja/site-packages`` -> ``$HOME/.aja/site-packages`` -> ``AJA_PATH``

Builtins and stdlib snapshot
----------------------------

General builtins
^^^^^^^^^^^^^^^^

- ``print``
- ``input``
- ``int`` / ``float`` / ``str`` casts
- ``error`` and ``raiseErr``
- ``length`` and ``sort``
- ``memStats`` and ``memCollect``

Concurrency builtins
^^^^^^^^^^^^^^^^^^^^

- ``kostroutine``
- ``waitAll``
- ``chan``, ``send``, ``recv``, ``close``
- ``trySend``, ``tryRecv``
- ``select`` + ``timeout(ms)``
- ``time.after(ms)`` timeout channel helper

Native stdlib modules
^^^^^^^^^^^^^^^^^^^^^

- ``math``, ``time``, ``json``, ``fs``, ``http``, ``rand``, ``os``, ``path``

Pure Ajasendiri libs
^^^^^^^^^^^^^^^^^^^^

- core: ``re``, ``str``, ``text``, ``list``, ``set``, ``setutil``, ``maputil``, ``validate``, ``assert``
- core: ``context``, ``pathlib``
- optional: ``httpx``, ``collections``, ``fileutil``, ``env``, ``log``, ``retry``, ``query``, ``randutil``, ``queue``, ``stack``, ``cache``, ``kv``, ``datetime``

Project layout
--------------

- ``include/core/``: token/AST/public API headers
- ``include/{lexer,parser,runtime,cli}/``: module-facing headers
- ``src/lexer/``: tokenizer
- ``src/parser/``: parser modules
- ``src/runtime/``: runtime modules
- ``src/cli/``: CLI and package tooling
- ``libs/``: pure Aja stdlib modules
- ``tests/spec/``: language regression tests

Behavior notes
--------------

- ``if``/``while``/``do-while`` conditions must be bool.
- ``and``/``or`` short-circuit and require bool operands.
- ``match`` uses strict comparable types (with int/float numeric compatibility).
- ``raiseErr`` is fatal but still runs deferred calls in the current function frame.
- ``http`` is runtime-native (supports ``http://``, ``https://``, and ``file://``).
