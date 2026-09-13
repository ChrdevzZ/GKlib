#include "gklib_config.h"

/* This source-level memory fixture does not exercise or link a regex backend. */
#undef HAVE_PCREPOSIX_H
#undef USE_PCRE
#ifndef USE_GKREGEX
#define USE_GKREGEX 1
#endif

#include <GKlib.h>


static size_t live_allocations;
static int mallocs_before_failure = -1;
static int fail_next_realloc;
static int error_count;


static void *test_malloc(size_t nbytes)
{
  void *ptr;

  if (mallocs_before_failure >= 0) {
    if (mallocs_before_failure == 0) {
      mallocs_before_failure = -1;
      return NULL;
    }
    mallocs_before_failure--;
  }

  ptr = malloc(nbytes);
  if (ptr != NULL)
    live_allocations++;
  return ptr;
}


static void *test_realloc(void *oldptr, size_t nbytes)
{
  void *ptr;
  int had_oldptr = (oldptr != NULL);

  if (fail_next_realloc) {
    fail_next_realloc = 0;
    return NULL;
  }

  ptr = realloc(oldptr, nbytes);
  if (ptr != NULL && !had_oldptr)
    live_allocations++;
  return ptr;
}


static void test_free(void *ptr)
{
  if (ptr != NULL) {
    free(ptr);
    live_allocations--;
  }
}


void gk_errexit(int signum, char *f_str, ...)
{
  (void)signum;
  (void)f_str;
  error_count++;
}


void errexit(char *f_str, ...)
{
  (void)f_str;
  error_count++;
}


#define malloc  test_malloc
#define realloc test_realloc
#define free    test_free
#undef GK_MKALLOC
#define GK_MKALLOC(PREFIX, TYPE)

#include "../src/mcore.c"
#include "../src/memory.c"

#undef malloc
#undef realloc
#undef free


FILE *gk_fopen(char *fname, char *mode, const char *msg)
{
  (void)fname;
  (void)mode;
  (void)msg;
  return NULL;
}


void gk_fclose(FILE *fp)
{
  (void)fp;
}


int gk_fexists(char *fname)
{
  (void)fname;
  return 0;
}


static int check_gkmcore_growth_failure(void)
{
  gk_mcore_t *mcore;
  gk_mop_t *old_mops;
  size_t i;

  mcore = gk_gkmcoreCreate();
  if (mcore == NULL)
    return 10;
  for (i=0; i<mcore->nmops; i++)
    gk_gkmcoreAdd(mcore, GK_MOPT_MARK, 0, NULL);

  old_mops = mcore->mops;
  fail_next_realloc = 1;
  error_count = 0;
  gk_gkmcoreAdd(mcore, GK_MOPT_MARK, 0, NULL);
  if (error_count != 1 || mcore->mops != old_mops ||
      mcore->nmops != 2048 || mcore->cmop != 2048)
    return 11;

  mcore->cmop = 0;
  gk_gkmcoreDestroy(&mcore, 0);
  return live_allocations == 0 ? 0 : 12;
}


static int check_mcore_create_failure(void)
{
  gk_mcore_t *mcore;

  mallocs_before_failure = 0;
  error_count = 0;
  mcore = gk_mcoreCreate(64);
  if (mcore != NULL || error_count != 1 || live_allocations != 0)
    return 15;

  mallocs_before_failure = 1;
  error_count = 0;
  mcore = gk_mcoreCreate(64);
  if (mcore != NULL || error_count != 1 || live_allocations != 0)
    return 16;

  mallocs_before_failure = 2;
  error_count = 0;
  mcore = gk_mcoreCreate(64);
  if (mcore != NULL || error_count != 1 || live_allocations != 0)
    return 17;

  mallocs_before_failure = 0;
  mcore = gk_gkmcoreCreate();
  if (mcore != NULL || live_allocations != 0)
    return 18;

  mallocs_before_failure = 1;
  mcore = gk_gkmcoreCreate();
  if (mcore != NULL || live_allocations != 0)
    return 19;

  return 0;
}


static int check_realloc_failure(void)
{
  unsigned char *ptr;
  void *result;

  if (!gk_malloc_init())
    return 20;
  ptr = (unsigned char *)gk_malloc(64, (char *)"tracked allocation");
  if (ptr == NULL || gk_GetCurMemoryUsed() != 64)
    return 21;
  ptr[0] = 73;

  fail_next_realloc = 1;
  error_count = 0;
  result = gk_realloc(ptr, 128, (char *)"tracked realloc");
  if (result != NULL || error_count != 1 || ptr[0] != 73 ||
      gk_GetCurMemoryUsed() != 64)
    return 22;

  gk_free((void **)&ptr, LTERM);
  gk_malloc_cleanup(0);
  return live_allocations == 0 ? 0 : 23;
}


