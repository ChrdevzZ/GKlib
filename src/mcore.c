/*!
\file 
\brief Functions dealing with creating and allocating mcores

\date Started 5/30/11
\author George
\author Copyright 1997-2011, Regents of the University of Minnesota 
\version $Id: mcore.c 13953 2013-03-30 16:20:07Z karypis $
*/

#include <GKlib.h>
#include "memory_internal.h"


static int gk_mcoreEnsureCapacity(gk_mcore_t *mcore)
{
  gk_mop_t *mops;
  size_t nmops;

  if (mcore->cmop < mcore->nmops)
    return 1;

  if (mcore->nmops > SIZE_MAX/2) {
    gk_errexit(SIGMEM, "***Memory allocation for mcore failed.\n");
    return 0;
  }
  nmops = (mcore->nmops == 0 ? 2048 : 2*mcore->nmops);
  if (nmops > SIZE_MAX/sizeof(gk_mop_t)) {
    gk_errexit(SIGMEM, "***Memory allocation for mcore failed.\n");
    return 0;
  }

  mops = (gk_mop_t *)gk_realloc_mcore(mcore->mops, nmops*sizeof(gk_mop_t),
      "gk_mcoreEnsureCapacity: mcore->mops");
  if (mops == NULL)
    return 0;

  mcore->mops = mops;
  mcore->nmops = nmops;
  return 1;
}


static void gk_mcoreAddReserved(gk_mcore_t *mcore, int type,
    size_t nbytes, void *ptr)
{
  mcore->mops[mcore->cmop].type   = type;
  mcore->mops[mcore->cmop].nbytes = nbytes;
  mcore->mops[mcore->cmop].ptr    = ptr;
  mcore->cmop++;

  switch (type) {
    case GK_MOPT_MARK:
      break;

    case GK_MOPT_CORE:
      mcore->num_callocs++;
      mcore->size_callocs += nbytes;
      mcore->cur_callocs  += nbytes;
      if (mcore->max_callocs < mcore->cur_callocs)
        mcore->max_callocs = mcore->cur_callocs;
      break;

    case GK_MOPT_HEAP:
      mcore->num_hallocs++;
      mcore->size_hallocs += nbytes;
      mcore->cur_hallocs  += nbytes;
      if (mcore->max_hallocs < mcore->cur_hallocs)
        mcore->max_hallocs = mcore->cur_hallocs;
      break;
  }
}


/*************************************************************************/
/*! This function creates an mcore 
 */
/*************************************************************************/
gk_mcore_t *gk_mcoreCreate(size_t coresize)
{
  gk_mcore_t *mcore;

  mcore = (gk_mcore_t *)gk_malloc(sizeof(gk_mcore_t), "gk_mcoreCreate: mcore");
  if (mcore == NULL)
    return NULL;
  memset(mcore, 0, sizeof(gk_mcore_t));

  mcore->coresize = coresize;
  mcore->corecpos = 0;

  mcore->core = (coresize == 0 ? NULL : gk_malloc(mcore->coresize, "gk_mcoreCreate: core"));
  if (coresize != 0 && mcore->core == NULL) {
    gk_free((void **)&mcore, LTERM);
    return NULL;
  }

  /* allocate the memory for keeping track of malloc ops */
  mcore->nmops = 2048;
  mcore->cmop  = 0;
  mcore->mops  = (gk_mop_t *)gk_malloc(mcore->nmops*sizeof(gk_mop_t), "gk_mcoreCreate: mcore->mops");
  if (mcore->mops == NULL) {
    gk_free((void **)&mcore->core, &mcore, LTERM);
    return NULL;
  }

  return mcore;
}


/*************************************************************************/
/*! This function creates an mcore. This version is used for gkmcore.
 */
/*************************************************************************/
gk_mcore_t *gk_gkmcoreCreate(void)
{
  gk_mcore_t *mcore;

  if ((mcore = (gk_mcore_t *)malloc(sizeof(gk_mcore_t))) == NULL)
    return NULL;
  memset(mcore, 0, sizeof(gk_mcore_t));

  /* allocate the memory for keeping track of malloc ops */
  mcore->nmops = 2048;
  mcore->cmop  = 0;
  if ((mcore->mops = (gk_mop_t *)malloc(mcore->nmops*sizeof(gk_mop_t))) == NULL) {
    free(mcore);
    return NULL;
  }

  return mcore;
}


/*************************************************************************/
/*! This function destroys an mcore.
 */
