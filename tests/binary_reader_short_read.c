/* Compile the included io.c definitions locally while retaining the configured
   shared-library accessors for state that remains owned by GKlib. */
#define GKLIB_EXPORT
#include <GKlib.h>


static FILE *opened_stream;
static int close_count;
static size_t signal_count;


static FILE *test_fopen(const char *filename, const char *mode)
{
  opened_stream = fopen(filename, mode);
  return opened_stream;
}


static size_t test_fread(void *buffer, size_t size, size_t count, FILE *stream)
{
  (void)buffer;
  (void)size;
  (void)count;
  (void)stream;
  return 0;
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
#define fclose test_fclose
#include "../src/io.c"
#undef fopen
#undef fread
#undef fclose


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


static int check_signal_recovery(char *filename, int which)
{
  int signum;

  close_count = 0;
  opened_stream = NULL;
  signal_count = 99;
  if (!gk_malloc_init() || !gk_sigtrap())
    return 20 + which;

  signum = gk_sigcatch();
  if (signum == 0) {
    (void)read_file(filename, which, &signal_count);
    return 30 + which;
  }

  if (!gk_siguntrap())
    return 40 + which;
  gk_malloc_cleanup(0);
  if (signum != SIGERR || signal_count != 0 || close_count != 1 ||
      opened_stream != NULL)
    return 50 + which;
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

  gk_set_exit_on_error(1);
  for (i=0; i<6; i++) {
    status = check_signal_recovery(filename, i);
    if (status != 0)
      return status;
  }
  gk_set_exit_on_error(0);

  if (remove(filename) != 0)
    return 3;
  return 0;
}
