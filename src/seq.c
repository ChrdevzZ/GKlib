/*
 *
 * Sequence handler library by Huzefa Rangwala
 * Date : 03.01.2007
 *
 *
 *
 */


#include <GKlib.h>
#include "memory_internal.h"

/*************************************************************************/
/*! Initializes a sequence object to an empty, safely releasable state.

    \param seq is the sequence object to initialize.
*/
/*************************************************************************/
void gk_seq_init(gk_seq_t *seq)
{
  seq->len = 0;
  seq->sequence = NULL;
  seq->pssm = NULL;
  seq->psfm = NULL;
  seq->name = NULL;
  seq->nsymbols = 0;
}


/*************************************************************************/
/*! Creates the character-to-index and index-to-character lookup tables.

    \param alphabet is the ordered set of symbols to place in the tables.
    \returns the initialized lookup table, or NULL on invalid input or an
             allocation failure.
*/
/*************************************************************************/
gk_i2cc2i_t *gk_i2cc2i_create_common(char *alphabet)
{
  int saved_errno;
  size_t i, nsymbols;
  gk_i2cc2i_t *t=NULL;

  if (alphabet == NULL) {
    errno = EINVAL;
    gk_errexit(SIGERR, "An alphabet is required.");
    errno = EINVAL;
    return NULL;
  }

  nsymbols = strlen(alphabet);
  if (nsymbols > 256) {
    errno = EOVERFLOW;
    gk_errexit(SIGMEM, "The alphabet contains too many symbols.");
    errno = EOVERFLOW;
    return NULL;
  }

  t = (gk_i2cc2i_t *)gk_malloc_nosignal(sizeof(gk_i2cc2i_t));
  if (t == NULL)
    goto memory_failure;
  t->n = (int)nsymbols;
  t->i2c = NULL;
  t->c2i = NULL;

  t->i2c = (char *)gk_malloc_nosignal(256*sizeof(char));
  if (t->i2c == NULL)
    goto memory_failure;
  t->c2i = (int *)gk_malloc_nosignal(256*sizeof(int));
  if (t->c2i == NULL)
    goto memory_failure;

  gk_cset(256, -1, t->i2c);
  gk_iset(256, -1, t->c2i);

  for (i=0; i<nsymbols; i++) {
    t->i2c[i] = alphabet[i];
    t->c2i[(unsigned char)alphabet[i]] = (int)i;
  }

  return t;

memory_failure:
  saved_errno = errno != 0 ? errno : ENOMEM;
  if (t != NULL)
    gk_free((void **)&t->i2c, &t->c2i, &t, LTERM);
  errno = saved_errno;
  gk_errexit(SIGMEM, "Memory allocation failed while creating an alphabet.");
  errno = saved_errno;
  return NULL;
}


/*************************************************************************/
/*! Returns the next whitespace-delimited token from a mutable record.

    Delimiters are replaced in place, and the cursor is advanced to the next
    unread byte. NULL denotes the end of the record.
*/
/*************************************************************************/
static char *gk_seq_next_token(char **cursor)
{
  char *token;

  while (isspace((unsigned char)**cursor))
    (*cursor)++;
  if (**cursor == '\0')
    return NULL;

  token = *cursor;
  while (**cursor != '\0' && !isspace((unsigned char)**cursor))
    (*cursor)++;
  if (**cursor != '\0')
    *(*cursor)++ = '\0';

  return token;
}


/*************************************************************************/
/*! Parses one complete decimal token in the range of int. */
/*************************************************************************/
static int gk_seq_parse_int(char *token, int *value)
{
  char *endptr;
  long parsed;

  if (token == NULL || token[0] == '\0')
    return 0;

  errno = 0;
  parsed = strtol(token, &endptr, 10);
  if (errno == ERANGE || parsed < INT_MIN || parsed > INT_MAX ||
      endptr == token || endptr[0] != '\0') {
    errno = EINVAL;
    return 0;
  }

  *value = (int)parsed;
  return 1;
}


/*************************************************************************/
/*! Reads one sequence record while distinguishing EOF from I/O errors.

    A return value of -1 denotes clean EOF and -2 denotes an error or embedded
    NUL byte. Nonnegative values are the byte counts returned by gk_getline().
*/
/*************************************************************************/
static ssize_t gk_seq_getline(char **line, size_t *capacity, FILE *stream)
{
  ssize_t nread;

  errno = 0;
  nread = gk_getline(line, capacity, stream);
  if (nread < 0) {
    if (!ferror(stream) && feof(stream))
      return -1;
    if (errno == 0)
      errno = EIO;
    return -2;
  }
  if (memchr(*line, '\0', (size_t)nread) != NULL) {
    errno = EINVAL;
    return -2;
  }

  return nread;
}


