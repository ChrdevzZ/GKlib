/*!
\file gk_externs.h
\brief This file contains definitions of external variables created by GKlib

\date   Started 3/27/2007
\author George
\version\verbatim $Id: gk_externs.h 10711 2011-08-31 22:23:04Z karypis $ \endverbatim
*/

#ifndef _GK_EXTERNS_H_
#define _GK_EXTERNS_H_


/*************************************************************************
* Jump-buffer state uses the configured TLS policy and DLL accessors.
**************************************************************************/
#ifndef _GK_ERROR_C_
/* declared in error.c */
#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) && GKLIB_BUILD_SHARED_LIBS && !defined(GKLIB_STATIC_DEFINE)
GKLIB_EXPORT int *gk_cur_jbufs_address(void);
GKLIB_EXPORT jmp_buf *gk_jbufs_address(void);
GKLIB_EXPORT jmp_buf *gk_jbuf_address(void);

#define gk_cur_jbufs (*gk_cur_jbufs_address())
#define gk_jbufs     (gk_jbufs_address())
#define gk_jbuf      (*gk_jbuf_address())
#else
GKLIB_EXPORT extern GKLIB_THREAD_LOCAL int gk_cur_jbufs;
GKLIB_EXPORT extern GKLIB_THREAD_LOCAL jmp_buf gk_jbufs[];
GKLIB_EXPORT extern GKLIB_THREAD_LOCAL jmp_buf gk_jbuf;
#endif

#ifdef __cplusplus
}
#endif

#endif

#endif
