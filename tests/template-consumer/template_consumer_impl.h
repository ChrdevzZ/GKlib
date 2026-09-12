#include "template_consumer.h"

#define template_key_lt(a, b) ((a) < (b))
#define template_sort_lt(a, b) (*(a) < *(b))
#define template_blas_kv_gt(a, b) ((a)->key > (b)->key)

GK_MKALLOC(template_int_, int)
GK_MKALLOC(template_kv_, template_kv_t)
GK_MKALLOC(template_blas_kv, template_blas_kv_t)

void template_blas_kvsortd(size_t n, template_blas_kv_t *values)
{
  GK_MKQSORT(template_blas_kv_t, values, n, template_blas_kv_gt);
}

GK_MKBLAS(template_blas_, int, int)
GK_MKPQUEUE(template_pq_, template_pq_t, template_kv_t, int, gk_idx_t,
    template_kv_malloc, INT_MAX, template_key_lt)
GK_MKPQUEUE2(template_pq2_, template_pq2_t, int, int,
    template_int_malloc, template_int_malloc, INT_MAX, template_key_lt)
GK_MKRANDOM(template_random_, size_t, int)
GK_MKARRAY2CSR(template_, int)

static void template_sort(int *values, size_t n)
{
  GK_MKQSORT(int, values, n, template_sort_lt);
}

int template_consumer_run(void)
{
  int result = 0;
  int values[] = {3, 1, 2};
  int array[] = {1, 0, 1};
  int ptr[3], ind[3];
  int top2 = -1;
  int *allocated;
  template_pq_t queue;
  template_pq2_t *queue2;

  result |= template_blas_sum(3, values, 1) != 6;
  allocated = template_int_smalloc(3, 7, (char *)"template consumer");
  result |= allocated == NULL;
  if (allocated != NULL) {
    result |= allocated[0] != 7 || allocated[2] != 7;
    gk_free((void **)&allocated, LTERM);
  }

  template_pq_Init(&queue, 3);
  result |= template_pq_Insert(&queue, 0, 3) != 0;
  result |= template_pq_Insert(&queue, 1, 1) != 0;
  result |= template_pq_GetTop(&queue) != 1;
  template_pq_Free(&queue);

  queue2 = template_pq2_Create2(3);
  result |= queue2 == NULL;
  if (queue2 != NULL) {
    result |= !template_pq2_Insert2(queue2, 30, 3);
    result |= !template_pq2_Insert2(queue2, 10, 1);
    result |= !template_pq2_GetTop2(queue2, &top2) || top2 != 10;
    template_pq2_Destroy2(&queue2);
  }

  template_random_srand(1);
  result |= template_random_randInRange(7) >= 7;
  template_array2csr(3, 2, array, ptr, ind);
  result |= ptr[0] != 0 || ptr[1] != 1 || ptr[2] != 3;
  template_sort(values, 3);
  result |= values[0] != 1 || values[1] != 2 || values[2] != 3;

  return result;
}

#undef template_sort_lt
#undef template_key_lt
#undef template_blas_kv_gt
