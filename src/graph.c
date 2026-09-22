/*!
 * \file 
 *
 * \brief Various routines with dealing with sparse graphs 
 *
 * \author George Karypis
 * \version\verbatim $Id: graph.c 22415 2019-09-05 16:55:00Z karypis $ \endverbatim
 */

#include <GKlib.h>
#include "io_internal.h"
#include "memory_internal.h"

#define OMPMINOPS       50000

/*************************************************************************/
/*! Allocates and initializes a graph.
    \returns the initialized graph, or NULL when allocation fails.
*/
/**************************************************************************/
gk_graph_t *gk_graph_Create(void)
{
  gk_graph_t *graph=NULL;

  if ((graph = (gk_graph_t *)gk_malloc(sizeof(gk_graph_t),
      "gk_graph_Create: graph")) != NULL)
    gk_graph_Init(graph);

  return graph;
}


/*************************************************************************/
/*! Initializes the graph.
    \param graph is the graph to be initialized.
*/
/*************************************************************************/
void gk_graph_Init(gk_graph_t *graph)
{
  memset(graph, 0, sizeof(gk_graph_t));
  graph->nvtxs = -1;
}


/*************************************************************************/
/*! Frees all the memory allocated for a graph.
    \param graph is the graph to be freed.
*/
/*************************************************************************/
void gk_graph_Free(gk_graph_t **graph)
{
  if (*graph == NULL)
    return;
  gk_graph_FreeContents(*graph);
  gk_free((void **)graph, LTERM);
}


/*************************************************************************/
/*! Frees only the memory allocated for the graph's different fields and
    sets them to NULL.
    \param graph is the graph whose contents will be freed.
*/    
/*************************************************************************/
void gk_graph_FreeContents(gk_graph_t *graph)
{
  gk_free((void *)&graph->xadj, &graph->adjncy, 
          &graph->iadjwgt, &graph->fadjwgt,
          &graph->ivwgts, &graph->fvwgts,
          &graph->ivsizes, &graph->fvsizes,
          &graph->vlabels, 
          LTERM);
}


/*************************************************************************/
/*! Releases a partially read graph and reports an input failure.

    The stream is closed before the error is reported so signal-based callers
    cannot bypass cleanup. The line buffer follows the gk_getline() ownership
    contract and is therefore released with free().
*/
/*************************************************************************/
static gk_graph_t *gk_graph_ReadError(FILE *fpin, char *line,
    gk_graph_t *graph, char *filename)
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
  gk_graph_Free(&graph);
  errno = saved_errno;
  gk_errexit(saved_errno == ENOMEM || saved_errno == EOVERFLOW ?
             SIGMEM : SIGERR,
             "Invalid or truncated graph file %s.",
             filename != NULL ? filename : "(null)");
  errno = saved_errno;
  return NULL;
}


/*************************************************************************/
/*! Allocates tracked graph storage without raising a signal. */
/*************************************************************************/
static void *gk_graph_MallocNoSignal(size_t nbytes, int *r_failed)
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
/*! Grows tracked graph-reader storage without raising a signal. */
/*************************************************************************/
static void *gk_graph_ReallocNoSignal(void *oldptr, size_t nbytes, int *r_failed)
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
/*! Creates an empty graph-reader result without raising a signal. */
/*************************************************************************/
static gk_graph_t *gk_graph_CreateNoSignal(int *r_failed)
{
  gk_graph_t *graph;

  graph = (gk_graph_t *)gk_graph_MallocNoSignal(sizeof(gk_graph_t), r_failed);
  if (graph != NULL)
    gk_graph_Init(graph);
  return graph;
}


/*************************************************************************/
/*! Validates the adjacency structure used by graph transformations. */
/*************************************************************************/
static int gk_graph_ValidateStructure(gk_graph_t *graph, ssize_t *r_nedges)
{
  ssize_t i, nedges;

  if (graph == NULL || graph->nvtxs < 0 || graph->xadj == NULL ||
      graph->xadj[0] != 0) {
    errno = EINVAL;
    return 0;
  }
  if ((size_t)graph->nvtxs+1 > SIZE_MAX/sizeof(ssize_t)) {
    errno = EOVERFLOW;
    return 0;
  }
  for (i=0; i<graph->nvtxs; i++) {
    if (graph->xadj[i] < 0 || graph->xadj[i] > graph->xadj[i+1]) {
      errno = EINVAL;
      return 0;
    }
  }
  nedges = graph->xadj[graph->nvtxs];
  if (nedges < 0) {
    errno = EINVAL;
    return 0;
  }
  if ((size_t)nedges > SIZE_MAX/sizeof(int32_t) ||
      (graph->fadjwgt != NULL &&
       (size_t)nedges > SIZE_MAX/sizeof(float))) {
    errno = EOVERFLOW;
    return 0;
  }
  if (nedges > 0 && graph->adjncy == NULL) {
    errno = EINVAL;
    return 0;
  }
  for (i=0; i<nedges; i++) {
    if (graph->adjncy[i] < 0 || graph->adjncy[i] >= graph->nvtxs) {
      errno = EINVAL;
      return 0;
    }
  }

  *r_nedges = nedges;
  return 1;
}


/*************************************************************************/
/*! Validates one side of a vertex permutation. */
/*************************************************************************/
static int gk_graph_ValidatePermutation(int32_t *permutation, int32_t nvtxs,
    int32_t *seen)
{
  int32_t i;

  memset(seen, 0, (size_t)nvtxs*sizeof(int32_t));
  for (i=0; i<nvtxs; i++) {
    if (permutation[i] < 0 || permutation[i] >= nvtxs ||
        seen[permutation[i]]) {
      errno = EINVAL;
      return 0;
    }
    seen[permutation[i]] = 1;
  }
  return 1;
}


/*************************************************************************/
/*! Copies tracked graph storage without raising a signal. */
/*************************************************************************/
static void *gk_graph_CopyNoSignal(const void *source, size_t count,
    size_t element_size, int *r_failed)
{
  void *destination;

  if (element_size != 0 && count > SIZE_MAX/element_size) {
    errno = EOVERFLOW;
    *r_failed = 1;
    return NULL;
  }
  destination = gk_graph_MallocNoSignal(count*element_size, r_failed);
  if (destination != NULL && count != 0)
    memcpy(destination, source, count*element_size);
  return destination;
}


/*************************************************************************/
/*! Cleans up a failed graph transformation before reporting the error. */
/*************************************************************************/
static gk_graph_t *gk_graph_TransformError(gk_graph_t **graph,
    int saved_errno, int allocation_failed, const char *operation)
{
  gk_graph_Free(graph);
  errno = saved_errno != 0 ? saved_errno : EINVAL;
  gk_errexit(allocation_failed || saved_errno == ENOMEM ||
      saved_errno == EOVERFLOW ? SIGMEM : SIGERR,
      "Failed to %s a graph.\n", operation);
  errno = saved_errno != 0 ? saved_errno : EINVAL;
  return NULL;
}


/*************************************************************************/
/*! Cleans up a graph reader before reporting an allocation failure. */
/*************************************************************************/
static gk_graph_t *gk_graph_ReadAllocationError(FILE *fpin, char *line,
    gk_graph_t *graph, char *filename)
{
  int saved_errno=errno != 0 ? errno : ENOMEM;

  if (fpin != NULL)
    fclose(fpin);
  free(line);
  gk_graph_Free(&graph);
  errno = saved_errno;
  gk_errexit(SIGMEM, "Memory allocation failed while reading %s.",
             filename != NULL ? filename : "(null)");
  errno = saved_errno;
  return NULL;
}


