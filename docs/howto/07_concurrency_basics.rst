Concurrency Basics
==================

Run work in background
----------------------

Use ``kostroutine`` to schedule a function.

.. code-block:: text

   fuc worker(name: str) -> void:
       print("run " + name)

   kostroutine worker("a")
   kostroutine worker("b")
   waitAll()

Channels
--------

Send and receive values safely between routines.

.. code-block:: text

   ch = chan()

   fuc producer() -> void:
       send(ch, "hello")

   kostroutine producer()
   msg = recv(ch)
   print(msg)
   waitAll()

select
------

Use ``select`` when waiting on multiple channel operations.

.. code-block:: text

   ch = chan()

   select:
       case recv(ch) as v:
           print(v)
       case timeout(1000):
           print("timeout")
       default:
           print("no event")
