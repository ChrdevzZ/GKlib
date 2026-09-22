#include "gklib_config.h"

#undef HAVE_PCREPOSIX_H
#undef USE_PCRE
#ifndef USE_GKREGEX
#define USE_GKREGEX 1
#endif

#include <GKlib.h>
#include "memory_failure_support.h"


static jmp_buf failure_jump;
static gk_test_memory_state_t test_memory = {{NULL}, 0, -1, 0, 0};
static int error_signal;
static int jump_on_error;

#define live_allocations           test_memory.live_allocations
#define allocations_before_failure test_memory.allocations_before_failure



static void test_release_all(void)
{
  gk_test_release_all(&test_memory);
}


void *gk_malloc_nosignal(size_t nbytes)
{
  return gk_test_malloc(&test_memory, nbytes);
}


void gk_errexit(int signum, const char *format, ...)
{
  (void)format;
  error_signal = signum;
  if (jump_on_error)
    longjmp(failure_jump, signum);
}


uint64_t *gk_ui64set(size_t n, uint64_t value, uint64_t *values)
{
  size_t i;

  for (i=0; i<n; i++)
    values[i] = value;
  return values;
}


size_t *gk_zuset(size_t n, size_t value, size_t *values)
{
  size_t i;

  for (i=0; i<n; i++)
    values[i] = value;
  return values;
}


void gk_free(void **ptr1, ...)
{
  va_list list;
  void **ptr = ptr1;

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


#include "../src/cache.c"


static int check_create_failure(int fail_after)
{
  gk_cache_t *cache;
  int signum;

  allocations_before_failure = fail_after;
  jump_on_error = 1;
  signum = setjmp(failure_jump);
  if (signum == 0) {
    cache = gk_cacheCreate(2, 0, 1);
    jump_on_error = 0;
    if (cache != NULL)
      gk_cacheDestroy(&cache);
    return 10;
  }
  jump_on_error = 0;

  if (signum != SIGMEM)
    return 11;
  if (live_allocations != 0) {
    test_release_all();
    return 12;
  }
  return 0;
}


int main(void)
{
  gk_cache_t *cache;
  int status;

  errno = 0;
  error_signal = 0;
  cache = gk_cacheCreate(0, 0, 1);
  if (cache != NULL || error_signal != SIGERR || errno != EINVAL)
    return 1;

  status = check_create_failure(0);
  if (status != 0)
    return status;
  status = check_create_failure(1);
  if (status != 0)
    return status;
  status = check_create_failure(2);
  if (status != 0)
    return status;

  allocations_before_failure = -1;
  cache = gk_cacheCreate(2, 0, 1);
  if (cache == NULL)
    return 20;
  gk_cacheLoad(cache, 2);
  gk_cacheLoad(cache, 2);
  if (cache->nhits != 1 || cache->nmisses != 1)
    return 21;
  gk_cacheDestroy(&cache);
  return cache == NULL && live_allocations == 0 ? 0 : 22;
}
