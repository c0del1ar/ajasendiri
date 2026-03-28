Language Quick Reference
========================

Core syntax
-----------

- function: ``fuc name(args...) -> Type:``
- immutable value: ``imut name = value``
- custom type: ``type Name:``
- interface: ``interface Name:``
- import block: ``import (...)``
- export block: ``export (...)``

Control flow
------------

- ``if / elif / else``
- ``match / case / default``
- ``for``
- ``while <cond> do:``
- ``do: ... while <cond>``
- ``break`` / ``continue``

Types
-----

- scalars: ``int``, ``float``, ``str``, ``bool``, ``error``, ``void``
- containers: ``list``, ``map``, ``chan``
- generic annotations: ``list[T]``, ``map[str, V]``, ``chan[T]``

Function features
-----------------

- default parameters
- named arguments
- keyword-only parameters via ``*``
- lambda-lite: ``fuc(args...) -> T: expr``
- multi-return: ``-> (T1, T2)``
