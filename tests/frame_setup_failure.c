#include <GKlib.h>
#include <pthread.h>


GK_MKPQUEUE2_T(frame_pq2_t, int, int)
GK_MKPQUEUE_PROTO(frame_pq_, gk_ipq_t, int, gk_idx_t)
GK_MKPQUEUE2_PROTO(frame_pq2_, frame_pq2_t, int, int)

#define frame_key_lt(a, b) ((a) < (b))
GK_MKPQUEUE(frame_pq_, gk_ipq_t, gk_ikv_t, int, gk_idx_t,
    gk_ikvmalloc, INT_MAX, frame_key_lt)
GK_MKPQUEUE2(frame_pq2_, frame_pq2_t, int, int,
    gk_imalloc, gk_imalloc, INT_MAX, frame_key_lt)

static int armed, hits, target_depth, query_error;

int __real_pthread_sigmask(int how, const sigset_t *set, sigset_t *oldset);


/* Only the producer's mask query fails; all restores reach the real libc. */
int __wrap_pthread_sigmask(int how, const sigset_t *set, sigset_t *oldset)
{
  if (armed && set == NULL && oldset != NULL &&
      gk_cur_jbufs == target_depth) {
    armed = 0;
    hits++;
    return query_error;
  }
  return __real_pthread_sigmask(how, set, oldset);
}


static int same_mask(const sigset_t *left, const sigset_t *right)
{
  int signum;

  for (signum=1; signum<=SIGRTMAX; signum++) {
    if (sigismember(left, signum) != sigismember(right, signum))
      return 0;
  }
  return 1;
}


/* Pointers and initialized queues survive a jump to this still-live caller. */
static int check_queue(int variant, int inner, int mode, int error)
{
  gk_ipq_t *volatile created=NULL;
  frame_pq2_t *volatile created2=NULL;
  static gk_ipq_t initialized;
  volatile int caught=0;
  unsigned char *marker;
  size_t baseline;
  int depth, saved_errno, expected, result=0;
  sigset_t before, after;

  memset(&initialized, 0, sizeof(initialized));
  if (!gk_malloc_init())
    return 1;
  marker = (unsigned char *)gk_malloc(32, "frame setup marker");
  if (marker == NULL || !gk_sigtrap())
    return 2;
  memset(marker, 0x5a, 32);
  baseline = gk_GetCurMemoryUsed();
  depth = gk_cur_jbufs;
  if (__real_pthread_sigmask(SIG_SETMASK, NULL, &before))
    return 3;
  target_depth = depth+inner;
  query_error = error;
  hits = 0;
  armed = 1;
  errno = 0;
  gk_set_exit_on_error(mode);
  switch (gk_sigcatch()) {
    case 0:
      if (variant == 0)
        created = gk_ipqCreate(4);
      else if (variant == 1)
        gk_ipqInit(&initialized, 4);
      else if (variant == 2)
        created = frame_pq_Create(4);
      else if (variant == 3)
        frame_pq_Init(&initialized, 4);
      else
        created2 = frame_pq2_Create2(4);
      break;
    case SIGMEM:
      caught = SIGMEM;
      break;
    default:
      caught = SIGERR;
      break;
  }
  saved_errno = errno;
  armed = 0;
  expected = error == ENOMEM || error == EOVERFLOW ? SIGMEM : SIGERR;
  if (__real_pthread_sigmask(SIG_SETMASK, NULL, &after) ||
      hits != 1 || saved_errno != error || gk_cur_jbufs != depth ||
      !same_mask(&before, &after) || created != NULL || created2 != NULL ||
      initialized.heap != NULL || initialized.locator != NULL ||
      initialized.nnodes != 0 || initialized.maxnodes != 0 ||
      caught != (mode ? expected : 0) ||
      gk_GetCurMemoryUsed() != baseline || marker[0] != 0x5a ||
      marker[31] != 0x5a)
    result = 4;

  /* The outer frame remains active after two consecutive failures. */
  if (result == 0) {
    query_error = EIO;
    target_depth = depth;
    armed = 1;
    errno = 0;
    if (gk_sigtrap() != 0 || errno != EIO || gk_cur_jbufs != depth)
      result = 5;
    armed = 0;
  }
  if (result == 0) {
    if (!gk_sigtrap())
      result = 6;
    else {
      switch (gk_sigcatch()) {
        case 0:
          break;
        default:
          result = 7;
          break;
      }
      if (!gk_siguntrap())
        result = 8;
    }
    created = gk_ipqCreate(4);
    if (created == NULL || gk_ipqInsert((gk_ipq_t *)created, 2, 7) != 0 ||
        gk_ipqGetTop((gk_ipq_t *)created) != 2)
      result = 9;
    gk_ipqDestroy((gk_ipq_t *)created);
    created = NULL;
  }
  if (gk_GetCurMemoryUsed() != baseline || marker[0] != 0x5a ||
      !gk_siguntrap() || gk_cur_jbufs != depth-1)
    result = 10;
  gk_free((void **)&marker, LTERM);
  gk_malloc_cleanup(0);
  gk_set_exit_on_error(1);
  if (result != 0)
    fprintf(stderr, "frame setup variant %d inner %d mode %d: errno %d expected %d, hits %d, result %d\n",
        variant, inner, mode, saved_errno, error, hits, result);
  return result;
}


int main(void)
{
  int errors[] = {EIO, ENOMEM, EOVERFLOW};
  int variant, inner, mode;
  size_t error;

  for (variant=0; variant<5; variant++) {
    for (inner=0; inner<((variant == 0 || variant == 2) ? 2 : 1); inner++) {
      for (mode=0; mode<2; mode++) {
        for (error=0; error<sizeof(errors)/sizeof(errors[0]); error++) {
          if (check_queue(variant, inner, mode, errors[error]) != 0)
            return 1;
        }
      }
    }
  }
  return 0;
}
