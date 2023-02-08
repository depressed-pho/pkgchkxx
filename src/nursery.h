#pragma once

#include <pthread.h>

/* An implementation of structured concurrency:
 * https://vorpus.org/blog/notes-on-structured-concurrency-or-go-statement-considered-harmful/
 */
struct nursery;

/* Create a nursery with a given maximum concurrency. It is typically the
 * number of available CPUs.
 */
struct nursery* nursery_create(int concurrency);

/* Register a child task to a nursery. The supplied function will run in a
 * separate thread. It is guaranteed to be started before nursery_destroy()
 * returns. The arguments 'func' and 'arg' has to be alive until
 * nursery_destroy() is called.
 *
 * This is a memory barrier. Whatever memory values the thread calling this
 * function can also be seen by the child task.
 */
void nursery_start_soon(struct nursery* nursery, void (*func)(void* arg), void* restrict arg);

/* Block until all the registered child tasks finish, then deallocate the
 * nursery.
 *
 * This is a memory barrier. Whatever memory values children tasks could
 * see before terminating can also be seen by the thread calling this
 * function.
 */
void nursery_destroy(struct nursery* nursery);
