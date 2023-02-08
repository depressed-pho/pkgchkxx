#include "config.h"

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/queue.h>

#include "nursery.h"

struct child {
    SIMPLEQ_ENTRY(child) entries;

    void (*func)(void*);
    void* arg;

    /* Invalid if the child task has not started yet. */
    pthread_t thr;
};
SIMPLEQ_HEAD(child_list, child);

struct nursery {
    int concurrency;
    pthread_mutex_t mtx;

    /* The list of children that haven't started yet. */
    struct child_list pending_children;

    /* The list of children that have started but haven't finished. */
    struct child_list running_children;
    size_t num_running_children;

    /* Signaled when a child finishes running. */
    pthread_cond_t finished;
};

struct thread_ctx {
    struct nursery* nursery;
    struct child* child;
};

struct nursery* nursery_create(int concurrency) {
    assert(concurrency > 0);

    struct nursery* nursery = malloc(sizeof(struct nursery));
    if (nursery == NULL) {
        errx(1, "out of memory");
    }

    nursery->concurrency = concurrency;
    if ((errno = pthread_mutex_init(&nursery->mtx, NULL)) != 0) {
        err(1, "pthread_mutex_init");
    }
    SIMPLEQ_INIT(&nursery->pending_children);
    SIMPLEQ_INIT(&nursery->running_children);
    nursery->num_running_children = 0;
    if ((errno = pthread_cond_init(&nursery->finished, NULL)) != 0) {
        err(1, "pthread_cond_init");
    }

    return nursery;
}

/* The entry point of child runner thread. */
static void* nursery_child_main(void* arg) {
    assert(arg != NULL);
    struct thread_ctx* ctx  = arg;
    struct nursery* nursery = ctx->nursery;
    struct child* child     = ctx->child;

    (child->func)(child->arg);

    /* Now that the child has finished, we want to remove it from
     * nursery->running_children. But before doing that we have to acquire
     * the lock. */
    if ((errno = pthread_mutex_lock(&nursery->mtx)) != 0) {
        err(1, "pthread_mutex_lock");
    }

    SIMPLEQ_REMOVE(&nursery->running_children, child, child, entries);
    nursery->num_running_children--;
    free(child);

    /* Signal the caller of nursery_destroy() so that it can spawn more
     * children. */
    if ((errno = pthread_cond_signal(&nursery->finished)) != 0) {
        err(1, "pthread_cond_signal");
    }

    if ((errno = pthread_mutex_unlock(&nursery->mtx)) != 0) {
        err(1, "pthread_mutex_unlock");
    }

    return NULL;
}

/* Start some pending children as long as we haven't reached the maximum
 * concurrency. Assumes nursery->mtx is locked. */
static void nursery_start_some(struct nursery* nursery) {
    while (!SIMPLEQ_EMPTY(&nursery->pending_children) &&
           nursery->num_running_children < nursery->concurrency) {

        struct child* child = SIMPLEQ_FIRST(&nursery->pending_children);
        SIMPLEQ_REMOVE_HEAD(&nursery->pending_children, entries);
        SIMPLEQ_INSERT_TAIL(&nursery->running_children, child, entries);
        nursery->num_running_children++;

        struct thread_ctx* ctx = malloc(sizeof(struct thread_ctx));
        if (ctx == NULL) {
            errx(1, "out of memory");
        }
        ctx->nursery = nursery;
        ctx->child   = child;

        if ((errno = pthread_create(&child->thr, NULL, nursery_child_main, ctx)) != 0) {
            err(1, "pthread_create");
        }
    }
}

void nursery_start_soon(struct nursery* nursery, void (*func)(void* arg), void* restrict arg) {
    assert(nursery != NULL);
    assert(func != NULL);

    struct child* child = malloc(sizeof(struct child));
    if (child == NULL) {
        errx(1, "out of memory");
    }

    child->func = func;
    child->arg  = arg;

    if ((errno = pthread_mutex_lock(&nursery->mtx)) != 0) {
        err(1, "pthread_mutex_lock");
    }

    SIMPLEQ_INSERT_TAIL(&nursery->pending_children, child, entries);
    nursery_start_some(nursery);

    if ((errno = pthread_mutex_unlock(&nursery->mtx)) != 0) {
        err(1, "pthread_mutex_unlock");
    }
}

void nursery_destroy(struct nursery* nursery) {
    assert(nursery != NULL);

    if ((errno = pthread_mutex_lock(&nursery->mtx)) != 0) {
        err(1, "pthread_mutex_lock");
    }

    while (true) {
        nursery_start_some(nursery);

        if (!SIMPLEQ_EMPTY(&nursery->pending_children)) {
            /* We still have pending children, which means we have reached
             * the maximum concurrency. In this case we should wait until
             * some children finishes so that we can start more.
             */
            if ((errno = pthread_cond_wait(&nursery->finished, &nursery->mtx)) != 0) {
                err(1, "pthread_cond_wait");
            }
        }
        else if (!SIMPLEQ_EMPTY(&nursery->running_children)) {
            /* We have no pending children anymore but some children are
             * still running. Wait until they all finish.
             */
            if ((errno = pthread_cond_wait(&nursery->finished, &nursery->mtx)) != 0) {
                err(1, "pthread_cond_wait");
            }
        }
        else {
            /* No pending nor running children. Everything's done. */
            break;
        }
    }

    if ((errno = pthread_mutex_unlock(&nursery->mtx)) != 0) {
        err(1, "pthread_mutex_unlock");
    }
    if ((errno = pthread_cond_destroy(&nursery->finished)) != 0) {
        err(1, "pthread_cond_destroy");
    }
    if ((errno = pthread_mutex_destroy(&nursery->mtx)) != 0) {
        err(1, "pthread_mutex_destroy");
    }
    free(nursery);
}
