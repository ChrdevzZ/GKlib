/*!
\file  memory.c
\brief This file contains various allocation routines 

The allocation routines included are for 1D and 2D arrays of the 
most datatypes that GKlib support. Many of these routines are 
defined with the help of the macros in gk_mkmemory.h. These macros
can be used to define other memory allocation routines.

\date   Started 4/3/2007
\author George
\version\verbatim $Id: memory.c 21050 2017-05-25 03:53:58Z karypis $ \endverbatim
*/


#include <GKlib.h>
#include "memory_internal.h"

/* This is for the global mcore that tracks all heap allocations */
static GKLIB_THREAD_LOCAL gk_mcore_t *gkmcore = NULL;


static int gk_gkmcoreFindQuiet(gk_mcore_t *mcore, void *ptr, size_t *r_mop)
{
  size_t i;

  for (i=mcore->cmop; i>0; ) {
    i--;
    if (mcore->mops[i].type == GK_MOPT_MARK)
      return 0;
    if (mcore->mops[i].ptr == ptr &&
        mcore->mops[i].type == GK_MOPT_HEAP) {
      *r_mop = i;
      return 1;
    }
  }

  return 0;
}


static int gk_gkmcoreFindAny(gk_mcore_t *mcore, void *ptr, size_t *r_mop)
{
  size_t i;

  for (i=mcore->cmop; i>0; ) {
    i--;
    if (mcore->mops[i].type == GK_MOPT_HEAP &&
        mcore->mops[i].ptr == ptr) {
      *r_mop = i;
      return 1;
    }
  }

  return 0;
}


static void gk_gkmcoreUpdateRealloc(gk_mcore_t *mcore, size_t mop,
    size_t nbytes, void *ptr)
{
  size_t oldnbytes = mcore->mops[mop].nbytes;

  mcore->mops[mop].nbytes = nbytes;
  mcore->mops[mop].ptr = ptr;
  mcore->num_hallocs++;
  mcore->size_hallocs += nbytes;
  mcore->cur_hallocs -= oldnbytes;
  mcore->cur_hallocs += nbytes;
  if (mcore->max_hallocs < mcore->cur_hallocs)
    mcore->max_hallocs = mcore->cur_hallocs;
}


/*************************************************************************/
/*! Checks whether a new tracked heap allocation fits every statistic. */
/*************************************************************************/
static int gk_gkmcoreCanAdd(gk_mcore_t *mcore, size_t nbytes)
{
  return nbytes <= (size_t)PTRDIFF_MAX &&
      mcore->num_hallocs < SIZE_MAX &&
      mcore->size_hallocs <= SIZE_MAX-nbytes &&
      mcore->cur_hallocs <= SIZE_MAX-nbytes;
}


/*************************************************************************/
/*! Checks a tracked reallocation without changing its record or counters.

    Shrinks always fit the current-byte counter. Growth is accepted only when
    the delta and cumulative allocation statistics remain representable.
*/
/*************************************************************************/
static int gk_gkmcoreCanRealloc(gk_mcore_t *mcore, size_t mop,
    size_t nbytes)
{
  size_t oldnbytes = mcore->mops[mop].nbytes;

  return nbytes <= (size_t)PTRDIFF_MAX &&
      mcore->num_hallocs < SIZE_MAX &&
      mcore->size_hallocs <= SIZE_MAX-nbytes &&
      (nbytes <= oldnbytes ||
       mcore->cur_hallocs <= SIZE_MAX-(nbytes-oldnbytes));
}


