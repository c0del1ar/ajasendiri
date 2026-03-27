Tooling and CLI
===============

Build and test
--------------

.. code-block:: bash

   make
   make test

Runner commands
---------------

.. code-block:: bash

   ./ajasendiri
   ./ajasendiri path/to/file.aja
   ./ajasendiri check path/to/file.aja
   ./ajasendiri test
   ./ajasendiri venv .venv

Virtual environment
-------------------

.. code-block:: bash

   ./ajasendiri venv .venv
   . .venv/bin/activate
   ajasendiri mmk install

Notes:

- Activating sets ``AJA_VENV`` and prepends ``.venv/bin`` to ``PATH``.
- With active ``AJA_VENV``, ``mmk install`` installs project dependencies into ``$AJA_VENV/site-packages``.
- Runtime module import checks ``$AJA_VENV/site-packages`` before global paths.

Formatter
---------

.. code-block:: bash

   ./ajasendiri fmt path/to/file.aja
   ./ajasendiri fmt --check path/to/dir
   ./ajasendiri fmt --stdin < file.aja

REPL and debugger
-----------------

.. code-block:: bash

   ./ajasendiri repl
   ./ajasendiri debug path/to/file.aja

LSP
---

.. code-block:: bash

   python3 tools/ajasendiri_lsp.py

Syntax Highlighting (VS Code)
-----------------------------

Syntax highlighting for ``.aja`` is available in:

``tools/vscode-ajasendiri``

Use locally:

1. Open ``tools/vscode-ajasendiri`` in VS Code.
2. Press ``F5``.
3. Open a ``.aja`` file in the Extension Development Host window.

Dependencies (mmk)
------------------

.. code-block:: bash

   ./ajasendiri mmk init
   ./ajasendiri mmk add <module> --version <x.y.z|^x.y|~x.y|latest|*>
   ./ajasendiri mmk install
   ./ajasendiri mmk install httpx --global
   ./ajasendiri mmk install-stdlib --global
   ./ajasendiri mmk install-stdlib --global --all
   ./ajasendiri mmk search [query]
   ./ajasendiri mmk info <module> [--version <selector>]
   ./ajasendiri mmk verify

Notes:

- ``mmk install`` installs project dependencies into ``./.aja/site-packages``.
- If ``AJA_VENV`` is active, ``mmk install`` installs into ``$AJA_VENV/site-packages``.
- ``mmk install <module> --global`` installs bundled optional libs by name (for example ``httpx``) into ``$HOME/.aja/site-packages``.
- ``mmk install-stdlib --global`` installs bundled **core** pure ``.aja`` stdlib libs.
- ``mmk install-stdlib --global --all`` installs core + optional bundled pure libs.
- With active ``AJA_VENV``, ``mmk install <module>`` and ``mmk install-stdlib`` default to ``$AJA_VENV/site-packages``.
- For registry dependencies, version selectors are resolved and pinned in ``requirements.txt`` (for example ``^1.2`` or ``latest``).
- If ``AJA_SIGN_KEY`` is set, ``mmk pack/publish`` sign packages and ``mmk install/verify`` validate signatures for registry dependencies.
- Set ``AJA_REQUIRE_SIGNATURE=1`` to require signatures on registry dependencies.
