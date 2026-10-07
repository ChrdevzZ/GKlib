#include <GKlib.h>
#include <locale.h>


static int check_error_text(void)
{
  int errors[] = {EINVAL, ENOMEM, EDOM, EINVAL};
  char expected[1024], unknown[1024];
  const char *message;
  size_t i;

  if (setlocale(LC_ALL, "C") == NULL)
    return 10;
  for (i=0; i<sizeof(errors)/sizeof(errors[0]); i++) {
    snprintf(expected, sizeof(expected), "%s", strerror(errors[i]));
    errno = E2BIG;
    message = gk_strerror(errors[i]);
    if (message == NULL || strcmp(message, expected) != 0 || errno != E2BIG)
      return 11+(int)i;
  }

  errno = E2BIG;
  message = gk_strerror(INT_MAX);
  if (message == NULL || message[0] == '\0' || errno != E2BIG)
    return 15;
  snprintf(unknown, sizeof(unknown), "%s", message);
  snprintf(expected, sizeof(expected), "%s", strerror(EINVAL));
  if (strcmp(unknown, expected) == 0)
    return 16;
  message = gk_strerror(EINVAL);
  return message != NULL && strcmp(message, expected) == 0 ? 0 : 17;
}


int main(void)
{
  volatile int signum=0;
  size_t value=0;
  int status;

  status = check_error_text();
  if (status != 0)
    return status;

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
