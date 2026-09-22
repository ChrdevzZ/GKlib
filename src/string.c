/************************************************************************/
/*! \file 

\brief Functions for manipulating strings.

Various functions for manipulating strings. Some of these functions 
provide new functionality, whereas others are drop-in replacements
of standard functions (but with enhanced functionality).

\date Started 11/1/99
\author George
\version $Id: string.c 14330 2013-05-18 12:15:15Z karypis $
*/
/************************************************************************/

/* the following is for strptime() */
#define _XOPEN_SOURCE
#include <time.h>
#undef _XOPEN_SOURCE

#include <GKlib.h>
#include "memory_internal.h"



/************************************************************************/
/*! \brief Replaces certain characters in a string.
 
This function takes a string and replaces all the characters in the
\c fromlist with the corresponding characters from the \c tolist. 
That is, each occurence of <tt>fromlist[i]</tt> is replaced by 
<tt>tolist[i]</tt>. 
If the \c tolist is shorter than \c fromlist, then the corresponding 
characters are deleted. The modifications on \c str are done in place. 
It tries to provide a functionality similar to Perl's \b tr// function.

\param str is the string whose characters will be replaced.
\param fromlist is the set of characters to be replaced.
\param tolist is the set of replacement characters .
\returns A pointer to \c str itself.
*/
/************************************************************************/
char *gk_strchr_replace(char *str, char *fromlist, char *tolist)
{
  ssize_t i, j, k, len, fromlen, tolen;

  len     = strlen(str);
  fromlen = strlen(fromlist);
  tolen   = strlen(tolist);

  for (i=j=0; i<len; i++) {
    for (k=0; k<fromlen; k++) {
      if (str[i] == fromlist[k]) {
        if (k < tolen) 
          str[j++] = tolist[k];
        break;
      }
    }
    if (k == fromlen)
      str[j++] = str[i];
  }
  str[j] = '\0';

  return str;
}



/************************************************************************/
/*! Ensures that a string builder can append the requested byte count.

    Capacity growth is geometric and checked before arithmetic. Reallocation
    is transactional: a failure leaves the original pointer and capacity
    unchanged so the caller can release the old buffer.
*/
/************************************************************************/
static int gk_strbuf_reserve(char **buffer, size_t *capacity,
    size_t used, size_t extra)
{
  char *new_buffer;
  size_t new_capacity, required;

  if (used == SIZE_MAX || extra > SIZE_MAX-used-1) {
    errno = EOVERFLOW;
    return 0;
  }
  required = used+extra+1;
  if (required <= *capacity)
    return 1;

  new_capacity = *capacity;
  while (new_capacity < required) {
    if (new_capacity > SIZE_MAX/2) {
      new_capacity = required;
      break;
    }
    new_capacity *= 2;
  }

  new_buffer = (char *)gk_realloc_nosignal(*buffer, new_capacity);
  if (new_buffer == NULL) {
    if (errno == 0)
      errno = ENOMEM;
    return 0;
  }

  *buffer = new_buffer;
  *capacity = new_capacity;
  return 1;
}


/************************************************************************/
/*! Releases all regex-replacement state before reporting an error. */
/************************************************************************/
static int gk_strstr_replace_error(regex_t *regex, char **new_str,
    int signum, const char *message)
{
  int saved_errno=errno != 0 ? errno :
      (signum == SIGMEM ? ENOMEM : EINVAL);

  gk_free((void **)new_str, LTERM);
  regfree(regex);
  errno = saved_errno;
  gk_errexit(signum, "%s", message);
  errno = saved_errno;
  return 0;
}


/************************************************************************/
/*! Duplicates a diagnostic without signaling before regex cleanup. */
/************************************************************************/
static char *gk_strstr_error_message(const char *message)
{
  char *copy;
  size_t length=strlen(message);

  if (length == SIZE_MAX) {
    errno = EOVERFLOW;
    return NULL;
  }
  copy = (char *)gk_malloc_nosignal(length+1);
  if (copy != NULL)
    memcpy(copy, message, length+1);
  else if (errno == 0)
    errno = ENOMEM;

  return copy;
}


