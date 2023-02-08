#pragma once

#include "options.h"

struct pkg_chk_env {
    char* PKG_PATH; /* Can be NULL */
    char* MAKECONF;
    char* PKGSRCDIR;
    char* LOCALBASE;
};

void collect_env(const struct pkg_chk_opts* opts, struct pkg_chk_env* env);
