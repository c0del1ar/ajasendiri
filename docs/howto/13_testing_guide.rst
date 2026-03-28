Testing Guide
=============

Run all tests
-------------

.. code-block:: bash

   make test

Run spec harness directly
-------------------------

.. code-block:: bash

   ./tests/run_tests.sh
   ./tests/run_tests.sh tests/spec/pass/your_test.aja

How spec tests are organized
----------------------------

- ``tests/spec/pass``: programs expected to succeed
- ``tests/spec/fail``: programs expected to fail
- ``tests/spec/check/pass``: check-only success cases
- ``tests/spec/check/fail``: check-only failure cases

Files used by harness
---------------------

For a pass test:

- ``name.aja`` (source)
- ``name.aja.out`` (expected stdout)
- optional ``name.aja.stdin`` (stdin input)

For a fail test:

- ``name.aja`` (source)
- ``name.aja.err`` (expected stderr substring)
- optional ``name.aja.code`` (expected exit code)

Example pass test
-----------------

``tests/spec/pass/simple_print.aja``:

.. code-block:: text

   print("ok")

``tests/spec/pass/simple_print.aja.out``:

.. code-block:: text

   ok

Tips
----

- Keep tests small and focused.
- Prefer one behavior per test.
- Add both pass and fail tests for new features.
