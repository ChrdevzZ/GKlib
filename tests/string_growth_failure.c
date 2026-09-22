#include "gklib_config.h"
#define GKLIB_EXPORT
#include <GKlib.h>


static int fail_next_realloc;
static char *signal_output;


static void *test_gk_malloc_nosignal(size_t nbytes)
{
  return gk_malloc(nbytes, "string-growth test allocation");
}


static void *test_gk_realloc_nosignal(void *ptr, size_t nbytes)
{
  if (fail_next_realloc) {
    fail_next_realloc = 0;
    errno = ENOMEM;
    return NULL;
  }

  return gk_realloc(ptr, nbytes, "string-growth test reallocation");
}


#define gk_malloc_nosignal  test_gk_malloc_nosignal
#define gk_realloc_nosignal test_gk_realloc_nosignal
#include "../src/string.c"
#undef gk_malloc_nosignal
#undef gk_realloc_nosignal


/************************************************************************/
/*! Verifies that SIGMEM is raised only after regex state is released. */
/************************************************************************/
static int check_signal_cleanup(void)
{
  size_t memory_before;

  signal_output = NULL;
  if (!gk_malloc_init() || !gk_sigtrap())
    return 10;
  memory_before = gk_GetCurMemoryUsed();

  switch (gk_sigcatch()) {
    case 0:
      fail_next_realloc = 1;
      (void)gk_strstr_replace((char *)"aaaa", (char *)"a",
          (char *)"abcdefghijklmnop", (char *)"", &signal_output);
      return 11;
    case SIGMEM:
      break;
    default:
      return 12;
  }

  if (!gk_siguntrap() || signal_output != NULL ||
      gk_GetCurMemoryUsed() != memory_before)
    return 13;
  gk_malloc_cleanup(0);
  return 0;
}


int main(void)
{
  char *output = NULL;
  size_t memory_before;
  int status;

  gk_set_exit_on_error(0);
  if (!gk_malloc_init())
    return 1;

  errno = 0;
  status = gk_strstr_replace(NULL, (char *)"a", (char *)"b",
      (char *)"", &output);
  if (status != 0 || output != NULL || errno != EINVAL)
    return 4;

  memory_before = gk_GetCurMemoryUsed();
  fail_next_realloc = 1;
  status = gk_strstr_replace((char *)"aaaa", (char *)"a",
      (char *)"abcdefghijklmnop", (char *)"", &output);
  if (status != 0 || output != NULL ||
      gk_GetCurMemoryUsed() != memory_before)
    return 2;

  status = gk_strstr_replace((char *)"aaaa", (char *)"a",
      (char *)"abcdefghijklmnop", (char *)"", &output);
  if (status != 2 || output == NULL ||
      strcmp(output, "abcdefghijklmnopaaa") != 0)
    return 3;

  gk_free((void **)&output, LTERM);
  gk_malloc_cleanup(0);

  gk_set_exit_on_error(1);
  return check_signal_cleanup();
}
