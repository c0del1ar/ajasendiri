Versioning and Release Policy
=============================

.. index::
   single: versioning
   single: release policy
   single: compatibility

Version Format
--------------

AjaSendiri uses semantic versioning: ``MAJOR.MINOR.PATCH``.

- ``PATCH``: bug fixes, docs fixes, no intentional breaking behavior.
- ``MINOR``: new language/runtime/tooling features, backward compatible by default.
- ``MAJOR``: intentional breaking changes.

Compatibility Rules
-------------------

- Existing valid programs should continue to run across patch and minor releases.
- New strict checks are allowed only when they prevent silent wrong behavior.
- Any breaking syntax/runtime change must be documented before release.

Release Channels
----------------

- ``main`` branch: release-ready state.
- ``dev`` branch: integration branch for ongoing work.
- feature branches: isolated feature or fix work before merge.

Deprecation Policy
------------------

1. Mark feature as deprecated in docs and release notes.
2. Keep compatibility for at least one minor cycle.
3. Remove only in a major release unless there is a security reason.

Docs Update Requirement
-----------------------

Every language/runtime feature change should include:

- docs update in ``docs/``
- at least one pass spec test
- fail/error spec tests when relevant
