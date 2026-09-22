/*!
\file io_internal.h
\brief Private helpers for consistent input and transactional output.
*/

#ifndef _GK_IO_INTERNAL_H_
#define _GK_IO_INTERNAL_H_

#include <fcntl.h>
#ifdef _WIN32
#include <windows.h>
#ifdef __MSC__
#include <io.h>
#include <share.h>
#endif
#else
#include <unistd.h>
#endif

#ifdef _WIN32
static inline void gk_set_errno_from_windows(DWORD error);
#endif


/*************************************************************************/
/*! Reads one complete line using the portable getline() implementation.

    This helper is always compiled so the fallback ownership and growth
    contract can be exercised even on platforms that provide getline().
    The caller owns the returned buffer and releases it with free().  A
    failed realloc leaves the original buffer and capacity unchanged.
*/
/*************************************************************************/
static inline ssize_t gk_getline_fallback(char **lineptr, size_t *n,
    FILE *stream)
{
  size_t capacity, i;
  char *line, *new_line;
  int ch;

  if (lineptr == NULL || n == NULL || stream == NULL) {
    errno = EINVAL;
    return -1;
  }

  errno = 0;
  line = *lineptr;
  capacity = *n;
  if (line == NULL) {
    capacity = 1024;
    line = (char *)malloc(capacity);
    if (line == NULL) {
      errno = ENOMEM;
      return -1;
    }
    *lineptr = line;
    *n = capacity;
  }
  else if (capacity == 0) {
    capacity = 1024;
    new_line = (char *)realloc(line, capacity);
    if (new_line == NULL) {
      errno = ENOMEM;
      return -1;
    }
    line = new_line;
    *lineptr = line;
    *n = capacity;
  }

  i = 0;
  while ((ch = getc(stream)) != EOF) {
    if (i == capacity-1) {
      size_t new_capacity;

      if (capacity > SIZE_MAX/2 || capacity*2 > (size_t)PTRDIFF_MAX) {
        errno = EOVERFLOW;
        return -1;
      }
      new_capacity = capacity*2;
      new_line = (char *)realloc(line, new_capacity);
      if (new_line == NULL) {
        errno = ENOMEM;
        return -1;
      }
      line = new_line;
      capacity = new_capacity;
      *lineptr = line;
      *n = capacity;
    }

    line[i++] = (char)ch;
    if (ch == '\n')
      break;
  }

  if (ch == EOF && ferror(stream)) {
    line[i] = '\0';
    if (errno == 0)
      errno = EIO;
    return -1;
  }
  if (i == 0)
    return -1;

  line[i] = '\0';
  *lineptr = line;
  *n = capacity;

  return (ssize_t)i;
}


/*************************************************************************/
/*! Gets the size of the file referenced by an open stream.

    The descriptor, rather than the pathname, is inspected so a path
    replacement cannot make validation and the subsequent read refer to
    different files.
*/
/*************************************************************************/
static inline int gk_stream_file_size(FILE *stream, size_t *r_size)
{
  if (stream == NULL || r_size == NULL) {
    errno = EINVAL;
    return 0;
  }
#ifdef __MSC__
  struct _stat64 status;

  if (_fstat64(_fileno(stream), &status) != 0)
    return 0;
  if ((status.st_mode & _S_IFMT) != _S_IFREG) {
    errno = EINVAL;
    return 0;
  }
#else
  struct stat status;

  if (fstat(fileno(stream), &status) != 0)
    return 0;
  if (!S_ISREG(status.st_mode)) {
    errno = EINVAL;
    return 0;
  }
#endif
  if (status.st_size < 0 ||
      (uintmax_t)status.st_size > (uintmax_t)PTRDIFF_MAX) {
    errno = EOVERFLOW;
    return 0;
  }

  *r_size = (size_t)status.st_size;
  return 1;
}