void *gk_realloc_mcore(void *oldptr, size_t nbytes, const char *msg)
{
  void *ptr;
  int had_oldptr = (oldptr != NULL), saved_errno;
  int tracked = 0;
  size_t mop = 0;

  if (nbytes == 0)
    nbytes++;

  if (gkmcore != NULL) {
    if (had_oldptr) {
      tracked = gk_gkmcoreFindAny(gkmcore, oldptr, &mop);
      if (tracked && !gk_gkmcoreCanRealloc(gkmcore, mop, nbytes)) {
        errno = EOVERFLOW;
        gk_errexit(SIGMEM, "***Memory allocation statistics overflow.");
        errno = EOVERFLOW;
        return NULL;
      }
    }
    else if (!gk_gkmcoreCanAdd(gkmcore, nbytes)) {
      errno = EOVERFLOW;
      gk_errexit(SIGMEM, "***Memory allocation statistics overflow.");
      errno = EOVERFLOW;
      return NULL;
    }
    else if (!gk_gkmcoreEnsureCapacity(gkmcore)) {
      saved_errno = errno != 0 ? errno : ENOMEM;
      fprintf(stderr, "   Maximum memory used: %10zu bytes\n", gk_GetMaxMemoryUsed());
      fprintf(stderr, "   Current memory used: %10zu bytes\n", gk_GetCurMemoryUsed());
      errno = saved_errno;
      gk_errexit(SIGMEM, "***Memory allocation for gkmcore failed.");
      errno = saved_errno;
      return NULL;
    }
  }

  ptr = (void *)realloc(oldptr, nbytes);
  if (ptr == NULL) {
    saved_errno = errno != 0 ? errno : ENOMEM;
    fprintf(stderr, "   Maximum memory used: %10zu bytes\n", gk_GetMaxMemoryUsed());
    fprintf(stderr, "   Current memory used: %10zu bytes\n", gk_GetCurMemoryUsed());
    errno = saved_errno;
    gk_errexit(SIGMEM, "***Memory realloc failed for %s. " "Requested size: %zu bytes",
        msg, nbytes);
    errno = saved_errno;
    return NULL;
  }

  if (gkmcore != NULL) {
    if (tracked)
      gk_gkmcoreUpdateRealloc(gkmcore, mop, nbytes, ptr);
    else if (!had_oldptr)
      gk_gkmcoreAddReserved(gkmcore, GK_MOPT_HEAP, nbytes, ptr);
  }

  return ptr;
}


/*************************************************************************/
/*! Define the set of memory allocation routines for each data type */
/**************************************************************************/
GK_MKALLOC(gk_c,    char)
GK_MKALLOC(gk_i,    int)
GK_MKALLOC(gk_i8,   int8_t)
GK_MKALLOC(gk_i16,  int16_t)
GK_MKALLOC(gk_i32,  int32_t)
GK_MKALLOC(gk_i64,  int64_t)
GK_MKALLOC(gk_ui8,  uint8_t)
GK_MKALLOC(gk_ui16, uint16_t)
GK_MKALLOC(gk_ui32, uint32_t)
GK_MKALLOC(gk_ui64, uint64_t)
GK_MKALLOC(gk_z,    ssize_t)
GK_MKALLOC(gk_zu,   size_t)
GK_MKALLOC(gk_f,    float)
GK_MKALLOC(gk_d,    double)
GK_MKALLOC(gk_idx,  gk_idx_t)

GK_MKALLOC(gk_ckv,   gk_ckv_t)
GK_MKALLOC(gk_ikv,   gk_ikv_t)
GK_MKALLOC(gk_i8kv,  gk_i8kv_t)
GK_MKALLOC(gk_i16kv, gk_i16kv_t)
GK_MKALLOC(gk_i32kv, gk_i32kv_t)
GK_MKALLOC(gk_i64kv, gk_i64kv_t)
GK_MKALLOC(gk_zkv,   gk_zkv_t)
GK_MKALLOC(gk_zukv,  gk_zukv_t)
GK_MKALLOC(gk_fkv,   gk_fkv_t)
GK_MKALLOC(gk_dkv,   gk_dkv_t)
GK_MKALLOC(gk_skv,   gk_skv_t)
GK_MKALLOC(gk_idxkv, gk_idxkv_t)






/*************************************************************************/
/*! This function allocates a two-dimensional matrix.
  */
