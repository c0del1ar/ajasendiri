Concurrency Reference
=====================

kostroutine
-----------

- ``kostroutine fn(args...)`` schedules a function on runtime worker threads.
- ``waitAll()`` joins pending scheduled routines.

Channels
--------

Creation:
- ``chan()`` unbounded
- ``chan(n)`` buffered, requires ``n > 0``

Operations:
- ``send(ch, value)``
- ``recv(ch)``
- ``close(ch)``
- ``trySend(ch, value)`` returns bool
- ``tryRecv(ch, fallback)`` returns received value or fallback

select
------

Supported cases:
- ``case recv(ch) as v:``
- ``case send(ch, v):``
- ``case timeout(ms):``
- ``default:``

Current behavior:
- deterministic case choice (first ready case in source order)
- timeout requires non-negative ``int`` milliseconds
