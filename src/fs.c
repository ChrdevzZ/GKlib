/*!
\file  fs.c
\brief Various file-system functions.

This file contains various functions that deal with interfacing with 
the filesystem in a portable way.

\date Started 4/10/95
\author George
\version\verbatim $Id: fs.c 14332 2013-05-18 12:22:57Z karypis $ \endverbatim
*/


#ifndef _WIN32
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include <GKlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#endif


/*************************************************************************
* This function checks if a file exists
**************************************************************************/
int gk_fexists(char *fname)
{
  struct stat status;

  if (fname == NULL) {
    errno = EINVAL;
    return 0;
  }
  if (stat(fname, &status) == -1)
    return 0;

  return S_ISREG(status.st_mode);
}


/*************************************************************************
* This function checks if a directory exists
**************************************************************************/
int gk_dexists(char *dirname)
{
  struct stat status;

  if (dirname == NULL) {
    errno = EINVAL;
    return 0;
  }
  if (stat(dirname, &status) == -1)
    return 0;

  return S_ISDIR(status.st_mode);
}


/*************************************************************************/
/*! \brief Returns the size of the file in bytes

This function returns the size of a file as a 64 bit integer. If there 
were any errors in stat'ing the file, -1 is returned.
\note That due to the -1 return code, the maximum file size is limited to
      63 bits (which I guess is okay for now).
*/
/**************************************************************************/
ssize_t gk_getfsize(char *filename)
{
  struct stat status;

  if (filename == NULL) {
    errno = EINVAL;
    return -1;
  }
  if (stat(filename, &status) == -1) 
    return -1;
  if (status.st_size < 0 || (uintmax_t)status.st_size > (uintmax_t)PTRDIFF_MAX) {
    errno = EOVERFLOW;
    return -1;
  }

  return (ssize_t)status.st_size;
}


/*************************************************************************/
/*! This function gets some basic statistics about the file. 
    \param fname is the name of the file
    \param r_nlines is the number of lines in the file. If it is NULL,
           this information is not returned.
    \param r_ntokens is the number of tokens in the file. If it is NULL,
           this information is not returned.
    \param r_max_nlntokens is the maximum number of tokens in any line
           in the file. If it is NULL this information is not returned.
    \param r_nbytes is the number of bytes in the file. If it is NULL,
           this information is not returned.
*/
/*************************************************************************/
void gk_getfilestats(char *fname, size_t *r_nlines, size_t *r_ntokens, 
        size_t *r_max_nlntokens, size_t *r_nbytes)
{
  int failed=0, saved_errno=0;
  size_t nlines=0, ntokens=0, max_nlntokens=0, nbytes=0;
  size_t line_tokens=0, nread, i;
  int intoken=0;
  unsigned char buffer[4096];
  FILE *fpin;

  if (r_nlines != NULL)
    *r_nlines = 0;
  if (r_ntokens != NULL)
    *r_ntokens = 0;
  if (r_max_nlntokens != NULL)
    *r_max_nlntokens = 0;
  if (r_nbytes != NULL)
    *r_nbytes = 0;

  fpin = gk_fopen(fname, "rb", "gk_GetFileStats");
  if (fpin == NULL)
    return;

  while ((nread = fread(buffer, 1, sizeof(buffer), fpin)) != 0) {
    if (nbytes > SIZE_MAX-nread) {
      failed = 1;
      saved_errno = EOVERFLOW;
      break;
    }
    nbytes += nread;

    for (i=0; i<nread; i++) {
      if (buffer[i] == '\n') {
        if (nlines == SIZE_MAX) {
          failed = 1;
          saved_errno = EOVERFLOW;
          break;
        }
        nlines++;
        intoken = 0;
        if (max_nlntokens < line_tokens)
          max_nlntokens = line_tokens;
        line_tokens = 0;
      }
      else if (isspace((unsigned char)buffer[i])) {
        intoken = 0;
      }
      else if (!intoken) {
        if (ntokens == SIZE_MAX || line_tokens == SIZE_MAX) {
          failed = 1;
          saved_errno = EOVERFLOW;
          break;
        }
        ntokens++;
        line_tokens++;
        intoken = 1;
      }
    }
    if (failed)
      break;
  }
  if (!failed && ferror(fpin)) {
    failed = 1;
    saved_errno = errno != 0 ? errno : EIO;
  }
  if (fclose(fpin) != 0 && !failed) {
    failed = 1;
    saved_errno = errno != 0 ? errno : EIO;
  }

  if (failed) {
    if (saved_errno == 0)
      saved_errno = EIO;
    errno = saved_errno;
    gk_errexit(saved_errno == ENOMEM || saved_errno == EOVERFLOW ?
        SIGMEM : SIGERR, "Failed while reading file statistics for %s.",
        fname);
    errno = saved_errno;
    return;
  }

  if (max_nlntokens < line_tokens)
    max_nlntokens = line_tokens;

  if (r_nlines != NULL)
    *r_nlines  = nlines;
  if (r_ntokens != NULL)
    *r_ntokens = ntokens;
  if (r_max_nlntokens != NULL)
    *r_max_nlntokens = max_nlntokens;
  if (r_nbytes != NULL)
    *r_nbytes  = nbytes;
}


