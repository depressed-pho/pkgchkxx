#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.71])
AC_INIT([pkg_chk], [0], [tech-pkg@NetBSD.org])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([config.h])

# Automake options
AM_INIT_AUTOMAKE([foreign])
AM_MAINTAINER_MODE([disable])
AM_SILENT_RULES([no])

# Precious variables
AC_ARG_VAR([BMAKE], [bmake command])
if test -z "$BMAKE"; then
    AC_PATH_PROG([BMAKE], [bmake], [bmake])
fi
AC_ARG_VAR([MAKECONF], [Path to mk.conf])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset strdup])
AX_PTHREAD(
    [],
    [AC_MSG_ERROR([pthread support is missing])])

AC_CONFIG_FILES([
    Makefile
    src/Makefile
])
AC_OUTPUT
