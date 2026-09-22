/*!
 * \file 
 *
 * \brief Various routines with dealing with CSR matrices
 *
 * \author George Karypis
 * \version\verbatim $Id: csr.c 21044 2017-05-24 22:50:32Z karypis $ \endverbatim
 */

#include <GKlib.h>
#include "io_internal.h"
#include "memory_internal.h"

#define OMPMINOPS       50000

static void *gk_csr_MallocNoSignal(size_t nbytes, int *r_failed);
static gk_csr_t *gk_csr_CreateNoSignal(int *r_failed);

/*************************************************************************/
/*! Allocate memory for a CSR matrix and initializes it 
    \returns the allocated matrix. The various fields are set to NULL.
*/
/**************************************************************************/
gk_csr_t *gk_csr_Create(void)
{
  gk_csr_t *mat=NULL;

  if ((mat = (gk_csr_t *)gk_malloc(sizeof(gk_csr_t), "gk_csr_Create: mat")))
    gk_csr_Init(mat);

  return mat;
}


/*************************************************************************/
/*! Initializes the matrix 
    \param mat is the matrix to be initialized.
*/
/*************************************************************************/
void gk_csr_Init(gk_csr_t *mat)
{
  memset(mat, 0, sizeof(gk_csr_t));
  mat->nrows = mat->ncols = 0;
}


/*************************************************************************/
/*! Frees all the memory allocated for matrix.
    \param mat is the matrix to be freed.
*/
/*************************************************************************/
void gk_csr_Free(gk_csr_t **mat)
{
  if (*mat == NULL)
    return;
  gk_csr_FreeContents(*mat);
  gk_free((void **)mat, LTERM);
}


/*************************************************************************/
/*! Frees only the memory allocated for the matrix's different fields and
    sets them to NULL.
    \param mat is the matrix whose contents will be freed.
*/    
/*************************************************************************/
void gk_csr_FreeContents(gk_csr_t *mat)
{
  gk_free((void *)&mat->rowptr, &mat->rowind, &mat->rowval, 
      &mat->rowids, &mat->rlabels, &mat->rmap,
      &mat->colptr, &mat->colind, &mat->colval, 
      &mat->colids, &mat->clabels, &mat->cmap,
      &mat->rnorms, &mat->cnorms, &mat->rsums, &mat->csums, 
      &mat->rsizes, &mat->csizes, &mat->rvols, &mat->cvols, 
      &mat->rwgts, &mat->cwgts, 
          LTERM);
}


/*************************************************************************/
/*! Validates one sparse row- or column-oriented view. */
/*************************************************************************/
static int gk_csr_ValidateSparseView(int32_t dimension,
    int32_t other_dimension, ssize_t *pointers, int32_t *indices,
    float *values, ssize_t *r_nnz)
{
  ssize_t i, nnz;

  if (pointers == NULL || pointers[0] != 0) {
    errno = EINVAL;
    return 0;
  }
  if ((size_t)dimension+1 > SIZE_MAX/sizeof(ssize_t)) {
    errno = EOVERFLOW;
    return 0;
  }
  for (i=0; i<dimension; i++) {
    if (pointers[i] < 0 || pointers[i] > pointers[i+1]) {
      errno = EINVAL;
      return 0;
    }
  }
  nnz = pointers[dimension];
  if (nnz < 0) {
    errno = EINVAL;
    return 0;
  }
  if ((size_t)nnz > SIZE_MAX/sizeof(int32_t) ||
      (values != NULL && (size_t)nnz > SIZE_MAX/sizeof(float))) {
    errno = EOVERFLOW;
    return 0;
  }
  if ((nnz > 0 || values != NULL) && indices == NULL) {
    errno = EINVAL;
    return 0;
  }
  for (i=0; i<nnz; i++) {
    if (indices[i] < 0 || indices[i] >= other_dimension) {
      errno = EINVAL;
      return 0;
    }
  }

  *r_nnz = nnz;
  return 1;
}


/*************************************************************************/
/*! Validates all sparse views present in a matrix. */
/*************************************************************************/
static int gk_csr_ValidateStructure(gk_csr_t *mat, ssize_t *r_rownnz,
    ssize_t *r_colnnz)
{
  if (mat == NULL || mat->nrows < 0 || mat->ncols < 0 ||
      (mat->rowptr == NULL &&
       (mat->rowind != NULL || mat->rowval != NULL)) ||
      (mat->colptr == NULL &&
       (mat->colind != NULL || mat->colval != NULL))) {
    errno = EINVAL;
    return 0;
  }
  *r_rownnz = *r_colnnz = 0;
  if (mat->rowptr != NULL &&
      !gk_csr_ValidateSparseView(mat->nrows, mat->ncols, mat->rowptr,
          mat->rowind, mat->rowval, r_rownnz))
    return 0;
  if (mat->colptr != NULL &&
      !gk_csr_ValidateSparseView(mat->ncols, mat->nrows, mat->colptr,
          mat->colind, mat->colval, r_colnnz))
    return 0;
  return 1;
}


/*************************************************************************/
/*! Validates one side of a row/column permutation. */
/*************************************************************************/
static int gk_csr_ValidatePermutation(int32_t *permutation, int32_t nrows,
    int32_t *seen)
{
  int32_t i;

  memset(seen, 0, (size_t)nrows*sizeof(int32_t));
  for (i=0; i<nrows; i++) {
    if (permutation[i] < 0 || permutation[i] >= nrows ||
        seen[permutation[i]]) {
      errno = EINVAL;
      return 0;
    }
    seen[permutation[i]] = 1;
  }
  return 1;
}


/*************************************************************************/
/*! Copies tracked matrix storage without raising a signal. */
/*************************************************************************/
static void *gk_csr_CopyNoSignal(const void *source, size_t count,
    size_t element_size, int *r_failed)
{
  void *destination;

  if (element_size != 0 && count > SIZE_MAX/element_size) {
    errno = EOVERFLOW;
    *r_failed = 1;
    return NULL;
  }
  destination = gk_csr_MallocNoSignal(count*element_size, r_failed);
  if (destination != NULL && count != 0)
    memcpy(destination, source, count*element_size);
  return destination;
}


/*************************************************************************/
/*! Cleans up a failed matrix transformation before reporting the error. */
/*************************************************************************/
static gk_csr_t *gk_csr_TransformError(gk_csr_t **mat, int saved_errno,
    int allocation_failed, const char *operation)
{
  gk_csr_Free(mat);
  errno = saved_errno != 0 ? saved_errno : EINVAL;
  gk_errexit(allocation_failed || saved_errno == ENOMEM ||
      saved_errno == EOVERFLOW ? SIGMEM : SIGERR,
      "Failed to %s a matrix.\n", operation);
  errno = saved_errno != 0 ? saved_errno : EINVAL;
  return NULL;
}


/*************************************************************************/
/*! Returns a copy of a matrix.
    \param mat is the matrix to be duplicated.
    \returns the newly created copy of the matrix.
*/
/**************************************************************************/
gk_csr_t *gk_csr_Dup(gk_csr_t *mat)
{
  ssize_t rownnz, colnnz;
  int allocation_failed=0, saved_errno=0;
  size_t nrows, ncols;
  gk_csr_t *nmat=NULL;

  if (!gk_csr_ValidateStructure(mat, &rownnz, &colnnz)) {
    saved_errno = errno;
    goto failure;
  }
  nrows = (size_t)mat->nrows;
  ncols = (size_t)mat->ncols;
  nmat = gk_csr_CreateNoSignal(&allocation_failed);
  if (nmat == NULL)
    goto allocation_failure;
  nmat->nrows = mat->nrows;
  nmat->ncols = mat->ncols;

  if (mat->rowptr)
    nmat->rowptr = (ssize_t *)gk_csr_CopyNoSignal(mat->rowptr, nrows+1,
        sizeof(ssize_t), &allocation_failed);
  if (mat->rowind)
    nmat->rowind = (int32_t *)gk_csr_CopyNoSignal(mat->rowind,
        (size_t)rownnz, sizeof(int32_t), &allocation_failed);
  if (mat->rowval)
    nmat->rowval = (float *)gk_csr_CopyNoSignal(mat->rowval,
        (size_t)rownnz, sizeof(float), &allocation_failed);
  if (mat->rowids)
    nmat->rowids = (int32_t *)gk_csr_CopyNoSignal(mat->rowids, nrows,
        sizeof(int32_t), &allocation_failed);
  if (mat->rlabels)
    nmat->rlabels = (int32_t *)gk_csr_CopyNoSignal(mat->rlabels, nrows,
        sizeof(int32_t), &allocation_failed);
  if (mat->rmap)
    nmat->rmap = (int32_t *)gk_csr_CopyNoSignal(mat->rmap, nrows,
        sizeof(int32_t), &allocation_failed);
  if (mat->rnorms)
    nmat->rnorms = (float *)gk_csr_CopyNoSignal(mat->rnorms, nrows,
        sizeof(float), &allocation_failed);
  if (mat->rsums)
    nmat->rsums = (float *)gk_csr_CopyNoSignal(mat->rsums, nrows,
        sizeof(float), &allocation_failed);
  if (mat->rsizes)
    nmat->rsizes = (float *)gk_csr_CopyNoSignal(mat->rsizes, nrows,
        sizeof(float), &allocation_failed);
  if (mat->rvols)
    nmat->rvols = (float *)gk_csr_CopyNoSignal(mat->rvols, nrows,
        sizeof(float), &allocation_failed);
  if (mat->rwgts)
    nmat->rwgts = (float *)gk_csr_CopyNoSignal(mat->rwgts, nrows,
        sizeof(float), &allocation_failed);

  if (mat->colptr)
    nmat->colptr = (ssize_t *)gk_csr_CopyNoSignal(mat->colptr, ncols+1,
        sizeof(ssize_t), &allocation_failed);
  if (mat->colind)
    nmat->colind = (int32_t *)gk_csr_CopyNoSignal(mat->colind,
        (size_t)colnnz, sizeof(int32_t), &allocation_failed);
  if (mat->colval)
    nmat->colval = (float *)gk_csr_CopyNoSignal(mat->colval,
        (size_t)colnnz, sizeof(float), &allocation_failed);
  if (mat->colids)
    nmat->colids = (int32_t *)gk_csr_CopyNoSignal(mat->colids, ncols,
        sizeof(int32_t), &allocation_failed);
  if (mat->clabels)
    nmat->clabels = (int32_t *)gk_csr_CopyNoSignal(mat->clabels, ncols,
        sizeof(int32_t), &allocation_failed);
  if (mat->cmap)
    nmat->cmap = (int32_t *)gk_csr_CopyNoSignal(mat->cmap, ncols,
        sizeof(int32_t), &allocation_failed);
  if (mat->cnorms)
    nmat->cnorms = (float *)gk_csr_CopyNoSignal(mat->cnorms, ncols,
        sizeof(float), &allocation_failed);
  if (mat->csums)
    nmat->csums = (float *)gk_csr_CopyNoSignal(mat->csums, ncols,
        sizeof(float), &allocation_failed);
  if (mat->csizes)
    nmat->csizes = (float *)gk_csr_CopyNoSignal(mat->csizes, ncols,
        sizeof(float), &allocation_failed);
  if (mat->cvols)
    nmat->cvols = (float *)gk_csr_CopyNoSignal(mat->cvols, ncols,
        sizeof(float), &allocation_failed);
  if (mat->cwgts)
    nmat->cwgts = (float *)gk_csr_CopyNoSignal(mat->cwgts, ncols,
        sizeof(float), &allocation_failed);
  if (allocation_failed)
    goto allocation_failure;

  return nmat;

allocation_failure:
  saved_errno = errno != 0 ? errno : ENOMEM;
  allocation_failed = 1;

failure:
  return gk_csr_TransformError(&nmat, saved_errno,
      allocation_failed, "duplicate");
}


/*************************************************************************/
/*! Returns a submatrix containint a set of consecutive rows.
    \param mat is the original matrix.
    \param rstart is the starting row.
    \param nrows is the number of rows from rstart to extract.
    \returns the row structure of the newly created submatrix.
*/
/**************************************************************************/
gk_csr_t *gk_csr_ExtractSubmatrix(gk_csr_t *mat, int rstart, int nrows)
{
  ssize_t i, first, last, nnz, rownnz, colnnz;
  int allocation_failed=0, saved_errno=0;
  size_t nrows_size;
  gk_csr_t *nmat=NULL;

  if (!gk_csr_ValidateStructure(mat, &rownnz, &colnnz)) {
    saved_errno = errno;
    goto failure;
  }
  if (mat->rowptr == NULL || rstart < 0 || nrows < 0 ||
      rstart > mat->nrows || nrows > mat->nrows-rstart) {
    saved_errno = EINVAL;
    goto failure;
  }
  first = mat->rowptr[rstart];
  last = mat->rowptr[rstart+nrows];
  nnz = last-first;
  nrows_size = (size_t)nrows;

  nmat = gk_csr_CreateNoSignal(&allocation_failed);
  if (nmat == NULL)
    goto allocation_failure;
  nmat->nrows = nrows;
  nmat->ncols = mat->ncols;
  nmat->rowptr = (ssize_t *)gk_csr_CopyNoSignal(mat->rowptr+rstart,
      nrows_size+1, sizeof(ssize_t), &allocation_failed);
  if (mat->rowind)
    nmat->rowind = (int32_t *)gk_csr_CopyNoSignal(mat->rowind+first,
        (size_t)nnz, sizeof(int32_t), &allocation_failed);
  if (mat->rowval)
    nmat->rowval = (float *)gk_csr_CopyNoSignal(mat->rowval+first,
        (size_t)nnz, sizeof(float), &allocation_failed);
  if (mat->rowids)
    nmat->rowids = (int32_t *)gk_csr_CopyNoSignal(mat->rowids+rstart,
        nrows_size, sizeof(int32_t), &allocation_failed);
  if (mat->rlabels)
    nmat->rlabels = (int32_t *)gk_csr_CopyNoSignal(mat->rlabels+rstart,
        nrows_size, sizeof(int32_t), &allocation_failed);
  if (mat->rmap)
    nmat->rmap = (int32_t *)gk_csr_CopyNoSignal(mat->rmap+rstart,
        nrows_size, sizeof(int32_t), &allocation_failed);
  if (mat->rnorms)
    nmat->rnorms = (float *)gk_csr_CopyNoSignal(mat->rnorms+rstart,
        nrows_size, sizeof(float), &allocation_failed);
  if (mat->rsums)
    nmat->rsums = (float *)gk_csr_CopyNoSignal(mat->rsums+rstart,
        nrows_size, sizeof(float), &allocation_failed);
  if (mat->rsizes)
    nmat->rsizes = (float *)gk_csr_CopyNoSignal(mat->rsizes+rstart,
        nrows_size, sizeof(float), &allocation_failed);
  if (mat->rvols)
    nmat->rvols = (float *)gk_csr_CopyNoSignal(mat->rvols+rstart,
        nrows_size, sizeof(float), &allocation_failed);
  if (mat->rwgts)
    nmat->rwgts = (float *)gk_csr_CopyNoSignal(mat->rwgts+rstart,
        nrows_size, sizeof(float), &allocation_failed);
  if (allocation_failed)
    goto allocation_failure;
  for (i=nrows; i>=0; i--)
    nmat->rowptr[i] -= first;

  return nmat;

allocation_failure:
  saved_errno = errno != 0 ? errno : ENOMEM;
  allocation_failed = 1;

failure:
  return gk_csr_TransformError(&nmat, saved_errno,
      allocation_failed, "extract a submatrix from");
}


/*************************************************************************/
/*! Returns a submatrix containing a certain set of rows.
    \param mat is the original matrix.
    \param nrows is the number of rows to extract.
    \param rind is the set of row numbers to extract.
    \returns the row structure of the newly created submatrix.
*/
/**************************************************************************/
gk_csr_t *gk_csr_ExtractRows(gk_csr_t *mat, int nrows, int *rind)
{
  ssize_t i, ii, j, nentries, nnz=0, rownnz, colnnz;
  int allocation_failed=0, saved_errno=0;
  size_t nrows_size;
  gk_csr_t *nmat=NULL;

  if (!gk_csr_ValidateStructure(mat, &rownnz, &colnnz)) {
    saved_errno = errno;
    goto failure;
  }
  if (mat->rowptr == NULL || nrows < 0 ||
      (nrows > 0 && rind == NULL)) {
    saved_errno = EINVAL;
    goto failure;
  }
  for (ii=0; ii<nrows; ii++) {
    i = rind[ii];
    if (i < 0 || i >= mat->nrows) {
      saved_errno = EINVAL;
      goto failure;
    }
    nentries = mat->rowptr[i+1]-mat->rowptr[i];
    if (nnz > PTRDIFF_MAX-nentries) {
      saved_errno = EOVERFLOW;
      allocation_failed = 1;
      goto failure;
    }
    nnz += nentries;
  }
  nrows_size = (size_t)nrows;
  if (nrows_size+1 > SIZE_MAX/sizeof(ssize_t) ||
      (size_t)nnz > SIZE_MAX/sizeof(int32_t) ||
      (mat->rowval != NULL && (size_t)nnz > SIZE_MAX/sizeof(float))) {
    saved_errno = EOVERFLOW;
    allocation_failed = 1;
    goto failure;
  }

  nmat = gk_csr_CreateNoSignal(&allocation_failed);
  if (nmat == NULL)
    goto allocation_failure;
  nmat->nrows = nrows;
  nmat->ncols = mat->ncols;
  nmat->rowptr = (ssize_t *)gk_csr_MallocNoSignal(
      (nrows_size+1)*sizeof(ssize_t), &allocation_failed);
  if (mat->rowind != NULL)
    nmat->rowind = (int32_t *)gk_csr_MallocNoSignal(
        (size_t)nnz*sizeof(int32_t), &allocation_failed);
  if (mat->rowval != NULL)
    nmat->rowval = (float *)gk_csr_MallocNoSignal(
        (size_t)nnz*sizeof(float), &allocation_failed);
  if (allocation_failed)
    goto allocation_failure;

  nmat->rowptr[0] = 0;
  for (nnz=0, j=0, ii=0; ii<nrows; ii++) {
    i = rind[ii];
    nentries = mat->rowptr[i+1]-mat->rowptr[i];
    if (nentries > 0) {
      gk_icopy(nentries, mat->rowind+mat->rowptr[i], nmat->rowind+nnz);
      if (mat->rowval != NULL)
        gk_fcopy(nentries, mat->rowval+mat->rowptr[i], nmat->rowval+nnz);
    }
    nnz += nentries;
    nmat->rowptr[++j] = nnz;
  }
  ASSERT(j == nmat->nrows);

  return nmat;

allocation_failure:
  saved_errno = errno != 0 ? errno : ENOMEM;
  allocation_failed = 1;

failure:
  return gk_csr_TransformError(&nmat, saved_errno,
      allocation_failed, "extract rows from");
}


/*************************************************************************/
/*! Returns a submatrix corresponding to a specified partitioning of rows.
    \param mat is the original matrix.
    \param part is the partitioning vector of the rows.
    \param pid is the partition ID that will be extracted.
    \returns the row structure of the newly created submatrix.
*/
/**************************************************************************/
gk_csr_t *gk_csr_ExtractPartition(gk_csr_t *mat, int *part, int pid)
{
  ssize_t i, j, nentries, nnz=0, rownnz, colnnz;
  int allocation_failed=0, saved_errno=0;
  size_t nrows;
  gk_csr_t *nmat=NULL;

  if (!gk_csr_ValidateStructure(mat, &rownnz, &colnnz)) {
    saved_errno = errno;
    goto failure;
  }
  if (mat->rowptr == NULL || (mat->nrows > 0 && part == NULL)) {
    saved_errno = EINVAL;
    goto failure;
  }
  for (nrows=0, i=0; i<mat->nrows; i++) {
    if (part[i] == pid) {
      nentries = mat->rowptr[i+1]-mat->rowptr[i];
      if (nnz > PTRDIFF_MAX-nentries) {
        saved_errno = EOVERFLOW;
        allocation_failed = 1;
        goto failure;
      }
      nrows++;
      nnz += nentries;
    }
  }
  if (nrows+1 > SIZE_MAX/sizeof(ssize_t) ||
      (size_t)nnz > SIZE_MAX/sizeof(int32_t) ||
      (mat->rowval != NULL && (size_t)nnz > SIZE_MAX/sizeof(float))) {
    saved_errno = EOVERFLOW;
    allocation_failed = 1;
    goto failure;
  }

  nmat = gk_csr_CreateNoSignal(&allocation_failed);
  if (nmat == NULL)
    goto allocation_failure;
  nmat->nrows = (int32_t)nrows;
  nmat->ncols = mat->ncols;
  nmat->rowptr = (ssize_t *)gk_csr_MallocNoSignal(
      (nrows+1)*sizeof(ssize_t), &allocation_failed);
  if (mat->rowind != NULL)
    nmat->rowind = (int32_t *)gk_csr_MallocNoSignal(
        (size_t)nnz*sizeof(int32_t), &allocation_failed);
  if (mat->rowval != NULL)
    nmat->rowval = (float *)gk_csr_MallocNoSignal(
        (size_t)nnz*sizeof(float), &allocation_failed);
  if (allocation_failed)
    goto allocation_failure;

  nmat->rowptr[0] = 0;
  for (nnz=0, j=0, i=0; i<mat->nrows; i++) {
    if (part[i] == pid) {
      nentries = mat->rowptr[i+1]-mat->rowptr[i];
      if (nentries > 0) {
        gk_icopy(nentries, mat->rowind+mat->rowptr[i], nmat->rowind+nnz);
        if (mat->rowval != NULL)
          gk_fcopy(nentries, mat->rowval+mat->rowptr[i], nmat->rowval+nnz);
      }
      nnz += nentries;
      nmat->rowptr[++j] = nnz;
    }
  }
  ASSERT(j == nmat->nrows);

  return nmat;

allocation_failure:
  saved_errno = errno != 0 ? errno : ENOMEM;
  allocation_failed = 1;

failure:
  return gk_csr_TransformError(&nmat, saved_errno,
      allocation_failed, "extract a partition from");
}


