#include <GKlib.h>

int main()
{
  regex_t expression;
  char *text = gk_strdup(const_cast<char *>("pcre"));
  int result = regcomp(&expression, "^pcre$", REG_EXTENDED);

  if (result == 0) {
    result = regexec(&expression, text, 0, 0, 0);
    regfree(&expression);
  }
  gk_free(reinterpret_cast<void **>(&text), LTERM);

  return result;
}
