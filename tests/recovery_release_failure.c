#include <GKlib.h>
#ifdef GKLIB_TEST_POSIX_RELEASE
#include <pthread.h>
#endif


GK_MKPQUEUE2_T(release_queue2_t, int, int)
GK_MKPQUEUE_PROTO(release_pq_, gk_ipq_t, int, gk_idx_t)
GK_MKPQUEUE2_PROTO(release_pq2_, release_queue2_t, int, int)

#define release_key_lt(a, b) ((a) < (b))
GK_MKPQUEUE(release_pq_, gk_ipq_t, gk_ikv_t, int, gk_idx_t,
    gk_ikvmalloc, INT_MAX, release_key_lt)
GK_MKPQUEUE2(release_pq2_, release_queue2_t, int, int,
    gk_imalloc, gk_imalloc, INT_MAX, release_key_lt)

static int fail_release;
#ifdef GKLIB_TEST_POSIX_RELEASE
static int failed_restores;
#endif


#ifdef __cplusplus
extern "C" {
#endif
int __real_gk_siguntrap(void);

/* A failed release keeps its real binding owned by the current activation. */
int __wrap_gk_siguntrap(void)
{
  if (fail_release && gk_cur_jbufs == 0) {
    errno = EIO;
    return 0;
  }
  return __real_gk_siguntrap();
}

#ifdef GKLIB_TEST_POSIX_RELEASE
int __real_pthread_sigmask(int how, const sigset_t *set, sigset_t *oldset);

int __wrap_pthread_sigmask(int how, const sigset_t *set, sigset_t *oldset)
{
  if (set != NULL && failed_restores != 0) {
    if (failed_restores > 0)
      failed_restores--;
    return EIO;
  }
  return __real_pthread_sigmask(how, set, oldset);
}
#endif
#ifdef __cplusplus
}
#endif


static void exit_callback(void)
{
  fputs("ATEXIT\n", stdout);
}


static void abort_handler(int signum)
{
  (void)signum;
  _Exit(EXIT_SUCCESS);
}


#ifdef __cplusplus
struct release_guard {
  ~release_guard() { fputs("DESTRUCTOR\n", stdout); }
};
#endif


#ifdef GKLIB_TEST_POSIX_RELEASE
static int check_current_mask(const sigset_t *expected)
{
  sigset_t actual;
  int signum, limit, result=0, saved_errno=errno;

  if (pthread_sigmask(SIG_SETMASK, NULL, &actual) != 0)
    return 1;
#ifdef NSIG
  limit = NSIG;
#else
  limit = SIGRTMAX+1;
#endif
  for (signum=1; signum<limit; signum++) {
    if (sigismember(expected, signum) != sigismember(&actual, signum))
      result = 2;
  }
  errno = saved_errno;
  return result;
}