/*************************************************************************/
/*! Splits the matrix into multiple sub-matrices based on the provided
    color array.
    \param mat is the original matrix.
    \param color is an array of size equal to the number of non-zeros
           in the matrix (row-wise structure). The matrix is split into
           as many parts as the number of colors. For meaningfull results,
           the colors should be numbered consecutively starting from 0.
    \returns an array of matrices for each supplied color number.
*/
/**************************************************************************/
gk_csr_t **gk_csr_Split(gk_csr_t *mat, int *color)
{
  ssize_t i, j, rownnz, colnnz;
  int allocation_failed=0, ncolors=1, saved_errno=0;
  int nrows;
  ssize_t *rowptr;
  int *rowind;
  float *rowval;
  gk_csr_t **smats=NULL;

  if (!gk_csr_ValidateStructure(mat, &rownnz, &colnnz)) {
    saved_errno = errno;
    goto failure;
  }
  if (mat->rowptr == NULL || (rownnz > 0 && color == NULL)) {
    saved_errno = EINVAL;
    goto failure;
  }
  for (j=0; j<rownnz; j++) {
    if (color[j] < 0) {
      saved_errno = EINVAL;
      goto failure;
    }
    if (color[j] == INT_MAX) {
      saved_errno = EOVERFLOW;
      allocation_failed = 1;
      goto failure;
    }
    if (ncolors <= color[j])
      ncolors = color[j]+1;
  }
  if ((size_t)ncolors > SIZE_MAX/sizeof(gk_csr_t *)) {
    saved_errno = EOVERFLOW;
    allocation_failed = 1;
    goto failure;
  }

  nrows  = mat->nrows;
  rowptr = mat->rowptr;
  rowind = mat->rowind;
  rowval = mat->rowval;

  smats = (gk_csr_t **)gk_csr_MallocNoSignal(
      (size_t)ncolors*sizeof(gk_csr_t *), &allocation_failed);
  if (smats == NULL)
    goto allocation_failure;
  memset(smats, 0, (size_t)ncolors*sizeof(gk_csr_t *));
  for (i=0; i<ncolors; i++) {
    smats[i] = gk_csr_CreateNoSignal(&allocation_failed);
    if (smats[i] == NULL)
      goto allocation_failure;
    smats[i]->nrows  = mat->nrows;
    smats[i]->ncols  = mat->ncols;
    smats[i]->rowptr = (ssize_t *)gk_csr_MallocNoSignal(
        ((size_t)nrows+1)*sizeof(ssize_t), &allocation_failed);
    if (smats[i]->rowptr == NULL)
      goto allocation_failure;
    memset(smats[i]->rowptr, 0, ((size_t)nrows+1)*sizeof(ssize_t));
  }

  for (i=0; i<nrows; i++) {
    for (j=rowptr[i]; j<rowptr[i+1]; j++) 
      smats[color[j]]->rowptr[i]++;
  }
  for (i=0; i<ncolors; i++) 
    MAKECSR(j, nrows, smats[i]->rowptr);

  for (i=0; i<ncolors; i++) {
    if (rowind != NULL)
      smats[i]->rowind = (int32_t *)gk_csr_MallocNoSignal(
          (size_t)smats[i]->rowptr[nrows]*sizeof(int32_t),
          &allocation_failed);
    if (rowval != NULL)
      smats[i]->rowval = (float *)gk_csr_MallocNoSignal(
          (size_t)smats[i]->rowptr[nrows]*sizeof(float),
          &allocation_failed);
    if (allocation_failed)
      goto allocation_failure;
  }

  for (i=0; i<nrows; i++) {
    for (j=rowptr[i]; j<rowptr[i+1]; j++) {
      smats[color[j]]->rowind[smats[color[j]]->rowptr[i]] = rowind[j];
      if (rowval != NULL)
        smats[color[j]]->rowval[smats[color[j]]->rowptr[i]] = rowval[j];
      smats[color[j]]->rowptr[i]++;
    }
  }

  for (i=0; i<ncolors; i++) 
    SHIFTCSR(j, nrows, smats[i]->rowptr);

  return smats;

allocation_failure:
  saved_errno = errno != 0 ? errno : ENOMEM;
  allocation_failed = 1;

failure:
  if (smats != NULL) {
    for (i=0; i<ncolors; i++)
      gk_csr_Free(&smats[i]);
    gk_free((void **)&smats, LTERM);
  }
  errno = saved_errno != 0 ? saved_errno : EINVAL;
  gk_errexit(allocation_failed || saved_errno == ENOMEM ||
      saved_errno == EOVERFLOW ? SIGMEM : SIGERR,
      "Failed to split a matrix.\n");
  errno = saved_errno != 0 ? saved_errno : EINVAL;
  return NULL;
}


/**************************************************************************/
/*! Determines the format of the CSR matrix based on the extension.
    \param filename is the name of the file.
    \param the user-supplied format.
    \returns the type. The extension of the file directly maps to the
           name of the format.
*/
/**************************************************************************/
int gk_csr_DetermineFormat(char *filename, int format)
{
  char *extension, *separator, *backslash;

  if (format != GK_CSR_FMT_AUTO)
    return format;

  if (filename == NULL)
    return GK_CSR_FMT_CSR;

  format = GK_CSR_FMT_CSR;
  extension = strrchr(filename, '.');
  separator = strrchr(filename, '/');
  backslash = strrchr(filename, '\\');
  if (backslash != NULL && (separator == NULL || separator < backslash))
    separator = backslash;
  if (extension == NULL || (separator != NULL && extension < separator))
    return format;
  extension++;

  if (!strcmp(extension, "csr"))
    format = GK_CSR_FMT_CSR;
  else if (!strcmp(extension, "ijv"))
    format = GK_CSR_FMT_IJV;
  else if (!strcmp(extension, "cluto"))
    format = GK_CSR_FMT_CLUTO;
  else if (!strcmp(extension, "metis"))
    format = GK_CSR_FMT_METIS;
  else if (!strcmp(extension, "binrow"))
    format = GK_CSR_FMT_BINROW;
  else if (!strcmp(extension, "bincol"))
    format = GK_CSR_FMT_BINCOL;
  else if (!strcmp(extension, "bijv"))
    format = GK_CSR_FMT_BIJV;

  return format;
}


/*************************************************************************/
/*! Allocates tracked reader storage without raising a signal.

    The caller can close its stream and release earlier allocations before
    reporting the memory failure through GKlib's configured error mechanism.
*/
/*************************************************************************/
static void *gk_csr_MallocNoSignal(size_t nbytes, int *r_failed)
{
  void *ptr;

  if (*r_failed)
    return NULL;

  ptr = gk_malloc_nosignal(nbytes);
  if (ptr == NULL)
    *r_failed = 1;
  return ptr;
}


/*************************************************************************/
/*! Grows tracked reader storage without raising a signal. */
/*************************************************************************/
static void *gk_csr_ReallocNoSignal(void *oldptr, size_t nbytes, int *r_failed)
{
  void *ptr;

  if (*r_failed)
    return NULL;

  ptr = gk_realloc_nosignal(oldptr, nbytes);
  if (ptr == NULL)
    *r_failed = 1;
  return ptr;
}


/*************************************************************************/
/*! Creates an empty reader matrix without raising a signal. */
/*************************************************************************/
static gk_csr_t *gk_csr_CreateNoSignal(int *r_failed)
{
  gk_csr_t *mat;

  mat = (gk_csr_t *)gk_csr_MallocNoSignal(sizeof(gk_csr_t), r_failed);
  if (mat != NULL)
    gk_csr_Init(mat);
  return mat;
}


/*************************************************************************/
/*! Reads and validates one binary row- or column-oriented CSR matrix.

    The file byte budget, dimensions, pointer monotonicity, indices, values,
    short reads, and close operation are checked before ownership is committed
    to the returned matrix. A failure closes the stream and releases every
    partially allocated array before it is reported.
*/
/*************************************************************************/
static gk_csr_t *gk_csr_ReadBinary(char *filename, int byrow, int readvals)
{
  int32_t i, nrows=0, ncols=0, dimension=0, other_dimension=0;
  int allocation_failed=0, failed=0, saved_errno=0;
  size_t element_bytes, fsize=0, pointer_count=0, index_count=0, required=0;
  ssize_t *pointers=NULL;
  int32_t *indices=NULL;
  float *values=NULL;
  FILE *fpin=NULL;
  gk_csr_t *mat=NULL;

  if (filename == NULL) {
    failed = 1;
    saved_errno = EINVAL;
  }
  if (!failed) {
    mat = gk_csr_CreateNoSignal(&allocation_failed);
    if (mat == NULL) {
      failed = 1;
      saved_errno = errno != 0 ? errno : ENOMEM;
    }
  }

  if (!failed) {
    fpin = fopen(filename, "rb");
    if (fpin == NULL) {
      failed = 1;
      saved_errno = errno != 0 ? errno : EIO;
    }
  }
  if (!failed && !gk_stream_file_size(fpin, &fsize)) {
    failed = 1;
    saved_errno = errno != 0 ? errno : EIO;
  }

  if (!failed &&
      (fread(&nrows, sizeof(int32_t), 1, fpin) != 1 ||
       fread(&ncols, sizeof(int32_t), 1, fpin) != 1)) {
    failed = 1;
    saved_errno = ferror(fpin) ? (errno != 0 ? errno : EIO) : EINVAL;
  }
  if (!failed && (nrows < 0 || ncols < 0)) {
    failed = 1;
    saved_errno = EINVAL;
  }

  if (!failed) {
    dimension = byrow ? nrows : ncols;
    other_dimension = byrow ? ncols : nrows;
    pointer_count = (size_t)dimension+1;
    if (pointer_count > SIZE_MAX/sizeof(ssize_t) ||
        2*sizeof(int32_t) > fsize ||
        pointer_count*sizeof(ssize_t) > fsize-2*sizeof(int32_t)) {
      failed = 1;
      saved_errno = pointer_count > SIZE_MAX/sizeof(ssize_t) ?
          EOVERFLOW : EINVAL;
    }
  }

  if (!failed) {
    pointers = (ssize_t *)gk_csr_MallocNoSignal(
        pointer_count*sizeof(ssize_t), &allocation_failed);
    if (pointers == NULL) {
      failed = 1;
      saved_errno = errno != 0 ? errno : ENOMEM;
    }
    else if (fread(pointers, sizeof(ssize_t), pointer_count, fpin) !=
             pointer_count) {
      failed = 1;
      saved_errno = ferror(fpin) ? (errno != 0 ? errno : EIO) : EINVAL;
    }
  }
  if (!failed && pointers[0] != 0) {
    failed = 1;
    saved_errno = EINVAL;
  }
  if (!failed) {
    for (i=0; i<dimension; i++) {
      if (pointers[i] < 0 || pointers[i] > pointers[i+1]) {
        failed = 1;
        saved_errno = EINVAL;
        break;
      }
    }
  }

  if (!failed) {
    if (pointers[dimension] < 0 ||
        (uintmax_t)pointers[dimension] > (uintmax_t)SIZE_MAX) {
      failed = 1;
      saved_errno = EINVAL;
    }
    else
      index_count = (size_t)pointers[dimension];
  }
  if (!failed) {
    required = 2*sizeof(int32_t)+pointer_count*sizeof(ssize_t);
    element_bytes = sizeof(int32_t)+(readvals ? sizeof(float) : 0);
    if (index_count > (SIZE_MAX-required)/element_bytes ||
        required+index_count*element_bytes != fsize) {
      failed = 1;
      saved_errno = index_count > (SIZE_MAX-required)/element_bytes ?
          EOVERFLOW : EINVAL;
    }
  }

  if (!failed) {
    indices = (int32_t *)gk_csr_MallocNoSignal(
        index_count*sizeof(int32_t), &allocation_failed);
    if (indices == NULL) {
      failed = 1;
      saved_errno = errno != 0 ? errno : ENOMEM;
    }
    else if (fread(indices, sizeof(int32_t), index_count, fpin) !=
             index_count) {
      failed = 1;
      saved_errno = ferror(fpin) ? (errno != 0 ? errno : EIO) : EINVAL;
    }
  }
  if (!failed) {
    size_t j;
    for (j=0; j<index_count; j++) {
      if (indices[j] < 0 || indices[j] >= other_dimension) {
        failed = 1;
        saved_errno = EINVAL;
        break;
      }
    }
  }

  if (!failed && readvals) {
    size_t j;

    values = (float *)gk_csr_MallocNoSignal(index_count*sizeof(float),
        &allocation_failed);
    if (values == NULL) {
      failed = 1;
      saved_errno = errno != 0 ? errno : ENOMEM;
    }
    else if (fread(values, sizeof(float), index_count, fpin) != index_count) {
      failed = 1;
      saved_errno = ferror(fpin) ? (errno != 0 ? errno : EIO) : EINVAL;
    }
    if (!failed) {
      for (j=0; j<index_count; j++) {
        if (!isfinite(values[j])) {
          failed = 1;
          saved_errno = EINVAL;
          break;
        }
      }
    }
  }
  if (!failed && fgetc(fpin) != EOF) {
    failed = 1;
    saved_errno = EINVAL;
  }
  if (!failed && ferror(fpin)) {
    failed = 1;
    saved_errno = errno != 0 ? errno : EIO;
  }
  if (fpin != NULL) {
    if (fclose(fpin) != 0 && !failed) {
      failed = 1;
      saved_errno = errno != 0 ? errno : EIO;
    }
    fpin = NULL;
  }

  if (failed) {
    if (saved_errno == 0)
      saved_errno = allocation_failed ? ENOMEM : EIO;
    gk_free((void **)&pointers, &indices, &values, LTERM);
    gk_csr_Free(&mat);
    errno = saved_errno;
    if (allocation_failed || saved_errno == ENOMEM ||
        saved_errno == EOVERFLOW) {
      gk_errexit(SIGMEM, "Memory allocation failed while reading %s.",
                 filename != NULL ? filename : "(null)");
      errno = saved_errno;
      return NULL;
    }
    gk_errexit(SIGERR, "Invalid or truncated binary CSR file %s.",
               filename != NULL ? filename : "(null)");
    errno = saved_errno;
    return NULL;
  }

  mat->nrows = nrows;
  mat->ncols = ncols;
  if (byrow) {
    mat->rowptr = pointers;
    mat->rowind = indices;
    mat->rowval = values;
  }
  else {
    mat->colptr = pointers;
    mat->colind = indices;
    mat->colval = values;
  }
  return mat;
}


/*************************************************************************/
/*! Releases a partially read text matrix and reports an input failure.

    The stream is closed before signaling, and the gk_getline() buffer is
    released with free() according to its public ownership contract.
*/
/*************************************************************************/
static gk_csr_t *gk_csr_TextReadError(FILE *fpin, char *line,
    gk_csr_t *mat, char *filename)
{
  int saved_errno;

  if (fpin != NULL && ferror(fpin))
    saved_errno = errno != 0 ? errno : EIO;
  else if (fpin == NULL && errno != 0)
    saved_errno = errno;
  else if (errno == EOVERFLOW)
    saved_errno = EOVERFLOW;
  else
    saved_errno = EINVAL;

  if (fpin != NULL)
    fclose(fpin);
  free(line);
  gk_csr_Free(&mat);
  errno = saved_errno;
  gk_errexit(saved_errno == ENOMEM || saved_errno == EOVERFLOW ?
             SIGMEM : SIGERR,
             "Invalid or truncated CSR file %s.",
             filename != NULL ? filename : "(null)");
  errno = saved_errno;
  return NULL;
}


/*************************************************************************/
/*! Cleans up a text reader before reporting an allocation failure. */
/*************************************************************************/
static gk_csr_t *gk_csr_TextAllocationError(FILE *fpin, char *line,
    gk_csr_t *mat, char *filename)
{
  int saved_errno=errno != 0 ? errno : ENOMEM;

  if (fpin != NULL)
    fclose(fpin);
  free(line);
  gk_csr_Free(&mat);
  errno = saved_errno;
  gk_errexit(SIGMEM, "Memory allocation failed while reading %s.",
             filename != NULL ? filename : "(null)");
  errno = saved_errno;
  return NULL;
}


/*************************************************************************/
/*! Parses a bounded list of complete unsigned decimal header fields. */
/*************************************************************************/
static int gk_csr_ParseHeader(char *line, size_t *values, size_t minimum,
    size_t maximum, size_t *count)
{
  char *cursor=line, *end;
  uintmax_t value;

  *count = 0;
  while (1) {
    while (isspace((unsigned char)*cursor))
      cursor++;
    if (*cursor == '\0')
      return *count >= minimum;
    if (*count == maximum || *cursor == '-')
      return 0;
    errno = 0;
    value = strtoumax(cursor, &end, 10);
    if (end == cursor || errno == ERANGE || value > SIZE_MAX ||
        (*end != '\0' && !isspace((unsigned char)*end)))
      return 0;
    values[(*count)++] = (size_t)value;
    cursor = end;
  }
}


/*************************************************************************/
/*! Parses one complete whitespace-delimited int32_t field. */
/*************************************************************************/
static int gk_csr_ParseI32(char **cursor, int32_t *value)
{
  char *end;
  intmax_t parsed;

  while (isspace((unsigned char)**cursor))
    (*cursor)++;
  errno = 0;
  parsed = strtoimax(*cursor, &end, 10);
  if (end == *cursor || errno == ERANGE ||
      parsed < INT32_MIN || parsed > INT32_MAX ||
      (*end != '\0' && !isspace((unsigned char)*end)))
    return 0;
  *cursor = end;
  *value = (int32_t)parsed;
  return 1;
}


/*************************************************************************/
/*! Parses one finite, complete whitespace-delimited float field. */
/*************************************************************************/
static int gk_csr_ParseFloat(char **cursor, float *value)
{
  char *end;
  float parsed;

  while (isspace((unsigned char)**cursor))
    (*cursor)++;
  errno = 0;
  parsed = strtof(*cursor, &end);
  if (end == *cursor || errno == ERANGE || !isfinite(parsed) ||
      (*end != '\0' && !isspace((unsigned char)*end)))
    return 0;
  *cursor = end;
  *value = parsed;
  return 1;
}


/*************************************************************************/
/*! Returns true when a CSR record contains no unparsed fields. */
/*************************************************************************/
static int gk_csr_EndRecord(char *cursor)
{
  while (isspace((unsigned char)*cursor))
    cursor++;
  return *cursor == '\0';
}