/*************************************************************************
* This function takes in a potentially full path specification of a file
* and just returns a string containing just the basename of the file.
* The basename is derived from the actual filename by stripping the last
* .ext part.
**************************************************************************/
char *gk_getbasename(char *path)
{
  char *startptr, *endptr;
  char *basename;

  if ((startptr = strrchr(path, '/')) == NULL) 
    startptr = path;
  else 
    startptr = startptr+1;

  basename = gk_strdup(startptr);

  if ((endptr = strrchr(basename, '.')) != NULL) 
    *endptr = '\0';

  return basename;
}

/*************************************************************************
* This function takes in a potentially full path specification of a file
* and just returns a string corresponding to its file extension. The
* extension of a file is considered to be the string right after the 
* last '.' character.
**************************************************************************/
char *gk_getextname(char *path)
{
  char *startptr;

  if ((startptr = strrchr(path, '.')) == NULL) 
    return gk_strdup(path);
  else 
    return gk_strdup(startptr+1);
}

/*************************************************************************
* This function takes in a potentially full path specification of a file
* and just returns a string containing just the filename.
**************************************************************************/
char *gk_getfilename(char *path)
{
  char *startptr;

  if ((startptr = strrchr(path, '/')) == NULL) 
    return gk_strdup(path);
  else 
    return gk_strdup(startptr+1);
}

/*************************************************************************
* This function takes in a potentially full path specification of a file
* and extracts the directory path component if it exists, otherwise it
* returns "./" as the path. The memory for it is dynamically allocated.
**************************************************************************/
char *getpathname(char *path)
{
  char *endptr, *tmp;

  if ((endptr = strrchr(path, '/')) == NULL) {
    return gk_strdup(".");
  }
  else  {
    tmp = gk_strdup(path);
    *(strrchr(tmp, '/')) = '\0';
    return tmp;
  }
}



/*************************************************************************/
/*! Returns true when a character is a platform path separator. */
/*************************************************************************/
static int gk_path_separator(char character)
{
#ifdef _WIN32
  return character == '/' || character == '\\';
#else
  return character == '/';
#endif
}


/*************************************************************************/
/*! Rejects deletion targets that could name a root or parent directory.

    Empty paths, dot components, drive roots, filesystem roots, and bare UNC
    server/share roots are rejected with errno set to EINVAL.
*/
/*************************************************************************/
static int gk_path_is_dangerous(const char *pathname)
{
  const char *component;
  size_t components=0, i, start;
  size_t length;

  if (pathname == NULL || pathname[0] == '\0') {
    errno = EINVAL;
    return 1;
  }

  length = strlen(pathname);
  while (length > 1 && gk_path_separator(pathname[length-1]))
    length--;
  for (start=0, i=0; i<=length; i++) {
    if (i == length || gk_path_separator(pathname[i])) {
      size_t component_length = i-start;

      if ((component_length == 1 && pathname[start] == '.') ||
          (component_length == 2 && pathname[start] == '.' &&
           pathname[start+1] == '.')) {
        errno = EINVAL;
        return 1;
      }
      start = i+1;
    }
  }
  component = pathname+length;
  while (component > pathname && !gk_path_separator(component[-1]))
    component--;
  if ((length == 1 && gk_path_separator(pathname[0])) ||
      (length-(size_t)(component-pathname) == 1 && component[0] == '.') ||
      (length-(size_t)(component-pathname) == 2 &&
       component[0] == '.' && component[1] == '.') ||
      (length <= 3 && length >= 2 && pathname[1] == ':')) {
    errno = EINVAL;
    return 1;
  }

  if (length >= 2 && gk_path_separator(pathname[0]) &&
      gk_path_separator(pathname[1])) {
    int in_component=0;
    for (i=2; i<length; i++) {
      if (gk_path_separator(pathname[i]))
        in_component = 0;
      else if (!in_component) {
        components++;
        in_component = 1;
      }
    }
    if (components <= 2) {
      errno = EINVAL;
      return 1;
    }
  }

  return 0;
}


