# -*- autoconf -*-
AC_DEFUN([AX_CXX_STD_THREAD], [
    AC_CACHE_CHECK(
        [for a compiler flag needed to use std::thread],
        [ax_cv_flag_cxx_std_thread],
        [saved_CXXFLAGS="$CXXFLAGS"
         found_thread=no
         AC_LANG_PUSH([C++])
         for flag in -pthread -lpthread; do
             CXXFLAGS="$saved_CXXFLAGS $flag"
             AC_LINK_IFELSE(
                 [AC_LANG_PROGRAM(
                      [@%:@include <thread>],
                      [std::thread thr(@<:@@:>@(){}); thr.join();])],
                 [ax_cv_flag_cxx_std_thread="$flag"
                  found_thread=yes
                  break])
         done
         AC_LANG_POP([C++])
         CXXFLAGS="$saved_CXXFLAGS"
         AS_IF([test x"$found_thread" = x"no"],
               [ax_cv_flag_cxx_std_thread="not found"])])
    AS_CASE(
        ["$ax_cv_flag_cxx_std_thread"],
        ["none required"], [],
        ["not found"], [AC_MSG_ERROR([Cannot find a way to use std::thread])],
        [CXXFLAGS="$CXXFLAGS $ax_cv_flag_cxx_std_thread"])
])
