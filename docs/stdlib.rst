Builtin and Stdlib Reference
============================

Builtins
--------

- ``print(...)``
- ``input()`` / ``input(prompt)``
- ``int(x)``, ``float(x)``, ``str(x)``
- ``error()`` / ``error(message)``
- ``raiseErr(message)``
- ``length(x)`` for list/map/string
- ``sort(list)``

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
- runtime-native implementation (no ``curl`` shell dependency)
- supports ``http://``, ``https://``, and ``file://`` URLs
- ``requestEx`` returns an object with fields ``status: int``, ``headers: map[str, str]``, ``body: str``

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

- ``libs/re.aja``: typed regex wrapper (compile/search/replaceAll/split)
- ``libs/str.aja``: starts/ends checks, split/join, trim, replaceAll
- ``libs/text.aja``: string utility helpers
- ``libs/list.aja``: clone/reverse/sum/max/find/unique helpers
- ``libs/set.aja``: string-set helpers (add/remove/has/union/intersect/difference)
- ``libs/setutil.aja``: set utilities (fromRange/isSubset/equals/toList)
- ``libs/maputil.aja``: merge/pick/omit/hasAll/getOr helpers
- ``libs/validate.aja``: input/value validation helpers
- ``libs/assert.aja``: assertion helpers for app checks/tests

Optional (install via ``mmk install <name>`` or ``mmk install-stdlib --all``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- ``libs/httpx.aja``: higher-level HTTP client helpers (pure Ajasendiri, wraps ``http``)
- ``libs/fileutil.aja``: text/lines/json helpers on top of ``fs`` + ``json``
- ``libs/env.aja``: typed env readers (string/int/bool + required keys)
- ``libs/log.aja``: minimal logging helpers
- ``libs/retry.aja``: exponential backoff + wait helpers
- ``libs/query.aja``: query string encode/parse/append helpers
- ``libs/randutil.aja``: random token/int list/choice/shuffle helpers
- ``libs/queue.aja``: FIFO queue helpers for ``str`` values
- ``libs/stack.aja``: LIFO stack helpers for ``str`` values
- ``libs/cache.aja``: simple in-memory string cache with optional TTL
- ``libs/kv.aja``: ``key=value`` parse/serialize/load/save helpers
- ``libs/datetime.aja``: UTC ISO parse/format and unix time helpers