/*************************************************************************/
/*! Transactionally grows the three parallel arrays of a sequence.

    All replacements are allocated and populated before the old arrays are
    released. A failure therefore preserves the original sequence object.
*/
/*************************************************************************/
static int gk_seq_grow(gk_seq_t *seq, size_t *capacity, size_t needed,
    int *r_allocation_failed)
{
  int saved_errno;
  int *new_sequence=NULL;
  int **new_pssm=NULL, **new_psfm=NULL;
  size_t copy_count, new_capacity;

  if (needed <= *capacity)
    return 1;
  if (needed > INT_MAX) {
    errno = EOVERFLOW;
    *r_allocation_failed = 1;
    return 0;
  }

  new_capacity = *capacity == 0 ? 16 : *capacity;
  while (new_capacity < needed) {
    if (new_capacity > (size_t)INT_MAX/2) {
      new_capacity = INT_MAX;
      break;
    }
    new_capacity *= 2;
  }
  if (new_capacity > SIZE_MAX/sizeof(*new_pssm) ||
      new_capacity > SIZE_MAX/sizeof(*new_sequence)) {
    errno = EOVERFLOW;
    *r_allocation_failed = 1;
    return 0;
  }

  new_sequence = (int *)gk_malloc_nosignal(new_capacity*sizeof(int));
  if (new_sequence == NULL)
    goto memory_failure;
  new_pssm = (int **)gk_malloc_nosignal(new_capacity*sizeof(*new_pssm));
  if (new_pssm == NULL)
    goto memory_failure;
  new_psfm = (int **)gk_malloc_nosignal(new_capacity*sizeof(*new_psfm));
  if (new_psfm == NULL)
    goto memory_failure;

  if (seq->len > 0) {
    copy_count = (size_t)seq->len;
    memcpy(new_sequence, seq->sequence, copy_count*sizeof(*new_sequence));
    memcpy(new_pssm, seq->pssm, copy_count*sizeof(*new_pssm));
    memcpy(new_psfm, seq->psfm, copy_count*sizeof(*new_psfm));
  }
  gk_free((void **)&seq->sequence, &seq->pssm, &seq->psfm, LTERM);

  seq->sequence = new_sequence;
  seq->pssm = new_pssm;
  seq->psfm = new_psfm;
  *capacity = new_capacity;
  return 1;

memory_failure:
  saved_errno = errno != 0 ? errno : ENOMEM;
  gk_free((void **)&new_sequence, &new_pssm, &new_psfm, LTERM);
  errno = saved_errno;
  *r_allocation_failed = 1;
  return 0;
}


