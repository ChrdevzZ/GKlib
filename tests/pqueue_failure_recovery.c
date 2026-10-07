#define _GK_ERROR_C_
#include "gklib_config.h"

#undef HAVE_PCREPOSIX_H
#undef USE_PCRE
#ifndef USE_GKREGEX
#define USE_GKREGEX 1
#endif

#include <GKlib.h>
#include "memory_failure_support.h"


GK_MKKEYVALUE_T(test_kv_t, int, gk_idx_t)
GK_MKPQUEUE_T(test_pq_t, test_kv_t)
GK_MKPQUEUE2_T(test_pq2_t, int, int)
GK_MKPQUEUE_PROTO(test_pq_, test_pq_t, int, gk_idx_t)
GK_MKPQUEUE2_PROTO(test_pq2_, test_pq2_t, int, int)

static gk_test_memory_state_t test_memory = {{NULL}, 0, -1, 0, 0};
static int failure_errno=ENOMEM;

#define live_allocations           test_memory.live_allocations
#define allocations_before_failure test_memory.allocations_before_failure


#include "../src/error.c"


static void *test_allocate(size_t nbytes)
{
  void *ptr;

  ptr = gk_test_malloc(&test_memory, nbytes);
  if (ptr == NULL) {
    errno = failure_errno;
    gk_errexit(SIGMEM, "controlled priority-queue allocation failure");
    errno = failure_errno;
  }
  return ptr;
}


void *gk_malloc(size_t nbytes, const char *msg)
{
  (void)msg;
  return test_allocate(nbytes);
}


static test_kv_t *test_kv_malloc(size_t n, const char *msg)
{
  (void)msg;
  return (test_kv_t *)test_allocate(n*sizeof(test_kv_t));
}


static int *test_int_malloc(size_t n, const char *msg)
{
  (void)msg;
  return (int *)test_allocate(n*sizeof(int));
}


gk_idx_t *gk_idxsmalloc(size_t n, gk_idx_t value, const char *msg)
{
  gk_idx_t *ptr;
  size_t i;

  (void)msg;
  ptr = (gk_idx_t *)test_allocate(n*sizeof(gk_idx_t));
  if (ptr != NULL) {
    for (i=0; i<n; i++)
      ptr[i] = value;
  }
  return ptr;
}


void gk_free(void **ptr1, ...)
{
  va_list list;
  void **ptr=ptr1;

  va_start(list, ptr1);
  while (ptr != LTERM) {
    if (*ptr != NULL) {
      gk_test_free(&test_memory, *ptr);
      *ptr = NULL;
    }
    ptr = va_arg(list, void **);
  }
  va_end(list);
}


#define test_key_lt(a, b) ((a) < (b))
GK_MKPQUEUE(test_pq_, test_pq_t, test_kv_t, int, gk_idx_t,
    test_kv_malloc, INT_MAX, test_key_lt)
GK_MKPQUEUE2(test_pq2_, test_pq2_t, int, int,
    test_int_malloc, test_int_malloc, INT_MAX, test_key_lt)


static int check_return_create(int variant, int fail_after, int error)
{
  test_pq_t *queue=NULL;
  test_pq2_t *queue2=NULL;

  failure_errno = error;
  allocations_before_failure = fail_after;
  errno = 0;
  gk_set_exit_on_error(0);
  if (variant == 1)
    queue = test_pq_Create(4);
  else
    queue2 = test_pq2_Create2(4);
  if (queue != NULL)
    test_pq_Destroy(queue);
  if (queue2 != NULL)
    test_pq2_Destroy2(&queue2);

  return queue == NULL && queue2 == NULL && errno == error &&
      live_allocations == 0;
}


static int check_return_init(int fail_after, int error)
{
  test_pq_t queue;

  memset(&queue, 0xff, sizeof(queue));
  failure_errno = error;
  allocations_before_failure = fail_after;
  errno = 0;
  gk_set_exit_on_error(0);
  test_pq_Init(&queue, 4);
  if (queue.nnodes != 0 || queue.maxnodes != 0 ||
      queue.heap != NULL || queue.locator != NULL || errno != error ||
      live_allocations != 0)
    return 0;
  test_pq_Free(&queue);
  return live_allocations == 0;
}


