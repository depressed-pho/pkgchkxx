#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "msg.h"

void msg(const struct pkg_chk_opts* opts, const char* fmt, ...) {
    assert(opts != NULL);

    if (opts->logfile) {
        va_list ap;
        va_start(ap, fmt);
        vfprintf(opts->logfile, fmt, ap);
        va_end(ap);
    }

    if (opts->mode == MODE_LIST_BIN_PKGS) {
        va_list ap;
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
    }
    else {
        va_list ap;
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
    }
}

void verbose(const struct pkg_chk_opts* opts, const char* fmt, ...) {
    assert(opts != NULL);

    if (opts->logfile) {
        va_list ap;
        va_start(ap, fmt);
        vfprintf(opts->logfile, fmt, ap);
        va_end(ap);
    }

    {
        va_list ap;
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
    }
}

void verbose_var(const struct pkg_chk_opts* opts, const char* var, const char* value) {
    assert(opts  != NULL);
    assert(var   != NULL);
    assert(value != NULL);
    verbose(opts, "Variable: %s = %s\n", var, strlen(value) > 0 ? value : "(empty)");
}
