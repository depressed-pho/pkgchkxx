dnl -*- autoconf -*-
dnl --------------------------------------------------------------------------
dnl AX_LIBSTDCXX_WORKAROUND
dnl --------------------------------------------------------------------------
dnl Workaround for a libstdc++ bug: It has deprecated std::result_of<> but
dnl its own implementation of std::bind() still uses it, causing Clang to
dnl warn.
dnl --------------------------------------------------------------------------
AC_DEFUN([AX_LIBSTDCXX_WORKAROUND], [
    AX_REQUIRE_DEFINED([AX_APPEND_FLAG])
    AX_REQUIRE_DEFINED([AX_CHECK_COMPILE_FLAG])
    AC_LANG_PUSH([C++])

    AX_CHECK_COMPILE_FLAG(
        [-Wdeprecated-declarations],
        [AC_CACHE_CHECK(
             [[if use of std::bind() causes a deprecation warning]],
             [ax_cv_std_bind_emits_deprecation_warning],
             [saved_CXXFLAGS="$CXXFLAGS"
              CXXFLAGS="$CXXFLAGS -Wdeprecated-declarations -Werror"
              AC_COMPILE_IFELSE(
                  [AC_LANG_PROGRAM(
                       [[#include <functional>]],
                       [[std::bind([]() {return;})();]])],
                  [ax_cv_std_bind_emits_deprecation_warning=no],
                  [ax_cv_std_bind_emits_deprecation_warning=yes])
              CXXFLAGS="$saved_CXXFLAGS"])
         AS_IF(
             [test x"$ax_cv_std_bind_emits_deprecation_warning" = xyes],
             [AX_APPEND_FLAG([-Wno-deprecated-declarations], [CXXFLAGS])])],
        [],
        [-Werror])

    AC_LANG_POP([C++])
])
