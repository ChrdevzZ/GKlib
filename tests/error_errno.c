#include <GKlib.h>


int main(void)
{
  volatile int signum=0;
  size_t value=0;

  gk_set_exit_on_error(0);

  errno = E2BIG;
  errexit("");
  if (errno != E2BIG)
    return 1;

  errno = EDOM;
  gk_errexit(SIGERR, "");
  if (errno != EDOM)
    return 2;

  errno = 0;
  gk_GetVMInfo(NULL, &value);
  if (errno != EINVAL)
    return 3;

  if (!gk_sigtrap())
    return 4;
  gk_set_exit_on_error(1);
  errno = 0;
  switch (gk_sigcatch()) {
    case 0:
      gk_GetVMInfo(NULL, &value);
      break;
    case SIGERR:
      signum = SIGERR;
      break;
    default:
      signum = SIGMEM;
      break;
  }
  if (!gk_siguntrap())
    return 5;
  if (signum != SIGERR || errno != EINVAL)
    return 6;

  gk_set_exit_on_error(1);
  return 0;
}
