Control Flow
============

if / elif / else
----------------

Conditions must be ``bool``.

.. code-block:: text

   is_ready = true

   if is_ready:
       print("go")
   elif false:
       print("wait")
   else:
       print("stop")

match / case / default
----------------------

.. code-block:: text

   code = 2

   match code:
       case 1:
           print("one")
       case 2:
           print("two")
       default:
           print("other")

while loop
----------

.. code-block:: text

   i = 0
   while i < 3 do:
       print(i)
       i++

do-while loop
-------------

.. code-block:: text

   i = 0
   do:
       print(i)
       i++
   while i < 3

break and continue
------------------

.. code-block:: text

   for n in range(10):
       if n == 3:
           continue
       if n == 7:
           break
       print(n)
