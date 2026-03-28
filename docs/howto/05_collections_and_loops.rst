Collections and Loops
=====================

Lists
-----

.. code-block:: text

   nums = [1, 2, 3]
   nums.append(4)
   print(length(nums))

Useful list methods:

- ``append(value)``
- ``extend(list)``
- ``insert(index, value)``
- ``pop()``
- ``has(value)``

Maps
----

.. code-block:: text

   scores = {"a": 10, "b": 20}
   print(scores.get("a"))
   print(scores.has("b"))

Useful map methods:

- ``has(key)``
- ``delete(key)``
- ``keys()``
- ``values()``
- ``get(key[, fallback])``
- ``pop(key[, fallback])``

for with range
--------------

.. code-block:: text

   for i in range(5):
       print(i)

for over list
-------------

.. code-block:: text

   names = ["aja", "sendiri"]
   for name in names:
       print(name)

Comprehensions
--------------

.. code-block:: text

   nums = [1, 2, 3, 4]
   squares = [x * x for x in nums]
   even_map = {str(x): x for x in nums if x % 2 == 0}
