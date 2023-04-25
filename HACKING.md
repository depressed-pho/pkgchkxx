# C++ version

We are currently stuck with the C++17 standard, because C++20 or any newer
standards aren't widely supported at the moment. In the future we might
want to migrate to newer standards.


# Agnostic on the contents of ``PKG_DBDIR``

We currently assume ``PKG_DBDIR`` to be a completely opaque directory
structure, and querying or modifying anything in the directory are done
through [pkg_install](https://pkgsrc.se/pkgtools/pkg_install) commands like
``pkg_info(1)``. This is good for tolerating changes in the database
structure, but of course comes with a cost of ``fork`` & ``exec``, which is
mitigated by spawning many of them and letting them run in parallel. Think
twice before changing this.
