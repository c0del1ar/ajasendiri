Tooling and CLI
===============

Build and test
--------------

.. code-block:: bash

   make
   make ci
   make test

Run commands
------------

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

- Activation sets ``AJA_VENV`` and prepends ``.venv/bin`` to ``PATH``.
- With ``AJA_VENV`` active, ``mmk install`` targets ``$AJA_VENV/site-packages``.
- Runtime import lookup checks venv site-packages before global locations.

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

VS Code syntax highlighting
---------------------------

Use ``tools/vscode-ajasendiri``:

1. Open that folder in VS Code.
2. Press ``F5``.
3. Open a ``.aja`` file in the Extension Development Host window.

Dependency tooling (mmk)
------------------------

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

- Default project install target is ``./.aja/site-packages``.
- With active ``AJA_VENV``, install target becomes ``$AJA_VENV/site-packages``.
- ``mmk install <module> --global`` installs optional bundled libs to ``$HOME/.aja/site-packages``.
- ``mmk install-stdlib --global`` installs core pure ``.aja`` libs.
- ``mmk install-stdlib --global --all`` installs core + optional pure libs.
- Registry selectors are resolved and pinned in ``requirements.txt``.
- If ``AJA_SIGN_KEY`` is set, ``mmk pack/publish`` sign packages and ``mmk install/verify`` validate signatures.
- Set ``AJA_REQUIRE_SIGNATURE=1`` to require signed registry dependencies.
