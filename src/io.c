/*!
\file  io.c
\brief Various file I/O functions.

This file contains various functions that perform I/O.

\date Started 4/10/95
\author George
\version\verbatim $Id: io.c 18951 2015-08-08 20:10:46Z karypis $ \endverbatim
*/

#ifdef HAVE_GETLINE
/* Get getline to be defined. */
#define _GNU_SOURCE
#include <stdio.h>
#undef _GNU_SOURCE
#endif

#include <GKlib.h>
#include "io_internal.h"
#include "memory_internal.h"

/*************************************************************************
* This function opens a file
**************************************************************************/
FILE *gk_fopen(char *fname, char *mode, const char *msg)
{
  int saved_errno;
  FILE *fp;

  if (fname == NULL || mode == NULL) {
    errno = EINVAL;
    fprintf(stderr, "file: %s, mode: %s, [%s]: invalid argument\n",
            fname != NULL ? fname : "(null)",
            mode != NULL ? mode : "(null)", msg != NULL ? msg : "");
    errno = EINVAL;
    gk_errexit(SIGERR, "Failed on gk_fopen()");
    errno = EINVAL;
    return NULL;
  }

  fp = fopen(fname, mode);
  if (fp != NULL)
    return fp;

  saved_errno = errno != 0 ? errno : EIO;
  fprintf(stderr, "file: %s, mode: %s, [%s]: ",
          fname != NULL ? fname : "(null)",
          mode != NULL ? mode : "(null)", msg != NULL ? msg : "");
  errno = saved_errno;
  perror("");
  errno = saved_errno;
  gk_errexit(saved_errno == ENOMEM || saved_errno == EOVERFLOW ?
      SIGMEM : SIGERR, "Failed on gk_fopen()");
  errno = saved_errno;

  return NULL;
}


/*************************************************************************
* This function closes a file
**************************************************************************/
void gk_fclose(FILE *fp)
{
  if (fp != NULL)
    fclose(fp);
}


/*************************************************************************/
/*! This function is a wrapper around the read() function that ensures 
    that all data is been read, by issuing multiple read requests.
    The only time when not 'count' items are read is when the EOF has been
    reached.
*/
/*************************************************************************/
ssize_t gk_read(int fd, void *vbuf, size_t count)
{
  char *buf = (char *)vbuf;
  ssize_t rsize;
  size_t request, remaining=count;

  if (count > (size_t)PTRDIFF_MAX) {
    errno = EOVERFLOW;
    return -1;
  }

  do {
    request = remaining > (size_t)INT_MAX ? (size_t)INT_MAX : remaining;
    if ((rsize = read(fd, buf, request)) == -1)
      return -1;
    buf   += rsize;
    remaining -= (size_t)rsize;
  } while (remaining > 0 && rsize > 0);

  return (ssize_t)(count-remaining);
}


/*************************************************************************/
/*! This function is a wrapper around the write() function that ensures 
    that all data is been written, by issueing multiple write requests.
*/
/*************************************************************************/
ssize_t gk_write(int fd, void *vbuf, size_t count)
{
  char *buf = (char *)vbuf;
  ssize_t size;
  size_t request, remaining=count;

  if (count > (size_t)PTRDIFF_MAX) {
    errno = EOVERFLOW;
    return -1;
  }

  do {
    request = remaining > (size_t)INT_MAX ? (size_t)INT_MAX : remaining;
    if ((size = write(fd, buf, request)) == -1)
      return -1;
    if (size == 0 && remaining > 0) {
      errno = EIO;
      return -1;
    }
    buf   += size;
    remaining -= (size_t)size;
  } while (remaining > 0);

  return (ssize_t)count;
}


