/*!
\file  gk_mkmemory.h
\brief Templates for memory allocation routines

\date   Started 3/29/07
\author George
\version\verbatim $Id: gk_mkmemory.h 10711 2011-08-31 22:23:04Z karypis $ \endverbatim
*/

#ifndef _GK_MKMEMORY_H_
#define _GK_MKMEMORY_H_

#include <stdint.h>


#define GK_MKALLOC(PRFX, TYPE)\
/*************************************************************************/\
/*! The macro for gk_?malloc()-class of routines */\
/**************************************************************************/\
TYPE *PRFX ## malloc(size_t n, const char *msg)\
{\
  if (n > SIZE_MAX/sizeof(TYPE)) { \
    errno = EOVERFLOW; \
    gk_errexit(SIGMEM, "***Memory allocation size overflow."); \
    errno = EOVERFLOW; \
    return NULL; \
  } \
  return (TYPE *)gk_malloc(n*sizeof(TYPE), msg);\
}\
\
\
/*************************************************************************/\
/*! The macro for gk_?realloc()-class of routines */\
/**************************************************************************/\
TYPE *PRFX ## realloc(TYPE *ptr, size_t n, const char *msg)\
{\
  if (n > SIZE_MAX/sizeof(TYPE)) { \
    errno = EOVERFLOW; \
    gk_errexit(SIGMEM, "***Memory reallocation size overflow."); \
    errno = EOVERFLOW; \
    return NULL; \
  } \
  return (TYPE *)gk_realloc((void *)ptr, n*sizeof(TYPE), msg);\
}\
\
\
/*************************************************************************/\
/*! The macro for gk_?smalloc()-class of routines */\
/**************************************************************************/\
TYPE *PRFX ## smalloc(size_t n, TYPE ival, const char *msg)\
{\
  TYPE *ptr;\
\
  if (n > SIZE_MAX/sizeof(TYPE)) { \
    errno = EOVERFLOW; \
    gk_errexit(SIGMEM, "***Memory allocation size overflow."); \
    errno = EOVERFLOW; \
    return NULL; \
  } \
  ptr = (TYPE *)gk_malloc(n*sizeof(TYPE), msg);\
  if (ptr == NULL) \
    return NULL; \
\
  return PRFX ## set(n, ival, ptr); \
}\
\
\
/*************************************************************************/\
/*! The macro for gk_?set()-class of routines */\
/*************************************************************************/\
TYPE *PRFX ## set(size_t n, TYPE val, TYPE *x)\
{\
  size_t i;\
\
  for (i=0; i<n; i++)\
    x[i] = val;\
\
  return x;\
}\
\
\
/*************************************************************************/\
/*! The macro for gk_?set()-class of routines */\
/*************************************************************************/\
TYPE *PRFX ## copy(size_t n, TYPE *a, TYPE *b)\
{\
  if (n > SIZE_MAX/sizeof(TYPE)) { \
    errno = EOVERFLOW; \
    gk_errexit(SIGMEM, "***Memory copy size overflow."); \
    errno = EOVERFLOW; \
    return NULL; \
  } \
  return (TYPE *)memmove((void *)b, (void *)a, n*sizeof(TYPE));\
}\
\
\
/*************************************************************************/\
/*! The macro for gk_?AllocMatrix()-class of routines */\
/**************************************************************************/\
TYPE **PRFX ## AllocMatrix(size_t ndim1, size_t ndim2, TYPE value, const char *errmsg)\
{\
  size_t i;\
  TYPE **matrix=NULL;\
\
  (void)errmsg; \
\
  if (ndim1 > SIZE_MAX/sizeof(TYPE *) || \
      (ndim1 != 0 && ndim2 > SIZE_MAX/sizeof(TYPE))) { \
    errno = EOVERFLOW; \
    gk_errexit(SIGMEM, "***Matrix allocation size overflow."); \
    errno = EOVERFLOW; \
    return NULL; \
  } \
  gk_AllocMatrix((void ***)&matrix, sizeof(TYPE), ndim1, ndim2); \
  if (matrix == NULL) \
    return NULL;\
\
  for (i=0; i<ndim1; i++) \
    PRFX ## set(ndim2, value, matrix[i]); \
\
  return matrix;\
}\
\
\
/*************************************************************************/\
/*! The macro for gk_?AllocMatrix()-class of routines */\
/**************************************************************************/\
void PRFX ## FreeMatrix(TYPE ***r_matrix, size_t ndim1, size_t ndim2)\
{\
  size_t i;\
  TYPE **matrix;\
\
  (void)ndim2; \
\
  if (r_matrix == NULL) { \
    errno = EINVAL; \
    gk_errexit(SIGERR, "***Matrix output pointer is NULL."); \
    errno = EINVAL; \
    return; \
  } \
  if (*r_matrix == NULL) \
    return; \
\
  matrix = *r_matrix;\
\
  for (i=0; i<ndim1; i++) \
    gk_free((void **)&(matrix[i]), LTERM);\
\
  gk_free((void **)r_matrix, LTERM);\
}\
\
\
/*************************************************************************/\
/*! The macro for gk_?SetMatrix()-class of routines */\
/**************************************************************************/\
void PRFX ## SetMatrix(TYPE **matrix, size_t ndim1, size_t ndim2, TYPE value)\
{\
  size_t i, j;\
\
  for (i=0; i<ndim1; i++) {\
    for (j=0; j<ndim2; j++)\
      matrix[i][j] = value;\
  }\
}\


#define GK_MKALLOC_PROTO(PRFX, TYPE)\
  GK_MKALLOC_PROTO_EX(PRFX, TYPE, )

#define GK_MKALLOC_PROTO_EX(PRFX, TYPE, API)\
  API TYPE  *PRFX ## malloc(size_t n, const char *msg);\
  API TYPE  *PRFX ## realloc(TYPE *ptr, size_t n, const char *msg);\
  API TYPE  *PRFX ## smalloc(size_t n, TYPE ival, const char *msg);\
  API TYPE  *PRFX ## set(size_t n, TYPE val, TYPE *x);\
  API TYPE  *PRFX ## copy(size_t n, TYPE *a, TYPE *b);\
  API TYPE **PRFX ## AllocMatrix(size_t ndim1, size_t ndim2, TYPE value, const char *errmsg);\
  API void   PRFX ## FreeMatrix(TYPE ***r_matrix, size_t ndim1, size_t ndim2);\
  API void   PRFX ## SetMatrix(TYPE **matrix, size_t ndim1, size_t ndim2, TYPE value);\



#endif