/*************************************************************************/
void gk_AllocMatrix(void ***r_matrix, size_t elmlen, size_t ndim1, size_t ndim2)
{
  int saved_errno;
  size_t i, j, matrixbytes, rowbytes=0;
  void **matrix;

  if (r_matrix == NULL) {
    errno = EINVAL;
    gk_errexit(SIGERR, "gk_AllocMatrix: output pointer is NULL");
    errno = EINVAL;
    return;
  }
  *r_matrix = NULL;

  if (!gk_size_mul(ndim1, sizeof(void *), &matrixbytes) ||
      (ndim1 != 0 && !gk_size_mul(ndim2, elmlen, &rowbytes))) {
    gk_errexit(SIGMEM, "gk_AllocMatrix: matrix size overflow");
    errno = EOVERFLOW;
    return;
  }

  matrix = (void **)gk_malloc_nosignal(matrixbytes);
  if (matrix == NULL) {
    saved_errno = errno != 0 ? errno : ENOMEM;
    errno = saved_errno;
    gk_errexit(SIGMEM, "gk_AllocMatrix: matrix allocation failed");
    errno = saved_errno;
    return;
  }

  for (i=0; i<ndim1; i++) {
    matrix[i] = gk_malloc_nosignal(rowbytes);
    if (matrix[i] == NULL) {
      saved_errno = errno != 0 ? errno : ENOMEM;
      for (j=0; j<i; j++) 
        gk_free((void **)&matrix[j], LTERM);
      gk_free((void **)&matrix, LTERM);
      errno = saved_errno;
      gk_errexit(SIGMEM, "gk_AllocMatrix: row allocation failed");
      errno = saved_errno;
      return;
    }
  }

  *r_matrix = matrix;
}


/*************************************************************************/
/*! This function frees a two-dimensional matrix.
  */
/*************************************************************************/
void gk_FreeMatrix(void ***r_matrix, size_t ndim1, size_t ndim2)
{
  size_t i;
  void **matrix;

  (void)ndim2;

  if (r_matrix == NULL) {
    errno = EINVAL;
    gk_errexit(SIGERR, "gk_FreeMatrix: output pointer is NULL");
    errno = EINVAL;
    return;
  }
  if ((matrix = *r_matrix) == NULL)
    return;

  for (i=0; i<ndim1; i++) 
    gk_free((void **)&matrix[i], LTERM);

  gk_free((void **)r_matrix, LTERM); 

}


/*************************************************************************/
/*! This function initializes tracking of heap allocations. 
*/
/*************************************************************************/
int gk_malloc_init(void)
{
  if (gkmcore == NULL)
    gkmcore = gk_gkmcoreCreate();

  if (gkmcore == NULL)
    return 0;

  if (!gk_gkmcoreEnsureCapacity(gkmcore))
    return 0;

  gk_gkmcorePush(gkmcore);

  return 1;
}


/*************************************************************************/
/*! This function frees the memory that has been allocated since the
    last call to gk_malloc_init().
*/
/*************************************************************************/
void gk_malloc_cleanup(int showstats)
{
  if (gkmcore != NULL) {
    gk_gkmcorePop(gkmcore);
    if (gkmcore->cmop == 0) {
      gk_gkmcoreDestroy(&gkmcore, showstats);
      gkmcore = NULL;
    }
  }
}


/*************************************************************************/
/*! This function is my wrapper around malloc that provides the following
    enhancements over malloc:
    * It always allocates one byte of memory, even if 0 bytes are requested.
      This is to ensure that checks of returned values do not lead to NULL
      due to 0 bytes requested.
    * It records allocations when memory tracking is active.
*/
/**************************************************************************/
void *gk_malloc_nosignal(size_t nbytes)
{
  void *ptr=NULL;

  if (nbytes == 0)
    nbytes++;  /* Force mallocs to actually allocate some memory */

  if (gkmcore != NULL) {
    if (!gk_gkmcoreCanAdd(gkmcore, nbytes)) {
      errno = EOVERFLOW;
      return NULL;
    }
    if (!gk_gkmcoreEnsureCapacity(gkmcore)) {
      if (errno == 0)
        errno = ENOMEM;
      return NULL;
    }
  }

  ptr = (void *)malloc(nbytes);

  if (ptr == NULL) {
    errno = ENOMEM;
    return NULL;
  }

  /* add this memory allocation */
  if (gkmcore != NULL)
    gk_gkmcoreAddReserved(gkmcore, GK_MOPT_HEAP, nbytes, ptr);

  return ptr;
}


