/*!
\file memory_internal.h
\brief Private helpers shared by the mcore and allocation tracker.
*/

#ifndef _GK_MEMORY_INTERNAL_H_
#define _GK_MEMORY_INTERNAL_H_

/* The global allocation tracker cannot grow itself through gk_realloc.
   Reserve with the raw allocator and commit its pointer and capacity only
   after realloc succeeds. */
static int gk_gkmcoreEnsureCapacity(gk_mcore_t *mcore)
{
  gk_mop_t *mops;
  size_t nmops;

  if (mcore->cmop < mcore->nmops)
    return 1;

  if (mcore->nmops > SIZE_MAX/2)
    return 0;
  nmops = (mcore->nmops == 0 ? 2048 : 2*mcore->nmops);
  if (nmops > SIZE_MAX/sizeof(gk_mop_t))
    return 0;

  mops = (gk_mop_t *)realloc(mcore->mops, nmops*sizeof(gk_mop_t));
  if (mops == NULL)
    return 0;

  mcore->mops = mops;
  mcore->nmops = nmops;
  return 1;
}


/* Call only after gk_gkmcoreEnsureCapacity has reserved the next slot. */
static void gk_gkmcoreAddReserved(gk_mcore_t *mcore, int type,
    size_t nbytes, void *ptr)
{
  mcore->mops[mcore->cmop].type   = type;
  mcore->mops[mcore->cmop].nbytes = nbytes;
  mcore->mops[mcore->cmop].ptr    = ptr;
  mcore->cmop++;

  if (type == GK_MOPT_HEAP) {
    mcore->num_hallocs++;
    mcore->size_hallocs += nbytes;
    mcore->cur_hallocs  += nbytes;
    if (mcore->max_hallocs < mcore->cur_hallocs)
      mcore->max_hallocs = mcore->cur_hallocs;
  }
}


/* Reallocate an ordinary mcore operation stack. If the stack belongs to any
   active allocation-tracker frame, update that existing record in place;
   otherwise leave the allocation untracked. */
GKLIB_NO_EXPORT void *gk_realloc_mcore(void *oldptr, size_t nbytes, char *msg);

#endif
