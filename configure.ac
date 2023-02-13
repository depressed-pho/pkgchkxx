#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.71])
AC_INIT([pkg_chk], [0], [tech-pkg@NetBSD.org])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/main.cxx])
AC_CONFIG_HEADERS([config.h])

# Automake options
AM_INIT_AUTOMAKE([foreign])
AM_MAINTAINER_MODE([disable])
AM_SILENT_RULES([no])

# Precious variables
AC_ARG_VAR([BMAKE], [path to bmake command])
if test -z "$BMAKE"; then
    AC_PATH_PROG([BMAKE], [bmake], [bmake])
fi
AC_ARG_VAR([MAKECONF], [path to mk.conf])

# Checks for compilers.
AC_PROG_CXX
AX_CXX_COMPILE_STDCXX([17], [noext], [mandatory])
AX_COMPILER_FLAGS([], [], [yes])

# Checks for programs.
AC_PATH_PROG([UNAME], [uname], [uname])
PKG_PROG_PKG_CONFIG # We have a run-time dependency on pkg-config to detect X11.

# Checks for libraries.
AX_CXX_STD_FILESYSTEM
AX_CXX_STD_THREAD

# Checks for header files.
AC_CHECK_HEADERS([unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([vfork])

AC_CONFIG_FILES([
    Makefile
    src/Makefile
])
AC_OUTPUT