/**************************************************************************/
/*! Reads a CSR matrix from the supplied file and stores its forward view.

    \param filename is the file that stores the data.
    \param format is GK_CSR_FMT_METIS, GK_CSR_FMT_CLUTO, GK_CSR_FMT_CSR,
           GK_CSR_FMT_BINROW, GK_CSR_FMT_BINCOL, GK_CSR_FMT_IJV, or
           GK_CSR_FMT_BIJV.
    \param readvals indicates whether applicable text and binary formats
           contain values.
    \param numbering is 1 for one-based CSR/IJV indices and 0 for zero-based
           indices. One-based indices are normalized during input.
    \returns the matrix that was read, or NULL after complete cleanup when
             input validation or a resource operation fails.
*/
/**************************************************************************/
gk_csr_t *gk_csr_Read(char *filename, int format, int readvals, int numbering)
{
  ssize_t i, k, l, line_status;
  int allocation_failed=0, saved_errno=0;
  size_t file_size=0;
  long parsed;
  size_t fields[4], input_ncols=0, nfields, nrows, ncols, nnz, fmt, ncon;
  size_t lnlen=0, rowcap=0, nnzcap=0, newcap, edgecap=0;
  ssize_t *rowptr=NULL, *newrowptr=NULL;
  int32_t *rowind=NULL, *newrowind=NULL, *iinds=NULL, *jinds=NULL, ival;
  float *rowval=NULL, *newrowval=NULL, *vals=NULL, fval;
  int readsizes, readwgts, dynamic_rows=0;
  char *line=NULL, *head, *tail, fmtstr[256];
  FILE *fpin=NULL;
  gk_csr_t *mat=NULL;

  if (filename == NULL) {
    errno = EINVAL;
    gk_errexit(SIGERR, "File (null) does not exist!\n");
    errno = EINVAL;
    return NULL;
  }
  format = gk_csr_DetermineFormat(filename, format);

  switch (format) {
    case GK_CSR_FMT_BINROW:
      return gk_csr_ReadBinary(filename, 1, readvals == 1);

    case GK_CSR_FMT_BINCOL:
      return gk_csr_ReadBinary(filename, 0, readvals != 0);


    case GK_CSR_FMT_IJV:
      numbering = (numbering ? - 1 : 0);

      fpin = gk_fopen(filename, "r", "gk_csr_Read: fpin");
      if (fpin == NULL) {
        gk_free((void **)&iinds, &jinds, &vals, LTERM);
        return NULL;
      }
      for (nrows=0, ncols=0, nnz=0;
           (line_status = gk_getline(&line, &lnlen, fpin)) != -1; ) {
        head = line;
        if (memchr(line, '\0', (size_t)line_status) != NULL)
          goto csr_ijv_failure;
        while (isspace((unsigned char)*head))
          head++;
        if (*head == '\0' || *head == '%')
          continue;
        if (nnz == edgecap) {
          int32_t *newints;
          float *newfloats;
          if (edgecap >= (size_t)PTRDIFF_MAX) {
            errno = EOVERFLOW;
            goto csr_ijv_failure;
          }
          newcap = edgecap == 0 ? 16 :
                   (edgecap > (size_t)PTRDIFF_MAX/2 ?
                    (size_t)PTRDIFF_MAX : edgecap*2);
          if (newcap > SIZE_MAX/sizeof(int32_t)) {
            errno = EOVERFLOW;
            goto csr_ijv_allocation_failure;
          }
          newints = (int32_t *)gk_csr_ReallocNoSignal(
              iinds, newcap*sizeof(int32_t), &allocation_failed);
          if (newints == NULL)
            goto csr_ijv_allocation_failure;
          iinds = newints;
          newints = (int32_t *)gk_csr_ReallocNoSignal(
              jinds, newcap*sizeof(int32_t), &allocation_failed);
          if (newints == NULL)
            goto csr_ijv_allocation_failure;
          jinds = newints;
          if (readvals) {
            if (newcap > SIZE_MAX/sizeof(float)) {
              errno = EOVERFLOW;
              goto csr_ijv_allocation_failure;
            }
            newfloats = (float *)gk_csr_ReallocNoSignal(
                vals, newcap*sizeof(float), &allocation_failed);
            if (newfloats == NULL)
              goto csr_ijv_allocation_failure;
            vals = newfloats;
          }
          edgecap = newcap;
        }
        if (!gk_csr_ParseI32(&head, &iinds[nnz]) ||
            !gk_csr_ParseI32(&head, &jinds[nnz]) ||
            (readvals && !gk_csr_ParseFloat(&head, &vals[nnz])) ||
            !gk_csr_EndRecord(head))
          goto csr_ijv_failure;
        if (numbering == -1 &&
            (iinds[nnz] == INT32_MIN || jinds[nnz] == INT32_MIN))
          goto csr_ijv_failure;
        iinds[nnz] += numbering;
        jinds[nnz] += numbering;
        if (iinds[nnz] < 0 || jinds[nnz] < 0)
          goto csr_ijv_failure;

        if (nrows < (size_t)iinds[nnz])
          nrows = iinds[nnz];
        if (ncols < (size_t)jinds[nnz])
          ncols = jinds[nnz];
        nnz++;
      }
      if (!feof(fpin) && (errno == ENOMEM || errno == EOVERFLOW))
        goto csr_ijv_allocation_failure;
      if (!feof(fpin) || ferror(fpin))
        goto csr_ijv_failure;
      if (nnz == 0) {
        errno = EINVAL;
        goto csr_ijv_failure;
      }
      if (nrows >= INT32_MAX || ncols >= INT32_MAX) {
        errno = EOVERFLOW;
        goto csr_ijv_failure;
      }
      nrows++;
      ncols++;
      if (fclose(fpin) != 0) {
        saved_errno = errno != 0 ? errno : EIO;
        goto csr_ijv_failure_closed;
      }
      fpin = NULL;
      free(line);
      line = NULL;

      /* convert (i, j, v) into a CSR matrix */
      mat = gk_csr_CreateNoSignal(&allocation_failed);
      if (mat == NULL)
        goto csr_ijv_allocation_failure;
      mat->nrows = nrows;
      mat->ncols = ncols;
      if (nrows == SIZE_MAX || nrows+1 > SIZE_MAX/sizeof(ssize_t) ||
          nnz > SIZE_MAX/sizeof(int32_t) ||
          (readvals && nnz > SIZE_MAX/sizeof(float))) {
        errno = EOVERFLOW;
        allocation_failed = 1;
        goto csr_ijv_allocation_failure;
      }
      rowptr = mat->rowptr = (ssize_t *)gk_csr_MallocNoSignal(
          (nrows+1)*sizeof(ssize_t), &allocation_failed);
      rowind = mat->rowind = (int32_t *)gk_csr_MallocNoSignal(
          nnz*sizeof(int32_t), &allocation_failed);
      if (readvals)
        rowval = mat->rowval = (float *)gk_csr_MallocNoSignal(
            nnz*sizeof(float), &allocation_failed);
      if (rowptr == NULL || rowind == NULL || (readvals && rowval == NULL))
        goto csr_ijv_allocation_failure;
      memset(rowptr, 0, (nrows+1)*sizeof(ssize_t));

      for (i=0; i<nnz; i++)
        rowptr[iinds[i]]++;
      MAKECSR(i, nrows, rowptr);

      for (i=0; i<nnz; i++) {
        rowind[rowptr[iinds[i]]] = jinds[i];
        if (readvals)
          rowval[rowptr[iinds[i]]] = vals[i];
        rowptr[iinds[i]]++;
      }
      SHIFTCSR(i, nrows, rowptr);

      gk_free((void **)&iinds, &jinds, &vals, LTERM);

      return mat;

      break;

csr_ijv_failure:
      saved_errno = fpin != NULL && ferror(fpin) ?
          (errno != 0 ? errno : EIO) :
          (errno == EOVERFLOW ? EOVERFLOW : EINVAL);
      if (fpin != NULL)
        fclose(fpin);
csr_ijv_failure_closed:
      free(line);
      gk_free((void **)&iinds, &jinds, &vals, LTERM);
      errno = saved_errno != 0 ? saved_errno : EINVAL;
      gk_errexit(saved_errno == ENOMEM || saved_errno == EOVERFLOW ?
                 SIGMEM : SIGERR,
                 "Invalid or truncated IJV file %s.\n", filename);
      errno = saved_errno != 0 ? saved_errno : EINVAL;
      return NULL;

csr_ijv_allocation_failure:
      saved_errno = errno != 0 ? errno : ENOMEM;
      if (fpin != NULL)
        fclose(fpin);
      free(line);
      gk_free((void **)&iinds, &jinds, &vals, LTERM);
      gk_csr_Free(&mat);
      errno = saved_errno;
      gk_errexit(SIGMEM, "Memory allocation failed while reading %s.\n",
                 filename);
      errno = saved_errno;
      return NULL;

    case GK_CSR_FMT_BIJV:
      mat = gk_csr_CreateNoSignal(&allocation_failed);
      if (mat == NULL) {
        saved_errno = errno != 0 ? errno : ENOMEM;
        goto bijv_failure_closed;
      }

      fpin = fopen(filename, "rb");
      if (fpin == NULL) {
        saved_errno = errno != 0 ? errno : EIO;
        gk_csr_Free(&mat);
        errno = saved_errno;
        gk_errexit(SIGERR, "Failed to open binary IJV file %s.\n", filename);
        errno = saved_errno;
        return NULL;
      }

      if (!gk_stream_file_size(fpin, &file_size)) {
        saved_errno = errno != 0 ? errno : EIO;
        goto bijv_failure;
      }
      if (fread(&(mat->nrows), sizeof(int32_t), 1, fpin) != 1)
        goto bijv_failure;
      if (fread(&(mat->ncols), sizeof(int32_t), 1, fpin) != 1)
        goto bijv_failure;
      if (fread(&nnz, sizeof(size_t), 1, fpin) != 1)
        goto bijv_failure;
      if (fread(&readvals, sizeof(int32_t), 1, fpin) != 1)
        goto bijv_failure;
      if (mat->nrows < 0 || mat->ncols < 0 ||
          (readvals != 0 && readvals != 1))
        goto bijv_failure;
      if ((size_t)mat->nrows+1 > SIZE_MAX/sizeof(ssize_t) ||
          nnz > (size_t)PTRDIFF_MAX ||
          nnz > (SIZE_MAX-(2*sizeof(int32_t)+sizeof(size_t)+sizeof(int32_t)))/
              (2*sizeof(int32_t)+(readvals ? sizeof(float) : 0))) {
        saved_errno = EOVERFLOW;
        goto bijv_failure;
      }
      if (2*sizeof(int32_t)+sizeof(size_t)+sizeof(int32_t)+
              nnz*(2*sizeof(int32_t)+(readvals ? sizeof(float) : 0)) !=
              file_size)
        goto bijv_failure;

      /* read the data into three arrays */
      iinds = (int32_t *)gk_csr_MallocNoSignal(nnz*sizeof(int32_t),
                                      &allocation_failed);
      jinds = (int32_t *)gk_csr_MallocNoSignal(nnz*sizeof(int32_t),
                                      &allocation_failed);
      vals  = (readvals ? (float *)gk_csr_MallocNoSignal(nnz*sizeof(float),
          &allocation_failed) : NULL);
      if (iinds == NULL || jinds == NULL || (readvals && vals == NULL)) {
        saved_errno = errno != 0 ? errno : ENOMEM;
        goto bijv_failure;
      }

      for (i=0; i<nnz; i++) {
        if (fread(&(iinds[i]), sizeof(int32_t), 1, fpin) != 1)
          goto bijv_failure;
        if (fread(&(jinds[i]), sizeof(int32_t), 1, fpin) != 1)
          goto bijv_failure;
        if (readvals) {
          if (fread(&(vals[i]), sizeof(float), 1, fpin) != 1)
            goto bijv_failure;
        }
        if (iinds[i] < 0 || iinds[i] >= mat->nrows ||
            jinds[i] < 0 || jinds[i] >= mat->ncols ||
            (readvals && !isfinite(vals[i])))
          goto bijv_failure;
      }
      if (fgetc(fpin) != EOF || ferror(fpin))
        goto bijv_failure;
      if (fclose(fpin) != 0) {
        saved_errno = errno != 0 ? errno : EIO;
        fpin = NULL;
        goto bijv_failure_closed;
      }
      fpin = NULL;

      /* convert (i, j, v) into a CSR matrix */
      rowptr = mat->rowptr = (ssize_t *)gk_csr_MallocNoSignal(
          ((size_t)mat->nrows+1)*sizeof(ssize_t), &allocation_failed);
      rowind = mat->rowind = (int32_t *)gk_csr_MallocNoSignal(
          nnz*sizeof(int32_t), &allocation_failed);
      if (readvals)
        rowval = mat->rowval = (float *)gk_csr_MallocNoSignal(
            nnz*sizeof(float), &allocation_failed);
      if (rowptr == NULL || rowind == NULL || (readvals && rowval == NULL)) {
        saved_errno = errno != 0 ? errno : ENOMEM;
        goto bijv_failure_closed;
      }
      memset(rowptr, 0, ((size_t)mat->nrows+1)*sizeof(ssize_t));

      for (i=0; i<nnz; i++)
        rowptr[iinds[i]]++;
      MAKECSR(i, mat->nrows, rowptr);

      for (i=0; i<nnz; i++) {
        rowind[rowptr[iinds[i]]] = jinds[i];
        if (readvals)
          rowval[rowptr[iinds[i]]] = vals[i];
        rowptr[iinds[i]]++;
      }
      SHIFTCSR(i, mat->nrows, rowptr);

      gk_free((void **)&iinds, &jinds, &vals, LTERM);

      return mat;

      break;

bijv_failure:
      if (saved_errno == 0)
        saved_errno = fpin != NULL && ferror(fpin) ?
            (errno != 0 ? errno : EIO) : EINVAL;
      if (fpin != NULL) {
        fclose(fpin);
        fpin = NULL;
      }
bijv_failure_closed:
      if (saved_errno == 0)
        saved_errno = allocation_failed ? ENOMEM : EIO;
      gk_free((void **)&iinds, &jinds, &vals, LTERM);
      gk_csr_Free(&mat);
      errno = saved_errno;
      if (allocation_failed || saved_errno == ENOMEM ||
          saved_errno == EOVERFLOW) {
        gk_errexit(SIGMEM, "Memory allocation failed while reading %s.\n",
                   filename != NULL ? filename : "(null)");
        errno = saved_errno;
        return NULL;
      }
      gk_errexit(SIGERR, "Invalid or truncated binary IJV file %s.\n", filename);
      errno = saved_errno;
      return NULL;


    /* the following are handled by a common input code, that comes after the switch */

    case GK_CSR_FMT_CLUTO:
      fpin = gk_fopen(filename, "r", "gk_csr_Read: fpin");
      if (fpin == NULL)
        return NULL;
      do {
        line_status = gk_getline(&line, &lnlen, fpin);
        if (line_status == -1 && !feof(fpin) &&
            (errno == ENOMEM || errno == EOVERFLOW))
          return gk_csr_TextAllocationError(fpin, line, NULL, filename);
        if (line_status <= 0 ||
            memchr(line, '\0', (size_t)line_status) != NULL)
          return gk_csr_TextReadError(fpin, line, NULL, filename);
      } while (line[0] == '%');

      if (!gk_csr_ParseHeader(line, fields, 3, 3, &nfields))
        return gk_csr_TextReadError(fpin, line, NULL, filename);
      nrows = fields[0];
      ncols = fields[1];
      nnz = fields[2];
      if (nrows > INT32_MAX || ncols > INT32_MAX ||
          nnz > (size_t)PTRDIFF_MAX) {
        errno = EOVERFLOW;
        return gk_csr_TextReadError(fpin, line, NULL, filename);
      }
      input_ncols = ncols;

      readsizes = 0;
      readwgts  = 0;
      readvals  = 1;
      numbering = 1;

      break;

    case GK_CSR_FMT_METIS:
      fpin = gk_fopen(filename, "r", "gk_csr_Read: fpin");
      if (fpin == NULL)
        return NULL;
      do {
        line_status = gk_getline(&line, &lnlen, fpin);
        if (line_status == -1 && !feof(fpin) &&
            (errno == ENOMEM || errno == EOVERFLOW))
          return gk_csr_TextAllocationError(fpin, line, NULL, filename);
        if (line_status <= 0 ||
            memchr(line, '\0', (size_t)line_status) != NULL)
          return gk_csr_TextReadError(fpin, line, NULL, filename);
      } while (line[0] == '%');

      fmt = ncon = 0;
      if (!gk_csr_ParseHeader(line, fields, 2, 4, &nfields))
        return gk_csr_TextReadError(fpin, line, NULL, filename);
      nrows = fields[0];
      nnz = fields[1];
      if (nfields > 2)
        fmt = fields[2];
      if (nfields > 3)
        ncon = fields[3];

      ncols = nrows;
      if (nrows > INT32_MAX || nnz > (size_t)PTRDIFF_MAX/2) {
        errno = EOVERFLOW;
        return gk_csr_TextReadError(fpin, line, NULL, filename);
      }
      nnz *= 2;

      if (fmt > 111 || fmt%10 > 1 || (fmt/10)%10 > 1 || (fmt/100)%10 > 1)
        return gk_csr_TextReadError(fpin, line, NULL, filename);

      sprintf(fmtstr, "%03zu", fmt%1000);
      readsizes = (fmtstr[0] == '1');
      readwgts  = (fmtstr[1] == '1');
      readvals  = (fmtstr[2] == '1');
      numbering = 1;
      if (nfields == 4 && (ncon == 0 || !readwgts))
        return gk_csr_TextReadError(fpin, line, NULL, filename);
      ncon      = (ncon == 0 ? 1 : ncon);
      if (readwgts && ncon != 1)
        return gk_csr_TextReadError(fpin, line, NULL, filename);
      input_ncols = nrows;
      if (ncon > (size_t)PTRDIFF_MAX ||
          ncon > SIZE_MAX/(nrows == 0 ? 1 : nrows)) {
        errno = EOVERFLOW;
        return gk_csr_TextReadError(fpin, line, NULL, filename);
      }

      break;

    case GK_CSR_FMT_CSR:
      readsizes = 0;
      readwgts  = 0;

      nnz = nnzcap = rowcap = 0;
      nrows = 0;
      dynamic_rows = 1;
      fpin = gk_fopen(filename, "r", "gk_csr_Read: fpin");
      if (fpin == NULL)
        return NULL;

      break;

    default:
      errno = EINVAL;
      gk_errexit(SIGERR, "Unknown csr format.\n");
      errno = EINVAL;
      return NULL;
  }

  nnzcap = nnz;
  mat = gk_csr_CreateNoSignal(&allocation_failed);
  if (mat == NULL)
    return gk_csr_TextAllocationError(fpin, line, NULL, filename);

  if (!dynamic_rows)
    rowcap = nrows;
  if (nrows > INT32_MAX || nnz > (size_t)PTRDIFF_MAX || rowcap == SIZE_MAX ||
      nrows == SIZE_MAX || (readwgts && nrows != 0 && ncon > SIZE_MAX/nrows) ||
      rowcap+1 > SIZE_MAX/sizeof(ssize_t) ||
      nnz > SIZE_MAX/sizeof(int32_t) ||
      (readvals != 2 && nnz > SIZE_MAX/sizeof(float)) ||
      (readsizes && nrows > SIZE_MAX/sizeof(float)) ||
      (readwgts && nrows*ncon > SIZE_MAX/sizeof(float))) {
    errno = EOVERFLOW;
    return gk_csr_TextReadError(fpin, line, mat, filename);
  }

  mat->nrows = nrows;

  rowptr = mat->rowptr = (ssize_t *)gk_csr_MallocNoSignal(
      (rowcap+1)*sizeof(ssize_t), &allocation_failed);
  rowind = mat->rowind = (int32_t *)gk_csr_MallocNoSignal(
      nnz*sizeof(int32_t), &allocation_failed);
  if (readvals != 2)
    rowval = mat->rowval = (float *)gk_csr_MallocNoSignal(
        nnz*sizeof(float), &allocation_failed);

  if (readsizes)
    mat->rsizes = (float *)gk_csr_MallocNoSignal(
        nrows*sizeof(float), &allocation_failed);

  if (readwgts)
    mat->rwgts = (float *)gk_csr_MallocNoSignal(
        nrows*ncon*sizeof(float), &allocation_failed);
  if (rowptr == NULL || rowind == NULL ||
      (readvals != 2 && rowval == NULL) ||
      (readsizes && mat->rsizes == NULL) ||
      (readwgts && mat->rwgts == NULL))
    return gk_csr_TextAllocationError(fpin, line, mat, filename);
  if (readvals != 2)
    gk_fset(nnz, 1.0, rowval);
  if (readsizes)
    gk_fset(nrows, 0.0, mat->rsizes);
  if (readwgts)
    gk_fset(nrows*ncon, 0.0, mat->rwgts);

  /*----------------------------------------------------------------------
   * Read the sparse matrix file
   *---------------------------------------------------------------------*/
  numbering = (numbering ? -1 : 0);
  for (ncols=0, rowptr[0]=0, k=0, i=0; dynamic_rows || i<nrows; i++) {
    do {
      line_status = gk_getline(&line, &lnlen, fpin);
      if (line_status == -1) {
        if (!feof(fpin) && (errno == ENOMEM || errno == EOVERFLOW))
          return gk_csr_TextAllocationError(fpin, line, mat, filename);
        if (dynamic_rows && feof(fpin))
          goto csr_text_done;
        return gk_csr_TextReadError(fpin, line, mat, filename);
      }
      if (memchr(line, '\0', (size_t)line_status) != NULL)
        return gk_csr_TextReadError(fpin, line, mat, filename);
    } while (line[0] == '%');

    if ((size_t)i == rowcap) {
      if ((size_t)i >= INT32_MAX) {
        errno = EOVERFLOW;
        return gk_csr_TextReadError(fpin, line, mat, filename);
      }
      newcap = rowcap == 0 ? 1 :
               (rowcap > (size_t)INT32_MAX/2 ? INT32_MAX : rowcap*2);
      if (newcap == SIZE_MAX || newcap+1 > SIZE_MAX/sizeof(ssize_t)) {
        errno = EOVERFLOW;
        return gk_csr_TextReadError(fpin, line, mat, filename);
      }
      newrowptr = (ssize_t *)gk_csr_ReallocNoSignal(
          rowptr, (newcap+1)*sizeof(ssize_t), &allocation_failed);
      if (newrowptr == NULL)
        return gk_csr_TextAllocationError(fpin, line, mat, filename);
      rowptr = mat->rowptr = newrowptr;
      rowcap = newcap;
    }

    head = line;
    tail = NULL;

    /* Read vertex sizes */
    if (readsizes) {
#ifdef __MSC__
      errno = 0;
      fval = (float)strtod(head, &tail);
#else
      errno = 0;
      fval = strtof(head, &tail);
#endif
      if (tail == head || errno == ERANGE || !isfinite(fval) || fval < 0)
        return gk_csr_TextReadError(fpin, line, mat, filename);
      mat->rsizes[i] = fval;
      head = tail;
    }

    /* Read vertex weights */
    if (readwgts) {
      for (l=0; l<ncon; l++) {
#ifdef __MSC__
        errno = 0;
        fval = (float)strtod(head, &tail);
#else
        errno = 0;
        fval = strtof(head, &tail);
#endif
        if (tail == head || errno == ERANGE || !isfinite(fval) || fval < 0)
          return gk_csr_TextReadError(fpin, line, mat, filename);
        mat->rwgts[i*ncon+l] = fval;
        head = tail;
      }
    }

   
    /* Read the rest of the row */
    while (1) {
      errno = 0;
      parsed = strtol(head, &tail, 10);
      if (tail == head) 
        break;
      head = tail;

      if (errno == ERANGE || parsed < INT32_MIN || parsed > INT32_MAX)
        return gk_csr_TextReadError(fpin, line, mat, filename);
      if ((size_t)k == nnzcap) {
        if (!dynamic_rows)
          return gk_csr_TextReadError(fpin, line, mat, filename);
        if (nnzcap >= (size_t)PTRDIFF_MAX) {
          errno = EOVERFLOW;
          return gk_csr_TextReadError(fpin, line, mat, filename);
        }
        newcap = nnzcap == 0 ? 1 :
                 (nnzcap > (size_t)PTRDIFF_MAX/2 ?
                  (size_t)PTRDIFF_MAX : nnzcap*2);
        if (newcap > SIZE_MAX/sizeof(int32_t) ||
            (readvals != 2 && newcap > SIZE_MAX/sizeof(float))) {
          errno = EOVERFLOW;
          return gk_csr_TextReadError(fpin, line, mat, filename);
        }
        newrowind = (int32_t *)gk_csr_ReallocNoSignal(
            rowind, newcap*sizeof(int32_t), &allocation_failed);
        if (newrowind == NULL)
          return gk_csr_TextAllocationError(fpin, line, mat, filename);
        rowind = mat->rowind = newrowind;
        if (readvals != 2) {
          newrowval = (float *)gk_csr_ReallocNoSignal(
              rowval, newcap*sizeof(float), &allocation_failed);
          if (newrowval == NULL)
            return gk_csr_TextAllocationError(fpin, line, mat, filename);
          rowval = mat->rowval = newrowval;
        }
        nnzcap = newcap;
      }
      ival = (int32_t)parsed;
      if (numbering == -1 && ival == INT32_MIN)
        return gk_csr_TextReadError(fpin, line, mat, filename);
      
      if ((rowind[k] = ival + numbering) < 0)
        return gk_csr_TextReadError(fpin, line, mat, filename);
      if ((format == GK_CSR_FMT_METIS || format == GK_CSR_FMT_CLUTO) &&
          rowind[k] >= (int32_t)input_ncols)
        return gk_csr_TextReadError(fpin, line, mat, filename);
      if (format == GK_CSR_FMT_CSR && rowind[k] == INT32_MAX)
        return gk_csr_TextReadError(fpin, line, mat, filename);

      ncols = gk_max(rowind[k], ncols);

      if (readvals == 1) {
#ifdef __MSC__
        errno = 0;
        fval = (float)strtod(head, &tail);
#else
        errno = 0;
        fval = strtof(head, &tail);
#endif
        if (tail == head || errno == ERANGE || !isfinite(fval) ||
            (format == GK_CSR_FMT_METIS && fval <= 0))
          return gk_csr_TextReadError(fpin, line, mat, filename);
        head = tail;

        rowval[k] = fval;
      }
      k++;
    }
    while (isspace((unsigned char)*head))
      head++;
    if (*head != '\0')
      return gk_csr_TextReadError(fpin, line, mat, filename);
    rowptr[i+1] = k;
  }

csr_text_done:
  if (dynamic_rows) {
    nrows = i;
    mat->nrows = nrows;
    nnz = k;
  }

  if (format == GK_CSR_FMT_METIS) {
    mat->ncols = mat->nrows;
  }
  else if (format == GK_CSR_FMT_CLUTO) {
    mat->ncols = (int32_t)input_ncols;
  }
  else {
    mat->ncols = k == 0 ? 0 : ncols+1;
  }

  if (!dynamic_rows && k != nnz)
    return gk_csr_TextReadError(fpin, line, mat, filename);

  if (!dynamic_rows) {
    while ((line_status = gk_getline(&line, &lnlen, fpin)) != -1) {
      char *extra=line;
      if (memchr(line, '\0', (size_t)line_status) != NULL)
        return gk_csr_TextReadError(fpin, line, mat, filename);
      while (isspace((unsigned char)*extra))
        extra++;
      if (*extra != '\0' && *extra != '%')
        return gk_csr_TextReadError(fpin, line, mat, filename);
    }
    if (!feof(fpin) && (errno == ENOMEM || errno == EOVERFLOW))
      return gk_csr_TextAllocationError(fpin, line, mat, filename);
    if (!feof(fpin) || ferror(fpin))
      return gk_csr_TextReadError(fpin, line, mat, filename);
  }

  if (fclose(fpin) != 0)
    return gk_csr_TextReadError(NULL, line, mat, filename);

  free(line);

  return mat;
}


