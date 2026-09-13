#include <GKlib.h>

int main(void)
{
  int values[] = {3, 1, 2};

  gk_isorti(3, values);
  return values[0] == 1 && values[1] == 2 && values[2] == 3 ? 0 : 1;
}
