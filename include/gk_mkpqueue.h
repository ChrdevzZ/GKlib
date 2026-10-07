/*!
\file  gk_mkpqueue.h
\brief Templates for priority queues

\date   Started 4/09/07
\author George
\version\verbatim $Id: gk_mkpqueue.h 21742 2018-01-26 16:59:15Z karypis $ \endverbatim
*/


#ifndef _GK_MKPQUEUE_H
#define _GK_MKPQUEUE_H


#define GK_MKPQUEUE(FPRFX, PQT, KVT, KT, VT, KVMALLOC, KMAX, KEY_LT)\
/*************************************************************************/\
/*! This function creates and initializes a priority queue */\
/**************************************************************************/\
PQT *FPRFX ## Create(size_t maxnodes)\
{\
  PQT *volatile queue;\
  void *cleanup_queue;\
  volatile int saved_errno;\
  volatile int signum=0;\
\
  queue = (PQT *)gk_malloc(sizeof(PQT), (char *)"gk_pqCreate: queue");\
  if (queue == NULL)\
    return NULL;\
  memset(queue, 0, sizeof(PQT));\
  if (!gk_sigtrap()) {\
    saved_errno = errno;\
    signum = saved_errno == ENOMEM || saved_errno == EOVERFLOW ? SIGMEM : SIGERR;\
    cleanup_queue = (PQT *)queue;\
    gk_free(&cleanup_queue, LTERM);\
    queue = (PQT *)cleanup_queue;\
    errno = saved_errno;\
    gk_errexit(signum, "gk_pqCreate: signal trap unavailable");\
    errno = saved_errno;\
    return NULL;\
  }\
  switch (gk_sigcatch()) {\
    case 0:\
      break;\
    case SIGMEM:\
      signum = SIGMEM;\
      break;\
    default:\
      signum = SIGERR;\
      break;\
  }\
  if (signum == 0)\
    FPRFX ## Init((PQT *)queue, maxnodes);\
  saved_errno = errno != 0 ? errno : ENOMEM;\
  if (!gk_siguntrap())\
    _Exit(EXIT_FAILURE);\
  if (queue->heap == NULL || queue->locator == NULL) {\
    FPRFX ## Free((PQT *)queue);\
    cleanup_queue = (PQT *)queue;\
    gk_free(&cleanup_queue, LTERM);\
    queue = (PQT *)cleanup_queue;\
    errno = saved_errno;\
  }\
  if (signum != 0) {\
    errno = saved_errno;\
    gk_sigthrow(signum);\
    errno = saved_errno;\
  }\
\
  return (PQT *)queue;\
}\
\
\
/*************************************************************************/\
/*! This function initializes the data structures of the priority queue */\
/**************************************************************************/\
void FPRFX ## Init(PQT *queue, size_t maxnodes)\
{\
  volatile int saved_errno;\
  volatile int signum=0;\
  PQT *volatile vqueue=queue;\
\
  if (queue == NULL) {\
    errno = EINVAL;\
    gk_errexit(SIGERR, "gk_pqInit: invalid queue");\
    errno = EINVAL;\
    return;\
  }\
  if (maxnodes > (size_t)PTRDIFF_MAX) {\
    memset(queue, 0, sizeof(PQT));\
    errno = EOVERFLOW;\
    gk_errexit(SIGMEM, "gk_PQInit: size overflow");\
    errno = EOVERFLOW;\
    return;\
  }\
  queue->nnodes = 0;\
  queue->maxnodes = maxnodes;\
  queue->heap = NULL;\
  queue->locator = NULL;\
\
  if (!gk_sigtrap()) {\
    saved_errno = errno;\
    signum = saved_errno == ENOMEM || saved_errno == EOVERFLOW ? SIGMEM : SIGERR;\
    queue->maxnodes = 0;\
    errno = saved_errno;\
    gk_errexit(signum, "gk_PQInit: signal trap unavailable");\
    errno = saved_errno;\
    return;\
  }\
  switch (gk_sigcatch()) {\
    case 0:\
      break;\
    case SIGMEM:\
      signum = SIGMEM;\
      break;\
    default:\
      signum = SIGERR;\
      break;\
  }\
  if (signum == 0) {\
    queue->heap    = KVMALLOC(maxnodes, (char *)"gk_PQInit: heap");\
    if (queue->heap != NULL)\
      queue->locator = gk_idxsmalloc(maxnodes, -1, (char *)"gk_PQInit: locator");\
  }\
  if (signum != 0 || vqueue->heap == NULL || vqueue->locator == NULL) {\
    saved_errno = errno != 0 ? errno : ENOMEM;\
    gk_free((void **)&vqueue->heap, LTERM);\
    vqueue->maxnodes = 0;\
    errno = saved_errno;\
  }\
  if (!gk_siguntrap())\
    _Exit(EXIT_FAILURE);\
  if (signum != 0) {\
    errno = saved_errno;\
    gk_sigthrow(signum);\
    errno = saved_errno;\
  }\
}\
\
\
/*************************************************************************/\
/*! This function resets the priority queue */\
/**************************************************************************/\
void FPRFX ## Reset(PQT *queue)\
{\
  size_t i;\
  ssize_t *locator=queue->locator;\
  KVT *heap=queue->heap;\
\
  for (i=queue->nnodes; i>0; i--)\
    locator[heap[i-1].val] = -1;\
  queue->nnodes = 0;\
}\
\
\
/*************************************************************************/\
/*! This function frees the internal datastructures of the priority queue */\
/**************************************************************************/\
void FPRFX ## Free(PQT *queue)\
{\
  if (queue == NULL) return;\
  gk_free((void **)&queue->heap, &queue->locator, LTERM);\
  queue->maxnodes = 0;\
}\
\
\
/*************************************************************************/\
/*! This function frees the internal datastructures of the priority queue \
    and the queue itself */\
