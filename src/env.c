#include <assert.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>

#include "env.h"
#include "makevars.h"
#include "msg.h"

static char* strdup_or_die(const char* str) {
    char* duped = strdup(str);
    if (duped) {
        return duped;
    }
    else {
        errx(1, "out of memory");
    }
}

static char* strdup_if_non_null(const char* str) {
    return str == NULL ? NULL : strdup_or_die(str);
}

static char* strdupcat(const char* s1, const char* s2) {
    char* duped = malloc(strlen(s1) + strlen(s2) + 1);
    if (duped) {
        duped[0] = '\0';
        strcat(duped, s1);
        strcat(duped, s2);
        return duped;
    }
    else {
        errx(1, "out of memory");
    }
}

void collect_env(const struct pkg_chk_opts* opts, struct pkg_chk_env* env) {
    assert(env != NULL);

    /* Hide PKG_PATH to avoid breakage in 'make' calls. */
    env->PKG_PATH = strdup_if_non_null(getenv("PKG_PATH"));
    if (env->PKG_PATH) {
        unsetenv("PKG_PATH");
    }

    /* MAKECONF */
    env->MAKECONF = strdup_if_non_null(getenv("MAKECONF"));
    if (!env->MAKECONF) {
        const char const* mkconf_candidates[] = {
            CFG_MAKECONF,
            CFG_PREFIX "/etc/mk.conf",
            "/etc/mk.conf",
            NULL
        };
        for (int i = 0; mkconf_candidates[i]; i++) {
            const char const* mkconf = mkconf_candidates[i];
            struct stat st;
            if (stat(mkconf, &st) == 0) {
                env->MAKECONF = strdup_or_die(mkconf);
                break;
            }
            else if (errno != ENOENT) {
                err(1, "stat");
            }
        }
        if (!env->MAKECONF) {
            env->MAKECONF = strdup_or_die("/dev/null");
        }
    }
    verbose_var(opts, "MAKECONF", env->MAKECONF);

    /* PKGSRCDIR and LOCALBASE */
    env->PKGSRCDIR = strdup_if_non_null(getenv("PKGSRCDIR"));
    env->LOCALBASE = strdup_if_non_null(getenv("LOCALBASE"));
    if (!env->PKGSRCDIR) {
        const char* vars[3];
        char* values[3];

        vars[0] = "PKGSRCDIR";
        if (env->LOCALBASE) {
            vars[1] = NULL;
        }
        else {
            vars[1] = "LOCALBASE";
            vars[2] = NULL;
        }

        extract_mk_vars(opts, env->MAKECONF, vars, values);

        env->PKGSRCDIR = values[0];
        if (!env->LOCALBASE) {
            env->LOCALBASE = values[1];
        }
    }
    if (!env->PKGSRCDIR) {
        /* We couldn't extract PKGSRCDIR from mk.conf. */
        char* const pkgsrcdir_candidates[] = {
            strdupcat(env->LOCALBASE ? env->LOCALBASE : "", "/pkgsrc"),
            strdup_or_die("/usr/pkgsrc"),
            strdup_or_die("."),
            strdup_or_die(".."),
            strdup_or_die("../.."),
            NULL
        };
        for (int i = 0; pkgsrcdir_candidates[i]; i++) {
            const char const* pkgsrcdir = pkgsrcdir_candidates[i];
            char* path = strdupcat(pkgsrcdir, "/mk/bsd.pkg.mk");
            struct stat st;
            if (stat(path, &st) == 0) {
                env->PKGSRCDIR = realpath(pkgsrcdir, NULL);
                if (!env->PKGSRCDIR) {
                    err(1, "realpath: %s", path);
                }
                free(path);
                break;
            }
            else if (errno != ENOENT) {
                err(1, "stat");
            }
            free(path);
        }
        for (int i = 0; pkgsrcdir_candidates[i]; i++) {
            free(pkgsrcdir_candidates[i]);
        }

        if (env->PKGSRCDIR) {
            verbose_var(opts, "PKGSRCDIR", env->PKGSRCDIR);
        }
    }
}
