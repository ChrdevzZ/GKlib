#ifndef GKLIB_TESTS_TEMPLATE_CONSUMER_H
#define GKLIB_TESTS_TEMPLATE_CONSUMER_H

#include <GKlib.h>

GK_MKKEYVALUE_T(template_kv_t, int, gk_idx_t)
GK_MKKEYVALUE_T(template_blas_kv_t, int, size_t)
GK_MKPQUEUE_T(template_pq_t, template_kv_t)
GK_MKPQUEUE2_T(template_pq2_t, int, int)

GK_MKBLAS_PROTO(template_blas_, int, int)
GK_MKALLOC_PROTO(template_int_, int)
GK_MKALLOC_PROTO(template_kv_, template_kv_t)
GK_MKALLOC_PROTO(template_blas_kv, template_blas_kv_t)
GK_MKPQUEUE_PROTO(template_pq_, template_pq_t, int, gk_idx_t)
GK_MKPQUEUE2_PROTO(template_pq2_, template_pq2_t, int, int)
GK_MKRANDOM_PROTO(template_random_, size_t, int)
GK_MKARRAY2CSR_PROTO(template_, int)
void template_blas_kvsortd(size_t n, template_blas_kv_t *values);

int template_consumer_run(void);

#endif