/*************************************************************************/
void gk_mcoreDestroy(gk_mcore_t **r_mcore, int showstats)
{
  gk_mcore_t *mcore = *r_mcore;

  if (mcore == NULL)
    return;

  if (showstats)
    printf("\n gk_mcore statistics\n" 
           "           coresize: %12zu         nmops: %12zu  cmop: %6zu\n"
           "        num_callocs: %12zu   num_hallocs: %12zu\n"
           "       size_callocs: %12zu  size_hallocs: %12zu\n"
           "        cur_callocs: %12zu   cur_hallocs: %12zu\n"
           "        max_callocs: %12zu   max_hallocs: %12zu\n",
           mcore->coresize, mcore->nmops, mcore->cmop,
           mcore->num_callocs,  mcore->num_hallocs,
           mcore->size_callocs, mcore->size_hallocs,
           mcore->cur_callocs,  mcore->cur_hallocs,
           mcore->max_callocs,  mcore->max_hallocs);

  if (mcore->cur_callocs != 0 || mcore->cur_hallocs != 0 || mcore->cmop != 0) {
    printf("***Warning: mcore memory was not fully freed when destroyed.\n"
           " cur_callocs: %6zu  cur_hallocs: %6zu cmop: %6zu\n",
           mcore->cur_callocs,  mcore->cur_hallocs, mcore->cmop);
  }

  gk_free((void **)&mcore->core, &mcore->mops, &mcore, LTERM);

  *r_mcore = mcore;
}


/*************************************************************************/
/*! This function destroys an mcore. This version is for gkmcore.
 */
/*************************************************************************/
void gk_gkmcoreDestroy(gk_mcore_t **r_mcore, int showstats)
{
  gk_mcore_t *mcore = *r_mcore;

  if (mcore == NULL)
    return;

  if (showstats)
    printf("\n gk_mcore statistics\n" 
           "         nmops: %12zu  cmop: %6zu\n"
           "   num_hallocs: %12zu\n"
           "  size_hallocs: %12zu\n"
           "   cur_hallocs: %12zu\n"
           "   max_hallocs: %12zu\n",
           mcore->nmops, mcore->cmop,
           mcore->num_hallocs,
           mcore->size_hallocs,
           mcore->cur_hallocs,
           mcore->max_hallocs);

  if (mcore->cur_hallocs != 0 || mcore->cmop != 0) {
    printf("***Warning: mcore memory was not fully freed when destroyed.\n"
           " cur_hallocs: %6zu cmop: %6zu\n",
           mcore->cur_hallocs, mcore->cmop);
  }

  free(mcore->mops);
  free(mcore);

  *r_mcore = NULL;
}


/*************************************************************************/
/*! This function allocate space from the core/heap 
 */
/*************************************************************************/
void *gk_mcoreMalloc(gk_mcore_t *mcore, size_t nbytes)
{
  void *ptr;

  /* pad to make pointers 8-byte aligned */
  nbytes += (nbytes%8 == 0 ? 0 : 8 - nbytes%8);

  if (!gk_mcoreEnsureCapacity(mcore))
    return NULL;

  if (mcore->corecpos + nbytes < mcore->coresize) {
    /* service this request from the core */
    ptr = ((char *)mcore->core)+mcore->corecpos;
    mcore->corecpos += nbytes;

    gk_mcoreAddReserved(mcore, GK_MOPT_CORE, nbytes, ptr);
  }
  else {
    /* service this request from the heap */
    ptr = gk_malloc(nbytes, "gk_mcoremalloc: ptr");
    if (ptr == NULL)
      return NULL;

    gk_mcoreAddReserved(mcore, GK_MOPT_HEAP, nbytes, ptr);
  }

  /*
  printf("MCMALLOC: %zu %d %8zu\n", mcore->cmop-1, 
      mcore->mops[mcore->cmop-1].type, mcore->mops[mcore->cmop-1].nbytes);
  */

  return ptr;
}


/*************************************************************************/
/*! This function sets a marker in the stack of malloc ops to be used
    subsequently for freeing purposes 
 */
/*************************************************************************/
void gk_mcorePush(gk_mcore_t *mcore)
{
  gk_mcoreAdd(mcore, GK_MOPT_MARK, 0, NULL);
  /* printf("MCPPUSH:   %zu\n", mcore->cmop-1); */
}


/*************************************************************************/
/*! This function sets a marker in the stack of malloc ops to be used
    subsequently for freeing purposes. This is the gkmcore version.
 */
/*************************************************************************/
void gk_gkmcorePush(gk_mcore_t *mcore)
{
  gk_gkmcoreAdd(mcore, GK_MOPT_MARK, 0, NULL);
  /* printf("MCPPUSH:   %zu\n", mcore->cmop-1); */
}


/*************************************************************************/
/*! This function frees all mops since the last push 
 */
/*************************************************************************/
void gk_mcorePop(gk_mcore_t *mcore)
{
  while (mcore->cmop > 0) {
    size_t mop = mcore->cmop-1;

    switch (mcore->mops[mop].type) {
      case GK_MOPT_MARK: /* push marker */
        mcore->cmop--;
        goto DONE;
        break; 

      case GK_MOPT_CORE: /* core free */
        if (mcore->corecpos < mcore->mops[mop].nbytes) {
          errexit("Internal Error: wspace's core is about to be over-freed [%zu, %zu, %zd]\n",
              mcore->coresize, mcore->corecpos, mcore->mops[mop].nbytes);
          return;
        }

        mcore->corecpos    -= mcore->mops[mop].nbytes;
        mcore->cur_callocs -= mcore->mops[mop].nbytes;
        mcore->cmop--;
        break;

      case GK_MOPT_HEAP: /* heap free */
        gk_free((void **)&mcore->mops[mop].ptr, LTERM);
        if (mcore->mops[mop].ptr != NULL)
          return;
        mcore->cur_hallocs -= mcore->mops[mop].nbytes;
        mcore->cmop--;
        break;

      default:
        gk_errexit(SIGMEM, "Unknown mop type of %d\n", mcore->mops[mop].type);
        return;
    }
  }

DONE:
  ;
  /*printf("MCPPOP:    %zu\n", mcore->cmop); */
}


