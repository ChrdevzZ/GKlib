/*!
\file
\brief A simple program to convert a tensor in coordinate format into an unfolded
       matrix

\author George
*/

#include <GKlib.h>


/*************************************************************************/
/*! Adds two allocation sizes without wrapping.

    \returns 1 and stores the sum on success, or 0 when the sum is not
             representable by size_t.
*/
/*************************************************************************/
static int size_add(size_t a, size_t b, size_t *result)
{
  if (a > SIZE_MAX-b)
    return 0;

  *result = a+b;
  return 1;
}


/*************************************************************************/
/*! Multiplies two allocation sizes without wrapping.

    \returns 1 and stores the product on success, or 0 when the product is
             not representable by size_t.
*/
/*************************************************************************/
static int size_mul(size_t a, size_t b, size_t *result)
{
  if (a != 0 && b > SIZE_MAX/a)
    return 0;

  *result = a*b;
  return 1;
}


/*************************************************************************/
/*! Advances an input cursor past whitespace without crossing the record. */
/*************************************************************************/
static void skip_space(char **cursor, char *end)
{
  while (*cursor < end && isspace((unsigned char)**cursor))
    (*cursor)++;
}


/*************************************************************************/
/*! Parses one positive, fully delimited int32_t tensor index. */
/*************************************************************************/
static int parse_positive_i32(char **cursor, char *end, int32_t *value)
{
  unsigned long long parsed;
  char *token_end;

  skip_space(cursor, end);
  if (*cursor == end || **cursor == '-')
    return 0;

  errno = 0;
  parsed = strtoull(*cursor, &token_end, 10);
  if (token_end == *cursor || errno == ERANGE || parsed == 0 ||
      parsed > INT32_MAX ||
      (token_end < end && !isspace((unsigned char)*token_end)))
    return 0;

  *cursor = token_end;
  *value = (int32_t)parsed;
  return 1;
}


/*************************************************************************/
/*! Parses one complete four-field tensor record.

    The three indices must be positive int32_t values and the weight must be
    finite. Embedded NUL bytes and trailing non-whitespace data are rejected.
*/
/*************************************************************************/
static int parse_record(char *line, size_t length, int32_t *k, int32_t *i,
    int32_t *j, float *value)
{
  char *cursor = line;
  char *end = line+length;
  char *token_end;

  if (memchr(line, '\0', length) != NULL ||
      !parse_positive_i32(&cursor, end, k) ||
      !parse_positive_i32(&cursor, end, i) ||
      !parse_positive_i32(&cursor, end, j))
    return 0;

  skip_space(&cursor, end);
  if (cursor == end)
    return 0;

  errno = 0;
  *value = strtof(cursor, &token_end);
  if (token_end == cursor || errno == ERANGE || !isfinite(*value) ||
      (token_end < end && !isspace((unsigned char)*token_end)))
    return 0;

  cursor = token_end;
  skip_space(&cursor, end);
  return cursor == end;
}


/*************************************************************************/
/*! Grows all coordinate arrays as one transactional operation.

    Existing arrays and their capacity remain unchanged if any size check or
    allocation fails. The maximum capacity also remains representable by the
    ssize_t pointers used by the generated CSR structures.
*/
/*************************************************************************/
static int grow_entries(size_t count, size_t *capacity, int32_t **I,
    int32_t **J, int32_t **K, float **V)
{
  size_t new_capacity, allocation_bytes, copy_bytes;
  int32_t *new_I = NULL, *new_J = NULL, *new_K = NULL;
  float *new_V = NULL;

  if (*capacity == 0) {
    new_capacity = 1024;
  }
  else {
    if (!size_add(*capacity, *capacity/2+1, &new_capacity))
      return 0;
  }
  if (new_capacity > (size_t)PTRDIFF_MAX ||
      !size_mul(new_capacity, sizeof(int32_t), &allocation_bytes) ||
      !size_mul(new_capacity, sizeof(float), &allocation_bytes))
    return 0;

  new_I = gk_i32malloc(new_capacity, "I");
  new_J = gk_i32malloc(new_capacity, "J");
  new_K = gk_i32malloc(new_capacity, "K");
  new_V = gk_fmalloc(new_capacity, "V");
  if (new_I == NULL || new_J == NULL || new_K == NULL || new_V == NULL)
    goto fail;

  if (count != 0) {
    if (!size_mul(count, sizeof(int32_t), &copy_bytes))
      goto fail;
    memcpy(new_I, *I, copy_bytes);
    memcpy(new_J, *J, copy_bytes);
    memcpy(new_K, *K, copy_bytes);
    if (!size_mul(count, sizeof(float), &copy_bytes))
      goto fail;
    memcpy(new_V, *V, copy_bytes);
  }

  gk_free((void **)I, J, K, V, LTERM);
  *I = new_I;
  *J = new_J;
  *K = new_K;
  *V = new_V;
  *capacity = new_capacity;
  return 1;

fail:
  gk_free((void **)&new_I, &new_J, &new_K, &new_V, LTERM);
  return 0;
}