/*************************************************************************/
/*! Writes formatted CSR output and reports whether vfprintf succeeded. */
/*************************************************************************/
static int gk_csr_Print(FILE *stream, const char *format, ...)
{
  int status;
  va_list args;

  va_start(args, format);
  status = vfprintf(stream, format, args);
  va_end(args);
  if (status < 0 && errno == 0)
    errno = EIO;
  return status >= 0;
}


/*************************************************************************/
/*! Writes a complete binary item array and detects short writes. */
/*************************************************************************/
static int gk_csr_WriteItems(FILE *stream, const void *items, size_t size,
                             size_t count)
{
  if (count == 0 || fwrite(items, size, count, stream) == count)
    return 1;
  if (errno == 0)
    errno = EIO;
  return 0;
}


/*************************************************************************/
/*! Validates the matrix view required by a selected writer format.

    The row- or column-oriented CSR pointers, indices, dimensions, and any
    required floating-point values are checked without modifying the matrix.
*/
/*************************************************************************/
static int gk_csr_ValidateView(gk_csr_t *mat, int byrow, int need_values)
{
  int32_t i, dimension, other_dimension;
  ssize_t j, count, *pointers;
  int32_t *indices;
  float *values;

  if (mat == NULL || mat->nrows < 0 || mat->ncols < 0)
    return 0;

  dimension = byrow ? mat->nrows : mat->ncols;
  other_dimension = byrow ? mat->ncols : mat->nrows;
  pointers = byrow ? mat->rowptr : mat->colptr;
  indices = byrow ? mat->rowind : mat->colind;
  values = byrow ? mat->rowval : mat->colval;
  if (pointers == NULL || pointers[0] != 0)
    return 0;
  for (i=0; i<dimension; i++) {
    if (pointers[i] < 0 || pointers[i] > pointers[i+1])
      return 0;
  }

  count = pointers[dimension];
  if (count < 0 || (count > 0 && indices == NULL) ||
      (need_values && count > 0 && values == NULL))
    return 0;
  if ((size_t)dimension+1 > SIZE_MAX/sizeof(ssize_t) ||
      (size_t)count > SIZE_MAX/sizeof(int32_t) ||
      (need_values && (size_t)count > SIZE_MAX/sizeof(float))) {
    errno = EOVERFLOW;
    return -1;
  }
  for (j=0; j<count; j++) {
    if (indices[j] < 0 || indices[j] >= other_dimension ||
        (need_values && !isfinite(values[j])))
      return 0;
  }

  return 1;
}


typedef struct {
  int32_t source;
  int32_t target;
} gk_csr_symmetry_edge_t;


/*************************************************************************/
/*! Orders matrix entries by source and target index. */
/*************************************************************************/
static int gk_csr_CompareSymmetryEdges(const void *first,
    const void *second)
{
  const gk_csr_symmetry_edge_t *left=first;
  const gk_csr_symmetry_edge_t *right=second;

  if (left->source != right->source)
    return left->source < right->source ? -1 : 1;
  if (left->target != right->target)
    return left->target < right->target ? -1 : 1;
  return 0;
}


/*************************************************************************/
/*! Finds the first sorted matrix entry matching an ordered index pair. */
/*************************************************************************/
static size_t gk_csr_LowerSymmetryEdge(gk_csr_symmetry_edge_t *edges,
    size_t nedges, int32_t source, int32_t target)
{
  gk_csr_symmetry_edge_t key;
  size_t first=0, length=nedges;

  key.source = source;
  key.target = target;
  while (length != 0) {
    size_t half=length/2;
    size_t middle=first+half;

    if (gk_csr_CompareSymmetryEdges(edges+middle, &key) < 0) {
      first = middle+1;
      length -= half+1;
    }
    else {
      length = half;
    }
  }

  return first;
}


/*************************************************************************/
/*! Validates the reciprocal multiset required by METIS matrix output.

    Duplicate entries must have equally many reverse entries. Self entries
    are rejected because METIS counts each undirected edge through two
    adjacency entries. A negative result denotes allocation failure.
*/
/*************************************************************************/
static int gk_csr_ValidateSymmetry(gk_csr_t *mat)
{
  gk_csr_symmetry_edge_t *edges;
  ssize_t edge;
  size_t i, j, nedges, reverse, reverse_end;
  int32_t row;

  nedges = (size_t)mat->rowptr[mat->nrows];
  if (nedges%2 != 0)
    return 0;
  if (nedges > SIZE_MAX/sizeof(*edges)) {
    errno = EOVERFLOW;
    return -1;
  }
  edges = (gk_csr_symmetry_edge_t *)gk_malloc_nosignal(
      nedges*sizeof(*edges));
  if (edges == NULL)
    return -1;

  i = 0;
  for (row=0; row<mat->nrows; row++) {
    for (edge=mat->rowptr[row]; edge<mat->rowptr[row+1]; edge++) {
      edges[i].source = row;
      edges[i].target = mat->rowind[edge];
      i++;
    }
  }
  qsort(edges, nedges, sizeof(*edges), gk_csr_CompareSymmetryEdges);

  for (i=0; i<nedges; i=j) {
    for (j=i+1; j<nedges &&
         gk_csr_CompareSymmetryEdges(edges+i, edges+j) == 0; j++) {
    }
    if (edges[i].source == edges[i].target) {
      gk_free((void **)&edges, LTERM);
      return 0;
    }

    reverse = gk_csr_LowerSymmetryEdge(edges, nedges,
        edges[i].target, edges[i].source);
    for (reverse_end=reverse; reverse_end<nedges &&
         edges[reverse_end].source == edges[i].target &&
         edges[reverse_end].target == edges[i].source; reverse_end++) {
    }
    if (reverse == nedges || reverse_end-reverse != j-i) {
      gk_free((void **)&edges, LTERM);
      return 0;
    }
  }

  gk_free((void **)&edges, LTERM);
  return 1;
}


/**************************************************************************/
/*! Writes the row-based structure of a matrix into a file.
    \param mat is the matrix to be written,
    \param filename is the name of the output file.
    \param format is one of: GK_CSR_FMT_CLUTO, GK_CSR_FMT_CSR,
           GK_CSR_FMT_BINROW, GK_CSR_FMT_BINCOL, GK_CSR_FMT_BIJV.
    \param writevals is either 1 or 0 indicating if the values will be
           written or not. This is only applicable when GK_CSR_FMT_CSR
           is used.
    \param numbering is either 1 or 0 indicating if the internal 0-based
           numbering will be shifted by one or not during output. This
           is only applicable when GK_CSR_FMT_CSR is used.
*/
/**************************************************************************/
void gk_csr_Write(gk_csr_t *mat, char *filename, int format, int writevals, int numbering)
{
  int symmetry, validation;
  ssize_t i, j;
  int saved_errno=0;
  int32_t edge[2];
  int failed=0;
  char *tempname=NULL;
  FILE *fpout=NULL;

  format = gk_csr_DetermineFormat(filename, format);

  validation = ((writevals != 0 && writevals != 1) ||
      (numbering != 0 && numbering != 1)) ? 0 :
      gk_csr_ValidateView(mat, format != GK_CSR_FMT_BINCOL,
                          writevals || format == GK_CSR_FMT_CLUTO);
  if (validation <= 0) {
    saved_errno = validation < 0 ?
        (errno != 0 ? errno : EOVERFLOW) : EINVAL;
    errno = saved_errno;
    gk_errexit(validation < 0 ? SIGMEM : SIGERR,
        "Cannot write an invalid CSR structure.\n");
    errno = saved_errno;
    return;
  }

  switch (format) {
    case GK_CSR_FMT_METIS:
      if (mat->nrows != mat->ncols) {
        errno = EINVAL;
        gk_errexit(SIGERR, "METIS output format requires a square symmetric matrix.\n");
        errno = EINVAL;
        return;
      }
      symmetry = gk_csr_ValidateSymmetry(mat);
      if (symmetry <= 0) {
        saved_errno = symmetry < 0 ?
            (errno != 0 ? errno : ENOMEM) : EINVAL;
        errno = saved_errno;
        gk_errexit(symmetry < 0 ? SIGMEM : SIGERR,
            symmetry < 0 ? "Failed to validate matrix symmetry.\n" :
            "METIS output format requires a square symmetric matrix.\n");
        errno = saved_errno;
        return;
      }

      if (filename)
        fpout = gk_open_output_file(filename, "w", &tempname);
      else
        fpout = stdout; 
      if (fpout == NULL)
        goto csr_write_open_failure;

      failed = !gk_csr_Print(fpout, "%d %zd\n", mat->nrows,
                             mat->rowptr[mat->nrows]/2);
      for (i=0; i<mat->nrows; i++) {
        for (j=mat->rowptr[i]; !failed && j<mat->rowptr[i+1]; j++)
          failed = !gk_csr_Print(fpout, " %d", mat->rowind[j]+1);
        if (!failed)
          failed = !gk_csr_Print(fpout, "\n");
        if (failed)
          break;
      }
      break;

    case GK_CSR_FMT_BINROW:
      if (filename == NULL) {
        errno = EINVAL;
        gk_errexit(SIGERR, "The filename parameter cannot be NULL.\n");
        errno = EINVAL;
        return;
      }
      fpout = gk_open_output_file(filename, "wb", &tempname);
      if (fpout == NULL)
        goto csr_write_open_failure;

      failed = !gk_csr_WriteItems(fpout, &mat->nrows, sizeof(int32_t), 1) ||
               !gk_csr_WriteItems(fpout, &mat->ncols, sizeof(int32_t), 1) ||
               !gk_csr_WriteItems(fpout, mat->rowptr, sizeof(ssize_t),
                                  (size_t)mat->nrows+1) ||
               !gk_csr_WriteItems(fpout, mat->rowind, sizeof(int32_t),
                                  (size_t)mat->rowptr[mat->nrows]) ||
               (writevals &&
                !gk_csr_WriteItems(fpout, mat->rowval, sizeof(float),
                                   (size_t)mat->rowptr[mat->nrows]));
      break;

    case GK_CSR_FMT_BINCOL:
      if (filename == NULL) {
        errno = EINVAL;
        gk_errexit(SIGERR, "The filename parameter cannot be NULL.\n");
        errno = EINVAL;
        return;
      }
      fpout = gk_open_output_file(filename, "wb", &tempname);
      if (fpout == NULL)
        goto csr_write_open_failure;

      failed = !gk_csr_WriteItems(fpout, &mat->nrows, sizeof(int32_t), 1) ||
               !gk_csr_WriteItems(fpout, &mat->ncols, sizeof(int32_t), 1) ||
               !gk_csr_WriteItems(fpout, mat->colptr, sizeof(ssize_t),
                                  (size_t)mat->ncols+1) ||
               !gk_csr_WriteItems(fpout, mat->colind, sizeof(int32_t),
                                  (size_t)mat->colptr[mat->ncols]) ||
               (writevals &&
                !gk_csr_WriteItems(fpout, mat->colval, sizeof(float),
                                   (size_t)mat->colptr[mat->ncols]));
      break;

    case GK_CSR_FMT_IJV:
      if (filename == NULL) {
        errno = EINVAL;
        gk_errexit(SIGERR, "The filename parameter cannot be NULL.\n");
        errno = EINVAL;
        return;
      }
      fpout = gk_open_output_file(filename, "w", &tempname);
      if (fpout == NULL)
        goto csr_write_open_failure;

      numbering = (numbering ? 1 : 0);
      for (i=0; i<mat->nrows; i++) {
        for (j=mat->rowptr[i]; j<mat->rowptr[i+1]; j++) {
          if (writevals)
            failed = !gk_csr_Print(fpout, "%zd %d %.8f\n", i+numbering,
                                   mat->rowind[j]+numbering, mat->rowval[j]);
          else
            failed = !gk_csr_Print(fpout, "%zd %d\n", i+numbering,
                                   mat->rowind[j]+numbering);
          if (failed)
            break;
        }
        if (failed)
          break;
      }
      break;

    case GK_CSR_FMT_BIJV:
      if (filename == NULL) {
        errno = EINVAL;
        gk_errexit(SIGERR, "The filename parameter cannot be NULL.\n");
        errno = EINVAL;
        return;
      }
      fpout = gk_open_output_file(filename, "wb", &tempname);
      if (fpout == NULL)
        goto csr_write_open_failure;

      failed = !gk_csr_WriteItems(fpout, &mat->nrows, sizeof(int32_t), 1) ||
               !gk_csr_WriteItems(fpout, &mat->ncols, sizeof(int32_t), 1) ||
               !gk_csr_WriteItems(fpout, &mat->rowptr[mat->nrows],
                                  sizeof(size_t), 1) ||
               !gk_csr_WriteItems(fpout, &writevals, sizeof(int32_t), 1);

      for (i=0; !failed && i<mat->nrows; i++) {
        edge[0] = (int32_t)i;
        for (j=mat->rowptr[i]; j<mat->rowptr[i+1]; j++) {
          edge[1] = mat->rowind[j];
          failed = !gk_csr_WriteItems(fpout, edge, sizeof(int32_t), 2) ||
                   (writevals &&
                    !gk_csr_WriteItems(fpout, &mat->rowval[j],
                                       sizeof(float), 1));
          if (failed)
            break;
        }
      }
      break;

    case GK_CSR_FMT_CLUTO:
    case GK_CSR_FMT_CSR:
      if (filename)
        fpout = gk_open_output_file(filename, "w", &tempname);
      else
        fpout = stdout; 
      if (fpout == NULL)
        goto csr_write_open_failure;

      if (format == GK_CSR_FMT_CLUTO) {
        failed = !gk_csr_Print(fpout, "%d %d %zd\n", mat->nrows,
                               mat->ncols, mat->rowptr[mat->nrows]);
        writevals = 1;
        numbering = 1;
      }

      for (i=0; !failed && i<mat->nrows; i++) {
        for (j=mat->rowptr[i]; !failed && j<mat->rowptr[i+1]; j++) {
          failed = !gk_csr_Print(fpout, " %d",
                                 mat->rowind[j]+(numbering ? 1 : 0));
          if (!failed && writevals)
            failed = !gk_csr_Print(fpout, " %f", mat->rowval[j]);
        }
        if (!failed)
          failed = !gk_csr_Print(fpout, "\n");
      }
      break;

    default:
      errno = EINVAL;
      gk_errexit(SIGERR, "Unknown CSR output format. %d\n", format);
      errno = EINVAL;
      return;
  }

  if (!gk_finish_output_file(fpout, &tempname, filename, !failed)) {
    failed = 1;
    saved_errno = errno != 0 ? errno : EIO;
  }
  if (failed) {
    if (saved_errno == 0)
      saved_errno = EIO;
    errno = saved_errno;
  }
  if (failed) {
    gk_errexit(saved_errno == ENOMEM || saved_errno == EOVERFLOW ?
        SIGMEM : SIGERR, "Failed to write CSR file %s.\n",
               filename != NULL ? filename : "<stdout>");
    errno = saved_errno;
  }
  return;

csr_write_open_failure:
  saved_errno = errno != 0 ? errno : EIO;
  errno = saved_errno;
  gk_errexit(saved_errno == ENOMEM || saved_errno == EOVERFLOW ?
      SIGMEM : SIGERR, "Failed to open CSR output file %s.\n",
             filename != NULL ? filename : "<stdout>");
  errno = saved_errno;
}


/*************************************************************************/
/*! Prunes certain rows/columns of the matrix. The prunning takes place 
    by analyzing the row structure of the matrix. The prunning takes place
    by removing rows/columns but it does not affect the numbering of the
    remaining rows/columns.
   
    \param mat the matrix to be prunned,
    \param what indicates if the rows (GK_CSR_ROW) or the columns (GK_CSR_COL)
           of the matrix will be prunned,
    \param minf is the minimum number of rows (columns) that a column (row) must
           be present in order to be kept,
    \param maxf is the maximum number of rows (columns) that a column (row) must
          be present at in order to be kept.
    \returns the prunned matrix consisting only of its row-based structure. 
          The input matrix is not modified. 
*/
/**************************************************************************/
gk_csr_t *gk_csr_Prune(gk_csr_t *mat, int what, int minf, int maxf)
{
  ssize_t i, j, nnz;
  int nrows, ncols;
  ssize_t *rowptr, *nrowptr;
  int *rowind, *nrowind, *collen;
  float *rowval, *nrowval;
  gk_csr_t *nmat;

  nmat = gk_csr_Create();
  
  nrows = nmat->nrows = mat->nrows;
  ncols = nmat->ncols = mat->ncols;

  rowptr = mat->rowptr;
  rowind = mat->rowind;
  rowval = mat->rowval;

  nrowptr = nmat->rowptr = gk_zmalloc(nrows+1, "gk_csr_Prune: nrowptr");
  nrowind = nmat->rowind = gk_imalloc(rowptr[nrows], "gk_csr_Prune: nrowind");
  nrowval = nmat->rowval = gk_fmalloc(rowptr[nrows], "gk_csr_Prune: nrowval");


  switch (what) {
    case GK_CSR_COL:
      collen = gk_ismalloc(ncols, 0, "gk_csr_Prune: collen");

      for (i=0; i<nrows; i++) {
        for (j=rowptr[i]; j<rowptr[i+1]; j++) {
          ASSERT(rowind[j] < ncols);
          collen[rowind[j]]++;
        }
      }
      for (i=0; i<ncols; i++)
        collen[i] = (collen[i] >= minf && collen[i] <= maxf ? 1 : 0);

      nrowptr[0] = 0;
      for (nnz=0, i=0; i<nrows; i++) {
        for (j=rowptr[i]; j<rowptr[i+1]; j++) {
          if (collen[rowind[j]]) {
            nrowind[nnz] = rowind[j];
            nrowval[nnz] = rowval[j];
            nnz++;
          }
        }
        nrowptr[i+1] = nnz;
      }
      gk_free((void **)&collen, LTERM);
      break;

    case GK_CSR_ROW:
      nrowptr[0] = 0;
      for (nnz=0, i=0; i<nrows; i++) {
        if (rowptr[i+1]-rowptr[i] >= minf && rowptr[i+1]-rowptr[i] <= maxf) {
          for (j=rowptr[i]; j<rowptr[i+1]; j++, nnz++) {
            nrowind[nnz] = rowind[j];
            nrowval[nnz] = rowval[j];
          }
        }
        nrowptr[i+1] = nnz;
      }
      break;

    default:
      gk_csr_Free(&nmat);
      gk_errexit(SIGERR, "Unknown prunning type of %d\n", what);
      return NULL;
  }

  return nmat;
}


