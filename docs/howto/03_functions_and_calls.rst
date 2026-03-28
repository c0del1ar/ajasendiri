Functions and Calls
===================

Declare a function
------------------

Use ``fuc`` and type annotations.

.. code-block:: text

   fuc add(a: int, b: int) -> int:
       return a + b

Call a function
---------------

.. code-block:: text

   total = add(3, 4)
   print(total)

Default parameters
------------------

.. code-block:: text

   fuc greet(name: str, prefix: str = "Hi") -> str:
       return prefix + ", " + name

   print(greet("Aja"))
   print(greet("Aja", "Hello"))

Named arguments
---------------

.. code-block:: text

   print(greet(name = "Aja", prefix = "Welcome"))

Keyword-only parameters
-----------------------

.. code-block:: text

   fuc tag(text: str, *, left: str, right: str) -> str:
       return left + text + right

   print(tag("x", left = "[", right = "]"))

Lambda-lite
-----------

.. code-block:: text

   inc = fuc(x: int) -> int: x + 1
   print(inc(5))

Multi-return
------------

.. code-block:: text

   fuc parse_num(s: str) -> (int, error):
       if s == "":
           return 0, error("empty")
       return int(s), error()

   n, err = parse_num("12")
