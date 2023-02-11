# -*- autoconf -*-
AC_DEFUN([AX_CXX_STD_FILESYSTEM], [
    AC_LANG_PUSH([C++])
    AC_MSG_CHECKING([for a library to use std::filesystem])
    saved_LIBS="$LIBS"
    found_fs=no
    for lib in _none_ stdc++fs; do
        if ! test x"$lib" = x"_none_"; then
            LIBS="$saved_LIBS -l$lib"
        fi
        AC_LINK_IFELSE(
            [AC_LANG_PROGRAM(
                 [#include <filesystem>],
                 [std::filesystem::path p = "/dev/null";
                  std::filesystem::exists(p);])],
            [if test x"$lib" = x"_none_"; then
                 AC_MSG_RESULT([none required])
             else
                 AC_MSG_RESULT([-l$lib])
             fi
             found_fs=yes
             break])
    done
    if test x"$found_fs" = x"no"; then
        AC_MSG_RESULT([not found])
        AC_MSG_ERROR([Cannot find a way to use std::filesystem])
    fi
    AC_LANG_POP([C++])
])
