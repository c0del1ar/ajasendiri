Builtins and Stdlib
===================

Builtins
--------

- ``print(...)``
- ``input()`` / ``input(prompt)``
- ``int(x)``, ``float(x)``, ``str(x)``
- ``error()`` / ``error(message)``
- ``raiseErr(message)``
- ``length(x)`` for list/map/string
- ``sort(list)``
- ``memStats()`` allocation counters
- ``memCollect()`` memory collector hook (currently stats snapshot)

Container methods
-----------------

list
^^^^

- ``append(value)``
- ``extend(list)``
- ``insert(index, value)``
- ``pop()``
- ``has(value)``

map
^^^

- ``has(key)``
- ``delete(key)``
- ``keys()``
- ``values()``
- ``get(key[, fallback])``
- ``pop(key[, fallback])``

Native stdlib modules
---------------------

math
^^^^

- ``pi``
- ``abs``, ``sqrt``, ``min``, ``max``

time
^^^^

- ``now_unix()``
- ``now_ms()``
- ``sleep(ms)``
- ``after(ms)`` -> ``chan`` (single timeout event)

json
^^^^

- ``encode(value)``
- ``decode(text)``

fs
^^

- ``read(path)``
- ``write(path, text)``
- ``append(path, text)``
- ``exists(path)``
- ``mkdir(path)``
- ``remove(path)``

http
^^^^

- ``get(url)``
- ``post(url, body)``
- ``put(url, body)``
- ``delete(url)``
- ``request(method, url[, body])``
- ``requestEx(method, url[, body])`` -> ``HttpResponse``

Notes:

- Implemented in runtime (no shell ``curl`` dependency).
- Supports ``http://``, ``https://``, and ``file://`` URLs.
- ``requestEx`` returns ``status``, ``headers``, and ``body``.

rand
^^^^

- ``seed(int)``
- ``int(min, max)``
- ``float()``

os
^^

- ``cwd()``
- ``chdir(path)``
- ``getenv(key)``
- ``setenv(key, value)``

path
^^^^

- ``join(a, b)``
- ``basename(path)``
- ``dirname(path)``
- ``ext(path)``

Pure Ajasendiri libs
--------------------

Core (installed by default with ``mmk install-stdlib``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- ``libs/re.aja``: typed regex wrapper
- ``libs/str.aja``: common string helpers
- ``libs/text.aja``: text utility helpers
- ``libs/list.aja``: list helpers
- ``libs/set.aja``: string set helpers
- ``libs/setutil.aja``: set utilities
- ``libs/maputil.aja``: map utilities
- ``libs/validate.aja``: value validation helpers
- ``libs/assert.aja``: lightweight assertion helpers
- ``libs/context.aja``: timeout/cancel helpers over channels
- ``libs/pathlib.aja``: typed path/file helper wrappers

Optional (install via ``mmk install <name>`` or ``mmk install-stdlib --all``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- ``libs/httpx.aja``
- ``libs/collections.aja``
- ``libs/fileutil.aja``
- ``libs/env.aja``
- ``libs/log.aja``
- ``libs/retry.aja``
- ``libs/query.aja``
- ``libs/randutil.aja``
- ``libs/queue.aja``
- ``libs/stack.aja``
- ``libs/cache.aja``
- ``libs/kv.aja``
- ``libs/datetime.aja``
