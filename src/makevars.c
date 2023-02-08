#include <assert.h>
#include <err.h>
#include <errno.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "makevars.h"
#include "msg.h"

extern char** environ;

void extract_mk_vars(
    const struct pkg_chk_opts* opts,
    const char* makeconf,
    const char* vars[],
    char* values[]) {

    assert(opts     != NULL);
    assert(makeconf != NULL);
    assert(vars     != NULL);
    assert(values   != NULL);

    if (vars[0] == NULL) {
        /* No variables to extract. */
        return;
    }

    /* We can't use popenve(3) because it's a NetBSD extension. */
    int stdin_fds[2];
    if (pipe(stdin_fds) != 0) {
        err(1, "pipe");
    }

    int stdout_fds[2];
    if (pipe(stdout_fds) != 0) {
        err(1, "pipe");
    }

    posix_spawn_file_actions_t actions;
    if ((errno = posix_spawn_file_actions_init(&actions)) != 0) {
        err(1, "posix_spawn_file_actions_init");
    }
    if ((errno = posix_spawn_file_actions_adddup2(&actions, stdin_fds[0], STDIN_FILENO)) != 0) {
        err(1, "posix_spawn_file_actions_adddup2");
    }
    if ((errno = posix_spawn_file_actions_addclose(&actions, stdin_fds[1])) != 0) {
        err(1, "posix_spawn_file_actions_addclose");
    }
    if ((errno = posix_spawn_file_actions_addclose(&actions, stdout_fds[0])) != 0) {
        err(1, "posix_spawn_file_actions_addclose");
    }
    if ((errno = posix_spawn_file_actions_adddup2(&actions, stdout_fds[1], STDOUT_FILENO)) != 0) {
        err(1, "posix_spawn_file_actions_adddup2");
    }

    pid_t pid;
    char* const argv[] = {
        CFG_BMAKE, "-f", "-", "-f", (char*)makeconf, "x", NULL
    };
    if ((errno = posix_spawnp(&pid, CFG_BMAKE, &actions, NULL, argv, environ)) != 0) {
        err(1, "posix_spawnp");
    }
    if ((errno = posix_spawn_file_actions_destroy(&actions)) != 0) {
        err(1, "posix_spawn_file_actions_destroy");
    }

    close(stdin_fds[0]);
    close(stdout_fds[1]);
    FILE* child_stdin = fdopen(stdin_fds[1], "w");
    if (!child_stdin) {
        err(1, "fdopen");
    }
    FILE* child_stdout = fdopen(stdout_fds[0], "r");
    if (!child_stdout) {
        err(1, "fdopen");
    }

    fprintf(child_stdin, "BSD_PKG_MK=1\n");
    fprintf(child_stdin, ".PHONY: x\n");
    fprintf(child_stdin, "x:\n");
    int num_vars = 0;
    for (int i = 0; vars[i]; i++) {
        fprintf(child_stdin, "\t@printf '%%s\\0' \"${%s}\"\n", vars[i]);
        num_vars++;
    }
    if (fclose(child_stdin) != 0) {
        err(1, "fclose");
    }

    for (int i = 0; i < num_vars; i++) {
        char* value = NULL;
        size_t n    = 0;
        if (getdelim(&value, &n, '\0', child_stdout) == -1) {
            errno = ferror(child_stdout);
            err(1, "getdelim");
        }
        else {
            values[i] = value;
            verbose_var(opts, vars[i], value);

            if (strlen(value) == 0) {
                values[i] = NULL;
                free(value);
            }
        }
    }
    if (fclose(child_stdout) != 0) {
        err(1, "fclose");
    }

    int status;
    if (waitpid(pid, &status, 0) == -1) {
        err(1, "waitpid");
    }
}