/*************************************************************************/
/*! This function is the GKlib implementation of glibc's getline()
    function.
    \returns -1 if the EOF has been reached, otherwise it returns the 
             number of bytes read.
*/
/*************************************************************************/
ssize_t gk_getline(char **lineptr, size_t *n, FILE *stream)
{
  if (lineptr == NULL || n == NULL || stream == NULL) {
    errno = EINVAL;
    return -1;
  }

  /* Make a clean EOF distinguishable from allocation and stream errors. */
  errno = 0;
#ifdef HAVE_GETLINE
  {
    ssize_t nread;

    nread = getline(lineptr, n, stream);
    if (nread < 0 && ferror(stream) && errno == 0)
      errno = EIO;
    return nread;
  }
#else
  return gk_getline_fallback(lineptr, n, stream);
#endif
}


/*************************************************************************/
/*! Grows a typed text-reader buffer using checked geometric expansion.

    The original pointer and capacity are retained when allocation fails.
    Arithmetic overflow sets errno to EOVERFLOW and returns zero.
*/
/*************************************************************************/
static int gk_grow_read_buffer(void **buffer, size_t *capacity,
    size_t needed, size_t element_size, int *r_allocation_failed)
{
  size_t new_capacity;
  void *new_buffer;

  if (*buffer != NULL && needed <= *capacity)
    return 1;

  new_capacity = *capacity == 0 ? 16 : *capacity;
  while (new_capacity < needed) {
    if (new_capacity > SIZE_MAX/2) {
      errno = EOVERFLOW;
      *r_allocation_failed = 1;
      return 0;
    }
    new_capacity *= 2;
  }
  if (new_capacity > SIZE_MAX/element_size) {
    errno = EOVERFLOW;
    *r_allocation_failed = 1;
    return 0;
  }

  new_buffer = gk_realloc_nosignal(*buffer, new_capacity*element_size);
  if (new_buffer == NULL) {
    *r_allocation_failed = 1;
    return 0;
  }

  *buffer = new_buffer;
  *capacity = new_capacity;
  return 1;
}


/*************************************************************************/
/*! Allocates the non-NULL result used for a successful empty text file. */
/*************************************************************************/
static void *gk_empty_read_result(size_t element_size, char *fname)
{
  int saved_errno;
  void *result;

  result = gk_malloc_nosignal(element_size);
  if (result != NULL)
    return result;

  saved_errno = errno != 0 ? errno : ENOMEM;
  errno = saved_errno;
  gk_errexit(SIGMEM, "Memory allocation failed while reading empty file %s.",
             fname != NULL ? fname : "(null)");
  errno = saved_errno;
  return NULL;
}


/*************************************************************************/
/*! Parses a line containing exactly one bounded decimal integer. */
/*************************************************************************/
static int gk_parse_integer_line(char *line, intmax_t minimum,
    intmax_t maximum, intmax_t *value)
{
  char *end;
  intmax_t parsed;

  errno = 0;
  parsed = strtoimax(line, &end, 10);
  if (end == line || errno == ERANGE || parsed < minimum || parsed > maximum) {
    errno = EINVAL;
    return 0;
  }
  while (isspace((unsigned char)*end))
    end++;
  if (*end != '\0') {
    errno = EINVAL;
    return 0;
  }

  *value = parsed;
  return 1;
}


