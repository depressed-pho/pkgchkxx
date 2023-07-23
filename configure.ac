#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.71])
AC_INIT([pkgchkxx], [0.1], [pkgsrc-users@NetBSD.org])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([lib/pkgxx/pkgname.hxx])
AC_CONFIG_HEADERS([lib/pkgxx/config.h])

LT_PREREQ([2.4.7])
LT_INIT

# Automake options.
AM_INIT_AUTOMAKE([foreign])
AM_MAINTAINER_MODE([disable])
AM_SILENT_RULES([no])

# Precious variables.
AX_COMMAND([bmake])
AX_COMMAND([pkg_add])
AX_COMMAND([pkg_admin])
AX_COMMAND([pkg_delete])
AX_COMMAND([pkg_info])

AC_ARG_VAR([MAKECONF], [path to mk.conf])
AS_IF([test -n "$MAKECONF"],
      [AC_DEFINE_UNQUOTED([CFG_MAKECONF], ["$MAKECONF"], [Define to the path to `mk.conf' file.])])

# Checks for compilers.
AC_PROG_CXX
AX_CXX_COMPILE_STDCXX([17], [noext], [mandatory])

AC_LANG_PUSH([C++])
AX_APPEND_COMPILE_FLAGS([ dnl
    -Wall dnl
    -Wextra dnl
    -Wconversion dnl
    -Wsuggest-attribute=cold dnl
    -Wsuggest-attribute=const dnl
    -Wsuggest-attribute=format dnl
    -Wsuggest-attribute=noreturn dnl
    -Wsuggest-attribute=pure dnl
    -Wsuggest-override dnl
    -Werror=return-type dnl
    -Wnull-dereference dnl
    -Wuninitialized dnl
])
AC_LANG_POP([C++])

# Checks for programs.
AC_PATH_PROG([UNAME], [uname], [uname])
AC_DEFINE_UNQUOTED([CFG_UNAME], ["$UNAME"], [Define to the path to uname command.])

AC_PATH_PROG([XARGS], [xargs], [xargs])
AC_DEFINE_UNQUOTED([CFG_XARGS], ["$XARGS"], [Define to the path to xargs command.])

PKG_PROG_PKG_CONFIG # We have a run-time dependency on pkg-config to detect X11.
AC_DEFINE_UNQUOTED([CFG_PKG_CONFIG], ["$PKG_CONFIG"], [Define to the path to pkg-config command.])
AC_DEFINE_UNQUOTED([CFG_PKG_CONFIG_LIBDIR], ["$PKG_CONFIG_LIBDIR"], [Define to the list of primary directories where `.pc' files are looked up.])
AC_DEFINE_UNQUOTED([CFG_PKG_CONFIG_PATH], ["$PKG_CONFIG_PATH"], [Define to the list of secondary directories where `.pc' files are looked up.])

AX_DOXYGEN

# Checks for libraries.
AX_CXX_STD_FILESYSTEM
AX_CXX_STD_THREAD
AX_BZIP2
AX_LIBFETCH
AX_ZLIB

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h])
AC_CHECK_HEADERS([unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([dup2])
AC_CHECK_FUNCS([strerror])
AC_CHECK_FUNCS([uname])
AC_CHECK_FUNCS([vfork])
AC_CHECK_FUNCS([execvpe])
AC_FUNC_FORK

AC_CONFIG_FILES([
    Makefile
    doc/Makefile
    doc/Doxyfile
    lib/Makefile
    lib/pkgxx/Makefile
    src/Makefile
    src/pkg_chk/Makefile
    src/pkg_rr/Makefile
])
AC_OUTPUT
