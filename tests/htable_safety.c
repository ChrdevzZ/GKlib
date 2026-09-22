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
static int error_count;
static int jump_on_error;

#define live_allocations           test_memory.live_allocations
#define allocations_before_failure test_memory.allocations_before_failure
#define fail_next_free             test_memory.fail_next_free



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
  error_count++;
  if (jump_on_error)
    longjmp(failure_jump, signum);
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


int gk_free_nosignal(void **r_ptr)
{
  if (fail_next_free) {
    fail_next_free = 0;
    errno = EINVAL;
    return 0;
  }
  if (*r_ptr != NULL) {
    gk_test_free(&test_memory, *r_ptr);
    *r_ptr = NULL;
  }
  return 1;
}


#include "../src/htable.c"


static int check_positive_capacity(void)
{
  gk_HTable_t *htable;

  allocations_before_failure = -1;
  error_count = 0;
  htable = HTable_Create(0);
  if (htable != NULL)
    HTable_Destroy(htable);
  if (htable != NULL || error_count != 1 || live_allocations != 0)
    return 10;
  if (HTable_HFunction(0, 3) != 0 || error_count != 2 || errno != EINVAL)
    return 11;
  return 0;
}


static int check_unsigned_hash(void)
{
  unsigned int expected = (unsigned int)-3 % 7U;

  return HTable_HFunction(7, -3) == (int)expected ? 0 : 12;
}


static int check_create_signal_cleanup(void)
{
  int signum;

  allocations_before_failure = 1;
  jump_on_error = 1;
  signum = setjmp(failure_jump);
  if (signum == 0) {
    (void)HTable_Create(4);
    jump_on_error = 0;
    return 13;
  }
  jump_on_error = 0;
  if (signum != SIGMEM)
    return 14;
  if (live_allocations != 0) {
    test_release_all();
    return 15;
  }
  return 0;
}


static int check_resize_signal_transaction(void)
{
  gk_HTable_t *htable;
  gk_ikv_t *old_harray;
  int old_nelements, old_htsize, signum, status=0;

  allocations_before_failure = -1;
  htable = HTable_Create(4);
  if (htable == NULL)
    return 16;
  HTable_Insert(htable, 0, 10);
  HTable_Insert(htable, 1, 11);
  old_harray = htable->harray;
  old_nelements = htable->nelements;
  old_htsize = htable->htsize;

  allocations_before_failure = 0;
  jump_on_error = 1;
  signum = setjmp(failure_jump);
  if (signum == 0) {
    HTable_Resize(htable, 8);
    jump_on_error = 0;
    status = 17;
  }
  else {
    jump_on_error = 0;
    if (signum != SIGMEM || htable->harray != old_harray ||
        htable->nelements != old_nelements || htable->htsize != old_htsize ||
        HTable_Search(htable, 0) != 10 || HTable_Search(htable, 1) != 11)
      status = 18;
  }

  htable->harray = old_harray;
  htable->nelements = old_nelements;
  htable->htsize = old_htsize;
  HTable_Destroy(htable);
  return status;
}


static int check_resize_growth_overflow(void)
{
  gk_HTable_t *htable;
  int old_nelements, old_htsize, signum, status=0;

  allocations_before_failure = -1;
  htable = HTable_Create(2);
  if (htable == NULL)
    return 22;
  HTable_Insert(htable, 0, 10);
  HTable_Insert(htable, 1, 11);

  old_nelements = INT_MAX/2 + 1;
  old_htsize = old_nelements/2 + 1;
  htable->nelements = old_nelements;
  htable->htsize = old_htsize;

  jump_on_error = 1;
  signum = setjmp(failure_jump);
  if (signum == 0) {
    HTable_Insert(htable, 3, 12);
    jump_on_error = 0;
    status = 23;
  }
  else {
    jump_on_error = 0;
    if (signum != SIGMEM || htable->nelements != old_nelements ||
        htable->htsize != old_htsize)
      status = 24;
  }

  htable->nelements = 2;
  htable->htsize = 2;
  HTable_Destroy(htable);
  return status;
}


static int check_resize_free_failure(void)
{
  gk_HTable_t *htable;
  gk_ikv_t *old_harray;
  int old_nelements, old_htsize;

  allocations_before_failure = -1;
  htable = HTable_Create(4);
  if (htable == NULL)
    return 35;
  HTable_Insert(htable, 0, 10);
  HTable_Insert(htable, 1, 11);
  old_harray = htable->harray;
  old_nelements = htable->nelements;
  old_htsize = htable->htsize;

  fail_next_free = 1;
  error_count = 0;
  HTable_Resize(htable, 8);
  if (error_count != 1 || errno != EINVAL ||
      htable->harray != old_harray ||
      htable->nelements != old_nelements || htable->htsize != old_htsize ||
      HTable_Search(htable, 0) != 10 || HTable_Search(htable, 1) != 11 ||
      live_allocations != 2) {
    HTable_Destroy(htable);
    return 36;
  }

  HTable_Destroy(htable);
  return live_allocations == 0 ? 0 : 37;
}


