#include <GKlib.h>

int main()
{
  int values[] = {2, 1};
  gk_isorti(2, values);
  return values[0] == 1 ? 0 : 1;
}
