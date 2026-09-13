#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef ptrdiff_t gk_idx_t;

#define LTERM ((void **)0)

static size_t allocation_count;
static size_t fail_at;
static size_t live_allocations;


static void *gk_malloc(size_t nbytes, char *msg)
{
  void *ptr;

  (void)msg;
  allocation_count++;
  if (allocation_count == fail_at)
    return NULL;

  ptr = malloc(nbytes == 0 ? 1 : nbytes);
  if (ptr != NULL)
    live_allocations++;

  return ptr;
}


static void *gk_realloc(void *oldptr, size_t nbytes, char *msg)
{
  (void)msg;
  return realloc(oldptr, nbytes == 0 ? 1 : nbytes);
}


static void gk_free(void **ptr1, ...)
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

#include "../include/gk_mkmemory.h"

int *test_iset(size_t n, int val, int *x);
GK_MKALLOC(test_i, int)


int main(void)
{
  int **matrix;

  for (fail_at=1; fail_at<=5; fail_at++) {
    allocation_count = 0;
    live_allocations = 0;
    matrix = test_iAllocMatrix(4, 3, 7, (char *)"failure fixture");

    if (matrix != NULL)
      return 1;
    if (live_allocations != 0)
      return 2;
  }

  fail_at = 0;
  allocation_count = 0;
  matrix = test_iAllocMatrix(4, 3, 7, (char *)"success fixture");
  if (matrix == NULL || live_allocations != 5)
    return 3;

  test_iFreeMatrix(&matrix, 4, 3);
  if (matrix != NULL || live_allocations != 0)
    return 4;

  return 0;
}
