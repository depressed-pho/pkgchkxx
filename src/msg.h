#pragma once

#include "options.h"

void msg(const struct pkg_chk_opts* opts, const char * fmt, ...) __attribute__((format(printf, 2, 3)));
void verbose(const struct pkg_chk_opts* opts, const char * fmt, ...) __attribute__((format(printf, 2, 3)));
void verbose_var(const struct pkg_chk_opts* opts, const char* var, const char* value);
