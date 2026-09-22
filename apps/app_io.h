/*!
\file app_io.h
\brief Transactional output finalization shared by GKlib applications.
*/

#ifndef _GK_APP_IO_H_
#define _GK_APP_IO_H_

#include "../src/io_internal.h"


/*************************************************************************/
/*! Opens a temporary application output beside its final destination.

    The destination remains untouched until gk_app_finish_output() commits a
    complete stream. The temporary path is returned to the caller for cleanup.
*/
/*************************************************************************/
static inline FILE *gk_app_open_output(const char *filename, const char *mode,
    char **r_tempname)
{
  return gk_open_output_file(filename, mode, r_tempname);
}


/*************************************************************************/
/*! Closes and commits a complete application output transaction. */
/*************************************************************************/
static inline int gk_app_finish_output(FILE *stream, char **r_tempname,
    const char *filename)
{
  return gk_finish_output_file(stream, r_tempname, filename, 1);
}

#endif
