/*!
\file app_parse.h
\brief Strict command-line scalar parsers shared by GKlib applications.
*/

#ifndef _GK_APP_PARSE_H_
#define _GK_APP_PARSE_H_

/*************************************************************************/
/*! Parses one complete command-line integer in the range of int.

    Missing values, range errors, and trailing characters are reported through
    the applications' existing fatal-error path.
*/
/*************************************************************************/
static inline int gk_app_parse_int(const char *text, const char *name)
{
  char *endptr;
  long value;

  if (text == NULL || text[0] == '\0') {
    errexit("Missing integer value for %s.\n", name);
    return 0;
  }

  errno = 0;
  value = strtol(text, &endptr, 10);
  if (errno == ERANGE || value < INT_MIN || value > INT_MAX ||
      endptr == text || endptr[0] != '\0') {
    errexit("Invalid integer value for %s: %s.\n", name, text);
    return 0;
  }

  return (int)value;
}


/*************************************************************************/
/*! Parses one complete, finite command-line floating-point value. */
/*************************************************************************/
static inline float gk_app_parse_float(const char *text, const char *name)
{
  char *endptr;
  float value;

  if (text == NULL || text[0] == '\0') {
    errexit("Missing floating-point value for %s.\n", name);
    return 0.0f;
  }

  errno = 0;
  value = strtof(text, &endptr);
  if (errno == ERANGE || !isfinite(value) ||
      endptr == text || endptr[0] != '\0') {
    errexit("Invalid floating-point value for %s: %s.\n", name, text);
    return 0.0f;
  }

  return value;
}

#endif
