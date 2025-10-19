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

  // Allocate buffer first so we can bail out without touching pthread objects.
  job_queue->buffer = malloc(sizeof(void *) * (size_t)capacity);
  if (job_queue->buffer == NULL)
  {
    return -1;
  }

  // Initialize core fields
  job_queue->capacity = capacity;
  job_queue->size = 0;
  job_queue->head = 0;
  job_queue->tail = 0;
  job_queue->destroyed = 0;

  // Initialize mutex and condvars. If any init fails we must clean up.
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
  if (job_queue == NULL)
  {
    return -1;
  }

  if (pthread_mutex_lock(&job_queue->mutex) != 0)
  {
    return -1;
  }

  // Mark as destroyed so no new pushes are allowed. Wake any threads
  // waiting in pop so they can return -1.
  job_queue->destroyed = 1;
  pthread_cond_broadcast(&job_queue->not_empty);
  pthread_cond_broadcast(&job_queue->not_full);

  // Wait until the queue is empty to ensure no work is lost.
  while (job_queue->size > 0)
  {
    pthread_cond_wait(&job_queue->empty, &job_queue->mutex);
  }

  // We can now release the mutex and safely destroy synchronization objects.
  // Unlock first, then destroy condvars and mutex.
  pthread_mutex_unlock(&job_queue->mutex);

  pthread_cond_destroy(&job_queue->empty);
  pthread_cond_destroy(&job_queue->not_full);
  pthread_cond_destroy(&job_queue->not_empty);
  pthread_mutex_destroy(&job_queue->mutex);

  free(job_queue->buffer);
  job_queue->buffer = NULL;

  return 0;
}

int job_queue_push(struct job_queue *job_queue, void *data)
{
  if (job_queue == NULL)
  {
    return -1;
  }

  if (pthread_mutex_lock(&job_queue->mutex) != 0)
  {
    return -1;
  }

  // If the queue has been destroyed, pushes are errors.
  if (job_queue->destroyed)
  {
    pthread_mutex_unlock(&job_queue->mutex);
    return -1;
  }

  // Wait while full. If destroyed while waiting, return error.
  while (job_queue->size == job_queue->capacity && !job_queue->destroyed)
  {
    pthread_cond_wait(&job_queue->not_full, &job_queue->mutex);
  }

  if (job_queue->destroyed)
  {
    pthread_mutex_unlock(&job_queue->mutex);
    return -1;
  }

  // Insert element at tail
  job_queue->buffer[job_queue->tail] = data;
  job_queue->tail = (job_queue->tail + 1) % job_queue->capacity;
  job_queue->size++;

  // Signal that queue is not empty (wake waiting poppers)
  if (job_queue->size == 1)
  {
    pthread_cond_broadcast(&job_queue->not_empty);
  }

  pthread_mutex_unlock(&job_queue->mutex);
  return 0;
}

int job_queue_pop(struct job_queue *job_queue, void **data)
{
  if (job_queue == NULL || data == NULL)
  {
    return -1;
  }

  if (pthread_mutex_lock(&job_queue->mutex) != 0)
  {
    return -1;
  }

  // Wait while empty. If destroyed while waiting and still empty, return -1.
  while (job_queue->size == 0 && !job_queue->destroyed)
  {
    pthread_cond_wait(&job_queue->not_empty, &job_queue->mutex);
  }

  // If queue is empty and destroyed, caller should be told to stop.
  if (job_queue->size == 0 && job_queue->destroyed)
  {
    pthread_mutex_unlock(&job_queue->mutex);
    return -1;
  }

  // Remove element from head
  *data = job_queue->buffer[job_queue->head];
  job_queue->head = (job_queue->head + 1) % job_queue->capacity;
  job_queue->size--;

  // Signal that queue has space for pushers
  if (job_queue->size == job_queue->capacity - 1)
  {
    pthread_cond_broadcast(&job_queue->not_full);
  }

  // If queue became empty, signal destroyer waiting on empty.
  if (job_queue->size == 0)
  {
    pthread_cond_broadcast(&job_queue->empty);
  }

  pthread_mutex_unlock(&job_queue->mutex);
  return 0;
}
