# -*- autoconf -*-
AC_DEFUN([AX_CXX_STD_THREAD], [
    AC_LANG_PUSH([C++])
    AC_MSG_CHECKING([for a compiler flag to use std::thread])
    saved_CXXFLAGS="$CXXFLAGS"
    found_thread=no
    for flag in _none_ -pthread -lpthread; do
        if ! test x"$flag" = x"_none_"; then
            CXXFLAGS="$saved_CXXFLAGS $flag"
        fi
        AC_LINK_IFELSE(
            [AC_LANG_PROGRAM(
                 [#include <thread>],
                 [std::thread thr(@<:@@:>@(){});])],
            [if test x"$flag" = x"_none_"; then
                 AC_MSG_RESULT([none required])
             else
                 AC_MSG_RESULT([$flag])
             fi
             found_thread=yes
             break])
    done
    if test x"$found_thread" = x"no"; then
        AC_MSG_RESULT([not found])
        AC_MSG_ERROR([Cannot find a way to use std::thread])
    fi
    AC_LANG_POP([C++])
])
