#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <sys/queue.h>

enum pkg_chk_mode {
    MODE_UNKNOWN = 0,
    MODE_ADD_UPDATE,           /* Default; no -g, -h, -l, -N, or -p */
    MODE_GENERATE_PKGCHK_CONF, /* -g */
    MODE_HELP,                 /* -h */
    MODE_LIST_BIN_PKGS,        /* -l */
    MODE_LOOKUP_TODO,          /* -N */
    MODE_PRINT_PKG_DIRS,       /* -p */
};

struct tag {
    SIMPLEQ_ENTRY(tag) entries;
    char* tag;
};
SIMPLEQ_HEAD(tag_list, tag);

struct pkg_chk_opts {
    enum pkg_chk_mode mode;
    bool add_missing;            /* -a */
    bool include_build_version;  /* -B */
    bool use_binary_pkgs;        /* -b */
    char* pkgchk_conf_path;      /* -C */
    struct tag_list add_tags;    /* -D */
    bool no_clean;               /* -d */
    bool fetch;                  /* -f */
    bool continue_on_errors;     /* -k */
    FILE* logfile;               /* -L */
    bool dry_run;                /* -n */
    char* bin_pkg_path;          /* -P */
    bool list_ver_diffs;         /* -q */
    bool delete_mismatched;      /* -r */
    bool build_from_source;      /* -s */
    struct tag_list remove_tags; /* -U */
    bool update;                 /* -u */
    bool verbose;                /* -v */
};

/* Does *not* exit the program. */
void usage(const char* progname);

/* Return 0 on success, -1 otherwise. */
int parse_opts(struct pkg_chk_opts* opts, int argc, char** argv);