/************************************************************************/
/*! \brief Regex-based search-and-replace function
 
This function is a C implementation of Perl's <tt> s//</tt> regular-expression
based substitution function.

\param str
  is the non-NULL input string on which the operation will be performed.
\param pattern
  is the non-NULL regular expression for the pattern to be matched.
\param replacement
  is the non-NULL replacement string, in which the possible captured pattern
  substrings are referred to as $1, $2, ..., $9. The entire matched pattern
  is referred to as $0.
\param options
  is a non-NULL string specifying options for the substitution operation.
  Currently <tt>"i"</tt> (case insensitive) and <tt>"g"</tt> (global
  substitution) are supported. Global substitution advances past one input
  byte after an empty match, retaining that byte, and stops after an empty
  match at the end.
\param new_str
  is a non-NULL output pointer that receives the newly allocated result
  string. It is allocated by gk_malloc() and must be freed with gk_free().
  The string is returned even when no substitutions were performed.
\returns 1 plus the number of substitutions on success, or 0 on error. A
         regular-expression diagnostic may be returned in \c new_str when
         compilation fails and must also be freed with gk_free().
*/
/************************************************************************/
int gk_strstr_replace(char *str, char *pattern, char *replacement, char *options,
      char **new_str)
{
  size_t i, len, rlen, capacity, offset, noffset, count;
  int j, rc, flags, global, nmatches;
  regex_t re;
  regmatch_t matches[10];

  if (new_str == NULL) {
    errno = EINVAL;
    gk_errexit(SIGERR, "gk_strstr_replace: output pointer must not be NULL");
    errno = EINVAL;
    return 0;
  }
  *new_str = NULL;
  if (str == NULL || pattern == NULL || replacement == NULL ||
      options == NULL) {
    errno = EINVAL;
    gk_errexit(SIGERR, "gk_strstr_replace: input strings must not be NULL");
    errno = EINVAL;
    return 0;
  }

  /* Parse the options */
  flags = REG_EXTENDED;
  if (strchr(options, 'i') != NULL)
    flags = flags | REG_ICASE;
  global = (strchr(options, 'g') != NULL ? 1 : 0);

  /* Compile the regex */
  if ((rc = regcomp(&re, pattern, flags)) != 0) {
    len = regerror(rc, &re, NULL, 0);
    *new_str = (char *)gk_malloc_nosignal(len);
    if (*new_str != NULL)
      regerror(rc, &re, *new_str, len);
    else {
      if (errno == 0)
        errno = ENOMEM;
      gk_errexit(SIGMEM,
          "gk_strstr_replace: failed to allocate regex diagnostic");
    }
    return 0;
  }

  /* Prepare the output string. */
  len = strlen(str);
  if (len == SIZE_MAX) {
    errno = EOVERFLOW;
    return gk_strstr_replace_error(&re, new_str, SIGMEM,
        "gk_strstr_replace: input size overflow");
  }
  capacity = (len <= (SIZE_MAX-1)/2 ? 2*len+1 : len+1);
  *new_str = (char *)gk_malloc_nosignal(capacity);
  if (*new_str == NULL) {
    if (errno == 0)
      errno = ENOMEM;
    return gk_strstr_replace_error(&re, new_str, SIGMEM,
        "gk_strstr_replace: failed to allocate output");
  }

  rlen = strlen(replacement);
  offset = 0;
  noffset = 0;
  nmatches = 0;
  do {
    rc = regexec(&re, str+offset, 10, matches,
        (offset > 0 ? REG_NOTBOL : 0));

    if (rc != 0 && rc != REG_NOMATCH) {
      gk_free((void **)new_str, LTERM);
      len = regerror(rc, &re, NULL, 0);
      *new_str = (char *)gk_malloc_nosignal(len);
      if (*new_str != NULL)
        regerror(rc, &re, *new_str, len);
      else {
        if (errno == 0)
          errno = ENOMEM;
        return gk_strstr_replace_error(&re, new_str, SIGMEM,
            "gk_strstr_replace: failed to allocate regex diagnostic");
      }
      regfree(&re);
      return 0;
    }
    else if (rc == REG_NOMATCH) {
      count = len-offset;
      if (!gk_strbuf_reserve(new_str, &capacity, noffset, count)) {
        return gk_strstr_replace_error(&re, new_str, SIGMEM,
            "gk_strstr_replace: failed to grow output");
      }
      memcpy(*new_str+noffset, str+offset, count);
      noffset += count;
      break;
    }
    else {
      if (matches[0].rm_so < 0 || matches[0].rm_eo < matches[0].rm_so ||
          (size_t)matches[0].rm_eo > len-offset) {
        errno = EINVAL;
        return gk_strstr_replace_error(&re, new_str, SIGERR,
            "gk_strstr_replace: invalid regex match range");
      }
      if (nmatches == INT_MAX-1) {
        errno = EOVERFLOW;
        return gk_strstr_replace_error(&re, new_str, SIGMEM,
            "gk_strstr_replace: too many matches");
      }
      nmatches++;

      count = (size_t)matches[0].rm_so;
      if (!gk_strbuf_reserve(new_str, &capacity, noffset, count)) {
        return gk_strstr_replace_error(&re, new_str, SIGMEM,
            "gk_strstr_replace: failed to grow output");
      }
      memcpy(*new_str+noffset, str+offset, count);
      noffset += count;

      /* Append the replacement string. */
      for (i=0; i<rlen; i++) {
        switch (replacement[i]) {
          case '\\':
            if (i+1 < rlen) {
              if (!gk_strbuf_reserve(new_str, &capacity, noffset, 1)) {
                return gk_strstr_replace_error(&re, new_str, SIGMEM,
                    "gk_strstr_replace: failed to grow output");
              }
              (*new_str)[noffset++] = replacement[++i];
            }
            else {
              gk_free((void **)new_str, LTERM);
              *new_str = gk_strstr_error_message(
                  "Error in replacement string. "
                  "Missing character following '\\'.");
              if (*new_str == NULL)
                return gk_strstr_replace_error(&re, new_str, SIGMEM,
                    "gk_strstr_replace: failed to allocate diagnostic");
              regfree(&re);
              return 0;
            }
            break;

          case '$':
            if (i+1 < rlen) {
              j = (int)(replacement[++i] - '0');
              if (j < 0 || j > 9) {
                gk_free((void **)new_str, LTERM);
                *new_str = gk_strstr_error_message(
                    "Error in captured subexpression specification.");
                if (*new_str == NULL)
                  return gk_strstr_replace_error(&re, new_str, SIGMEM,
                      "gk_strstr_replace: failed to allocate diagnostic");
                regfree(&re);
                return 0;
              }
              if (matches[j].rm_so < 0)
                break;
              if (matches[j].rm_eo < matches[j].rm_so ||
                  (size_t)matches[j].rm_eo > len-offset) {
                errno = EINVAL;
                return gk_strstr_replace_error(&re, new_str, SIGERR,
                    "gk_strstr_replace: invalid capture range");
              }

              count = (size_t)(matches[j].rm_eo-matches[j].rm_so);
              if (!gk_strbuf_reserve(new_str, &capacity, noffset, count)) {
                return gk_strstr_replace_error(&re, new_str, SIGMEM,
                    "gk_strstr_replace: failed to grow output");
              }
              memcpy(*new_str+noffset, str+offset+matches[j].rm_so, count);
              noffset += count;
            }
            else {
              gk_free((void **)new_str, LTERM);
              *new_str = gk_strstr_error_message(
                  "Error in replacement string. "
                  "Missing subexpression number following '$'.");
              if (*new_str == NULL)
                return gk_strstr_replace_error(&re, new_str, SIGMEM,
                    "gk_strstr_replace: failed to allocate diagnostic");
              regfree(&re);
              return 0;
            }
            break;

          default:
            if (!gk_strbuf_reserve(new_str, &capacity, noffset, 1)) {
              return gk_strstr_replace_error(&re, new_str, SIGMEM,
                  "gk_strstr_replace: failed to grow output");
            }
            (*new_str)[noffset++] = replacement[i];
        }
      }

      offset += (size_t)matches[0].rm_eo;

      /* Empty matches must make progress without discarding input text. */
      if (global && matches[0].rm_so == matches[0].rm_eo) {
        if (offset == len)
          break;
        if (!gk_strbuf_reserve(new_str, &capacity, noffset, 1)) {
          return gk_strstr_replace_error(&re, new_str, SIGMEM,
              "gk_strstr_replace: failed to grow output");
        }
        (*new_str)[noffset++] = str[offset++];
      }

      if (!global) {
        count = len-offset;
        if (!gk_strbuf_reserve(new_str, &capacity, noffset, count)) {
          return gk_strstr_replace_error(&re, new_str, SIGMEM,
              "gk_strstr_replace: failed to grow output");
        }
        memcpy(*new_str+noffset, str+offset, count);
        noffset += count;
      }
    }
  } while (global);

  (*new_str)[noffset] = '\0';
  regfree(&re);
  return nmatches + 1;
}



