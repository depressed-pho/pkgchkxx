# -*- autoconf -*-
AC_DEFUN([AX_COMMAND], [
    m4_pushdef([VARIABLE], m4_bpatsubst(m4_toupper([$1]), [@<:@^A-Z0-9@:>@], [_]))
    m4_pushdef([COMMAND], [$1])

    AC_ARG_VAR(VARIABLE, [path to ]COMMAND[ command])
    AS_IF([test -z "$]VARIABLE["],
          [AC_PATH_PROG(VARIABLE, COMMAND, COMMAND)])
    AC_SUBST(VARIABLE)
    AC_DEFINE_UNQUOTED([CFG_]VARIABLE, ["$]VARIABLE["], [Define to the path to ]COMMAND[ command.])

    m4_popdef([COMMAND])
    m4_popdef([VARIABLE])
])
