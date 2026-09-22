/*
 * Copyright 1997-2011, Regents of the University of Minnesota
 *
 * tokenizer_failure.c
 *
 * Regression checks for transactional tokenizer allocation failures.
 */

#include "gklib_config.h"
#define GKLIB_EXPORT
#include <GKlib.h>


static int allocation_count;
static int fail_allocation;
static gk_Tokens_t signal_tokens;


static void *test_gk_malloc_nosignal(size_t nbytes)
{
  allocation_count++;
  if (allocation_count == fail_allocation) {
    errno = ENOMEM;
    return NULL;
  }

  return gk_malloc(nbytes, "tokenizer-failure test allocation");
}


#define gk_malloc_nosignal test_gk_malloc_nosignal
#include "../src/tokenizer.c"
#undef gk_malloc_nosignal


/************************************************************************/
/*! Exercises the returning allocation-failure contract. */
/************************************************************************/
static int check_returning_failure(void)
{
  gk_Tokens_t tokens;
  size_t memory_before;

  memset(&tokens, 0, sizeof(tokens));
  if (!gk_malloc_init())
    return 10;
  errno = 0;
  gk_strtokenize(NULL, (char *)",", &tokens);
  if (errno != EINVAL || tokens.ntoks != 0 || tokens.strbuf != NULL ||
      tokens.list != NULL)
    return 12;
  memory_before = gk_GetCurMemoryUsed();
  allocation_count = 0;
  fail_allocation = 2;
  gk_strtokenize((char *)"alpha,beta", (char *)",", &tokens);
  if (tokens.ntoks != 0 || tokens.strbuf != NULL || tokens.list != NULL ||
      gk_GetCurMemoryUsed() != memory_before)
    return 11;

  gk_malloc_cleanup(0);
  return 0;
}


/************************************************************************/
/*! Exercises the signal-recovery allocation-failure contract. */
/************************************************************************/
static int check_signal_failure(void)
{
  size_t memory_before;

  memset(&signal_tokens, 0, sizeof(signal_tokens));
  if (!gk_malloc_init() || !gk_sigtrap())
    return 20;
  memory_before = gk_GetCurMemoryUsed();

  switch (gk_sigcatch()) {
    case 0:
      allocation_count = 0;
      fail_allocation = 2;
      gk_strtokenize((char *)"alpha,beta", (char *)",", &signal_tokens);
      return 21;
    case SIGMEM:
      break;
    default:
      return 22;
  }

  if (!gk_siguntrap() || signal_tokens.ntoks != 0 ||
      signal_tokens.strbuf != NULL || signal_tokens.list != NULL ||
      gk_GetCurMemoryUsed() != memory_before)
    return 23;
  gk_malloc_cleanup(0);
  return 0;
}


int main(void)
{
  int status;

  gk_set_exit_on_error(0);
  status = check_returning_failure();
  if (status != 0)
    return status;

  gk_set_exit_on_error(1);
  return check_signal_failure();
}