/*************************************************************************/
/*! Eliminates certain entries from the rows/columns of the matrix. The 
    filtering takes place by keeping only the highest weight entries whose
    sum accounts for a certain fraction of the overall weight of the 
    row/column.
   
    \param mat the matrix to be prunned,
    \param what indicates if the rows (GK_CSR_ROW) or the columns (GK_CSR_COL)
           of the matrix will be prunned,
    \param norm indicates the norm that will be used to aggregate the weights
           and possible values are 1 or 2,
    \param fraction is the fraction of the overall norm that will be retained
           by the kept entries.
    \returns the filtered matrix consisting only of its row-based structure. 
           The input matrix is not modified. 
*/
/**************************************************************************/
gk_csr_t *gk_csr_LowFilter(gk_csr_t *mat, int what, int norm, float fraction)
{
  ssize_t i, j, nnz;
  int nrows, ncols, ncand, maxlen=0;
  ssize_t *rowptr, *colptr, *nrowptr;
  int *rowind, *colind, *nrowind;
  float *rowval, *colval, *nrowval, rsum, tsum;
  gk_csr_t *nmat;
  gk_fkv_t *cand;

  nmat = gk_csr_Create();
  
  nrows = nmat->nrows = mat->nrows;
  ncols = nmat->ncols = mat->ncols;

  rowptr = mat->rowptr;
  rowind = mat->rowind;
  rowval = mat->rowval;
  colptr = mat->colptr;
  colind = mat->colind;
  colval = mat->colval;

  nrowptr = nmat->rowptr = gk_zmalloc(nrows+1, "gk_csr_LowFilter: nrowptr");
  nrowind = nmat->rowind = gk_imalloc(rowptr[nrows], "gk_csr_LowFilter: nrowind");
  nrowval = nmat->rowval = gk_fmalloc(rowptr[nrows], "gk_csr_LowFilter: nrowval");


  switch (what) {
    case GK_CSR_COL:
      if (mat->colptr == NULL) 
        gk_errexit(SIGERR, "Cannot filter columns when column-based structure has not been created.\n");

      gk_zcopy(nrows+1, rowptr, nrowptr);

      for (i=0; i<ncols; i++) 
        maxlen = gk_max(maxlen, colptr[i+1]-colptr[i]);

      #pragma omp parallel private(i, j, ncand, rsum, tsum, cand)
      {
        cand = gk_fkvmalloc(maxlen, "gk_csr_LowFilter: cand");

        #pragma omp for schedule(static)
        for (i=0; i<ncols; i++) {
          for (tsum=0.0, ncand=0, j=colptr[i]; j<colptr[i+1]; j++, ncand++) {
            cand[ncand].val = colind[j];
            cand[ncand].key = colval[j];
            tsum += (norm == 1 ? colval[j] : colval[j]*colval[j]);
          }
          gk_fkvsortd(ncand, cand);

          for (rsum=0.0, j=0; j<ncand && rsum<=fraction*tsum; j++) {
            rsum += (norm == 1 ? cand[j].key : cand[j].key*cand[j].key);
            nrowind[nrowptr[cand[j].val]] = i;
            nrowval[nrowptr[cand[j].val]] = cand[j].key;
            nrowptr[cand[j].val]++;
          }
        }

        gk_free((void **)&cand, LTERM);
      }

      /* compact the nrowind/nrowval */
      for (nnz=0, i=0; i<nrows; i++) {
        for (j=rowptr[i]; j<nrowptr[i]; j++, nnz++) {
          nrowind[nnz] = nrowind[j];
          nrowval[nnz] = nrowval[j];
        }
        nrowptr[i] = nnz;
      }
      SHIFTCSR(i, nrows, nrowptr);

      break;

    case GK_CSR_ROW:
      if (mat->rowptr == NULL) 
        gk_errexit(SIGERR, "Cannot filter rows when row-based structure has not been created.\n");

      for (i=0; i<nrows; i++) 
        maxlen = gk_max(maxlen, rowptr[i+1]-rowptr[i]);

      #pragma omp parallel private(i, j, ncand, rsum, tsum, cand)
      {
        cand = gk_fkvmalloc(maxlen, "gk_csr_LowFilter: cand");

        #pragma omp for schedule(static)
        for (i=0; i<nrows; i++) {
          for (tsum=0.0, ncand=0, j=rowptr[i]; j<rowptr[i+1]; j++, ncand++) {
            cand[ncand].val = rowind[j];
            cand[ncand].key = rowval[j];
            tsum += (norm == 1 ? rowval[j] : rowval[j]*rowval[j]);
          }
          gk_fkvsortd(ncand, cand);

          for (rsum=0.0, j=0; j<ncand && rsum<=fraction*tsum; j++) {
            rsum += (norm == 1 ? cand[j].key : cand[j].key*cand[j].key);
            nrowind[rowptr[i]+j] = cand[j].val;
            nrowval[rowptr[i]+j] = cand[j].key;
          }
          nrowptr[i+1] = rowptr[i]+j;
        }

        gk_free((void **)&cand, LTERM);
      }

      /* compact nrowind/nrowval */
      nrowptr[0] = nnz = 0;
      for (i=0; i<nrows; i++) {
        for (j=rowptr[i]; j<nrowptr[i+1]; j++, nnz++) {
          nrowind[nnz] = nrowind[j];
          nrowval[nnz] = nrowval[j];
        }
        nrowptr[i+1] = nnz;
      }

      break;

    default:
      gk_csr_Free(&nmat);
      gk_errexit(SIGERR, "Unknown prunning type of %d\n", what);
      return NULL;
  }

  return nmat;
}


/*************************************************************************/
/*! Eliminates certain entries from the rows/columns of the matrix. The 
    filtering takes place by keeping only the highest weight top-K entries 
    along each row/column and those entries whose weight is greater than
    a specified value.
   
    \param mat the matrix to be prunned,
    \param what indicates if the rows (GK_CSR_ROW) or the columns (GK_CSR_COL)
           of the matrix will be prunned,
    \param topk is the number of the highest weight entries to keep.
    \param keepval is the weight of a term above which will be kept. This
           is used to select additional terms past the first topk.
    \returns the filtered matrix consisting only of its row-based structure. 
           The input matrix is not modified. 
*/
/**************************************************************************/
gk_csr_t *gk_csr_TopKPlusFilter(gk_csr_t *mat, int what, int topk, float keepval)
{
  ssize_t i, j, k, nnz;
  int nrows, ncols, ncand;
  ssize_t *rowptr, *colptr, *nrowptr;
  int *rowind, *colind, *nrowind;
  float *rowval, *colval, *nrowval;
  gk_csr_t *nmat;
  gk_fkv_t *cand;

  nmat = gk_csr_Create();
  
  nrows = nmat->nrows = mat->nrows;
  ncols = nmat->ncols = mat->ncols;

  rowptr = mat->rowptr;
  rowind = mat->rowind;
  rowval = mat->rowval;
  colptr = mat->colptr;
  colind = mat->colind;
  colval = mat->colval;

  nrowptr = nmat->rowptr = gk_zmalloc(nrows+1, "gk_csr_LowFilter: nrowptr");
  nrowind = nmat->rowind = gk_imalloc(rowptr[nrows], "gk_csr_LowFilter: nrowind");
  nrowval = nmat->rowval = gk_fmalloc(rowptr[nrows], "gk_csr_LowFilter: nrowval");


  switch (what) {
    case GK_CSR_COL:
      if (mat->colptr == NULL) 
        gk_errexit(SIGERR, "Cannot filter columns when column-based structure has not been created.\n");

      cand = gk_fkvmalloc(nrows, "gk_csr_LowFilter: cand");

      gk_zcopy(nrows+1, rowptr, nrowptr);
      for (i=0; i<ncols; i++) {
        for (ncand=0, j=colptr[i]; j<colptr[i+1]; j++, ncand++) {
          cand[ncand].val = colind[j];
          cand[ncand].key = colval[j];
        }
        gk_fkvsortd(ncand, cand);

        k = gk_min(topk, ncand);
        for (j=0; j<k; j++) {
          nrowind[nrowptr[cand[j].val]] = i;
          nrowval[nrowptr[cand[j].val]] = cand[j].key;
          nrowptr[cand[j].val]++;
        }
        for (; j<ncand; j++) {
          if (cand[j].key < keepval) 
            break;

          nrowind[nrowptr[cand[j].val]] = i;
          nrowval[nrowptr[cand[j].val]] = cand[j].key;
          nrowptr[cand[j].val]++;
        }
      }

      /* compact the nrowind/nrowval */
      for (nnz=0, i=0; i<nrows; i++) {
        for (j=rowptr[i]; j<nrowptr[i]; j++, nnz++) {
          nrowind[nnz] = nrowind[j];
          nrowval[nnz] = nrowval[j];
        }
        nrowptr[i] = nnz;
      }
      SHIFTCSR(i, nrows, nrowptr);

      gk_free((void **)&cand, LTERM);
      break;

    case GK_CSR_ROW:
      if (mat->rowptr == NULL) 
        gk_errexit(SIGERR, "Cannot filter rows when row-based structure has not been created.\n");

      cand = gk_fkvmalloc(ncols, "gk_csr_LowFilter: cand");

      nrowptr[0] = 0;
      for (nnz=0, i=0; i<nrows; i++) {
        for (ncand=0, j=rowptr[i]; j<rowptr[i+1]; j++, ncand++) {
          cand[ncand].val = rowind[j];
          cand[ncand].key = rowval[j];
        }
        gk_fkvsortd(ncand, cand);

        k = gk_min(topk, ncand);
        for (j=0; j<k; j++, nnz++) {
          nrowind[nnz] = cand[j].val;
          nrowval[nnz] = cand[j].key;
        }
        for (; j<ncand; j++, nnz++) {
          if (cand[j].key < keepval) 
            break;

          nrowind[nnz] = cand[j].val;
          nrowval[nnz] = cand[j].key;
        }
        nrowptr[i+1] = nnz;
      }

      gk_free((void **)&cand, LTERM);
      break;

    default:
      gk_csr_Free(&nmat);
      gk_errexit(SIGERR, "Unknown prunning type of %d\n", what);
      return NULL;
  }

  return nmat;
}


/*************************************************************************/
/*! Eliminates certain entries from the rows/columns of the matrix. The 
    filtering takes place by keeping only the terms whose contribution to
    the total length of the document is greater than a user-splied multiple
    over the average.

    This routine assumes that the vectors are normalized to be unit length.
   
    \param mat the matrix to be prunned,
    \param what indicates if the rows (GK_CSR_ROW) or the columns (GK_CSR_COL)
           of the matrix will be prunned,
    \param zscore is the multiplicative factor over the average contribution 
           to the length of the document.
    \returns the filtered matrix consisting only of its row-based structure. 
           The input matrix is not modified. 
*/
/**************************************************************************/
gk_csr_t *gk_csr_ZScoreFilter(gk_csr_t *mat, int what, float zscore)
{
  ssize_t i, j, nnz;
  int nrows;
  ssize_t *rowptr, *nrowptr;
  int *rowind, *nrowind;
  float *rowval, *nrowval, avgwgt;
  gk_csr_t *nmat;

  nmat = gk_csr_Create();
  
  nmat->nrows = mat->nrows;
  nmat->ncols = mat->ncols;

  nrows  = mat->nrows; 
  rowptr = mat->rowptr;
  rowind = mat->rowind;
  rowval = mat->rowval;

  nrowptr = nmat->rowptr = gk_zmalloc(nrows+1, "gk_csr_ZScoreFilter: nrowptr");
  nrowind = nmat->rowind = gk_imalloc(rowptr[nrows], "gk_csr_ZScoreFilter: nrowind");
  nrowval = nmat->rowval = gk_fmalloc(rowptr[nrows], "gk_csr_ZScoreFilter: nrowval");


  switch (what) {
    case GK_CSR_COL:
      gk_errexit(SIGERR, "This has not been implemented yet.\n");
      break;

    case GK_CSR_ROW:
      if (mat->rowptr == NULL) 
        gk_errexit(SIGERR, "Cannot filter rows when row-based structure has not been created.\n");

      nrowptr[0] = 0;
      for (nnz=0, i=0; i<nrows; i++) {
        avgwgt = zscore/(rowptr[i+1]-rowptr[i]);
        for (j=rowptr[i]; j<rowptr[i+1]; j++) {
          if (rowval[j] > avgwgt) {
            nrowind[nnz] = rowind[j];
            nrowval[nnz] = rowval[j];
            nnz++;
          }
        }
        nrowptr[i+1] = nnz;
      }
      break;

    default:
      gk_csr_Free(&nmat);
      gk_errexit(SIGERR, "Unknown prunning type of %d\n", what);
      return NULL;
  }

  return nmat;
}


/*************************************************************************/
/*! Compacts the column-space of the matrix by removing empty columns.
    As a result of the compaction, the column numbers are renumbered. 
    The compaction operation is done in place and only affects the row-based
    representation of the matrix.
    The new columns are ordered in decreasing frequency.
   
    \param mat the matrix whose empty columns will be removed.
*/
/**************************************************************************/
void gk_csr_CompactColumns(gk_csr_t *mat)
{
  ssize_t i;
  int nrows, ncols, nncols;
  ssize_t *rowptr;
  int *rowind, *colmap;
  gk_ikv_t *clens;

  nrows  = mat->nrows;
  ncols  = mat->ncols;
  rowptr = mat->rowptr;
  rowind = mat->rowind;

  colmap = gk_imalloc(ncols, "gk_csr_CompactColumns: colmap");

  clens = gk_ikvmalloc(ncols, "gk_csr_CompactColumns: clens");
  for (i=0; i<ncols; i++) {
    clens[i].key = 0;
    clens[i].val = i;
  }

  for (i=0; i<rowptr[nrows]; i++) 
    clens[rowind[i]].key++;
  gk_ikvsortd(ncols, clens);

  for (nncols=0, i=0; i<ncols; i++) {
    if (clens[i].key > 0) 
      colmap[clens[i].val] = nncols++;
    else
      break;
  }

  for (i=0; i<rowptr[nrows]; i++) 
    rowind[i] = colmap[rowind[i]];

  mat->ncols = nncols;

  gk_free((void **)&colmap, &clens, LTERM);
}


/*************************************************************************/
/*! Sorts the indices in increasing order
    \param mat the matrix itself,
    \param what is either GK_CSR_ROW or GK_CSR_COL indicating which set of
           indices to sort.
*/
/**************************************************************************/
void gk_csr_SortIndices(gk_csr_t *mat, int what)
{
  int n, nn=0;
  ssize_t *ptr;
  int *ind;
  float *val;

  switch (what) {
    case GK_CSR_ROW:
      if (!mat->rowptr)
        gk_errexit(SIGERR, "Row-based view of the matrix does not exists.\n");

      n   = mat->nrows;
      ptr = mat->rowptr;
      ind = mat->rowind;
      val = mat->rowval;
      break;

    case GK_CSR_COL:
      if (!mat->colptr)
        gk_errexit(SIGERR, "Column-based view of the matrix does not exists.\n");

      n   = mat->ncols;
      ptr = mat->colptr;
      ind = mat->colind;
      val = mat->colval;
      break;

    default:
      gk_errexit(SIGERR, "Invalid index type of %d.\n", what);
      return;
  }

  #pragma omp parallel if (n > 100)
  {
    ssize_t i, j, k;
    gk_ikv_t *cand;
    float *tval;

    #pragma omp single
    for (i=0; i<n; i++) 
      nn = gk_max(nn, ptr[i+1]-ptr[i]);
  
    cand = gk_ikvmalloc(nn, "gk_csr_SortIndices: cand");
    tval = gk_fmalloc(nn, "gk_csr_SortIndices: tval");
  
    #pragma omp for schedule(static)
    for (i=0; i<n; i++) {
      for (k=0, j=ptr[i]; j<ptr[i+1]; j++) {
        if (j > ptr[i] && ind[j] < ind[j-1])
          k = 1; /* an inversion */
        cand[j-ptr[i]].val = j-ptr[i];
        cand[j-ptr[i]].key = ind[j];
        tval[j-ptr[i]]     = val[j];
      }
      if (k) {
        gk_ikvsorti(ptr[i+1]-ptr[i], cand);
        for (j=ptr[i]; j<ptr[i+1]; j++) {
          ind[j] = cand[j-ptr[i]].key;
          val[j] = tval[cand[j-ptr[i]].val];
        }
      }
    }

    gk_free((void **)&cand, &tval, LTERM);
  }

}


/*************************************************************************/
/*! Creates a row/column index from the column/row data.
    \param mat the matrix itself,
    \param what is either GK_CSR_ROW or GK_CSR_COL indicating which index
           will be created.
*/
/**************************************************************************/
void gk_csr_CreateIndex(gk_csr_t *mat, int what)
{
  /* 'f' stands for forward, 'r' stands for reverse */
  ssize_t i, j, k, nf, nr;
  ssize_t *fptr, *rptr;
  int *find, *rind;
  float *fval, *rval;

  switch (what) {
    case GK_CSR_COL:
      nf   = mat->nrows;
      fptr = mat->rowptr;
      find = mat->rowind;
      fval = mat->rowval;

      if (mat->colptr) gk_free((void **)&mat->colptr, LTERM);
      if (mat->colind) gk_free((void **)&mat->colind, LTERM);
      if (mat->colval) gk_free((void **)&mat->colval, LTERM);

      nr   = mat->ncols;
      rptr = mat->colptr = gk_zsmalloc(nr+1, 0, "gk_csr_CreateIndex: rptr");
      rind = mat->colind = gk_imalloc(fptr[nf], "gk_csr_CreateIndex: rind");
      rval = mat->colval = (fval ? gk_fmalloc(fptr[nf], "gk_csr_CreateIndex: rval") : NULL);
      break;
    case GK_CSR_ROW:
      nf   = mat->ncols;
      fptr = mat->colptr;
      find = mat->colind;
      fval = mat->colval;

      if (mat->rowptr) gk_free((void **)&mat->rowptr, LTERM);
      if (mat->rowind) gk_free((void **)&mat->rowind, LTERM);
      if (mat->rowval) gk_free((void **)&mat->rowval, LTERM);

      nr   = mat->nrows;
      rptr = mat->rowptr = gk_zsmalloc(nr+1, 0, "gk_csr_CreateIndex: rptr");
      rind = mat->rowind = gk_imalloc(fptr[nf], "gk_csr_CreateIndex: rind");
      rval = mat->rowval = (fval ? gk_fmalloc(fptr[nf], "gk_csr_CreateIndex: rval") : NULL);
      break;
    default:
      gk_errexit(SIGERR, "Invalid index type of %d.\n", what);
      return;
  }


  for (i=0; i<nf; i++) {
    for (j=fptr[i]; j<fptr[i+1]; j++)
      rptr[find[j]]++;
  }
  MAKECSR(i, nr, rptr);
  
  if (rptr[nr] > 6*nr) {
    for (i=0; i<nf; i++) {
      for (j=fptr[i]; j<fptr[i+1]; j++) 
        rind[rptr[find[j]]++] = i;
    }
    SHIFTCSR(i, nr, rptr);

    if (fval) {
      for (i=0; i<nf; i++) {
        for (j=fptr[i]; j<fptr[i+1]; j++) 
          rval[rptr[find[j]]++] = fval[j];
      }
      SHIFTCSR(i, nr, rptr);
    }
  }
  else {
    if (fval) {
      for (i=0; i<nf; i++) {
        for (j=fptr[i]; j<fptr[i+1]; j++) {
          k = find[j];
          rind[rptr[k]]   = i;
          rval[rptr[k]++] = fval[j];
        }
      }
    }
    else {
      for (i=0; i<nf; i++) {
        for (j=fptr[i]; j<fptr[i+1]; j++) 
          rind[rptr[find[j]]++] = i;
      }
    }
    SHIFTCSR(i, nr, rptr);
  }
}


/*************************************************************************/
/*! Normalizes the rows/columns of the matrix to be unit 
    length.
    \param mat the matrix itself,
    \param what indicates what will be normalized and is obtained by
           specifying GK_CSR_ROW, GK_CSR_COL, GK_CSR_ROW|GK_CSR_COL. 
    \param norm indicates what norm is to normalize to, 1: 1-norm, 2: 2-norm
*/
/**************************************************************************/
void gk_csr_Normalize(gk_csr_t *mat, int what, int norm)
{
  ssize_t i, j;
  int n;
  ssize_t *ptr;
  float *val, sum;


  if (what&GK_CSR_ROW && mat->rowval) {
    n   = mat->nrows;
    ptr = mat->rowptr;
    val = mat->rowval;

    #pragma omp parallel for if (ptr[n] > OMPMINOPS) private(j,sum) schedule(static)
    for (i=0; i<n; i++) {
      sum = 0.0;
      if (norm == 1) {
        for (j=ptr[i]; j<ptr[i+1]; j++) 
          sum += val[j]; /* assume val[j] > 0 */ 
        if (sum > 0)
          sum = 1.0/sum;
      }
      else if (norm == 2) {
        for (j=ptr[i]; j<ptr[i+1]; j++) 
          sum += val[j]*val[j];
        if (sum > 0)
          sum = 1.0/sqrt(sum); 
      }
      for (j=ptr[i]; j<ptr[i+1]; j++)
        val[j] *= sum;
    }
  }

  if (what&GK_CSR_COL && mat->colval) {
    n   = mat->ncols;
    ptr = mat->colptr;
    val = mat->colval;

    #pragma omp parallel for if (ptr[n] > OMPMINOPS) private(j,sum) schedule(static)
    for (i=0; i<n; i++) {
      sum = 0.0;
      if (norm == 1) {
        for (j=ptr[i]; j<ptr[i+1]; j++) 
          sum += val[j]; /* assume val[j] > 0 */ 
        if (sum > 0)
          sum = 1.0/sum;
      }
      else if (norm == 2) {
        for (j=ptr[i]; j<ptr[i+1]; j++) 
          sum += val[j]*val[j];
        if (sum > 0)
          sum = 1.0/sqrt(sum); 
      }
      for (j=ptr[i]; j<ptr[i+1]; j++)
        val[j] *= sum;
    }
  }

}