/*************************************************************************/
/*! This function reads the contents of a text file and returns it in the
    form of an array of strings.
    \param fname is the name of the file
    \param r_nlines is the number of lines in the file. If it is NULL,
           this information is not returned.
*/
/*************************************************************************/
char **gk_readfile(char *fname, size_t *r_nlines)
{
  int allocation_failed=0, failed=0, saved_errno=0;
  ssize_t line_length;
  size_t capacity=0, lnlen=0, nlines=0, i, string_length;
  char *line=NULL, **lines=NULL;
  FILE *fpin=NULL;

  if (r_nlines != NULL)
    *r_nlines = 0;

  fpin = gk_fopen(fname, "r", "gk_readfile");
  if (fpin == NULL)
    return NULL;
  errno = 0;

  while ((line_length = gk_getline(&line, &lnlen, fpin)) != -1) {
    if (memchr(line, '\0', (size_t)line_length) != NULL) {
      errno = EINVAL;
      failed = 1;
      break;
    }
    if (nlines == SIZE_MAX) {
      errno = EOVERFLOW;
      allocation_failed = failed = 1;
      break;
    }
    if (!gk_grow_read_buffer((void **)&lines, &capacity, nlines+1,
                             sizeof(char *), &allocation_failed)) {
      failed = 1;
      break;
    }
    gk_strtprune(line, "\n\r");
    string_length = strlen(line);
    if (string_length == SIZE_MAX) {
      errno = EOVERFLOW;
      allocation_failed = failed = 1;
      break;
    }
    lines[nlines] = (char *)gk_malloc_nosignal(string_length+1);
    if (lines[nlines] == NULL) {
      allocation_failed = failed = 1;
      break;
    }
    memcpy(lines[nlines], line, string_length+1);
    nlines++;
  }
  if (!failed && line_length == -1 && !feof(fpin)) {
    failed = 1;
    if (errno == ENOMEM || errno == EOVERFLOW)
      allocation_failed = 1;
  }
  if (!failed && ferror(fpin))
    failed = 1;
  if (failed)
    saved_errno = errno != 0 ? errno :
        (allocation_failed ? ENOMEM : EIO);
  if (fclose(fpin) != 0) {
    if (!failed)
      saved_errno = errno != 0 ? errno : EIO;
    failed = 1;
  }

  free(line);

  if (failed) {
    for (i=0; i<nlines; i++)
      gk_free((void **)&lines[i], LTERM);
    gk_free((void **)&lines, LTERM);
    errno = saved_errno;
    gk_errexit(allocation_failed ? SIGMEM : SIGERR,
               "Failed while reading file %s.", fname);
    errno = saved_errno;
    return NULL;
  }

  if (r_nlines != NULL)
    *r_nlines = nlines;

  if (lines == NULL)
    lines = (char **)gk_empty_read_result(sizeof(char *), fname);

  return lines;
}


/*************************************************************************/
/*! This function reads the contents of a file and returns it in the
    form of an array of int32_t.
    \param fname is the name of the file
    \param r_nlines is the number of lines in the file. If it is NULL,
           this information is not returned.
*/
/*************************************************************************/
int32_t *gk_i32readfile(char *fname, size_t *r_nlines)
{
  int allocation_failed=0, failed=0, saved_errno=0;
  ssize_t line_length;
  intmax_t value;
  size_t capacity=0, lnlen=0, nlines=0;
  char *line=NULL;
  int32_t *array=NULL;
  FILE *fpin=NULL;

  if (r_nlines != NULL)
    *r_nlines = 0;

  fpin = gk_fopen(fname, "r", "gk_i32readfile");
  if (fpin == NULL)
    return NULL;
  errno = 0;

  while ((line_length = gk_getline(&line, &lnlen, fpin)) != -1) {
    if (memchr(line, '\0', (size_t)line_length) != NULL ||
        !gk_parse_integer_line(line, INT32_MIN, INT32_MAX, &value)) {
      if (errno == 0)
        errno = EINVAL;
      failed = 1;
      break;
    }
    if (nlines == SIZE_MAX) {
      errno = EOVERFLOW;
      allocation_failed = failed = 1;
      break;
    }
    if (!gk_grow_read_buffer((void **)&array, &capacity, nlines+1,
                             sizeof(int32_t), &allocation_failed)) {
      failed = 1;
      break;
    }
    array[nlines++] = (int32_t)value;
  }
  if (!failed && line_length == -1 && !feof(fpin)) {
    failed = 1;
    if (errno == ENOMEM || errno == EOVERFLOW)
      allocation_failed = 1;
  }
  if (!failed && ferror(fpin))
    failed = 1;
  if (failed)
    saved_errno = errno != 0 ? errno :
        (allocation_failed ? ENOMEM : EIO);
  if (fclose(fpin) != 0) {
    if (!failed)
      saved_errno = errno != 0 ? errno : EIO;
    failed = 1;
  }

  free(line);

  if (failed) {
    gk_free((void **)&array, LTERM);
    errno = saved_errno;
    gk_errexit(allocation_failed ? SIGMEM : SIGERR,
               "Invalid or unreadable integer file %s.", fname);
    errno = saved_errno;
    return NULL;
  }

  if (r_nlines != NULL)
    *r_nlines = nlines;

  if (array == NULL)
    array = (int32_t *)gk_empty_read_result(sizeof(int32_t), fname);

  return array;
}

