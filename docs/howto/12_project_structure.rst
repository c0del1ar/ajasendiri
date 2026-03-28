Project Structure
=================

Recommended layout
------------------

.. code-block:: text

   your-project/
     src/
       main.aja
       service/
         user.aja
     libs/
     tests/
     aja.toml
     requirements.txt
     requirements.lock

What each part is for
---------------------

- ``src/``: your application modules
- ``libs/``: local reusable modules before publishing
- ``tests/``: test programs and fixtures
- ``requirements.txt``: dependency declarations
- ``requirements.lock``: pinned versions + hashes

Naming and import style
-----------------------

- Use lowercase module filenames.
- Keep module names stable; treat them as public API once reused.
- Prefer selective import when you only need a few symbols.

.. code-block:: text

   import (
       {parse_user validate_user} from "service/user"
   )

Venv and site-packages
----------------------

Use a virtual environment for isolated dependencies:

.. code-block:: bash

   ./ajasendiri venv .venv
   . .venv/bin/activate
   ajasendiri mmk install

With active venv, modules resolve from ``$AJA_VENV/site-packages`` first.