/*************************************************************************/
/*! Joins two path components in a checked malloc-allocated buffer.

    The caller owns the result and must release it with free(). NULL is
    returned with errno set by the overflow or allocation failure.
*/
/*************************************************************************/
static char *gk_path_join(const char *pathname, const char *name)
{
  size_t path_length = strlen(pathname);
  size_t name_length = strlen(name);
  size_t total_length;
  int needs_separator = path_length != 0 &&
      !gk_path_separator(pathname[path_length-1]);
  char *result;

  if (path_length > SIZE_MAX-name_length) {
    errno = EOVERFLOW;
    return NULL;
  }
  total_length = path_length+name_length;
  if (total_length > SIZE_MAX-(size_t)needs_separator-1) {
    errno = EOVERFLOW;
    return NULL;
  }
  result = (char *)malloc(total_length+(size_t)needs_separator+1);
  if (result == NULL)
    return NULL;
  memcpy(result, pathname, path_length);
  if (needs_separator)
#ifdef _WIN32
    result[path_length++] = '\\';
#else
    result[path_length++] = '/';
#endif
  memcpy(result+path_length, name, name_length+1);
  return result;
}


#ifdef _WIN32
/*************************************************************************/
/*! Maps a Win32 filesystem error into errno and returns -1. */
/*************************************************************************/
static int gk_windows_error(DWORD error)
{
  switch (error) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
      errno = ENOENT;
      break;
    case ERROR_ACCESS_DENIED:
    case ERROR_SHARING_VIOLATION:
      errno = EACCES;
      break;
    case ERROR_ALREADY_EXISTS:
    case ERROR_FILE_EXISTS:
      errno = EEXIST;
      break;
    case ERROR_DIR_NOT_EMPTY:
      errno = ENOTEMPTY;
      break;
    default:
      errno = EIO;
      break;
  }
  return -1;
}


/*************************************************************************/
/*! Creates one Windows directory or accepts an existing directory. */
/*************************************************************************/
static int gk_windows_make_directory(const char *pathname)
{
  DWORD attributes;

  if (CreateDirectoryA(pathname, NULL))
    return 0;
  if (GetLastError() != ERROR_ALREADY_EXISTS)
    return gk_windows_error(GetLastError());
  attributes = GetFileAttributesA(pathname);
  if (attributes == INVALID_FILE_ATTRIBUTES ||
      !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
    errno = ENOTDIR;
    return -1;
  }
  return 0;
}


/*************************************************************************/
/*! Marks an opened Windows entry for deletion and closes its handle.

    A delete-pending child can briefly remain visible to its parent.
*/
/*************************************************************************/
static int gk_windows_delete_handle(HANDLE handle)
{
  FILE_DISPOSITION_INFO disposition;
  DWORD attempt, error;

  disposition.DeleteFile = TRUE;
  for (attempt=0; !SetFileInformationByHandle(handle, FileDispositionInfo,
      &disposition, sizeof(disposition)); attempt++) {
    error = GetLastError();
    if (error != ERROR_DIR_NOT_EMPTY || attempt == 15) {
      CloseHandle(handle);
      return gk_windows_error(error);
    }
    Sleep(1);
  }
  if (!CloseHandle(handle))
    return gk_windows_error(GetLastError());
  return 0;
}


