# -*- autoconf -*-
AC_DEFUN([AX_CXX_STD_FILESYSTEM], [
    AC_CACHE_CHECK(
        [for a library needed to use std::filesystem],
        [ax_cv_lib_cxx_std_filesystem],
        [saved_LIBS="$LIBS"
         found_fs=no
         AC_LANG_PUSH([C++])
         # -lstdc++fs is required for GCC < 9.1
         # -lc++fs is required for LLVM < 9.0
         for lib in _none_ stdc++fs c++fs; do
             AS_IF([! test x"$lib" = x"_none_"],
                   [LIBS="$saved_LIBS -l$lib"])
             AC_LINK_IFELSE(
                 [AC_LANG_PROGRAM(
                      [@%:@include <filesystem>],
                      [std::filesystem::path p = "/dev/null";
                       std::filesystem::exists(p);])],
                 [AS_IF([test x"$lib" = x"_none_"],
                        [ax_cv_lib_cxx_std_filesystem="none required"],
                        [ax_cv_lib_cxx_std_filesystem="-l$lib"])
                  found_fs=yes
                  break])
         done
         AC_LANG_POP([C++])
         LIBS="$saved_LIBS"
         AS_IF([test x"$found_fs" = x"no"],
               [ax_cv_lib_cxx_std_filesystem="not found"])])
    AS_CASE(
        ["$ax_cv_lib_cxx_std_filesystem"],
        ["none required"], [],
        ["not found"], [AC_MSG_ERROR([Cannot find a way to use std::filesystem])],
        [LIBS="$LIBS $ax_cv_lib_cxx_std_filesystem"
         # Libraries implementing std::filesystem can usually only be found by
         # CXX. Switch to C++ or all of subsequent tests would fail.
         AC_LANG([C++])])
])
