#include <GKlib.h>

#ifndef _WIN32
#include <unistd.h>
#endif


static int fail_next_malloc;
static int reallocs_before_failure=-1;


static void *test_malloc(size_t nbytes)
{
  if (fail_next_malloc) {
    fail_next_malloc = 0;
    errno = ENOMEM;
    return NULL;
  }
  return malloc(nbytes);
}


static void *test_realloc(void *pointer, size_t nbytes)
{
  if (reallocs_before_failure >= 0) {
    if (reallocs_before_failure == 0) {
      reallocs_before_failure = -1;
      errno = ENOMEM;
      return NULL;
    }
    reallocs_before_failure--;
  }
  return realloc(pointer, nbytes);
}


#define malloc  test_malloc
#define realloc test_realloc
#include "../src/io_internal.h"
#undef malloc
#undef realloc


static FILE *open_input(const char *contents)
{
  FILE *stream;
  size_t length=strlen(contents);
#ifndef _WIN32
  char name[] = "getline-fallback-XXXXXX";
  int fd;
#endif

#ifdef _WIN32
  stream = tmpfile();
#else
  /* Create exclusively in the binary working directory, then remove the name. */
  fd = mkstemp(name);
  if (fd < 0)
    return NULL;
  if (unlink(name) != 0) {
    close(fd);
    return NULL;
  }
  stream = fdopen(fd, "w+b");
  if (stream == NULL)
    close(fd);
#endif
  if (stream == NULL)
    return NULL;
  if (fwrite(contents, 1, length, stream) != length ||
      fflush(stream) != 0 || fseek(stream, 0, SEEK_SET) != 0) {
    fclose(stream);
    return NULL;
  }
  return stream;
}


int main(void)
{
  char *line, *old_line;
  size_t capacity;
  FILE *stream;

  stream = open_input("abc\n");
  if (stream == NULL)
    return 1;
  line = NULL;
  capacity = 77;
  fail_next_malloc = 1;
  errno = 0;
  if (gk_getline_fallback(&line, &capacity, stream) != -1 ||
      errno != ENOMEM || line != NULL || capacity != 77) {
    free(line);
    fclose(stream);
    return 2;
  }
  fclose(stream);

  stream = open_input("abc\n");
  if (stream == NULL)
    return 3;
  line = (char *)malloc(2);
  if (line == NULL) {
    fclose(stream);
    return 4;
  }
  old_line = line;
  capacity = 2;
  reallocs_before_failure = 0;
  errno = 0;
  if (gk_getline_fallback(&line, &capacity, stream) != -1 ||
      errno != ENOMEM || line != old_line || capacity != 2) {
    free(line);
    fclose(stream);
    return 5;
  }
  free(line);
  fclose(stream);

  stream = open_input("abcdef\n");
  if (stream == NULL)
    return 6;
  line = (char *)malloc(2);
  if (line == NULL) {
    fclose(stream);
    return 7;
  }
  capacity = 2;
  reallocs_before_failure = 1;
  errno = 0;
  if (gk_getline_fallback(&line, &capacity, stream) != -1 ||
      errno != ENOMEM || line == NULL || capacity != 4 ||
      memcmp(line, "abc", 3) != 0) {
    free(line);
    fclose(stream);
    return 8;
  }
  free(line);
  fclose(stream);

  stream = open_input("x\n");
  if (stream == NULL)
    return 9;
  line = (char *)malloc(1);
  if (line == NULL) {
    fclose(stream);
    return 10;
  }
  old_line = line;
  capacity = 0;
  reallocs_before_failure = 0;
  errno = 0;
  if (gk_getline_fallback(&line, &capacity, stream) != -1 ||
      errno != ENOMEM || line != old_line || capacity != 0) {
    free(line);
    fclose(stream);
    return 11;
  }
  free(line);
  fclose(stream);

  return 0;
}
