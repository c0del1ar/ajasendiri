Concurrency Reference
=====================

kostroutine
-----------

- ``kostroutine fn(args...)`` schedules a function on runtime worker threads.
- ``waitAll()`` blocks until all scheduled routines finish.

Channels
--------

Create channels:

- ``chan()`` for unbounded channels
- ``chan(n)`` for buffered channels (``n`` must be ``> 0``)

Channel operations:

- ``send(ch, value)``
- ``recv(ch)``
- ``close(ch)``
- ``trySend(ch, value)`` returns ``bool``
- ``tryRecv(ch, fallback)`` returns received value or fallback

select
------

Supported case forms:

- ``case recv(ch) as v:``
- ``case send(ch, v):``
- ``case timeout(ms):``
- ``default:``

Current behavior:

- deterministic selection (first ready case in source order)
- ``timeout(ms)`` requires non-negative ``int`` milliseconds
