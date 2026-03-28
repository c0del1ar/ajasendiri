Types and Variables
===================

Core types
----------

Ajasendiri uses strict runtime typing.

Common types:

- ``int``
- ``float``
- ``str`` (or ``string``)
- ``bool``
- ``error``
- ``void`` (for no return value)

Type locking
------------

A variable type is locked on first assignment.

.. code-block:: text

   age = 21      # int
   age = 22      # valid
   # age = "22" # invalid: type mismatch

Immutable values with ``imut``
------------------------------

Use ``imut`` for values that should not change.

.. code-block:: text

   imut app_name = "ajasendiri"
   # app_name = "other"  # invalid

Casts
-----

.. code-block:: text

   n = int("42")
   f = float("3.14")
   s = str(123)

Input + cast pattern
--------------------

.. code-block:: text

   age = int(input("Age: "))
   score = float(input("Score: "))
   name = str(input("Name: "))
