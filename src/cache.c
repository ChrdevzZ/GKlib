/*!
\file 
\brief Functions dealing with simulating cache behavior for performance
       modeling and analysis;

\date Started 4/13/18
\author George
\author Copyright 1997-2011, Regents of the University of Minnesota 
\version $Id: cache.c 21991 2018-04-16 03:08:12Z karypis $
*/

#include <GKlib.h>
#include "memory_internal.h"


/*************************************************************************/
/*! This function creates a cache 
 */
/*************************************************************************/
gk_cache_t *gk_cacheCreate(uint32_t nway, uint32_t lnbits, size_t cnbits)
{
  int saved_errno;
  gk_cache_t *cache;
  size_t address_bits, cline_bytes, latime_bytes, nlines;

  if (nway == 0) {
    errno = EINVAL;
    gk_errexit(SIGERR, "gk_cacheCreate: invalid cache dimensions");
    errno = EINVAL;
    return NULL;
  }
  if (lnbits >= sizeof(size_t)*CHAR_BIT ||
      cnbits >= sizeof(size_t)*CHAR_BIT ||
      !gk_size_add(cnbits, lnbits, &address_bits) ||
      address_bits >= sizeof(size_t)*CHAR_BIT ||
      nway > (SIZE_MAX >> address_bits)) {
    errno = EOVERFLOW;
    gk_errexit(SIGMEM, "gk_cacheCreate: invalid cache dimensions");
    errno = EOVERFLOW;
    return NULL;
  }

  if (!gk_size_mul((size_t)1 << cnbits, nway, &nlines)) {
    errno = EOVERFLOW;
    gk_errexit(SIGMEM, "gk_cacheCreate: cache metadata size overflow");
    errno = EOVERFLOW;
    return NULL;
  }
  if (!gk_size_mul(nlines, sizeof(uint64_t), &latime_bytes) ||
      !gk_size_mul(nlines, sizeof(size_t), &cline_bytes)) {
    errno = EOVERFLOW;
    gk_errexit(SIGMEM, "gk_cacheCreate: cache metadata size overflow");
    errno = EOVERFLOW;
    return NULL;
  }

  cache = (gk_cache_t *)gk_malloc_nosignal(sizeof(gk_cache_t));
  if (cache == NULL) {
    saved_errno = errno != 0 ? errno : ENOMEM;
    errno = saved_errno;
    gk_errexit(SIGMEM, "gk_cacheCreate: cache allocation failed");
    errno = saved_errno;
    return NULL;
  }
  memset(cache, 0, sizeof(gk_cache_t));

  cache->nway   = nway;
  cache->lnbits = lnbits;
  cache->cnbits = (uint32_t)cnbits;
  cache->csize  = (size_t)1 << cnbits;
  cache->cmask  = cache->csize-1;

  cache->latimes = (uint64_t *)gk_malloc_nosignal(latime_bytes);
  if (cache->latimes == NULL) {
    saved_errno = errno != 0 ? errno : ENOMEM;
    gk_free((void **)&cache, LTERM);
    errno = saved_errno;
    gk_errexit(SIGMEM, "gk_cacheCreate: latimes allocation failed");
    errno = saved_errno;
    return NULL;
  }
  gk_ui64set(nlines, 0, cache->latimes);

  cache->clines = (size_t *)gk_malloc_nosignal(cline_bytes);
  if (cache->clines == NULL) {
    saved_errno = errno != 0 ? errno : ENOMEM;
    gk_free((void **)&cache->latimes, &cache, LTERM);
    errno = saved_errno;
    gk_errexit(SIGMEM, "gk_cacheCreate: clines allocation failed");
    errno = saved_errno;
    return NULL;
  }
  gk_zuset(nlines, 0, cache->clines);

  return cache;
}


/*************************************************************************/
/*! This function resets a cache 
 */
/*************************************************************************/
void gk_cacheReset(gk_cache_t *cache)
{
  size_t nlines = cache->csize*cache->nway;

  cache->nhits   = 0;
  cache->nmisses = 0;
  cache->clock   = 0;

  gk_ui64set(nlines, 0, cache->latimes);
  gk_zuset(nlines, 0, cache->clines);

  return;
}


/*************************************************************************/
/*! This function destroys a cache.
 */
/*************************************************************************/
void gk_cacheDestroy(gk_cache_t **r_cache)
{
  gk_cache_t *cache = *r_cache;

  if (cache == NULL)
    return;

  gk_free((void **)&cache->clines, &cache->latimes, &cache, LTERM);

  *r_cache = NULL;
}


/*************************************************************************/
/*! This function simulates a load(ptr) operation.
 */
/*************************************************************************/
int gk_cacheLoad(gk_cache_t *cache, size_t addr)
{
  uint32_t i, nway=cache->nway;
  size_t lru=0, set;

  //printf("%16"PRIx64" ", (uint64_t)addr);
  addr = addr>>(cache->lnbits);
  //printf("%16"PRIx64" %16"PRIx64" %16"PRIx64" ", (uint64_t)addr, (uint64_t)addr&(cache->cmask), (uint64_t)cache->cmask);

  set = (addr&cache->cmask)*nway;
  size_t *clines    = cache->clines  + set;
  uint64_t *latimes = cache->latimes + set;

  cache->clock++;
  for (i=0; i<nway; i++) { /* look for hits */
    if (latimes[i] != 0 && clines[i] == addr) {
      cache->nhits++;
      latimes[i] = cache->clock;
      goto DONE;
    }
  }

  for (i=0; i<nway; i++) { /* look for empty spots or the lru spot */
    if (latimes[i] == 0) {
      lru = i;
      break;
    }
    else if (latimes[i] < latimes[lru]) {
      lru = i;
    }
  }

  /* initial fill or replace */
  cache->nmisses++;
  clines[lru]  = addr;
  latimes[lru] = cache->clock;

DONE:
  //printf(" %"PRIu64" %"PRIu64"\n", cache->nhits, cache->clock);
  return 1;
}


/*************************************************************************/
/*! This function returns the cache's hitrate
 */
/*************************************************************************/
double gk_cacheGetHitRate(gk_cache_t *cache)
{
  return cache->clock == 0 ? 0.0 :
      ((double)cache->nhits)/((double)cache->clock);
}