/*************************************************************************/
/*! Applies different row scaling methods.
    \param mat the matrix itself,
    \param type indicates the type of row scaling. Possible values are:
           GK_CSR_MAXTF, GK_CSR_SQRT, GK_CSR_LOG, GK_CSR_IDF, GK_CSR_MAXTF2.
*/
/**************************************************************************/
void gk_csr_Scale(gk_csr_t *mat, int type)
{
  ssize_t i, j;
  int nrows, ncols, nnzcols, bgfreq;
  ssize_t *rowptr;
  int *rowind, *collen;
  float *rowval, *cscale, maxtf;
  double logscale = 1.0/log(2.0);

  nrows  = mat->nrows;
  rowptr = mat->rowptr;
  rowind = mat->rowind;
  rowval = mat->rowval;

  switch (type) {
    case GK_CSR_MAXTF: /* TF' = .5 + .5*TF/MAX(TF) */
      #pragma omp parallel for if (rowptr[nrows] > OMPMINOPS) private(j, maxtf) schedule(static)
      for (i=0; i<nrows; i++) {
        maxtf = fabs(rowval[rowptr[i]]);
        for (j=rowptr[i]; j<rowptr[i+1]; j++) 
          maxtf = (maxtf < fabs(rowval[j]) ? fabs(rowval[j]) : maxtf);
  
        for (j=rowptr[i]; j<rowptr[i+1]; j++)
          rowval[j] = .5 + .5*rowval[j]/maxtf;
      }
      break;

    case GK_CSR_MAXTF2: /* TF' = .1 + .9*TF/MAX(TF) */
      #pragma omp parallel for if (rowptr[nrows] > OMPMINOPS) private(j, maxtf) schedule(static)
      for (i=0; i<nrows; i++) {
        maxtf = fabs(rowval[rowptr[i]]);
        for (j=rowptr[i]; j<rowptr[i+1]; j++) 
          maxtf = (maxtf < fabs(rowval[j]) ? fabs(rowval[j]) : maxtf);
  
        for (j=rowptr[i]; j<rowptr[i+1]; j++)
          rowval[j] = .1 + .9*rowval[j]/maxtf;
      }
      break;

    case GK_CSR_SQRT: /* TF' = .1+SQRT(TF) */
      #pragma omp parallel for if (rowptr[nrows] > OMPMINOPS) private(j) schedule(static)
      for (i=0; i<nrows; i++) {
        for (j=rowptr[i]; j<rowptr[i+1]; j++) { 
          if (rowval[j] != 0.0)
            rowval[j] = .1+sign(rowval[j], sqrt(fabs(rowval[j])));
        }
      }
      
      break;

    case GK_CSR_POW25: /* TF' = .1+POW(TF,.25) */
      #pragma omp parallel for if (rowptr[nrows] > OMPMINOPS) private(j) schedule(static)
      for (i=0; i<nrows; i++) {
        for (j=rowptr[i]; j<rowptr[i+1]; j++) { 
          if (rowval[j] != 0.0)
            rowval[j] = .1+sign(rowval[j], sqrt(sqrt(fabs(rowval[j]))));
        }
      }
      break;

    case GK_CSR_POW65: /* TF' = .1+POW(TF,.65) */
      #pragma omp parallel for if (rowptr[nrows] > OMPMINOPS) private(j) schedule(static)
      for (i=0; i<nrows; i++) {
        for (j=rowptr[i]; j<rowptr[i+1]; j++) { 
          if (rowval[j] != 0.0)
            rowval[j] = .1+sign(rowval[j], powf(fabs(rowval[j]), .65));
        }
      }
      break;

    case GK_CSR_POW75: /* TF' = .1+POW(TF,.75) */
      #pragma omp parallel for if (rowptr[nrows] > OMPMINOPS) private(j) schedule(static)
      for (i=0; i<nrows; i++) {
        for (j=rowptr[i]; j<rowptr[i+1]; j++) { 
          if (rowval[j] != 0.0)
            rowval[j] = .1+sign(rowval[j], powf(fabs(rowval[j]), .75));
        }
      }
      break;

    case GK_CSR_POW85: /* TF' = .1+POW(TF,.85) */
      #pragma omp parallel for if (rowptr[nrows] > OMPMINOPS) private(j) schedule(static)
      for (i=0; i<nrows; i++) {
        for (j=rowptr[i]; j<rowptr[i+1]; j++) { 
          if (rowval[j] != 0.0)
            rowval[j] = .1+sign(rowval[j], powf(fabs(rowval[j]), .85));
        }
      }
      break;

    case GK_CSR_LOG: /* TF' = 1+log_2(TF) */
      #pragma omp parallel for if (rowptr[nrows] > OMPMINOPS) schedule(static,32)
      for (i=0; i<rowptr[nrows]; i++) {
        if (rowval[i] != 0.0)
          rowval[i] = 1+(rowval[i]>0.0 ? log(rowval[i]) : -log(-rowval[i]))*logscale;
      }
#ifdef XXX
      #pragma omp parallel for private(j) schedule(static)
      for (i=0; i<nrows; i++) {
        for (j=rowptr[i]; j<rowptr[i+1]; j++) { 
          if (rowval[j] != 0.0)
            rowval[j] = 1+(rowval[j]>0.0 ? log(rowval[j]) : -log(-rowval[j]))*logscale;
            //rowval[j] = 1+sign(rowval[j], log(fabs(rowval[j]))*logscale);
        }
      }
#endif
      break;

    case GK_CSR_IDF: /* TF' = TF*IDF */
      ncols  = mat->ncols;
      cscale = gk_fmalloc(ncols, "gk_csr_Scale: cscale");
      collen = gk_ismalloc(ncols, 0, "gk_csr_Scale: collen");

      for (i=0; i<nrows; i++) {
        for (j=rowptr[i]; j<rowptr[i+1]; j++)
          collen[rowind[j]]++;
      }

      #pragma omp parallel for if (ncols > OMPMINOPS) schedule(static)
      for (i=0; i<ncols; i++)
        cscale[i] = (collen[i] > 0 ? log(1.0*nrows/collen[i]) : 0.0);

      #pragma omp parallel for if (rowptr[nrows] > OMPMINOPS) private(j) schedule(static)
      for (i=0; i<nrows; i++) {
        for (j=rowptr[i]; j<rowptr[i+1]; j++)
          rowval[j] *= cscale[rowind[j]];
      }
      
      gk_free((void **)&cscale, &collen, LTERM);
      break;

    case GK_CSR_IDF2: /* TF' = TF*IDF */
      ncols  = mat->ncols;
      cscale = gk_fmalloc(ncols, "gk_csr_Scale: cscale");
      collen = gk_ismalloc(ncols, 0, "gk_csr_Scale: collen");

      for (i=0; i<nrows; i++) {
        for (j=rowptr[i]; j<rowptr[i+1]; j++)
          collen[rowind[j]]++;
      }

      nnzcols = 0;
      #pragma omp parallel for if (ncols > OMPMINOPS) schedule(static) reduction(+:nnzcols)
      for (i=0; i<ncols; i++)
        nnzcols += (collen[i] > 0 ? 1 : 0);

      bgfreq = gk_max(10, (ssize_t)(.5*rowptr[nrows]/nnzcols));
      printf("nnz: %zd, nnzcols: %d, bgfreq: %d\n", rowptr[nrows], nnzcols, bgfreq);

      #pragma omp parallel for if (ncols > OMPMINOPS) schedule(static)
      for (i=0; i<ncols; i++)
        cscale[i] = (collen[i] > 0 ? log(1.0*(nrows+2*bgfreq)/(bgfreq+collen[i])) : 0.0);

      #pragma omp parallel for if (rowptr[nrows] > OMPMINOPS) private(j) schedule(static)
      for (i=0; i<nrows; i++) {
        for (j=rowptr[i]; j<rowptr[i+1]; j++)
          rowval[j] *= cscale[rowind[j]];
      }

      gk_free((void **)&cscale, &collen, LTERM);
      break;

    default:
      gk_errexit(SIGERR, "Unknown scaling type of %d\n", type);
  }
}


/*************************************************************************/
/*! Computes the sums of the rows/columns
    \param mat the matrix itself,
    \param what is either GK_CSR_ROW or GK_CSR_COL indicating which 
           sums to compute.
*/
/**************************************************************************/
void gk_csr_ComputeSums(gk_csr_t *mat, int what)
{
  ssize_t i;
  int n;
  ssize_t *ptr;
  float *val, *sums;

  switch (what) {
    case GK_CSR_ROW:
      n   = mat->nrows;
      ptr = mat->rowptr;
      val = mat->rowval;

      if (mat->rsums) 
        gk_free((void **)&mat->rsums, LTERM);

      sums = mat->rsums = gk_fsmalloc(n, 0, "gk_csr_ComputeSums: sums");
      break;
    case GK_CSR_COL:
      n   = mat->ncols;
      ptr = mat->colptr;
      val = mat->colval;

      if (mat->csums) 
        gk_free((void **)&mat->csums, LTERM);

      sums = mat->csums = gk_fsmalloc(n, 0, "gk_csr_ComputeSums: sums");
      break;
    default:
      gk_errexit(SIGERR, "Invalid sum type of %d.\n", what);
      return;
  }

  if (val) {
    #pragma omp parallel for if (ptr[n] > OMPMINOPS) schedule(static)
    for (i=0; i<n; i++) 
      sums[i] = gk_fsum(ptr[i+1]-ptr[i], val+ptr[i], 1);
  }
  else {
    #pragma omp parallel for if (ptr[n] > OMPMINOPS) schedule(static)
    for (i=0; i<n; i++) 
      sums[i] = ptr[i+1]-ptr[i];
  }
}


/*************************************************************************/
/*! Computes the norms of the rows/columns

    \param mat the matrix itself,
    \param what is either GK_CSR_ROW or GK_CSR_COL indicating which 
           squared norms to compute.

    \note If the rowval/colval arrays are NULL, the matrix is assumed
          to be binary and the norms are computed accordingly.
*/
/**************************************************************************/
void gk_csr_ComputeNorms(gk_csr_t *mat, int what)
{
  ssize_t i;
  int n;
  ssize_t *ptr;
  float *val, *norms;

  switch (what) {
    case GK_CSR_ROW:
      n   = mat->nrows;
      ptr = mat->rowptr;
      val = mat->rowval;

      if (mat->rnorms) gk_free((void **)&mat->rnorms, LTERM);

      norms = mat->rnorms = gk_fsmalloc(n, 0, "gk_csr_ComputeSums: norms");
      break;
    case GK_CSR_COL:
      n   = mat->ncols;
      ptr = mat->colptr;
      val = mat->colval;

      if (mat->cnorms) gk_free((void **)&mat->cnorms, LTERM);

      norms = mat->cnorms = gk_fsmalloc(n, 0, "gk_csr_ComputeSums: norms");
      break;
    default:
      gk_errexit(SIGERR, "Invalid norm type of %d.\n", what);
      return;
  }

  if (val) {
    #pragma omp parallel for if (ptr[n] > OMPMINOPS) schedule(static)
    for (i=0; i<n; i++) 
      norms[i] = sqrt(gk_fdot(ptr[i+1]-ptr[i], val+ptr[i], 1, val+ptr[i], 1));
  }
  else {
    #pragma omp parallel for if (ptr[n] > OMPMINOPS) schedule(static)
    for (i=0; i<n; i++) 
      norms[i] = sqrt(ptr[i+1]-ptr[i]);
  }
}


/*************************************************************************/
/*! Computes the squared of the norms of the rows/columns

    \param mat the matrix itself,
    \param what is either GK_CSR_ROW or GK_CSR_COL indicating which 
           squared norms to compute.

    \note If the rowval/colval arrays are NULL, the matrix is assumed
          to be binary and the norms are computed accordingly.
*/
/**************************************************************************/
void gk_csr_ComputeSquaredNorms(gk_csr_t *mat, int what)
{
  ssize_t i;
  int n;
  ssize_t *ptr;
  float *val, *norms;

  switch (what) {
    case GK_CSR_ROW:
      n   = mat->nrows;
      ptr = mat->rowptr;
      val = mat->rowval;

      if (mat->rnorms) gk_free((void **)&mat->rnorms, LTERM);

      norms = mat->rnorms = gk_fsmalloc(n, 0, "gk_csr_ComputeSums: norms");
      break;
    case GK_CSR_COL:
      n   = mat->ncols;
      ptr = mat->colptr;
      val = mat->colval;

      if (mat->cnorms) gk_free((void **)&mat->cnorms, LTERM);

      norms = mat->cnorms = gk_fsmalloc(n, 0, "gk_csr_ComputeSums: norms");
      break;
    default:
      gk_errexit(SIGERR, "Invalid norm type of %d.\n", what);
      return;
  }

  if (val) {
    #pragma omp parallel for if (ptr[n] > OMPMINOPS) schedule(static)
    for (i=0; i<n; i++) 
      norms[i] = gk_fdot(ptr[i+1]-ptr[i], val+ptr[i], 1, val+ptr[i], 1);
  }
  else {
    #pragma omp parallel for if (ptr[n] > OMPMINOPS) schedule(static)
    for (i=0; i<n; i++) 
      norms[i] = ptr[i+1]-ptr[i];
  }
}


/*************************************************************************/
/*! Returns a new matrix whose rows/columns are shuffled.
   
    \param mat the matrix to be shuffled,
    \param what indicates if the rows (GK_CSR_ROW), columns (GK_CSR_COL),
           or both (GK_CSR_ROWCOL) will be shuffled,
    \param symmetric indicates if the same shuffling will be applied to 
           both rows and columns. This is valid with nrows==ncols and 
           GK_CSR_ROWCOL was specified.
    \returns the shuffled matrix. 
          The input matrix is not modified. 
*/
/**************************************************************************/
gk_csr_t *gk_csr_Shuffle(gk_csr_t *mat, int what, int symmetric)
{
  ssize_t i, j;
  int nrows, ncols;
  ssize_t *rowptr, *nrowptr;
  int *rowind, *nrowind;
  int *rperm, *cperm;
  float *rowval, *nrowval;
  gk_csr_t *nmat;

  if (what == GK_CSR_ROWCOL && symmetric && mat->nrows != mat->ncols)
    gk_errexit(SIGERR, "The matrix is not square for a symmetric rowcol shuffling.\n");

  nrows  = mat->nrows;
  ncols  = mat->ncols;
  rowptr = mat->rowptr;
  rowind = mat->rowind;
  rowval = mat->rowval;

  rperm = gk_imalloc(nrows, "gk_csr_Shuffle: rperm");
  cperm = gk_imalloc(ncols, "gk_csr_Shuffle: cperm");

  switch (what) {
    case GK_CSR_ROW:
      gk_RandomPermute(nrows, rperm, 1);
      for (i=0; i<20; i++)
        gk_RandomPermute(nrows, rperm, 0);

      for (i=0; i<ncols; i++)
        cperm[i] = i;
      break;

    case GK_CSR_COL:
      gk_RandomPermute(ncols, cperm, 1);
      for (i=0; i<20; i++)
        gk_RandomPermute(ncols, cperm, 0);

      for (i=0; i<nrows; i++)
        rperm[i] = i;
      break;

    case GK_CSR_ROWCOL:
      gk_RandomPermute(nrows, rperm, 1);
      for (i=0; i<20; i++)
        gk_RandomPermute(nrows, rperm, 0);

      if (symmetric)
        gk_icopy(nrows, rperm, cperm);
      else {
        gk_RandomPermute(ncols, cperm, 1);
        for (i=0; i<20; i++)
          gk_RandomPermute(ncols, cperm, 0);
      }
      break;

    default:
      gk_free((void **)&rperm, &cperm, LTERM);
      gk_errexit(SIGERR, "Unknown shuffling type of %d\n", what);
      return NULL;
  }

  nmat = gk_csr_Create();
  nmat->nrows = nrows;
  nmat->ncols = ncols;

  nrowptr = nmat->rowptr = gk_zmalloc(nrows+1, "gk_csr_Shuffle: nrowptr");
  nrowind = nmat->rowind = gk_imalloc(rowptr[nrows], "gk_csr_Shuffle: nrowind");
  nrowval = nmat->rowval = (rowval ? gk_fmalloc(rowptr[nrows], "gk_csr_Shuffle: nrowval") : NULL) ;

  for (i=0; i<nrows; i++)
    nrowptr[rperm[i]] = rowptr[i+1]-rowptr[i];
  MAKECSR(i, nrows, nrowptr);

  for (i=0; i<nrows; i++) {
    for (j=rowptr[i]; j<rowptr[i+1]; j++) {
      nrowind[nrowptr[rperm[i]]] = cperm[rowind[j]];
      if (nrowval)
        nrowval[nrowptr[rperm[i]]] = rowval[j];
      nrowptr[rperm[i]]++;
    }
  }
  SHIFTCSR(i, nrows, nrowptr);

  gk_free((void **)&rperm, &cperm, LTERM);

  return nmat;

}


/*************************************************************************/
/*! Returns the transpose of the matrix.
   
    \param mat the matrix to be transposed,
    \returns the transposed matrix. 
          The input matrix is not modified. 
*/
/**************************************************************************/
gk_csr_t *gk_csr_Transpose(gk_csr_t *mat)
{
  int32_t i, column;
  ssize_t j, nnz, rownnz, colnnz;
  int allocation_failed=0, saved_errno=0;
  size_t nrows;
  gk_csr_t *nmat=NULL;

  if (!gk_csr_ValidateStructure(mat, &rownnz, &colnnz)) {
    saved_errno = errno;
    goto failure;
  }
  if (mat->rowptr == NULL) {
    saved_errno = EINVAL;
    goto failure;
  }
  nnz = rownnz;
  nrows = (size_t)mat->ncols;
  nmat = gk_csr_CreateNoSignal(&allocation_failed);
  if (nmat == NULL)
    goto allocation_failure;
  nmat->nrows = mat->ncols;
  nmat->ncols = mat->nrows;
  nmat->rowptr = (ssize_t *)gk_csr_MallocNoSignal(
      (nrows+1)*sizeof(ssize_t), &allocation_failed);
  nmat->rowind = (int32_t *)gk_csr_MallocNoSignal(
      (size_t)nnz*sizeof(int32_t), &allocation_failed);
  if (mat->rowval)
    nmat->rowval = (float *)gk_csr_MallocNoSignal(
        (size_t)nnz*sizeof(float), &allocation_failed);
  if (allocation_failed)
    goto allocation_failure;
  memset(nmat->rowptr, 0, (nrows+1)*sizeof(ssize_t));

  for (j=0; j<nnz; j++)
    nmat->rowptr[mat->rowind[j]]++;
  MAKECSR(i, mat->ncols, nmat->rowptr);
  for (i=0; i<mat->nrows; i++) {
    for (j=mat->rowptr[i]; j<mat->rowptr[i+1]; j++) {
      column = mat->rowind[j];
      nmat->rowind[nmat->rowptr[column]] = i;
      if (mat->rowval)
        nmat->rowval[nmat->rowptr[column]] = mat->rowval[j];
      nmat->rowptr[column]++;
    }
  }
  SHIFTCSR(i, mat->ncols, nmat->rowptr);

  return nmat;

allocation_failure:
  saved_errno = errno != 0 ? errno : ENOMEM;
  allocation_failed = 1;

failure:
  return gk_csr_TransformError(&nmat, saved_errno,
      allocation_failed, "transpose");

}


/*************************************************************************/
/*! Computes the similarity between two rows/columns

    \param mat the matrix itself. The routine assumes that the indices
           are sorted in increasing order.
    \param i1 is the first row/column,
    \param i2 is the second row/column,
    \param what is either GK_CSR_ROW or GK_CSR_COL indicating the type of
           objects between the similarity will be computed,
    \param simtype is the type of similarity and is one of GK_CSR_COS,
           GK_CSR_JAC, GK_CSR_MIN, GK_CSR_AMIN
    \returns the similarity between the two rows/columns.
*/
/**************************************************************************/
float gk_csr_ComputeSimilarity(gk_csr_t *mat, int i1, int i2, int what, 
          int simtype)
{
  int nind1, nind2;
  int *ind1, *ind2;
  float *val1, *val2, stat1, stat2, sim;

  switch (what) {
    case GK_CSR_ROW:
      if (!mat->rowptr)
        gk_errexit(SIGERR, "Row-based view of the matrix does not exists.\n");
      nind1 = mat->rowptr[i1+1]-mat->rowptr[i1];
      nind2 = mat->rowptr[i2+1]-mat->rowptr[i2];
      ind1  = mat->rowind + mat->rowptr[i1];
      ind2  = mat->rowind + mat->rowptr[i2];
      val1  = mat->rowval + mat->rowptr[i1];
      val2  = mat->rowval + mat->rowptr[i2];
      break;

    case GK_CSR_COL:
      if (!mat->colptr)
        gk_errexit(SIGERR, "Column-based view of the matrix does not exists.\n");
      nind1 = mat->colptr[i1+1]-mat->colptr[i1];
      nind2 = mat->colptr[i2+1]-mat->colptr[i2];
      ind1  = mat->colind + mat->colptr[i1];
      ind2  = mat->colind + mat->colptr[i2];
      val1  = mat->colval + mat->colptr[i1];
      val2  = mat->colval + mat->colptr[i2];
      break;

    default:
      gk_errexit(SIGERR, "Invalid index type of %d.\n", what);
      return 0.0;
  }


  switch (simtype) {
    case GK_CSR_COS:
    case GK_CSR_JAC:
      sim = stat1 = stat2 = 0.0;
      i1 = i2 = 0;
      while (i1<nind1 && i2<nind2) {
        if (i1 == nind1) {
          stat2 += val2[i2]*val2[i2];
          i2++;
        }
        else if (i2 == nind2) {
          stat1 += val1[i1]*val1[i1];
          i1++;
        }
        else if (ind1[i1] < ind2[i2]) {
          stat1 += val1[i1]*val1[i1];
          i1++;
        }
        else if (ind1[i1] > ind2[i2]) {
          stat2 += val2[i2]*val2[i2];
          i2++;
        }
        else {
          sim   += val1[i1]*val2[i2];
          stat1 += val1[i1]*val1[i1];
          stat2 += val2[i2]*val2[i2];
          i1++;
          i2++;
        }
      }
      if (simtype == GK_CSR_COS)
        sim = (stat1*stat2 > 0.0 ? sim/sqrt(stat1*stat2) : 0.0);
      else 
        sim = (stat1+stat2-sim > 0.0 ? sim/(stat1+stat2-sim) : 0.0);
      break;

    case GK_CSR_MIN:
      sim = stat1 = stat2 = 0.0;
      i1 = i2 = 0;
      while (i1<nind1 && i2<nind2) {
        if (i1 == nind1) {
          stat2 += val2[i2];
          i2++;
        }
        else if (i2 == nind2) {
          stat1 += val1[i1];
          i1++;
        }
        else if (ind1[i1] < ind2[i2]) {
          stat1 += val1[i1];
          i1++;
        }
        else if (ind1[i1] > ind2[i2]) {
          stat2 += val2[i2];
          i2++;
        }
        else {
          sim   += gk_min(val1[i1],val2[i2]);
          stat1 += val1[i1];
          stat2 += val2[i2];
          i1++;
          i2++;
        }
      }
      sim = (stat1+stat2-sim > 0.0 ? sim/(stat1+stat2-sim) : 0.0);

      break;

    case GK_CSR_AMIN:
      sim = stat1 = stat2 = 0.0;
      i1 = i2 = 0;
      while (i1<nind1 && i2<nind2) {
        if (i1 == nind1) {
          stat2 += val2[i2];
          i2++;
        }
        else if (i2 == nind2) {
          stat1 += val1[i1];
          i1++;
        }
        else if (ind1[i1] < ind2[i2]) {
          stat1 += val1[i1];
          i1++;
        }
        else if (ind1[i1] > ind2[i2]) {
          stat2 += val2[i2];
          i2++;
        }
        else {
          sim   += gk_min(val1[i1],val2[i2]);
          stat1 += val1[i1];
          stat2 += val2[i2];
          i1++;
          i2++;
        }
      }
      sim = (stat1 > 0.0 ? sim/stat1 : 0.0);

      break;

    default:
      gk_errexit(SIGERR, "Unknown similarity measure %d\n", simtype);
      return -1;
  }

  return sim;

}


