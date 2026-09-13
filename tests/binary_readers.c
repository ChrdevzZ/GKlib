#include <GKlib.h>

#if defined(_WIN32)
#include <windows.h>
#endif


#define TEST_READER(prefix, type, ...) \
  do { \
    type expected[] = {__VA_ARGS__}; \
    type *actual; \
    size_t count = 0; \
    size_t i; \
    if (gk_ ## prefix ## writefilebin(filename, 4, expected) != 4) \
      return 10; \
    actual = gk_ ## prefix ## readfilebin(filename, &count); \
    if (actual == NULL) \
      return 11; \
    if (count != 4) { \
      gk_free((void **)&actual, LTERM); \
      return 11; \
    } \
    for (i=0; i<count; i++) { \
      if (actual[i] != expected[i]) { \
        gk_free((void **)&actual, LTERM); \
        return 12; \
      } \
    } \
    gk_free((void **)&actual, LTERM); \
    actual = gk_ ## prefix ## readfilebin(filename, NULL); \
    if (actual == NULL) \
      return 13; \
    for (i=0; i<4; i++) { \
      if (actual[i] != expected[i]) { \
        gk_free((void **)&actual, LTERM); \
        return 14; \
      } \
    } \
    gk_free((void **)&actual, LTERM); \
  } while (0)


int main(int argc, char **argv)
{
  char *filename;

#if defined(_WIN32)
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif

  if (argc != 3)
    return 1;
  filename = argv[2];

  if (strcmp(argv[1], "char") == 0) {
    TEST_READER(c, char, 'a', 'b', 'c', 'd');
  }
  else if (strcmp(argv[1], "int32") == 0) {
    TEST_READER(i32, int32_t, INT32_C(-4), INT32_C(7), INT32_C(21), INT32_C(42));
  }
  else if (strcmp(argv[1], "int64") == 0) {
    TEST_READER(i64, int64_t, INT64_C(-4), INT64_C(7), INT64_C(21), INT64_C(42));
  }
  else if (strcmp(argv[1], "ssize") == 0) {
    TEST_READER(z, ssize_t, (ssize_t)-4, (ssize_t)7, (ssize_t)21, (ssize_t)42);
  }
  else if (strcmp(argv[1], "float") == 0) {
    TEST_READER(f, float, -4.0f, 7.0f, 21.0f, 42.0f);
  }
  else if (strcmp(argv[1], "double") == 0) {
    TEST_READER(d, double, -4.0, 7.0, 21.0, 42.0);
  }
  else {
    return 2;
  }

  if (remove(filename) != 0)
    return 3;

  return 0;
}
