Modules and Imports
===================

Create a module
---------------

File: ``mathx.aja``

.. code-block:: text

   fuc add(a: int, b: int) -> int:
       return a + b

   fuc sub(a: int, b: int) -> int:
       return a - b

   export (
       add,
       sub
   )

Import all from module
----------------------

File: ``main.aja``

.. code-block:: text

   import (
       "mathx"
   )

   print(mathx.add(2, 3))

Selective import
----------------

.. code-block:: text

   import (
       {add} from "mathx"
   )

   print(add(10, 5))

Alias import
------------

.. code-block:: text

   import (
       "mathx" as m
   )

   print(m.sub(7, 2))

Resolution notes
----------------

Ajasendiri checks module locations in this order:

1. current file directory
2. ``$AJA_VENV/site-packages`` (if active)
3. parent ``.aja/site-packages``
4. global ``$HOME/.aja/site-packages``
5. directories from ``AJA_PATH``
