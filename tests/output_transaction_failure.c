#include <GKlib.h>

#ifdef _WIN32
#include <windows.h>
#endif


static int fail_flush;
static int fail_close;
static int fail_commit;
static int close_calls;


static int test_fflush(FILE *stream)
{
  if (fail_flush) {
    errno = ENOSPC;
    return EOF;
  }
  return fflush(stream);
}


static int test_fclose(FILE *stream)
{
  int status;

  close_calls++;
  status = fclose(stream);
  if (fail_close && status == 0) {
    errno = EIO;
    return EOF;
  }
  return status;
}


#ifdef _WIN32
static BOOL test_replace_file(LPCSTR replaced, LPCSTR replacement,
    LPCSTR backup, DWORD flags, LPVOID exclude, LPVOID reserved)
{
  if (fail_commit) {
    SetLastError(ERROR_ACCESS_DENIED);
    return FALSE;
  }
  return ReplaceFileA(replaced, replacement, backup, flags, exclude, reserved);
}


static BOOL test_move_file(LPCSTR existing, LPCSTR destination, DWORD flags)
{
  return MoveFileExA(existing, destination, flags);
}
#else
static int test_rename(const char *old_name, const char *new_name)
{
  if (fail_commit) {
    errno = EACCES;
    return -1;
  }
  return rename(old_name, new_name);
}
#endif


#define fflush test_fflush
#define fclose test_fclose
#ifdef _WIN32
#define ReplaceFileA test_replace_file
#define MoveFileExA test_move_file
#else
#define rename test_rename
#endif
#include "../src/io_internal.h"
#undef fflush
#undef fclose
#ifdef _WIN32
#undef ReplaceFileA
#undef MoveFileExA
#else
#undef rename
#endif


static char *copy_name(const char *filename)
{
  char *result;
  size_t length=strlen(filename);

  result = (char *)malloc(length+1);
  if (result != NULL)
    memcpy(result, filename, length+1);
  return result;
}


static int write_contents(const char *filename, const char *contents)
{
  FILE *stream;
  size_t length=strlen(contents);

  stream = fopen(filename, "wb");
  if (stream == NULL)
    return 0;
  if (fwrite(contents, 1, length, stream) != length) {
    fclose(stream);
    return 0;
  }
  return fclose(stream) == 0;
}


static int contents_equal(const char *filename, const char *expected)
{
  char buffer[64];
  FILE *stream;
  size_t count, length=strlen(expected);

  stream = fopen(filename, "rb");
  if (stream == NULL)
    return 0;
  count = fread(buffer, 1, sizeof(buffer), stream);
  if (fclose(stream) != 0)
    return 0;
  return count == length && memcmp(buffer, expected, length) == 0;
}


static int prepare_transaction(const char *target, const char *temporary,
    FILE **r_stream, char **r_tempname)
{
  *r_stream = NULL;
  *r_tempname = NULL;
  if (!write_contents(target, "keep"))
    return 0;
  *r_stream = fopen(temporary, "wb");
  *r_tempname = copy_name(temporary);
  if (*r_stream == NULL || *r_tempname == NULL)
    return 0;
  return fwrite("replace", 1, 7, *r_stream) == 7;
}


static int transaction_preserved(const char *target, const char *temporary,
    char *tempname)
{
  return tempname == NULL && !gk_fexists((char *)temporary) &&
      contents_equal(target, "keep");
}


int main(void)
{
  const char *target="gklib-transaction-target";
  const char *temporary="gklib-transaction-temporary";
  char *tempname=NULL;
  FILE *stream=NULL;
  int result=1;

  remove(target);
  remove(temporary);

  /* A short public write is represented by complete == 0. */
  if (!prepare_transaction(target, temporary, &stream, &tempname))
    goto cleanup;
  errno = 0;
  if (gk_finish_output_file(stream, &tempname, target, 0) || errno != EIO ||
      !transaction_preserved(target, temporary, tempname))
    goto cleanup;
  stream = NULL;

  /* The first stream error wins even when close reports a second error. */
  if (!prepare_transaction(target, temporary, &stream, &tempname))
    goto cleanup;
  fail_flush = 1;
  fail_close = 1;
  close_calls = 0;
  errno = 0;
  if (gk_finish_output_file(stream, &tempname, target, 1) ||
      errno != ENOSPC || close_calls != 1 ||
      !transaction_preserved(target, temporary, tempname))
    goto cleanup;
  stream = NULL;
  fail_flush = 0;
  fail_close = 0;

  if (!prepare_transaction(target, temporary, &stream, &tempname))
    goto cleanup;
  fail_close = 1;
  errno = 0;
  if (gk_finish_output_file(stream, &tempname, target, 1) || errno != EIO ||
      !transaction_preserved(target, temporary, tempname))
    goto cleanup;
  stream = NULL;
  fail_close = 0;

  if (!prepare_transaction(target, temporary, &stream, &tempname))
    goto cleanup;
  fail_commit = 1;
  errno = 0;
  if (gk_finish_output_file(stream, &tempname, target, 1) ||
      errno != EACCES ||
      !transaction_preserved(target, temporary, tempname))
    goto cleanup;
  stream = NULL;
  fail_commit = 0;

  result = 0;

cleanup:
  if (stream != NULL)
    fclose(stream);
  free(tempname);
  remove(temporary);
  remove(target);
  return result;
}