/*************************************************************************/
/*! Reads a position-specific scoring matrix in GKMOD format.

    The header must contain each of the 20 supported residue symbols exactly
    once. Every following row contains its ordinal, residue symbol, 20 PSSM
    values, and 20 PSFM values. Arrays grow from the records actually read;
    no pre-scan is trusted as an allocation contract.

    \param filename is the name of the PSSM file.
    \returns the parsed sequence, or NULL after complete cleanup on failure.
*/
/*************************************************************************/
gk_seq_t *gk_seq_ReadGKMODPSSM(char *filename)
{
  enum { PSSMWIDTH = 20 };
  static char AAORDER[] = "ARNDCQEGHILKMFPSTWYVBZX*";
  char *basename, *cursor, *extension, *header=NULL, *line=NULL, *token;
  char *windows_basename;
  int allocation_failed=0, header_seen[PSSMWIDTH], i, ignored;
  int saved_errno=0, symbol, value;
  int *pssm_row=NULL, *psfm_row=NULL;
  ssize_t nread;
  size_t capacity=0, length, lnlen=0;
  FILE *fpin=NULL;
  gk_i2cc2i_t *converter=NULL;
  gk_seq_t *seq=NULL;

  if (filename == NULL) {
    errno = EINVAL;
    gk_errexit(SIGERR, "A sequence filename is required.");
    errno = EINVAL;
    return NULL;
  }

  converter = gk_i2cc2i_create_common(AAORDER);
  if (converter == NULL)
    return NULL;
  header = (char *)gk_malloc_nosignal(PSSMWIDTH*sizeof(char));
  if (header == NULL)
    allocation_failed = 1;
  seq = (gk_seq_t *)gk_malloc_nosignal(sizeof(gk_seq_t));
  if (seq == NULL)
    allocation_failed = 1;
  if (seq != NULL)
    gk_seq_init(seq);
  if (header == NULL || seq == NULL)
    goto failure;

  seq->nsymbols = PSSMWIDTH;
  basename = strrchr(filename, '/');
  windows_basename = strrchr(filename, '\\');
  if (basename == NULL ||
      (windows_basename != NULL && windows_basename > basename))
    basename = windows_basename;
  basename = basename == NULL ? filename : basename+1;
  length = strlen(basename);
  if (length == SIZE_MAX) {
    errno = EOVERFLOW;
    allocation_failed = 1;
    goto failure;
  }
  seq->name = (char *)gk_malloc_nosignal(length+1);
  if (seq->name == NULL) {
    allocation_failed = 1;
    goto failure;
  }
  memcpy(seq->name, basename, length+1);
  extension = strrchr(seq->name, '.');
  if (extension != NULL)
    *extension = '\0';

  fpin = gk_fopen(filename, "r", "gk_seq_ReadGKMODPSSM");
  if (fpin == NULL)
    goto failure;

  nread = gk_seq_getline(&line, &lnlen, fpin);
  if (nread < 0) {
    if (nread < -1 && (errno == ENOMEM || errno == EOVERFLOW))
      allocation_failed = 1;
    goto invalid_input;
  }
  gk_strtoupper(line);
  cursor = line;
  memset(header_seen, 0, sizeof(header_seen));
  for (i=0; i<PSSMWIDTH; i++) {
    token = gk_seq_next_token(&cursor);
    if (token == NULL || token[1] != '\0')
      goto invalid_input;
    symbol = converter->c2i[(unsigned char)token[0]];
    if (symbol < 0 || symbol >= PSSMWIDTH || header_seen[symbol])
      goto invalid_input;
    header_seen[symbol] = 1;
    header[i] = token[0];
  }
  if (gk_seq_next_token(&cursor) != NULL)
    goto invalid_input;

  while ((nread = gk_seq_getline(&line, &lnlen, fpin)) >= 0) {
    gk_strtoupper(line);
    cursor = line;
    if (!gk_seq_parse_int(gk_seq_next_token(&cursor), &ignored))
      goto invalid_input;

    token = gk_seq_next_token(&cursor);
    if (token == NULL || token[1] != '\0')
      goto invalid_input;
    symbol = converter->c2i[(unsigned char)token[0]];
    if (symbol < 0)
      goto invalid_input;

    if (!gk_seq_grow(seq, &capacity, (size_t)seq->len+1,
                     &allocation_failed))
      goto failure;
    pssm_row = (int *)gk_malloc_nosignal(PSSMWIDTH*sizeof(int));
    psfm_row = (int *)gk_malloc_nosignal(PSSMWIDTH*sizeof(int));
    if (pssm_row == NULL || psfm_row == NULL) {
      allocation_failed = 1;
      goto failure;
    }
    gk_iset(PSSMWIDTH, 0, pssm_row);
    gk_iset(PSSMWIDTH, 0, psfm_row);

    for (i=0; i<PSSMWIDTH; i++) {
      if (!gk_seq_parse_int(gk_seq_next_token(&cursor), &value))
        goto invalid_input;
      pssm_row[converter->c2i[(unsigned char)header[i]]] = value;
    }
    for (i=0; i<PSSMWIDTH; i++) {
      if (!gk_seq_parse_int(gk_seq_next_token(&cursor), &value))
        goto invalid_input;
      psfm_row[converter->c2i[(unsigned char)header[i]]] = value;
    }
    if (gk_seq_next_token(&cursor) != NULL)
      goto invalid_input;

    seq->sequence[seq->len] = symbol;
    seq->pssm[seq->len] = pssm_row;
    seq->psfm[seq->len] = psfm_row;
    pssm_row = psfm_row = NULL;
    seq->len++;
  }
  if (nread < -1) {
    if (errno == ENOMEM || errno == EOVERFLOW)
      allocation_failed = 1;
    goto failure;
  }
  if (fclose(fpin) != 0) {
    fpin = NULL;
    goto failure;
  }
  fpin = NULL;

  free(line);
  gk_free((void **)&header, &converter->i2c, &converter->c2i,
      &converter, LTERM);
  return seq;

invalid_input:
  if (errno == 0)
    errno = EINVAL;

failure:
  saved_errno = errno != 0 ? errno :
      (allocation_failed ? ENOMEM : EIO);
  gk_free((void **)&pssm_row, &psfm_row, LTERM);
  if (fpin != NULL && fclose(fpin) != 0 && saved_errno == 0)
    saved_errno = errno != 0 ? errno : EIO;
  free(line);
  gk_free((void **)&header, LTERM);
  if (converter != NULL)
    gk_free((void **)&converter->i2c, &converter->c2i, &converter, LTERM);
  if (seq != NULL)
    gk_seq_free(seq);
  errno = saved_errno;
  if (allocation_failed)
    gk_errexit(SIGMEM, "Memory allocation failed while reading %s.",
               filename);
  else
    gk_errexit(saved_errno == ENOMEM || saved_errno == EOVERFLOW ?
        SIGMEM : SIGERR, "Failed to read sequence file %s.", filename);
  errno = saved_errno;
  return NULL;
}


/*************************************************************************/
/*! Frees a sequence and all of its owned arrays.

    \param seq is the sequence to free; NULL is accepted.
*/
/*************************************************************************/
void gk_seq_free(gk_seq_t *seq)
{
  if (seq == NULL)
    return;

  gk_iFreeMatrix(&seq->pssm, seq->len, seq->nsymbols);
  gk_iFreeMatrix(&seq->psfm, seq->len, seq->nsymbols);
  gk_free((void **)&seq->name, &seq->sequence, &seq, LTERM);
}
