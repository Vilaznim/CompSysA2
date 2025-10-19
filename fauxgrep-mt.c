// Setting _DEFAULT_SOURCE is necessary to activate visibility of
// certain header file contents on GNU/Linux systems.
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>

// err.h contains various nonstandard BSD extensions, but they are
// very handy.
#include <err.h>

#include <pthread.h>

#include "job_queue.h"

// worker function and args
struct worker_args
{
  struct job_queue *q;
  const char *needle;
};

<<<<<<< HEAD
void *worker(void *arg_) {
    struct worker_args *arg = arg_;
    for (;;) {
        char *path = NULL;
        int rc = job_queue_pop(arg->q, (void**)&path);
        if (rc == -1) break;              // queue destroyed → time to exit
        // Search 'path' for 'needle' just like fauxgrep_file() does:
        //   - read file line by line
        //   - if strstr(line, needle) -> print "file:lineNo: line"
        // Guard only the printing:
        // pthread_mutex_lock(arg->print_mu); print...; pthread_mutex_unlock(...)
        // (Do NOT hold the print lock while reading the file.)
        free(path);
    }
    return NULL;
}

int fauxgrep_file(char const *needle, char const *path)
{
  FILE *f = fopen(path, "r");

=======
int fauxgrep_file(char const *needle, char const *path)
{
  FILE *f = fopen(path, "r");

>>>>>>> bestbranch
  if (f == NULL)
  {
    warn("failed to open %s", path);
    return -1;
  }

  char *line = NULL;
  size_t linelen = 0;
  int lineno = 1;

  while (getline(&line, &linelen, f) != -1)
  {
    if (strstr(line, needle) != NULL)
    {
      printf("%s:%d: %s", path, lineno, line);
    }

    lineno++;
  }

  free(line);
  fclose(f);

  return 0;
}

// Worker thread: pop file paths from the queue and process them.
<<<<<<< HEAD
static void *worker_thread(void *vargs) {
=======
static void *worker_thread(void *vargs)
{
>>>>>>> bestbranch
  struct worker_args *args = vargs;
  struct job_queue *q = args->q;
  const char *needle = args->needle;

<<<<<<< HEAD
  for (;;) {
    void *data = NULL;
    int r = job_queue_pop(q, &data);
    if (r != 0) {
=======
  for (;;)
  {
    void *data = NULL;
    int r = job_queue_pop(q, &data);
    if (r != 0)
    {
>>>>>>> bestbranch
      // queue was destroyed and empty -> exit worker
      break;
    }

    char *path = data;
    // process file and free the duplicated path
    (void)fauxgrep_file(needle, path);
    free(path);
  }

  return NULL;
}

int main(int argc, char *const *argv)
{
  if (argc < 2)
  {
    err(1, "usage: [-n INT] STRING paths...");
    exit(1);
  }

  int num_threads = 1;
  char const *needle = argv[1];
  char *const *paths = &argv[2];

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

    needle = argv[3];
    paths = &argv[4];
  }
  else
  {
    needle = argv[1];
    paths = &argv[2];
  }

<<<<<<< HEAD
  struct job_queue q;
  job_queue_init(&q);

  // Create worker threads
  pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
  struct worker_args args = { .q = &q, .needle = needle };
  
  for (int i = 0; i < num_threads; i++) {
    if (pthread_create(&threads[i], NULL, worker_thread, &args) != 0) {
      err(1, "pthread_create() failed");
=======
  // Initialise job queue and spawn worker threads.
  struct job_queue q;
  const int q_capacity = 64;
  if (job_queue_init(&q, q_capacity) != 0)
  {
    err(1, "failed to init job queue");
  }

  pthread_t *threads = malloc(sizeof(pthread_t) * (size_t)num_threads);
  if (threads == NULL)
  {
    err(1, "failed to allocate threads array");
  }

  // Create per-thread args array so each thread gets its own args struct.
  struct worker_args *targs = malloc(sizeof(struct worker_args) * (size_t)num_threads);
  if (targs == NULL)
  {
    free(threads);
    err(1, "failed to allocate thread args");
  }

  for (int i = 0; i < num_threads; i++)
  {
    targs[i].q = &q;
    targs[i].needle = needle;
    if (pthread_create(&threads[i], NULL, worker_thread, &targs[i]) != 0)
    {
      err(1, "failed to create worker thread");
>>>>>>> bestbranch
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
<<<<<<< HEAD
      char *path_copy = strdup(p->fts_path);
      if (path_copy == NULL) {
        err(1, "strdup() failed");
      }
      job_queue_push(&q, path_copy);
      break;
=======
    {
      // Duplicate the path because the FTS library may reuse buffers.
      char *pathcopy = strdup(p->fts_path);
      if (pathcopy == NULL)
      {
        warn("strdup failed for %s", p->fts_path);
        break;
      }

      // Push the duplicated path onto the job queue. Worker frees it.
      if (job_queue_push(&q, pathcopy) != 0)
      {
        // If push fails (e.g., queue destroyed), free and continue.
        free(pathcopy);
      }
    }
    break;
>>>>>>> bestbranch
    default:
      break;
    }
  }

  fts_close(ftsp);

<<<<<<< HEAD
  // Destroy queue and wait for workers to finish
  job_queue_destroy(&q);
  
  for (int i = 0; i < num_threads; i++) {
    if (pthread_join(threads[i], NULL) != 0) {
      err(1, "pthread_join() failed");
    }
  }
  
  free(threads);
=======
  // No more files will be pushed. Destroy the queue which blocks until all
  // queued jobs are processed. This wakes blocked poppers so they can exit.
  if (job_queue_destroy(&q) != 0)
  {
    err(1, "failed to destroy job queue");
  }
>>>>>>> bestbranch

  // Join worker threads and free resources.
  for (int i = 0; i < num_threads; i++)
  {
    pthread_join(threads[i], NULL);
  }
  free(threads);
  free(targs);
  return 0;
}
