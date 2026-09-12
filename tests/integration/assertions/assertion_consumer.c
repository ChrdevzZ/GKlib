#include <GKlib.h>

#ifndef NDEBUG
#error "The assertion consumer must define NDEBUG"
#endif

int main(void)
{
#if TEST_EXPENSIVE
  ASSERT2(0);
#else
  ASSERT(0);
#endif
  return 0;
}