static int check_realloc_success_stats(void)
{
  unsigned char *ptr;
  size_t cmop;

  if (!gk_malloc_init())
    return 30;
  ptr = (unsigned char *)gk_malloc(64, (char *)"tracked allocation");
  if (ptr == NULL)
    return 31;
  cmop = gkmcore->cmop;

  ptr = (unsigned char *)gk_realloc(ptr, 96, (char *)"tracked realloc");
  if (ptr == NULL || gkmcore->cmop != cmop ||
      gkmcore->num_hallocs != 2 || gkmcore->size_hallocs != 160 ||
      gkmcore->cur_hallocs != 96 || gkmcore->max_hallocs != 96)
    return 32;

  gk_free((void **)&ptr, LTERM);
  gk_malloc_cleanup(0);
  return live_allocations == 0 ? 0 : 33;
}


static int check_realloc_marker_boundary(void)
{
  unsigned char *ptr;
  void *result;

  if (!gk_malloc_init())
    return 35;
  ptr = (unsigned char *)gk_malloc(64, (char *)"outer allocation");
  if (ptr == NULL || !gk_malloc_init())
    return 36;
  ptr[0] = 91;

  error_count = 0;
  result = gk_realloc(ptr, 96, (char *)"cross-marker realloc");
  if (result != NULL || error_count != 1 || ptr[0] != 91 ||
      gk_GetCurMemoryUsed() != 64)
    return 37;

  gk_malloc_cleanup(0);
  gk_free((void **)&ptr, LTERM);
  gk_malloc_cleanup(0);
  return live_allocations == 0 ? 0 : 38;
}


static int check_free_marker_boundary(void)
{
  unsigned char *ptr;
  int status = 0;

  if (!gk_malloc_init())
    return 80;
  ptr = (unsigned char *)gk_malloc(64, (char *)"outer allocation");
  if (ptr == NULL || !gk_malloc_init())
    return 81;
  ptr[0] = 92;

  error_count = 0;
  gk_free((void **)&ptr, LTERM);
  if (ptr == NULL || error_count != 1 || ptr[0] != 92 ||
      gk_GetCurMemoryUsed() != 64)
    status = 82;

  gk_malloc_cleanup(0);
  if (ptr != NULL)
    gk_free((void **)&ptr, LTERM);
  gk_malloc_cleanup(0);
  if (live_allocations != 0 && status == 0)
    status = 83;
  return status;
}


static int check_free_variadic_boundary(void)
{
  unsigned char *outer, *first, *after;
  int status = 0;

  if (!gk_malloc_init())
    return 84;
  outer = (unsigned char *)gk_malloc(24, (char *)"outer allocation");
  if (outer == NULL || !gk_malloc_init())
    return 85;
  first = (unsigned char *)gk_malloc(8, (char *)"first inner allocation");
  after = (unsigned char *)gk_malloc(16, (char *)"later inner allocation");
  if (first == NULL || after == NULL)
    return 86;
  outer[0] = 93;
  after[0] = 94;

  error_count = 0;
  gk_free((void **)&first, &outer, &after, LTERM);
  if (first != NULL || outer == NULL || after == NULL ||
      outer[0] != 93 || after[0] != 94 || error_count != 1 ||
      gk_GetCurMemoryUsed() != 40)
    status = 87;

  if (after != NULL)
    gk_free((void **)&after, LTERM);
  gk_malloc_cleanup(0);
  if (outer != NULL)
    gk_free((void **)&outer, LTERM);
  gk_malloc_cleanup(0);
  if (live_allocations != 0 && status == 0)
    status = 88;
  return status;
}


static int check_free_multiple_success(void)
{
  void *first, *second;

  if (!gk_malloc_init())
    return 89;
  first = gk_malloc(8, (char *)"first allocation");
  second = gk_malloc(16, (char *)"second allocation");
  if (first == NULL || second == NULL || gk_GetCurMemoryUsed() != 24)
    return 90;

  error_count = 0;
  gk_free(&first, &second, LTERM);
  if (first != NULL || second != NULL || error_count != 0 ||
      gk_GetCurMemoryUsed() != 0)
    return 91;

  gk_malloc_cleanup(0);
  return live_allocations == 0 ? 0 : 92;
}


