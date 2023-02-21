# -*- autoconf -*-
AC_DEFUN([AX_CXX_STD_FILESYSTEM], [
    AC_LANG_PUSH([C++])
    AC_MSG_CHECKING([for a library to use std::filesystem])
    saved_LIBS="$LIBS"
    found_fs=no
    changed_LIBS=no
    # -lstdc++fs is required for GCC < 9.1
    # -lc++fs is required for LLVM < 9.0
    for lib in _none_ stdc++fs c++fs; do
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
                 changed_LIBS=yes
             fi
             found_fs=yes
             break])
    done
    if test x"$found_fs" = x"no"; then
        AC_MSG_RESULT([not found])
        AC_MSG_ERROR([Cannot find a way to use std::filesystem])
    fi
    AC_LANG_POP([C++])

    # Libraries implementing std::filesystem can usually only be found by
    # CXX. Switch to C++ or all of subsequent tests would fail.
    if test x"$changed_LIBS" = x"yes"; then
        AC_LANG([C++])
    fi
])
