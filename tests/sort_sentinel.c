#include <GKlib.h>

#define TEST_SIZE 33

#define CHECK_SCALAR(PRFX, TYPE)                                         \
  do {                                                                  \
    TYPE values[TEST_SIZE];                                             \
    size_t i;                                                           \
                                                                        \
    for (i=0; i<TEST_SIZE; i++)                                         \
      values[i] = (TYPE)((i*17 + 5)%13);                                \
    PRFX ## sorti(TEST_SIZE, values);                                   \
    for (i=1; i<TEST_SIZE; i++) {                                       \
      if (values[i-1] > values[i])                                      \
        return 1;                                                       \
    }                                                                   \
                                                                        \
    for (i=0; i<TEST_SIZE; i++)                                         \
      values[i] = (TYPE)((i*19 + 3)%11);                                \
    PRFX ## sortd(TEST_SIZE, values);                                   \
    for (i=1; i<TEST_SIZE; i++) {                                       \
      if (values[i-1] < values[i])                                      \
        return 2;                                                       \
    }                                                                   \
  } while (0)

#define CHECK_KEYVALUE(PRFX, TYPE, KEYTYPE)                              \
  do {                                                                  \
    TYPE values[TEST_SIZE];                                             \
    size_t i;                                                           \
                                                                        \
    for (i=0; i<TEST_SIZE; i++) {                                       \
      values[i].key = (KEYTYPE)((i*17 + 5)%13);                         \
      values[i].val = (ssize_t)i;                                       \
    }                                                                   \
    PRFX ## sorti(TEST_SIZE, values);                                   \
    for (i=1; i<TEST_SIZE; i++) {                                       \
      if (values[i-1].key > values[i].key)                              \
        return 3;                                                       \
    }                                                                   \
                                                                        \
    for (i=0; i<TEST_SIZE; i++) {                                       \
      values[i].key = (KEYTYPE)((i*19 + 3)%11);                         \
      values[i].val = (ssize_t)i;                                       \
    }                                                                   \
    PRFX ## sortd(TEST_SIZE, values);                                   \
    for (i=1; i<TEST_SIZE; i++) {                                       \
      if (values[i-1].key < values[i].key)                              \
        return 4;                                                       \
    }                                                                   \
  } while (0)


int main(void)
{
  gk_skv_t strings[TEST_SIZE];
  size_t i;

  CHECK_SCALAR(gk_c, char);
  CHECK_SCALAR(gk_i, int);
  CHECK_SCALAR(gk_i32, int32_t);
  CHECK_SCALAR(gk_i64, int64_t);
  CHECK_SCALAR(gk_ui32, uint32_t);
  CHECK_SCALAR(gk_ui64, uint64_t);
  CHECK_SCALAR(gk_f, float);
  CHECK_SCALAR(gk_d, double);
  CHECK_SCALAR(gk_idx, gk_idx_t);

  CHECK_KEYVALUE(gk_ckv, gk_ckv_t, char);
  CHECK_KEYVALUE(gk_ikv, gk_ikv_t, int);
  CHECK_KEYVALUE(gk_i32kv, gk_i32kv_t, int32_t);
  CHECK_KEYVALUE(gk_i64kv, gk_i64kv_t, int64_t);
  CHECK_KEYVALUE(gk_zkv, gk_zkv_t, ssize_t);
  CHECK_KEYVALUE(gk_zukv, gk_zukv_t, size_t);
  CHECK_KEYVALUE(gk_fkv, gk_fkv_t, float);
  CHECK_KEYVALUE(gk_dkv, gk_dkv_t, double);
  CHECK_KEYVALUE(gk_idxkv, gk_idxkv_t, gk_idx_t);

  for (i=0; i<TEST_SIZE; i++) {
    strings[i].key = (char *)(i%3 == 0 ? "gamma" :
        (i%3 == 1 ? "alpha" : "beta"));
    strings[i].val = (ssize_t)i;
  }
  gk_skvsorti(TEST_SIZE, strings);
  for (i=1; i<TEST_SIZE; i++) {
    if (strcmp(strings[i-1].key, strings[i].key) > 0)
      return 5;
  }

  gk_skvsortd(TEST_SIZE, strings);
  for (i=1; i<TEST_SIZE; i++) {
    if (strcmp(strings[i-1].key, strings[i].key) < 0)
      return 6;
  }

  return 0;
}
