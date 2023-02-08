#pragma once

#include "options.h"

/* Extract a set of variables from a given mk.conf. 'vars' is a
 * NULL-terminated array of variables to extract. 'values' is an array of
 * char** that will be replaced with pointers to heap-allocated string
 * values for each variable listed in 'vars'. */
void extract_mk_vars(
    const struct pkg_chk_opts* opts,
    const char* makeconf,
    const char* vars[],
    char* values[]);
