Packages with mmk
=================

Initialize project metadata
---------------------------

.. code-block:: bash

   ./ajasendiri mmk init

Add dependency
--------------

Local file source:

.. code-block:: bash

   ./ajasendiri mmk add ./libs/mydep.aja --version 1.0.0

Registry source:

.. code-block:: bash

   ./ajasendiri mmk add mydep --version ^1.2

Install dependencies
--------------------

.. code-block:: bash

   ./ajasendiri mmk install

Verify lock and installed files
-------------------------------

.. code-block:: bash

   ./ajasendiri mmk verify

Explore local registry
----------------------

.. code-block:: bash

   ./ajasendiri mmk search
   ./ajasendiri mmk info mydep --version latest

Create virtual environment (recommended)
----------------------------------------

.. code-block:: bash

   ./ajasendiri venv .venv
   . .venv/bin/activate
   ajasendiri mmk install
