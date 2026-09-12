#include <GKlib.h>

#ifdef GKLIB_STATIC_DEFINE
#error "This consumer must not define GKLIB_STATIC_DEFINE"
#endif

int main(void)
{
  jmp_buf *buffers;

  gk_cur_jbufs = 0;
  buffers = gk_jbufs;
  if (buffers != &gk_jbufs[0])
    return 1;
  if (gk_sigcatch() != 0)
    return 2;
  return gk_cur_jbufs == 0 ? 0 : 3;
}