/*************************************************************************/
/*! This function reads the contents of a file and returns it in the
    form of an array of int64_t.
    \param fname is the name of the file
    \param r_nlines is the number of lines in the file. If it is NULL,
           this information is not returned.
*/
/*************************************************************************/
int64_t *gk_i64readfile(char *fname, size_t *r_nlines)
{
  int allocation_failed=0, failed=0, saved_errno=0;
  ssize_t line_length;
  intmax_t value;
  size_t capacity=0, lnlen=0, nlines=0;
  char *line=NULL;
  int64_t *array=NULL;
  FILE *fpin=NULL;

  if (r_nlines != NULL)
    *r_nlines = 0;

  fpin = gk_fopen(fname, "r", "gk_i64readfile");
  if (fpin == NULL)
    return NULL;
  errno = 0;

  while ((line_length = gk_getline(&line, &lnlen, fpin)) != -1) {
    if (memchr(line, '\0', (size_t)line_length) != NULL ||
        !gk_parse_integer_line(line, INT64_MIN, INT64_MAX, &value)) {
      if (errno == 0)
        errno = EINVAL;
      failed = 1;
      break;
    }
    if (nlines == SIZE_MAX) {
      errno = EOVERFLOW;
      allocation_failed = failed = 1;
      break;
    }
    if (!gk_grow_read_buffer((void **)&array, &capacity, nlines+1,
                             sizeof(int64_t), &allocation_failed)) {
      failed = 1;
      break;
    }
    array[nlines++] = (int64_t)value;
  }
  if (!failed && line_length == -1 && !feof(fpin)) {
    failed = 1;
    if (errno == ENOMEM || errno == EOVERFLOW)
      allocation_failed = 1;
  }
  if (!failed && ferror(fpin))
    failed = 1;
  if (failed)
    saved_errno = errno != 0 ? errno :
        (allocation_failed ? ENOMEM : EIO);
  if (fclose(fpin) != 0) {
    if (!failed)
      saved_errno = errno != 0 ? errno : EIO;
    failed = 1;
  }

  free(line);

  if (failed) {
    gk_free((void **)&array, LTERM);
    errno = saved_errno;
    gk_errexit(allocation_failed ? SIGMEM : SIGERR,
               "Invalid or unreadable integer file %s.", fname);
    errno = saved_errno;
    return NULL;
  }

  if (r_nlines != NULL)
    *r_nlines  = nlines;

  if (array == NULL)
    array = (int64_t *)gk_empty_read_result(sizeof(int64_t), fname);

  return array;
}

