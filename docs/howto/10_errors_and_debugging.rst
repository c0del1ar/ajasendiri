Errors and Debugging
====================

Error values
------------

Use ``error()`` for an empty error value and ``error("message")`` for a populated one.

.. code-block:: text

   err = error()
   if err != error():
       print("has error")

Raise a fatal runtime error
---------------------------

Use ``raiseErr(message)`` when execution must stop immediately.

.. code-block:: text

   fuc require_non_empty(s: str) -> void:
       if s == "":
           raiseErr("input cannot be empty")

``raiseErr`` is fatal and reports a stack trace with function context.

Common error cases
------------------

Type mismatch after variable lock:

.. code-block:: text

   value = 10
   # value = "ten"  # runtime type error

Non-bool condition:

.. code-block:: text

   # if 1:  # invalid, condition must be bool
   #     print("x")

Wrong function argument type:

.. code-block:: text

   fuc add(a: int, b: int) -> int:
       return a + b

   # add("1", 2)  # runtime type error

Debug workflow
--------------

1. Run with type-check first:

.. code-block:: bash

   ./ajasendiri check your_file.aja

2. Run in debugger mode when needed:

.. code-block:: bash

   ./ajasendiri debug your_file.aja

3. Add small prints around suspected values/types.
4. Reduce failing code to a minimal reproducible snippet.