static int check_resize_success(void)
{
  gk_HTable_t *htable;
  gk_ikv_t *old_harray;
  int old_nelements, old_htsize;

  allocations_before_failure = -1;
  htable = HTable_Create(5);
  if (htable == NULL)
    return 25;
  HTable_Insert(htable, 0, 10);
  HTable_Insert(htable, 1, 11);
  HTable_Insert(htable, 2, 12);
  HTable_Delete(htable, 1);

  HTable_Resize(htable, 7);
  if (htable->nelements != 7 || htable->htsize != 2 ||
      HTable_Search(htable, 0) != 10 || HTable_Search(htable, 1) != -1 ||
      HTable_Search(htable, 2) != 12) {
    HTable_Destroy(htable);
    return 26;
  }

  old_harray = htable->harray;
  old_nelements = htable->nelements;
  old_htsize = htable->htsize;
  error_count = 0;
  HTable_Resize(htable, 1);
  if (error_count != 1 || errno != EINVAL || htable->harray != old_harray ||
      htable->nelements != old_nelements || htable->htsize != old_htsize) {
    HTable_Destroy(htable);
    return 27;
  }

  HTable_Insert(htable, 3, 13);
  HTable_Insert(htable, 4, 14);
  HTable_Insert(htable, 5, 15);
  if (htable->nelements != 14 || htable->htsize != 5 ||
      HTable_Search(htable, 5) != 15) {
    HTable_Destroy(htable);
    return 28;
  }

  HTable_Destroy(htable);
  return live_allocations == 0 ? 0 : 29;
}


static int check_reserved_and_negative_keys(void)
{
  gk_HTable_t *htable;
  int value;

  htable = HTable_Create(5);
  if (htable == NULL)
    return 30;

  error_count = 0;
  HTable_Insert(htable, HTABLE_EMPTY, 10);
  HTable_Insert(htable, HTABLE_DELETED, 11);
  (void)HTable_Search(htable, HTABLE_EMPTY);
  HTable_Delete(htable, HTABLE_DELETED);
  (void)HTable_GetNext(htable, HTABLE_EMPTY, &value, HTABLE_FIRST);
  (void)HTable_SearchAndDelete(htable, HTABLE_DELETED);
  if (error_count != 6 || htable->htsize != 0) {
    HTable_Destroy(htable);
    return 31;
  }

  HTable_Insert(htable, -3, 17);
  if (HTable_Search(htable, -3) != 17) {
    HTable_Destroy(htable);
    return 32;
  }
  HTable_Delete(htable, -3);
  if (HTable_Search(htable, -3) != -1) {
    HTable_Destroy(htable);
    return 33;
  }

  HTable_Destroy(htable);
  return live_allocations == 0 ? 0 : 34;
}


static int check_invalid_tables(void)
{
  gk_HTable_t invalid = {0, 0, NULL};
  int value=0;

  error_count = 0;
  errno = 0;
  HTable_Reset(NULL);
  HTable_Delete(NULL, 0);
  if (HTable_Search(NULL, 0) != -1 ||
      HTable_GetNext(NULL, 0, &value, HTABLE_FIRST) != -1 ||
      HTable_SearchAndDelete(NULL, 0) != -1 ||
      error_count != 5 || errno != EINVAL)
    return 38;

  error_count = 0;
  errno = 0;
  HTable_Reset(&invalid);
  HTable_Delete(&invalid, 0);
  if (HTable_Search(&invalid, 0) != -1 ||
      HTable_GetNext(&invalid, 0, NULL, HTABLE_FIRST) != -1 ||
      HTable_GetNext(&invalid, 0, &value, 0) != -1 ||
      HTable_SearchAndDelete(&invalid, 0) != -1 ||
      error_count != 6 || errno != EINVAL)
    return 39;

  HTable_Destroy(NULL);
  return 0;
}


int main(void)
{
  int status;

  status = check_positive_capacity();
  if (status != 0)
    return status;
  status = check_unsigned_hash();
  if (status != 0)
    return status;
  status = check_create_signal_cleanup();
  if (status != 0)
    return status;
  status = check_resize_signal_transaction();
  if (status != 0)
    return status;
  status = check_resize_growth_overflow();
  if (status != 0)
    return status;
  status = check_resize_free_failure();
  if (status != 0)
    return status;
  status = check_resize_success();
  if (status != 0)
    return status;

  status = check_reserved_and_negative_keys();
  if (status != 0)
    return status;

  return check_invalid_tables();
}
