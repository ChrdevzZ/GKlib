#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef ptrdiff_t gk_idx_t;

#define LTERM ((void **)0)
#define SIGMEM SIGABRT
#define SIGERR SIGTERM

static int error_count;
static size_t allocation_count;
static size_t fail_at;
static size_t live_allocations;


void gk_AllocMatrix(void ***r_matrix, size_t elmlen, size_t ndim1,
    size_t ndim2)
{
  size_t i, j;
  void **matrix;

  *r_matrix = NULL;
  allocation_count++;
  if (allocation_count == fail_at) {
    error_count++;
    return;
  }
  matrix = (void **)malloc(ndim1*sizeof(void *));
  if (matrix == NULL) {
    error_count++;
    return;
  }
  live_allocations++;

  for (i=0; i<ndim1; i++) {
    allocation_count++;
    if (allocation_count == fail_at)
      break;
    matrix[i] = malloc(ndim2*elmlen);
    if (matrix[i] == NULL)
      break;
    live_allocations++;
  }
  if (i < ndim1) {
    for (j=0; j<i; j++) {
      free(matrix[j]);
      live_allocations--;
    }
    free(matrix);
    live_allocations--;
    error_count++;
    return;
  }

  *r_matrix = matrix;
}


void gk_free(void **ptr1, ...)
{
  va_list plist;
  void **ptr;

  ptr = ptr1;
  va_start(plist, ptr1);
  while (ptr != LTERM) {
    if (*ptr != NULL) {
      free(*ptr);
      *ptr = NULL;
      live_allocations--;
    }
    ptr = va_arg(plist, void **);
  }
  va_end(plist);
}


void gk_errexit(int signum, const char *format, ...)
{
  (void)signum;
  (void)format;
  error_count++;
}


void *gk_malloc(size_t nbytes, const char *msg)
{
  (void)nbytes;
  (void)msg;
  return NULL;
}


void *gk_realloc(void *oldptr, size_t nbytes, const char *msg)
{
  (void)oldptr;
  (void)nbytes;
  (void)msg;
  return NULL;
}


#include "../include/gk_mkmemory.h"

int *test_iset(size_t n, int val, int *x);
GK_MKALLOC(test_i, int)


int main(void)
{
  int **matrix;

  for (fail_at=1; fail_at<=5; fail_at++) {
    allocation_count = 0;
    error_count = 0;
    live_allocations = 0;
    matrix = test_iAllocMatrix(4, 3, 7, "failure fixture");

    if (matrix != NULL || error_count != 1)
      return 1;
    if (live_allocations != 0)
      return 2;
  }

  fail_at = 0;
  allocation_count = 0;
  error_count = 0;
  matrix = test_iAllocMatrix(4, 3, 7, "success fixture");
  if (matrix == NULL || error_count != 0 || live_allocations != 5)
    return 3;

  test_iFreeMatrix(&matrix, 4, 3);
  if (matrix != NULL || live_allocations != 0)
    return 4;

  errno = 0;
  error_count = 0;
  test_iFreeMatrix(NULL, 4, 3);
  if (error_count != 1 || errno != EINVAL || live_allocations != 0)
    return 5;

  return 0;
}