/*************************************************************************/
/*! This function reads the contents of a file and returns it in the
    form of an array of ssize_t.
    \param fname is the name of the file
    \param r_nlines is the number of lines in the file. If it is NULL,
           this information is not returned.
*/
/*************************************************************************/
ssize_t *gk_zreadfile(char *fname, size_t *r_nlines)
{
  int allocation_failed=0, failed=0, saved_errno=0;
  ssize_t line_length;
  intmax_t value;
  size_t capacity=0, lnlen=0, nlines=0;
  char *line=NULL;
  ssize_t *array=NULL;
  FILE *fpin=NULL;

  if (r_nlines != NULL)
    *r_nlines = 0;

  fpin = gk_fopen(fname, "r", "gk_zreadfile");
  if (fpin == NULL)
    return NULL;
  errno = 0;

  while ((line_length = gk_getline(&line, &lnlen, fpin)) != -1) {
    if (memchr(line, '\0', (size_t)line_length) != NULL ||
        !gk_parse_integer_line(line, (intmax_t)(-PTRDIFF_MAX-1),
          (intmax_t)PTRDIFF_MAX, &value)) {
      if (errno == 0)
        errno = EINVAL;
      failed = 1;
      break;
    }
    if (nlines == SIZE_MAX) {
      errno = EOVERFLOW;
      allocation_failed = failed = 1;
      break;
    }
    if (!gk_grow_read_buffer((void **)&array, &capacity, nlines+1,
                             sizeof(ssize_t), &allocation_failed)) {
      failed = 1;
      break;
    }
    array[nlines++] = (ssize_t)value;
  }
  if (!failed && line_length == -1 && !feof(fpin)) {
    failed = 1;
    if (errno == ENOMEM || errno == EOVERFLOW)
      allocation_failed = 1;
  }
  if (!failed && ferror(fpin))
    failed = 1;
  if (failed)
    saved_errno = errno != 0 ? errno :
        (allocation_failed ? ENOMEM : EIO);
  if (fclose(fpin) != 0) {
    if (!failed)
      saved_errno = errno != 0 ? errno : EIO;
    failed = 1;
  }

  free(line);

  if (failed) {
    gk_free((void **)&array, LTERM);
    errno = saved_errno;
    gk_errexit(allocation_failed ? SIGMEM : SIGERR,
               "Invalid or unreadable integer file %s.", fname);
    errno = saved_errno;
    return NULL;
  }

  if (r_nlines != NULL)
    *r_nlines  = nlines;

  if (array == NULL)
    array = (ssize_t *)gk_empty_read_result(sizeof(ssize_t), fname);

  return array;
}

/*************************************************************************/
/*! Implements checked binary input for each public typed reader.

    \param fname is the name of the file.
    \param r_nelmnts optionally receives the element count.
    \param element_size is the byte width of one typed element.
    \param msg identifies the public reader in allocation diagnostics.
    \returns a GKlib-allocated array, including a unique allocation for an
             empty file, or NULL after complete cleanup on failure.
*/
/*************************************************************************/
static void *gk_readfilebin_impl(char *fname, size_t *r_nelmnts,
                                 size_t element_size, const char *msg)
{
  int allocation_failed=0, saved_errno=0;
  size_t fsize=0, nelmnts=0;
  void *array=NULL;
  FILE *stream=NULL;

  if (r_nelmnts != NULL)
    *r_nelmnts = 0;
  if (fname == NULL || element_size == 0) {
    saved_errno = EINVAL;
    goto failure;
  }
  stream = fopen(fname, "rb");
  if (stream == NULL) {
    saved_errno = errno != 0 ? errno : EIO;
    goto failure;
  }
  if (!gk_stream_file_size(stream, &fsize)) {
    saved_errno = errno != 0 ? errno : EIO;
    goto failure;
  }
  if (fsize%element_size != 0) {
    saved_errno = EINVAL;
    goto failure;
  }
  nelmnts = fsize/element_size;
  /* Keep an empty successful read distinguishable from a NULL failure. */
  array = gk_malloc_nosignal(fsize == 0 ? 1 : fsize);
  if (array == NULL) {
    allocation_failed = 1;
    saved_errno = errno != 0 ? errno : ENOMEM;
    goto failure;
  }
  if (fread(array, element_size, nelmnts, stream) != nelmnts) {
    saved_errno = ferror(stream) ? (errno != 0 ? errno : EIO) : EINVAL;
    goto failure;
  }
  if (fgetc(stream) != EOF) {
    saved_errno = EINVAL;
    goto failure;
  }
  if (ferror(stream)) {
    saved_errno = errno != 0 ? errno : EIO;
    goto failure;
  }
  if (fclose(stream) != 0) {
    saved_errno = errno != 0 ? errno : EIO;
    stream = NULL;
    goto failure;
  }
  if (r_nelmnts != NULL)
    *r_nelmnts = nelmnts;
  return array;

failure:
  if (stream != NULL) {
    if (fclose(stream) != 0 && saved_errno == 0)
      saved_errno = errno != 0 ? errno : EIO;
  }
  gk_free(&array, LTERM);
  if (saved_errno == 0)
    saved_errno = EIO;
  errno = saved_errno;
  gk_errexit(allocation_failed || saved_errno == ENOMEM ||
      saved_errno == EOVERFLOW ? SIGMEM : SIGERR,
      allocation_failed ? "Memory allocation failed in %s for %s.\n" :
      "Invalid or truncated binary file %s (%s).\n",
      allocation_failed ? msg : (fname != NULL ? fname : "(null)"),
      allocation_failed ? (fname != NULL ? fname : "(null)") : msg);
  errno = saved_errno;
  return NULL;
}