/*************************************************************************/
/*! Removes a Windows path tree without traversing reparse points.

    Each entry is opened without FILE_SHARE_DELETE and with
    FILE_FLAG_OPEN_REPARSE_POINT before its attributes are inspected. The
    live handle therefore fixes the entry identity throughout traversal,
    and deletion through that same handle cannot be redirected by a
    concurrent pathname replacement. Regular files and reparse points are
    removed as leaves; only ordinary directories are enumerated.
*/
/*************************************************************************/
static int gk_windows_remove_opened_path(const char *pathname)
{
  BY_HANDLE_FILE_INFORMATION information;
  DWORD error;
  HANDLE find_handle, path_handle;
  WIN32_FIND_DATAA find_data;
  char *pattern, *child;

  path_handle = CreateFileA(pathname, DELETE | FILE_READ_ATTRIBUTES,
      FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
  if (path_handle == INVALID_HANDLE_VALUE)
    return gk_windows_error(GetLastError());
  if (!GetFileInformationByHandle(path_handle, &information)) {
    error = GetLastError();
    CloseHandle(path_handle);
    return gk_windows_error(error);
  }
  if (!(information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
      (information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
    return gk_windows_delete_handle(path_handle);

  pattern = gk_path_join(pathname, "*");
  if (pattern == NULL) {
    CloseHandle(path_handle);
    return -1;
  }
  find_handle = FindFirstFileA(pattern, &find_data);
  free(pattern);
  if (find_handle == INVALID_HANDLE_VALUE) {
    error = GetLastError();
    if (error != ERROR_FILE_NOT_FOUND) {
      CloseHandle(path_handle);
      return gk_windows_error(error);
    }
  }
  else {
    do {
      if (strcmp(find_data.cFileName, ".") == 0 ||
          strcmp(find_data.cFileName, "..") == 0)
        continue;
      child = gk_path_join(pathname, find_data.cFileName);
      if (child == NULL || gk_windows_remove_opened_path(child) != 0) {
        int saved_errno = child == NULL ? ENOMEM : errno;
        free(child);
        FindClose(find_handle);
        CloseHandle(path_handle);
        errno = saved_errno;
        return -1;
      }
      free(child);
    } while (FindNextFileA(find_handle, &find_data));
    error = GetLastError();
    if (!FindClose(find_handle) && error == ERROR_NO_MORE_FILES)
      error = GetLastError();
    if (error != ERROR_NO_MORE_FILES) {
      CloseHandle(path_handle);
      return gk_windows_error(error);
    }
  }

  return gk_windows_delete_handle(path_handle);
}


/*************************************************************************/
/*! Locks every explicit parent component before recursive deletion.

    Windows has no Win32 openat equivalent. Keeping no-delete-share handles
    for each supplied parent prevents an intermediate component from being
    exchanged for a junction while descendant paths are opened by name.
*/
/*************************************************************************/
static int gk_windows_remove_path(const char *pathname)
{
  BY_HANDLE_FILE_INFORMATION information;
  char *path;
  DWORD error;
  HANDLE handle, *parents;
  size_t i, j, length, nparents=0, start=0;
  int result, saved_errno;

  length = strlen(pathname);
  if (length > SIZE_MAX/sizeof(*parents)) {
    errno = EOVERFLOW;
    return -1;
  }
  path = (char *)malloc(length+1);
  parents = (HANDLE *)malloc(length*sizeof(*parents));
  if (path == NULL || (length > 0 && parents == NULL)) {
    free(path);
    free(parents);
    errno = ENOMEM;
    return -1;
  }
  memcpy(path, pathname, length+1);

  if (length >= 2 && path[1] == ':') {
    if (length < 3 || !gk_path_separator(path[2])) {
      free(path);
      free(parents);
      errno = EINVAL;
      return -1;
    }
    start = 3;
  }
  else if (length >= 2 && gk_path_separator(path[0]) &&
      gk_path_separator(path[1])) {
    start = 2;
    for (j=0; j<2; j++) {
      while (start < length && !gk_path_separator(path[start]))
        start++;
      while (start < length && gk_path_separator(path[start]))
        start++;
    }
  }
  else if (length > 0 && gk_path_separator(path[0])) {
    start = 1;
  }

  for (i=start; i<length; i++) {
    if (!gk_path_separator(path[i]))
      continue;
    for (j=i; j<length && gk_path_separator(path[j]); j++)
      ;
    if (i == start || j == length) {
      i = j == 0 ? 0 : j-1;
      continue;
    }

    path[i] = '\0';
    handle = CreateFileA(path, FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    path[i] = pathname[i];
    if (handle == INVALID_HANDLE_VALUE) {
      error = GetLastError();
      goto error;
    }
    if (!GetFileInformationByHandle(handle, &information)) {
      error = GetLastError();
      CloseHandle(handle);
      goto error;
    }
    if (!(information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
        (information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
      CloseHandle(handle);
      error = ERROR_ACCESS_DENIED;
      goto error;
    }
    parents[nparents++] = handle;
    i = j-1;
  }

  result = gk_windows_remove_opened_path(pathname);
  saved_errno = errno;
  while (nparents > 0)
    CloseHandle(parents[--nparents]);
  free(path);
  free(parents);
  errno = saved_errno;
  return result;

error:
  while (nparents > 0)
    CloseHandle(parents[--nparents]);
  free(path);
  free(parents);
  return gk_windows_error(error);
}
#else
/*************************************************************************/
/*! Removes one POSIX directory entry without following symbolic links.

    The directory descriptor remains open while its children are enumerated.
    This prevents a concurrent pathname replacement from redirecting recursive
    descent outside the tree that was originally opened.
*/
/*************************************************************************/
static int gk_posix_remove_entry(int parent_fd, const char *pathname)
{
  struct dirent *entry;
  DIR *directory;
  struct stat status;
  int directory_fd, flags, open_errno, saved_errno;

  flags = O_RDONLY | O_DIRECTORY | O_NOFOLLOW;
#ifdef O_CLOEXEC
  flags |= O_CLOEXEC;
#endif
  directory_fd = openat(parent_fd, pathname, flags);
  if (directory_fd == -1) {
    open_errno = errno;
    if (fstatat(parent_fd, pathname, &status, AT_SYMLINK_NOFOLLOW) != 0)
      return -1;

    /* unlinkat() never follows the final component. If the entry changed
       into a directory after fstatat(), this call fails rather than
       recursively entering the replacement. */
    if (!S_ISDIR(status.st_mode))
      return unlinkat(parent_fd, pathname, 0);

    errno = open_errno;
    return -1;
  }

  directory = fdopendir(directory_fd);
  if (directory == NULL) {
    saved_errno = errno;
    close(directory_fd);
    errno = saved_errno;
    return -1;
  }

  errno = 0;
  while ((entry = readdir(directory)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;
    if (gk_posix_remove_entry(dirfd(directory), entry->d_name) != 0) {
      saved_errno = errno;
      closedir(directory);
      errno = saved_errno;
      return -1;
    }
    errno = 0;
  }
  saved_errno = errno;
  if (closedir(directory) != 0 && saved_errno == 0)
    saved_errno = errno;
  if (saved_errno != 0) {
    errno = saved_errno;
    return -1;
  }
  return unlinkat(parent_fd, pathname, AT_REMOVEDIR);
}


/*************************************************************************/
/*! Removes a POSIX path using a no-follow descriptor chain.

    Every parent component is opened separately and retained until the next
    component has been resolved. O_NOFOLLOW therefore applies to the entire
    path rather than only its final component, preventing an exchanged
    ancestor symlink from redirecting recursion outside the requested tree.
*/
/*************************************************************************/
static int gk_posix_remove_path(const char *pathname)
{
  char *component, *cursor, *path;
  size_t length;
  int child_fd, close_status, flags, parent_fd, result, saved_errno;

  length = strlen(pathname);
  path = (char *)malloc(length+1);
  if (path == NULL)
    return -1;
  memcpy(path, pathname, length+1);
  while (length > 1 && path[length-1] == '/')
    path[--length] = '\0';

  flags = O_RDONLY | O_DIRECTORY | O_NOFOLLOW;
#ifdef O_CLOEXEC
  flags |= O_CLOEXEC;
#endif
  parent_fd = open(path[0] == '/' ? "/" : ".", flags);
  if (parent_fd == -1) {
    saved_errno = errno;
    free(path);
    errno = saved_errno;
    return -1;
  }

  component = path;
  while (*component == '/')
    component++;
  for (;;) {
    cursor = component;
    while (*cursor != '\0' && *cursor != '/')
      cursor++;
    while (*cursor == '/')
      *cursor++ = '\0';

    if (*cursor == '\0') {
      result = gk_posix_remove_entry(parent_fd, component);
      saved_errno = result == 0 ? 0 : errno;
      close_status = close(parent_fd);
      if (result == 0 && close_status != 0) {
        result = -1;
        saved_errno = errno;
      }
      free(path);
      if (result != 0)
        errno = saved_errno;
      return result;
    }

    child_fd = openat(parent_fd, component, flags);
    if (child_fd == -1) {
      saved_errno = errno;
      close(parent_fd);
      free(path);
      errno = saved_errno;
      return -1;
    }
    if (close(parent_fd) != 0) {
      saved_errno = errno;
      close(child_fd);
      free(path);
      errno = saved_errno;
      return -1;
    }
    parent_fd = child_fd;
    component = cursor;
  }
}
#endif


#ifndef _WIN32
/*************************************************************************/
/*! Creates one POSIX directory or accepts an existing directory. */
/*************************************************************************/
static int gk_posix_make_directory(const char *pathname)
{
  struct stat status;

  if (mkdir(pathname, 0777) == 0)
    return 0;
  if (errno != EEXIST)
    return -1;
  if (stat(pathname, &status) != 0)
    return -1;
  if (!S_ISDIR(status.st_mode)) {
    errno = ENOTDIR;
    return -1;
  }
  return 0;
}
#endif


/*************************************************************************/
/*! Creates every missing directory component in a path.

    Components are formed dynamically, so input length is limited only by the
    platform and available memory. The function returns zero on success and
    -1 with a meaningful errno on failure.
*/
/*************************************************************************/
int gk_mkpath(char *pathname)
{
  char *first_creatable, *path, *cursor;
  size_t length;

  if (pathname == NULL || pathname[0] == '\0') {
    errno = EINVAL;
    return -1;
  }
  length = strlen(pathname);
  path = (char *)malloc(length+1);
  if (path == NULL)
    return -1;
  memcpy(path, pathname, length+1);
  first_creatable = path;
#ifdef _WIN32
  if (gk_path_separator(path[0]) && gk_path_separator(path[1])) {
    first_creatable = path+2;
    while (*first_creatable != '\0' && !gk_path_separator(*first_creatable))
      first_creatable++;
    if (*first_creatable != '\0')
      first_creatable++;
    while (*first_creatable != '\0' && !gk_path_separator(*first_creatable))
      first_creatable++;
  }
#endif

  for (cursor=path; *cursor!='\0'; cursor++) {
    if (!gk_path_separator(*cursor))
      continue;
    if (cursor < first_creatable || cursor == path ||
        (cursor == path+2 && path[1] == ':'))
      continue;
    *cursor = '\0';
#ifdef _WIN32
    if (gk_windows_make_directory(path) != 0) {
      int saved_errno = errno;
      free(path);
      errno = saved_errno;
      return -1;
    }
#else
    if (gk_posix_make_directory(path) != 0) {
      int saved_errno = errno;
      free(path);
      errno = saved_errno;
      return -1;
    }
#endif
    *cursor =
#ifdef _WIN32
        '\\';
#else
        '/';
#endif
  }

#ifdef _WIN32
  if (gk_windows_make_directory(path) != 0) {
    int saved_errno = errno;
    free(path);
    errno = saved_errno;
    return -1;
  }
#else
  if (gk_posix_make_directory(path) != 0) {
    int saved_errno = errno;
    free(path);
    errno = saved_errno;
    return -1;
  }
#endif
  free(path);
  return 0;
}


/*************************************************************************/
/*! Deletes a file or directory tree without following link-like leaves.

    Dangerous root and dot targets are rejected. Symbolic links and Windows
    reparse points are removed as leaves, while regular directory contents are
    recursively removed. The function returns zero on success and -1 with a
    meaningful errno on failure.
*/
/*************************************************************************/
int gk_rmpath(char *pathname)
{
  if (gk_path_is_dangerous(pathname))
    return -1;

#ifdef _WIN32
  return gk_windows_remove_path(pathname);
#else
  return gk_posix_remove_path(pathname);
#endif
}
