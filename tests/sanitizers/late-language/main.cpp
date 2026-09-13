#include <GKlib.h>

extern "C" const char asan_empty[] = "";
extern "C" const char asan_text[] = "late-language";

int main()
{
  char label[] = "late-language";
  volatile const char *empty = asan_empty;
  volatile const char *text = asan_text;
  void *memory;

  memory = gk_malloc(8, label);
  gk_free(&memory, LTERM);
  return empty[0] != '\0' || text[0] != 'l';
}