/*************************************************************************/
/*! Reads a binary file into an array of char.

    \param fname is the name of the file.
    \param r_nelmnts optionally receives the number of elements.
    \returns the allocated array, or NULL on failure.
*/
/*************************************************************************/
char *gk_creadfilebin(char *fname, size_t *r_nelmnts)
{
  return (char *)gk_readfilebin_impl(fname, r_nelmnts, sizeof(char),
                                     "gk_creadfilebin");
}

/*************************************************************************/
/*! Implements checked binary output for each public typed writer.

    The byte count, input pointer, open, complete write, and close operation
    are validated. The destination is replaced only after a complete temporary
    file has been closed successfully.
*/
/*************************************************************************/
static size_t gk_writefilebin_impl(char *fname, size_t n, const void *array,
                                   size_t element_size, const char *msg)
{
  int saved_errno=0;
  size_t written;
  char *tempname=NULL;
  FILE *stream=NULL;

  if (n > SIZE_MAX/element_size) {
    errno = EOVERFLOW;
    gk_errexit(SIGMEM, "Binary output size overflow for file %s.\n",
        fname != NULL ? fname : "(null)");
    errno = EOVERFLOW;
    return 0;
  }
  if (n > 0 && array == NULL) {
    errno = EINVAL;
    gk_errexit(SIGERR, "Null binary output buffer for file %s.\n",
        fname != NULL ? fname : "(null)");
    errno = EINVAL;
    return 0;
  }

  stream = gk_open_output_file(fname, "wb", &tempname);
  if (stream == NULL) {
    saved_errno = errno != 0 ? errno : EIO;
    errno = saved_errno;
    gk_errexit(saved_errno == ENOMEM || saved_errno == EOVERFLOW ?
        SIGMEM : SIGERR, "Failed to open binary output file %s (%s).\n",
        fname != NULL ? fname : "(null)", msg != NULL ? msg : "");
    errno = saved_errno;
    return 0;
  }

  written = n == 0 ? 0 : fwrite(array, element_size, n, stream);
  if (written != n)
    saved_errno = errno != 0 ? errno : EIO;
  if (!gk_finish_output_file(stream, &tempname, fname, written == n)) {
    if (saved_errno == 0)
      saved_errno = errno != 0 ? errno : EIO;
    errno = saved_errno;
    gk_errexit(saved_errno == ENOMEM || saved_errno == EOVERFLOW ?
        SIGMEM : SIGERR, "Failed to write all data to file %s.\n", fname);
    errno = saved_errno;
    return 0;
  }

  return n;
}


/*************************************************************************/
/*! Writes an array of char into a binary file.

    \param fname is the name of the file.
    \param n is the number of elements in the array.
    \param a is the array to write.
    \returns n on success and zero on failure.
*/
/*************************************************************************/
size_t gk_cwritefilebin(char *fname, size_t n, char *a)
{
  return gk_writefilebin_impl(fname, n, a, sizeof(char),
                              "gk_cwritefilebin");
}

/*************************************************************************/
/*! This function reads the contents of a binary file and returns it in the
    form of an array of int32_t.
    \param fname is the name of the file
    \param r_nelmnts is the number of elements in the file. If it is NULL,
           this information is not returned.
*/
/*************************************************************************/
int32_t *gk_i32readfilebin(char *fname, size_t *r_nelmnts)
{
  return (int32_t *)gk_readfilebin_impl(fname, r_nelmnts, sizeof(int32_t),
                                        "gk_i32readfilebin");
}

