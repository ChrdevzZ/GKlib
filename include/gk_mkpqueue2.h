/*!
\file  gk_mkpqueue2.h
\brief Templates for priority queues that do not utilize locators and as such
       they can use different types of values.

\date   Started 4/09/07
\author George
\version\verbatim $Id: gk_mkpqueue2.h 13005 2012-10-23 22:34:36Z karypis $ \endverbatim
*/


#ifndef _GK_MKPQUEUE2_H
#define _GK_MKPQUEUE2_H


#define GK_MKPQUEUE2(FPRFX, PQT, KT, VT, KMALLOC, VMALLOC, KMAX, KEY_LT)\
/*************************************************************************/\
/*! This function creates and initializes a priority queue */\
/**************************************************************************/\
PQT *FPRFX ## Create2(ssize_t maxnodes)\
{\
  PQT *volatile queue;\
  void *cleanup_queue;\
  volatile int saved_errno;\
  volatile int signum=0;\
\
  queue = (PQT *)gk_malloc(sizeof(PQT), (char *)"gk_pqCreate2: queue");\
  if (queue == NULL)\
    return NULL;\
  memset(queue, 0, sizeof(PQT));\
  queue->maxnodes = maxnodes;\
  if (!gk_sigtrap()) {\
    saved_errno = errno;\
    signum = saved_errno == ENOMEM || saved_errno == EOVERFLOW ? SIGMEM : SIGERR;\
    cleanup_queue = (PQT *)queue;\
    gk_free(&cleanup_queue, LTERM);\
    queue = (PQT *)cleanup_queue;\
    errno = saved_errno;\
    gk_errexit(signum, "gk_pqCreate2: signal trap unavailable");\
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
  if (signum == 0) {\
    queue->keys     = KMALLOC(maxnodes, (char *)"gk_pqCreate2: keys");\
    if (queue->keys != NULL)\
      queue->vals = VMALLOC(maxnodes, (char *)"gk_pqCreate2: vals");\
  }\
  saved_errno = errno != 0 ? errno : ENOMEM;\
  if (!gk_siguntrap())\
    _Exit(EXIT_FAILURE);\
  if (queue->keys == NULL || queue->vals == NULL) {\
    cleanup_queue = (PQT *)queue;\
    gk_free((void **)&queue->keys, &queue->vals, &cleanup_queue, LTERM);\
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
/*! This function resets the priority queue */\
/**************************************************************************/\
void FPRFX ## Reset2(PQT *queue)\
{\
  queue->nnodes = 0;\
}\
\
\
/*************************************************************************/\
/*! This function frees the internal datastructures of the priority queue */\
/**************************************************************************/\
void FPRFX ## Destroy2(PQT **r_queue)\
{\
  PQT *queue = *r_queue; \
  if (queue == NULL) return;\
  gk_free((void **)&queue->keys, &queue->vals, &queue, LTERM);\
  *r_queue = NULL;\
}\
\
\
/*************************************************************************/\
/*! This function returns the length of the queue */\
/**************************************************************************/\
size_t FPRFX ## Length2(PQT *queue)\
{\
  return queue->nnodes;\
}\
\
\
/*************************************************************************/\
/*! This function adds an item in the priority queue. */\
/**************************************************************************/\
int FPRFX ## Insert2(PQT *queue, VT val, KT key)\
{\
  ssize_t i, j;\
  KT *keys=queue->keys;\
  VT *vals=queue->vals;\
\
  ASSERT2(FPRFX ## CheckHeap2(queue));\
\
  if (queue->nnodes == queue->maxnodes) \
    return 0;\
\
  ASSERT2(FPRFX ## CheckHeap2(queue));\
\
  i = queue->nnodes++;\
  while (i > 0) {\
    j = (i-1)>>1;\
    if (KEY_LT(key, keys[j])) {\
      keys[i] = keys[j];\
      vals[i] = vals[j];\
      i = j;\
    }\
    else\
      break;\
  }\
  ASSERT(i >= 0);\
  keys[i] = key;\
  vals[i] = val;\
\
  ASSERT2(FPRFX ## CheckHeap2(queue));\
\
  return 1;\
}\
\
\
/*************************************************************************/\
/*! This function returns the item at the top of the queue and removes\
    it from the priority queue */\
/**************************************************************************/\
int FPRFX ## GetTop2(PQT *queue, VT *r_val)\
{\
  ssize_t i, j;\
  KT key, *keys=queue->keys;\
  VT val, *vals=queue->vals;\
\
  ASSERT2(FPRFX ## CheckHeap2(queue));\
\
  if (queue->nnodes == 0)\
    return 0;\
\
  queue->nnodes--;\
\
  *r_val = vals[0];\
\
  if ((i = queue->nnodes) > 0) {\
    key = keys[i];\
    val = vals[i];\
    i = 0;\
    while ((j=2*i+1) < queue->nnodes) {\
      if (KEY_LT(keys[j], key)) {\
        if (j+1 < queue->nnodes && KEY_LT(keys[j+1], keys[j]))\
          j = j+1;\
        keys[i] = keys[j];\
        vals[i] = vals[j];\
        i = j;\
      }\
      else if (j+1 < queue->nnodes && KEY_LT(keys[j+1], key)) {\
        j = j+1;\
        keys[i] = keys[j];\
        vals[i] = vals[j];\
        i = j;\
      }\
      else\
        break;\
    }\
\
    keys[i] = key;\
    vals[i] = val;\
  }\
\
  ASSERT2(FPRFX ## CheckHeap2(queue));\
\
  return 1;\
}\
\
\
/*************************************************************************/\
/*! This function returns the item at the top of the queue. The item is not\
    deleted from the queue. */\
/**************************************************************************/\
int FPRFX ## SeeTopVal2(PQT *queue, VT *r_val)\
{\
  if (queue->nnodes == 0) \
    return 0;\
\
  *r_val = queue->vals[0];\
\
  return 1;\
}\
\
\
/*************************************************************************/\
/*! This function returns the key of the top item. The item is not\
    deleted from the queue. */\
/**************************************************************************/\
KT FPRFX ## SeeTopKey2(PQT *queue)\
{\
  return (queue->nnodes == 0 ? KMAX : queue->keys[0]);\
}\
\
\
/*************************************************************************/\
/*! This functions checks the consistency of the heap */\
/**************************************************************************/\
int FPRFX ## CheckHeap2(PQT *queue)\
{\
  ssize_t i;\
  KT *keys=queue->keys;\
  (void)keys;\
\
  if (queue->nnodes == 0)\
    return 1;\
\
  for (i=1; i<queue->nnodes; i++) {\
    ASSERT(!KEY_LT(keys[i], keys[(i-1)/2]));\
  }\
  for (i=1; i<queue->nnodes; i++)\
    ASSERT(!KEY_LT(keys[i], keys[0]));\
\
  return 1;\
}\


#define GK_MKPQUEUE2_PROTO(FPRFX, PQT, KT, VT)\
  GK_MKPQUEUE2_PROTO_EX(FPRFX, PQT, KT, VT, )

#define GK_MKPQUEUE2_PROTO_EX(FPRFX, PQT, KT, VT, API)\
  API PQT *  FPRFX ## Create2(ssize_t maxnodes);\
  API void   FPRFX ## Reset2(PQT *queue);\
  API void   FPRFX ## Destroy2(PQT **r_queue);\
  API size_t FPRFX ## Length2(PQT *queue);\
  API int    FPRFX ## Insert2(PQT *queue, VT node, KT key);\
  API int    FPRFX ## GetTop2(PQT *queue, VT *r_val);\
  API int    FPRFX ## SeeTopVal2(PQT *queue, VT *r_val);\
  API KT     FPRFX ## SeeTopKey2(PQT *queue);\
  API int    FPRFX ## CheckHeap2(PQT *queue);\


#endif