/*************************************************************************/
/*! Converts a coordinate tensor into a column-oriented unfolded matrix. */
/*************************************************************************/
int main(int argc, char *argv[])
{
  size_t nnz = 0, capacity = 0, i, nI = 0, nJ = 0, nK = 0;
  size_t nrows, ncols, pointer_count, row;
  ssize_t nread, p;
  int32_t *I = NULL, *J = NULL, *K = NULL, *rowind = NULL, *colind = NULL;
  ssize_t *rowptr = NULL, *colptr = NULL;
  float *V = NULL, *rowval = NULL, *colval = NULL;
  char *line = NULL;
  size_t line_capacity = 0;
  FILE *fpin = NULL;
  int status = EXIT_FAILURE;

  if (argc != 2) {
    fprintf(stderr, "Usage: %s <infile>\n", argv[0]);
    goto cleanup;
  }

  fpin = gk_fopen(argv[1], "r", "infile");
  if (fpin == NULL)
    goto cleanup;

  for (;;) {
    int32_t record_I, record_J, record_K;
    float record_V;

    errno = 0;
    nread = gk_getline(&line, &line_capacity, fpin);
    if (nread == -1) {
      if (ferror(fpin) || errno != 0) {
        fprintf(stderr, "Failed while reading %s.\n", argv[1]);
        goto cleanup;
      }
      break;
    }
    if (!parse_record(line, (size_t)nread, &record_K, &record_I, &record_J,
          &record_V)) {
      fprintf(stderr, "Invalid tensor record at line %zu.\n", nnz+1);
      goto cleanup;
    }
    if (nnz == capacity &&
        !grow_entries(nnz, &capacity, &I, &J, &K, &V)) {
      fprintf(stderr, "Tensor is too large or could not be allocated.\n");
      goto cleanup;
    }

    K[nnz] = record_K-1;
    I[nnz] = record_I-1;
    J[nnz] = record_J-1;
    V[nnz] = record_V;
    nK = gk_max(nK, (size_t)record_K);
    nI = gk_max(nI, (size_t)record_I);
    nJ = gk_max(nJ, (size_t)record_J);
    nnz++;
  }
  if (fclose(fpin) != 0) {
    fpin = NULL;
    fprintf(stderr, "Failed to close %s after reading.\n", argv[1]);
    goto cleanup;
  }
  fpin = NULL;
  free(line);
  line = NULL;

  if (nnz == 0) {
    fprintf(stderr, "Tensor input is empty.\n");
    goto cleanup;
  }
  if (!size_mul(nK, nI, &nrows) || nrows > INT32_MAX) {
    fprintf(stderr, "Unfolded row dimension is too large.\n");
    goto cleanup;
  }
  ncols = nJ;
  if (!size_add(nrows, 1, &pointer_count))
    goto cleanup;

  fprintf(stderr, "Input nnz: %zu\n", nnz);
  fprintf(stderr, "nI: %zu, nJ: %zu, nK: %zu\n", nI, nJ, nK);

  rowptr = gk_zsmalloc(pointer_count, 0, "rowptr");
  rowind = gk_i32malloc(nnz, "rowind");
  rowval = gk_fmalloc(nnz, "rowval");
  if (rowptr == NULL || rowind == NULL || rowval == NULL)
    goto cleanup;

  for (i=0; i<nnz; i++) {
    row = (size_t)K[i]*nI+(size_t)I[i];
    rowptr[row]++;
  }
  MAKECSR(i, nrows, rowptr);
  for (i=0; i<nnz; i++) {
    row = (size_t)K[i]*nI+(size_t)I[i];
    p = rowptr[row]++;
    rowind[p] = J[i];
    rowval[p] = V[i];
  }
  SHIFTCSR(i, nrows, rowptr);

  gk_free((void **)&I, &J, &K, &V, LTERM);

  if (!size_add(ncols, 1, &pointer_count))
    goto cleanup;
  colptr = gk_zsmalloc(pointer_count, 0, "colptr");
  colind = gk_i32malloc(nnz, "colind");
  colval = gk_fmalloc(nnz, "colval");
  if (colptr == NULL || colind == NULL || colval == NULL)
    goto cleanup;

  for (i=0; i<nrows; i++) {
    for (p=rowptr[i]; p<rowptr[i+1]; p++)
      colptr[rowind[p]]++;
  }
  MAKECSR(i, ncols, colptr);
  for (i=0; i<nrows; i++) {
    for (p=rowptr[i]; p<rowptr[i+1]; p++) {
      ssize_t destination = colptr[rowind[p]]++;
      colind[destination] = (int32_t)i;
      colval[destination] = rowval[p];
    }
  }
  SHIFTCSR(i, ncols, colptr);

  /* sanity check */
  for (i=0; i<ncols; i++) {
    for (p=colptr[i]+1; p<colptr[i+1]; p++) {
      if (colind[p-1] == colind[p])
        fprintf(stderr, "Duplicate row indices: %d %d %d\n", (int)i,
            colind[p], colind[p-1]);
    }
  }

  if (printf("%zu %zu %zu\n", nrows, ncols, nnz) < 0)
    goto output_error;
  for (i=0; i<ncols; i++) {
    if (printf("%zd\n", colptr[i+1]-colptr[i]) < 0)
      goto output_error;
    for (p=colptr[i]; p<colptr[i+1]; p++) {
      if (printf("%d %.3f\n", colind[p], colval[p]) < 0)
        goto output_error;
    }
  }
  if (fflush(stdout) == EOF || ferror(stdout))
    goto output_error;

  status = EXIT_SUCCESS;
  goto cleanup;

output_error:
  fprintf(stderr, "Failed while writing converted tensor.\n");

cleanup:
  if (fpin != NULL && fclose(fpin) != 0)
    status = EXIT_FAILURE;
  free(line);
  gk_free((void **)&I, &J, &K, &V, &rowptr, &rowind, &rowval,
      &colptr, &colind, &colval, LTERM);
  return status;
}