/*************************************************************************/
/*! Opens an exclusive temporary output beside its final destination.

    \param filename is the final output path.
    \param mode is the text or binary write mode for the returned stream.
    \param r_tempname receives a malloc-allocated temporary path that must be
           passed to gk_finish_output_file().
    \returns the temporary output stream, or NULL without changing an existing
             destination.
*/
/*************************************************************************/
static inline FILE *gk_open_output_file(const char *filename, const char *mode,
    char **r_tempname)
{
  FILE *stream;
  char *tempname;
  size_t filename_length;
  unsigned int attempt;
  int fd=-1, saved_errno=0;
#ifndef _WIN32
  struct stat destination_status, temporary_status;
  int destination_exists=0;
  long process_id=(long)getpid();
#else
  LARGE_INTEGER counter;
  unsigned long process_id=(unsigned long)GetCurrentProcessId();
  unsigned long thread_id=(unsigned long)GetCurrentThreadId();
#ifdef __MSC__
  int flags;
#endif
#endif

  if (r_tempname != NULL)
    *r_tempname = NULL;
  if (filename == NULL || filename[0] == '\0' || mode == NULL ||
      mode[0] != 'w' || r_tempname == NULL) {
    errno = EINVAL;
    return NULL;
  }

  filename_length = strlen(filename);
  if (filename_length > SIZE_MAX-96) {
    errno = EOVERFLOW;
    return NULL;
  }
  tempname = (char *)malloc(filename_length+96);
  if (tempname == NULL) {
    errno = ENOMEM;
    return NULL;
  }
  memcpy(tempname, filename, filename_length);

#ifndef _WIN32
  if (lstat(filename, &destination_status) == 0) {
    if (!S_ISREG(destination_status.st_mode) ||
        destination_status.st_nlink != 1) {
      free(tempname);
      errno = EINVAL;
      return NULL;
    }
    destination_exists = 1;
  }
  else if (errno != ENOENT) {
    saved_errno = errno;
    free(tempname);
    errno = saved_errno;
    return NULL;
  }

  for (attempt=0; attempt<65536; attempt++) {
    if (snprintf(tempname+filename_length, 96, ".tmp.%ld.%u",
        process_id, attempt) < 0) {
      saved_errno = EINVAL;
      break;
    }
    fd = open(tempname, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd >= 0)
      break;
    if (errno != EEXIST) {
      saved_errno = errno;
      break;
    }
  }
#else
  {
    BY_HANDLE_FILE_INFORMATION information;
    DWORD attributes=GetFileAttributesA(filename);
    HANDLE handle;

    if (attributes == INVALID_FILE_ATTRIBUTES) {
      DWORD error=GetLastError();

      if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) {
        free(tempname);
        gk_set_errno_from_windows(error);
        return NULL;
      }
    }
    else {
      if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
          (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        free(tempname);
        errno = EACCES;
        return NULL;
      }
      handle = CreateFileA(filename, FILE_READ_ATTRIBUTES,
          FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
          OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
      if (handle == INVALID_HANDLE_VALUE) {
        free(tempname);
        gk_set_errno_from_windows(GetLastError());
        return NULL;
      }
      if (!GetFileInformationByHandle(handle, &information)) {
        DWORD error=GetLastError();

        CloseHandle(handle);
        free(tempname);
        gk_set_errno_from_windows(error);
        return NULL;
      }
      CloseHandle(handle);
      if (information.nNumberOfLinks != 1) {
        free(tempname);
        errno = EINVAL;
        return NULL;
      }
    }
  }

  QueryPerformanceCounter(&counter);
#ifdef __MSC__
  flags = _O_WRONLY | _O_CREAT | _O_EXCL |
      (strchr(mode, 'b') != NULL ? _O_BINARY : _O_TEXT);
#endif
  for (attempt=0; attempt<65536; attempt++) {
    if (snprintf(tempname+filename_length, 96, ".tmp.%lu.%lu.%08lx.%u",
        process_id, thread_id, (unsigned long)counter.LowPart, attempt) < 0) {
      saved_errno = EINVAL;
      break;
    }
#ifdef __MSC__
    _sopen_s(&fd, tempname, flags, _SH_DENYNO, _S_IREAD | _S_IWRITE);
#else
    fd = open(tempname, O_WRONLY | O_CREAT | O_EXCL, 0600);
#endif
    if (fd >= 0)
      break;
    if (errno != EEXIST) {
      saved_errno = errno;
      break;
    }
  }
#endif
  if (fd < 0) {
    if (saved_errno == 0)
      saved_errno = errno != 0 ? errno : EEXIST;
    free(tempname);
    errno = saved_errno;
    return NULL;
  }

#ifndef _WIN32
  /* open() applies the process umask for a new destination. When replacing
     a regular file, retain its owner and permission bits at rename. */
  if (destination_exists) {
    if (fstat(fd, &temporary_status) != 0 ||
        ((temporary_status.st_uid != destination_status.st_uid ||
          temporary_status.st_gid != destination_status.st_gid) &&
         fchown(fd, destination_status.st_uid, destination_status.st_gid) != 0) ||
        fchmod(fd, destination_status.st_mode & 07777) != 0) {
      saved_errno = errno != 0 ? errno : EIO;
      close(fd);
      remove(tempname);
      free(tempname);
      errno = saved_errno;
      return NULL;
    }
  }
#endif

#ifdef __MSC__
  stream = _fdopen(fd, mode);
#else
  stream = fdopen(fd, mode);
#endif
  if (stream == NULL) {
    saved_errno = errno != 0 ? errno : EIO;
#ifdef __MSC__
    _close(fd);
#else
    close(fd);
#endif
    remove(tempname);
    free(tempname);
    errno = saved_errno;
    return NULL;
  }

  *r_tempname = tempname;
  return stream;
}


