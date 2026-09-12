#include <GKlib.h>

#include <cmath>
#include <vector>

int main()
{
  // Execute a normalization region, so dead stripping cannot hide a missing
  // OpenMP runtime behind a create/free-only consumer.
  char message[] = "OpenMP consumer";
  gk_csr_t *matrix = gk_csr_Create();
  matrix->nrows = 1;
  matrix->ncols = 2;
  matrix->rowptr = gk_zmalloc(2, message);
  matrix->rowind = gk_imalloc(2, message);
  matrix->rowval = gk_fmalloc(2, message);
  matrix->rowptr[0] = 0;
  matrix->rowptr[1] = 2;
  matrix->rowind[0] = 0;
  matrix->rowind[1] = 1;
  matrix->rowval[0] = 3.0f;
  matrix->rowval[1] = 4.0f;
  gk_csr_Normalize(matrix, GK_CSR_ROW, 2);
  bool normalized = std::fabs(matrix->rowval[0] - 0.6f) < 1e-6f &&
                    std::fabs(matrix->rowval[1] - 0.8f) < 1e-6f;
  gk_csr_Free(&matrix);
  std::vector<int> values{4, 2, 3};
  gk_isorti(static_cast<int>(values.size()), values.data());
  return normalized && gk_isum(static_cast<int>(values.size()), values.data(), 1) == 9 &&
                 values.front() == 2
             ? 0
             : 1;
}
