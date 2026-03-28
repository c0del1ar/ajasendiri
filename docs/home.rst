.. index::
   single: quickstart
   single: install
   single: examples
   single: docs navigation

AjaSendiri Docs
===============

.. rst-class:: hero-subtitle

A strict, Python-like language with Go-style ideas, built in C.

Quick Start
-----------

.. code-block:: bash

   make
   ./ajasendiri examples/ok.aja
   ./ajasendiri

If you only want syntax/type checks:

.. code-block:: bash

   ./ajasendiri check examples/ok.aja

Start Here
----------

.. rst-class:: quickgrid

- :doc:`howto/01_getting_started`
- :doc:`howto/14_build_cli_todo`
- :doc:`examples/index`
- :doc:`reference/index`
- :doc:`language`
- :doc:`stdlib`

What You Get
------------

- Indentation-based syntax and strict runtime typing.
- ``fuc`` functions with typed parameters and returns.
- ``if/elif/else``, ``match/case/default``, loops, ``defer``.
- Lists/maps, comprehensions, and first-class functions.
- Concurrency primitives: ``kostroutine``, channels, ``select``.
- Native modules plus pure ``.aja`` standard libraries.

Project Workflow
----------------

1. Write code in ``.aja`` files.
2. Run ``./ajasendiri check file.aja`` while editing.
3. Run ``./ajasendiri file.aja`` to execute.
4. Use ``./ajasendiri fmt`` before commit.
5. Use ``make test`` for regression.

Helpful Pages
-------------

- :doc:`tooling` for CLI, formatter, LSP, and ``mmk`` package commands.
- :doc:`versioning` for release channels and compatibility policy.
- :doc:`glossary` for core language terms.
- :doc:`architecture` for contributor-level implementation structure.