static int check_mcore_delete_boundaries(void)
{
  gk_mcore_t *mcore;
  void *coreptr, *heapptr;
  size_t cmop, corecpos, cur_callocs;
  int status = 0;

  mcore = gk_mcoreCreate(64);
  if (mcore == NULL)
    return 93;
  coreptr = gk_mcoreMalloc(mcore, 8);
  if (coreptr == NULL)
    return 94;
  cmop = mcore->cmop;
  corecpos = mcore->corecpos;
  cur_callocs = mcore->cur_callocs;

  error_count = 0;
  gk_mcoreDel(mcore, coreptr);
  if (error_count != 1 || mcore->cmop != cmop ||
      mcore->corecpos != corecpos || mcore->cur_callocs != cur_callocs)
    status = 95;

  gk_mcorePop(mcore);
  gk_mcoreDestroy(&mcore, 0);
  if (live_allocations != 0)
    return status == 0 ? 96 : status;

  mcore = gk_mcoreCreate(0);
  if (mcore == NULL)
    return 97;
  heapptr = gk_mcoreMalloc(mcore, 8);
  if (heapptr == NULL)
    return 98;
  gk_mcorePush(mcore);
  cmop = mcore->cmop;

  error_count = 0;
  gk_mcoreDel(mcore, heapptr);
  if (error_count != 1 || mcore->cmop != cmop ||
      mcore->cur_hallocs != 8)
    status = 99;

  gk_mcorePop(mcore);
  gk_mcoreDel(mcore, heapptr);
  gk_free(&heapptr, LTERM);
  gk_mcoreDestroy(&mcore, 0);
  if (live_allocations != 0)
    return status == 0 ? 100 : status;
  return status;
}


static int check_mcore_pop_marker_boundary(void)
{
  gk_mcore_t *mcore;
  unsigned char *ptr;
  size_t global_cur;
  int status = 0;

  if (!gk_malloc_init())
    return 101;
  mcore = gk_mcoreCreate(0);
  if (mcore == NULL)
    return 102;
  gk_mcorePush(mcore);
  ptr = (unsigned char *)gk_mcoreMalloc(mcore, 8);
  if (ptr == NULL || !gk_malloc_init())
    return 103;
  ptr[0] = 95;
  global_cur = gk_GetCurMemoryUsed();

  error_count = 0;
  gk_mcorePop(mcore);
  if (error_count != 1 || mcore->cmop != 2 ||
      mcore->cur_hallocs != 8 || mcore->mops[1].ptr != ptr ||
      ptr[0] != 95 || gk_GetCurMemoryUsed() != global_cur)
    status = 104;

  gk_malloc_cleanup(0);
  gk_mcorePop(mcore);
  if (mcore->cmop != 0 || mcore->cur_hallocs != 0)
    status = status == 0 ? 105 : status;
  gk_mcoreDestroy(&mcore, 0);
  gk_malloc_cleanup(0);
  if (live_allocations != 0)
    return status == 0 ? 106 : status;
  return status;
}


static int check_mcore_destroy_marker_boundary(void)
{
  gk_mcore_t *mcore;
  size_t global_cur, old_live;
  int status = 0;

  if (!gk_malloc_init())
    return 107;
  mcore = gk_mcoreCreate(64);
  if (mcore == NULL || !gk_malloc_init())
    return 108;
  global_cur = gk_GetCurMemoryUsed();
  old_live = live_allocations;

  error_count = 0;
  gk_mcoreDestroy(&mcore, 0);
  if (error_count != 1 || mcore == NULL ||
      gk_GetCurMemoryUsed() != global_cur || live_allocations != old_live)
    status = 109;

  gk_malloc_cleanup(0);
  gk_mcoreDestroy(&mcore, 0);
  if (mcore != NULL)
    status = status == 0 ? 110 : status;
  gk_malloc_cleanup(0);
  if (live_allocations != 0)
    return status == 0 ? 111 : status;
  return status;
}


static int check_malloc_reserves_first(void)
{
  gk_mop_t *old_mops;
  size_t old_cmop;
  size_t old_live;
  void *ptr;

  if (!gk_malloc_init())
    return 40;
  old_mops = gkmcore->mops;
  old_cmop = gkmcore->cmop;
  old_live = live_allocations;
  gkmcore->cmop = gkmcore->nmops;

  fail_next_realloc = 1;
  error_count = 0;
  ptr = gk_malloc(16, (char *)"reserve before payload");
  if (ptr != NULL || error_count != 1 || live_allocations != old_live ||
      gkmcore->mops != old_mops || gkmcore->nmops != 2048)
    return 41;

  gkmcore->cmop = old_cmop;
  gk_malloc_cleanup(0);
  return live_allocations == 0 ? 0 : 42;
}


