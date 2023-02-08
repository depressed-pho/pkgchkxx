#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "msg.h"
#include "options.h"

extern char* optarg;
extern int optind;

/* Return 0 on success, -1 otherwise. */
int parse_opts(struct pkg_chk_opts* opts, int argc, char** argv) {
    assert(opts != NULL);
    memset(opts, 0, sizeof(*opts));
    SIMPLEQ_INIT(&opts->add_tags);
    SIMPLEQ_INIT(&opts->remove_tags);

    int ch;
    while ((ch = getopt(argc, argv, "BC:D:L:P:U:abcdfghiklNnpqrsuv")) != -1) {
        switch (ch) {
        case 'a':
            opts->mode        = MODE_ADD_UPDATE;
            opts->add_missing = true;
            break;
        case 'B':
            opts->include_build_version = true;
            break;
        case 'b':
            opts->use_binary_pkgs = true;
            break;
        case 'C':
            opts->pkgchk_conf_path = strdup(optarg);
            break;
        case 'c':
            warnx("option -c is deprecated. Use -a -q");
            opts->mode           = MODE_ADD_UPDATE;
            opts->add_missing    = true;
            opts->list_ver_diffs = true;
            break;
        case 'D': {
            char* tags = strdup(optarg);
            char* tag;
            char* last;
            while ((tag = strtok_r(tags, ",", &last)) != NULL) {
                struct tag* t = malloc(sizeof(struct tag));
                t->tag = strdup(tag);
                SIMPLEQ_INSERT_TAIL(&opts->add_tags, t, entries);
            }
            free(tags);
        }
            break;
        case 'f':
            opts->fetch = true;
            break;
        case 'g':
            opts->mode = MODE_GENERATE_PKGCHK_CONF;
            break;
        case 'h':
            opts->mode = MODE_HELP;
            break;
        case 'i':
            warnx("option -i is deprecated. Use -u -q");
            opts->mode           = MODE_ADD_UPDATE;
            opts->update         = true;
            opts->list_ver_diffs = true;
            break;
        case 'k':
            opts->continue_on_errors = true;
            break;
        case 'L':
            opts->logfile = fopen(optarg, "a");
            if (!opts->logfile) {
                warnx("%s", optarg);
            }
            break;
        case 'l':
            opts->mode = MODE_LIST_BIN_PKGS;
            break;
        case 'N':
            opts->mode = MODE_LOOKUP_TODO;
            break;
        case 'n':
            opts->dry_run = true;
            break;
        case 'p':
            opts->mode = MODE_PRINT_PKG_DIRS;
            break;
        case 'q':
            opts->list_ver_diffs = true;
            break;
        case 'r':
            opts->mode              = MODE_ADD_UPDATE;
            opts->delete_mismatched = true;
            break;
        case 's':
            opts->build_from_source = true;
            break;
        case 'U': {
            char* tags = strdup(optarg);
            char* tag;
            char* last;
            while ((tag = strtok_r(tags, ",", &last)) != NULL) {
                struct tag* t = malloc(sizeof(struct tag));
                t->tag = strdup(tag);
                SIMPLEQ_INSERT_TAIL(&opts->remove_tags, t, entries);
            }
            free(tags);
        }
            break;
        case 'u':
            opts->mode   = MODE_ADD_UPDATE;
            opts->update = true;
            break;
        case 'v':
            opts->verbose = true;
            break;
        case '?':
            return -1;
        default:
            warnx("Unknown option: %c\n", (char)ch);
            return -1;
        }
    }

    verbose(opts, "ARGV:");
    for (int i = 0; i < argc; i++) {
        verbose(opts, " %s", argv[i]);
    }
    verbose(opts, "\n");

    argc -= optind;
    argv += optind;

    if (!opts->use_binary_pkgs && !opts->build_from_source) {
        opts->use_binary_pkgs   = true;
        opts->build_from_source = true;
    }

    if (opts->mode == MODE_UNKNOWN) {
        warnx("must specify at least one of -a, -g, -l, -p, -r, -u, or -N");
        return -1;
    }

    if (argc > 0) {
        warnx("an additional argument is given: %s", argv[0]);
        return -1;
    }

    return 0;
}

void usage(const char* progname) {
    printf("Usage: %s [opts]\n", progname);
    printf("    -a       Add all missing packages\n");
    printf("    -B       Force exact pkg match - check \"Build version\" & even downgrade\n");
    printf("    -b       Use binary packages\n");
    printf("    -C conf  Use pkgchk.conf file 'conf'\n");
    printf("    -D tags  Comma separated list of additional pkgchk.conf tags to set\n");
    printf("    -d       Do not clean the pkg build dirs\n");
    printf("    -f       Perform a 'make fetch' for all required packages\n");
    printf("    -g       Generate an initial pkgchk.conf file\n");
    printf("    -h       Print this help\n");
    printf("    -k       Continue with further packages if errors are encountered\n");
    printf("    -L file  Redirect output from commands run into file (should be fullpath)\n");
    printf("    -l       List binary packages including dependencies\n");
    printf("    -N       List installed packages for which a newer version is in TODO\n");
    printf("    -n       Display actions that would be taken, but do not perform them\n");
    printf("    -p       Display the list of pkgdirs that match the current tags\n");
    printf("    -P dir   Set PACKAGES dir (overrides any other setting)\n");
    printf("    -q       Do not display actions or take any action; only list packages\n");
    printf("    -r       Recursively remove mismatches (use with care)\n");
    printf("    -s       Use source for building packages\n");
    printf("    -U tags  Comma separated list of pkgchk.conf tags to unset ('*' for all)\n");
    printf("    -u       Update all mismatched packages\n");
    printf("    -v       Be verbose\n");
    printf("\n");
    printf("pkg_chk verifies installed packages against pkgsrc.\n");
    printf("The most common usage is 'pkg_chk -u -q' to check all installed packages or\n");
    printf("'pkg_chk -u' to update all out of date packages.\n");
    printf("For more advanced usage, including defining a set of desired packages based\n");
    printf("on hostname and type, see pkg_chk(8).\n");
    printf("\n");
    printf("If neither -b nor -s is given, both are assumed with -b preferred.\n");
    printf("\n");
}
