#include <GKlib.h>

#include <signal.h>
#include <stdlib.h>
#include <string.h>

#ifndef NDEBUG
#error "The assertion consumer must define NDEBUG"
#endif

static void handle_abort(int signal_number)
{
  (void) signal_number;
  _Exit(86);
}


int main(int argc, char *argv[])
{
  const char *kind = TEST_EXPENSIVE ? "expensive" : "ordinary";

#if defined(_MSC_VER)
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
  setvbuf(stdout, NULL, _IONBF, 0);
  signal(SIGABRT, handle_abort);

  if (argc > 1 && strcmp(argv[1], "timeout") == 0) {
    volatile unsigned long counter = 0;
    printf("GKLIB_ASSERTION_ARMED:%s\n", kind);
    for (;;)
      ++counter;
  }

  if (argc > 1 && strcmp(argv[1], "wrong-tier") == 0)
    kind = TEST_EXPENSIVE ? "ordinary" : "expensive";

  printf("GKLIB_ASSERTION_ARMED:%s\n", kind);
  fflush(stdout);

  if (argc > 1 && strcmp(argv[1], "unrelated-exit") == 0)
    return 77;
  if (argc > 1 && strcmp(argv[1], "marker-only") == 0)
    _Exit(86);
  if (argc > 1 && strcmp(argv[1], "plain-crash") == 0) {
    signal(SIGABRT, SIG_DFL);
    abort();
  }

#if TEST_EXPENSIVE
  ASSERT2(0);
#else
  ASSERT(0);
#endif
  return 0;
}