static int check_signal_create(int variant, int fail_after, int error)
{
  volatile int signum=0;

  failure_errno = error;
  allocations_before_failure = fail_after;
  errno = 0;
  if (!gk_sigtrap())
    return 0;
  gk_set_exit_on_error(1);
  switch (gk_sigcatch()) {
    case 0:
      if (variant == 1)
        (void)test_pq_Create(4);
      else
        (void)test_pq2_Create2(4);
      break;
    case SIGMEM:
      signum = SIGMEM;
      break;
    default:
      signum = SIGERR;
      break;
  }
  if (!gk_siguntrap())
    return 0;
  gk_set_exit_on_error(0);
  return signum == SIGMEM && errno == error && live_allocations == 0;
}


static int check_signal_init(int fail_after, int error)
{
  volatile int signum=0;
  static test_pq_t queue;

  memset(&queue, 0xff, sizeof(queue));
  failure_errno = error;
  allocations_before_failure = fail_after;
  errno = 0;
  if (!gk_sigtrap())
    return 0;
  gk_set_exit_on_error(1);
  switch (gk_sigcatch()) {
    case 0:
      test_pq_Init(&queue, 4);
      break;
    case SIGMEM:
      signum = SIGMEM;
      break;
    default:
      signum = SIGERR;
      break;
  }
  if (!gk_siguntrap())
    return 0;
  gk_set_exit_on_error(0);
  if (signum != SIGMEM || errno != error || queue.nnodes != 0 ||
      queue.maxnodes != 0 || queue.heap != NULL || queue.locator != NULL ||
      live_allocations != 0)
    return 0;
  test_pq_Free(&queue);
  return live_allocations == 0;
}


static int check_return_size(void)
{
  test_pq_t queue;

  if ((uintmax_t)PTRDIFF_MAX >= (uintmax_t)SIZE_MAX)
    return 1;
  memset(&queue, 0xff, sizeof(queue));
  allocations_before_failure = -1;
  errno = 0;
  gk_set_exit_on_error(0);
  test_pq_Init(&queue, (size_t)PTRDIFF_MAX+1);

  return queue.nnodes == 0 && queue.maxnodes == 0 &&
      queue.heap == NULL && queue.locator == NULL && errno == EOVERFLOW &&
      live_allocations == 0;
}


static int check_signal_size(void)
{
  volatile int signum=0;
  static test_pq_t queue;

  if ((uintmax_t)PTRDIFF_MAX >= (uintmax_t)SIZE_MAX)
    return 1;
  memset(&queue, 0xff, sizeof(queue));
  allocations_before_failure = -1;
  errno = 0;
  if (!gk_sigtrap())
    return 0;
  gk_set_exit_on_error(1);
  switch (gk_sigcatch()) {
    case 0:
      test_pq_Init(&queue, (size_t)PTRDIFF_MAX+1);
      break;
    case SIGMEM:
      signum = SIGMEM;
      break;
    default:
      signum = SIGERR;
      break;
  }
  if (!gk_siguntrap())
    return 0;
  gk_set_exit_on_error(0);

  return signum == SIGMEM && errno == EOVERFLOW && queue.nnodes == 0 &&
      queue.maxnodes == 0 && queue.heap == NULL && queue.locator == NULL &&
      live_allocations == 0;
}


int main(void)
{
  int errors[] = {ENOMEM, EOVERFLOW};
  int error, fail_after, i, variant;

  for (i=0; i<2; i++) {
    error = errors[i];
    for (variant=1; variant<=2; variant++) {
      for (fail_after=0; fail_after<3; fail_after++) {
        if (!check_return_create(variant, fail_after, error))
          return 10 + 10*variant + fail_after;
        if (!check_signal_create(variant, fail_after, error))
          return 40 + 10*variant + fail_after;
      }
    }
    for (fail_after=0; fail_after<2; fail_after++) {
      if (!check_return_init(fail_after, error))
        return 70 + fail_after;
      if (!check_signal_init(fail_after, error))
        return 80 + fail_after;
    }
  }
  if (!check_return_size())
    return 90;
  if (!check_signal_size())
    return 91;

  gk_set_exit_on_error(1);
  return 0;
}
