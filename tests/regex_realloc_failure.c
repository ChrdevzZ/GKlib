#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "gklib_config.h"

#undef HAVE_PCREPOSIX_H
#undef USE_PCRE
#ifndef USE_GKREGEX
#define USE_GKREGEX 1
#endif

#define DFA_ARRAY_COUNT 5
#define TEST_ALLOCATION_COUNT 16

struct allocation_record {
  void *ptr;
  size_t size;
};

static struct allocation_record allocations[TEST_ALLOCATION_COUNT];
static size_t allocation_count;
static size_t malloc_count;
static size_t malloc_fail_at;
static size_t realloc_count;
static size_t fail_at;
static int fail_next_calloc;


static void *test_allocate(size_t size)
{
  void *ptr;

  if (allocation_count == TEST_ALLOCATION_COUNT)
    return NULL;

  ptr = malloc(size);
  if (ptr == NULL)
    return NULL;

  allocations[allocation_count].ptr = ptr;
  allocations[allocation_count].size = size;
  allocation_count++;

  return ptr;
}


static void *test_malloc(size_t size)
{
  malloc_count++;
  if (malloc_count == malloc_fail_at)
    return NULL;
  return test_allocate(size);
}


static void *test_calloc(size_t count, size_t size)
{
  void *ptr;

  if (fail_next_calloc) {
    fail_next_calloc = 0;
    return NULL;
  }
  if (count != 0 && size > SIZE_MAX/count)
    return NULL;
  ptr = test_allocate(count*size);
  if (ptr != NULL)
    memset(ptr, 0, count*size);
  return ptr;
}


static void *test_realloc(void *oldptr, size_t size)
{
  size_t i;
  void *ptr;

  realloc_count++;
  if (realloc_count == fail_at)
    return NULL;

  for (i=0; i<allocation_count; i++) {
    if (allocations[i].ptr == oldptr)
      break;
  }
  if (i == allocation_count)
    return NULL;

  /* Allocate before releasing the old block so every successful operation
     deterministically moves storage and exposes stale owner pointers. */
  ptr = malloc(size);
  if (ptr == NULL)
    return NULL;
  memcpy(ptr, oldptr, allocations[i].size < size ? allocations[i].size : size);
  free(oldptr);
  allocations[i].ptr = ptr;
  allocations[i].size = size;

  return ptr;
}


static void test_free(void *ptr)
{
  size_t i;

  if (ptr == NULL)
    return;

  for (i=0; i<allocation_count; i++) {
    if (allocations[i].ptr == ptr) {
      allocations[i] = allocations[--allocation_count];
      break;
    }
  }
  free(ptr);
}


#define calloc  test_calloc
#define malloc  test_malloc
#define realloc test_realloc
#define free    test_free
#include "../src/gkregex.c"
#undef calloc
#undef malloc
#undef realloc
#undef free


static int owns_all_arrays(const re_dfa_t *dfa)
{
  void *arrays[DFA_ARRAY_COUNT];
  size_t i, j;

  arrays[0] = dfa->nodes;
  arrays[1] = dfa->nexts;
  arrays[2] = dfa->org_indices;
  arrays[3] = dfa->edests;
  arrays[4] = dfa->eclosures;

  for (i=0; i<DFA_ARRAY_COUNT; i++) {
    for (j=0; j<allocation_count; j++) {
      if (arrays[i] == allocations[j].ptr)
        break;
    }
    if (j == allocation_count)
      return 0;
  }

  return 1;
}


static void release_arrays(void)
{
  size_t i;

  for (i=0; i<allocation_count; i++)
    free(allocations[i].ptr);
  allocation_count = 0;
}


static int run_failure(size_t failure)
{
  re_dfa_t dfa;
  re_token_t token;
  int result;

  memset(&dfa, 0, sizeof(dfa));
  memset(&token, 0, sizeof(token));
  dfa.nodes_alloc = 1;
  dfa.nodes_len = 1;
  dfa.nodes = test_allocate(sizeof(*dfa.nodes));
  dfa.nexts = test_allocate(sizeof(*dfa.nexts));
  dfa.org_indices = test_allocate(sizeof(*dfa.org_indices));
  dfa.edests = test_allocate(sizeof(*dfa.edests));
  dfa.eclosures = test_allocate(sizeof(*dfa.eclosures));
  if (allocation_count != DFA_ARRAY_COUNT) {
    release_arrays();
    return 0;
  }

  realloc_count = 0;
  fail_at = failure;
  result = re_dfa_add_node(&dfa, token);
  if (result != -1 || dfa.nodes_alloc != 1 || dfa.nodes_len != 1 ||
      !owns_all_arrays(&dfa)) {
    release_arrays();
    return 0;
  }

  release_arrays();
  return 1;
}