/*************************************************************************/
/*! Reports a failed allocation after the no-signal tracker path returns. */
/*************************************************************************/
void *gk_malloc(size_t nbytes, const char *msg)
{
  void *ptr;
  int saved_errno;

  ptr = gk_malloc_nosignal(nbytes);
  if (ptr == NULL) {
    saved_errno = errno != 0 ? errno : ENOMEM;
    fprintf(stderr, "   Current memory used:  %10zu bytes\n", gk_GetCurMemoryUsed());
    fprintf(stderr, "   Maximum memory used:  %10zu bytes\n", gk_GetMaxMemoryUsed());
    errno = saved_errno;
    gk_errexit(SIGMEM, "***Memory allocation failed for %s. Requested size: %zu bytes",
        msg, nbytes);
    errno = saved_errno;
  }
  return ptr;
}


/*************************************************************************/
/*! Reallocates tracked storage without invoking the signal error path. */
/*************************************************************************/
void *gk_realloc_nosignal(void *oldptr, size_t nbytes)
{
  void *ptr=NULL;
  int had_oldptr;
  size_t mop=0;

  if (nbytes == 0)
    nbytes++;  /* Force mallocs to actually allocate some memory */
  had_oldptr = (oldptr != NULL);

  if (gkmcore != NULL) {
    if (had_oldptr) {
      if (!gk_gkmcoreFindQuiet(gkmcore, oldptr, &mop)) {
        errno = EINVAL;
        return NULL;
      }
      if (!gk_gkmcoreCanRealloc(gkmcore, mop, nbytes)) {
        errno = EOVERFLOW;
        return NULL;
      }
    }
    else if (!gk_gkmcoreCanAdd(gkmcore, nbytes)) {
      errno = EOVERFLOW;
      return NULL;
    }
    else if (!gk_gkmcoreEnsureCapacity(gkmcore)) {
      if (errno == 0)
        errno = ENOMEM;
      return NULL;
    }
  }

  ptr = (void *)realloc(oldptr, nbytes);

  if (ptr == NULL) {
    errno = ENOMEM;
    return NULL;
  }

  if (gkmcore != NULL) {
    if (!had_oldptr) {
      gk_gkmcoreAddReserved(gkmcore, GK_MOPT_HEAP, nbytes, ptr);
    }
    else
      gk_gkmcoreUpdateRealloc(gkmcore, mop, nbytes, ptr);
  }

  return ptr;
}


/*************************************************************************
* This function is my wrapper around realloc
**************************************************************************/
void *gk_realloc(void *oldptr, size_t nbytes, const char *msg)
{
  void *ptr;
  int saved_errno;

  ptr = gk_realloc_nosignal(oldptr, nbytes);
  if (ptr == NULL) {
    saved_errno = errno != 0 ? errno : ENOMEM;
    fprintf(stderr, "   Maximum memory used: %10zu bytes\n", gk_GetMaxMemoryUsed());
    fprintf(stderr, "   Current memory used: %10zu bytes\n", gk_GetCurMemoryUsed());
    errno = saved_errno;
    gk_errexit(saved_errno == EINVAL ? SIGERR : SIGMEM,
        "***Memory realloc failed for %s. Requested size: %zu bytes",
        msg, nbytes);
    errno = saved_errno;
  }
  return ptr;
}


int gk_free_nosignal(void **r_ptr)
{
  size_t mop;

  if (r_ptr == NULL) {
    errno = EINVAL;
    return 0;
  }

  if (*r_ptr != NULL) {
    if (gkmcore != NULL) {
      if (!gk_gkmcoreFindQuiet(gkmcore, *r_ptr, &mop)) {
        errno = EINVAL;
        return 0;
      }
      gkmcore->cur_hallocs -= gkmcore->mops[mop].nbytes;
      gkmcore->mops[mop] = gkmcore->mops[--gkmcore->cmop];
    }

    free(*r_ptr);
  }
  *r_ptr = NULL;

  return 1;
}


