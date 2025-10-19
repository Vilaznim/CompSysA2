// Setting _DEFAULT_SOURCE is necessary to activate visibility of
// certain header file contents on GNU/Linux systems.
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>

#include "job_queue.h"

pthread_mutex_t stdout_mutex = PTHREAD_MUTEX_INITIALIZER;

// err.h contains various nonstandard BSD extensions, but they are
// very handy.
#include <err.h>

#include "histogram.h"

// Global histogram updated by workers. Protected by hist_mutex.
static int global_histogram[8] = {0};
static pthread_mutex_t hist_mutex = PTHREAD_MUTEX_INITIALIZER;

// Worker args for fhistogram workers
struct fhist_worker_args
{
  struct job_queue *q;
};

// Top-level worker function. Pops file paths, computes local histogram,
// merges into the global histogram and prints the global histogram.
static void *fhist_worker_thread(void *v)
{
  struct fhist_worker_args *a = v;
  struct job_queue *qq = a->q;

  for (;;)
  {
    void *data = NULL;
    int r = job_queue_pop(qq, &data);
    if (r != 0)
    {
      /* queue destroyed and empty -> exit */
      break;
    }

    char *path = data;
    int local_histogram[8] = {0};

    // Read the file byte-by-byte and update local histogram.
    FILE *f = fopen(path, "r");
    if (f != NULL)
    {
      unsigned char c;
      while (fread(&c, sizeof(c), 1, f) == 1)
      {
        update_histogram(local_histogram, c);
      }
      fclose(f);
    }
    else
    {
      fflush(stdout);
      warn("failed to open %s", path);
    }

    // Merge local histogram into global and PRINT while holding the lock
    // so the multi-line, multi-printf print_histogram() can't interleave.
    pthread_mutex_lock(&hist_mutex);
    merge_histogram(local_histogram, global_histogram);
    print_histogram(global_histogram);
    fflush(stdout); // ensure the printed block is flushed to the terminal
    pthread_mutex_unlock(&hist_mutex);

    free(path);
  }

  return NULL;
}

int main(int argc, char *const *argv)
{
  if (argc < 2)
  {
    err(1, "usage: paths...");
    exit(1);
  }

  int num_threads = 1;
  char *const *paths = &argv[1];

  if (argc > 3 && strcmp(argv[1], "-n") == 0)
  {
    // Since atoi() simply returns zero on syntax errors, we cannot
    // distinguish between the user entering a zero, or some
    // non-numeric garbage.  In fact, we cannot even tell whether the
    // given option is suffixed by garbage, i.e. '123foo' returns
    // '123'.  A more robust solution would use strtol(), but its
    // interface is more complicated, so here we are.
    num_threads = atoi(argv[2]);

    if (num_threads < 1)
    {
      err(1, "invalid thread count: %s", argv[2]);
    }

    paths = &argv[3];
  }
  else
  {
    paths = &argv[1];
  }

  // Initialise job queue and spawn worker threads.
  struct job_queue q;
  const int q_capacity = 64; /* tuneable */
  if (job_queue_init(&q, q_capacity) != 0)
  {
    err(1, "failed to init job queue");
  }

  pthread_t *threads = malloc(sizeof(pthread_t) * (size_t)num_threads);
  if (threads == NULL)
  {
    err(1, "failed to allocate threads array");
  }

  struct fhist_worker_args *targs = malloc(sizeof(struct fhist_worker_args) * (size_t)num_threads);
  if (targs == NULL)
  {
    free(threads);
    err(1, "failed to allocate thread args");
  }

  for (int i = 0; i < num_threads; i++)
  {
    targs[i].q = &q;
    if (pthread_create(&threads[i], NULL, fhist_worker_thread, &targs[i]) != 0)
    {
      err(1, "failed to create worker thread");
    }
  }

  // FTS_LOGICAL = follow symbolic links
  // FTS_NOCHDIR = do not change the working directory of the process
  //
  // (These are not particularly important distinctions for our simple
  // uses.)
  int fts_options = FTS_LOGICAL | FTS_NOCHDIR;

  FTS *ftsp;
  if ((ftsp = fts_open(paths, fts_options, NULL)) == NULL)
  {
    err(1, "fts_open() failed");
    return -1;
  }

  FTSENT *p;
  while ((p = fts_read(ftsp)) != NULL)
  {
    switch (p->fts_info)
    {
    case FTS_D:
      break;
    case FTS_F:
    {
      // Duplicate the path because FTS may reuse internal buffers.
      char *pathcopy = strdup(p->fts_path);
      if (pathcopy == NULL)
      {
        warn("strdup failed for %s", p->fts_path);
        break;
      }

      // Push the duplicated path onto the job queue. Worker will free it.
      if (job_queue_push(&q, pathcopy) != 0)
      {
        // If push fails (e.g. queue was destroyed), free and continue.
        free(pathcopy);
      }
    }
    break;
    default:
      break;
    }
  }

  fts_close(ftsp);

  // No more files will be pushed. Destroy the queue which blocks until all
  // queued jobs have been consumed. This wakes blocked poppers so they can exit.
  if (job_queue_destroy(&q) != 0)
  {
    err(1, "failed to destroy job queue");
  }

  // Join worker threads and free resources.
  for (int i = 0; i < num_threads; i++)
  {
    pthread_join(threads[i], NULL);
  }
  free(threads);
  free(targs);

  move_lines(9);

  return 0;
}