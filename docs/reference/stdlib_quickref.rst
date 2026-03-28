Stdlib Quick Reference
======================

Builtins
--------

- ``print``
- ``input``
- ``int`` / ``float`` / ``str``
- ``error``
- ``raiseErr``
- ``length``
- ``sort``
- ``memStats``
- ``memCollect``

Native modules
--------------

- ``math``
- ``time``
- ``json``
- ``fs``
- ``http``
- ``rand``
- ``os``
- ``path``

Concurrency
-----------

- ``kostroutine``
- ``waitAll``
- ``chan`` / ``send`` / ``recv`` / ``close``
- ``trySend`` / ``tryRecv``
- ``select`` + ``timeout(ms)``

Pure `.aja` libs (bundled)
--------------------------

Core:

- ``re``, ``str``, ``text``, ``list``, ``set``, ``setutil``, ``maputil``, ``validate``, ``assert``, ``context``, ``pathlib``

Optional:

- ``httpx``, ``collections``, ``fileutil``, ``env``, ``log``, ``retry``, ``query``, ``randutil``, ``queue``, ``stack``, ``cache``, ``kv``, ``datetime``