static int gk_freePointer(void **ptr)
{
  int saved_errno;
  void *value;

  value = ptr == NULL ? NULL : *ptr;

  if (!gk_free_nosignal(ptr)) {
    saved_errno = errno != 0 ? errno : EINVAL;
    errno = saved_errno;
    gk_errexit(SIGERR, "Could not free pointer %p\n", value);
    errno = saved_errno;
    return 0;
  }

  return 1;
}


/*************************************************************************
* This function is my wrapper around free, allows multiple pointers    
**************************************************************************/
void gk_free(void **ptr1,...)
{
  va_list plist;
  void **ptr;

  if (!gk_freePointer(ptr1))
    return;

  va_start(plist, ptr1);
  while ((ptr = va_arg(plist, void **)) != LTERM) {
    if (!gk_freePointer(ptr)) {
      va_end(plist);
      return;
    }
  }
  va_end(plist);
}          


/*************************************************************************
* This function returns the current ammount of dynamically allocated
* memory that is used by the system
**************************************************************************/
size_t gk_GetCurMemoryUsed(void)
{
  if (gkmcore == NULL)
    return 0;
  else
    return gkmcore->cur_hallocs;
}


/*************************************************************************
* This function returns the maximum ammount of dynamically allocated 
* memory that was used by the system
**************************************************************************/
size_t gk_GetMaxMemoryUsed(void)
{
  if (gkmcore == NULL)
    return 0;
  else
    return gkmcore->max_hallocs;
}


/*************************************************************************/
/*! This function returns the VmSize and VmRSS of the calling process. */
/*************************************************************************/
void gk_GetVMInfo(size_t *vmsize, size_t *vmrss)
{
  int failed=0, saved_errno=0;
  FILE *fp;
  char fname[1024];
  size_t parsed_vmsize, parsed_vmrss;

  if (vmsize == NULL || vmrss == NULL) {
    errno = EINVAL;
    gk_errexit(SIGERR, "gk_GetVMInfo: output pointers must not be NULL");
    errno = EINVAL;
    return;
  }
  *vmsize = 0;
  *vmrss = 0;

  sprintf(fname, "/proc/%d/statm", getpid());
  fp = gk_fopen(fname, "r", "proc/pid/statm");
  if (fp == NULL)
    return;
  if (fscanf(fp, "%zu %zu", &parsed_vmsize, &parsed_vmrss) != 2) {
    saved_errno = errno != 0 ? errno : EIO;
    failed = 1;
  }
  if (fclose(fp) != 0) {
    if (!failed)
      saved_errno = errno != 0 ? errno : EIO;
    failed = 1;
  }
  if (failed) {
    errno = saved_errno;
    gk_errexit(saved_errno == ENOMEM || saved_errno == EOVERFLOW ?
        SIGMEM : SIGERR, "Failed to read values from %s", fname);
    errno = saved_errno;
    return;
  }

  *vmsize = parsed_vmsize;
  *vmrss = parsed_vmrss;

  /*
  *vmsize *= sysconf(_SC_PAGESIZE);
  *vmrss  *= sysconf(_SC_PAGESIZE);
  */

  return;
}


/*************************************************************************/
/*! This function returns the peak virtual memory of the calling process
    by reading the VmPeak field in /proc/self/status . */
/*************************************************************************/
size_t gk_GetProcVmPeak(void)
{
  int read_failed;
  FILE *fp;
  char *endptr, line[128];
  uintmax_t value;
  size_t vmpeak=0;

  if (gk_fexists("/proc/self/status")) {
    fp = gk_fopen("/proc/self/status", "r", "proc/self/status");
    if (fp == NULL)
      return 0;
    while (fgets(line, 128, fp) != NULL) {
      if (strncmp(line, "VmPeak:", 7) == 0) {
        errno = 0;
        value = strtoumax(line+7, &endptr, 10);
        if (endptr != line+7 && errno != ERANGE &&
            value <= SIZE_MAX/1024)
          vmpeak = (size_t)value*1024;
        break;
      }
    }
    read_failed = ferror(fp);
    if (fclose(fp) != 0)
      read_failed = 1;
    if (read_failed)
      vmpeak = 0;
  }

  return vmpeak;
}