/*************************************************************************/
/*! This function writes the contents of an array into a binary file.
    \param fname is the name of the file
    \param n the number of elements in the array.
    \param a the array to be written out.
*/
/*************************************************************************/
size_t gk_i32writefilebin(char *fname, size_t n, int32_t *a)
{
  return gk_writefilebin_impl(fname, n, a, sizeof(int32_t),
                              "gk_i32writefilebin");
}

/*************************************************************************/
/*! This function reads the contents of a binary file and returns it in the
    form of an array of int64_t.
    \param fname is the name of the file
    \param r_nelmnts is the number of elements in the file. If it is NULL,
           this information is not returned.
*/
/*************************************************************************/
int64_t *gk_i64readfilebin(char *fname, size_t *r_nelmnts)
{
  return (int64_t *)gk_readfilebin_impl(fname, r_nelmnts, sizeof(int64_t),
                                        "gk_i64readfilebin");
}

/*************************************************************************/
/*! This function writes the contents of an array into a binary file.
    \param fname is the name of the file
    \param n the number of elements in the array.
    \param a the array to be written out.
*/
/*************************************************************************/
size_t gk_i64writefilebin(char *fname, size_t n, int64_t *a)
{
  return gk_writefilebin_impl(fname, n, a, sizeof(int64_t),
                              "gk_i64writefilebin");
}

/*************************************************************************/
/*! This function reads the contents of a binary file and returns it in the
    form of an array of ssize_t.
    \param fname is the name of the file
    \param r_nelmnts is the number of elements in the file. If it is NULL,
           this information is not returned.
*/
/*************************************************************************/
ssize_t *gk_zreadfilebin(char *fname, size_t *r_nelmnts)
{
  return (ssize_t *)gk_readfilebin_impl(fname, r_nelmnts, sizeof(ssize_t),
                                        "gk_zreadfilebin");
}

/*************************************************************************/
/*! This function writes the contents of an array into a binary file.
    \param fname is the name of the file
    \param n the number of elements in the array.
    \param a the array to be written out.
*/
/*************************************************************************/
size_t gk_zwritefilebin(char *fname, size_t n, ssize_t *a)
{
  return gk_writefilebin_impl(fname, n, a, sizeof(ssize_t),
                              "gk_zwritefilebin");
}

/*************************************************************************/
/*! This function reads the contents of a binary file and returns it in the
    form of an array of float.
    \param fname is the name of the file
    \param r_nelmnts is the number of elements in the file. If it is NULL,
           this information is not returned.
*/
/*************************************************************************/
float *gk_freadfilebin(char *fname, size_t *r_nelmnts)
{
  return (float *)gk_readfilebin_impl(fname, r_nelmnts, sizeof(float),
                                      "gk_freadfilebin");
}

/*************************************************************************/
/*! This function writes the contents of an array into a binary file.
    \param fname is the name of the file
    \param n the number of elements in the array.
    \param a the array to be written out.
*/
/*************************************************************************/
size_t gk_fwritefilebin(char *fname, size_t n, float *a)
{
  return gk_writefilebin_impl(fname, n, a, sizeof(float),
                              "gk_fwritefilebin");
}

/*************************************************************************/
/*! This function reads the contents of a binary file and returns it in the
    form of an array of double.
    \param fname is the name of the file
    \param r_nelmnts is the number of elements in the file. If it is NULL,
           this information is not returned.
*/
/*************************************************************************/
double *gk_dreadfilebin(char *fname, size_t *r_nelmnts)
{
  return (double *)gk_readfilebin_impl(fname, r_nelmnts, sizeof(double),
                                       "gk_dreadfilebin");
}

/*************************************************************************/
/*! This function writes the contents of an array into a binary file.
    \param fname is the name of the file
    \param n the number of elements in the array.
    \param a the array to be written out.
*/
/*************************************************************************/
size_t gk_dwritefilebin(char *fname, size_t n, double *a)
{
  return gk_writefilebin_impl(fname, n, a, sizeof(double),
                              "gk_dwritefilebin");
}
