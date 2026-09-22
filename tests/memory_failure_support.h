/*
 * Private allocation-failure support shared by source-level test fixtures.
 */

#ifndef _GK_MEMORY_FAILURE_SUPPORT_H_
#define _GK_MEMORY_FAILURE_SUPPORT_H_

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>


#define GK_TEST_MAX_ALLOCS 64

typedef struct {
  void *live_ptrs[GK_TEST_MAX_ALLOCS];
  size_t live_allocations;
  int allocations_before_failure;
  int fail_next_realloc;
  int fail_next_free;
} gk_test_memory_state_t;


static inline void gk_test_record_allocation(gk_test_memory_state_t *state,
    void *ptr)
{
  size_t i;

  if (ptr == NULL)
    return;
  for (i=0; i<GK_TEST_MAX_ALLOCS; i++) {
    if (state->live_ptrs[i] == NULL) {
      state->live_ptrs[i] = ptr;
      state->live_allocations++;
      return;
    }
  }
  abort();
}


static inline void gk_test_forget_allocation(gk_test_memory_state_t *state,
    void *ptr)
{
  size_t i;

  if (ptr == NULL)
    return;
  for (i=0; i<GK_TEST_MAX_ALLOCS; i++) {
    if (state->live_ptrs[i] == ptr) {
      state->live_ptrs[i] = NULL;
      state->live_allocations--;
      return;
    }
  }
  abort();
}


static inline void gk_test_release_all(gk_test_memory_state_t *state)
{
  size_t i;

  for (i=0; i<GK_TEST_MAX_ALLOCS; i++) {
    if (state->live_ptrs[i] != NULL) {
      free(state->live_ptrs[i]);
      state->live_ptrs[i] = NULL;
    }
  }
  state->live_allocations = 0;
}


static inline void *gk_test_malloc(gk_test_memory_state_t *state,
    size_t nbytes)
{
  void *ptr;

  if (state->allocations_before_failure >= 0) {
    if (state->allocations_before_failure == 0) {
      state->allocations_before_failure = -1;
      errno = ENOMEM;
      return NULL;
    }
    state->allocations_before_failure--;
  }

  ptr = malloc(nbytes == 0 ? 1 : nbytes);
  gk_test_record_allocation(state, ptr);
  return ptr;
}


static inline void *gk_test_realloc(gk_test_memory_state_t *state,
    void *oldptr, size_t nbytes)
{
  void *ptr;
  size_t i, oldslot=GK_TEST_MAX_ALLOCS;
  int had_oldptr = oldptr != NULL;

  if (state->fail_next_realloc) {
    state->fail_next_realloc = 0;
    errno = ENOMEM;
    return NULL;
  }

  if (had_oldptr) {
    for (i=0; i<GK_TEST_MAX_ALLOCS; i++) {
      if (state->live_ptrs[i] == oldptr) {
        oldslot = i;
        break;
      }
    }
    if (oldslot == GK_TEST_MAX_ALLOCS)
      abort();
  }

  ptr = realloc(oldptr, nbytes == 0 ? 1 : nbytes);
  if (ptr != NULL) {
    if (had_oldptr)
      state->live_ptrs[oldslot] = ptr;
    else
      gk_test_record_allocation(state, ptr);
  }
  return ptr;
}


static inline void gk_test_free(gk_test_memory_state_t *state, void *ptr)
{
  if (ptr != NULL) {
    gk_test_forget_allocation(state, ptr);
    free(ptr);
  }
}


#endif
