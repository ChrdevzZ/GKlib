/* The getopt header must provide its own export declarations. */
#include <gk_getopt.h>
#include "template_consumer_impl.h"

int main(void)
{
  return gk_optind < 0 || template_consumer_run();
}
