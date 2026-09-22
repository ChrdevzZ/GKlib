/*!
\file memory_internal.h
\brief Private helpers shared by the mcore and allocation tracker.
*/

#ifndef _GK_MEMORY_INTERNAL_H_
#define _GK_MEMORY_INTERNAL_H_

/* Allocate or reallocate raw bytes without invoking the signal error path. */
GKLIB_NO_EXPORT void *gk_malloc_nosignal(size_t nbytes);
GKLIB_NO_EXPORT void *gk_realloc_nosignal(void *oldptr, size_t nbytes);
GKLIB_NO_EXPORT int gk_free_nosignal(void **r_ptr);


/* Checked size operations return zero and set errno to EOVERFLOW. */
static inline int gk_size_add(size_t left, size_t right, size_t *r_sum)
{
  if (left > SIZE_MAX-right) {
    errno = EOVERFLOW;
    return 0;
  }

  *r_sum = left+right;
  return 1;
}


static inline int gk_size_mul(size_t left, size_t right, size_t *r_product)
{
  if (left != 0 && right > SIZE_MAX/left) {
    errno = EOVERFLOW;
    return 0;
  }

  *r_product = left*right;
  return 1;
}


/* Array allocation keeps arithmetic and allocation failures on the local
   return path. Raw allocation preserves EOVERFLOW and EINVAL and uses ENOMEM
   for allocator failure. */
static inline void *gk_malloc_array_nosignal(size_t n, size_t elmlen)
{
  size_t nbytes;

  if (!gk_size_mul(n, elmlen, &nbytes))
    return NULL;

  return gk_malloc_nosignal(nbytes);
}


static inline void *gk_realloc_array_nosignal(void *oldptr, size_t n,
    size_t elmlen)
{
  size_t nbytes;

  if (!gk_size_mul(n, elmlen, &nbytes))
    return NULL;

  return gk_realloc_nosignal(oldptr, nbytes);
}


/* The global allocation tracker cannot grow itself through gk_realloc.
   Reserve with the raw allocator and commit its pointer and capacity only
   after realloc succeeds. */
static inline int gk_gkmcoreEnsureCapacity(gk_mcore_t *mcore)
{
  gk_mop_t *mops;
  size_t nbytes, nmops;

  if (mcore->cmop < mcore->nmops)
    return 1;

  if (mcore->nmops == 0)
    nmops = 2048;
  else if (!gk_size_add(mcore->nmops, mcore->nmops, &nmops))
    return 0;
  if (!gk_size_mul(nmops, sizeof(gk_mop_t), &nbytes))
    return 0;

  mops = (gk_mop_t *)realloc(mcore->mops, nbytes);
  if (mops == NULL) {
    if (errno == 0)
      errno = ENOMEM;
    return 0;
  }

  mcore->mops = mops;
  mcore->nmops = nmops;
  return 1;
}


/* Call only after gk_gkmcoreEnsureCapacity has reserved the next slot. */
static inline void gk_gkmcoreAddReserved(gk_mcore_t *mcore, int type,
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
GKLIB_NO_EXPORT void *gk_realloc_mcore(void *oldptr, size_t nbytes,
    const char *msg);

#endif
