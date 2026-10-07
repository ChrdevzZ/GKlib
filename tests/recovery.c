#include <GKlib.h>

#if !defined(_WIN32) && !defined(__MINGW32__)
#include <pthread.h>
#endif


static volatile sig_atomic_t host_calls;


static void host_handler(int signum)
{
  (void)signum;
  host_calls++;
}


static int check_failure(int signum)
{
  volatile int caught=0;
  size_t value=0;

  if (!gk_sigtrap())
    return 1;
  switch (gk_sigcatch()) {
    case 0:
      errno = EDOM;
      if (signum == SIGERR)
        gk_GetVMInfo(NULL, &value);
      else
        gk_errexit(signum, "");
      break;
    case SIGMEM:
      caught = SIGMEM;
      break;
    case SIGERR:
      caught = SIGERR;
      break;
    default:
      caught = -1;
      break;
  }
  if (!gk_siguntrap())
    return 2;
  return caught == signum && errno == (signum == SIGERR ? EINVAL : EDOM) ? 0 : 3;
}


static int check_nested(void)
{
  volatile int failures=0, inner_status=0;
  unsigned char * volatile outer=NULL;
  size_t baseline;

  if (!gk_malloc_init())
    return 10;
  outer = (unsigned char *)gk_malloc(32, "recovery outer");
  if (outer == NULL || !gk_sigtrap())
    return 11;
  outer[0] = 73;
  baseline = gk_GetCurMemoryUsed();
  switch (gk_sigcatch()) {
    case 0:
      if (!gk_malloc_init()) {
        inner_status = 1;
        break;
      }
      gk_malloc(16, "recovery inner");
      inner_status = check_failure(SIGERR);
      gk_malloc_cleanup(0);
      if (inner_status != 0 || outer[0] != 73 ||
          gk_GetCurMemoryUsed() != baseline) {
        inner_status = 2;
        break;
      }
      errno = ENOMEM;
      gk_errexit(SIGMEM, "");
      break;
    case SIGMEM:
      failures++;
      if (failures < 2) {
        inner_status = check_failure(SIGERR);
        errno = ENOMEM;
        gk_errexit(SIGMEM, "");
      }
      break;
    default:
      inner_status = 3;
      break;
  }
  if (!gk_siguntrap() || failures != 2 || inner_status != 0 ||
      errno != ENOMEM || outer[0] != 73 || gk_GetCurMemoryUsed() != baseline)
    return 12;
  gk_malloc_cleanup(0);
  if (gk_GetCurMemoryUsed() != 0 || check_failure(SIGMEM) != 0 ||
      !gk_sigtrap() || !gk_siguntrap())
    return 13;
  return 0;
}


static int check_legacy(void)
{
  volatile int caught=0, inner_status=0;

  if (!gk_sigtrap())
    return 20;
  switch (gk_sigcatch()) {
    case 0:
      gk_SetSignalHandlers();
      switch (setjmp(gk_jbuf)) {
        case 0:
          errno = 0;
          gk_SetSignalHandlers();
          if (errno != EINVAL || gk_siguntrap() != 0) {
            inner_status = 1;
            break;
          }
          inner_status = check_failure(SIGMEM);
          errno = E2BIG;
          gk_errexit(SIGERR, "");
          break;
        case SIGERR:
          caught = SIGERR;
          break;
        default:
          caught = -1;
          break;
      }
      gk_UnsetSignalHandlers();
      break;
    default:
      inner_status = 2;
      break;
  }
  if (!gk_siguntrap() || caught != SIGERR || errno != E2BIG || inner_status != 0)
    return 21;
  return check_failure(SIGERR);
}


static int check_capacity(void)
{
  int count=0, depth;

  while (count < 1024 && gk_sigtrap())
    count++;
  depth = gk_cur_jbufs;
  if (count != 128 || errno != EOVERFLOW ||
      depth != count-1 || gk_sigtrap() != 0 || gk_cur_jbufs != depth)
    return 30;
  while (count-- > 0) {
    if (!gk_siguntrap())
      return 31;
  }
  if (gk_cur_jbufs != -1 || gk_siguntrap() != 0 || errno != EINVAL)
    return 32;
  return 0;
}


