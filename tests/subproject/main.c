#include <GKlib.h>


int main(void)
{
  int values[] = {4, 5};

  return gk_isum(2, values, 1) == 9 ? 0 : 1;
}