/************************************************************************/
/*! \brief Prunes characters from the end of the string.

This function removes any trailing characters that are included in the
\c rmlist. The trimming stops at the last character (i.e., first character 
from the end) that is not in \c rmlist.  
This function can be used to removed trailing spaces, newlines, etc.
This is a distructive operation as it modifies the string.

\param str is the string that will be trimmed.
\param rmlist contains the set of characters that will be removed.
\returns A pointer to \c str itself.
\sa gk_strhprune()
*/
/*************************************************************************/
char *gk_strtprune(char *str, char *rmlist)
{
  ssize_t i, j, len;

  len = strlen(rmlist);

  for (i=strlen(str)-1; i>=0; i--) {
    for (j=0; j<len; j++) {
      if (str[i] == rmlist[j])
        break;
    }
    if (j == len)
      break;
  }

  str[i+1] = '\0';

  return str;
}


/************************************************************************/
/*! \brief Prunes characters from the beginning of the string.

This function removes any starting characters that are included in the
\c rmlist. The trimming stops at the first character that is not in 
\c rmlist.
This function can be used to removed leading spaces, tabs, etc.
This is a distructive operation as it modifies the string.

\param str is the string that will be trimmed.
\param rmlist contains the set of characters that will be removed.
\returns A pointer to \c str itself.
\sa gk_strtprune()
*/
/*************************************************************************/
char *gk_strhprune(char *str, char *rmlist)
{
  ssize_t i, j, len;

  len = strlen(rmlist);

  for (i=0; str[i]; i++) {
    for (j=0; j<len; j++) {
      if (str[i] == rmlist[j])
        break;
    }
    if (j == len)
      break;
  }

  if (i>0) { /* If something needs to be removed */
    for (j=0; str[i]; i++, j++)
      str[j] = str[i];
    str[j] = '\0';
  }

  return str;
}