static int run_success(void)
{
  re_dfa_t dfa;
  re_token_t token;
  int result;

  memset(&dfa, 0, sizeof(dfa));
  memset(&token, 0, sizeof(token));
  dfa.nodes_alloc = 1;
  dfa.nodes_len = 1;
  dfa.nodes = test_allocate(sizeof(*dfa.nodes));
  dfa.nexts = test_allocate(sizeof(*dfa.nexts));
  dfa.org_indices = test_allocate(sizeof(*dfa.org_indices));
  dfa.edests = test_allocate(sizeof(*dfa.edests));
  dfa.eclosures = test_allocate(sizeof(*dfa.eclosures));
  if (allocation_count != DFA_ARRAY_COUNT) {
    release_arrays();
    return 0;
  }

  realloc_count = 0;
  fail_at = 0;
  result = re_dfa_add_node(&dfa, token);
  if (result != 1 || dfa.nodes_alloc != 2 || dfa.nodes_len != 2 ||
      !owns_all_arrays(&dfa)) {
    release_arrays();
    return 0;
  }

  release_arrays();
  return 1;
}


static int run_register_allocation_failure(void)
{
  struct re_registers regs;
  regmatch_t matches[1];
  unsigned result;

  memset(&regs, 0, sizeof(regs));
  memset(matches, 0, sizeof(matches));
  malloc_count = 0;
  malloc_fail_at = 2;
  result = re_copy_regs(&regs, matches, 1, REGS_UNALLOCATED);
  malloc_fail_at = 0;
  if (result != REGS_UNALLOCATED || regs.start != NULL || regs.end != NULL ||
      regs.num_regs != 0 || allocation_count != 0) {
    release_arrays();
    return 0;
  }
  malloc_count = 0;
  result = re_copy_regs(&regs, matches, 1, REGS_UNALLOCATED);
  if (result != REGS_REALLOCATE || regs.start == NULL || regs.end == NULL ||
      regs.num_regs != 2 || allocation_count != 2) {
    release_arrays();
    return 0;
  }
  test_free(regs.start);
  test_free(regs.end);

  return allocation_count == 0;
}


static int run_register_reallocation_failure(size_t failure)
{
  struct re_registers regs;
  regmatch_t matches[2];
  unsigned result;

  memset(&regs, 0, sizeof(regs));
  memset(matches, 0, sizeof(matches));
  regs.num_regs = 1;
  regs.start = test_allocate(sizeof(*regs.start));
  regs.end = test_allocate(sizeof(*regs.end));
  if (allocation_count != 2) {
    release_arrays();
    return 0;
  }

  realloc_count = 0;
  fail_at = failure;
  result = re_copy_regs(&regs, matches, 2, REGS_REALLOCATE);
  fail_at = 0;
  if (result != REGS_UNALLOCATED || regs.start != NULL || regs.end != NULL ||
      regs.num_regs != 0 || allocation_count != 0) {
    release_arrays();
    return 0;
  }
  malloc_count = 0;
  result = re_copy_regs(&regs, matches, 2, REGS_UNALLOCATED);
  if (result != REGS_REALLOCATE || regs.start == NULL || regs.end == NULL ||
      regs.num_regs != 3 || allocation_count != 2) {
    release_arrays();
    return 0;
  }
  test_free(regs.start);
  test_free(regs.end);

  return allocation_count == 0;
}


static int run_fail_stack_failure(size_t realloc_failure,
    size_t malloc_failure)
{
  struct re_fail_stack_t fs;
  regmatch_t regs[1];
  int eps_elem = 0;
  re_node_set eps_via_nodes;
  reg_errcode_t result;
  size_t expected_allocations;

  memset(&fs, 0, sizeof(fs));
  memset(regs, 0, sizeof(regs));
  eps_via_nodes.alloc = eps_via_nodes.nelem = 1;
  eps_via_nodes.elems = &eps_elem;
  fs.alloc = 2;
  fs.num = 1;
  fs.stack = test_allocate(fs.alloc*sizeof(*fs.stack));
  if (fs.stack == NULL) {
    release_arrays();
    return 0;
  }
  fs.stack[0].idx = 1;
  fs.stack[0].node = 2;
  fs.stack[0].regs = test_allocate(sizeof(*fs.stack[0].regs));
  re_node_set_init_empty(&fs.stack[0].eps_via_nodes);
  if (fs.stack[0].regs == NULL) {
    release_arrays();
    return 0;
  }
  expected_allocations = allocation_count;

  realloc_count = 0;
  fail_at = realloc_failure;
  malloc_count = 0;
  malloc_fail_at = malloc_failure;
  result = push_fail_stack(&fs, 3, 4, 1, regs, &eps_via_nodes);
  fail_at = 0;
  malloc_fail_at = 0;
  if (result != REG_ESPACE || fs.num != 1 ||
      allocation_count != expected_allocations) {
    release_arrays();
    return 0;
  }

  free_fail_stack_return(&fs);
  return allocation_count == 0;
}


