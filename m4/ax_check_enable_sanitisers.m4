dnl -*- autoconf -*-
dnl
dnl --------------------------------------------------------------------------
dnl AX_TRY_SANITISER(
dnl     NAME, [ACTION-IF-AVAILABLE], [ACTION-IF-UNAVAILABLE], [EXTRA-FLAGS])
dnl --------------------------------------------------------------------------
AC_DEFUN([AX_TRY_SANITISER], [
    AC_PREREQ(2.64)dnl for _AC_LANG_PREFIX and AS_VAR_IF

    AS_VAR_PUSHDEF([CACHEVAR],[ax_cv_check_[]_AC_LANG_ABBREV[]flags_$4_$1])dnl
    AC_CACHE_CHECK(
        [whether _AC_LANG compiler supports -fsanitize=$1],
        CACHEVAR,
        [ax_check_save_flags="$[]_AC_LANG_PREFIX[]FLAGS"
         _AC_LANG_PREFIX[]FLAGS="$[]_AC_LANG_PREFIX[]FLAGS $4 -fsanitize=$1"
         ax_check_save_ldflags="$LDFLAGS"
         LDFLAGS="$LDFLAGS -fsanitize=$1"
         AC_LINK_IFELSE(
             [AC_LANG_PROGRAM()],
             [AS_VAR_SET(CACHEVAR, [yes])],
             [AS_VAR_SET(CACHEVAR, [no])])
         _AC_LANG_PREFIX[]FLAGS=$ax_check_save_flags
         LDFLAGS="$ax_check_save_ldflags"])
    AS_VAR_IF(
        CACHEVAR, [yes],
        [m4_default([$2], :)],
        [m4_default([$3], :)])
    AS_VAR_POPDEF([CACHEVAR])dnl
])
dnl
dnl --------------------------------------------------------------------------
dnl AX_APPEND_SANITISER(NAME, VARIABLE)
dnl --------------------------------------------------------------------------
AC_DEFUN([AX_APPEND_SANITISER], [
    AX_REQUIRE_DEFINED([AX_APPEND_FLAG])
    AX_REQUIRE_DEFINED([AX_TRY_SANITISER])

    AX_TRY_SANITISER(
        [$1],
        [AX_APPEND_FLAG([-fsanitize=$1], [$2])],
        [],
        [${$2}])
])
dnl
dnl --------------------------------------------------------------------------
dnl AX_CHECK_ENABLE_SANITISERS()
dnl --------------------------------------------------------------------------
dnl When you use this macro, insert the following lines to your
dnl configure.ac, just after calling AC_INIT():
dnl
dnl # Prevent AC_PROG_CC or AC_PROG_CXX from setting default FLAGS. Necessary
dnl # for using AX_CHECK_ENABLE_SANITISERS.
dnl : ${CFLAGS=}
dnl : ${CXXFLAGS=}
dnl
dnl It is also necessary to put this in Makefile.am that can run tests:
dnl
dnl # Sanitisers tend to fail to run if LD_PRELOAD (or DYLD_INSERT_LIBRARIES on
dnl # Mach-O platforms) is set to something. Clear them before running our
dnl # tests.
dnl AM_TESTS_ENVIRONMENT = \
dnl 	unset -v LD_PRELOAD; \
dnl 	unset -v DYLD_INSERT_LIBRARIES;
dnl --------------------------------------------------------------------------
AC_DEFUN([AX_CHECK_ENABLE_SANITISERS], [
    AX_REQUIRE_DEFINED([AX_APPEND_FLAG])
    AX_REQUIRE_DEFINED([AX_APPEND_SANITISER])

    AC_ARG_ENABLE(
        [sanitisers],
        [AS_HELP_STRING(
           [--enable-sanitisers@<:@=ARG@:>@],
           [comma-separated list of sanitisers to use, e.g. 'address,leak'. Omit the argument or specify 'all' to use as many sanitisers as possible])])
    AS_CASE(
        ["$enable_sanitisers"],
        ["no"],  [enable_sanitisers=],
        ["yes"], [enable_sanitisers=all])

    ax_sanitiser_cflags=
    AS_IF(
        [test x"$enable_sanitisers" = x"all"],
        [# Prefer HWASan over regular ASan because it's faster.
         AX_TRY_SANITISER(
             [hwaddress],
             [AX_APPEND_FLAG([-fsanitize=hwaddress], [ax_sanitiser_cflags])],
             [# It's not supported. Try ASan instead.
              AX_APPEND_SANITISER([address], [ax_sanitiser_cflags])],
             [${ax_sanitiser_cflags}])

         # Try enabling LSan and UBSan, which are supported by both GCC and Clang.
         AX_APPEND_SANITISER([leak],      [ax_sanitiser_cflags])
         AX_APPEND_SANITISER([undefined], [ax_sanitiser_cflags])

         # GCC doesn't allow TSan to be enabled along with ASan or LSan,
         # but it doesn't hurt to try.
         AX_APPEND_SANITISER([thread],    [ax_sanitiser_cflags])

         # Try TySan. Only Clang supports it.
         AX_APPEND_SANITISER([type],      [ax_sanitiser_cflags])
        ],
        [for i in `AS_ECHO([["$enable_sanitisers"]]) | tr "," "\n"`; do
             AX_APPEND_FLAG([-fsanitize-$i], [ax_sanitiser_cflags])
         done])

    # Warnings from sanitisers aren't helpful without debug symbols.
    AS_IF(
        [test x"ax_sanitiser_cflags" != x],
        [AX_APPEND_FLAG([-g], [CFLAGS])
         AX_APPEND_FLAG([-g], [CXXFLAGS])])

    # Sanitisers are slow. Use -O1 unless the user explicitly gave an
    # optimisation flag.
    AS_IF(
        [test x"ax_sanitiser_cflags" != x],
        [AS_CASE(
             [" $CFLAGS "],
             [*" -O"*], [],
             [AX_APPEND_FLAG([-O1], [CFLAGS])])
         AS_CASE(
             [" $CXXFLAGS "],
             [*" -O"*], [],
             [AX_APPEND_FLAG([-O1], [CXXFLAGS])])])

    # Append our sanitiser flags.
    for i in $ax_sanitiser_cflags; do
        AX_APPEND_FLAG([$i], [CFLAGS])
        AX_APPEND_FLAG([$i], [CXXFLAGS])
        AX_APPEND_FLAG([$i], [LDFLAGS])
    done
])
