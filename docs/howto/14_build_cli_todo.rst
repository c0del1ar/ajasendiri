Build a CLI Todo in 30 Minutes
==============================

.. index::
   single: tutorial; todo app
   single: mini project

Goal
----

Build a small command-line todo app with:

- add task
- list tasks
- mark done
- exit loop cleanly

This tutorial uses only core language features: ``fuc``, maps, loops, ``if/elif/else``, ``input``, and strict typing.

Step 1: Program skeleton
------------------------

.. code-block:: text

   fuc menu() -> void:
       print("1) add")
       print("2) list")
       print("3) done")
       print("4) exit")

   tasks = {}
   done = {}
   count = 0
   running = True

   while running do:
       menu()
       cmd = input("> ")

Step 2: Add tasks
-----------------

Store tasks by string key, using ``count`` as id.

.. code-block:: text

   if cmd == "1":
       text = input("task: ")
       key = str(count)
       tasks[key] = text
       done[key] = False
       count++

Step 3: List tasks
------------------

.. code-block:: text

   elif cmd == "2":
       if count == 0:
           print("(empty)")
       else:
           for i in range(count):
               key = str(i)
               mark = "[ ]"
               if done[key]:
                   mark = "[x]"
               print(mark + " " + key + " " + tasks[key])

Step 4: Mark task done
----------------------

Use ``in`` with map keys for safe lookup.

.. code-block:: text

   elif cmd == "3":
       idtxt = input("id: ")
       if idtxt in tasks:
           done[idtxt] = True
       else:
           print("id not found")

Step 5: Exit and unknown command
--------------------------------

.. code-block:: text

   elif cmd == "4":
       running = False
   else:
       print("unknown command")

Full Program
------------

.. code-block:: text

   fuc menu() -> void:
       print("1) add")
       print("2) list")
       print("3) done")
       print("4) exit")

   tasks = {}
   done = {}
   count = 0
   running = True

   while running do:
       menu()
       cmd = input("> ")

       if cmd == "1":
           text = input("task: ")
           key = str(count)
           tasks[key] = text
           done[key] = False
           count++
       elif cmd == "2":
           if count == 0:
               print("(empty)")
           else:
               for i in range(count):
                   key = str(i)
                   mark = "[ ]"
                   if done[key]:
                       mark = "[x]"
                   print(mark + " " + key + " " + tasks[key])
       elif cmd == "3":
           idtxt = input("id: ")
           if idtxt in tasks:
               done[idtxt] = True
           else:
               print("id not found")
       elif cmd == "4":
           running = False
       else:
           print("unknown command")

Run it with:

.. code-block:: bash

   ./ajasendiri todo.aja

Next
----

- Add persistence later with ``fs`` and ``json`` modules.
- Split menu/actions into a separate module and import it.
