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
static void *worker_thread(void *vargs) {
  struct worker_args *args = vargs;
  struct job_queue *q = args->q;
  const char *needle = args->needle;

  for (;;) {
    void *data = NULL;
    int r = job_queue_pop(q, &data);
    if (r != 0) {
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

  struct job_queue q;
  job_queue_init(&q);

  // Create worker threads
  pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
  struct worker_args args = { .q = &q, .needle = needle };
  
  for (int i = 0; i < num_threads; i++) {
    if (pthread_create(&threads[i], NULL, worker_thread, &args) != 0) {
      err(1, "pthread_create() failed");
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
      char *path_copy = strdup(p->fts_path);
      if (path_copy == NULL) {
        err(1, "strdup() failed");
      }
      job_queue_push(&q, path_copy);
      break;
    default:
      break;
    }
  }

  fts_close(ftsp);

  // Destroy queue and wait for workers to finish
  job_queue_destroy(&q);
  
  for (int i = 0; i < num_threads; i++) {
    if (pthread_join(threads[i], NULL) != 0) {
      err(1, "pthread_join() failed");
    }
  }
  
  free(threads);

  return 0;
}
