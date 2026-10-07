#include <GKlib.h>
#include "../src/io_internal.h"


static int write_bytes(const char *filename, const void *data, size_t size)
{
  FILE *stream = fopen(filename, "wb");

  if (stream == NULL)
    return 0;
  if (fwrite(data, 1, size, stream) != size) {
    fclose(stream);
    return 0;
  }
  return fclose(stream) == 0;
}


static int expect_fopen_signal(char *filename, char *mode, int error)
{
  volatile int signum=0;

  if (!gk_sigtrap())
    return 0;
  gk_set_exit_on_error(1);
  errno = 0;
  switch (gk_sigcatch()) {
    case 0:
      (void)gk_fopen(filename, mode, "signal contract");
      break;
    case SIGERR:
      signum = SIGERR;
      break;
    default:
      signum = SIGMEM;
      break;
  }
  if (!gk_siguntrap())
    return 0;
  gk_set_exit_on_error(0);

  return signum == SIGERR && errno == error;
}


int main(void)
{
  static const unsigned char stats_data[] = {
    'a', ' ', 'b', '\0', 'c', '\n', '\r', '\n', 'z'
  };
  static const unsigned char nul_line[] = {'a', '\0', 'b', '\n'};
  static const unsigned char nul_integer[] = {'1', '\0', '2', '\n'};
  char *binary;
  char **lines;
  int32_t *integers;
  int64_t *large_integers;
  ssize_t *offsets;
  size_t nbytes, nlines, ntokens, max_tokens;
  FILE *stream;

  gk_set_exit_on_error(0);
  errno = 0;
  if (gk_read(-1, NULL, (size_t)PTRDIFF_MAX+1) != -1 ||
      errno != EOVERFLOW)
    return 34;
  errno = 0;
  if (gk_write(-1, NULL, (size_t)PTRDIFF_MAX+1) != -1 ||
      errno != EOVERFLOW)
    return 35;
  errno = 0;
  if (gk_fopen(NULL, (char *)"r", "null filename") != NULL ||
      errno != EINVAL)
    return 25;
  errno = 0;
  if (gk_fopen((char *)"unused", NULL, "null mode") != NULL ||
      errno != EINVAL)
    return 26;
  if (!expect_fopen_signal(NULL, (char *)"r", EINVAL))
    return 75;
  if (!expect_fopen_signal((char *)"gklib-missing-file",
                           (char *)"rb", ENOENT))
    return 76;
  nlines = 99;
  errno = 0;
  if (gk_creadfilebin(NULL, &nlines) != NULL || nlines != 0 ||
      errno != EINVAL)
    return 39;
  if (gk_mkpath((char *)"gklib-binary-directory") != 0)
    return 63;
  nlines = 99;
  errno = 0;
  binary = gk_creadfilebin((char *)"gklib-binary-directory", &nlines);
  if (binary != NULL || nlines != 0 || errno == 0) {
    gk_free((void **)&binary, LTERM);
    gk_rmpath((char *)"gklib-binary-directory");
    return 64;
  }
  if (gk_rmpath((char *)"gklib-binary-directory") != 0)
    return 65;
  if (!write_bytes("gklib-empty.txt", "", 0))
    return 66;
  lines = gk_readfile((char *)"gklib-empty.txt", &nlines);
  if (lines == NULL || nlines != 0)
    return 67;
  gk_free((void **)&lines, LTERM);
  integers = gk_i32readfile((char *)"gklib-empty.txt", &nlines);
  if (integers == NULL || nlines != 0)
    return 68;
  gk_free((void **)&integers, LTERM);
  large_integers = gk_i64readfile((char *)"gklib-empty.txt", &nlines);
  if (large_integers == NULL || nlines != 0)
    return 69;
  gk_free((void **)&large_integers, LTERM);
  offsets = gk_zreadfile((char *)"gklib-empty.txt", &nlines);
  if (offsets == NULL || nlines != 0)
    return 70;
  gk_free((void **)&offsets, LTERM);
  if (!write_bytes("gklib-final-fragment.txt", "last record", 11))
    return 1;
  lines = gk_readfile((char *)"gklib-final-fragment.txt", &nlines);
  if (lines == NULL || nlines != 1 || strcmp(lines[0], "last record") != 0)
    return 2;
  gk_free((void **)&lines[0], (void **)&lines, LTERM);
  if (!write_bytes("gklib-newline-records.txt", "one\ntwo\n", 8))
    return 3;
  lines = gk_readfile((char *)"gklib-newline-records.txt", &nlines);
  if (lines == NULL || nlines != 2 || strcmp(lines[0], "one") != 0 ||
      strcmp(lines[1], "two") != 0)
    return 4;
  gk_free((void **)&lines[0], (void **)&lines[1], (void **)&lines, LTERM);

  if (!write_bytes("gklib-stats.bin", stats_data, sizeof(stats_data)))
    return 5;
  gk_getfilestats((char *)"gklib-stats.bin", &nlines, &ntokens,
                  &max_tokens, &nbytes);
  if (nlines != 2 || ntokens != 3 || max_tokens != 2 ||
      nbytes != sizeof(stats_data))
    return 6;

  stream = fopen("gklib-long-line.txt", "wb");
  if (stream == NULL)
    return 7;
  for (nbytes=0; nbytes<4097; nbytes++)
    if (fputc('x', stream) == EOF)
      return 8;
  if (fclose(stream) != 0)
    return 9;
  stream = fopen("gklib-long-line.txt", "rb");
  if (stream == NULL)
    return 10;
  {
    char *line = (char *)malloc(2);
    size_t capacity = 2;
    if (!gk_malloc_init())
      return 11;
    if (line == NULL || gk_getline(&line, &capacity, stream) != 4097 ||
        capacity < 4098 || line[4097] != '\0')
      return 12;
    free(line);
    if (gk_GetCurMemoryUsed() != 0)
      return 13;
    gk_malloc_cleanup(0);
  }
  if (fclose(stream) != 0)
    return 14;
  stream = fopen("gklib-long-line.txt", "rb");
  if (stream == NULL)
    return 15;
  {
    char *line = (char *)malloc(1);
    size_t capacity = 1;
    if (line == NULL || gk_getline(&line, &capacity, stream) != 4097)
      return 16;
    free(line);
  }
  if (fclose(stream) != 0)
    return 17;

  /* Exercise the fallback independently of the configured system getline. */
  stream = fopen("gklib-long-line.txt", "rb");
  if (stream == NULL)
    return 40;
  {
    char *line = NULL;
    size_t capacity = 0;

    if (gk_getline_fallback(&line, &capacity, stream) != 4097 ||
        capacity < 4098 || line == NULL || line[4097] != '\0') {
      free(line);
      fclose(stream);
      return 41;
    }
    free(line);
  }
  if (fclose(stream) != 0)
    return 42;

  /* The fallback also accepts a non-NULL buffer with zero advertised size.
     System getline may allocate a replacement without reclaiming that input. */
  stream = fopen("gklib-long-line.txt", "rb");
  if (stream == NULL)
    return 71;
  {
    char *line = (char *)malloc(1);
    size_t capacity = 0;

    if (line == NULL || gk_getline_fallback(&line, &capacity, stream) != 4097 ||
        capacity < 4098 || line[4097] != '\0') {
      free(line);
      fclose(stream);
      return 72;
    }
    free(line);
  }
  if (fclose(stream) != 0)
    return 73;

  stream = fopen("gklib-getline-error.txt", "wb");
  if (stream == NULL)
    return 36;
  {
    char *line = NULL;
    size_t capacity = 0;
    if (gk_getline(&line, &capacity, stream) != -1 || !ferror(stream)) {
      free(line);
      fclose(stream);
      return 37;
    }
    free(line);
  }
  if (fclose(stream) != 0)
    return 38;

  stream = fopen("gklib-getline-error.txt", "wb");
  if (stream == NULL)
    return 43;
  {
    char *line = NULL;
    size_t capacity = 0;

    if (gk_getline_fallback(&line, &capacity, stream) != -1 ||
        !ferror(stream)) {
      free(line);
      fclose(stream);
      return 44;
    }
    free(line);
  }
  if (fclose(stream) != 0)
    return 45;

  if (!write_bytes("gklib-invalid-integer.txt", "12junk", 6))
    return 18;
  nlines = 99;
  integers = gk_i32readfile((char *)"gklib-invalid-integer.txt", &nlines);
  if (integers != NULL || nlines != 0)
    return 19;
  stream = fopen("gklib-invalid-integer.txt", "wb");
  if (stream == NULL)
    return 71;
  for (nbytes=0; nbytes<256; nbytes++) {
    if (fputc('9', stream) == EOF) {
      fclose(stream);
      return 72;
    }
  }
  if (fputc('\n', stream) == EOF || fclose(stream) != 0)
    return 73;
  nlines = 99;
  errno = 0;
  integers = gk_i32readfile((char *)"gklib-invalid-integer.txt", &nlines);
  if (integers != NULL || nlines != 0 || errno != EINVAL)
    return 74;

  if (!write_bytes("gklib-final-integers.txt", "42", 2))
    return 58;
  integers = gk_i32readfile((char *)"gklib-final-integers.txt", &nlines);
  if (integers == NULL || nlines != 1 || integers[0] != 42)
    return 59;
  gk_free((void **)&integers, LTERM);
  large_integers = gk_i64readfile((char *)"gklib-final-integers.txt",
                                  &nlines);
  if (large_integers == NULL || nlines != 1 || large_integers[0] != 42)
    return 60;
  gk_free((void **)&large_integers, LTERM);
  offsets = gk_zreadfile((char *)"gklib-final-integers.txt", &nlines);
  if (offsets == NULL || nlines != 1 || offsets[0] != 42)
    return 61;
  gk_free((void **)&offsets, LTERM);
  {
    char upper[] = {(char)0x80, 'a', '\0'};
    char lower[] = {(char)0x80, 'A', '\0'};

    if (gk_strtoupper(upper) != upper ||
        (unsigned char)upper[0] != 0x80 || upper[1] != 'A' ||
        gk_strtolower(lower) != lower ||
        (unsigned char)lower[0] != 0x80 || lower[1] != 'a' ||
        !gk_strcasecmp(upper, lower))
      return 62;
  }

  if (!write_bytes("gklib-i32-boundaries.txt",
      "-2147483648\n2147483647\n", 23))
    return 46;
  integers = gk_i32readfile((char *)"gklib-i32-boundaries.txt", &nlines);
  if (integers == NULL || nlines != 2 || integers[0] != INT32_MIN ||
      integers[1] != INT32_MAX)
    return 47;
  gk_free((void **)&integers, LTERM);
  if (!write_bytes("gklib-i32-boundaries.txt", "2147483648\n", 11))
    return 48;
  nlines = 99;
  integers = gk_i32readfile((char *)"gklib-i32-boundaries.txt", &nlines);
  if (integers != NULL || nlines != 0)
    return 49;
  if (!write_bytes("gklib-i32-boundaries.txt", "-2147483649\n", 12))
    return 50;
  nlines = 99;
  integers = gk_i32readfile((char *)"gklib-i32-boundaries.txt", &nlines);
  if (integers != NULL || nlines != 0)
    return 51;

  if (!write_bytes("gklib-i64-boundaries.txt",
      "-9223372036854775808\n9223372036854775807\n", 41))
    return 52;
  large_integers = gk_i64readfile((char *)"gklib-i64-boundaries.txt",
                                  &nlines);
  if (large_integers == NULL || nlines != 2 ||
      large_integers[0] != INT64_MIN || large_integers[1] != INT64_MAX)
    return 53;
  gk_free((void **)&large_integers, LTERM);
  if (!write_bytes("gklib-i64-boundaries.txt", "9223372036854775808\n", 20))
    return 54;
  nlines = 99;
  large_integers = gk_i64readfile((char *)"gklib-i64-boundaries.txt",
                                  &nlines);
  if (large_integers != NULL || nlines != 0)
    return 55;
  if (!write_bytes("gklib-i64-boundaries.txt", "-9223372036854775809\n", 21))
    return 56;
  nlines = 99;
  large_integers = gk_i64readfile((char *)"gklib-i64-boundaries.txt",
                                  &nlines);
  if (large_integers != NULL || nlines != 0)
    return 57;

  if (!write_bytes("gklib-nul-line.txt", nul_line, sizeof(nul_line)))
    return 27;
  lines = gk_readfile((char *)"gklib-nul-line.txt", &nlines);
  if (lines != NULL || nlines != 0)
    return 28;
  if (!write_bytes("gklib-nul-integer.txt", nul_integer,
                   sizeof(nul_integer)))
    return 29;
  integers = gk_i32readfile((char *)"gklib-nul-integer.txt", &nlines);
  if (integers != NULL || nlines != 0)
    return 30;

  if (gk_mkpath((char *)"gklib path & literal/subdir") != 0 ||
      !gk_dexists((char *)"gklib path & literal/subdir"))
    return 20;
  if (!write_bytes("gklib path & literal/subdir/file.txt", "x", 1))
    return 21;
  if (gk_rmpath((char *)"gklib path & literal") != 0 ||
      gk_dexists((char *)"gklib path & literal"))
    return 22;
  if (write_bytes("gklib-ordinary-file.txt", "x", 1) == 0 ||
      gk_rmpath((char *)"gklib-ordinary-file.txt") != 0 ||
      gk_fexists((char *)"gklib-ordinary-file.txt"))
    return 23;
  if (gk_rmpath((char *)".") != -1 || errno != EINVAL)
    return 24;
  if (gk_mkpath((char *)"gklib-safe") != 0 ||
      !write_bytes("gklib-victim", "x", 1))
    return 31;
  errno = 0;
  if (gk_rmpath((char *)"gklib-safe/../gklib-victim") != -1 ||
      errno != EINVAL || !gk_fexists((char *)"gklib-victim"))
    return 32;
  if (gk_rmpath((char *)"gklib-safe") != 0 ||
      gk_rmpath((char *)"gklib-victim") != 0)
    return 33;

  remove("gklib-final-fragment.txt");
  remove("gklib-newline-records.txt");
  remove("gklib-stats.bin");
  remove("gklib-long-line.txt");
  remove("gklib-invalid-integer.txt");
  remove("gklib-final-integers.txt");
  remove("gklib-i32-boundaries.txt");
  remove("gklib-i64-boundaries.txt");
  remove("gklib-nul-line.txt");
  remove("gklib-nul-integer.txt");
  remove("gklib-getline-error.txt");
  remove("gklib-empty.txt");
  return 0;
}
