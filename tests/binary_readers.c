#include <GKlib.h>

#if defined(_WIN32)
#include <windows.h>
#endif


static int create_symbolic_link(const char *linkname, const char *target)
{
#ifdef _WIN32
  DWORD error;

  if (CreateSymbolicLinkA(linkname, target,
      SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE))
    return 1;
  error = GetLastError();
  if (error == ERROR_INVALID_PARAMETER &&
      CreateSymbolicLinkA(linkname, target, 0))
    return 1;
  if (error == ERROR_PRIVILEGE_NOT_HELD || error == ERROR_INVALID_PARAMETER ||
      error == ERROR_NOT_SUPPORTED)
    return 0;
  return -1;
#else
  if (symlink(target, linkname) == 0)
    return 1;
  return errno == EPERM || errno == ENOSYS || errno == EOPNOTSUPP ? 0 : -1;
#endif
}


static int test_symbolic_link_reader(const char *filename)
{
  char *linkname, *contents;
  int link_status;
  size_t count=99, length=strlen(filename);

  if (length > SIZE_MAX-6)
    return 20;
  linkname = (char *)malloc(length+6);
  if (linkname == NULL)
    return 21;
  memcpy(linkname, filename, length);
  memcpy(linkname+length, ".link", 6);
  remove(linkname);
  link_status = create_symbolic_link(linkname, filename);
  if (link_status < 0) {
    free(linkname);
    return 22;
  }
  if (link_status == 1) {
    contents = gk_creadfilebin(linkname, &count);
    if (contents == NULL || count != 0) {
      gk_free((void **)&contents, LTERM);
      remove(linkname);
      free(linkname);
      return 23;
    }
    gk_free((void **)&contents, LTERM);
    if (remove(linkname) != 0) {
      free(linkname);
      return 24;
    }
  }
  else {
    printf("GKLIB_LINK_TEST_SKIPPED: symbolic links are unavailable\n");
  }
  free(linkname);
  return 0;
}


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
    if (gk_ ## prefix ## writefilebin(filename, 0, NULL) != 0) \
      return 15; \
    count = 99; \
    actual = gk_ ## prefix ## readfilebin(filename, &count); \
    if (actual == NULL || count != 0) { \
      gk_free((void **)&actual, LTERM); \
      return 16; \
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
    if (test_symbolic_link_reader(filename) != 0)
      return 4;
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
