#include <GKlib.h>


static int check_invalid_cache_dimensions(void)
{
  gk_cache_t *cache;
  size_t memory_before;

  if (!gk_malloc_init())
    return 20;
  memory_before = gk_GetCurMemoryUsed();

  if (gk_cacheCreate(0, 0, 1) != NULL ||
      gk_cacheCreate(1, (uint32_t)(sizeof(size_t)*CHAR_BIT), 1) != NULL ||
      gk_cacheCreate(1, 0, sizeof(size_t)*CHAR_BIT) != NULL ||
      gk_GetCurMemoryUsed() != memory_before)
    return 21;

  errno = 0;
  cache = gk_cacheCreate(1, 0, sizeof(size_t)*CHAR_BIT-3);
  if (cache != NULL || errno != EOVERFLOW ||
      gk_GetCurMemoryUsed() != memory_before)
    return 22;

  gk_malloc_cleanup(0);
  return 0;
}


static int check_distinct_cache_sets(void)
{
  gk_cache_t *cache;

  cache = gk_cacheCreate(2, 0, 1);
  if (cache == NULL)
    return 1;

  gk_cacheLoad(cache, 2);
  gk_cacheLoad(cache, 4);
  gk_cacheLoad(cache, 3);
  gk_cacheLoad(cache, 5);
  gk_cacheLoad(cache, 4);

  if (cache->nhits != 1 || cache->nmisses != 4) {
    gk_cacheDestroy(&cache);
    return 2;
  }

  gk_cacheDestroy(&cache);
  return cache == NULL ? 0 : 3;
}


static int check_zero_tag_and_hitrate(void)
{
  gk_cache_t *cache;

  cache = gk_cacheCreate(1, 0, 0);
  if (cache == NULL)
    return 4;
  if (gk_cacheGetHitRate(cache) != 0.0) {
    gk_cacheDestroy(&cache);
    return 5;
  }

  gk_cacheLoad(cache, 0);
  if (cache->nhits != 0 || cache->nmisses != 1) {
    gk_cacheDestroy(&cache);
    return 6;
  }
  gk_cacheLoad(cache, 0);
  if (cache->nhits != 1 || cache->nmisses != 1 ||
      gk_cacheGetHitRate(cache) != 0.5) {
    gk_cacheDestroy(&cache);
    return 7;
  }

  gk_cacheReset(cache);
  if (cache->clock != 0 || cache->nhits != 0 || cache->nmisses != 0 ||
      gk_cacheGetHitRate(cache) != 0.0) {
    gk_cacheDestroy(&cache);
    return 8;
  }
  gk_cacheLoad(cache, 0);
  if (cache->nhits != 0 || cache->nmisses != 1) {
    gk_cacheDestroy(&cache);
    return 9;
  }

  gk_cacheDestroy(&cache);
  return 0;
}


int main(void)
{
  int status;

  gk_set_exit_on_error(0);

  status = check_invalid_cache_dimensions();
  if (status != 0)
    return status;
  status = check_distinct_cache_sets();
  if (status != 0)
    return status;

  return check_zero_tag_and_hitrate();
}