/**************************************************************************/\
void FPRFX ## Destroy(PQT *queue)\
{\
  if (queue == NULL) return;\
  FPRFX ## Free(queue);\
  gk_free((void **)&queue, LTERM);\
}\
\
\
/*************************************************************************/\
/*! This function returns the length of the queue */\
/**************************************************************************/\
size_t FPRFX ## Length(PQT *queue)\
{\
  return queue->nnodes;\
}\
\
\
/*************************************************************************/\
/*! This function adds an item in the priority queue */\
/**************************************************************************/\
int FPRFX ## Insert(PQT *queue, VT node, KT key)\
{\
  size_t i, j;\
  ssize_t *locator=queue->locator;\
  KVT *heap=queue->heap;\
\
  ASSERT2(FPRFX ## CheckHeap(queue));\
\
  ASSERT(locator[node] == -1);\
\
  i = queue->nnodes++;\
  while (i > 0) {\
    j = (i-1)>>1;\
    if (KEY_LT(key, heap[j].key)) {\
      heap[i] = heap[j];\
      locator[heap[i].val] = (ssize_t)i;\
      i = j;\
    }\
    else\
      break;\
  }\
  heap[i].key   = key;\
  heap[i].val   = node;\
  locator[node] = (ssize_t)i;\
\
  ASSERT2(FPRFX ## CheckHeap(queue));\
\
  return 0;\
}\
\
\
/*************************************************************************/\
/*! This function deletes an item from the priority queue */\
/**************************************************************************/\
int FPRFX ## Delete(PQT *queue, VT node)\
{\
  size_t i, j;\
  size_t nnodes;\
  KT newkey, oldkey;\
  ssize_t *locator=queue->locator;\
  KVT *heap=queue->heap;\
\
  ASSERT(locator[node] != -1);\
  ASSERT(heap[locator[node]].val == node);\
\
  ASSERT2(FPRFX ## CheckHeap(queue));\
\
  i = (size_t)locator[node];\
  locator[node] = -1;\
\
  if (--queue->nnodes > 0 && heap[queue->nnodes].val != node) {\
    node   = heap[queue->nnodes].val;\
    newkey = heap[queue->nnodes].key;\
    oldkey = heap[i].key;\
\
    if (KEY_LT(newkey, oldkey)) { /* Filter-up */\
      while (i > 0) {\
        j = (i-1)>>1;\
        if (KEY_LT(newkey, heap[j].key)) {\
          heap[i] = heap[j];\
          locator[heap[i].val] = (ssize_t)i;\
          i = j;\
        }\
        else\
          break;\
      }\
    }\
    else { /* Filter down */\
      nnodes = queue->nnodes;\
      while ((j=(i<<1)+1) < nnodes) {\
        if (KEY_LT(heap[j].key, newkey)) {\
          if (j+1 < nnodes && KEY_LT(heap[j+1].key, heap[j].key))\
            j++;\
          heap[i] = heap[j];\
          locator[heap[i].val] = (ssize_t)i;\
          i = j;\
        }\
        else if (j+1 < nnodes && KEY_LT(heap[j+1].key, newkey)) {\
          j++;\
          heap[i] = heap[j];\
          locator[heap[i].val] = (ssize_t)i;\
          i = j;\
        }\
        else\
          break;\
      }\
    }\
\
    heap[i].key   = newkey;\
    heap[i].val   = node;\
    locator[node] = (ssize_t)i;\
  }\
\
  ASSERT2(FPRFX ## CheckHeap(queue));\
\
  return 0;\
}\
\
\
/*************************************************************************/\
/*! This function updates the key values associated for a particular item */ \
/**************************************************************************/\
void FPRFX ## Update(PQT *queue, VT node, KT newkey)\
{\
  size_t i, j;\
  size_t nnodes;\
  KT oldkey;\
  ssize_t *locator=queue->locator;\
  KVT *heap=queue->heap;\
\
  oldkey = heap[locator[node]].key;\
  if (!KEY_LT(newkey, oldkey) && !KEY_LT(oldkey, newkey)) return;\
\
  ASSERT(locator[node] != -1);\
  ASSERT(heap[locator[node]].val == node);\
  ASSERT2(FPRFX ## CheckHeap(queue));\
\
  i = (size_t)locator[node];\
\
  if (KEY_LT(newkey, oldkey)) { /* Filter-up */\
    while (i > 0) {\
      j = (i-1)>>1;\
      if (KEY_LT(newkey, heap[j].key)) {\
        heap[i] = heap[j];\
        locator[heap[i].val] = (ssize_t)i;\
        i = j;\
      }\
      else\
        break;\
    }\
  }\
  else { /* Filter down */\
    nnodes = queue->nnodes;\
    while ((j=(i<<1)+1) < nnodes) {\
      if (KEY_LT(heap[j].key, newkey)) {\
        if (j+1 < nnodes && KEY_LT(heap[j+1].key, heap[j].key))\
          j++;\
        heap[i] = heap[j];\
        locator[heap[i].val] = (ssize_t)i;\
        i = j;\
      }\
      else if (j+1 < nnodes && KEY_LT(heap[j+1].key, newkey)) {\
        j++;\
        heap[i] = heap[j];\
        locator[heap[i].val] = (ssize_t)i;\
        i = j;\
      }\
      else\
        break;\
    }\
  }\
\
  heap[i].key   = newkey;\
  heap[i].val   = node;\
  locator[node] = (ssize_t)i;\
\
  ASSERT2(FPRFX ## CheckHeap(queue));\
\
  return;\
}\
\
\
/*************************************************************************/\
/*! This function returns the item at the top of the queue and removes\
    it from the priority queue */\
/**************************************************************************/\
VT FPRFX ## GetTop(PQT *queue)\
{\
  size_t i, j;\
  ssize_t *locator;\
  KVT *heap;\
  VT vtx, node;\
  KT key;\
\
  ASSERT2(FPRFX ## CheckHeap(queue));\
\
  if (queue->nnodes == 0)\
    return -1;\
\
  queue->nnodes--;\
\
  heap    = queue->heap;\
  locator = queue->locator;\
\
  vtx = heap[0].val;\
  locator[vtx] = -1;\
\
  if ((i = queue->nnodes) > 0) {\
    key  = heap[i].key;\
    node = heap[i].val;\
    i = 0;\
    while ((j=2*i+1) < queue->nnodes) {\
      if (KEY_LT(heap[j].key, key)) {\
        if (j+1 < queue->nnodes && KEY_LT(heap[j+1].key, heap[j].key))\
          j = j+1;\
        heap[i] = heap[j];\
        locator[heap[i].val] = (ssize_t)i;\
        i = j;\
      }\
      else if (j+1 < queue->nnodes && KEY_LT(heap[j+1].key, key)) {\
        j = j+1;\
        heap[i] = heap[j];\
        locator[heap[i].val] = (ssize_t)i;\
        i = j;\
      }\
      else\
        break;\
    }\
\
    heap[i].key   = key;\
    heap[i].val   = node;\
    locator[node] = (ssize_t)i;\
  }\
\
  ASSERT2(FPRFX ## CheckHeap(queue));\
  return vtx;\
}\
\
\
/*************************************************************************/\
/*! This function returns the item at the top of the queue. The item is not\
    deleted from the queue. */\
/**************************************************************************/\
VT FPRFX ## SeeTopVal(PQT *queue)\
{\
  return (queue->nnodes == 0 ? -1 : queue->heap[0].val);\
}\
\
\
/*************************************************************************/\
/*! This function returns the key of the top item. The item is not\
    deleted from the queue. */\
/**************************************************************************/\
KT FPRFX ## SeeTopKey(PQT *queue)\
{\
  return (queue->nnodes == 0 ? KMAX : queue->heap[0].key);\
}\
\
\
/*************************************************************************/\
/*! This function returns the key of a specific item */\
/**************************************************************************/\
KT FPRFX ## SeeKey(PQT *queue, VT node)\
{\
  ssize_t *locator;\
  KVT *heap;\
\
  heap    = queue->heap;\
  locator = queue->locator;\
\
  return heap[locator[node]].key;\
}\
\
\
/*************************************************************************/\
/*! This function returns the first item in a breadth-first traversal of\
    the heap whose key is less than maxwgt. This function is here due to\
    hMETIS and is not general!*/\
/**************************************************************************/\
/*\
VT FPRFX ## SeeConstraintTop(PQT *queue, KT maxwgt, KT *wgts)\
{\
  ssize_t i;\
\
  if (queue->nnodes == 0)\
    return -1;\
\
  if (maxwgt <= 1000)\
    return FPRFX ## SeeTopVal(queue);\
\
  for (i=0; i<queue->nnodes; i++) {\
    if (queue->heap[i].key > 0) {\
      if (wgts[queue->heap[i].val] <= maxwgt)\
        return queue->heap[i].val;\
    }\
    else {\
      if (queue->heap[i/2].key <= 0)\
        break;\
    }\
  }\
\
  return queue->heap[0].val;\
\
}\
*/\
\
\
/*************************************************************************/\
/*! This functions checks the consistency of the heap */\
/**************************************************************************/\
int FPRFX ## CheckHeap(PQT *queue)\
{\
  size_t i, j;\
  size_t nnodes;\
  ssize_t *locator;\
  KVT *heap;\
\
  heap    = queue->heap;\
  locator = queue->locator;\
  nnodes  = queue->nnodes;\
  (void)heap;\
\
  if (nnodes == 0)\
    return 1;\
\
  ASSERT(locator[heap[0].val] == 0);\
  for (i=1; i<nnodes; i++) {\
    ASSERT((size_t)locator[heap[i].val] == i);\
    ASSERT(!KEY_LT(heap[i].key, heap[(i-1)/2].key));\
  }\
  for (i=1; i<nnodes; i++)\
    ASSERT(!KEY_LT(heap[i].key, heap[0].key));\
\
  for (j=i=0; i<queue->maxnodes; i++) {\
    if (locator[i] != -1)\
      j++;\
  }\
  (void)j;\
  ASSERTP(j == nnodes, ("%jd %jd\n", (intmax_t)j, (intmax_t)nnodes));\
\
  return 1;\
}\


#define GK_MKPQUEUE_PROTO(FPRFX, PQT, KT, VT)\
  GK_MKPQUEUE_PROTO_EX(FPRFX, PQT, KT, VT, )

#define GK_MKPQUEUE_PROTO_EX(FPRFX, PQT, KT, VT, API)\
  API PQT *  FPRFX ## Create(size_t maxnodes);\
  API void   FPRFX ## Init(PQT *queue, size_t maxnodes);\
  API void   FPRFX ## Reset(PQT *queue);\
  API void   FPRFX ## Free(PQT *queue);\
  API void   FPRFX ## Destroy(PQT *queue);\
  API size_t FPRFX ## Length(PQT *queue);\
  API int    FPRFX ## Insert(PQT *queue, VT node, KT key);\
  API int    FPRFX ## Delete(PQT *queue, VT node);\
  API void   FPRFX ## Update(PQT *queue, VT node, KT newkey);\
  API VT     FPRFX ## GetTop(PQT *queue);\
  API VT     FPRFX ## SeeTopVal(PQT *queue);\
  API KT     FPRFX ## SeeTopKey(PQT *queue);\
  API KT     FPRFX ## SeeKey(PQT *queue, VT node);\
  API VT     FPRFX ## SeeConstraintTop(PQT *queue, KT maxwgt, KT *wgts);\
  API int    FPRFX ## CheckHeap(PQT *queue);\


/* This is how these macros are used
GK_MKPQUEUE(gk_dkvPQ, gk_dkvPQ_t, double, gk_idx_t, gk_dkvmalloc, DBL_MAX)
GK_MKPQUEUE_PROTO(gk_dkvPQ, gk_dkvPQ_t, double, gk_idx_t)
*/


#endif