/*************************************************************************/
/*! Parses the unsigned fields of a METIS graph header.

    Between two and four complete decimal fields are accepted. Negative,
    overflowing, or trailing non-whitespace input is rejected.
*/
/*************************************************************************/
static int gk_graph_ParseHeader(char *line, size_t *values, size_t *count)
{
  char *cursor=line, *end;
  uintmax_t value;

  *count = 0;
  while (1) {
    while (isspace((unsigned char)*cursor))
      cursor++;
    if (*cursor == '\0')
      return *count >= 2;
    if (*count == 4 || *cursor == '-')
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
static int gk_graph_ParseI32(char **cursor, int32_t *value)
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
static int gk_graph_ParseFloat(char **cursor, float *value)
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
/*! Returns true when a graph record contains no unparsed fields. */
/*************************************************************************/
static int gk_graph_EndRecord(char *cursor)
{
  while (isspace((unsigned char)*cursor))
    cursor++;
  return *cursor == '\0';
}


/**************************************************************************/
/*! Reads a sparse graph from the supplied file.

    \param filename is the file that stores the data.
    \param format is the graph format. The supported values are
           GK_GRAPH_FMT_METIS and GK_GRAPH_FMT_IJV.
    \param hasvals is 1 if the input file has values.
    \param numbering is 1 if the input file numbering starts from one.
    \param isfewgts is 1 if edge weights should be read as floats.
    \param isfvwgts is 1 if vertex weights should be read as floats.
    \param isfvsizes is 1 if vertex sizes should be read as floats.
    \returns the graph that was read, or NULL after a complete cleanup when
             the input is invalid or a resource operation fails.
*/
/**************************************************************************/
gk_graph_t *gk_graph_Read(char *filename, int format, int hasvals, 
                int numbering, int isfewgts, int isfvwgts, int isfvsizes)
{
  ssize_t i, k, l, line_status;
  size_t fields[4], nfields, nvtxs, nedges, fmt, ncon, lnlen=0;
  size_t edgecap=0, newcap;
  ssize_t *xadj;
  intmax_t parsed;
  int32_t declared_nvtxs=0, header_cols=0;
  int32_t ival, *iinds=NULL, *jinds=NULL, *ivals=NULL, *adjncy, *iadjwgt;
  int32_t *newints;
  float fval, *fvals=NULL, *fadjwgt, *newfloats;
  int allocation_failed=0, readsizes=0, readwgts=0, readvals=0;
  int saved_errno=0;
  char *line=NULL, *head, *tail, fmtstr[256];
  FILE *fpin=NULL;
  gk_graph_t *graph=NULL;


  if (filename == NULL) {
    errno = EINVAL;
    return gk_graph_ReadError(NULL, NULL, NULL, filename);
  }
  errno = 0;

  switch (format) {
    case GK_GRAPH_FMT_METIS:
      fpin = gk_fopen(filename, "r", "gk_graph_Read: fpin");
      if (fpin == NULL)
        return NULL;
      do {
        line_status = gk_getline(&line, &lnlen, fpin);
        if (line_status == -1 && !feof(fpin) &&
            (errno == ENOMEM || errno == EOVERFLOW))
          return gk_graph_ReadAllocationError(fpin, line, NULL, filename);
        if (line_status <= 0 ||
            memchr(line, '\0', (size_t)line_status) != NULL)
          return gk_graph_ReadError(fpin, line, NULL, filename);
      } while (line[0] == '%');

      fmt = ncon = 0;
      if (!gk_graph_ParseHeader(line, fields, &nfields))
        return gk_graph_ReadError(fpin, line, NULL, filename);
      nvtxs = fields[0];
      nedges = fields[1];
      if (nfields > 2)
        fmt = fields[2];
      if (nfields > 3)
        ncon = fields[3];

      if (nvtxs > INT32_MAX || nedges > (size_t)PTRDIFF_MAX/2) {
        errno = EOVERFLOW;
        return gk_graph_ReadError(fpin, line, NULL, filename);
      }
      nedges *= 2;

      if (fmt > 111 || fmt%10 > 1 || (fmt/10)%10 > 1 || (fmt/100)%10 > 1)
        return gk_graph_ReadError(fpin, line, NULL, filename);

      sprintf(fmtstr, "%03zu", fmt%1000);
      readsizes = (fmtstr[0] == '1');
      readwgts  = (fmtstr[1] == '1');
      readvals  = (fmtstr[2] == '1');
      numbering = 1;
      if (nfields == 4 && (ncon == 0 || !readwgts))
        return gk_graph_ReadError(fpin, line, NULL, filename);
      ncon      = (ncon == 0 ? 1 : ncon);
      if (readwgts && ncon != 1)
        return gk_graph_ReadError(fpin, line, NULL, filename);
      if (ncon > (size_t)PTRDIFF_MAX ||
          ncon > SIZE_MAX/(nvtxs == 0 ? 1 : nvtxs)) {
        errno = EOVERFLOW;
        return gk_graph_ReadError(fpin, line, NULL, filename);
      }

      if (nvtxs == SIZE_MAX || nvtxs+1 > SIZE_MAX/sizeof(ssize_t) ||
          nedges > SIZE_MAX/sizeof(int32_t) ||
          (readvals && isfewgts && nedges > SIZE_MAX/sizeof(float)) ||
          (readsizes && nvtxs >
           SIZE_MAX/(isfvsizes ? sizeof(float) : sizeof(int32_t))) ||
          (readwgts && nvtxs*ncon >
           SIZE_MAX/(isfvwgts ? sizeof(float) : sizeof(int32_t)))) {
        errno = EOVERFLOW;
        return gk_graph_ReadError(fpin, line, NULL, filename);
      }

      graph = gk_graph_CreateNoSignal(&allocation_failed);
      if (graph == NULL)
        return gk_graph_ReadAllocationError(fpin, line, NULL, filename);
    
      graph->nvtxs = nvtxs;
    
      graph->xadj = (ssize_t *)gk_graph_MallocNoSignal(
          (nvtxs+1)*sizeof(ssize_t), &allocation_failed);
      graph->adjncy = (int32_t *)gk_graph_MallocNoSignal(
          nedges*sizeof(int32_t), &allocation_failed);
      if (readvals) {
        if (isfewgts)
          graph->fadjwgt = (float *)gk_graph_MallocNoSignal(
              nedges*sizeof(float), &allocation_failed);
        else
          graph->iadjwgt = (int32_t *)gk_graph_MallocNoSignal(
              nedges*sizeof(int32_t), &allocation_failed);
      }
    
      if (readsizes) {
        if (isfvsizes)
          graph->fvsizes = (float *)gk_graph_MallocNoSignal(
              nvtxs*sizeof(float), &allocation_failed);
        else
          graph->ivsizes = (int32_t *)gk_graph_MallocNoSignal(
              nvtxs*sizeof(int32_t), &allocation_failed);
      }
    
      if (readwgts) {
        if (isfvwgts)
          graph->fvwgts = (float *)gk_graph_MallocNoSignal(
              nvtxs*ncon*sizeof(float), &allocation_failed);
        else
          graph->ivwgts = (int32_t *)gk_graph_MallocNoSignal(
              nvtxs*ncon*sizeof(int32_t), &allocation_failed);
      }
      if (graph->xadj == NULL || graph->adjncy == NULL ||
          (readvals && graph->fadjwgt == NULL && graph->iadjwgt == NULL) ||
          (readsizes && graph->fvsizes == NULL && graph->ivsizes == NULL) ||
          (readwgts && graph->fvwgts == NULL && graph->ivwgts == NULL))
        return gk_graph_ReadAllocationError(fpin, line, graph, filename);
    
    
      /*----------------------------------------------------------------------
       * Read the sparse graph file
       *---------------------------------------------------------------------*/
      numbering = (numbering ? - 1 : 0);
      for (graph->xadj[0]=0, k=0, i=0; i<nvtxs; i++) {
        do {
          line_status = gk_getline(&line, &lnlen, fpin);
          if (line_status == -1 && !feof(fpin) &&
              (errno == ENOMEM || errno == EOVERFLOW))
            return gk_graph_ReadAllocationError(
                fpin, line, graph, filename);
          if (line_status == -1 ||
              memchr(line, '\0', (size_t)line_status) != NULL)
            return gk_graph_ReadError(fpin, line, graph, filename);
        } while (line[0] == '%');
    
        head = line;
        tail = NULL;
    
        /* Read vertex sizes */
        if (readsizes) {
          if (isfvsizes) {
#ifdef __MSC__
            errno = 0;
            fval = (float)strtod(head, &tail);
#else
            errno = 0;
            fval = strtof(head, &tail);
#endif
            if (tail == head || errno == ERANGE || !isfinite(fval) || fval < 0)
              return gk_graph_ReadError(fpin, line, graph, filename);
            graph->fvsizes[i] = fval;
          }
          else {
            errno = 0;
            parsed = strtoimax(head, &tail, 10);
            if (tail == head || errno == ERANGE || parsed < 0 ||
                parsed > INT32_MAX)
              return gk_graph_ReadError(fpin, line, graph, filename);
            graph->ivsizes[i] = (int32_t)parsed;
          }
          head = tail;
        }
    
        /* Read vertex weights */
        if (readwgts) {
          for (l=0; l<ncon; l++) {
            if (isfvwgts) {
#ifdef __MSC__
              errno = 0;
              fval = (float)strtod(head, &tail);
#else
              errno = 0;
              fval = strtof(head, &tail);
#endif
              if (tail == head || errno == ERANGE || !isfinite(fval) ||
                  fval < 0)
                return gk_graph_ReadError(fpin, line, graph, filename);
              graph->fvwgts[i*ncon+l] = fval;
            }
            else {
              errno = 0;
              parsed = strtoimax(head, &tail, 10);
              if (tail == head || errno == ERANGE || parsed < 0 ||
                  parsed > INT32_MAX)
                return gk_graph_ReadError(fpin, line, graph, filename);
              graph->ivwgts[i*ncon+l] = (int32_t)parsed;
            }
            head = tail;
          }
        }
    
       
        /* Read the rest of the row */
        while (1) {
          errno = 0;
          parsed = strtoimax(head, &tail, 10);
          if (tail == head) 
            break;
          head = tail;

          if (errno == ERANGE || parsed < INT32_MIN || parsed > INT32_MAX ||
              (size_t)k >= nedges)
            return gk_graph_ReadError(fpin, line, graph, filename);
          ival = (int32_t)parsed;
          if (numbering == -1 && ival == INT32_MIN)
            return gk_graph_ReadError(fpin, line, graph, filename);
          
          if ((graph->adjncy[k] = ival + numbering) < 0 ||
              graph->adjncy[k] >= (int32_t)nvtxs)
            return gk_graph_ReadError(fpin, line, graph, filename);
    
          if (readvals) {
            if (isfewgts) {
#ifdef __MSC__
              errno = 0;
              fval = (float)strtod(head, &tail);
#else
              errno = 0;
              fval = strtof(head, &tail);
#endif
              if (tail == head || errno == ERANGE || !isfinite(fval) ||
                  fval <= 0)
                return gk_graph_ReadError(fpin, line, graph, filename);
    
              graph->fadjwgt[k] = fval;
            }
            else {
              errno = 0;
              parsed = strtoimax(head, &tail, 10);
              if (tail == head || errno == ERANGE || parsed <= 0 ||
                  parsed > INT32_MAX)
                return gk_graph_ReadError(fpin, line, graph, filename);
    
              graph->iadjwgt[k] = (int32_t)parsed;
            }
            head = tail;
          }
          k++;
        }
        while (isspace((unsigned char)*head))
          head++;
        if (*head != '\0')
          return gk_graph_ReadError(fpin, line, graph, filename);
        graph->xadj[i+1] = k;
      }
    
      if (k != nedges)
        return gk_graph_ReadError(fpin, line, graph, filename);
      while ((line_status = gk_getline(&line, &lnlen, fpin)) != -1) {
        char *extra=line;
        if (memchr(line, '\0', (size_t)line_status) != NULL)
          return gk_graph_ReadError(fpin, line, graph, filename);
        while (isspace((unsigned char)*extra))
          extra++;
        if (*extra != '\0' && *extra != '%')
          return gk_graph_ReadError(fpin, line, graph, filename);
      }
      if (!feof(fpin) && (errno == ENOMEM || errno == EOVERFLOW))
        return gk_graph_ReadAllocationError(fpin, line, graph, filename);
      if (!feof(fpin) || ferror(fpin))
        return gk_graph_ReadError(fpin, line, graph, filename);
    
      if (fclose(fpin) != 0)
        return gk_graph_ReadError(NULL, line, graph, filename);
  
      free(line);
      line = NULL;

      break;

    case GK_GRAPH_FMT_IJV:
    case GK_GRAPH_FMT_HIJV:
      numbering = (numbering ? -1 : 0);

      fpin = gk_fopen(filename, "r", "gk_graph_Read: fpin");
      if (fpin == NULL) {
        gk_free((void **)&iinds, &jinds, &fvals, &ivals, LTERM);
        return NULL;
      }

      if (format == GK_GRAPH_FMT_HIJV) { /* read the #rows/#cols values */
        line_status = gk_getline(&line, &lnlen, fpin);
        if (line_status == -1 && !feof(fpin) &&
            (errno == ENOMEM || errno == EOVERFLOW))
          goto ijv_allocation_failure;
        head = line;
        if (line_status <= 0 ||
            memchr(line, '\0', (size_t)line_status) != NULL ||
            !gk_graph_ParseI32(&head, &declared_nvtxs) ||
            !gk_graph_ParseI32(&head, &header_cols) ||
            !gk_graph_EndRecord(head) || declared_nvtxs < 0 ||
            declared_nvtxs != header_cols)
          goto ijv_read_failure;
      }

      for (nvtxs=0, nedges=0;
           (line_status = gk_getline(&line, &lnlen, fpin)) != -1; ) {
        if (memchr(line, '\0', (size_t)line_status) != NULL)
          goto ijv_read_failure;
        head = line;
        while (isspace((unsigned char)*head))
          head++;
        if (*head == '\0' || *head == '%')
          continue;
        if (nedges == edgecap) {
          if (edgecap >= (size_t)PTRDIFF_MAX) {
            errno = EOVERFLOW;
            goto ijv_read_failure;
          }
          newcap = edgecap == 0 ? 16 :
                   (edgecap > (size_t)PTRDIFF_MAX/2 ?
                    (size_t)PTRDIFF_MAX : edgecap*2);
          if (newcap > SIZE_MAX/sizeof(int32_t) ||
              (hasvals && isfewgts && newcap > SIZE_MAX/sizeof(float))) {
            errno = EOVERFLOW;
            allocation_failed = 1;
            goto ijv_allocation_failure;
          }
          newints = (int32_t *)gk_graph_ReallocNoSignal(
              iinds, newcap*sizeof(int32_t), &allocation_failed);
          if (newints == NULL)
            goto ijv_allocation_failure;
          iinds = newints;
          newints = (int32_t *)gk_graph_ReallocNoSignal(
              jinds, newcap*sizeof(int32_t), &allocation_failed);
          if (newints == NULL)
            goto ijv_allocation_failure;
          jinds = newints;
          if (hasvals && isfewgts) {
            newfloats = (float *)gk_graph_ReallocNoSignal(
                fvals, newcap*sizeof(float), &allocation_failed);
            if (newfloats == NULL)
              goto ijv_allocation_failure;
            fvals = newfloats;
          }
          else if (hasvals) {
            newints = (int32_t *)gk_graph_ReallocNoSignal(
                ivals, newcap*sizeof(int32_t), &allocation_failed);
            if (newints == NULL)
              goto ijv_allocation_failure;
            ivals = newints;
          }
          edgecap = newcap;
        }
        if (!gk_graph_ParseI32(&head, &iinds[nedges]) ||
            !gk_graph_ParseI32(&head, &jinds[nedges]) ||
            (hasvals && isfewgts &&
             !gk_graph_ParseFloat(&head, &fvals[nedges])) ||
            (hasvals && !isfewgts &&
             !gk_graph_ParseI32(&head, &ivals[nedges])) ||
            !gk_graph_EndRecord(head))
          goto ijv_read_failure;
        if (numbering == -1 &&
            (iinds[nedges] == INT32_MIN || jinds[nedges] == INT32_MIN))
          goto ijv_read_failure;
        iinds[nedges] += numbering;
        jinds[nedges] += numbering;
        if (iinds[nedges] < 0 || jinds[nedges] < 0)
          goto ijv_read_failure;
        if (format == GK_GRAPH_FMT_HIJV &&
            (iinds[nedges] >= declared_nvtxs ||
             jinds[nedges] >= declared_nvtxs))
          goto ijv_read_failure;

        if (nvtxs < (size_t)iinds[nedges])
          nvtxs = iinds[nedges];
        if (nvtxs < (size_t)jinds[nedges])
          nvtxs = jinds[nedges];
        nedges++;
      }
      if (!feof(fpin) && (errno == ENOMEM || errno == EOVERFLOW))
        goto ijv_allocation_failure;
      if (!feof(fpin) || ferror(fpin))
        goto ijv_read_failure;
      if (format == GK_GRAPH_FMT_HIJV)
        nvtxs = (size_t)declared_nvtxs;
      else {
        if (nedges == 0) {
          errno = EINVAL;
          goto ijv_read_failure;
        }
        if (nvtxs >= INT32_MAX) {
          errno = EOVERFLOW;
          goto ijv_read_failure;
        }
        nvtxs++;
      }
      if (fclose(fpin) != 0) {
        saved_errno = errno != 0 ? errno : EIO;
        fpin = NULL;
        gk_free((void **)&iinds, &jinds, &fvals, &ivals, LTERM);
        errno = saved_errno;
        return gk_graph_ReadError(NULL, line, NULL, filename);
      }
      fpin = NULL;
      free(line);
      line = NULL;

      /* convert (i, j, v) into a graph format */
      if (nvtxs == SIZE_MAX || nvtxs+1 > SIZE_MAX/sizeof(ssize_t) ||
          nedges > SIZE_MAX/sizeof(int32_t) ||
          (hasvals && isfewgts && nedges > SIZE_MAX/sizeof(float))) {
        errno = EOVERFLOW;
        allocation_failed = 1;
        goto ijv_allocation_failure;
      }
      graph = gk_graph_CreateNoSignal(&allocation_failed);
      if (graph == NULL)
        goto ijv_allocation_failure;
      graph->nvtxs  = (int32_t)nvtxs;
      xadj = graph->xadj = (ssize_t *)gk_graph_MallocNoSignal(
          (nvtxs+1)*sizeof(ssize_t), &allocation_failed);
      adjncy = graph->adjncy = (int32_t *)gk_graph_MallocNoSignal(
          nedges*sizeof(int32_t), &allocation_failed);
      if (hasvals) {
        if (isfewgts)
          fadjwgt = graph->fadjwgt = (float *)gk_graph_MallocNoSignal(
              nedges*sizeof(float), &allocation_failed);
        else
          iadjwgt = graph->iadjwgt = (int32_t *)gk_graph_MallocNoSignal(
              nedges*sizeof(int32_t), &allocation_failed);
      }
      if (xadj == NULL || adjncy == NULL ||
          (hasvals && graph->fadjwgt == NULL && graph->iadjwgt == NULL))
        goto ijv_allocation_failure;
      memset(xadj, 0, (nvtxs+1)*sizeof(ssize_t));

      for (i=0; i<nedges; i++)
        xadj[iinds[i]]++;
      MAKECSR(i, nvtxs, xadj);

      for (i=0; i<nedges; i++) {
        adjncy[xadj[iinds[i]]] = jinds[i];
        if (hasvals) {
          if (isfewgts)
            fadjwgt[xadj[iinds[i]]] = fvals[i];
          else
            iadjwgt[xadj[iinds[i]]] = ivals[i];
        }
        xadj[iinds[i]]++;
      }
      SHIFTCSR(i, nvtxs, xadj);

      gk_free((void **)&iinds, &jinds, &fvals, &ivals, LTERM);
      break;

ijv_read_failure:
      saved_errno = fpin != NULL && ferror(fpin) ?
          (errno != 0 ? errno : EIO) :
          (errno == EOVERFLOW ? EOVERFLOW : EINVAL);
      if (fpin != NULL)
        fclose(fpin);
      gk_free((void **)&iinds, &jinds, &fvals, &ivals, LTERM);
      errno = saved_errno;
      return gk_graph_ReadError(NULL, line, NULL, filename);

ijv_allocation_failure:
      saved_errno = errno != 0 ? errno : ENOMEM;
      if (fpin != NULL)
        fclose(fpin);
      free(line);
      gk_free((void **)&iinds, &jinds, &fvals, &ivals, LTERM);
      gk_graph_Free(&graph);
      errno = saved_errno;
      gk_errexit(SIGMEM, "Memory allocation failed while reading %s.\n",
                 filename != NULL ? filename : "(null)");
      errno = saved_errno;
      return NULL;

    default:
      errno = EINVAL;
      gk_errexit(SIGERR, "Unrecognized format: %d\n", format);
      errno = EINVAL;
  }

  return graph;
}


/*************************************************************************/
/*! Writes formatted graph output and reports whether vfprintf succeeded. */
/*************************************************************************/
static int gk_graph_Print(FILE *stream, const char *format, ...)
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
/*! Validates the graph view and values required by every writer format.

    The function checks dimensions, CSR monotonicity, adjacency indices,
    numbering mode, and finite floating-point values without modifying the
    graph.
*/
/*************************************************************************/
static int gk_graph_Validate(gk_graph_t *graph, int numbering, int format)
{
  int32_t i;
  ssize_t j, nedges;

  if (graph == NULL || graph->nvtxs < 0 || graph->xadj == NULL ||
      (numbering != 0 && numbering != 1) || graph->xadj[0] != 0 ||
      (graph->iadjwgt != NULL && graph->fadjwgt != NULL) ||
      (graph->ivwgts != NULL && graph->fvwgts != NULL) ||
      (graph->ivsizes != NULL && graph->fvsizes != NULL))
    return 0;
  for (i=0; i<graph->nvtxs; i++) {
    if (graph->xadj[i] < 0 || graph->xadj[i] > graph->xadj[i+1])
      return 0;
  }
  nedges = graph->xadj[graph->nvtxs];
  if (nedges < 0 || (nedges > 0 && graph->adjncy == NULL))
    return 0;
  if ((uintmax_t)nedges > (uintmax_t)SIZE_MAX) {
    errno = EOVERFLOW;
    return -1;
  }
  if ((size_t)graph->nvtxs+1 > SIZE_MAX/sizeof(ssize_t) ||
      (size_t)nedges > SIZE_MAX/sizeof(int32_t) ||
      (graph->fadjwgt != NULL &&
       (size_t)nedges > SIZE_MAX/sizeof(float))) {
    errno = EOVERFLOW;
    return -1;
  }
  for (j=0; j<nedges; j++) {
    if (graph->adjncy[j] < 0 || graph->adjncy[j] >= graph->nvtxs ||
        (graph->fadjwgt != NULL && !isfinite(graph->fadjwgt[j])) ||
        (format == GK_GRAPH_FMT_METIS &&
         ((graph->iadjwgt != NULL && graph->iadjwgt[j] <= 0) ||
          (graph->fadjwgt != NULL && graph->fadjwgt[j] <= 0))))
      return 0;
  }
  for (i=0; i<graph->nvtxs; i++) {
    if ((graph->fvwgts != NULL && !isfinite(graph->fvwgts[i])) ||
        (graph->fvsizes != NULL && !isfinite(graph->fvsizes[i])) ||
        (format == GK_GRAPH_FMT_METIS &&
         ((graph->ivwgts != NULL && graph->ivwgts[i] < 0) ||
          (graph->fvwgts != NULL && graph->fvwgts[i] < 0) ||
          (graph->ivsizes != NULL && graph->ivsizes[i] < 0) ||
          (graph->fvsizes != NULL && graph->fvsizes[i] < 0))))
      return 0;
  }

  return 1;
}


typedef struct {
  int32_t source;
  int32_t target;
  double weight;
} gk_graph_symmetry_edge_t;


/*************************************************************************/
/*! Orders graph arcs by source, target, and the weight written to disk. */
/*************************************************************************/
static int gk_graph_CompareSymmetryEdges(const void *first,
    const void *second)
{
  const gk_graph_symmetry_edge_t *left=first;
  const gk_graph_symmetry_edge_t *right=second;

  if (left->source != right->source)
    return left->source < right->source ? -1 : 1;
  if (left->target != right->target)
    return left->target < right->target ? -1 : 1;
  if (left->weight != right->weight)
    return left->weight < right->weight ? -1 : 1;
  return 0;
}


/*************************************************************************/
/*! Finds the first sorted arc matching a source, target, and weight. */
/*************************************************************************/
static size_t gk_graph_LowerSymmetryEdge(gk_graph_symmetry_edge_t *edges,
    size_t nedges, int32_t source, int32_t target, double weight)
{
  gk_graph_symmetry_edge_t key;
  size_t first=0, length=nedges;

  key.source = source;
  key.target = target;
  key.weight = weight;
  while (length != 0) {
    size_t half=length/2;
    size_t middle=first+half;

    if (gk_graph_CompareSymmetryEdges(edges+middle, &key) < 0) {
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
/*! Validates the reciprocal multiset required by the METIS graph format.

    Parallel arcs must have equally many reverse arcs with the same weight.
    Self loops are rejected because they cannot represent an undirected edge
    through the format's two-adjacency-entry accounting rule. A negative
    result denotes allocation failure; zero denotes an invalid graph.
*/
/*************************************************************************/
static int gk_graph_ValidateSymmetry(gk_graph_t *graph)
{
  gk_graph_symmetry_edge_t *edges;
  ssize_t edge;
  size_t i, j, nedges, reverse, reverse_end;
  int32_t vertex;

  nedges = (size_t)graph->xadj[graph->nvtxs];
  if (nedges%2 != 0)
    return 0;
  if (nedges > SIZE_MAX/sizeof(*edges)) {
    errno = EOVERFLOW;
    return -1;
  }
  edges = (gk_graph_symmetry_edge_t *)gk_malloc_nosignal(
      nedges*sizeof(*edges));
  if (edges == NULL)
    return -1;

  i = 0;
  for (vertex=0; vertex<graph->nvtxs; vertex++) {
    for (edge=graph->xadj[vertex]; edge<graph->xadj[vertex+1]; edge++) {
      edges[i].source = vertex;
      edges[i].target = graph->adjncy[edge];
      edges[i].weight = graph->iadjwgt != NULL ? graph->iadjwgt[edge] :
          (graph->fadjwgt != NULL ? graph->fadjwgt[edge] : 0.0);
      i++;
    }
  }
  qsort(edges, nedges, sizeof(*edges), gk_graph_CompareSymmetryEdges);

  for (i=0; i<nedges; i=j) {
    for (j=i+1; j<nedges &&
         gk_graph_CompareSymmetryEdges(edges+i, edges+j) == 0; j++) {
    }
    if (edges[i].source == edges[i].target) {
      gk_free((void **)&edges, LTERM);
      return 0;
    }

    reverse = gk_graph_LowerSymmetryEdge(edges, nedges,
        edges[i].target, edges[i].source, edges[i].weight);
    for (reverse_end=reverse; reverse_end<nedges &&
         edges[reverse_end].source == edges[i].target &&
         edges[reverse_end].target == edges[i].source &&
         edges[reverse_end].weight == edges[i].weight; reverse_end++) {
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
/*! Writes a graph into a file.
    \param graph is the graph to be written,
    \param filename is the name of the output file.
    \param format specifies the format of the output file.
    \param numbering is either 0 or 1, indicating if the first vertex
           will be numbered 0 or 1. Some formats ignore this.
*/
/**************************************************************************/
void gk_graph_Write(gk_graph_t *graph, char *filename, int format, int numbering)
{
  int symmetry, validation;
  int saved_errno=0;
  int32_t i;
  ssize_t j;
  int hasvwgts, hasvsizes, hasewgts, failed=0;
  char *tempname=NULL;
  FILE *fpout=NULL;

  if (format != GK_GRAPH_FMT_METIS && format != GK_GRAPH_FMT_IJV) {
    errno = EINVAL;
    gk_errexit(SIGERR, "Unknown file format. %d\n", format);
    errno = EINVAL;
    return;
  }
  validation = gk_graph_Validate(graph, numbering, format);
  if (validation <= 0) {
    saved_errno = validation < 0 ?
        (errno != 0 ? errno : EOVERFLOW) : EINVAL;
    errno = saved_errno;
    gk_errexit(validation < 0 ? SIGMEM : SIGERR,
        "Cannot write an invalid graph structure.\n");
    errno = saved_errno;
    return;
  }
  if (format == GK_GRAPH_FMT_METIS) {
    symmetry = gk_graph_ValidateSymmetry(graph);
    if (symmetry <= 0) {
      saved_errno = symmetry < 0 ?
          (errno != 0 ? errno : ENOMEM) : EINVAL;
      errno = saved_errno;
      gk_errexit(symmetry < 0 ? SIGMEM : SIGERR,
          symmetry < 0 ? "Failed to validate graph symmetry.\n" :
          "METIS output format requires a symmetric graph.\n");
      errno = saved_errno;
      return;
    }
  }

  if (filename)
    fpout = gk_open_output_file(filename, "w", &tempname);
  else
    fpout = stdout; 
  if (fpout == NULL) {
    saved_errno = errno != 0 ? errno : EIO;
    errno = saved_errno;
    gk_errexit(saved_errno == ENOMEM || saved_errno == EOVERFLOW ?
        SIGMEM : SIGERR, "Failed to open graph output file %s.\n",
               filename != NULL ? filename : "<stdout>");
    errno = saved_errno;
    return;
  }


  hasewgts  = (graph->iadjwgt || graph->fadjwgt);
  hasvwgts  = (graph->ivwgts || graph->fvwgts);
  hasvsizes = (graph->ivsizes || graph->fvsizes);

  switch (format) {
    case GK_GRAPH_FMT_METIS:
      /* write the header line */
      if (!gk_graph_Print(fpout, "%d %zd", graph->nvtxs,
                          graph->xadj[graph->nvtxs]/2)) {
        failed = 1;
        break;
      }
      if (hasvwgts || hasvsizes || hasewgts) 
        if (!gk_graph_Print(fpout, " %d%d%d", hasvsizes, hasvwgts,
                            hasewgts)) {
          failed = 1;
          break;
        }
      if (!gk_graph_Print(fpout, "\n")) {
        failed = 1;
        break;
      }
    
    
      for (i=0; i<graph->nvtxs; i++) {
        if (hasvsizes) {
          if (graph->ivsizes)
            failed = !gk_graph_Print(fpout, " %d", graph->ivsizes[i]);
          else
            failed = !gk_graph_Print(fpout, " %f", graph->fvsizes[i]);
          if (failed)
            break;
        }
    
        if (hasvwgts) {
          if (graph->ivwgts)
            failed = !gk_graph_Print(fpout, " %d", graph->ivwgts[i]);
          else
            failed = !gk_graph_Print(fpout, " %f", graph->fvwgts[i]);
          if (failed)
            break;
        }
    
        for (j=graph->xadj[i]; j<graph->xadj[i+1]; j++) {
          if (!gk_graph_Print(fpout, " %d", graph->adjncy[j]+1)) {
            failed = 1;
            break;
          }
          if (hasewgts) {
            if (graph->iadjwgt)
              failed = !gk_graph_Print(fpout, " %d", graph->iadjwgt[j]);
            else 
              failed = !gk_graph_Print(fpout, " %f", graph->fadjwgt[j]);
            if (failed)
              break;
          }
        }
        if (failed || !gk_graph_Print(fpout, "\n")) {
          failed = 1;
          break;
        }
      }
      break;

    case GK_GRAPH_FMT_IJV:
      for (i=0; i<graph->nvtxs; i++) {
        for (j=graph->xadj[i]; j<graph->xadj[i+1]; j++) {
          if (!gk_graph_Print(fpout, "%d %d ", i+numbering,
                              graph->adjncy[j]+numbering)) {
            failed = 1;
            break;
          }
          if (hasewgts) {
            if (graph->iadjwgt)
              failed = !gk_graph_Print(fpout, " %d\n", graph->iadjwgt[j]);
            else 
              failed = !gk_graph_Print(fpout, " %f\n", graph->fadjwgt[j]);
          }
          else {
            failed = !gk_graph_Print(fpout, " 1\n");
          }
          if (failed)
            break;
        }
        if (failed)
          break;
      }
      break;

    default:
      failed = 1;
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
        SIGMEM : SIGERR, "Failed to write graph file %s.\n",
               filename != NULL ? filename : "<stdout>");
    errno = saved_errno;
  }
}


/*************************************************************************/
/*! Returns a copy of a graph.
    \param graph is the graph to be duplicated.
    \returns the newly created copy of the graph.
*/
/**************************************************************************/
gk_graph_t *gk_graph_Dup(gk_graph_t *graph)
{
  ssize_t nedges;
  int allocation_failed=0, saved_errno=0;
  size_t nvtxs;
  gk_graph_t *ngraph=NULL;

  if (!gk_graph_ValidateStructure(graph, &nedges)) {
    saved_errno = errno;
    goto failure;
  }
  nvtxs = (size_t)graph->nvtxs;

  ngraph = gk_graph_CreateNoSignal(&allocation_failed);
  if (ngraph == NULL)
    goto allocation_failure;
  ngraph->nvtxs = graph->nvtxs;
  ngraph->xadj = (ssize_t *)gk_graph_CopyNoSignal(graph->xadj, nvtxs+1,
      sizeof(ssize_t), &allocation_failed);
  if (graph->adjncy)
    ngraph->adjncy = (int32_t *)gk_graph_CopyNoSignal(graph->adjncy,
        (size_t)nedges, sizeof(int32_t), &allocation_failed);
  if (graph->iadjwgt)
    ngraph->iadjwgt = (int32_t *)gk_graph_CopyNoSignal(graph->iadjwgt,
        (size_t)nedges, sizeof(int32_t), &allocation_failed);
  if (graph->fadjwgt)
    ngraph->fadjwgt = (float *)gk_graph_CopyNoSignal(graph->fadjwgt,
        (size_t)nedges, sizeof(float), &allocation_failed);
  if (graph->ivwgts)
    ngraph->ivwgts = (int32_t *)gk_graph_CopyNoSignal(graph->ivwgts,
        nvtxs, sizeof(int32_t), &allocation_failed);
  if (graph->fvwgts)
    ngraph->fvwgts = (float *)gk_graph_CopyNoSignal(graph->fvwgts,
        nvtxs, sizeof(float), &allocation_failed);
  if (graph->ivsizes)
    ngraph->ivsizes = (int32_t *)gk_graph_CopyNoSignal(graph->ivsizes,
        nvtxs, sizeof(int32_t), &allocation_failed);
  if (graph->fvsizes)
    ngraph->fvsizes = (float *)gk_graph_CopyNoSignal(graph->fvsizes,
        nvtxs, sizeof(float), &allocation_failed);
  if (graph->vlabels)
    ngraph->vlabels = (int32_t *)gk_graph_CopyNoSignal(graph->vlabels,
        nvtxs, sizeof(int32_t), &allocation_failed);
  if (allocation_failed)
    goto allocation_failure;

  return ngraph;

allocation_failure:
  saved_errno = errno != 0 ? errno : ENOMEM;
  allocation_failed = 1;

failure:
  return gk_graph_TransformError(&ngraph, saved_errno,
      allocation_failed, "duplicate");
}


/*************************************************************************/
/*! Returns the transpose of a graph.
    \param graph is the graph to be transposed.
    \returns the newly created copy of the graph.
*/
/**************************************************************************/
gk_graph_t *gk_graph_Transpose(gk_graph_t *graph)
{
  int32_t vi, vj;
  ssize_t ei, nedges;
  int allocation_failed=0, saved_errno=0;
  size_t nvtxs;
  gk_graph_t *ngraph=NULL;

  if (!gk_graph_ValidateStructure(graph, &nedges)) {
    saved_errno = errno;
    goto failure;
  }
  nvtxs = (size_t)graph->nvtxs;

  ngraph = gk_graph_CreateNoSignal(&allocation_failed);
  if (ngraph == NULL)
    goto allocation_failure;
  ngraph->nvtxs = graph->nvtxs;
  ngraph->xadj = (ssize_t *)gk_graph_MallocNoSignal(
      (nvtxs+1)*sizeof(ssize_t), &allocation_failed);
  ngraph->adjncy = (int32_t *)gk_graph_MallocNoSignal(
      (size_t)nedges*sizeof(int32_t), &allocation_failed);
  if (graph->iadjwgt)
    ngraph->iadjwgt = (int32_t *)gk_graph_MallocNoSignal(
        (size_t)nedges*sizeof(int32_t), &allocation_failed);
  if (graph->fadjwgt)
    ngraph->fadjwgt = (float *)gk_graph_MallocNoSignal(
        (size_t)nedges*sizeof(float), &allocation_failed);
  if (graph->ivwgts)
    ngraph->ivwgts = (int32_t *)gk_graph_CopyNoSignal(graph->ivwgts,
        nvtxs, sizeof(int32_t), &allocation_failed);
  if (graph->fvwgts)
    ngraph->fvwgts = (float *)gk_graph_CopyNoSignal(graph->fvwgts,
        nvtxs, sizeof(float), &allocation_failed);
  if (graph->ivsizes)
    ngraph->ivsizes = (int32_t *)gk_graph_CopyNoSignal(graph->ivsizes,
        nvtxs, sizeof(int32_t), &allocation_failed);
  if (graph->fvsizes)
    ngraph->fvsizes = (float *)gk_graph_CopyNoSignal(graph->fvsizes,
        nvtxs, sizeof(float), &allocation_failed);
  if (graph->vlabels)
    ngraph->vlabels = (int32_t *)gk_graph_CopyNoSignal(graph->vlabels,
        nvtxs, sizeof(int32_t), &allocation_failed);
  if (allocation_failed)
    goto allocation_failure;

  memset(ngraph->xadj, 0, (nvtxs+1)*sizeof(ssize_t));

  for (vi=0; vi<graph->nvtxs; vi++) {
    for (ei=graph->xadj[vi]; ei<graph->xadj[vi+1]; ei++)
      ngraph->xadj[graph->adjncy[ei]]++;
  }
  MAKECSR(vi, ngraph->nvtxs, ngraph->xadj);

  for (vi=0; vi<graph->nvtxs; vi++) {
    for (ei=graph->xadj[vi]; ei<graph->xadj[vi+1]; ei++) {
      vj = graph->adjncy[ei];
      ngraph->adjncy[ngraph->xadj[vj]] = vi;
      if (ngraph->iadjwgt)
        ngraph->iadjwgt[ngraph->xadj[vj]] = graph->iadjwgt[ei];
      if (ngraph->fadjwgt)
        ngraph->fadjwgt[ngraph->xadj[vj]] = graph->fadjwgt[ei];
      ngraph->xadj[vj]++;
    }
  }
  SHIFTCSR(vi, ngraph->nvtxs, ngraph->xadj);

  return ngraph;

allocation_failure:
  saved_errno = errno != 0 ? errno : ENOMEM;
  allocation_failed = 1;

failure:
  return gk_graph_TransformError(&ngraph, saved_errno,
      allocation_failed, "transpose");
}


/*************************************************************************/
/*! Returns a subgraph containing a set of consecutive vertices.
    \param graph is the original graph.
    \param vstart is the starting vertex.
    \param nvtxs is the number of vertices from vstart to extract.
    \returns the newly created subgraph.
*/
/**************************************************************************/
gk_graph_t *gk_graph_ExtractSubgraph(gk_graph_t *graph, int vstart, int nvtxs)
{
  ssize_t i, j, firstedge, lastedge, nedges, totaledges;
  int allocation_failed=0, saved_errno=0;
  size_t nvertices;
  gk_graph_t *ngraph=NULL;

  if (!gk_graph_ValidateStructure(graph, &totaledges)) {
    saved_errno = errno;
    goto failure;
  }
  if (vstart < 0 || nvtxs < 0 || vstart > graph->nvtxs ||
      nvtxs > graph->nvtxs-vstart) {
    saved_errno = EINVAL;
    goto failure;
  }

  firstedge = graph->xadj[vstart];
  lastedge = graph->xadj[vstart+nvtxs];
  nedges = lastedge-firstedge;
  nvertices = (size_t)nvtxs;
  ngraph = gk_graph_CreateNoSignal(&allocation_failed);
  if (ngraph == NULL)
    goto allocation_failure;
  ngraph->nvtxs = nvtxs;
  ngraph->xadj = (ssize_t *)gk_graph_MallocNoSignal(
      (nvertices+1)*sizeof(ssize_t), &allocation_failed);
  if (graph->adjncy)
    ngraph->adjncy = (int32_t *)gk_graph_MallocNoSignal(
        (size_t)nedges*sizeof(int32_t), &allocation_failed);
  if (graph->iadjwgt)
    ngraph->iadjwgt = (int32_t *)gk_graph_MallocNoSignal(
        (size_t)nedges*sizeof(int32_t), &allocation_failed);
  if (graph->fadjwgt)
    ngraph->fadjwgt = (float *)gk_graph_MallocNoSignal(
        (size_t)nedges*sizeof(float), &allocation_failed);
  if (graph->ivwgts)
    ngraph->ivwgts = (int32_t *)gk_graph_CopyNoSignal(graph->ivwgts+vstart,
        nvertices, sizeof(int32_t), &allocation_failed);
  if (graph->fvwgts)
    ngraph->fvwgts = (float *)gk_graph_CopyNoSignal(graph->fvwgts+vstart,
        nvertices, sizeof(float), &allocation_failed);
  if (graph->ivsizes)
    ngraph->ivsizes = (int32_t *)gk_graph_CopyNoSignal(graph->ivsizes+vstart,
        nvertices, sizeof(int32_t), &allocation_failed);
  if (graph->fvsizes)
    ngraph->fvsizes = (float *)gk_graph_CopyNoSignal(graph->fvsizes+vstart,
        nvertices, sizeof(float), &allocation_failed);
  if (graph->vlabels)
    ngraph->vlabels = (int32_t *)gk_graph_CopyNoSignal(graph->vlabels+vstart,
        nvertices, sizeof(int32_t), &allocation_failed);
  if (allocation_failed)
    goto allocation_failure;

  ngraph->xadj[0] = nedges = 0;
  for (i=0; i<nvtxs; i++) {
    for (j=graph->xadj[vstart+i]; j<graph->xadj[vstart+i+1]; j++) {
      if (graph->adjncy[j] >= vstart &&
          graph->adjncy[j] < vstart+nvtxs) {
        ngraph->adjncy[nedges] = graph->adjncy[j]-vstart;
        if (ngraph->iadjwgt)
          ngraph->iadjwgt[nedges] = graph->iadjwgt[j];
        if (ngraph->fadjwgt)
          ngraph->fadjwgt[nedges] = graph->fadjwgt[j];
        nedges++;
      }
    }
    ngraph->xadj[i+1] = nedges;
  }

  return ngraph;

allocation_failure:
  saved_errno = errno != 0 ? errno : ENOMEM;
  allocation_failed = 1;

failure:
  return gk_graph_TransformError(&ngraph, saved_errno,
      allocation_failed, "extract a subgraph from");
}


/*************************************************************************/
/*! Returns a graph that has been reordered according to the permutation.
    \param[IN] graph is the graph to be re-ordered.
    \param[IN] perm is the new ordering of the graph's vertices
    \param[IN] iperm is the original ordering of the re-ordered graph's vertices
    \returns the newly created copy of the graph.

    \note Either perm or iperm can be NULL but not both.
*/
/**************************************************************************/
gk_graph_t *gk_graph_Reorder(gk_graph_t *graph, int32_t *perm, int32_t *iperm)
{
  ssize_t j, jj, nedges;
  int32_t i, u, v, nvtxs, *seen=NULL;
  int32_t *localperm=NULL, *localiperm=NULL;
  int allocation_failed=0, saved_errno=0;
  size_t nvertices;
  gk_graph_t *ngraph=NULL;

  if (perm == NULL && iperm == NULL) {
    saved_errno = EINVAL;
    goto failure;
  }
  if (!gk_graph_ValidateStructure(graph, &nedges)) {
    saved_errno = errno;
    goto failure;
  }
  nvtxs = graph->nvtxs;
  nvertices = (size_t)nvtxs;
  seen = (int32_t *)gk_graph_MallocNoSignal(nvertices*sizeof(int32_t),
      &allocation_failed);
  if (seen == NULL)
    goto allocation_failure;
  if ((perm != NULL &&
       !gk_graph_ValidatePermutation(perm, nvtxs, seen)) ||
      (iperm != NULL &&
       !gk_graph_ValidatePermutation(iperm, nvtxs, seen))) {
    saved_errno = errno;
    goto failure;
  }
  if (perm != NULL && iperm != NULL) {
    for (i=0; i<nvtxs; i++) {
      if (perm[iperm[i]] != i) {
        saved_errno = EINVAL;
        goto failure;
      }
    }
  }
  if (perm == NULL) {
    localperm = (int32_t *)gk_graph_MallocNoSignal(
        nvertices*sizeof(int32_t), &allocation_failed);
    if (localperm == NULL)
      goto allocation_failure;
    for (i=0; i<nvtxs; i++)
      localperm[iperm[i]] = i;
    perm = localperm;
  }
  if (iperm == NULL) {
    localiperm = (int32_t *)gk_graph_MallocNoSignal(
        nvertices*sizeof(int32_t), &allocation_failed);
    if (localiperm == NULL)
      goto allocation_failure;
    for (i=0; i<nvtxs; i++)
      localiperm[perm[i]] = i;
    iperm = localiperm;
  }

  ngraph = gk_graph_CreateNoSignal(&allocation_failed);
  if (ngraph == NULL)
    goto allocation_failure;
  ngraph->nvtxs = nvtxs;
  ngraph->xadj = (ssize_t *)gk_graph_MallocNoSignal(
      (nvertices+1)*sizeof(ssize_t), &allocation_failed);
  ngraph->adjncy = (int32_t *)gk_graph_MallocNoSignal(
      (size_t)nedges*sizeof(int32_t), &allocation_failed);
  if (graph->iadjwgt)
    ngraph->iadjwgt = (int32_t *)gk_graph_MallocNoSignal(
        (size_t)nedges*sizeof(int32_t), &allocation_failed);
  if (graph->fadjwgt)
    ngraph->fadjwgt = (float *)gk_graph_MallocNoSignal(
        (size_t)nedges*sizeof(float), &allocation_failed);
  if (graph->ivwgts)
    ngraph->ivwgts = (int32_t *)gk_graph_MallocNoSignal(
        nvertices*sizeof(int32_t), &allocation_failed);
  if (graph->fvwgts)
    ngraph->fvwgts = (float *)gk_graph_MallocNoSignal(
        nvertices*sizeof(float), &allocation_failed);
  if (graph->ivsizes)
    ngraph->ivsizes = (int32_t *)gk_graph_MallocNoSignal(
        nvertices*sizeof(int32_t), &allocation_failed);
  if (graph->fvsizes)
    ngraph->fvsizes = (float *)gk_graph_MallocNoSignal(
        nvertices*sizeof(float), &allocation_failed);
  if (graph->vlabels)
    ngraph->vlabels = (int32_t *)gk_graph_MallocNoSignal(
        nvertices*sizeof(int32_t), &allocation_failed);
  if (allocation_failed)
    goto allocation_failure;

  ngraph->xadj[0] = jj = 0;
  for (v=0; v<nvtxs; v++) {
    u = iperm[v];
    for (j=graph->xadj[u]; j<graph->xadj[u+1]; j++, jj++) {
      ngraph->adjncy[jj] = perm[graph->adjncy[j]];
      if (graph->iadjwgt)
        ngraph->iadjwgt[jj] = graph->iadjwgt[j];
      if (graph->fadjwgt)
        ngraph->fadjwgt[jj] = graph->fadjwgt[j];
    }
    if (graph->ivwgts)
      ngraph->ivwgts[v] = graph->ivwgts[u];
    if (graph->fvwgts)
      ngraph->fvwgts[v] = graph->fvwgts[u];
    if (graph->ivsizes)
      ngraph->ivsizes[v] = graph->ivsizes[u];
    if (graph->fvsizes)
      ngraph->fvsizes[v] = graph->fvsizes[u];
    if (graph->vlabels)
      ngraph->vlabels[v] = graph->vlabels[u];
    ngraph->xadj[v+1] = jj;
  }

  gk_free((void **)&seen, &localperm, &localiperm, LTERM);
  return ngraph;

allocation_failure:
  saved_errno = errno != 0 ? errno : ENOMEM;
  allocation_failed = 1;

failure:
  gk_free((void **)&seen, &localperm, &localiperm, LTERM);
  return gk_graph_TransformError(&ngraph, saved_errno,
      allocation_failed, "reorder");
}


/*************************************************************************/
/*! This function finds the connected components in a graph.

    \param graph is the graph structure
    \param cptr is the ptr structure of the CSR representation of the 
           components. The length of this vector must be graph->nvtxs+1.
    \param cind is the indices structure of the CSR representation of 
           the components. The length of this vector must be graph->nvtxs.

    \returns the number of components that it found.

    \note The cptr and cind parameters can be NULL, in which case only the
          number of connected components is returned.
*/
/*************************************************************************/
int gk_graph_FindComponents(gk_graph_t *graph, int32_t *cptr, int32_t *cind)
{
  ssize_t i, j, k, nedges, nvtxs, first, last, ntodo, position;
  int32_t ncmps=0, start;
  int32_t *work_cptr=NULL, *work_cind=NULL, *pos=NULL, *todo=NULL;
  int allocation_failed=0, saved_errno=0;
  size_t nvertices;

  if ((cptr == NULL) != (cind == NULL)) {
    saved_errno = EINVAL;
    goto failure;
  }
  if (!gk_graph_ValidateStructure(graph, &nedges)) {
    saved_errno = errno;
    goto failure;
  }
  nvtxs = graph->nvtxs;
  nvertices = (size_t)nvtxs;
  work_cptr = (int32_t *)gk_graph_MallocNoSignal(
      (nvertices+1)*sizeof(int32_t), &allocation_failed);
  work_cind = (int32_t *)gk_graph_MallocNoSignal(
      nvertices*sizeof(int32_t), &allocation_failed);
  pos = (int32_t *)gk_graph_MallocNoSignal(
      nvertices*sizeof(int32_t), &allocation_failed);
  todo = (int32_t *)gk_graph_MallocNoSignal(
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
    for (j=graph->xadj[i]; j<graph->xadj[i+1]; j++) {
      k = graph->adjncy[j];
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

  if (cptr != NULL) {
    memcpy(cptr, work_cptr, ((size_t)ncmps+1)*sizeof(int32_t));
    memcpy(cind, work_cind, nvertices*sizeof(int32_t));
  }
  gk_free((void **)&work_cptr, &work_cind, &pos, &todo, LTERM);
  return ncmps;

allocation_failure:
  saved_errno = errno != 0 ? errno : ENOMEM;
  allocation_failed = 1;

failure:
  gk_free((void **)&work_cptr, &work_cind, &pos, &todo, LTERM);
  errno = saved_errno != 0 ? saved_errno : EINVAL;
  gk_errexit(allocation_failed || saved_errno == ENOMEM ||
      saved_errno == EOVERFLOW ? SIGMEM : SIGERR,
      "Failed to find graph components.\n");
  errno = saved_errno != 0 ? saved_errno : EINVAL;
  return -1;
}


/*************************************************************************/
/*! This function computes a permutation of the vertices based on a
    breadth-first-traversal. It can be used for re-ordering the graph
    to reduce its bandwidth for better cache locality.
    The algorithm used is a simplified version of the method used to find
    the connected components.

    \param[IN]  graph is the graph structure
    \param[IN]  v is the starting vertex of the BFS
    \param[OUT] perm[i] stores the ID of vertex i in the re-ordered graph.
    \param[OUT] iperm[i] stores the ID of the vertex that corresponds to 
                the ith vertex in the re-ordered graph.

    \note The perm or iperm (but not both) can be NULL, at which point, 
          the corresponding arrays are not returned. Though the program
          works fine when both are NULL, doing that is not smart.
          The returned arrays should be freed with gk_free().
*/
/*************************************************************************/
void gk_graph_ComputeBFSOrdering(gk_graph_t *graph, int v, int32_t **r_perm,
          int32_t **r_iperm)
{
  ssize_t j, *xadj;
  int i, k, nvtxs, first, last;
  int32_t *adjncy, *cot, *pos;

  if (graph->nvtxs <= 0)
    return;

  nvtxs  = graph->nvtxs;
  xadj   = graph->xadj;
  adjncy = graph->adjncy;

  /* This array will function like pos + touched of the CC method */
  pos = gk_i32incset(nvtxs, 0, gk_i32malloc(nvtxs, "gk_graph_ComputeBFSOrdering: pos"));

  /* This array ([C]losed[O]pen[T]odo => cot) serves three purposes. 
     Positions from [0...first) is the current iperm[] vector of the explored vertices; 
     Positions from [first...last) is the OPEN list (i.e., visited vertices);
     Positions from [last...nvtxs) is the todo list. */
  cot = gk_i32incset(nvtxs, 0, gk_i32malloc(nvtxs, "gk_graph_ComputeBFSOrdering: cot"));


  /* put v at the front of the todo list */
  pos[0] = cot[0] = v;
  pos[v] = cot[v] = 0;

  /* compute a BFS ordering from the seed vertex */
  first = last = 0;
  while (first < nvtxs) {
    if (first == last) { /* Find another starting vertex */
      k = cot[last];
      ASSERT(pos[k] != -1);
      pos[k] = -1; /* mark node as being visited */
      last++;
    }

    i = cot[first++];  /* the ++ advances the explored vertices */
    for (j=xadj[i]; j<xadj[i+1]; j++) {
      k = adjncy[j];
      /* if a node has already been visited, its pos[] will be -1 */
      if (pos[k] != -1) {
        /* pos[k] is the location within cot[] where k resides (it is in the 'todo' part); 
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
    for (i=0; i<nvtxs; i++)
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
/*! This function computes a permutation of the vertices based on a
    best-first-traversal. It can be used for re-ordering the graph
    to reduce its bandwidth for better cache locality.

    \param[IN]  graph is the graph structure.
    \param[IN]  v is the starting vertex of the best-first traversal.
    \param[IN]  type indicates the criteria to use to measure the 'bestness'
                of a vertex.
    \param[OUT] perm[i] stores the ID of vertex i in the re-ordered graph.
    \param[OUT] iperm[i] stores the ID of the vertex that corresponds to 
                the ith vertex in the re-ordered graph.

    \note The perm or iperm (but not both) can be NULL, at which point, 
          the corresponding arrays are not returned. Though the program
          works fine when both are NULL, doing that is not smart.
          The returned arrays should be freed with gk_free().
*/
/*************************************************************************/
void gk_graph_ComputeBestFOrdering0(gk_graph_t *graph, int v, int type, 
          int32_t **r_perm, int32_t **r_iperm)
{
  ssize_t j, jj, *xadj;
  int i, k, u, nvtxs;
  int32_t *adjncy, *perm, *degrees, *minIDs, *open;
  gk_i32pq_t *queue;

  if (graph->nvtxs <= 0)
    return;

  nvtxs  = graph->nvtxs;
  xadj   = graph->xadj;
  adjncy = graph->adjncy;

  /* the degree of the vertices in the closed list */
  degrees = gk_i32smalloc(nvtxs, 0, "gk_graph_ComputeBestFOrdering: degrees");

  /* the minimum vertex ID of an open vertex to the closed list */ 
  minIDs  = gk_i32smalloc(nvtxs, nvtxs+1, "gk_graph_ComputeBestFOrdering: minIDs");

  /* the open list */ 
  open  = gk_i32malloc(nvtxs, "gk_graph_ComputeBestFOrdering: open");

  /* if perm[i] >= 0, then perm[i] is the order of vertex i; 
     otherwise perm[i] == -1.
  */
  perm = gk_i32smalloc(nvtxs, -1, "gk_graph_ComputeBestFOrdering: perm");

  /* create the queue and put everything in it */
  queue = gk_i32pqCreate(nvtxs);
  for (i=0; i<nvtxs; i++)
    gk_i32pqInsert(queue, i, 0);
  gk_i32pqUpdate(queue, v, 1);

  open[0] = v;

  /* start processing the nodes */
  for (i=0; i<nvtxs; i++) {
    if ((v = gk_i32pqGetTop(queue)) == -1) 
      gk_errexit(SIGERR, "The priority queue got empty ahead of time [i=%d].\n", i);
    if (perm[v] != -1)
      gk_errexit(SIGERR, "The perm[%d] has already been set.\n", v);
    perm[v] = i;


    for (j=xadj[v]; j<xadj[v+1]; j++) {
      u = adjncy[j];
      if (perm[u] == -1) {
        degrees[u]++;
        minIDs[u] = (i < minIDs[u] ? i : minIDs[u]);

        switch (type) {
          case 1: /* DFS */
            gk_i32pqUpdate(queue, u, 1);
            break;
          case 2: /* Max in closed degree */
            gk_i32pqUpdate(queue, u, degrees[u]);
            break;
          case 3: /* Sum of orders in closed list */
            for (k=0, jj=xadj[u]; jj<xadj[u+1]; jj++) {
              if (perm[adjncy[jj]] != -1)
                k += perm[adjncy[jj]];
            }
            gk_i32pqUpdate(queue, u, k);
            break;
          case 4: /* Sum of order-differences (w.r.t. current number) in closed 
                     list (updated once in a while) */
            for (k=0, jj=xadj[u]; jj<xadj[u+1]; jj++) {
              if (perm[adjncy[jj]] != -1)
                k += (i-perm[adjncy[jj]]);
            }
            gk_i32pqUpdate(queue, u, k);
            break;
          default:
            ;
        }
      }
    }
  }


  /* time to decide what to return */
  if (r_perm != NULL) {
    *r_perm = perm;
    perm = NULL;
  }

  if (r_iperm != NULL) {
    /* use the 'degrees' array to build the iperm array */
    for (i=0; i<nvtxs; i++)
      degrees[perm[i]] = i;

    *r_iperm = degrees;
    degrees = NULL;
  }



  /* cleanup memory */
  gk_i32pqDestroy(queue);
  gk_free((void **)&perm, &degrees, &minIDs, &open, LTERM);

}


/*************************************************************************/
/*! This function computes a permutation of the vertices based on a
    best-first-traversal. It can be used for re-ordering the graph
    to reduce its bandwidth for better cache locality.

    \param[IN]  graph is the graph structure.
    \param[IN]  v is the starting vertex of the best-first traversal.
    \param[IN]  type indicates the criteria to use to measure the 'bestness'
                of a vertex.
    \param[OUT] perm[i] stores the ID of vertex i in the re-ordered graph.
    \param[OUT] iperm[i] stores the ID of the vertex that corresponds to 
                the ith vertex in the re-ordered graph.

    \note The perm or iperm (but not both) can be NULL, at which point, 
          the corresponding arrays are not returned. Though the program
          works fine when both are NULL, doing that is not smart.
          The returned arrays should be freed with gk_free().
*/
/*************************************************************************/
void gk_graph_ComputeBestFOrdering(gk_graph_t *graph, int v, int type, 
          int32_t **r_perm, int32_t **r_iperm)
{
  ssize_t j, jj, *xadj;
  int i, k, u, nvtxs, nopen, ntodo;
  int32_t *adjncy, *perm, *degrees, *sod, *level, *ot, *pos;
  int64_t *wdegrees;
  gk_i32pq_t *queue;

  if (graph->nvtxs <= 0)
    return;

  nvtxs  = graph->nvtxs;
  xadj   = graph->xadj;
  adjncy = graph->adjncy;

  /* the degree of the vertices in the closed list */
  degrees = gk_i32smalloc(nvtxs, 0, "gk_graph_ComputeBestFOrdering: degrees");

  /* the weighted degree of the vertices in the closed list for type==3 */
  wdegrees = gk_i64smalloc(nvtxs, 0, "gk_graph_ComputeBestFOrdering: wdegrees");

  /* the sum of differences for type==4 */
  sod = gk_i32smalloc(nvtxs, 0, "gk_graph_ComputeBestFOrdering: sod");

  /* the encountering level of a vertex type==5 */
  level = gk_i32smalloc(nvtxs, 0, "gk_graph_ComputeBestFOrdering: level");

  /* The open+todo list of vertices. 
     The vertices from [0..nopen] are the open vertices.
     The vertices from [nopen..ntodo) are the todo vertices.
     */
  ot = gk_i32incset(nvtxs, 0, gk_i32malloc(nvtxs, "gk_graph_FindComponents: ot"));

  /* For a vertex that has not been explored, pos[i] is the position in the ot list. */
  pos = gk_i32incset(nvtxs, 0, gk_i32malloc(nvtxs, "gk_graph_FindComponents: pos"));

  /* if perm[i] >= 0, then perm[i] is the order of vertex i; otherwise perm[i] == -1. */
  perm = gk_i32smalloc(nvtxs, -1, "gk_graph_ComputeBestFOrdering: perm");

  /* create the queue and put the starting vertex in it */
  queue = gk_i32pqCreate(nvtxs);
  gk_i32pqInsert(queue, v, 1);

  /* put v at the front of the open list */
  pos[0] = ot[0] = v;
  pos[v] = ot[v] = 0;
  nopen = 1;
  ntodo = nvtxs;

  /* start processing the nodes */
  for (i=0; i<nvtxs; i++) {
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

    for (j=xadj[v]; j<xadj[v+1]; j++) {
      u = adjncy[j];
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
            gk_i32pqUpdate(queue, u, (int32_t)sqrt(wdegrees[u]));
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
          gk_errexit(SIGERR, "For i=%d, the open list contains a closed vertex: ot[%zd]=%d, perm[%d]=%d.\n", i, j, u, u, perm[u]);
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
  if (r_perm != NULL) {
    *r_perm = perm;
    perm = NULL;
  }

  if (r_iperm != NULL) {
    /* use the 'degrees' array to build the iperm array */
    for (i=0; i<nvtxs; i++)
      degrees[perm[i]] = i;

    *r_iperm = degrees;
    degrees = NULL;
  }



  /* cleanup memory */
  gk_i32pqDestroy(queue);
  gk_free((void **)&perm, &degrees, &wdegrees, &sod, &ot, &pos, &level, LTERM);

}


/*************************************************************************/
/*! This function computes the single-source shortest path lengths from the
    root node to all the other nodes in the graph. If the graph is not 
    connected then, the sortest part to the vertices in the other components 
    is -1.

    \param[IN]  graph is the graph structure.
    \param[IN]  v is the root of the single-source shortest path computations.
    \param[IN]  type indicates the criteria to use to measure the 'bestness'
                of a vertex.
    \param[OUT] sps[i] stores the length of the shortest path from v to vertex i.
                If no such path exists, then it is -1. Note that the returned
                array will be either an array of int32_t or an array of floats.
                The specific type is determined by the existance of non NULL
                iadjwgt and fadjwgt arrays. If both of these arrays exist, then
                priority is given to iadjwgt.

    \note The returned array should be freed with gk_free().
*/
/*************************************************************************/
void gk_graph_SingleSourceShortestPaths(gk_graph_t *graph, int v, void **r_sps)
{
  ssize_t *xadj;
  int i, u, nvtxs;
  int32_t *adjncy, *inqueue;

  if (graph->nvtxs <= 0)
    return;

  nvtxs  = graph->nvtxs;
  xadj   = graph->xadj;
  adjncy = graph->adjncy;

  inqueue = gk_i32smalloc(nvtxs, 0, "gk_graph_SingleSourceShortestPaths: inqueue");

  /* determine if you will be computing using int32_t or float and proceed from there */
  if (graph->iadjwgt != NULL) {
    gk_i32pq_t *queue;
    int32_t *adjwgt;
    int32_t *sps;

    adjwgt = graph->iadjwgt;

    queue = gk_i32pqCreate(nvtxs);
    gk_i32pqInsert(queue, v, 0);
    inqueue[v] = 1;

    sps = gk_i32smalloc(nvtxs, -1, "gk_graph_SingleSourceShortestPaths: sps");
    sps[v] = 0;

    /* start processing the nodes */
    while ((v = gk_i32pqGetTop(queue)) != -1) {
      inqueue[v] = 2;

      /* relax the adjacent edges */
      for (i=xadj[v]; i<xadj[v+1]; i++) {
        u = adjncy[i];
        if (inqueue[u] == 2)
          continue;

        if (sps[u] < 0 || sps[v]+adjwgt[i] < sps[u]) {
          sps[u] = sps[v]+adjwgt[i];

          if (inqueue[u])
            gk_i32pqUpdate(queue, u, -sps[u]);
          else {
            gk_i32pqInsert(queue, u, -sps[u]);
            inqueue[u] = 1;
          }
        }
      }
    }

    *r_sps = (void *)sps;

    gk_i32pqDestroy(queue);
  }
  else {
    gk_fpq_t *queue;
    float *adjwgt;
    float *sps;

    adjwgt = graph->fadjwgt;

    queue = gk_fpqCreate(nvtxs);
    gk_fpqInsert(queue, v, 0);
    inqueue[v] = 1;

    sps = gk_fsmalloc(nvtxs, -1, "gk_graph_SingleSourceShortestPaths: sps");
    sps[v] = 0;

    /* start processing the nodes */
    while ((v = gk_fpqGetTop(queue)) != -1) {
      inqueue[v] = 2;

      /* relax the adjacent edges */
      for (i=xadj[v]; i<xadj[v+1]; i++) {
        u = adjncy[i];
        if (inqueue[u] == 2)
          continue;

        if (sps[u] < 0 || sps[v]+adjwgt[i] < sps[u]) {
          sps[u] = sps[v]+adjwgt[i];

          if (inqueue[u])
            gk_fpqUpdate(queue, u, -sps[u]);
          else {
            gk_fpqInsert(queue, u, -sps[u]);
            inqueue[u] = 1;
          }
        }
      }
    }

    *r_sps = (void *)sps;

    gk_fpqDestroy(queue);
  }

  gk_free((void **)&inqueue, LTERM);

}


/*************************************************************************/
/*! Sorts the adjacency lists in increasing vertex order
    \param graph the graph itself,
*/
/**************************************************************************/
void gk_graph_SortAdjacencies(gk_graph_t *graph)
{
  int32_t nvtxs, nn=0;
  ssize_t *xadj;
  int32_t *adjncy;
  int32_t *iadjwgt;
  float *fadjwgt;

  nvtxs   = graph->nvtxs;
  xadj    = graph->xadj;
  adjncy  = graph->adjncy;
  iadjwgt = graph->iadjwgt;
  fadjwgt = graph->fadjwgt;

  #pragma omp parallel if (nvtxs > 100)
  {
    ssize_t i, j, k;
    gk_ikv_t *cand;
    int32_t *itwgts=NULL;
    float *ftwgts=NULL;

    #pragma omp single
    for (i=0; i<nvtxs; i++) 
      nn = gk_max(nn, xadj[i+1]-xadj[i]);
  
    cand   = gk_ikvmalloc(nn, "gk_graph_SortIndices: cand");
    if (iadjwgt)
      itwgts = gk_i32malloc(nn, "gk_graph_SortIndices: itwgts");
    if (fadjwgt)
      ftwgts = gk_fmalloc(nn, "gk_graph_SortIndices: ftwgts");
  
    #pragma omp for schedule(static)
    for (i=0; i<nvtxs; i++) {
      for (k=0, j=xadj[i]; j<xadj[i+1]; j++) {
        if (j > xadj[i] && adjncy[j] < adjncy[j-1])
          k = 1; /* an inversion */
        cand[j-xadj[i]].val = (int32_t)(j-xadj[i]);
        cand[j-xadj[i]].key = adjncy[j];
        if (itwgts)
          itwgts[j-xadj[i]] = iadjwgt[j];
        if (ftwgts)
          ftwgts[j-xadj[i]] = fadjwgt[j];
      }
      if (k) {
        gk_ikvsorti(xadj[i+1]-xadj[i], cand);
        for (j=xadj[i]; j<xadj[i+1]; j++) {
          adjncy[j] = cand[j-xadj[i]].key;
          if (itwgts)
            iadjwgt[j] = itwgts[cand[j-xadj[i]].val];
          if (ftwgts)
            fadjwgt[j] = ftwgts[cand[j-xadj[i]].val];
        }
      }
    }

    gk_free((void **)&cand, &itwgts, &ftwgts, LTERM);
  }
}


/*************************************************************************/
/*! Returns a symmetric version of a graph. The symmetric version
    is constructed by applying an A op A^T operation, where op is one of
    GK_GRAPH_SYM_SUM, GK_GRAPH_SYM_MIN, GK_GRAPH_SYM_MAX, GK_GRAPH_SYM_AVG.
   
    \param mat the matrix to be symmetrized,
    \param op indicates the operation to be performed. The possible values are
           GK_GRAPH_SYM_SUM, GK_GRAPH_SYM_MIN, GK_GRAPH_SYM_MAX, and GK_GRAPH_SYM_AVG.

    \returns the symmetrized matrix consisting only of its row-based structure. 
          The input matrix is not modified. 

TODO: Need to deal with all vertex attributes that are currently do not get
      copied over.
*/
/**************************************************************************/
gk_graph_t *gk_graph_MakeSymmetric(gk_graph_t *graph, int op)
{
  ssize_t i, j, nnz, nadj, nout, nedges, maxedges, position;
  ssize_t *marker=NULL, *last=NULL, *next=NULL;
  int32_t nrows;
  int hasvals, allocation_failed=0, saved_errno=0;
  ssize_t *rowptr, *colptr=NULL, *nrowptr;
  int32_t *rowind, *colind=NULL, *nrowind, *ids=NULL;
  float *rowval=NULL, *colval=NULL, *nrowval=NULL, *wgts=NULL;
  int32_t *irowval=NULL, *icolval=NULL, *nirowval=NULL, *iwgts=NULL;
  gk_graph_t *ngraph=NULL;

  if (graph == NULL || graph->nvtxs < 0 || graph->xadj == NULL ||
      graph->xadj[0] != 0 ||
      (graph->iadjwgt != NULL && graph->fadjwgt != NULL) ||
      (op != GK_GRAPH_SYM_SUM && op != GK_GRAPH_SYM_MIN &&
       op != GK_GRAPH_SYM_MAX && op != GK_GRAPH_SYM_AVG)) {
    saved_errno = EINVAL;
    goto failure;
  }

  hasvals = (graph->iadjwgt != NULL || graph->fadjwgt != NULL);

  nrows  = graph->nvtxs;
  rowptr = graph->xadj;
  rowind = graph->adjncy;
  for (i=0; i<nrows; i++) {
    if (rowptr[i] < 0 || rowptr[i] > rowptr[i+1]) {
      saved_errno = EINVAL;
      goto failure;
    }
  }
  nedges = rowptr[nrows];
  if (nedges < 0 || (nedges > 0 && rowind == NULL)) {
    saved_errno = EINVAL;
    goto failure;
  }
  if (nedges > PTRDIFF_MAX/2 ||
      (size_t)nrows+1 > SIZE_MAX/sizeof(ssize_t) ||
      (size_t)nedges > SIZE_MAX/(2*sizeof(int32_t)) ||
      (size_t)nedges > SIZE_MAX/(2*sizeof(ssize_t)) ||
      (hasvals && graph->fadjwgt != NULL &&
       (size_t)nedges > SIZE_MAX/(2*sizeof(float))) ||
      (size_t)nrows > SIZE_MAX/sizeof(ssize_t)) {
    saved_errno = EOVERFLOW;
    goto failure;
  }
  maxedges = 2*nedges;
  for (i=0; i<nedges; i++) {
    if (rowind[i] < 0 || rowind[i] >= nrows ||
        (graph->fadjwgt != NULL && !isfinite(graph->fadjwgt[i]))) {
      saved_errno = EINVAL;
      goto failure;
    }
  }
  if (hasvals) {
    irowval = graph->iadjwgt;
    rowval = graph->fadjwgt;
  }

  /* create the column view for efficient processing */
  colptr = (ssize_t *)gk_graph_MallocNoSignal(
      ((size_t)nrows+1)*sizeof(ssize_t), &allocation_failed);
  colind = (int32_t *)gk_graph_MallocNoSignal(
      (size_t)nedges*sizeof(int32_t), &allocation_failed);
  if (hasvals) {
    if (rowval)
      colval = (float *)gk_graph_MallocNoSignal(
          (size_t)nedges*sizeof(float), &allocation_failed);
    if (irowval)
      icolval = (int32_t *)gk_graph_MallocNoSignal(
          (size_t)nedges*sizeof(int32_t), &allocation_failed);
  }
  if (colptr == NULL || colind == NULL ||
      (rowval != NULL && colval == NULL) ||
      (irowval != NULL && icolval == NULL))
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
      if (hasvals) {
        if (rowval)
          colval[colptr[rowind[j]]] = rowval[j];
        if (irowval)
          icolval[colptr[rowind[j]]] = irowval[j];
      }
      colptr[rowind[j]]++;
    }
  }
  SHIFTCSR(i, nrows, colptr);


  ngraph = gk_graph_CreateNoSignal(&allocation_failed);
  if (ngraph == NULL)
    goto allocation_failure;
  ngraph->nvtxs = graph->nvtxs;

  nrowptr = ngraph->xadj = (ssize_t *)gk_graph_MallocNoSignal(
      ((size_t)nrows+1)*sizeof(ssize_t), &allocation_failed);
  nrowind = ngraph->adjncy = (int32_t *)gk_graph_MallocNoSignal(
      (size_t)maxedges*sizeof(int32_t), &allocation_failed);
  if (hasvals) {
    if (rowval)
      nrowval = ngraph->fadjwgt = (float *)gk_graph_MallocNoSignal(
          (size_t)maxedges*sizeof(float), &allocation_failed);
    if (irowval)
      nirowval = ngraph->iadjwgt = (int32_t *)gk_graph_MallocNoSignal(
          (size_t)maxedges*sizeof(int32_t), &allocation_failed);
  }

  marker = (ssize_t *)gk_graph_MallocNoSignal(
      (size_t)nrows*sizeof(ssize_t), &allocation_failed);
  last = (ssize_t *)gk_graph_MallocNoSignal(
      (size_t)nrows*sizeof(ssize_t), &allocation_failed);
  next = (ssize_t *)gk_graph_MallocNoSignal(
      (size_t)maxedges*sizeof(ssize_t), &allocation_failed);
  ids = (int32_t *)gk_graph_MallocNoSignal(
      (size_t)maxedges*sizeof(int32_t), &allocation_failed);
  if (hasvals) {
    if (rowval)
      wgts = (float *)gk_graph_MallocNoSignal(
          (size_t)maxedges*sizeof(float), &allocation_failed);
    if (irowval)
      iwgts = (int32_t *)gk_graph_MallocNoSignal(
          (size_t)maxedges*sizeof(int32_t), &allocation_failed);
  }
  if (nrowptr == NULL || nrowind == NULL || marker == NULL || last == NULL ||
      next == NULL || ids == NULL ||
      (rowval != NULL && (nrowval == NULL || wgts == NULL)) ||
      (irowval != NULL && (nirowval == NULL || iwgts == NULL)))
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
      if (wgts)
        wgts[nadj] = rowval[j];
      if (iwgts)
        iwgts[nadj] = irowval[j];
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
        if (op != GK_GRAPH_SYM_MIN) {
          ids[nadj] = colind[j]; 
          if (wgts) 
            wgts[nadj] = (op == GK_GRAPH_SYM_AVG ? 0.5*colval[j] : colval[j]);
          if (iwgts) 
            iwgts[nadj] = (op == GK_GRAPH_SYM_AVG ? icolval[j]/2 : icolval[j]);
          nadj++;
        }
      }
      else {
        position = marker[colind[j]];
        marker[colind[j]] = next[position];
        if (marker[colind[j]] == -1)
          last[colind[j]] = -1;
        next[position] = -2;
        if (wgts) {
          switch (op) {
            case GK_GRAPH_SYM_MAX:
              wgts[position] = gk_max(colval[j], wgts[position]);
              break;
            case GK_GRAPH_SYM_MIN:
              wgts[position] = gk_min(colval[j], wgts[position]);
              break;
            case GK_GRAPH_SYM_SUM:
              wgts[position] += colval[j];
              if (!isfinite(wgts[position])) {
                saved_errno = EOVERFLOW;
                goto failure;
              }
              break;
            case GK_GRAPH_SYM_AVG:
              wgts[position] = (float)(0.5*((double)wgts[position] +
                  colval[j]));
              break;
          }
        }
        if (iwgts) {
          switch (op) {
            case GK_GRAPH_SYM_MAX:
              iwgts[position] = gk_max(icolval[j], iwgts[position]);
              break;
            case GK_GRAPH_SYM_MIN:
              iwgts[position] = gk_min(icolval[j], iwgts[position]);
              break;
            case GK_GRAPH_SYM_SUM:
              {
                int64_t sum=(int64_t)iwgts[position] + icolval[j];

                if (sum < INT32_MIN || sum > INT32_MAX) {
                  saved_errno = EOVERFLOW;
                  goto failure;
                }
                iwgts[position] = (int32_t)sum;
              }
              break;
            case GK_GRAPH_SYM_AVG:
              iwgts[position] = (int32_t)(
                  ((int64_t)iwgts[position] + icolval[j])/2);
              break;
          }
        }
      }
    }

    /* resolve any out-edges that were not found in the in-edges */
    for (j=0; j<nout; j++) {
      if (next[j] != -2) {
        if (op == GK_GRAPH_SYM_MIN)
          ids[j] = -1;
        else if (op == GK_GRAPH_SYM_AVG) {
          if (wgts)
            wgts[j] *= 0.5f;
          if (iwgts)
            iwgts[j] /= 2;
        }
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
        if (wgts)
          nrowval[nnz] = wgts[j];
        if (iwgts)
          nirowval[nnz] = iwgts[j];
        nnz++;
      }
    }
    nrowptr[i+1] = nnz;
  }

  gk_free((void **)&colptr, &colind, &colval, &icolval, &marker, &last,
          &next, &ids, &wgts, &iwgts, LTERM);

  return ngraph;

allocation_failure:
  saved_errno = errno != 0 ? errno : ENOMEM;
  allocation_failed = 1;

failure:
  gk_free((void **)&colptr, &colind, &colval, &icolval, &marker, &last,
          &next, &ids, &wgts, &iwgts, LTERM);
  gk_graph_Free(&ngraph);
  errno = saved_errno != 0 ? saved_errno : EINVAL;
  gk_errexit(allocation_failed || saved_errno == ENOMEM ||
      saved_errno == EOVERFLOW ? SIGMEM : SIGERR,
      "Failed to make a symmetric graph.\n");
  errno = saved_errno != 0 ? saved_errno : EINVAL;
  return NULL;
}



#ifdef XXX

/*************************************************************************/
/*! Returns a subgraphrix containing a certain set of rows.
    \param graph is the original graphrix.
    \param nrows is the number of rows to extract.
    \param rind is the set of row numbers to extract.
    \returns the row structure of the newly created subgraphrix.
*/
/**************************************************************************/
gk_graph_t *gk_graph_ExtractRows(gk_graph_t *graph, int nrows, int *rind)
{
  ssize_t i, ii, j, nnz;
  gk_graph_t *ngraph;

  ngraph = gk_graph_Create();

  ngraph->nrows = nrows;
  ngraph->ncols = graph->ncols;

  for (nnz=0, i=0; i<nrows; i++)  
    nnz += graph->rowptr[rind[i]+1]-graph->rowptr[rind[i]];

  ngraph->rowptr = gk_zmalloc(ngraph->nrows+1, "gk_graph_ExtractPartition: rowptr");
  ngraph->rowind = gk_imalloc(nnz, "gk_graph_ExtractPartition: rowind");
  ngraph->rowval = gk_fmalloc(nnz, "gk_graph_ExtractPartition: rowval");

  ngraph->rowptr[0] = 0;
  for (nnz=0, j=0, ii=0; ii<nrows; ii++) {
    i = rind[ii];
    gk_icopy(graph->rowptr[i+1]-graph->rowptr[i], graph->rowind+graph->rowptr[i], ngraph->rowind+nnz);
    gk_fcopy(graph->rowptr[i+1]-graph->rowptr[i], graph->rowval+graph->rowptr[i], ngraph->rowval+nnz);
    nnz += graph->rowptr[i+1]-graph->rowptr[i];
    ngraph->rowptr[++j] = nnz;
  }
  ASSERT(j == ngraph->nrows);

  return ngraph;
}


/*************************************************************************/
/*! Returns a subgraphrix corresponding to a specified partitioning of rows.
    \param graph is the original graphrix.
    \param part is the partitioning vector of the rows.
    \param pid is the partition ID that will be extracted.
    \returns the row structure of the newly created subgraphrix.
*/
/**************************************************************************/
gk_graph_t *gk_graph_ExtractPartition(gk_graph_t *graph, int *part, int pid)
{
  ssize_t i, j, nnz;
  gk_graph_t *ngraph;

  ngraph = gk_graph_Create();

  ngraph->nrows = 0;
  ngraph->ncols = graph->ncols;

  for (nnz=0, i=0; i<graph->nrows; i++) {
    if (part[i] == pid) {
      ngraph->nrows++;
      nnz += graph->rowptr[i+1]-graph->rowptr[i];
    }
  }

  ngraph->rowptr = gk_zmalloc(ngraph->nrows+1, "gk_graph_ExtractPartition: rowptr");
  ngraph->rowind = gk_imalloc(nnz, "gk_graph_ExtractPartition: rowind");
  ngraph->rowval = gk_fmalloc(nnz, "gk_graph_ExtractPartition: rowval");

  ngraph->rowptr[0] = 0;
  for (nnz=0, j=0, i=0; i<graph->nrows; i++) {
    if (part[i] == pid) {
      gk_icopy(graph->rowptr[i+1]-graph->rowptr[i], graph->rowind+graph->rowptr[i], ngraph->rowind+nnz);
      gk_fcopy(graph->rowptr[i+1]-graph->rowptr[i], graph->rowval+graph->rowptr[i], ngraph->rowval+nnz);
      nnz += graph->rowptr[i+1]-graph->rowptr[i];
      ngraph->rowptr[++j] = nnz;
    }
  }
  ASSERT(j == ngraph->nrows);

  return ngraph;
}


/*************************************************************************/
/*! Splits the graphrix into multiple sub-graphrices based on the provided
    color array.
    \param graph is the original graphrix.
    \param color is an array of size equal to the number of non-zeros
           in the graphrix (row-wise structure). The graphrix is split into
           as many parts as the number of colors. For meaningfull results,
           the colors should be numbered consecutively starting from 0.
    \returns an array of graphrices for each supplied color number.
*/
/**************************************************************************/
gk_graph_t **gk_graph_Split(gk_graph_t *graph, int *color)
{
  ssize_t i, j;
  int nrows, ncolors;
  ssize_t *rowptr;
  int *rowind;
  float *rowval;
  gk_graph_t **sgraphs;

  nrows  = graph->nrows;
  rowptr = graph->rowptr;
  rowind = graph->rowind;
  rowval = graph->rowval;

  ncolors = gk_imax(rowptr[nrows], color)+1;

  sgraphs = (gk_graph_t **)gk_malloc(sizeof(gk_graph_t *)*ncolors, "gk_graph_Split: sgraphs");
  for (i=0; i<ncolors; i++) {
    sgraphs[i] = gk_graph_Create();
    sgraphs[i]->nrows  = graph->nrows;
    sgraphs[i]->ncols  = graph->ncols;
    sgraphs[i]->rowptr = gk_zsmalloc(nrows+1, 0, "gk_graph_Split: sgraphs[i]->rowptr"); 
  }

  for (i=0; i<nrows; i++) {
    for (j=rowptr[i]; j<rowptr[i+1]; j++) 
      sgraphs[color[j]]->rowptr[i]++;
  }
  for (i=0; i<ncolors; i++) 
    MAKECSR(j, nrows, sgraphs[i]->rowptr);

  for (i=0; i<ncolors; i++) {
    sgraphs[i]->rowind = gk_imalloc(sgraphs[i]->rowptr[nrows], "gk_graph_Split: sgraphs[i]->rowind"); 
    sgraphs[i]->rowval = gk_fmalloc(sgraphs[i]->rowptr[nrows], "gk_graph_Split: sgraphs[i]->rowval"); 
  }

  for (i=0; i<nrows; i++) {
    for (j=rowptr[i]; j<rowptr[i+1]; j++) {
      sgraphs[color[j]]->rowind[sgraphs[color[j]]->rowptr[i]] = rowind[j];
      sgraphs[color[j]]->rowval[sgraphs[color[j]]->rowptr[i]] = rowval[j];
      sgraphs[color[j]]->rowptr[i]++;
    }
  }

  for (i=0; i<ncolors; i++) 
    SHIFTCSR(j, nrows, sgraphs[i]->rowptr);

  return sgraphs;
}


/*************************************************************************/
/*! Prunes certain rows/columns of the graphrix. The prunning takes place 
    by analyzing the row structure of the graphrix. The prunning takes place
    by removing rows/columns but it does not affect the numbering of the
    remaining rows/columns.
   
    \param graph the graphrix to be prunned,
    \param what indicates if the rows (GK_CSR_ROW) or the columns (GK_CSR_COL)
           of the graphrix will be prunned,
    \param minf is the minimum number of rows (columns) that a column (row) must
           be present in order to be kept,
    \param maxf is the maximum number of rows (columns) that a column (row) must
          be present at in order to be kept.
    \returns the prunned graphrix consisting only of its row-based structure. 
          The input graphrix is not modified. 
*/
/**************************************************************************/
gk_graph_t *gk_graph_Prune(gk_graph_t *graph, int what, int minf, int maxf)
{
  ssize_t i, j, nnz;
  int nrows, ncols;
  ssize_t *rowptr, *nrowptr;
  int *rowind, *nrowind, *collen;
  float *rowval, *nrowval;
  gk_graph_t *ngraph;

  ngraph = gk_graph_Create();
  
  nrows = ngraph->nrows = graph->nrows;
  ncols = ngraph->ncols = graph->ncols;

  rowptr = graph->rowptr;
  rowind = graph->rowind;
  rowval = graph->rowval;

  nrowptr = ngraph->rowptr = gk_zmalloc(nrows+1, "gk_graph_Prune: nrowptr");
  nrowind = ngraph->rowind = gk_imalloc(rowptr[nrows], "gk_graph_Prune: nrowind");
  nrowval = ngraph->rowval = gk_fmalloc(rowptr[nrows], "gk_graph_Prune: nrowval");


  switch (what) {
    case GK_CSR_COL:
      collen = gk_ismalloc(ncols, 0, "gk_graph_Prune: collen");

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
      gk_graph_Free(&ngraph);
      gk_errexit(SIGERR, "Unknown prunning type of %d\n", what);
      return NULL;
  }

  return ngraph;
}



/*************************************************************************/
/*! Normalizes the rows/columns of the graphrix to be unit 
    length.
    \param graph the graphrix itself,
    \param what indicates what will be normalized and is obtained by
           specifying GK_CSR_ROW, GK_CSR_COL, GK_CSR_ROW|GK_CSR_COL. 
    \param norm indicates what norm is to normalize to, 1: 1-norm, 2: 2-norm
*/
/**************************************************************************/
void gk_graph_Normalize(gk_graph_t *graph, int what, int norm)
{
  ssize_t i, j;
  int n;
  ssize_t *ptr;
  float *val, sum;

  if (what&GK_CSR_ROW && graph->rowval) {
    n   = graph->nrows;
    ptr = graph->rowptr;
    val = graph->rowval;

    #pragma omp parallel if (ptr[n] > OMPMINOPS) 
    {
      #pragma omp for private(j,sum) schedule(static)
      for (i=0; i<n; i++) {
        for (sum=0.0, j=ptr[i]; j<ptr[i+1]; j++){
  	if (norm == 2)
  	  sum += val[j]*val[j];
  	else if (norm == 1)
  	  sum += val[j]; /* assume val[j] > 0 */ 
        }
        if (sum > 0) {
  	if (norm == 2)
  	  sum=1.0/sqrt(sum); 
  	else if (norm == 1)
  	  sum=1.0/sum; 
          for (j=ptr[i]; j<ptr[i+1]; j++)
            val[j] *= sum;
  	
        }
      }
    }
  }

  if (what&GK_CSR_COL && graph->colval) {
    n   = graph->ncols;
    ptr = graph->colptr;
    val = graph->colval;

    #pragma omp parallel if (ptr[n] > OMPMINOPS)
    {
    #pragma omp for private(j,sum) schedule(static)
      for (i=0; i<n; i++) {
        for (sum=0.0, j=ptr[i]; j<ptr[i+1]; j++)
  	if (norm == 2)
  	  sum += val[j]*val[j];
  	else if (norm == 1)
  	  sum += val[j]; 
        if (sum > 0) {
  	if (norm == 2)
  	  sum=1.0/sqrt(sum); 
  	else if (norm == 1)
  	  sum=1.0/sum; 
          for (j=ptr[i]; j<ptr[i+1]; j++)
            val[j] *= sum;
        }
      }
    }
  }
}


#endif