static int check_legacy_outer(void)
{
  volatile int caught=0, status=0;
  int depth=gk_cur_jbufs;

  errno = 0;
  gk_SetSignalHandlers();
  if (errno != 0)
    return 33;
  switch (setjmp(gk_jbuf)) {
    case 0:
      status = check_failure(SIGMEM);
      if (status != 0 || !gk_sigtrap()) {
        status = 34;
        break;
      }
      errno = 0;
      gk_UnsetSignalHandlers();
      if (errno != EINVAL || gk_cur_jbufs != depth+1)
        status = 35;
      if (!gk_siguntrap())
        status = 36;
      if (status == 0) {
        errno = E2BIG;
        gk_errexit(SIGERR, "");
        status = 37;
      }
      break;
    case SIGERR:
      caught = SIGERR;
      break;
    default:
      status = 38;
      break;
  }
  gk_UnsetSignalHandlers();
  return status == 0 && caught == SIGERR && errno == E2BIG &&
      gk_cur_jbufs == depth ? 0 : 39;
}


static int check_host(void)
{
  void (*previous)(int), (*current)(int);

  previous = signal(SIGERR, host_handler);
  if (previous == SIG_ERR || !gk_sigtrap())
    return 40;
  /* External delivery is deliberately owned by the host, even before catch. */
  host_calls = 0;
  raise(SIGERR);
  if (host_calls != 1)
    return 41;
  signal(SIGERR, host_handler);
  if (check_failure(SIGERR) != 0 || !gk_siguntrap())
    return 42;
  current = signal(SIGERR, previous);
  return current == host_handler && host_calls == 1 ? 0 : 43;
}


#if !defined(_WIN32) && !defined(__MINGW32__)
static int check_current_mask(const sigset_t *expected)
{
  sigset_t actual;
  int signum, limit;

  if (pthread_sigmask(SIG_SETMASK, NULL, &actual) != 0)
    return 1;
#ifdef NSIG
  limit = NSIG;
#else
  limit = SIGRTMAX+1;
#endif
  for (signum=1; signum<limit; signum++) {
    if (sigismember(expected, signum) != sigismember(&actual, signum))
      return 2;
  }
  return 0;
}


static void fail_with_mask(const sigset_t *entry)
{
  sigset_t changed;

  changed = *entry;
  sigdelset(&changed, SIGUSR1);
  sigdelset(&changed, SIGERR);
  if (pthread_sigmask(SIG_SETMASK, &changed, NULL) != 0)
    return;
  errno = EDOM;
  gk_errexit(SIGERR, "");
}


static int check_mask_failure(int legacy)
{
  sigset_t entry;
  volatile int caught=0;
  int status;

  if (pthread_sigmask(SIG_SETMASK, NULL, &entry) != 0)
    return 1;
  if (legacy) {
    errno = 0;
    gk_SetSignalHandlers();
    if (errno != 0)
      return 2;
    switch (setjmp(gk_jbuf)) {
      case 0:
        fail_with_mask(&entry);
        break;
      case SIGERR:
        caught = SIGERR;
        break;
      default:
        caught = -1;
        break;
    }
  }
  else {
    if (!gk_sigtrap())
      return 3;
    switch (gk_sigcatch()) {
      case 0:
        fail_with_mask(&entry);
        break;
      case SIGERR:
        caught = SIGERR;
        break;
      default:
        caught = -1;
        break;
    }
  }
  /* Observe the mask before release can restore it independently. */
  status = caught == SIGERR && errno == EDOM ? 0 : 4;
  if (check_current_mask(&entry) != 0)
    status = 5;
  if (legacy)
    gk_UnsetSignalHandlers();
  else if (!gk_siguntrap())
    status = 6;
  if (check_current_mask(&entry) != 0)
    status = 7;
  return status;
}


static int check_mask(void)
{
  sigset_t original, entry, nested;
  int status=0;

  if (pthread_sigmask(SIG_SETMASK, NULL, &original) != 0)
    return 50;
  entry = original;
  sigaddset(&entry, SIGUSR1);
  sigaddset(&entry, SIGERR);
  if (pthread_sigmask(SIG_SETMASK, &entry, NULL) != 0)
    return 51;
  if (check_mask_failure(0) != 0 || check_mask_failure(0) != 0 ||
      check_mask_failure(1) != 0 || !gk_sigtrap()) {
    status = 52;
    goto CLEANUP;
  }
  /* A normal outer frame must retain its own entry mask after inner errors. */
  nested = entry;
  sigaddset(&nested, SIGUSR2);
  if (pthread_sigmask(SIG_SETMASK, &nested, NULL) != 0 ||
      check_mask_failure(0) != 0 || check_mask_failure(1) != 0)
    status = 53;
  if (!gk_siguntrap() || check_current_mask(&entry) != 0)
    status = 54;

CLEANUP:
  if (pthread_sigmask(SIG_SETMASK, &original, NULL) != 0)
    status = 55;
  return status;
}


