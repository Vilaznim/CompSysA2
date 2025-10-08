#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "job_queue.h"

int job_queue_init(struct job_queue *job_queue, int capacity)
{
  if (job_queue == NULL || capacity <= 0)
  {
    return -1;
  }

  /* Allocate buffer first so we can bail out without touching pthread objects. */
  job_queue->buffer = malloc(sizeof(void *) * (size_t)capacity);
  if (job_queue->buffer == NULL)
  {
    return -1;
  }

  /* Initialize core fields */
  job_queue->capacity = capacity;
  job_queue->size = 0;
  job_queue->head = 0;
  job_queue->tail = 0;
  job_queue->destroyed = 0;

  /* Initialize mutex and condvars. If any init fails we must clean up. */
  if (pthread_mutex_init(&job_queue->mutex, NULL) != 0)
  {
    free(job_queue->buffer);
    job_queue->buffer = NULL;
    return -1;
  }

  if (pthread_cond_init(&job_queue->not_empty, NULL) != 0)
  {
    pthread_mutex_destroy(&job_queue->mutex);
    free(job_queue->buffer);
    job_queue->buffer = NULL;
    return -1;
  }

  if (pthread_cond_init(&job_queue->not_full, NULL) != 0)
  {
    pthread_cond_destroy(&job_queue->not_empty);
    pthread_mutex_destroy(&job_queue->mutex);
    free(job_queue->buffer);
    job_queue->buffer = NULL;
    return -1;
  }

  if (pthread_cond_init(&job_queue->empty, NULL) != 0)
  {
    pthread_cond_destroy(&job_queue->not_full);
    pthread_cond_destroy(&job_queue->not_empty);
    pthread_mutex_destroy(&job_queue->mutex);
    free(job_queue->buffer);
    job_queue->buffer = NULL;
    return -1;
  }

  return 0;
}

int job_queue_destroy(struct job_queue *job_queue)
{
  assert(0);
}

int job_queue_push(struct job_queue *job_queue, void *data)
{
  assert(0);
}

int job_queue_pop(struct job_queue *job_queue, void **data)
{
  assert(0);
}
