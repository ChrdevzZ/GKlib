#define _GK_ERROR_C_
#include "gklib_config.h"

/* This source-level fixture does not exercise or link a regex backend. */
#undef HAVE_PCREPOSIX_H
#undef USE_PCRE
#ifndef USE_GKREGEX
#define USE_GKREGEX 1
#endif

#include <GKlib.h>
#include "memory_failure_support.h"


static gk_test_memory_state_t test_memory = {{NULL}, 0, -1, 0, 0};

#define live_allocations           test_memory.live_allocations
#define allocations_before_failure test_memory.allocations_before_failure
#define fail_next_realloc          test_memory.fail_next_realloc


static void test_release_all(void)
{
  gk_test_release_all(&test_memory);
}


static void *test_malloc(size_t nbytes)
{
  return gk_test_malloc(&test_memory, nbytes);
}


static void *test_realloc(void *oldptr, size_t nbytes)
{
  return gk_test_realloc(&test_memory, oldptr, nbytes);
}


static void test_free(void *ptr)
{
  gk_test_free(&test_memory, ptr);
}


#include "../src/error.c"

#define malloc  test_malloc
#define realloc test_realloc
#define free    test_free
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


static int test_caught_memory_failure(void (*operation)(void))
{
  int signum;

  gk_set_exit_on_error(1);
  if (!gk_sigtrap())
    return 0;

  switch (gk_sigcatch()) {
    case 0:
      operation();
      signum = 0;
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
  return signum == SIGMEM;
}


static void typed_matrix_row_overflow(void)
{
  size_t count = SIZE_MAX/sizeof(int64_t) + 1;

  (void)gk_i64AllocMatrix(1, count, 0, "typed row overflow");
}


static void typed_matrix_partial_failure(void)
{
  (void)gk_i64AllocMatrix(3, 1, 0, "typed row failure");
}


static void generic_matrix_partial_failure(void)
{
  void **matrix = (void **)1;

  gk_AllocMatrix(&matrix, sizeof(int64_t), 3, 1);
}


static void mcore_core_failure(void)
{
  (void)gk_mcoreCreate(64);
}


static void mcore_records_failure(void)
{
  (void)gk_mcoreCreate(64);
}


static int check_typed_matrix_overflow_cleanup(void)
{
  allocations_before_failure = -1;
  if (!test_caught_memory_failure(typed_matrix_row_overflow))
    return 10;
  if (live_allocations != 0) {
    test_release_all();
    return 11;
  }
  return 0;
}


static int check_typed_matrix_partial_cleanup(void)
{
  allocations_before_failure = 2;
  if (!test_caught_memory_failure(typed_matrix_partial_failure))
    return 12;
  if (live_allocations != 0) {
    test_release_all();
    return 13;
  }
  return 0;
}


static int check_generic_matrix_partial_cleanup(void)
{
  allocations_before_failure = 2;
  if (!test_caught_memory_failure(generic_matrix_partial_failure))
    return 14;
  if (live_allocations != 0) {
    test_release_all();
    return 15;
  }
  return 0;
}


static int check_mcore_create_cleanup(void)
{
  allocations_before_failure = 1;
  if (!test_caught_memory_failure(mcore_core_failure))
    return 16;
  if (live_allocations != 0) {
    test_release_all();
    return 17;
  }

  allocations_before_failure = 2;
  if (!test_caught_memory_failure(mcore_records_failure))
    return 18;
  if (live_allocations != 0) {
    test_release_all();
    return 19;
  }
  return 0;
}


static int check_typed_linear_overflows(void)
{
  int64_t value=37, *ptr;
  size_t count = SIZE_MAX/sizeof(int64_t) + 1;

  gk_set_exit_on_error(0);
  ptr = gk_i64malloc(1, "typed allocation");
  if (ptr == NULL)
    return 29;
  ptr[0] = value;

  errno = 0;
  if (gk_i64malloc(count, "typed overflow") != NULL ||
      errno != EOVERFLOW || live_allocations != 1)
    return 30;
  errno = 0;
  if (gk_i64smalloc(count, 0, "typed overflow") != NULL ||
      errno != EOVERFLOW || live_allocations != 1)
    return 31;
  errno = 0;
  if (gk_i64copy(count, &value, &value) != NULL ||
      errno != EOVERFLOW || value != 37)
    return 32;
  errno = 0;
  if (gk_i64realloc(ptr, count, "typed overflow") != NULL ||
      errno != EOVERFLOW || ptr[0] != value || live_allocations != 1)
    return 33;

  gk_free((void **)&ptr, LTERM);
  return live_allocations == 0 ? 0 : 34;
}


static int check_generic_matrix_overflows(void)
{
  void **matrix = (void **)1;
  size_t rowcount = SIZE_MAX/sizeof(int64_t) + 1;
  size_t outercount = SIZE_MAX/sizeof(void *) + 1;

  errno = 0;
  gk_AllocMatrix(&matrix, sizeof(int64_t), 1, rowcount);
  if (matrix != NULL || errno != EOVERFLOW || live_allocations != 0)
    return 35;

  matrix = (void **)1;
  errno = 0;
  gk_AllocMatrix(&matrix, 1, outercount, 0);
  if (matrix != NULL || errno != EOVERFLOW || live_allocations != 0)
    return 36;

  return 0;
}


static int check_generic_matrix_arguments(void)
{
  errno = 0;
  gk_AllocMatrix(NULL, sizeof(int), 1, 1);
  if (errno != EINVAL || live_allocations != 0)
    return 45;

  errno = 0;
  gk_FreeMatrix(NULL, 1, 1);
  if (errno != EINVAL || live_allocations != 0)
    return 46;

  return 0;
}


static int check_private_size_helpers(void)
{
  size_t result;
  void *ptr;

  errno = 0;
  if (gk_size_add(SIZE_MAX, 1, &result) || errno != EOVERFLOW)
    return 37;
  errno = 0;
  if (gk_size_mul(SIZE_MAX, 2, &result) || errno != EOVERFLOW)
    return 38;
  if (!gk_size_add(5, 7, &result) || result != 12 ||
      !gk_size_mul(5, 7, &result) || result != 35)
    return 39;

  allocations_before_failure = 0;
  errno = 0;
  ptr = gk_malloc_array_nosignal(2, sizeof(int64_t));
  if (ptr != NULL || errno != ENOMEM || live_allocations != 0)
    return 40;

  ptr = gk_malloc_array_nosignal(1, sizeof(int64_t));
  if (ptr == NULL || live_allocations != 1)
    return 41;
  errno = 0;
  if (gk_realloc_array_nosignal(ptr, SIZE_MAX, 2) != NULL ||
      errno != EOVERFLOW || live_allocations != 1)
    return 42;
  fail_next_realloc = 1;
  errno = 0;
  if (gk_realloc_array_nosignal(ptr, 2, sizeof(int64_t)) != NULL ||
      errno != ENOMEM || live_allocations != 1)
    return 43;

  gk_free(&ptr, LTERM);
  return live_allocations == 0 ? 0 : 44;
}


static int check_zero_size_contracts(void)
{
  int64_t **typed_matrix;
  void **generic_matrix = NULL;
  void *ptr;

  gk_set_exit_on_error(0);
  ptr = gk_malloc(0, "zero allocation");
  if (ptr == NULL)
    return 20;
  ptr = gk_realloc(ptr, 0, "zero reallocation");
  if (ptr == NULL)
    return 21;
  gk_free(&ptr, LTERM);

  typed_matrix = gk_i64AllocMatrix(0, SIZE_MAX, 0,
      "zero-row typed matrix");
  if (typed_matrix == NULL)
    return 22;
  gk_i64FreeMatrix(&typed_matrix, 0, SIZE_MAX);

  gk_AllocMatrix(&generic_matrix, 2, 0, SIZE_MAX);
  if (generic_matrix == NULL)
    return 23;
  gk_FreeMatrix(&generic_matrix, 0, SIZE_MAX);

  return live_allocations == 0 ? 0 : 24;
}


static int check_mcore_boundaries(void)
{
  gk_mcore_t *mcore;
  void *exact, *zero;

  mcore = gk_mcoreCreate(16);
  if (mcore == NULL)
    return 25;
  zero = gk_mcoreMalloc(mcore, 0);
  if (zero != mcore->core || mcore->mops[0].type != GK_MOPT_CORE ||
      mcore->mops[0].nbytes != 0 || mcore->corecpos != 0)
    return 26;
  exact = gk_mcoreMalloc(mcore, 16);
  if (exact == NULL || mcore->mops[1].type != GK_MOPT_HEAP ||
      mcore->corecpos != 0)
    return 27;

  gk_mcoreDel(mcore, exact);
  gk_free(&exact, LTERM);
  gk_mcorePop(mcore);
  gk_mcoreDestroy(&mcore, 0);
  return mcore == NULL && live_allocations == 0 ? 0 : 28;
}


int main(void)
{
  int status;

  gk_set_exit_on_error(0);

  status = check_typed_matrix_overflow_cleanup();
  if (status != 0)
    return status;
  status = check_typed_matrix_partial_cleanup();
  if (status != 0)
    return status;
  status = check_generic_matrix_partial_cleanup();
  if (status != 0)
    return status;
  status = check_mcore_create_cleanup();
  if (status != 0)
    return status;
  status = check_typed_linear_overflows();
  if (status != 0)
    return status;
  status = check_generic_matrix_overflows();
  if (status != 0)
    return status;
  status = check_generic_matrix_arguments();
  if (status != 0)
    return status;
  status = check_private_size_helpers();
  if (status != 0)
    return status;
  status = check_zero_size_contracts();
  if (status != 0)
    return status;

  return check_mcore_boundaries();
}