/*************************************************************************/
/*! Computes the similarity between two rows/columns

    \param mat_a the first matrix. The routine assumes that the indices
           are sorted in increasing order.
    \param mat_b the second matrix. The routine assumes that the indices
           are sorted in increasing order.
    \param i1 is the row/column from the first matrix (mat_a),
    \param i2 is the row/column from the second matrix (mat_b),
    \param what is either GK_CSR_ROW or GK_CSR_COL indicating the type of
           objects between the similarity will be computed,
    \param simtype is the type of similarity and is one of GK_CSR_COS,
           GK_CSR_JAC, GK_CSR_MIN, GK_CSR_AMIN
    \returns the similarity between the two rows/columns.
*/
/**************************************************************************/
float gk_csr_ComputePairSimilarity(gk_csr_t *mat_a, gk_csr_t *mat_b, 
          int i1, int i2, int what, int simtype)
{
  int nind1, nind2;
  int *ind1, *ind2;
  float *val1, *val2, stat1, stat2, sim;

  switch (what) {
    case GK_CSR_ROW:
      if (!mat_a->rowptr || !mat_b->rowptr)
        gk_errexit(SIGERR, "Row-based view of the matrix does not exists.\n");
      nind1 = mat_a->rowptr[i1+1]-mat_a->rowptr[i1];
      nind2 = mat_b->rowptr[i2+1]-mat_b->rowptr[i2];
      ind1  = mat_a->rowind + mat_a->rowptr[i1];
      ind2  = mat_b->rowind + mat_b->rowptr[i2];
      val1  = mat_a->rowval + mat_a->rowptr[i1];
      val2  = mat_b->rowval + mat_b->rowptr[i2];
      break;

    case GK_CSR_COL:
      if (!mat_a->colptr || !mat_b->colptr)
        gk_errexit(SIGERR, "Column-based view of the matrix does not exists.\n");
      nind1 = mat_a->colptr[i1+1]-mat_a->colptr[i1];
      nind2 = mat_b->colptr[i2+1]-mat_b->colptr[i2];
      ind1  = mat_a->colind + mat_a->colptr[i1];
      ind2  = mat_b->colind + mat_b->colptr[i2];
      val1  = mat_a->colval + mat_a->colptr[i1];
      val2  = mat_b->colval + mat_b->colptr[i2];
      break;

    default:
      gk_errexit(SIGERR, "Invalid index type of %d.\n", what);
      return 0.0;
  }


  switch (simtype) {
    case GK_CSR_COS:
    case GK_CSR_JAC:
      sim = stat1 = stat2 = 0.0;
      i1 = i2 = 0;
      while (i1<nind1 && i2<nind2) {
        if (i1 == nind1) {
          stat2 += val2[i2]*val2[i2];
          i2++;
        }
        else if (i2 == nind2) {
          stat1 += val1[i1]*val1[i1];
          i1++;
        }
        else if (ind1[i1] < ind2[i2]) {
          stat1 += val1[i1]*val1[i1];
          i1++;
        }
        else if (ind1[i1] > ind2[i2]) {
          stat2 += val2[i2]*val2[i2];
          i2++;
        }
        else {
          sim   += val1[i1]*val2[i2];
          stat1 += val1[i1]*val1[i1];
          stat2 += val2[i2]*val2[i2];
          i1++;
          i2++;
        }
      }
      if (simtype == GK_CSR_COS)
        sim = (stat1*stat2 > 0.0 ? sim/sqrt(stat1*stat2) : 0.0);
      else 
        sim = (stat1+stat2-sim > 0.0 ? sim/(stat1+stat2-sim) : 0.0);
      break;

    case GK_CSR_MIN:
      sim = stat1 = stat2 = 0.0;
      i1 = i2 = 0;
      while (i1<nind1 && i2<nind2) {
        if (i1 == nind1) {
          stat2 += val2[i2];
          i2++;
        }
        else if (i2 == nind2) {
          stat1 += val1[i1];
          i1++;
        }
        else if (ind1[i1] < ind2[i2]) {
          stat1 += val1[i1];
          i1++;
        }
        else if (ind1[i1] > ind2[i2]) {
          stat2 += val2[i2];
          i2++;
        }
        else {
          sim   += gk_min(val1[i1],val2[i2]);
          stat1 += val1[i1];
          stat2 += val2[i2];
          i1++;
          i2++;
        }
      }
      sim = (stat1+stat2-sim > 0.0 ? sim/(stat1+stat2-sim) : 0.0);

      break;

    case GK_CSR_AMIN:
      sim = stat1 = stat2 = 0.0;
      i1 = i2 = 0;
      while (i1<nind1 && i2<nind2) {
        if (i1 == nind1) {
          stat2 += val2[i2];
          i2++;
        }
        else if (i2 == nind2) {
          stat1 += val1[i1];
          i1++;
        }
        else if (ind1[i1] < ind2[i2]) {
          stat1 += val1[i1];
          i1++;
        }
        else if (ind1[i1] > ind2[i2]) {
          stat2 += val2[i2];
          i2++;
        }
        else {
          sim   += gk_min(val1[i1],val2[i2]);
          stat1 += val1[i1];
          stat2 += val2[i2];
          i1++;
          i2++;
        }
      }
      sim = (stat1 > 0.0 ? sim/stat1 : 0.0);

      break;

    default:
      gk_errexit(SIGERR, "Unknown similarity measure %d\n", simtype);
      return -1;
  }

  return sim;

}

/*************************************************************************/
/*! Finds the n most similar rows (neighbors) to the query.

    \param mat the matrix itself
    \param nqterms is the number of columns in the query
    \param qind is the list of query columns
    \param qval is the list of correspodning query weights
    \param simtype is the type of similarity and is one of GK_CSR_DOTP,
           GK_CSR_COS, GK_CSR_JAC, GK_CSR_MIN, GK_CSR_AMIN. In case of 
           GK_CSR_COS, the rows and the query are assumed to be of unit 
           length.
    \param nsim is the maximum number of requested most similar rows.
           If -1 is provided, then everything is returned unsorted.
    \param minsim is the minimum similarity of the requested most 
           similar rows
    \param hits is the result set. This array should be at least
           of length nsim.
    \param i_marker is an array of size equal to the number of rows
           whose values are initialized to -1. If NULL is provided
           then this array is allocated and freed internally.
    \param i_cand is an array of size equal to the number of rows.
           If NULL is provided then this array is allocated and freed 
           internally.
    \returns The number of identified most similar rows, which can be
             smaller than the requested number of nnbrs in those cases
             in which there are no sufficiently many neighbors.
*/
/**************************************************************************/
int gk_csr_GetSimilarRows(gk_csr_t *mat, int nqterms, int *qind, 
        float *qval, int simtype, int nsim, float minsim, gk_fkv_t *hits, 
        int *i_marker, gk_fkv_t *i_cand)
{
  ssize_t i, ii, j, k;
  int nrows, ncols, ncand;
  ssize_t *colptr;
  int *colind, *marker;
  float *colval, *rnorms, mynorm, *rsums, mysum;
  gk_fkv_t *cand;

  if (nqterms == 0)
    return 0;

  nrows  = mat->nrows;
  ncols  = mat->ncols;
  GKASSERT((colptr = mat->colptr) != NULL);
  GKASSERT((colind = mat->colind) != NULL);
  GKASSERT((colval = mat->colval) != NULL);

  marker = (i_marker ? i_marker : gk_ismalloc(nrows, -1, "gk_csr_SimilarRows: marker"));
  cand   = (i_cand   ? i_cand   : gk_fkvmalloc(nrows, "gk_csr_SimilarRows: cand"));

  switch (simtype) {
    case GK_CSR_DOTP:
    case GK_CSR_COS:
      for (ncand=0, ii=0; ii<nqterms; ii++) {
        i = qind[ii];
        if (i < ncols) {
          for (j=colptr[i]; j<colptr[i+1]; j++) {
            k = colind[j];
            if (marker[k] == -1) {
              cand[ncand].val = k;
              cand[ncand].key = 0;
              marker[k]       = ncand++;
            }
            cand[marker[k]].key += colval[j]*qval[ii];
          }
        }
      }
      break;

    case GK_CSR_JAC:
      for (ncand=0, ii=0; ii<nqterms; ii++) {
        i = qind[ii];
        if (i < ncols) {
          for (j=colptr[i]; j<colptr[i+1]; j++) {
            k = colind[j];
            if (marker[k] == -1) {
              cand[ncand].val = k;
              cand[ncand].key = 0;
              marker[k]       = ncand++;
            }
            cand[marker[k]].key += colval[j]*qval[ii];
          }
        }
      }

      GKASSERT((rnorms = mat->rnorms) != NULL);
      mynorm = gk_fdot(nqterms, qval, 1, qval, 1);

      for (i=0; i<ncand; i++)
        cand[i].key = cand[i].key/(rnorms[cand[i].val]+mynorm-cand[i].key);
      break;

    case GK_CSR_MIN:
      for (ncand=0, ii=0; ii<nqterms; ii++) {
        i = qind[ii];
        if (i < ncols) {
          for (j=colptr[i]; j<colptr[i+1]; j++) {
            k = colind[j];
            if (marker[k] == -1) {
              cand[ncand].val = k;
              cand[ncand].key = 0;
              marker[k]       = ncand++;
            }
            cand[marker[k]].key += gk_min(colval[j], qval[ii]);
          }
        }
      }

      GKASSERT((rsums = mat->rsums) != NULL);
      mysum = gk_fsum(nqterms, qval, 1);

      for (i=0; i<ncand; i++)
        cand[i].key = cand[i].key/(rsums[cand[i].val]+mysum-cand[i].key);
      break;

    /* Assymetric MIN  similarity */
    case GK_CSR_AMIN:
      for (ncand=0, ii=0; ii<nqterms; ii++) {
        i = qind[ii];
        if (i < ncols) {
          for (j=colptr[i]; j<colptr[i+1]; j++) {
            k = colind[j];
            if (marker[k] == -1) {
              cand[ncand].val = k;
              cand[ncand].key = 0;
              marker[k]       = ncand++;
            }
            cand[marker[k]].key += gk_min(colval[j], qval[ii]);
          }
        }
      }

      mysum = gk_fsum(nqterms, qval, 1);

      for (i=0; i<ncand; i++)
        cand[i].key = cand[i].key/mysum;
      break;

    default:
      gk_errexit(SIGERR, "Unknown similarity measure %d\n", simtype);
      return -1;
  }

  /* go and prune the hits that are bellow minsim */
  for (j=0, i=0; i<ncand; i++) {
    marker[cand[i].val] = -1;
    if (cand[i].key >= minsim) 
      cand[j++] = cand[i];
  }
  ncand = j;

  if (nsim == -1 || nsim >= ncand) {
    nsim = ncand;
  }
  else {
    nsim = gk_min(nsim, ncand);
    gk_dfkvkselect(ncand, nsim, cand);
    gk_fkvsortd(nsim, cand);
  }

  gk_fkvcopy(nsim, cand, hits);

  if (i_marker == NULL)
    gk_free((void **)&marker, LTERM);
  if (i_cand == NULL)
    gk_free((void **)&cand, LTERM);

  return nsim;
}


/*************************************************************************/
/*! Returns a symmetric version of a square matrix. The symmetric version
    is constructed by applying an A op A^T operation, where op is one of
    GK_CSR_SYM_SUM, GK_CSR_SYM_MIN, GK_CSR_SYM_MAX, GK_CSR_SYM_AVG.
   
    \param mat the matrix to be symmetrized,
    \param op indicates the operation to be performed. The possible values are
           GK_CSR_SYM_SUM, GK_CSR_SYM_MIN, GK_CSR_SYM_MAX, and GK_CSR_SYM_AVG.

    \returns the symmetrized matrix consisting only of its row-based structure. 
          The input matrix is not modified. 
*/
/**************************************************************************/
gk_csr_t *gk_csr_MakeSymmetric(gk_csr_t *mat, int op)
{
  ssize_t i, j, nnz, nadj, nout, nedges, maxedges, position;
  ssize_t *marker=NULL, *last=NULL, *next=NULL;
  int32_t nrows;
  int hasvals, allocation_failed=0, saved_errno=0;
  ssize_t *rowptr, *colptr=NULL, *nrowptr;
  int32_t *rowind, *colind=NULL, *nrowind, *ids=NULL;
  float *rowval=NULL, *colval=NULL, *nrowval=NULL, *wgts=NULL;
  gk_csr_t *nmat=NULL;

  if (mat == NULL || mat->nrows < 0 || mat->ncols < 0 ||
      mat->nrows != mat->ncols || mat->rowptr == NULL ||
      mat->rowptr[0] != 0 ||
      (op != GK_CSR_SYM_SUM && op != GK_CSR_SYM_MIN &&
       op != GK_CSR_SYM_MAX && op != GK_CSR_SYM_AVG)) {
    saved_errno = EINVAL;
    goto failure;
  }

  hasvals = (mat->rowval != NULL);

  nrows  = mat->nrows;
  rowptr = mat->rowptr;
  rowind = mat->rowind;
  if (hasvals)
    rowval = mat->rowval;
  for (i=0; i<nrows; i++) {
    if (rowptr[i] < 0 || rowptr[i] > rowptr[i+1]) {
      saved_errno = EINVAL;
      goto failure;
    }
  }
  nedges = rowptr[nrows];
  if (nedges < 0 || nedges > PTRDIFF_MAX/2 ||
      (size_t)nrows+1 > SIZE_MAX/sizeof(ssize_t) ||
      (size_t)nedges > SIZE_MAX/(2*sizeof(int32_t)) ||
      (size_t)nedges > SIZE_MAX/(2*sizeof(ssize_t)) ||
      (hasvals && (size_t)nedges > SIZE_MAX/(2*sizeof(float))) ||
      (size_t)nrows > SIZE_MAX/sizeof(ssize_t)) {
    saved_errno = nedges < 0 ? EINVAL : EOVERFLOW;
    goto failure;
  }
  if (nedges > 0 && rowind == NULL) {
    saved_errno = EINVAL;
    goto failure;
  }
  for (i=0; i<nedges; i++) {
    if (rowind[i] < 0 || rowind[i] >= nrows ||
        (hasvals && !isfinite(rowval[i]))) {
      saved_errno = EINVAL;
      goto failure;
    }
  }
  maxedges = 2*nedges;

  /* create the column view for efficient processing */
  colptr = (ssize_t *)gk_csr_MallocNoSignal(
      ((size_t)nrows+1)*sizeof(ssize_t), &allocation_failed);
  colind = (int32_t *)gk_csr_MallocNoSignal(
      (size_t)nedges*sizeof(int32_t), &allocation_failed);
  if (hasvals)
    colval = (float *)gk_csr_MallocNoSignal(
        (size_t)nedges*sizeof(float), &allocation_failed);
  if (colptr == NULL || colind == NULL || (hasvals && colval == NULL))
    goto allocation_failure;
  memset(colptr, 0, ((size_t)nrows+1)*sizeof(ssize_t));

  for (i=0; i<nrows; i++) {
    for (j=rowptr[i]; j<rowptr[i+1]; j++) 
      colptr[rowind[j]]++;
  }
  MAKECSR(i, nrows, colptr);

  for (i=0; i<nrows; i++) {
    for (j=rowptr[i]; j<rowptr[i+1]; j++) {
      colind[colptr[rowind[j]]] = i;
      if (hasvals)
        colval[colptr[rowind[j]]] = rowval[j];
      colptr[rowind[j]]++;
    }
  }
  SHIFTCSR(i, nrows, colptr);


  nmat = gk_csr_CreateNoSignal(&allocation_failed);
  if (nmat == NULL)
    goto allocation_failure;
  
  nmat->nrows = mat->nrows;
  nmat->ncols = mat->ncols;

  nrowptr = nmat->rowptr = (ssize_t *)gk_csr_MallocNoSignal(
      ((size_t)nrows+1)*sizeof(ssize_t), &allocation_failed);
  nrowind = nmat->rowind = (int32_t *)gk_csr_MallocNoSignal(
      (size_t)maxedges*sizeof(int32_t), &allocation_failed);
  if (hasvals)
    nrowval = nmat->rowval = (float *)gk_csr_MallocNoSignal(
        (size_t)maxedges*sizeof(float), &allocation_failed);

  marker = (ssize_t *)gk_csr_MallocNoSignal(
      (size_t)nrows*sizeof(ssize_t), &allocation_failed);
  last = (ssize_t *)gk_csr_MallocNoSignal(
      (size_t)nrows*sizeof(ssize_t), &allocation_failed);
  next = (ssize_t *)gk_csr_MallocNoSignal(
      (size_t)maxedges*sizeof(ssize_t), &allocation_failed);
  ids = (int32_t *)gk_csr_MallocNoSignal(
      (size_t)maxedges*sizeof(int32_t), &allocation_failed);
  if (hasvals)
    wgts = (float *)gk_csr_MallocNoSignal(
        (size_t)maxedges*sizeof(float), &allocation_failed);
  if (nrowptr == NULL || nrowind == NULL || marker == NULL || last == NULL ||
      next == NULL || ids == NULL ||
      (hasvals && (nrowval == NULL || wgts == NULL)))
    goto allocation_failure;
  for (i=0; i<nrows; i++) {
    marker[i] = -1;
    last[i] = -1;
  }

  nrowptr[0] = nnz = 0;
  for (i=0; i<nrows; i++) {
    nadj = 0;
    /* out-edges */
    for (j=rowptr[i]; j<rowptr[i+1]; j++) {
      ids[nadj] = rowind[j]; 
      if (hasvals)
        wgts[nadj] = rowval[j];
      if (marker[rowind[j]] == -1)
        marker[rowind[j]] = nadj;
      else
        next[last[rowind[j]]] = nadj;
      last[rowind[j]] = nadj;
      next[nadj++] = -1;
    }
    nout = nadj;

    /* in-edges */
    for (j=colptr[i]; j<colptr[i+1]; j++) {
      if (marker[colind[j]] == -1) {
        if (op != GK_CSR_SYM_MIN) {
          ids[nadj] = colind[j]; 
          if (hasvals) 
            wgts[nadj] = (op == GK_CSR_SYM_AVG ? 0.5*colval[j] : colval[j]);
          nadj++;
        }
      }
      else {
        position = marker[colind[j]];
        marker[colind[j]] = next[position];
        if (marker[colind[j]] == -1)
          last[colind[j]] = -1;
        next[position] = -2;
        if (hasvals) {
          switch (op) {
            case GK_CSR_SYM_MAX:
              wgts[position] = gk_max(colval[j], wgts[position]);
              break;
            case GK_CSR_SYM_MIN:
              wgts[position] = gk_min(colval[j], wgts[position]);
              break;
            case GK_CSR_SYM_SUM:
              wgts[position] += colval[j];
              if (!isfinite(wgts[position])) {
                saved_errno = EOVERFLOW;
                goto failure;
              }
              break;
            case GK_CSR_SYM_AVG:
              wgts[position] = (float)(0.5*((double)wgts[position] +
                  colval[j]));
              break;
          }
        }
      }
    }

    /* resolve any out-edges that were not found in the in-edges */
    for (j=0; j<nout; j++) {
      if (next[j] != -2) {
        if (op == GK_CSR_SYM_MIN)
          ids[j] = -1;
        else if (op == GK_CSR_SYM_AVG && hasvals)
          wgts[j] *= 0.5f;
      }
    }
    for (j=rowptr[i]; j<rowptr[i+1]; j++) {
      marker[rowind[j]] = -1;
      last[rowind[j]] = -1;
    }

    /* put the non '-1' entries in ids[] into i's row */
    for (j=0; j<nadj; j++) {
      if (ids[j] != -1) {
        nrowind[nnz] = ids[j];
        if (hasvals)
          nrowval[nnz] = wgts[j];
        nnz++;
      }
    }
    nrowptr[i+1] = nnz;
  }

  gk_free((void **)&colptr, &colind, &colval, &marker, &last, &next,
          &ids, &wgts, LTERM);

  return nmat;

allocation_failure:
  saved_errno = errno != 0 ? errno : ENOMEM;
  allocation_failed = 1;

failure:
  gk_free((void **)&colptr, &colind, &colval, &marker, &last, &next,
          &ids, &wgts, LTERM);
  gk_csr_Free(&nmat);
  errno = saved_errno != 0 ? saved_errno : EINVAL;
  gk_errexit(allocation_failed || saved_errno == ENOMEM ||
      saved_errno == EOVERFLOW ? SIGMEM : SIGERR,
      "Failed to make a symmetric matrix.\n");
  errno = saved_errno != 0 ? saved_errno : EINVAL;
  return NULL;
}


/*************************************************************************/
/*! This function finds the connected components in a graph stored in
    CSR format.

    \param mat is the graph structure in CSR format
    \param cptr is the ptr structure of the CSR representation of the 
           components. The length of this vector must be mat->nrows+1.
    \param cind is the indices structure of the CSR representation of 
           the components. The length of this vector must be mat->nrows.
    \param cids is an array that stores the component # of each vertex
           of the graph. The length of this vector must be mat->nrows.

    \returns the number of components that it found.

    \note The cptr, cind, and cids parameters can be NULL, in which case 
          only the number of connected components is returned.
*/
/*************************************************************************/
int gk_csr_FindConnectedComponents(gk_csr_t *mat, int32_t *cptr, int32_t *cind, 
        int32_t *cids)
{
  ssize_t i, j, k, rownnz, colnnz, nvtxs, first, last, ntodo, position;
  int32_t component, ncmps=0, start;
  int32_t *work_cptr=NULL, *work_cind=NULL, *work_cids=NULL;
  int32_t *pos=NULL, *todo=NULL;
  int allocation_failed=0, saved_errno=0;
  size_t nvertices;

  if ((cptr == NULL) != (cind == NULL)) {
    saved_errno = EINVAL;
    goto failure;
  }
  if (!gk_csr_ValidateStructure(mat, &rownnz, &colnnz)) {
    saved_errno = errno;
    goto failure;
  }
  if (mat->nrows != mat->ncols || mat->rowptr == NULL) {
    saved_errno = EINVAL;
    goto failure;
  }
  nvtxs = mat->nrows;
  nvertices = (size_t)nvtxs;
  work_cptr = (int32_t *)gk_csr_MallocNoSignal(
      (nvertices+1)*sizeof(int32_t), &allocation_failed);
  work_cind = (int32_t *)gk_csr_MallocNoSignal(
      nvertices*sizeof(int32_t), &allocation_failed);
  if (cids != NULL)
    work_cids = (int32_t *)gk_csr_MallocNoSignal(
        nvertices*sizeof(int32_t), &allocation_failed);
  pos = (int32_t *)gk_csr_MallocNoSignal(
      nvertices*sizeof(int32_t), &allocation_failed);
  todo = (int32_t *)gk_csr_MallocNoSignal(
      nvertices*sizeof(int32_t), &allocation_failed);
  if (allocation_failed)
    goto allocation_failure;
  for (i=0; i<nvtxs; i++)
    pos[i] = todo[i] = (int32_t)i;

  ntodo = nvtxs;
  first = last = 0;
  while (first < last || ntodo > 0) {
    if (first == last) {
      work_cptr[ncmps++] = (int32_t)first;
      start = todo[0];
      work_cind[last++] = start;
      pos[start] = -1;
      ntodo--;
      if (ntodo > 0) {
        todo[0] = todo[ntodo];
        pos[todo[0]] = 0;
      }
    }

    i = work_cind[first++];
    for (j=mat->rowptr[i]; j<mat->rowptr[i+1]; j++) {
      k = mat->rowind[j];
      if (pos[k] != -1) {
        work_cind[last++] = (int32_t)k;
        position = pos[k];
        pos[k] = -1;
        ntodo--;
        if (position < ntodo) {
          todo[position] = todo[ntodo];
          pos[todo[position]] = (int32_t)position;
        }
      }
    }
  }
  work_cptr[ncmps] = (int32_t)first;

  if (cids != NULL) {
    for (component=0; component<ncmps; component++) {
      for (j=work_cptr[component]; j<work_cptr[component+1]; j++)
        work_cids[work_cind[j]] = component;
    }
    memcpy(cids, work_cids, nvertices*sizeof(int32_t));
  }
  if (cptr != NULL) {
    memcpy(cptr, work_cptr, ((size_t)ncmps+1)*sizeof(int32_t));
    memcpy(cind, work_cind, nvertices*sizeof(int32_t));
  }
  gk_free((void **)&work_cptr, &work_cind, &work_cids, &pos, &todo,
          LTERM);
  return ncmps;

allocation_failure:
  saved_errno = errno != 0 ? errno : ENOMEM;
  allocation_failed = 1;

failure:
  gk_free((void **)&work_cptr, &work_cind, &work_cids, &pos, &todo,
          LTERM);
  errno = saved_errno != 0 ? saved_errno : EINVAL;
  gk_errexit(allocation_failed || saved_errno == ENOMEM ||
      saved_errno == EOVERFLOW ? SIGMEM : SIGERR,
      "Failed to find matrix components.\n");
  errno = saved_errno != 0 ? saved_errno : EINVAL;
  return -1;
}