/*************************************************************************/
/*! This function frees all mops since the last push. This version is
    for poping the gkmcore and it uses free instead of gk_free.
 */
/*************************************************************************/
void gk_gkmcorePop(gk_mcore_t *mcore)
{
  while (mcore->cmop > 0) {
    mcore->cmop--;
    switch (mcore->mops[mcore->cmop].type) {
      case GK_MOPT_MARK: /* push marker */
        goto DONE;
        break; 

      case GK_MOPT_HEAP: /* heap free */
        free(mcore->mops[mcore->cmop].ptr);
        mcore->cur_hallocs -= mcore->mops[mcore->cmop].nbytes;
        break;

      default:
        gk_errexit(SIGMEM, "Unknown mop type of %d\n", mcore->mops[mcore->cmop].type);
    }
  }

DONE:
  ;
}


/*************************************************************************/
/*! Adds a memory allocation at the end of the list.
 */
/*************************************************************************/
void gk_mcoreAdd(gk_mcore_t *mcore, int type, size_t nbytes, void *ptr)
{
  if (type != GK_MOPT_MARK && type != GK_MOPT_CORE && type != GK_MOPT_HEAP) {
    gk_errexit(SIGMEM, "Incorrect mcore type operation.\n");
    return;
  }

  if (!gk_mcoreEnsureCapacity(mcore))
    return;

  gk_mcoreAddReserved(mcore, type, nbytes, ptr);
}


/*************************************************************************/
/*! Adds a memory allocation at the end of the list. This is the gkmcore
    version.
 */
/*************************************************************************/
void gk_gkmcoreAdd(gk_mcore_t *mcore, int type, size_t nbytes, void *ptr)
{
  if (type != GK_MOPT_MARK && type != GK_MOPT_HEAP) {
    gk_errexit(SIGMEM, "Incorrect mcore type operation.\n");
    return;
  }

  if (!gk_gkmcoreEnsureCapacity(mcore)) {
    gk_errexit(SIGMEM, "***Memory allocation for gkmcore failed.\n");
    return;
  }

  gk_gkmcoreAddReserved(mcore, type, nbytes, ptr);
}


/*************************************************************************/
/*! This function deletes the mop associated with the supplied pointer.
    The mop must be a heap allocation in the active frame. Invalid deletions
    report SIGMEM and leave the mcore unchanged.
 */
/*************************************************************************/
void gk_mcoreDel(gk_mcore_t *mcore, void *ptr)
{
  int i;

  for (i=mcore->cmop-1; i>=0; i--) {
    if (mcore->mops[i].type == GK_MOPT_MARK) {
      gk_errexit(SIGMEM, "Could not find pointer %p in mcore\n", ptr);
      return;
    }

    if (mcore->mops[i].ptr == ptr) {
      if (mcore->mops[i].type != GK_MOPT_HEAP) {
        gk_errexit(SIGMEM, "Trying to delete a non-HEAP mop.\n");
        return;
      }

      mcore->cur_hallocs -= mcore->mops[i].nbytes;
      mcore->mops[i] = mcore->mops[--mcore->cmop];
      return;
    }
  }

  gk_errexit(SIGMEM, "mcoreDel should never have been here!\n");
}


/*************************************************************************/
/*! This function deletes the mop associated with the supplied pointer.
    The mop must be a heap allocation in the active frame. Invalid deletions
    report SIGMEM and leave the mcore unchanged.
    This is the gkmcore version.
 */
/*************************************************************************/
void gk_gkmcoreDel(gk_mcore_t *mcore, void *ptr)
{
  int i;

  for (i=mcore->cmop-1; i>=0; i--) {
    if (mcore->mops[i].type == GK_MOPT_MARK) {
      gk_errexit(SIGMEM, "Could not find pointer %p in mcore\n", ptr);
      return;
    }

    if (mcore->mops[i].ptr == ptr) {
      if (mcore->mops[i].type != GK_MOPT_HEAP) {
        gk_errexit(SIGMEM, "Trying to delete a non-HEAP mop.\n");
        return;
      }

      mcore->cur_hallocs -= mcore->mops[i].nbytes;
      mcore->mops[i] = mcore->mops[--mcore->cmop];
      return;
    }
  }

  gk_errexit(SIGMEM, "gkmcoreDel should never have been here!\n");
}