static int check_pending(int legacy)
{
  sigset_t original, entry, changed, pending;
  struct sigaction action, previous, current;
  volatile int caught=0;
  int status=0, binding=0, signum, limit;
  int flags=SA_RESTART|SA_SIGINFO|SA_NODEFER|SA_RESETHAND|SA_NOCLDSTOP|SA_NOCLDWAIT;

#ifdef SA_ONSTACK
  flags |= SA_ONSTACK;
#endif

  if (pthread_sigmask(SIG_SETMASK, NULL, &original) != 0)
    return 60;
  entry = original;
  sigdelset(&entry, SIGUSR2);
  if (pthread_sigmask(SIG_SETMASK, &entry, NULL) != 0)
    return 61;
  memset(&action, 0, sizeof(action));
  action.sa_handler = host_handler;
  action.sa_flags = SA_RESTART;
  sigemptyset(&action.sa_mask);
  sigaddset(&action.sa_mask, SIGUSR1);
  if (sigaction(SIGUSR2, &action, &previous) != 0) {
    pthread_sigmask(SIG_SETMASK, &original, NULL);
    return 62;
  }
  if (sigaction(SIGUSR2, NULL, &action) != 0) {
    status = 62;
    goto CLEANUP;
  }
  host_calls = 0;
  if (legacy) {
    errno = 0;
    gk_SetSignalHandlers();
    if (errno != 0) {
      status = 63;
      goto CLEANUP;
    }
    binding = 1;
    switch (setjmp(gk_jbuf)) {
      case 0:
        break;
      case SIGMEM:
        caught = SIGMEM;
        break;
      default:
        caught = -1;
        break;
    }
  }
  else {
    if (!gk_sigtrap()) {
      status = 64;
      goto CLEANUP;
    }
    binding = 1;
    switch (gk_sigcatch()) {
      case 0:
        break;
      case SIGMEM:
        caught = SIGMEM;
        break;
      default:
        caught = -1;
        break;
    }
  }
  if (caught == 0) {
    changed = entry;
    sigaddset(&changed, SIGUSR2);
    if (pthread_sigmask(SIG_SETMASK, &changed, NULL) != 0 ||
        raise(SIGUSR2) != 0)
      status = 65;
    else {
      errno = EDOM;
      gk_errexit(SIGMEM, "");
      status = 66;
    }
  }
  /* Restoring the entry mask delivers the host signal before catch resumes. */
  if (status == 0 && (caught != SIGMEM || errno != EDOM || host_calls != 1 ||
      check_current_mask(&entry) != 0 || sigpending(&pending) != 0 ||
      sigismember(&pending, SIGUSR2) != 0))
    status = 67;
  if (sigaction(SIGUSR2, NULL, &current) != 0 ||
      current.sa_handler != action.sa_handler ||
      (current.sa_flags&flags) != (action.sa_flags&flags))
    status = 68;
  else {
#ifdef NSIG
    limit = NSIG;
#else
    limit = SIGRTMAX+1;
#endif
    for (signum=1; signum<limit; signum++) {
      if (sigismember(&current.sa_mask, signum) !=
          sigismember(&action.sa_mask, signum))
        status = 68;
    }
  }

CLEANUP:
  if (binding) {
    if (legacy) {
      errno = 0;
      gk_UnsetSignalHandlers();
      if (errno != 0)
        status = 69;
    }
    else if (!gk_siguntrap())
      status = 69;
  }
  if (pthread_sigmask(SIG_SETMASK, &original, NULL) != 0)
    status = 70;
  if (sigaction(SIGUSR2, &previous, NULL) != 0)
    status = 70;
  return status;
}

#endif


int main(void)
{
  int status;

  gk_set_exit_on_error(1);
  status = check_nested();
  if (status == 0)
    status = check_legacy();
  if (status == 0)
    status = check_capacity();
  if (status == 0)
    status = check_legacy_outer();
  if (status == 0)
    status = check_host();
#if !defined(_WIN32) && !defined(__MINGW32__)
  if (status == 0)
    status = check_mask();
  if (status == 0)
    status = check_pending(0);
  if (status == 0)
    status = check_pending(1);
#endif
  return status;
}
