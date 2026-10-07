/*!
\file  error.c
\brief Various error-handling functions

This file contains functions dealing with error reporting and termination

\author George
\date 1/1/2007
\version\verbatim $Id: error.c 10711 2011-08-31 22:23:04Z karypis $ \endverbatim
*/


#define _GK_ERROR_C_  /* Define the jump-buffer storage here without importing
                         its consumer declarations and DLL accessor macros. */

#include <GKlib.h>
#if !defined(_WIN32) && !defined(__MINGW32__)
#include <pthread.h>
#endif


/* These are the jmp_buf for the graceful exit in case of severe errors.
   Multiple buffers are defined to allow for recursive invokation. */
#define MAX_JBUFS 128
#if defined(_WIN32) && GKLIB_BUILD_SHARED_LIBS && !defined(GKLIB_STATIC_DEFINE)
static GKLIB_THREAD_LOCAL int gk_cur_jbufs_storage=-1;
static GKLIB_THREAD_LOCAL jmp_buf gk_jbufs_storage[MAX_JBUFS];
static GKLIB_THREAD_LOCAL jmp_buf gk_jbuf_storage;

GKLIB_EXPORT int *gk_cur_jbufs_address(void)
{
  return &gk_cur_jbufs_storage;
}

GKLIB_EXPORT jmp_buf *gk_jbufs_address(void)
{
  return gk_jbufs_storage;
}

GKLIB_EXPORT jmp_buf *gk_jbuf_address(void)
{
  return &gk_jbuf_storage;
}

#define gk_cur_jbufs gk_cur_jbufs_storage
#define gk_jbufs     gk_jbufs_storage
#define gk_jbuf      gk_jbuf_storage
#else
GKLIB_EXPORT GKLIB_THREAD_LOCAL int gk_cur_jbufs=-1;
GKLIB_EXPORT GKLIB_THREAD_LOCAL jmp_buf gk_jbufs[MAX_JBUFS];
GKLIB_EXPORT GKLIB_THREAD_LOCAL jmp_buf gk_jbuf;
#endif

/* Recovery is explicit and synchronous. External signals remain owned by the
   host, including while another thread enters or leaves a recovery frame. */
static GKLIB_THREAD_LOCAL unsigned char gk_frame_active[MAX_JBUFS];
static GKLIB_THREAD_LOCAL int gk_legacy_active=0;
static GKLIB_THREAD_LOCAL int gk_legacy_depth=-1;
#if !defined(_WIN32) && !defined(__MINGW32__)
static GKLIB_THREAD_LOCAL sigset_t gk_frame_masks[MAX_JBUFS];
static GKLIB_THREAD_LOCAL sigset_t gk_legacy_mask;


static int gk_save_mask(sigset_t *mask)
{
  int status, saved_errno;

  saved_errno = errno;
  status = pthread_sigmask(SIG_SETMASK, NULL, mask);
  if (status != 0) {
    errno = status;
    return 0;
  }
  errno = saved_errno;
  return 1;
}


static int gk_restore_mask(const sigset_t *mask)
{
  int status, saved_errno;

  saved_errno = errno;
  status = pthread_sigmask(SIG_SETMASK, mask, NULL);
  if (status != 0) {
    errno = status;
    return 0;
  }
  errno = saved_errno;
  return 1;
}
#endif

/* The following is used to control if the gk_errexit() will actually abort or not.
   There is always a single copy of this variable */
static int gk_exit_on_error = 1;


/*************************************************************************/
/*! This function sets the gk_exit_on_error variable 
 */
/*************************************************************************/
void gk_set_exit_on_error(int value)
{
  gk_exit_on_error = value;
}



/*************************************************************************/
/*! This function prints an error message and exits. The caller's errno is
    preserved.
 */
/*************************************************************************/
void errexit(const char *f_str,...)
{
  int saved_errno;
  va_list argp;

  saved_errno = errno;
  va_start(argp, f_str);
  vfprintf(stderr, f_str, argp);
  va_end(argp);

  if (strlen(f_str) == 0 || f_str[strlen(f_str)-1] != '\n')
        fprintf(stderr,"\n");
  fflush(stderr);

  errno = saved_errno;
  if (gk_exit_on_error)
    exit(-2);

  errno = saved_errno;

  /* abort(); */
}


/*************************************************************************/
/*! Reports an error to the current synchronous binding for SIGMEM/SIGERR,
    or raises the signal for the host when no binding is active. The caller's
    errno is preserved.
 */
/*************************************************************************/
void gk_errexit(int signum, const char *f_str,...)
{
  int saved_errno;
  va_list argp;

  saved_errno = errno;
  va_start(argp, f_str);
  vfprintf(stderr, f_str, argp);
  va_end(argp);

  fprintf(stderr,"\n");
  fflush(stderr);

  errno = saved_errno;
  if (gk_exit_on_error) {
    if ((signum == SIGMEM || signum == SIGERR) &&
        (gk_legacy_active || (gk_cur_jbufs >= 0 &&
         gk_cur_jbufs < MAX_JBUFS && gk_frame_active[gk_cur_jbufs])))
      gk_sigthrow(signum);
    raise(signum);
  }

  errno = saved_errno;
}


