Stdlib Recipes
==============

Read and write a text file
--------------------------

.. code-block:: text

   fs.write("notes.txt", "hello")
   text = fs.read("notes.txt")
   print(text)

Work with JSON
--------------

.. code-block:: text

   payload = {"name": "aja", "score": 10}
   raw = json.encode(payload)
   parsed = json.decode(raw)
   print(parsed["name"])

HTTP GET
--------

.. code-block:: text

   body = http.get("https://example.com")
   print(length(body))

HTTP request with status/header/body
------------------------------------

.. code-block:: text

   res = http.requestEx("GET", "https://example.com")
   print(res.status)
   print(res.body)

Environment variable with fallback
----------------------------------

.. code-block:: text

   mode = os.getenv("APP_MODE")
   if mode == "":
       mode = "dev"

   print(mode)

Path operations
---------------

.. code-block:: text

   full = path.join("logs", "app.txt")
   print(path.dirname(full))
   print(path.basename(full))

Random number in range
----------------------

.. code-block:: text

   rand.seed(123)
   n = rand.int(1, 10)
   print(n)