static int check_mcore_reserves_first(void)
{
  gk_mcore_t *mcore;
  gk_mop_t *old_mops;
  void *ptr;

  mcore = gk_mcoreCreate(64);
  if (mcore == NULL)
    return 50;
  old_mops = mcore->mops;
  mcore->cmop = mcore->nmops;

  fail_next_realloc = 1;
  error_count = 0;
  ptr = gk_mcoreMalloc(mcore, 8);
  if (ptr != NULL || error_count != 1 || mcore->corecpos != 0 ||
      mcore->mops != old_mops || mcore->nmops != 2048)
    return 51;

  mcore->cmop = 0;
  gk_mcoreDestroy(&mcore, 0);
  return live_allocations == 0 ? 0 : 52;
}


static int check_tracked_mcore_growth(void)
{
  gk_mcore_t *mcore;

  if (!gk_malloc_init())
    return 60;
  mcore = gk_mcoreCreate(0);
  if (mcore == NULL)
    return 61;
  mcore->cmop = mcore->nmops;

  error_count = 0;
  gk_mcorePush(mcore);
  if (error_count != 0 || mcore->nmops != 4096 || mcore->cmop != 2049)
    return 62;

  mcore->cmop = 0;
  gk_mcoreDestroy(&mcore, 0);
  if (error_count != 0)
    return 63;
  gk_malloc_cleanup(0);
  return live_allocations == 0 ? 0 : 64;
}


static int check_tracked_mcore_outer_frame_growth(void)
{
  gk_mcore_t *mcore;
  gk_mop_t *old_mops;
  size_t old_cmop, old_cur, old_max, old_num, old_size;
  size_t old_mops_bytes, new_mops_bytes;

  if (!gk_malloc_init())
    return 70;
  mcore = gk_mcoreCreate(0);
  if (mcore == NULL)
    return 71;
  mcore->cmop = mcore->nmops;

  if (!gk_malloc_init())
    return 72;
  old_mops = mcore->mops;
  old_cmop = mcore->cmop;
  old_cur = gkmcore->cur_hallocs;
  old_max = gkmcore->max_hallocs;
  old_num = gkmcore->num_hallocs;
  old_size = gkmcore->size_hallocs;
  old_mops_bytes = mcore->nmops*sizeof(gk_mop_t);
  new_mops_bytes = 2*old_mops_bytes;

  fail_next_realloc = 1;
  error_count = 0;
  gk_mcorePush(mcore);
  if (error_count != 1 || mcore->mops != old_mops ||
      mcore->nmops != 2048 || mcore->cmop != old_cmop ||
      gkmcore->cur_hallocs != old_cur || gkmcore->max_hallocs != old_max ||
      gkmcore->num_hallocs != old_num || gkmcore->size_hallocs != old_size)
    return 73;

  error_count = 0;
  gk_mcorePush(mcore);
  if (error_count != 0 || mcore->nmops != 4096 ||
      mcore->cmop != old_cmop + 1 || gkmcore->num_hallocs != old_num + 1 ||
      gkmcore->size_hallocs != old_size + new_mops_bytes ||
      gkmcore->cur_hallocs != old_cur - old_mops_bytes + new_mops_bytes ||
      gkmcore->max_hallocs != gkmcore->cur_hallocs)
    return 74;

  gk_malloc_cleanup(0);
  mcore->cmop = 0;
  gk_mcoreDestroy(&mcore, 0);
  if (error_count != 0)
    return 75;
  gk_malloc_cleanup(0);
  return live_allocations == 0 ? 0 : 76;
}


int main(void)
{
  int status;

  status = check_gkmcore_growth_failure();
  if (status != 0)
    return status;
  status = check_mcore_create_failure();
  if (status != 0)
    return status;
  status = check_realloc_failure();
  if (status != 0)
    return status;
  status = check_realloc_success_stats();
  if (status != 0)
    return status;
  status = check_realloc_marker_boundary();
  if (status != 0)
    return status;
  status = check_free_marker_boundary();
  if (status != 0)
    return status;
  status = check_free_variadic_boundary();
  if (status != 0)
    return status;
  status = check_free_multiple_success();
  if (status != 0)
    return status;
  status = check_mcore_delete_boundaries();
  if (status != 0)
    return status;
  status = check_mcore_pop_marker_boundary();
  if (status != 0)
    return status;
  status = check_mcore_destroy_marker_boundary();
  if (status != 0)
    return status;
  status = check_malloc_reserves_first();
  if (status != 0)
    return status;
  status = check_mcore_reserves_first();
  if (status != 0)
    return status;
  status = check_tracked_mcore_growth();
  if (status != 0)
    return status;
  status = check_tracked_mcore_outer_frame_growth();
  if (status != 0)
    return status;

  return 0;
}
