#include <GKlib.h>
#include <vector>

int main()
{
  std::vector<int> values = {3, 1, 2};

  gk_isorti(values.size(), values.data());
  return values[0] == 1 && values[1] == 2 && values[2] == 3 ? 0 : 1;
}