static int run_fail_stack_success(void)
{
  struct re_fail_stack_t fs;
  regmatch_t regs[1];
  int eps_elem = 0;
  re_node_set eps_via_nodes;
  reg_errcode_t result;

  memset(&fs, 0, sizeof(fs));
  memset(regs, 0, sizeof(regs));
  eps_via_nodes.alloc = eps_via_nodes.nelem = 1;
  eps_via_nodes.elems = &eps_elem;
  fs.alloc = 2;
  fs.stack = test_allocate(fs.alloc*sizeof(*fs.stack));
  if (fs.stack == NULL) {
    release_arrays();
    return 0;
  }

  realloc_count = 0;
  fail_at = 0;
  malloc_count = 0;
  malloc_fail_at = 0;
  result = push_fail_stack(&fs, 3, 4, 1, regs, &eps_via_nodes);
  if (result != REG_NOERROR || fs.num != 1 || allocation_count != 3 ||
      fs.stack[0].idx != 3 || fs.stack[0].node != 4 ||
      fs.stack[0].eps_via_nodes.nelem != 1 ||
      fs.stack[0].eps_via_nodes.elems[0] != eps_elem) {
    release_arrays();
    return 0;
  }

  free_fail_stack_return(&fs);
  return allocation_count == 0;
}


static int run_context_state_failure(size_t failure, int calloc_failure)
{
  re_dfa_t dfa;
  re_token_t token;
  re_node_set nodes;
  re_dfastate_t *state;
  int elem = 0;

  memset(&dfa, 0, sizeof(dfa));
  memset(&token, 0, sizeof(token));
  nodes.alloc = nodes.nelem = 1;
  nodes.elems = &elem;
  token.type = ANCHOR;
  token.opr.ctx_type = PREV_WORD_CONSTRAINT;
  dfa.nodes = &token;
  dfa.state_table = test_allocate(sizeof(*dfa.state_table));
  if (dfa.state_table == NULL) {
    release_arrays();
    return 0;
  }
  memset(dfa.state_table, 0, sizeof(*dfa.state_table));
  dfa.state_table[0].alloc = 1;
  dfa.state_table[0].array = test_allocate(sizeof(*dfa.state_table[0].array));
  if (dfa.state_table[0].array == NULL) {
    release_arrays();
    return 0;
  }

  malloc_count = 0;
  malloc_fail_at = failure;
  fail_next_calloc = calloc_failure;
  state = create_cd_newstate(&dfa, &nodes, CONTEXT_WORD,
      calc_state_hash(&nodes, CONTEXT_WORD));
  malloc_fail_at = 0;
  fail_next_calloc = 0;
  if (failure != 0 || calloc_failure) {
    if (state != NULL || dfa.state_table[0].num != 0 || allocation_count != 2) {
      if (state != NULL)
        free_state(state);
      release_arrays();
      return 0;
    }
  }
  else {
    if (state == NULL || dfa.state_table[0].num != 1 ||
        dfa.state_table[0].array[0] != state ||
        state->entrance_nodes == &state->nodes ||
        state->entrance_nodes->nelem != 1) {
      if (state != NULL)
        free_state(state);
      release_arrays();
      return 0;
    }
    free_state(state);
    if (allocation_count != 2) {
      release_arrays();
      return 0;
    }
  }

  release_arrays();
  return 1;
}


int main(void)
{
  size_t failure;

  for (failure=1; failure<=DFA_ARRAY_COUNT; failure++) {
    if (!run_failure(failure))
      return (int)failure;
  }

  if (!run_success())
    return 6;
  if (!run_register_reallocation_failure(1))
    return 7;
  if (!run_register_reallocation_failure(2))
    return 8;
  if (!run_register_allocation_failure())
    return 9;
  if (!run_fail_stack_failure(1, 0))
    return 10;
  if (!run_fail_stack_failure(0, 1))
    return 11;
  if (!run_fail_stack_failure(0, 2))
    return 12;
  if (!run_fail_stack_success())
    return 13;
  for (failure=1; failure<=3; failure++) {
    if (!run_context_state_failure(failure, 0))
      return 14+(int)failure;
  }
  if (!run_context_state_failure(0, 1))
    return 18;
  if (!run_context_state_failure(0, 0))
    return 19;

  return 0;
}