static int check_public_retry(void)
{
  sigset_t entry, changed;
  int depth=gk_cur_jbufs;
  unsigned char *marker;
  size_t baseline;

  if (!gk_malloc_init() ||
      pthread_sigmask(SIG_SETMASK, NULL, &entry) != 0)
    return 1;
  marker = (unsigned char *)gk_malloc(32, "public retry marker");
  if (marker == NULL)
    return 1;
  marker[0] = 73;
  baseline = gk_GetCurMemoryUsed();
  changed = entry;
  if (sigismember(&entry, SIGUSR1) == 1)
    sigdelset(&changed, SIGUSR1);
  else
    sigaddset(&changed, SIGUSR1);
  if (SIGRTMIN <= SIGRTMAX) {
    if (sigismember(&entry, SIGRTMIN) == 1)
      sigdelset(&changed, SIGRTMIN);
    else
      sigaddset(&changed, SIGRTMIN);
    if (sigismember(&entry, SIGRTMAX) == 1)
      sigdelset(&changed, SIGRTMAX);
    else
      sigaddset(&changed, SIGRTMAX);
  }
  if (!gk_sigtrap())
    return 1;
  switch (gk_sigcatch()) {
    case 0:
      break;
    default:
      return 2;
  }
  if (pthread_sigmask(SIG_SETMASK, &changed, NULL) != 0)
    return 2;
  failed_restores = 2;
  if (gk_siguntrap() != 0 || errno != EIO || gk_cur_jbufs != depth+1 ||
      check_current_mask(&changed) != 0 ||
      marker[0] != 73 || gk_GetCurMemoryUsed() != baseline ||
      gk_siguntrap() != 0 || errno != EIO || gk_cur_jbufs != depth+1 ||
      check_current_mask(&changed) != 0 || marker[0] != 73 ||
      gk_GetCurMemoryUsed() != baseline)
    return 3;
  errno = EDOM;
  if (!gk_siguntrap() || errno != EDOM || gk_cur_jbufs != depth ||
      check_current_mask(&entry) != 0)
    return 4;

  errno = 0;
  gk_SetSignalHandlers();
  if (errno != 0)
    return 5;
  switch (setjmp(gk_jbuf)) {
    case 0:
      break;
    default:
      return 5;
  }
  if (pthread_sigmask(SIG_SETMASK, &changed, NULL) != 0)
    return 5;
  failed_restores = 2;
  gk_UnsetSignalHandlers();
  if (errno != EIO || check_current_mask(&changed) != 0 ||
      gk_cur_jbufs != depth || marker[0] != 73 ||
      gk_GetCurMemoryUsed() != baseline)
    return 6;
  errno = 0;
  gk_UnsetSignalHandlers();
  if (errno != EIO || check_current_mask(&changed) != 0 ||
      gk_cur_jbufs != depth || marker[0] != 73 ||
      gk_GetCurMemoryUsed() != baseline)
    return 7;
  errno = EDOM;
  gk_UnsetSignalHandlers();
  if (errno != EDOM || check_current_mask(&entry) != 0)
    return 8;
  errno = 0;
  gk_SetSignalHandlers();
  if (errno != 0)
    return 9;
  gk_UnsetSignalHandlers();
  if (errno != 0 || gk_cur_jbufs != depth || marker[0] != 73 ||
      gk_GetCurMemoryUsed() != baseline)
    return 10;
  gk_free((void **)&marker, LTERM);
  gk_malloc_cleanup(0);
  return 0;
}
#endif


int main(int argc, char *argv[])
{
  gk_ipq_t queue;
#ifdef __cplusplus
  release_guard guard;
#endif

  if (argc == 2 && strcmp(argv[1], "exit-status") == 0)
    _Exit(EXIT_FAILURE);
#ifdef GKLIB_TEST_POSIX_RELEASE
  if (argc == 2 && strcmp(argv[1], "public-retry") == 0)
    return check_public_retry();
#endif
  if (argc != 3 || atexit(exit_callback) != 0 ||
      signal(SIGABRT, abort_handler) == SIG_ERR)
    return EXIT_SUCCESS;
  gk_set_exit_on_error(atoi(argv[2]));
  fail_release = 1;
  if (strcmp(argv[1], "queue-create") == 0)
    release_pq_Create(4);
  else if (strcmp(argv[1], "queue-init") == 0)
    release_pq_Init(&queue, 4);
  else if (strcmp(argv[1], "queue2-create") == 0)
    release_pq2_Create2(4);
#ifdef GKLIB_TEST_POSIX_RELEASE
  else if (strcmp(argv[1], "modern") == 0) {
    if (!gk_sigtrap())
      return EXIT_SUCCESS;
    switch (gk_sigcatch()) {
      case 0:
        failed_restores = -1;
        gk_sigthrow(SIGERR);
        break;
      default:
        fputs("CATCH\n", stdout);
        break;
    }
  }
  else if (strcmp(argv[1], "legacy") == 0) {
    errno = 0;
    gk_SetSignalHandlers();
    if (errno != 0)
      return EXIT_SUCCESS;
    switch (setjmp(gk_jbuf)) {
      case 0:
        failed_restores = -1;
        gk_sigthrow(SIGERR);
        break;
      default:
        fputs("CATCH\n", stdout);
        break;
    }
  }
#endif
  fputs("AFTER\n", stdout);
  return EXIT_FAILURE;
}
