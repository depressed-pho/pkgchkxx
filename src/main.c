#include "env.h"
#include "options.h"

int main(int argc, char** argv) {
    struct pkg_chk_opts opts;
    if (parse_opts(&opts, argc, argv) != 0) {
        return 1;
    }

    if (opts.mode == MODE_HELP) {
        usage(argv[0]);
        return 1;
    }

    struct pkg_chk_env env;
    collect_env(&opts, &env);

    return 0;
}