/************************************************************************/
/*! \brief Converts a string to upper case.

This function converts a string to upper case. This operation modifies the 
string itself.

\param str is the string whose case will be changed.
\returns A pointer to \c str itself.
\sa gk_strtolower()
*/
/*************************************************************************/
char *gk_strtoupper(char *str)
{
  int i;

  for (i=0; str[i]!='\0';
       str[i]=(char)toupper((unsigned char)str[i]), i++);
  return str;
}


/************************************************************************/
/*! \brief Converts a string to lower case.

This function converts a string to lower case. This operation modifies the 
string itself.

\param str is the string whose case will be changed.
\returns A pointer to \c str itself.
\sa gk_strtoupper()
*/
/*************************************************************************/
char *gk_strtolower(char *str)
{
  int i;

  for (i=0; str[i]!='\0';
       str[i]=(char)tolower((unsigned char)str[i]), i++);
  return str;
}


/************************************************************************/
/*! \brief Duplicates a string

This function is a replacement for C's standard <em>strdup()</em> function.
The key differences between the two are that gk_strdup():
  - uses the dynamic memory allocation routines of \e GKlib. 
  - it correctly handles NULL input strings.

The string that is returned must be freed by gk_free().

\param orgstr is the string that will be duplicated.
\returns A pointer to the newly created string.
\sa gk_free()
*/
/*************************************************************************/
char *gk_strdup(char *orgstr)
{
  size_t len;
  char *str=NULL;

  if (orgstr != NULL) {
    len = strlen(orgstr);
    if (len == SIZE_MAX) {
      gk_errexit(SIGMEM, "gk_strdup: string size overflow");
      return NULL;
    }
    len++;
    str = gk_malloc(len, "gk_strdup: str");
    if (str != NULL)
      memcpy(str, orgstr, len);
  }

  return str;
}


