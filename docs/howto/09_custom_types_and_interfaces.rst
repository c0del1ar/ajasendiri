Custom Types and Interfaces
===========================

Define a custom type
--------------------

Use ``type`` to define object fields.

.. code-block:: text

   type User:
       name: str
       age: int

Create a value
--------------

.. code-block:: text

   u = User(name = "Aja", age = 21)
   print(u.name)

Define a receiver method
------------------------

.. code-block:: text

   fuc (u: User) greet(prefix: str) -> str:
       return prefix + " " + u.name

   print(u.greet("Hi"))

Define an interface
-------------------

Interfaces describe behavior contracts.

.. code-block:: text

   interface Speaker:
       speak(prefix: str) -> str

Use interface in function parameters
------------------------------------

.. code-block:: text

   fuc say(s: Speaker) -> str:
       return s.speak("Hello")

If a custom type implements the required methods with matching signatures,
it can be passed where the interface is expected.
