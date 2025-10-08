#ifndef JOB_QUEUE_H
#define JOB_QUEUE_H

#include <pthread.h>

/*
 * job_queue
 *
 * The struct is intentionally not opaque: the assignment expects the
 * caller to allocate a `struct job_queue` (e.g. `struct job_queue q;`)
 * and then call job_queue_init(&q, capacity).
 *
 * Fields:
 *  - buffer/capacity/head/tail/size : circular buffer implementation
 *  - mutex/not_empty/not_full     : synchronization primitives
 *  - empty                        : optional condvar to let destroy wait until empty
 *  - destroyed                    : flag set by job_queue_destroy()
 *
 * Implementations should use the mutex to protect all fields and use
 * condition variables for blocking push/pop/destroy semantics.
 */
struct job_queue {
  /* circular buffer of void* */
  void **buffer;     /* dynamically allocated array of pointers */
  int capacity;      /* max elements in buffer (size of buffer array) */
  int size;          /* current number of elements stored */
  int head;          /* index of next element to pop */
  int tail;          /* index where next push will store */

  /* synchronization */
  pthread_mutex_t mutex;
  pthread_cond_t not_empty; /* signalled when size goes from 0 -> >0 */
  pthread_cond_t not_full;  /* signalled when size goes from capacity -> <capacity */
  pthread_cond_t empty;     /* signalled when size == 0 (helpful for destroy) */

  /* state flags */
  int destroyed;      /* set to 1 when job_queue_destroy() is called */
};

// Initialise a job queue with the given capacity.  The queue starts out
// empty.  Returns non-zero on error.
int job_queue_init(struct job_queue *job_queue, int capacity);

// Destroy the job queue.  Blocks until the queue is empty before it
// is destroyed.
int job_queue_destroy(struct job_queue *job_queue);

// Push an element onto the end of the job queue.  Blocks if the
// job_queue is full (its size is equal to its capacity).  Returns
// non-zero on error.  It is an error to push a job onto a queue that
// has been destroyed.
int job_queue_push(struct job_queue *job_queue, void *data);

// Pop an element from the front of the job queue.  Blocks if the
// job_queue contains zero elements.  Returns non-zero on error.  If
// job_queue_destroy() has been called (possibly after the call to
// job_queue_pop() blocked), this function will return -1.
int job_queue_pop(struct job_queue *job_queue, void **data);

#endif