/*************************************************************************/
/*! Returns a matrix that has been reordered according to the provided
    row/column permutation. The matrix is required to be square and the same
    permutation is applied to both rows and columns.

    \param[IN] mat is the matrix to be re-ordered.
    \param[IN] perm is the new ordering of the rows & columns
    \param[IN] iperm is the original ordering of the re-ordered matrix's rows & columns
    \returns the newly created reordered matrix.

    \note Either perm or iperm can be NULL but not both.
*/
/**************************************************************************/
gk_csr_t *gk_csr_ReorderSymmetric(gk_csr_t *mat, int32_t *perm, int32_t *iperm)
{
  ssize_t j, jj, rownnz, colnnz;
  int32_t i, u, v, nrows, *seen=NULL;
  int32_t *localperm=NULL, *localiperm=NULL;
  int allocation_failed=0, saved_errno=0;
  size_t nrows_size;
  gk_csr_t *nmat=NULL;

  if (perm == NULL && iperm == NULL) {
    saved_errno = EINVAL;
    goto failure;
  }
  if (!gk_csr_ValidateStructure(mat, &rownnz, &colnnz)) {
    saved_errno = errno;
    goto failure;
  }
  if (mat->nrows != mat->ncols || mat->rowptr == NULL) {
    saved_errno = EINVAL;
    goto failure;
  }
  nrows = mat->nrows;
  nrows_size = (size_t)nrows;
  seen = (int32_t *)gk_csr_MallocNoSignal(
      nrows_size*sizeof(int32_t), &allocation_failed);
  if (seen == NULL)
    goto allocation_failure;
  if ((perm != NULL &&
       !gk_csr_ValidatePermutation(perm, nrows, seen)) ||
      (iperm != NULL &&
       !gk_csr_ValidatePermutation(iperm, nrows, seen))) {
    saved_errno = errno;
    goto failure;
  }
  if (perm != NULL && iperm != NULL) {
    for (i=0; i<nrows; i++) {
      if (perm[iperm[i]] != i) {
        saved_errno = EINVAL;
        goto failure;
      }
    }
  }
  if (perm == NULL) {
    localperm = (int32_t *)gk_csr_MallocNoSignal(
        nrows_size*sizeof(int32_t), &allocation_failed);
    if (localperm == NULL)
      goto allocation_failure;
    for (i=0; i<nrows; i++)
      localperm[iperm[i]] = i;
    perm = localperm;
  }
  if (iperm == NULL) {
    localiperm = (int32_t *)gk_csr_MallocNoSignal(
        nrows_size*sizeof(int32_t), &allocation_failed);
    if (localiperm == NULL)
      goto allocation_failure;
    for (i=0; i<nrows; i++)
      localiperm[perm[i]] = i;
    iperm = localiperm;
  }

  nmat = gk_csr_CreateNoSignal(&allocation_failed);
  if (nmat == NULL)
    goto allocation_failure;
  nmat->nrows = nrows;
  nmat->ncols = nrows;
  nmat->rowptr = (ssize_t *)gk_csr_MallocNoSignal(
      (nrows_size+1)*sizeof(ssize_t), &allocation_failed);
  nmat->rowind = (int32_t *)gk_csr_MallocNoSignal(
      (size_t)rownnz*sizeof(int32_t), &allocation_failed);
  if (mat->rowval)
    nmat->rowval = (float *)gk_csr_MallocNoSignal(
        (size_t)rownnz*sizeof(float), &allocation_failed);
  if (mat->rowids)
    nmat->rowids = (int32_t *)gk_csr_MallocNoSignal(
        nrows_size*sizeof(int32_t), &allocation_failed);
  if (mat->rlabels)
    nmat->rlabels = (int32_t *)gk_csr_MallocNoSignal(
        nrows_size*sizeof(int32_t), &allocation_failed);
  if (mat->rmap)
    nmat->rmap = (int32_t *)gk_csr_MallocNoSignal(
        nrows_size*sizeof(int32_t), &allocation_failed);
  if (mat->rnorms)
    nmat->rnorms = (float *)gk_csr_MallocNoSignal(
        nrows_size*sizeof(float), &allocation_failed);
  if (mat->rsums)
    nmat->rsums = (float *)gk_csr_MallocNoSignal(
        nrows_size*sizeof(float), &allocation_failed);
  if (mat->rsizes)
    nmat->rsizes = (float *)gk_csr_MallocNoSignal(
        nrows_size*sizeof(float), &allocation_failed);
  if (mat->rvols)
    nmat->rvols = (float *)gk_csr_MallocNoSignal(
        nrows_size*sizeof(float), &allocation_failed);
  if (mat->rwgts)
    nmat->rwgts = (float *)gk_csr_MallocNoSignal(
        nrows_size*sizeof(float), &allocation_failed);
  if (mat->colids)
    nmat->colids = (int32_t *)gk_csr_MallocNoSignal(
        nrows_size*sizeof(int32_t), &allocation_failed);
  if (mat->clabels)
    nmat->clabels = (int32_t *)gk_csr_MallocNoSignal(
        nrows_size*sizeof(int32_t), &allocation_failed);
  if (mat->cmap)
    nmat->cmap = (int32_t *)gk_csr_MallocNoSignal(
        nrows_size*sizeof(int32_t), &allocation_failed);
  if (mat->cnorms)
    nmat->cnorms = (float *)gk_csr_MallocNoSignal(
        nrows_size*sizeof(float), &allocation_failed);
  if (mat->csums)
    nmat->csums = (float *)gk_csr_MallocNoSignal(
        nrows_size*sizeof(float), &allocation_failed);
  if (mat->csizes)
    nmat->csizes = (float *)gk_csr_MallocNoSignal(
        nrows_size*sizeof(float), &allocation_failed);
  if (mat->cvols)
    nmat->cvols = (float *)gk_csr_MallocNoSignal(
        nrows_size*sizeof(float), &allocation_failed);
  if (mat->cwgts)
    nmat->cwgts = (float *)gk_csr_MallocNoSignal(
        nrows_size*sizeof(float), &allocation_failed);
  if (allocation_failed)
    goto allocation_failure;

  nmat->rowptr[0] = jj = 0;
  for (v=0; v<nrows; v++) {
    u = iperm[v];
    for (j=mat->rowptr[u]; j<mat->rowptr[u+1]; j++, jj++) {
      nmat->rowind[jj] = perm[mat->rowind[j]];
      if (mat->rowval)
        nmat->rowval[jj] = mat->rowval[j];
    }
    if (mat->rowids)
      nmat->rowids[v] = mat->rowids[u];
    if (mat->rlabels)
      nmat->rlabels[v] = mat->rlabels[u];
    if (mat->rmap)
      nmat->rmap[v] = mat->rmap[u];
    if (mat->rnorms)
      nmat->rnorms[v] = mat->rnorms[u];
    if (mat->rsums)
      nmat->rsums[v] = mat->rsums[u];
    if (mat->rsizes)
      nmat->rsizes[v] = mat->rsizes[u];
    if (mat->rvols)
      nmat->rvols[v] = mat->rvols[u];
    if (mat->rwgts)
      nmat->rwgts[v] = mat->rwgts[u];
    if (mat->colids)
      nmat->colids[v] = mat->colids[u];
    if (mat->clabels)
      nmat->clabels[v] = mat->clabels[u];
    if (mat->cmap)
      nmat->cmap[v] = mat->cmap[u];
    if (mat->cnorms)
      nmat->cnorms[v] = mat->cnorms[u];
    if (mat->csums)
      nmat->csums[v] = mat->csums[u];
    if (mat->csizes)
      nmat->csizes[v] = mat->csizes[u];
    if (mat->cvols)
      nmat->cvols[v] = mat->cvols[u];
    if (mat->cwgts)
      nmat->cwgts[v] = mat->cwgts[u];
    nmat->rowptr[v+1] = jj;
  }

  gk_free((void **)&seen, &localperm, &localiperm, LTERM);
  return nmat;

allocation_failure:
  saved_errno = errno != 0 ? errno : ENOMEM;
  allocation_failed = 1;

failure:
  gk_free((void **)&seen, &localperm, &localiperm, LTERM);
  return gk_csr_TransformError(&nmat, saved_errno,
      allocation_failed, "reorder");
}


/*************************************************************************/
/*! This function computes a permutation of the rows/columns of a symmetric
    matrix based on a breadth-first-traversal. It can be used for re-ordering 
    the matrix to reduce its bandwidth for better cache locality.

    \param[IN]  mat is the matrix whose ordering to be computed.
    \param[IN]  maxdegree is the maximum number of nonzeros of the rows that
                will participate in the BFS ordering. Rows with more nonzeros
                will be put at the front of the ordering in decreasing degree
                order. 
    \param[IN]  v is the starting row of the BFS. A value of -1 indicates that
                a randomly selected row will be used.
    \param[OUT] perm[i] stores the ID of row i in the re-ordered matrix.
    \param[OUT] iperm[i] stores the ID of the row that corresponds to 
                the ith vertex in the re-ordered matrix.

    \note The perm or iperm (but not both) can be NULL, at which point, 
          the corresponding arrays are not returned. Though the program
          works fine when both are NULL, doing that is not smart.
          The returned arrays should be freed with gk_free().
*/
/*************************************************************************/
void gk_csr_ComputeBFSOrderingSymmetric(gk_csr_t *mat, int maxdegree, int v, 
          int32_t **r_perm, int32_t **r_iperm)
{
  int i, k, nrows, first, last;
  ssize_t j, *rowptr;
  int32_t *rowind, *cot, *pos;

  if (mat->nrows != mat->ncols) {
    fprintf(stderr, "gk_csr_ComputeBFSOrderingSymmetric: The matrix needs to be square.\n");
    return;
  }
  if (maxdegree < mat->nrows && v != -1) {
    fprintf(stderr, "gk_csr_ComputeBFSOrderingSymmetric: Since maxdegree node renumbering is requested the starting row should be -1.\n");
    return;
  }
  if (mat->nrows <= 0)
    return;

  nrows  = mat->nrows;
  rowptr = mat->rowptr;
  rowind = mat->rowind;

  /* This array will function like pos + touched of the CC method */
  pos = gk_i32incset(nrows, 0, gk_i32malloc(nrows, "gk_csr_ComputeBFSOrderingSymmetric: pos"));

  /* This array ([C]losed[O]pen[T]odo => cot) serves three purposes. 
     Positions from [0...first) is the current iperm[] vector of the explored rows; 
     Positions from [first...last) is the OPEN list (i.e., visited rows);
     Positions from [last...nrows) is the todo list. */
  cot = gk_i32incset(nrows, 0, gk_i32malloc(nrows, "gk_csr_ComputeBFSOrderingSymmetric: cot"));

  first = last = 0;

  /* deal with maxdegree handling */
  if (maxdegree < nrows) {
    last = nrows;
    for (i=nrows-1; i>=0; i--) {
      if (rowptr[i+1]-rowptr[i] < maxdegree) {
        cot[--last] = i;
        pos[i] = last;
      }
      else {
        cot[first++] = i;
        pos[i] = -1;
      }
    }
    GKASSERT(first == last);

    if (last > 0) { /* reorder them in degree decreasing order */
      gk_ikv_t *cand = gk_ikvmalloc(first, "gk_csr_ComputeBFSOrderingSymmetric: cand");

      for (i=0; i<first; i++) {
        k = cot[i];
        cand[i].key = (int)(rowptr[k+1]-rowptr[k]);
        cand[i].val = k;
      }

      gk_ikvsortd(first, cand);
      for (i=0; i<first; i++) 
        cot[i] = cand[i].val;

      gk_free((void **)&cand, LTERM);
    }

    v = cot[last + RandomInRange(nrows-last)];
  }


  /* swap v with the front of the todo list */
  cot[pos[v]] = cot[last];
  pos[cot[last]] = pos[v];

  cot[last] = v;
  pos[v] = last;


  /* start processing the nodes */
  while (first < nrows) {
    if (first == last) { /* find another starting row */
      k = cot[last];
      GKASSERT(pos[k] != -1);
      pos[k] = -1; /* mark node as being visited */
      last++;
    }

    i = cot[first++];  /* the ++ advances the explored rows */
    for (j=rowptr[i]; j<rowptr[i+1]; j++) {
      k = rowind[j];
      /* if a node has already been visited, its perm[] will be -1 */
      if (pos[k] != -1) {
        /* pos[k] is the location within iperm of where k resides (it is in the 'todo' part); 
           It is placed in that location cot[last] (end of OPEN list) that we 
           are about to overwrite and update pos[cot[last]] to reflect that. */
        cot[pos[k]]    = cot[last]; /* put the head of the todo list to 
                                       where k was in the todo list */
        pos[cot[last]] = pos[k];    /* update perm to reflect the move */

        cot[last++] = k;  /* put node at the end of the OPEN list */
        pos[k]      = -1; /* mark node as being visited */
      }
    }
  }

  /* time to decide what to return */
  if (r_perm != NULL) {
    /* use the 'pos' array to build the perm array */
    for (i=0; i<nrows; i++)
      pos[cot[i]] = i;

    *r_perm = pos;
    pos = NULL;
  }

  if (r_iperm != NULL) {
    *r_iperm = cot;
    cot = NULL;
  }

  /* cleanup memory */
  gk_free((void **)&pos, &cot, LTERM);

}


/*************************************************************************/
/*! This function computes a permutation of the rows of a symmetric matrix
    based on a best-first-traversal. It can be used for re-ordering the matrix
    to reduce its bandwidth for better cache locality.

    \param[IN]  mat is the matrix structure.
    \param[IN]  v is the starting row of the best-first traversal.
    \param[IN]  type indicates the criteria to use to measure the 'bestness'
                of a row.
    \param[OUT] perm[i] stores the ID of row i in the re-ordered matrix.
    \param[OUT] iperm[i] stores the ID of the row that corresponds to 
                the ith row in the re-ordered matrix.

    \note The perm or iperm (but not both) can be NULL, at which point, 
          the corresponding arrays are not returned. Though the program
          works fine when both are NULL, doing that is not smart.
          The returned arrays should be freed with gk_free().
*/
/*************************************************************************/
void gk_csr_ComputeBestFOrderingSymmetric(gk_csr_t *mat, int v, int type, 
          int32_t **r_perm, int32_t **r_iperm)
{
  ssize_t j, jj, *rowptr;
  int i, k, u, nrows, nopen, ntodo;
  int32_t *rowind, *perm, *degrees, *wdegrees, *sod, *level, *ot, *pos;
  gk_i32pq_t *queue;

  if (mat->nrows != mat->ncols) {
    fprintf(stderr, "gk_csr_ComputeBestFOrderingSymmetric: The matrix needs to be square.\n");
    return;
  }
  if (mat->nrows <= 0)
    return;

  nrows  = mat->nrows;
  rowptr = mat->rowptr;
  rowind = mat->rowind;


  /* the degree of the vertices in the closed list */
  degrees = gk_i32smalloc(nrows, 0, "gk_csr_ComputeBestFOrderingSymmetric: degrees");

  /* the weighted degree of the vertices in the closed list for type==3 */
  wdegrees = gk_i32smalloc(nrows, 0, "gk_csr_ComputeBestFOrderingSymmetric: wdegrees");

  /* the sum of differences for type==4 */
  sod = gk_i32smalloc(nrows, 0, "gk_csr_ComputeBestFOrderingSymmetric: sod");

  /* the encountering level of a vertex type==5 */
  level = gk_i32smalloc(nrows, 0, "gk_csr_ComputeBestFOrderingSymmetric: level");

  /* The open+todo list of vertices. 
     The vertices from [0..nopen] are the open vertices.
     The vertices from [nopen..ntodo) are the todo vertices.
     */
  ot = gk_i32incset(nrows, 0, gk_i32malloc(nrows, "gk_csr_ComputeBestFOrderingSymmetric: ot"));

  /* For a vertex that has not been explored, pos[i] is the position in the ot list. */
  pos = gk_i32incset(nrows, 0, gk_i32malloc(nrows, "gk_csr_ComputeBestFOrderingSymmetric: pos"));

  /* if perm[i] >= 0, then perm[i] is the order of vertex i; otherwise perm[i] == -1. */
  perm = gk_i32smalloc(nrows, -1, "gk_csr_ComputeBestFOrderingSymmetric: perm");

  /* create the queue and put the starting vertex in it */
  queue = gk_i32pqCreate(nrows);
  gk_i32pqInsert(queue, v, 1);

  /* put v at the front of the open list */
  pos[0] = ot[0] = v;
  pos[v] = ot[v] = 0;
  nopen = 1;
  ntodo = nrows;

  /* start processing the nodes */
  for (i=0; i<nrows; i++) {
    if (nopen == 0) { /* deal with non-connected graphs */
      gk_i32pqInsert(queue, ot[0], 1);  
      nopen++;
    }

    if ((v = gk_i32pqGetTop(queue)) == -1)
      gk_errexit(SIGERR, "The priority queue got empty ahead of time [i=%d].\n", i);

    if (perm[v] != -1)
      gk_errexit(SIGERR, "The perm[%d] has already been set.\n", v);
    perm[v] = i;

    if (ot[pos[v]] != v)
      gk_errexit(SIGERR, "Something went wrong [ot[pos[%d]]!=%d.\n", v, v);
    if (pos[v] >= nopen)
      gk_errexit(SIGERR, "The position of v is not in open list. pos[%d]=%d is >=%d.\n", v, pos[v], nopen);

    /* remove v from the open list and re-arrange the todo part of the list */
    ot[pos[v]]       = ot[nopen-1];
    pos[ot[nopen-1]] = pos[v];
    if (ntodo > nopen) {
      ot[nopen-1]      = ot[ntodo-1];
      pos[ot[ntodo-1]] = nopen-1;
    }
    nopen--;
    ntodo--;

    for (j=rowptr[v]; j<rowptr[v+1]; j++) {
      u = rowind[j];
      if (perm[u] == -1) {
        /* update ot list, if u is not in the open list by putting it at the end
           of the open list. */
        if (degrees[u] == 0) {
          ot[pos[u]]     = ot[nopen];
          pos[ot[nopen]] = pos[u];
          ot[nopen]      = u;
          pos[u]         = nopen;
          nopen++;

          level[u] = level[v]+1;
          gk_i32pqInsert(queue, u, 0);  
        }


        /* update the in-closed degree */
        degrees[u]++;

        /* update the queues based on the type */
        switch (type) {
          case 1: /* DFS */
            gk_i32pqUpdate(queue, u, 1000*(i+1)+degrees[u]);
            break;

          case 2: /* Max in closed degree */
            gk_i32pqUpdate(queue, u, degrees[u]);
            break;

          case 3: /* Sum of orders in closed list */
            wdegrees[u] += i;
            gk_i32pqUpdate(queue, u, wdegrees[u]);
            break;

          case 4: /* Sum of order-differences */
            /* this is handled at the end of the loop */
            ;
            break;

          case 5: /* BFS with in degree priority */
            gk_i32pqUpdate(queue, u, -(1000*level[u] - degrees[u]));
            break;

          case 6: /* Hybrid of 1+2 */
            gk_i32pqUpdate(queue, u, (i+1)*degrees[u]);
            break;

          default:
            ;
        }
      }
    }

    if (type == 4) { /* update all the vertices in the open list */
      for (j=0; j<nopen; j++) {
        u = ot[j];
        if (perm[u] != -1)
          gk_errexit(SIGERR, "For i=%d, the open list contains a closed row: ot[%zd]=%d, perm[%d]=%d.\n", i, j, u, u, perm[u]);
        sod[u] += degrees[u];
        if (i<1000 || i%25==0)
          gk_i32pqUpdate(queue, u, sod[u]);
      }
    }

    /*
    for (j=0; j<ntodo; j++) {
      if (pos[ot[j]] != j)
        gk_errexit(SIGERR, "pos[ot[%zd]] != %zd.\n", j, j);
    }
    */

  }


  /* time to decide what to return */
  if (r_iperm != NULL) {
    /* use the 'degrees' array to build the iperm array */
    for (i=0; i<nrows; i++)
      degrees[perm[i]] = i;

    *r_iperm = degrees;
    degrees = NULL;
  }

  if (r_perm != NULL) {
    *r_perm = perm;
    perm = NULL;
  }




  /* cleanup memory */
  gk_i32pqDestroy(queue);
  gk_free((void **)&perm, &degrees, &wdegrees, &sod, &ot, &pos, &level, LTERM);

}
