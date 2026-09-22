/*!
\file  tokenizer.c
\brief String tokenization routines

This file contains various routines for splitting an input string into
tokens and returning them in form of a list. The goal is to mimic perl's 
split function.

\date   Started 11/23/04
\author George
\version\verbatim $Id: tokenizer.c 10711 2011-08-31 22:23:04Z karypis $ \endverbatim
*/


#include <GKlib.h>
#include "memory_internal.h"


/*************************************************************************/
/*! Tokenizes a string using the supplied delimiter characters.

    The token pointers refer into one duplicated string buffer. Both objects
    are owned by \c tokens and must be released with gk_freetokenslist(). The
    output is initialized to an empty state before validation and is committed
    only after all allocations succeed.

    \param str is the string to tokenize.
    \param delim contains the delimiter characters.
    \param tokens receives the token count, string buffer, and pointer array.
*/
/*************************************************************************/
void gk_strtokenize(char *str, char *delim, gk_Tokens_t *tokens)
{
  size_t i, ntoks, slen;
  char *strbuf;
  char **list;

  if (tokens == NULL) {
    errno = EINVAL;
    gk_errexit(SIGERR, "gk_strtokenize: tokens must not be NULL");
    errno = EINVAL;
    return;
  }
  tokens->ntoks = 0;
  tokens->strbuf = NULL;
  tokens->list = NULL;
  if (str == NULL || delim == NULL) {
    errno = EINVAL;
    gk_errexit(SIGERR, "gk_strtokenize: input strings must not be NULL");
    errno = EINVAL;
    return;
  }

  slen = strlen(str);

  /* Scan once to determine the number of tokens */
  for (ntoks=0, i=0; i<slen;) {
    /* Consume all the consecutive characters from the delimiters list */
    while (i<slen && strchr(delim, str[i])) 
      i++;

    if (i == slen)
      break;

    if (ntoks == INT_MAX) {
      errno = EOVERFLOW;
      gk_errexit(SIGMEM, "gk_strtokenize: too many tokens");
      errno = EOVERFLOW;
      return;
    }
    ntoks++;

    /* Consume all the consecutive characters from the token */
    while (i<slen && !strchr(delim, str[i])) 
      i++;
  }


  if (ntoks > SIZE_MAX/sizeof(char *)) {
    errno = EOVERFLOW;
    gk_errexit(SIGMEM, "gk_strtokenize: token array size overflow");
    errno = EOVERFLOW;
    return;
  }
  if (slen == SIZE_MAX) {
    errno = EOVERFLOW;
    gk_errexit(SIGMEM, "gk_strtokenize: string size overflow");
    errno = EOVERFLOW;
    return;
  }
  strbuf = (char *)gk_malloc_nosignal(slen+1);
  if (strbuf == NULL) {
    int saved_errno;

    if (errno == 0)
      errno = ENOMEM;
    saved_errno = errno;
    gk_errexit(SIGMEM, "gk_strtokenize: failed to duplicate input");
    errno = saved_errno;
    return;
  }
  memcpy(strbuf, str, slen+1);

  list = (char **)gk_malloc_nosignal(ntoks*sizeof(char *));
  if (list == NULL) {
    int saved_errno=errno != 0 ? errno : ENOMEM;

    gk_free((void **)&strbuf, LTERM);
    errno = saved_errno;
    gk_errexit(SIGMEM, "gk_strtokenize: failed to allocate token array");
    errno = saved_errno;
    return;
  }


  /* Scan a second time to mark and link the tokens */
  str = strbuf;
  for (ntoks=0, i=0; i<slen;) {
    /* Consume all the consecutive characters from the delimiters list */
    while (i<slen && strchr(delim, str[i])) 
      str[i++] = '\0';

    if (i == slen)
      break;

    list[ntoks++] = str+i;

    /* Consume all the consecutive characters from the token */
    while (i<slen && !strchr(delim, str[i])) 
      i++;
  }

  tokens->ntoks = (int)ntoks;
  tokens->strbuf = strbuf;
  tokens->list = list;
}


/************************************************************************
* This function frees the memory associated with a gk_Tokens_t
*************************************************************************/
void gk_freetokenslist(gk_Tokens_t *tokens)
{
  if (tokens == NULL)
    return;

  gk_free((void *)&tokens->list, &tokens->strbuf, LTERM);
  tokens->ntoks = 0;
}