/************************************************************************/
/*! \brief Case insensitive string comparison.

This function compares two strings for equality by ignoring the case of the
strings. 

\warning This function is \b not equivalent to a case-insensitive 
         <em>strcmp()</em> function, as it does not return ordering 
         information.

\todo Remove the above warning.

\param s1 is the first string to be compared.
\param s2 is the second string to be compared.
\retval 1 if the strings are identical,
\retval 0 otherwise.
*/
/*************************************************************************/
int gk_strcasecmp(char *s1, char *s2)
{
  int i=0;

  if (strlen(s1) != strlen(s2))
    return 0;

  while (s1[i] != '\0') {
    if (tolower((unsigned char)s1[i]) != tolower((unsigned char)s2[i]))
      return 0;
    i++;
  }

  return 1;
}


/************************************************************************/
/*! \brief Compare two strings in revere order

This function is similar to strcmp but it performs the comparison as
if the two strings were reversed.

\param s1 is the first string to be compared.
\param s2 is the second string to be compared.
\retval -1, 0, 1, if the s1 < s2, s1 == s2, or s1 > s2.
*/
/*************************************************************************/
int gk_strrcmp(char *s1, char *s2)
{
  int i1 = strlen(s1)-1;
  int i2 = strlen(s2)-1;

  while ((i1 >= 0) && (i2 >= 0)) {
    if (s1[i1] != s2[i2])
      return (s1[i1] - s2[i2]);
    i1--;
    i2--;
  }

  /* i1 == -1 and/or i2 == -1 */

  if (i1 < i2)
    return -1;
  if (i1 > i2)
    return 1;
  return 0;
}



/************************************************************************/
/*! \brief Converts a time_t time into a string 

This function takes a time_t-specified time and returns a string-formated
representation of the corresponding time. The format of the string is
<em>mm/dd/yyyy hh:mm:ss</em>, in which the hours are in military time.

\param time is the time to be converted.
\return It returns a pointer to a statically allocated string that is 
        over-written in successive calls of this function. If the 
        conversion failed, it returns NULL.

*/
/*************************************************************************/
char *gk_time2str(time_t time)
{
  static char datestr[128];
  struct tm *tm;

  tm = localtime(&time);

  if (strftime(datestr, 128, "%m/%d/%Y %H:%M:%S", tm) == 0)
    return NULL;
  else
    return datestr;
}



#if !defined(_WIN32) && !defined(__MINGW32__)
/************************************************************************/
/*! \brief Converts a date/time string into its equivalent time_t value

This function takes date and/or time specification and converts it in
the equivalent time_t representation. The conversion is done using the
strptime() function. The format that gk_str2time() understands is
<em>mm/dd/yyyy hh:mm:ss</em>, in which the hours are in military time.

\param str is the date/time string to be converted.
\return If the conversion was successful it returns the time, otherwise 
        it returns -1.
*/
/*************************************************************************/
time_t gk_str2time(char *str)
{
  struct tm time;
  time_t rtime;

  memset(&time, '\0', sizeof(time));
  
  if (strptime(str, "%m/%d/%Y %H:%M:%S", &time) == NULL)
    return -1;

  rtime = mktime(&time);
  return (rtime < 0 ? 0 : rtime);
}
#endif


/*************************************************************************
* This function returns the ID of a particular string based on the 
* supplied StringMap array
**************************************************************************/
int gk_GetStringID(gk_StringMap_t *strmap, char *key)
{
  int i;

  for (i=0; strmap[i].name; i++) {
    if (gk_strcasecmp(key, strmap[i].name))
      return strmap[i].id;
  }

  return -1;
}
