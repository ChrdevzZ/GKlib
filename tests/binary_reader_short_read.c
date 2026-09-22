/* Compile the included io.c definitions locally while retaining the configured
   shared-library accessors for state that remains owned by GKlib. */
#define GKLIB_EXPORT
#include <GKlib.h>
#include "../src/memory_internal.h"


static FILE *opened_stream;
static int close_count;
static int fake_complete_read;
static int fake_short_write;
static size_t signal_count;
static int fail_allocation;


static void *test_malloc_nosignal(size_t nbytes)
{
  if (fail_allocation) {
    errno = ENOMEM;
    return NULL;
  }
  return gk_malloc(nbytes, "binary reader test");
}


static void *test_realloc_nosignal(void *oldptr, size_t nbytes)
{
  return gk_realloc(oldptr, nbytes, "binary reader test");
}


static FILE *test_fopen(const char *filename, const char *mode)
{
  opened_stream = fopen(filename, mode);
  return opened_stream;
}


static size_t test_fread(void *buffer, size_t size, size_t count, FILE *stream)
{
  (void)buffer;
  (void)size;
  (void)stream;
  return fake_complete_read ? count : 0;
}


static size_t test_fwrite(const void *buffer, size_t size, size_t count,
    FILE *stream)
{
  if (fake_short_write)
    return 0;
  return fwrite(buffer, size, count, stream);
}


static int test_fclose(FILE *stream)
{
  close_count++;
  if (stream == opened_stream)
    opened_stream = NULL;
  return fclose(stream);
}


#define fopen  test_fopen
#define fread  test_fread
#define fwrite test_fwrite
#define fclose test_fclose
#define gk_malloc_nosignal test_malloc_nosignal
#define gk_realloc_nosignal test_realloc_nosignal
#include "../src/io.c"
#undef fopen
#undef fread
#undef fwrite
#undef fclose
#undef gk_malloc_nosignal
#undef gk_realloc_nosignal


static void *read_file(char *filename, int which, size_t *count)
{
  switch (which) {
    case 0: return gk_creadfilebin(filename, count);
    case 1: return gk_i32readfilebin(filename, count);
    case 2: return gk_i64readfilebin(filename, count);
    case 3: return gk_zreadfilebin(filename, count);
    case 4: return gk_freadfilebin(filename, count);
    default: return gk_dreadfilebin(filename, count);
  }
}


static int check_returning_error(char *filename, int which)
{
  void *result;
  size_t count = 99;

  close_count = 0;
  opened_stream = NULL;
  result = read_file(filename, which, &count);
  if (result != NULL || count != 0 || close_count != 1 ||
      opened_stream != NULL)
    return 10 + which;
  return 0;
}


static int check_unconsumed_data(char *filename)
{
  void *result;
  size_t count = 99;

  close_count = 0;
  fake_complete_read = 1;
  opened_stream = NULL;
  result = read_file(filename, 0, &count);
  fake_complete_read = 0;
  if (result != NULL || count != 0 || close_count != 1 ||
      opened_stream != NULL)
    return 19;
  return 0;
}


static int check_signal_recovery(char *filename, int which)
{
  int signum;

  close_count = 0;
  opened_stream = NULL;
  signal_count = 99;
  if (!gk_malloc_init() || !gk_sigtrap())
    return 20 + which;

  switch (gk_sigcatch()) {
    case 0:
      (void)read_file(filename, which, &signal_count);
      return 30 + which;
    case SIGERR:
      signum = SIGERR;
      break;
    default:
      signum = SIGMEM;
      break;
  }

  if (!gk_siguntrap())
    return 40 + which;
  gk_malloc_cleanup(0);
  if (signum != SIGERR || signal_count != 0 || close_count != 1 ||
      opened_stream != NULL)
    return 50 + which;
  return 0;
}


static int check_allocation_failure(char *filename, int signal_errors)
{
  int signum=0;
  void *result=NULL;

  close_count = 0;
  opened_stream = NULL;
  signal_count = 99;
  fail_allocation = 1;
  if (!signal_errors) {
    errno = 0;
    result = read_file(filename, 0, &signal_count);
  }
  else {
    if (!gk_malloc_init() || !gk_sigtrap())
      return 70;
    switch (gk_sigcatch()) {
      case 0:
        (void)read_file(filename, 0, &signal_count);
        return 71;
      case SIGMEM:
        signum = SIGMEM;
        break;
      default:
        signum = SIGERR;
        break;
    }
    if (!gk_siguntrap())
      return 72;
    gk_malloc_cleanup(0);
  }
  fail_allocation = 0;

  if ((!signal_errors && (result != NULL || errno != ENOMEM)) ||
      (signal_errors && signum != SIGMEM) || signal_count != 0 ||
      close_count != 1 || opened_stream != NULL)
    return 73;
  return 0;
}


static int check_failed_write_preserves_destination(char *filename)
{
  unsigned char value=1;
  char preserved[5]={0};
  FILE *stream;

  stream = fopen(filename, "wb");
  if (stream == NULL || fwrite("keep", 1, 4, stream) != 4 ||
      fclose(stream) != 0)
    return 60;

  close_count = 0;
  fake_short_write = 1;
  if (gk_cwritefilebin(filename, 1, (char *)&value) != 0) {
    fake_short_write = 0;
    return 61;
  }
  fake_short_write = 0;
  if (close_count != 1)
    return 62;

  stream = fopen(filename, "rb");
  if (stream == NULL || fread(preserved, 1, 4, stream) != 4 ||
      fclose(stream) != 0 || strcmp(preserved, "keep") != 0)
    return 63;

  return 0;
}


int main(void)
{
  char filename[] = "binary-reader-short-read.bin";
  unsigned char bytes[8] = {0};
  FILE *stream;
  int i, status;

  stream = fopen(filename, "wb");
  if (stream == NULL || fwrite(bytes, 1, sizeof(bytes), stream) != sizeof(bytes))
    return 1;
  if (fclose(stream) != 0)
    return 2;

  gk_set_exit_on_error(0);
  for (i=0; i<6; i++) {
    status = check_returning_error(filename, i);
    if (status != 0)
      return status;
  }
  status = check_unconsumed_data(filename);
  if (status != 0)
    return status;
  status = check_allocation_failure(filename, 0);
  if (status != 0)
    return status;

  gk_set_exit_on_error(1);
  for (i=0; i<6; i++) {
    status = check_signal_recovery(filename, i);
    if (status != 0)
      return status;
  }
  status = check_allocation_failure(filename, 1);
  if (status != 0)
    return status;
  gk_set_exit_on_error(0);
  status = check_failed_write_preserves_destination(filename);
  if (status != 0)
    return status;

  if (remove(filename) != 0)
    return 3;
  return 0;
}
