Language Reference
==================

Files and blocks
----------------

- Source files use ``.aja``.
- Blocks are indentation-based and start after ``:``.

Variables and typing
--------------------

- First assignment locks a variable type.
- Reassignment must keep the same type.
- Immutable bindings use ``imut``.

Example:

.. code-block:: text

   imut version = "v1"
   age = 21
   age = 22

Containers and comprehensions
-----------------------------

List and map literals:

.. code-block:: text

   nums = [1, 2, 3]
   scores = {"a": 10, "b": 20}

Map helpers:

.. code-block:: text

   value = scores.get("a")
   value = scores.get("missing", 0)
   removed = scores.pop("a")
   removed = scores.pop("missing", 0)

List comprehension:

.. code-block:: text

   squares = [x * x for x in nums]
   tail = [x for x in nums if x > 1]

Map comprehension:

.. code-block:: text

   m = {str(x): x * x for x in nums if x > 1}

Rules:

- Comprehension iterable must be ``list`` or ``map``.
- Comprehension ``if`` must evaluate to ``bool``.
- Map comprehension keys must evaluate to ``str``.

Membership operators
--------------------

Supported operators: ``in`` and ``not in``.

.. code-block:: text

   2 in [1, 2, 3]
   "a" in {"a": 1}
   "ell" in "hello"
   5 not in [1, 2, 3]

Rules:

- Right operand must be ``list``, ``map``, or ``string``.
- For ``map``, membership checks key existence (left side must be ``string``).
- For ``string``, left side must be ``string`` (substring check).
- For ``list``, left type must match the list element type.

Functions
---------

Function declaration:

.. code-block:: text

   fuc add(a: int, b: int) -> int:
       return a + b

Default parameters and named arguments:

.. code-block:: text

   fuc greet(name: str, prefix: str = "Hi") -> str:
       return prefix + ", " + name

   greet("Aja")
   greet(name = "Aja", prefix = "Hello")

Keyword-only parameters:

.. code-block:: text

   fuc greet(name: str, *, prefix: str, suffix: str = "!") -> str:
       return prefix + " " + name + suffix

   greet("Aja", prefix = "Hi")

Lambda-lite:

.. code-block:: text

   inc = fuc(x: int) -> int: x + 1

Multi-return:

.. code-block:: text

   fuc parse(v: str) -> (int, error):
       if v == "":
           return 0, error("empty")
       return int(v), error()

   n, err = parse("42")

User types and methods
----------------------

.. code-block:: text

   type User:
       name: str
       age: int

   fuc (u: User) greet(prefix: str) -> str:
       return prefix + " " + u.name

Interfaces
----------

.. code-block:: text

   interface Speaker:
       speak(prefix: str) -> str

   fuc say(s: Speaker) -> str:
       return s.speak("Hi")

Imports and exports
-------------------

Import all:

.. code-block:: text

   import (
       "mod" as m
   )

Selective import:

.. code-block:: text

   import (
       {a b} from "mod"
   )

Export list:

.. code-block:: text

   export (
       a,
       b
   )