#ifdef _WIN32
/*************************************************************************/
/*! Maps the Win32 file errors used by output transactions to errno. */
/*************************************************************************/
static inline void gk_set_errno_from_windows(DWORD error)
{
  switch (error) {
    case ERROR_ACCESS_DENIED:
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
      errno = EACCES;
      break;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
      errno = ENOENT;
      break;
    case ERROR_ALREADY_EXISTS:
    case ERROR_FILE_EXISTS:
      errno = EEXIST;
      break;
    case ERROR_DISK_FULL:
    case ERROR_HANDLE_DISK_FULL:
      errno = ENOSPC;
      break;
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
      errno = ENOMEM;
      break;
    default:
      errno = EIO;
      break;
  }
}
#endif


/*************************************************************************/
/*! Finalizes an output stream and commits a complete temporary file.

    Named outputs replace their destination only after all stream operations
    succeed. Standard output is flushed but never closed. On failure, the
    temporary-file cleanup is best effort, and errno describes the first
    detected error.
*/
/*************************************************************************/
static inline int gk_finish_output_file(FILE *stream, char **r_tempname,
    const char *filename, int complete)
{
  int failed=!complete;
  int saved_errno=complete ? 0 : (errno != 0 ? errno : EIO);
  char *tempname=r_tempname != NULL ? *r_tempname : NULL;

  if (stream == NULL) {
    failed = 1;
    if (saved_errno == 0)
      saved_errno = EINVAL;
  }
  else {
    if (ferror(stream)) {
      failed = 1;
      if (saved_errno == 0)
        saved_errno = errno != 0 ? errno : EIO;
    }
    if (fflush(stream) != 0) {
      failed = 1;
      if (saved_errno == 0)
        saved_errno = errno != 0 ? errno : EIO;
    }
    if (tempname != NULL && fclose(stream) != 0) {
      failed = 1;
      if (saved_errno == 0)
        saved_errno = errno != 0 ? errno : EIO;
    }
  }

  if (!failed && tempname != NULL) {
#ifdef _WIN32
    if (!ReplaceFileA(filename, tempname, NULL, REPLACEFILE_WRITE_THROUGH,
                      NULL, NULL)) {
      DWORD replace_error=GetLastError();

      if (replace_error != ERROR_FILE_NOT_FOUND &&
          replace_error != ERROR_PATH_NOT_FOUND) {
        failed = 1;
        gk_set_errno_from_windows(replace_error);
        saved_errno = errno;
      }
      else if (!MoveFileExA(tempname, filename, MOVEFILE_WRITE_THROUGH)) {
        failed = 1;
        gk_set_errno_from_windows(GetLastError());
        saved_errno = errno;
      }
    }
#else
    if (rename(tempname, filename) != 0) {
      failed = 1;
      saved_errno = errno != 0 ? errno : EIO;
    }
#endif
  }

  if (tempname != NULL) {
    if (failed && remove(tempname) != 0 && saved_errno == 0)
      saved_errno = errno != 0 ? errno : EIO;
    free(tempname);
    *r_tempname = NULL;
  }
  if (failed) {
    errno = saved_errno != 0 ? saved_errno : EIO;
    return 0;
  }

  return 1;
}

#endif