/***************************************************************************/
/*! Establishes a synchronous recovery frame. The caller must initialize its
    jump buffer with gk_sigcatch() before calling code that can report errors.
*/
/***************************************************************************/
int gk_sigtrap(void)
{
  int next;

  if (gk_cur_jbufs < -1 || gk_cur_jbufs >= MAX_JBUFS-1) {
    errno = EOVERFLOW;
    return 0;
  }
  next = gk_cur_jbufs+1;
#if !defined(_WIN32) && !defined(__MINGW32__)
  if (!gk_save_mask(&gk_frame_masks[next]))
    return 0;
#endif

  gk_frame_active[next] = 1;
  gk_cur_jbufs = next;

  return 1;
}
  

/***************************************************************************/
/*! Releases the current frame and restores its entry signal mask. Modern and
    legacy bindings must be released in reverse order of establishment.
 */
/***************************************************************************/
int gk_siguntrap(void)
{
  if (gk_cur_jbufs < 0 || gk_cur_jbufs >= MAX_JBUFS ||
      !gk_frame_active[gk_cur_jbufs] ||
      (gk_legacy_active && gk_cur_jbufs <= gk_legacy_depth)) {
    errno = EINVAL;
    return 0;
  }

#if !defined(_WIN32) && !defined(__MINGW32__)
  if (!gk_restore_mask(&gk_frame_masks[gk_cur_jbufs]))
    return 0;
#endif

  gk_frame_active[gk_cur_jbufs] = 0;
  gk_cur_jbufs--;

  return 1;
}
  

/*************************************************************************/
/*! Rethrows a synchronous error to the most recent binding in this thread.
    With no binding, the host owns the raised signal. This is not an
    asynchronous signal handler.
 */
/*************************************************************************/
void gk_sigthrow(int signum)
{
  if (gk_legacy_active && gk_cur_jbufs <= gk_legacy_depth)
    gk_NonLocalExit_Handler(signum);
  if (gk_cur_jbufs >= 0 && gk_cur_jbufs < MAX_JBUFS) {
#if !defined(_WIN32) && !defined(__MINGW32__)
    if (gk_frame_active[gk_cur_jbufs] &&
        !gk_restore_mask(&gk_frame_masks[gk_cur_jbufs]))
      _Exit(EXIT_FAILURE);
#endif
    longjmp(gk_jbufs[gk_cur_jbufs], signum);
  }
  raise(signum);
}
  

/***************************************************************************
* Establish a single legacy synchronous binding to the caller's gk_jbuf.
* Repeated establishment leaves the original binding intact and sets EINVAL.
****************************************************************************/
void gk_SetSignalHandlers(void)
{
  if (gk_legacy_active) {
    errno = EINVAL;
    return;
  }
#if !defined(_WIN32) && !defined(__MINGW32__)
  if (!gk_save_mask(&gk_legacy_mask))
    return;
#endif
  gk_legacy_depth = gk_cur_jbufs;
  gk_legacy_active = 1;
}
  

/***************************************************************************
* Release the legacy binding after all bindings established inside it exit.
****************************************************************************/
void gk_UnsetSignalHandlers(void)
{
  if (!gk_legacy_active || gk_cur_jbufs != gk_legacy_depth) {
    errno = EINVAL;
    return;
  }
#if !defined(_WIN32) && !defined(__MINGW32__)
  if (!gk_restore_mask(&gk_legacy_mask))
    return;
#endif
  gk_legacy_active = 0;
  gk_legacy_depth = -1;
}
  

/*************************************************************************
* Jump synchronously to the caller's initialized legacy buffer. The historical
* name is retained; asynchronous signal-handler invocation is not supported.
**************************************************************************/
void gk_NonLocalExit_Handler(int signum)
{
#if !defined(_WIN32) && !defined(__MINGW32__)
  if (gk_legacy_active && !gk_restore_mask(&gk_legacy_mask))
    _Exit(EXIT_FAILURE);
#endif
  longjmp(gk_jbuf, signum);
}
  

/*************************************************************************/
/*! \brief Thread-safe implementation of strerror() */
/**************************************************************************/
char *gk_strerror(int errnum)
{
  char *message;
  int saved_errno=errno;

#if defined(_WIN32) || defined(__MINGW32__)
  message = strerror(errnum);
#else 
#ifndef SUNOS
  static GKLIB_THREAD_LOCAL char buf[1024];

  buf[0] = '\0';
#if defined(__GLIBC__) && defined(__USE_GNU)
  /* GNU libc may return immutable storage instead of writing to buf. */
  message = strerror_r(errnum, buf, sizeof(buf));
  if (message != NULL && message != buf)
    strncpy(buf, message, sizeof(buf)-1);
#else
  if (strerror_r(errnum, buf, sizeof(buf)) != 0)
    buf[0] = '\0';
#endif

  buf[1023] = '\0';
  if (buf[0] == '\0')
    snprintf(buf, sizeof(buf), "Unknown error %d", errnum);
  message = buf;
#else
  message = strerror(errnum);
#endif
#endif
  errno = saved_errno;
  return message;
}



/*************************************************************************
* This function prints a backtrace of calling functions
**************************************************************************/
void PrintBackTrace(void)
{
#ifdef HAVE_EXECINFO_H
  void *array[10];
  int i, size;
  char **strings;

  size = backtrace(array, 10);
  strings = backtrace_symbols(array, size);
  
  printf("Obtained %d stack frames.\n", size);
  for (i=0; i<size; i++) {
    printf("%s\n", strings[i]);
  }
  free(strings);
#endif
}
