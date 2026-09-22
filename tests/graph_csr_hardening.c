#include <GKlib.h>

static int write_bytes(const char *filename, const void *data, size_t size)
{
  FILE *stream = fopen(filename, "wb");

  if (stream == NULL)
    return 0;
  if (fwrite(data, 1, size, stream) != size) {
    fclose(stream);
    return 0;
  }
  return fclose(stream) == 0;
}


static int test_graph_make_symmetric(void)
{
  ssize_t xadj[] = {0, 1, 2};
  int32_t adjncy[] = {1, 0};
  int32_t iadjwgt[] = {2, 4};
  int32_t *unexpected;
  gk_graph_t graph = {0};
  gk_graph_t *symmetric;

  graph.nvtxs = 2;
  graph.xadj = xadj;
  graph.adjncy = adjncy;
  graph.iadjwgt = iadjwgt;

  symmetric = gk_graph_MakeSymmetric(&graph, GK_GRAPH_SYM_SUM);
  unexpected = graph.iadjwgt != iadjwgt ? graph.iadjwgt : NULL;
  graph.iadjwgt = iadjwgt;
  if (symmetric == NULL || unexpected != NULL ||
      symmetric->iadjwgt == NULL || symmetric->iadjwgt == iadjwgt ||
      symmetric->iadjwgt[0] != 6 || symmetric->iadjwgt[1] != 6) {
    gk_graph_Free(&symmetric);
    gk_free((void **)&unexpected, LTERM);
    return 0;
  }
  gk_graph_Free(&symmetric);

  symmetric = gk_graph_MakeSymmetric(&graph, GK_GRAPH_SYM_AVG);
  if (symmetric == NULL || graph.iadjwgt != iadjwgt ||
      symmetric->iadjwgt == NULL || symmetric->iadjwgt == iadjwgt ||
      symmetric->iadjwgt[0] != 3 || symmetric->iadjwgt[1] != 3) {
    if (graph.iadjwgt != iadjwgt) {
      unexpected = graph.iadjwgt;
      graph.iadjwgt = iadjwgt;
      gk_free((void **)&unexpected, LTERM);
    }
    gk_graph_Free(&symmetric);
    return 0;
  }
  gk_graph_Free(&symmetric);

  if (graph.iadjwgt != iadjwgt || iadjwgt[0] != 2 || iadjwgt[1] != 4)
    return 0;
  {
    ssize_t invalid_xadj[] = {1, 1, 1};

    graph.xadj = invalid_xadj;
    errno = 0;
    symmetric = gk_graph_MakeSymmetric(&graph, GK_GRAPH_SYM_SUM);
    if (symmetric != NULL || errno != EINVAL)
      return 0;
  }
  {
    ssize_t overflow_xadj[] = {0, PTRDIFF_MAX};
    int32_t only_vertex = 0;

    graph.nvtxs = 1;
    graph.xadj = overflow_xadj;
    graph.adjncy = &only_vertex;
    errno = 0;
    symmetric = gk_graph_MakeSymmetric(&graph, GK_GRAPH_SYM_SUM);
    if (symmetric != NULL || errno != EOVERFLOW)
      return 0;
  }
  {
    static const int operations[] = {
      GK_GRAPH_SYM_MAX, GK_GRAPH_SYM_MIN,
      GK_GRAPH_SYM_SUM, GK_GRAPH_SYM_AVG
    };
    static const ssize_t expected_edges[] = {3, 2, 3, 3};
    static const int32_t expected_weights[][3] = {
      {5, 8, 4}, {2, 1, 0}, {7, 9, 4}, {3, 4, 2}
    };
    ssize_t parallel_xadj[] = {0, 3, 5};
    int32_t parallel_adjncy[] = {1, 1, 1, 0, 0};
    int32_t parallel_iadjwgt[] = {2, 8, 4, 5, 1};
    int operation, row;

    graph.nvtxs = 2;
    graph.xadj = parallel_xadj;
    graph.adjncy = parallel_adjncy;
    graph.iadjwgt = parallel_iadjwgt;
    for (operation=0; operation<4; operation++) {
      symmetric = gk_graph_MakeSymmetric(&graph, operations[operation]);
      if (symmetric == NULL || symmetric->xadj[1] != expected_edges[operation] ||
          symmetric->xadj[2] != 2*expected_edges[operation]) {
        gk_graph_Free(&symmetric);
        return 0;
      }
      for (row=0; row<2; row++) {
        ssize_t edge, first=symmetric->xadj[row];

        for (edge=0; edge<expected_edges[operation]; edge++) {
          if (symmetric->adjncy[first+edge] != 1-row ||
              symmetric->iadjwgt[first+edge] !=
                  expected_weights[operation][edge]) {
            gk_graph_Free(&symmetric);
            return 0;
          }
        }
      }
      gk_graph_Free(&symmetric);
    }
  }
  {
    ssize_t loop_xadj[] = {0, 2};
    int32_t loop_adjncy[] = {0, 0};
    int32_t loop_iadjwgt[] = {2, 8};

    graph.nvtxs = 1;
    graph.xadj = loop_xadj;
    graph.adjncy = loop_adjncy;
    graph.iadjwgt = loop_iadjwgt;
    symmetric = gk_graph_MakeSymmetric(&graph, GK_GRAPH_SYM_SUM);
    if (symmetric == NULL || symmetric->xadj[1] != 2 ||
        symmetric->adjncy[0] != 0 || symmetric->adjncy[1] != 0 ||
        symmetric->iadjwgt[0] != 4 || symmetric->iadjwgt[1] != 16) {
      gk_graph_Free(&symmetric);
      return 0;
    }
    gk_graph_Free(&symmetric);
  }

  return 1;
}


static int test_graph_extract_induced_subgraph(void)
{
  ssize_t xadj[] = {0, 1, 4, 6, 10, 11};
  int32_t adjncy[] = {1, 0, 2, 4, 1, 3, 0, 1, 4, 2, 3};
  int32_t iadjwgt[] = {1, 10, 12, 14, 21, 23, 30, 31, 34, 32, 43};
  float fadjwgt[] = {1.5f, 10.5f, 12.5f, 14.5f, 21.5f, 23.5f,
                     30.5f, 31.5f, 34.5f, 32.5f, 43.5f};
  int32_t ivwgts[] = {100, 101, 102, 103, 104};
  float fvwgts[] = {100.5f, 101.5f, 102.5f, 103.5f, 104.5f};
  int32_t ivsizes[] = {200, 201, 202, 203, 204};
  float fvsizes[] = {200.5f, 201.5f, 202.5f, 203.5f, 204.5f};
  int32_t vlabels[] = {300, 301, 302, 303, 304};
  const ssize_t expected_xadj[] = {0, 1, 3, 5};
  const int32_t expected_adjncy[] = {1, 0, 2, 0, 1};
  const int32_t expected_iadjwgt[] = {12, 21, 23, 31, 32};
  const float expected_fadjwgt[] = {12.5f, 21.5f, 23.5f, 31.5f, 32.5f};
  gk_graph_t graph = {0};
  gk_graph_t *subgraph;
  ssize_t i;

  graph.nvtxs = 5;
  graph.xadj = xadj;
  graph.adjncy = adjncy;
  graph.iadjwgt = iadjwgt;
  graph.fadjwgt = fadjwgt;
  graph.ivwgts = ivwgts;
  graph.fvwgts = fvwgts;
  graph.ivsizes = ivsizes;
  graph.fvsizes = fvsizes;
  graph.vlabels = vlabels;

  subgraph = gk_graph_ExtractSubgraph(&graph, 1, 3);
  if (subgraph == NULL || subgraph->nvtxs != 3 ||
      subgraph->xadj[0] != expected_xadj[0] ||
      subgraph->xadj[1] != expected_xadj[1] ||
      subgraph->xadj[2] != expected_xadj[2] ||
      subgraph->xadj[3] != expected_xadj[3]) {
    gk_graph_Free(&subgraph);
    return 0;
  }
  for (i=0; i<expected_xadj[3]; i++) {
    if (subgraph->adjncy[i] != expected_adjncy[i] ||
        subgraph->iadjwgt[i] != expected_iadjwgt[i] ||
        subgraph->fadjwgt[i] != expected_fadjwgt[i]) {
      gk_graph_Free(&subgraph);
      return 0;
    }
  }
  for (i=0; i<3; i++) {
    if (subgraph->ivwgts[i] != ivwgts[i+1] ||
        subgraph->fvwgts[i] != fvwgts[i+1] ||
        subgraph->ivsizes[i] != ivsizes[i+1] ||
        subgraph->fvsizes[i] != fvsizes[i+1] ||
        subgraph->vlabels[i] != vlabels[i+1]) {
      gk_graph_Free(&subgraph);
      return 0;
    }
  }
  gk_graph_Free(&subgraph);

  return graph.xadj == xadj && graph.adjncy == adjncy &&
      graph.iadjwgt == iadjwgt && graph.fadjwgt == fadjwgt &&
      xadj[5] == 11 && adjncy[1] == 0 && iadjwgt[1] == 10 &&
      fadjwgt[1] == 10.5f;
}


static int test_graph_transform_validation(void)
{
  ssize_t invalid_xadj[] = {1, 1};
  ssize_t overflow_xadj[] = {0, PTRDIFF_MAX};
  ssize_t valid_xadj[] = {0, 1, 2};
  int32_t only_vertex=0;
  int32_t valid_adjncy[] = {1, 0};
  int32_t duplicate_perm[] = {0, 0};
  int32_t valid_perm[] = {1, 0};
  int32_t cptr[] = {101, 102};
  int32_t cind[] = {103};
  gk_graph_t graph = {0};
  gk_graph_t *result;

  graph.nvtxs = -1;
  graph.xadj = invalid_xadj;
  errno = 0;
  result = gk_graph_Dup(&graph);
  if (result != NULL || errno != EINVAL) {
    gk_graph_Free(&result);
    return 0;
  }
  graph.nvtxs = 1;
  graph.xadj = invalid_xadj;
  errno = 0;
  result = gk_graph_Dup(&graph);
  if (result != NULL || errno != EINVAL) {
    gk_graph_Free(&result);
    return 0;
  }
  errno = 0;
  result = gk_graph_Transpose(&graph);
  if (result != NULL || errno != EINVAL) {
    gk_graph_Free(&result);
    return 0;
  }
  errno = 0;
  if (gk_graph_FindComponents(&graph, cptr, cind) != -1 ||
      errno != EINVAL || cptr[0] != 101 || cptr[1] != 102 ||
      cind[0] != 103)
    return 0;

  graph.xadj = overflow_xadj;
  graph.adjncy = &only_vertex;
  errno = 0;
  result = gk_graph_Dup(&graph);
  if (result != NULL || errno != EOVERFLOW) {
    gk_graph_Free(&result);
    return 0;
  }

  graph.nvtxs = 2;
  graph.xadj = valid_xadj;
  graph.adjncy = valid_adjncy;
  errno = 0;
  result = gk_graph_ExtractSubgraph(&graph, -1, 1);
  if (result != NULL || errno != EINVAL) {
    gk_graph_Free(&result);
    return 0;
  }
  errno = 0;
  result = gk_graph_ExtractSubgraph(&graph, 2, 1);
  if (result != NULL || errno != EINVAL) {
    gk_graph_Free(&result);
    return 0;
  }
  errno = 0;
  result = gk_graph_Reorder(&graph, duplicate_perm, duplicate_perm);
  if (result != NULL || errno != EINVAL) {
    gk_graph_Free(&result);
    return 0;
  }

  result = gk_graph_Dup(&graph);
  if (result == NULL || result->xadj == valid_xadj ||
      result->adjncy == valid_adjncy || result->xadj[2] != 2 ||
      result->adjncy[0] != 1 || result->adjncy[1] != 0) {
    gk_graph_Free(&result);
    return 0;
  }
  gk_graph_Free(&result);
  result = gk_graph_Transpose(&graph);
  if (result == NULL || result->xadj[2] != 2 ||
      result->adjncy[0] != 1 || result->adjncy[1] != 0) {
    gk_graph_Free(&result);
    return 0;
  }
  gk_graph_Free(&result);
  result = gk_graph_ExtractSubgraph(&graph, 0, 2);
  if (result == NULL || result->xadj[2] != 2) {
    gk_graph_Free(&result);
    return 0;
  }
  gk_graph_Free(&result);
  result = gk_graph_Reorder(&graph, valid_perm, NULL);
  if (result == NULL || result->xadj[2] != 2 ||
      result->adjncy[0] != 1 || result->adjncy[1] != 0) {
    gk_graph_Free(&result);
    return 0;
  }
  gk_graph_Free(&result);
  if (graph.xadj != valid_xadj || graph.adjncy != valid_adjncy ||
      valid_xadj[0] != 0 || valid_xadj[2] != 2)
    return 0;
  {
    ssize_t self_xadj[] = {0, 1};
    int32_t self_adjncy[] = {0};
    int32_t self_cptr[] = {-1, -1};
    int32_t self_cind[] = {-1};

    graph.nvtxs = 1;
    graph.xadj = self_xadj;
    graph.adjncy = self_adjncy;
    if (gk_graph_FindComponents(&graph, self_cptr, self_cind) != 1 ||
        self_cptr[0] != 0 || self_cptr[1] != 1 || self_cind[0] != 0)
      return 0;
  }

  return valid_xadj[0] == 0 && valid_xadj[2] == 2;
}


static int test_csr_make_symmetric(void)
{
  ssize_t rowptr[] = {0, 1, 2};
  int32_t rowind[] = {1, 0};
  float rowval[] = {2.0f, 4.0f};
  gk_csr_t matrix = {0};
  gk_csr_t *symmetric;

  matrix.nrows = matrix.ncols = 2;
  matrix.rowptr = rowptr;
  matrix.rowind = rowind;
  matrix.rowval = rowval;
  symmetric = gk_csr_MakeSymmetric(&matrix, GK_CSR_SYM_AVG);
  if (symmetric == NULL || matrix.rowval != rowval ||
      symmetric->rowval == NULL || symmetric->rowval == rowval ||
      symmetric->rowval[0] != 3.0f || symmetric->rowval[1] != 3.0f) {
    gk_csr_Free(&symmetric);
    return 0;
  }
  gk_csr_Free(&symmetric);

  {
    ssize_t structural_rowptr[] = {0, 1, 1};
    int32_t structural_rowind[] = {1};

    matrix.rowptr = structural_rowptr;
    matrix.rowind = structural_rowind;
    matrix.rowval = NULL;
    symmetric = gk_csr_MakeSymmetric(&matrix, GK_CSR_SYM_AVG);
    if (symmetric == NULL || symmetric->rowval != NULL ||
        symmetric->rowptr[2] != 2) {
      gk_csr_Free(&symmetric);
      return 0;
    }
    gk_csr_Free(&symmetric);
  }

  {
    ssize_t invalid_rowptr[] = {1, 1, 1};

    matrix.rowptr = invalid_rowptr;
    errno = 0;
    symmetric = gk_csr_MakeSymmetric(&matrix, GK_CSR_SYM_SUM);
    if (symmetric != NULL || errno != EINVAL)
      return 0;
  }
  {
    ssize_t overflow_rowptr[] = {0, PTRDIFF_MAX};
    int32_t only_column = 0;

    matrix.nrows = matrix.ncols = 1;
    matrix.rowptr = overflow_rowptr;
    matrix.rowind = &only_column;
    matrix.rowval = NULL;
    errno = 0;
    symmetric = gk_csr_MakeSymmetric(&matrix, GK_CSR_SYM_SUM);
    if (symmetric != NULL || errno != EOVERFLOW)
      return 0;
  }
  {
    static const int operations[] = {
      GK_CSR_SYM_MAX, GK_CSR_SYM_MIN, GK_CSR_SYM_SUM, GK_CSR_SYM_AVG
    };
    static const ssize_t expected_entries[] = {3, 2, 3, 3};
    static const float expected_values[][3] = {
      {5.0f, 8.0f, 4.0f}, {2.0f, 1.0f, 0.0f},
      {7.0f, 9.0f, 4.0f}, {3.5f, 4.5f, 2.0f}
    };
    ssize_t parallel_rowptr[] = {0, 3, 5};
    int32_t parallel_rowind[] = {1, 1, 1, 0, 0};
    float parallel_rowval[] = {2.0f, 8.0f, 4.0f, 5.0f, 1.0f};
    int operation, row;

    matrix.nrows = matrix.ncols = 2;
    matrix.rowptr = parallel_rowptr;
    matrix.rowind = parallel_rowind;
    matrix.rowval = parallel_rowval;
    for (operation=0; operation<4; operation++) {
      symmetric = gk_csr_MakeSymmetric(&matrix, operations[operation]);
      if (symmetric == NULL ||
          symmetric->rowptr[1] != expected_entries[operation] ||
          symmetric->rowptr[2] != 2*expected_entries[operation]) {
        gk_csr_Free(&symmetric);
        return 0;
      }
      for (row=0; row<2; row++) {
        ssize_t entry, first=symmetric->rowptr[row];

        for (entry=0; entry<expected_entries[operation]; entry++) {
          if (symmetric->rowind[first+entry] != 1-row ||
              symmetric->rowval[first+entry] !=
                  expected_values[operation][entry]) {
            gk_csr_Free(&symmetric);
            return 0;
          }
        }
      }
      gk_csr_Free(&symmetric);
    }
  }
  {
    ssize_t loop_rowptr[] = {0, 2};
    int32_t loop_rowind[] = {0, 0};
    float loop_rowval[] = {2.0f, 8.0f};

    matrix.nrows = matrix.ncols = 1;
    matrix.rowptr = loop_rowptr;
    matrix.rowind = loop_rowind;
    matrix.rowval = loop_rowval;
    symmetric = gk_csr_MakeSymmetric(&matrix, GK_CSR_SYM_SUM);
    if (symmetric == NULL || symmetric->rowptr[1] != 2 ||
        symmetric->rowind[0] != 0 || symmetric->rowind[1] != 0 ||
        symmetric->rowval[0] != 4.0f || symmetric->rowval[1] != 16.0f) {
      gk_csr_Free(&symmetric);
      return 0;
    }
    gk_csr_Free(&symmetric);
  }

  return 1;
}


static int test_csr_transform_validation(void)
{
  ssize_t invalid_rowptr[] = {1, 1};
  ssize_t overflow_rowptr[] = {0, PTRDIFF_MAX};
  ssize_t valid_rowptr[] = {0, 1, 2};
  int colors[] = {0, 1};
  int invalid_color[] = {-1, 0};
  int overflow_color[] = {INT_MAX, 0};
  int parts[] = {0, 1};
  int selected_row=0, invalid_row=2;
  int32_t only_column=0;
  int32_t valid_rowind[] = {1, 0};
  int32_t duplicate_perm[] = {0, 0};
  int32_t valid_perm[] = {1, 0};
  int32_t cptr[] = {201, 202};
  int32_t cind[] = {203};
  int32_t cids[] = {204};
  gk_csr_t matrix = {0};
  gk_csr_t **split;
  gk_csr_t *result;

  matrix.nrows = -1;
  matrix.ncols = 0;
  errno = 0;
  result = gk_csr_Dup(&matrix);
  if (result != NULL || errno != EINVAL) {
    gk_csr_Free(&result);
    return 0;
  }
  matrix.nrows = matrix.ncols = 1;
  matrix.rowptr = invalid_rowptr;
  errno = 0;
  result = gk_csr_Dup(&matrix);
  if (result != NULL || errno != EINVAL) {
    gk_csr_Free(&result);
    return 0;
  }
  errno = 0;
  result = gk_csr_Transpose(&matrix);
  if (result != NULL || errno != EINVAL) {
    gk_csr_Free(&result);
    return 0;
  }
  errno = 0;
  if (gk_csr_FindConnectedComponents(&matrix, cptr, cind, cids) != -1 ||
      errno != EINVAL || cptr[0] != 201 || cptr[1] != 202 ||
      cind[0] != 203 || cids[0] != 204)
    return 0;

  matrix.rowptr = overflow_rowptr;
  matrix.rowind = &only_column;
  errno = 0;
  result = gk_csr_Dup(&matrix);
  if (result != NULL || errno != EOVERFLOW) {
    gk_csr_Free(&result);
    return 0;
  }

  matrix.nrows = matrix.ncols = 2;
  matrix.rowptr = valid_rowptr;
  matrix.rowind = valid_rowind;
  errno = 0;
  result = gk_csr_ExtractSubmatrix(&matrix, -1, 1);
  if (result != NULL || errno != EINVAL) {
    gk_csr_Free(&result);
    return 0;
  }
  errno = 0;
  result = gk_csr_ExtractSubmatrix(&matrix, 2, 1);
  if (result != NULL || errno != EINVAL) {
    gk_csr_Free(&result);
    return 0;
  }
  errno = 0;
  result = gk_csr_ReorderSymmetric(&matrix, duplicate_perm,
                                   duplicate_perm);
  if (result != NULL || errno != EINVAL) {
    gk_csr_Free(&result);
    return 0;
  }
  errno = 0;
  result = gk_csr_ExtractRows(&matrix, -1, NULL);
  if (result != NULL || errno != EINVAL) {
    gk_csr_Free(&result);
    return 0;
  }
  errno = 0;
  result = gk_csr_ExtractRows(&matrix, 1, &invalid_row);
  if (result != NULL || errno != EINVAL) {
    gk_csr_Free(&result);
    return 0;
  }
  errno = 0;
  result = gk_csr_ExtractPartition(&matrix, NULL, 0);
  if (result != NULL || errno != EINVAL) {
    gk_csr_Free(&result);
    return 0;
  }
  errno = 0;
  split = gk_csr_Split(&matrix, invalid_color);
  if (split != NULL || errno != EINVAL)
    return 0;
  errno = 0;
  split = gk_csr_Split(&matrix, overflow_color);
  if (split != NULL || errno != EOVERFLOW)
    return 0;

  result = gk_csr_ExtractRows(&matrix, 1, &selected_row);
  if (result == NULL || result->nrows != 1 || result->ncols != 2 ||
      result->rowptr[0] != 0 || result->rowptr[1] != 1 ||
      result->rowind[0] != 1 || result->rowval != NULL) {
    gk_csr_Free(&result);
    return 0;
  }
  gk_csr_Free(&result);
  result = gk_csr_ExtractPartition(&matrix, parts, 0);
  if (result == NULL || result->nrows != 1 || result->ncols != 2 ||
      result->rowptr[0] != 0 || result->rowptr[1] != 1 ||
      result->rowind[0] != 1 || result->rowval != NULL) {
    gk_csr_Free(&result);
    return 0;
  }
  gk_csr_Free(&result);
  split = gk_csr_Split(&matrix, colors);
  if (split == NULL || split[0] == NULL || split[1] == NULL ||
      split[0]->rowval != NULL || split[1]->rowval != NULL ||
      split[0]->rowptr[0] != 0 || split[0]->rowptr[1] != 1 ||
      split[0]->rowptr[2] != 1 || split[0]->rowind[0] != 1 ||
      split[1]->rowptr[0] != 0 || split[1]->rowptr[1] != 0 ||
      split[1]->rowptr[2] != 1 || split[1]->rowind[0] != 0) {
    if (split != NULL) {
      gk_csr_Free(&split[0]);
      gk_csr_Free(&split[1]);
    }
    gk_free((void **)&split, LTERM);
    return 0;
  }
  gk_csr_Free(&split[0]);
  gk_csr_Free(&split[1]);
  gk_free((void **)&split, LTERM);

  matrix.colptr = valid_rowptr;
  matrix.colind = valid_rowind;
  result = gk_csr_Dup(&matrix);
  if (result == NULL || result->rowptr == valid_rowptr ||
      result->rowind == valid_rowind || result->colptr == valid_rowptr ||
      result->colind == valid_rowind || result->rowptr[2] != 2 ||
      result->colptr[2] != 2) {
    gk_csr_Free(&result);
    return 0;
  }
  gk_csr_Free(&result);
  result = gk_csr_Transpose(&matrix);
  if (result == NULL || result->rowptr[2] != 2 ||
      result->rowind[0] != 1 || result->rowind[1] != 0 ||
      matrix.colptr != valid_rowptr || matrix.colind != valid_rowind) {
    gk_csr_Free(&result);
    return 0;
  }
  gk_csr_Free(&result);
  result = gk_csr_ExtractSubmatrix(&matrix, 0, 2);
  if (result == NULL || result->rowptr[2] != 2) {
    gk_csr_Free(&result);
    return 0;
  }
  gk_csr_Free(&result);
  result = gk_csr_ReorderSymmetric(&matrix, valid_perm, NULL);
  if (result == NULL || result->rowval != NULL ||
      result->rowptr[2] != 2 || result->rowind[0] != 1 ||
      result->rowind[1] != 0) {
    gk_csr_Free(&result);
    return 0;
  }
  gk_csr_Free(&result);
  if (matrix.rowptr != valid_rowptr || matrix.rowind != valid_rowind ||
      matrix.rowval != NULL || matrix.colptr != valid_rowptr ||
      matrix.colind != valid_rowind)
    return 0;
  {
    ssize_t self_rowptr[] = {0, 1};
    int32_t self_rowind[] = {0};
    int32_t self_cptr[] = {-1, -1};
    int32_t self_cind[] = {-1};
    int32_t self_cids[] = {-1};

    matrix.nrows = matrix.ncols = 1;
    matrix.rowptr = self_rowptr;
    matrix.rowind = self_rowind;
    matrix.colptr = NULL;
    matrix.colind = NULL;
    if (gk_csr_FindConnectedComponents(&matrix, self_cptr, self_cind,
        self_cids) != 1 || self_cptr[0] != 0 || self_cptr[1] != 1 ||
        self_cind[0] != 0 || self_cids[0] != 0)
      return 0;
  }

  return valid_rowptr[0] == 0 && valid_rowptr[2] == 2;
}


static int test_transform_signals(void)
{
  ssize_t invalid_pointers[] = {1, 1};
  ssize_t overflow_pointers[] = {0, PTRDIFF_MAX};
  int part=0, row=0;
  int32_t only_index=0;
  int expected_signal, operation, saved_errno, use_csr, overflow;

  for (use_csr=0; use_csr<2; use_csr++) {
    for (operation=0; operation<(use_csr ? 9 : 6); operation++) {
      for (overflow=0; overflow<2; overflow++) {
        volatile int signum=0;
        gk_graph_t graph = {0};
        gk_csr_t matrix = {0};

        graph.nvtxs = 1;
        graph.xadj = overflow ? overflow_pointers : invalid_pointers;
        graph.adjncy = overflow ? &only_index : NULL;
        matrix.nrows = matrix.ncols = 1;
        matrix.rowptr = overflow ? overflow_pointers : invalid_pointers;
        matrix.rowind = overflow ? &only_index : NULL;
        expected_signal = overflow ? SIGMEM : SIGERR;
        if (!gk_sigtrap())
          return 0;
        gk_set_exit_on_error(1);
        errno = 0;
        switch (gk_sigcatch()) {
          case 0:
            if (use_csr) {
              if (operation == 0)
                (void)gk_csr_Dup(&matrix);
              else if (operation == 1)
                (void)gk_csr_MakeSymmetric(&matrix, GK_CSR_SYM_SUM);
              else if (operation == 2)
                (void)gk_csr_FindConnectedComponents(&matrix, NULL, NULL,
                                                      NULL);
              else if (operation == 3)
                (void)gk_csr_Transpose(&matrix);
              else if (operation == 4)
                (void)gk_csr_ExtractSubmatrix(&matrix, 0, 1);
              else if (operation == 5)
                (void)gk_csr_ReorderSymmetric(&matrix, &only_index, NULL);
              else if (operation == 6)
                (void)gk_csr_ExtractRows(&matrix, 1, &row);
              else if (operation == 7)
                (void)gk_csr_ExtractPartition(&matrix, &part, 0);
              else
                (void)gk_csr_Split(&matrix, &part);
            }
            else {
              if (operation == 0)
                (void)gk_graph_Dup(&graph);
              else if (operation == 1)
                (void)gk_graph_MakeSymmetric(&graph, GK_GRAPH_SYM_SUM);
              else if (operation == 2)
                (void)gk_graph_FindComponents(&graph, NULL, NULL);
              else if (operation == 3)
                (void)gk_graph_Transpose(&graph);
              else if (operation == 4)
                (void)gk_graph_ExtractSubgraph(&graph, 0, 1);
              else
                (void)gk_graph_Reorder(&graph, &only_index, NULL);
            }
            break;
          case SIGMEM:
            signum = SIGMEM;
            break;
          case SIGERR:
            signum = SIGERR;
            break;
          default:
            signum = -1;
            break;
        }
        saved_errno = errno;
        if (!gk_siguntrap())
          return 0;
        gk_set_exit_on_error(0);
        if (signum != expected_signal ||
            saved_errno != (overflow ? EOVERFLOW : EINVAL))
          return 0;
      }
    }
  }
  return 1;
}


static int test_reader_overflow_signals(char *filename)
{
  volatile int signum;
  int use_csr;

  for (use_csr=0; use_csr<2; use_csr++) {
    signum = 0;
    if (!gk_sigtrap())
      return 0;
    gk_set_exit_on_error(1);
    errno = 0;
    switch (gk_sigcatch()) {
      case 0:
        if (use_csr)
          (void)gk_csr_Read(filename, GK_CSR_FMT_METIS, 0, 1);
        else
          (void)gk_graph_Read(filename, GK_GRAPH_FMT_METIS,
                              0, 1, 0, 0, 0);
        break;
      case SIGMEM:
        signum = SIGMEM;
        break;
      default:
        signum = SIGERR;
        break;
    }
    if (!gk_siguntrap())
      return 0;
    gk_set_exit_on_error(0);
    if (signum != SIGMEM || errno != EOVERFLOW)
      return 0;
  }

  return 1;
}


int main(void)
{
  static const char valid_graph[] = "2 1\n2\n1";
  static const char invalid_graph[] = "2 1\n3\n1";
  static const char invalid_format[] = "2 1 222\n2\n1";
  static const char valid_ijv[] = "1 1\n2 2";
  static const char invalid_hijv[] = "2 2\n3 1\n";
  static const char malformed_ijv[] = "1 1 junk\n";
  static const unsigned char nul_ijv[] = {'1', ' ', '1', '\0', '2', '\n'};
  static const char valid_csr[] = "0\n1";
  static const char overflowing_csr_column[] = "2147483647\n";
  static const char declared_columns_cluto[] = "1 3 1\n1 1.0\n";
  static const char isolated_metis[] = "3 1\n2\n1\n\n";
  static const char weighted_ncon_metis[] =
      "2 1 010 2\n1 2 2\n3 4 1\n";
  static const char unweighted_ncon_metis[] =
      "2 1 000 2\n2\n1\n";
  static const char negative_edge_metis[] =
      "2 1 001\n2 -1\n1 -1\n";
  static const char zero_edge_metis[] =
      "2 1 001\n2 0\n1 0\n";
  static const char negative_vertex_weight_metis[] =
      "2 1 010\n-1 2\n1 1\n";
  static const char negative_vertex_size_metis[] =
      "2 1 100\n-1 2\n1 1\n";
  static const char zero_vertex_values_metis[] =
      "2 1 111\n0 0 2 1\n0 0 1 1\n";
  static const char empty_ijv[] = "% no entries\n\n";
  static const char empty_hijv[] = "0 0\n";
  static const char signed_ijv[] = "0 1 -5\n1 0 -7\n";
  static const char extra_graph[] = "2 1\n2\n1\n1";
  static const unsigned char nul_graph[] = {
    '2', ' ', '1', '\n', '2', '\0', 'x', '\n', '1'
  };
  struct {
    int32_t nrows;
    int32_t ncols;
    ssize_t rowptr[2];
  } truncated_csr = {1, 1, {0, 1}};
  char overflow_metis[64];
  gk_graph_t *graph;
  gk_graph_t *valid_graph_obj;
  gk_csr_t *matrix;

  gk_set_exit_on_error(0);
  if (!gk_malloc_init())
    return 1;
  if (!test_transform_signals()) {
    fprintf(stderr, "transform signal classification failed\n");
    return 50;
  }
  if (!test_graph_transform_validation()) {
    fprintf(stderr, "graph transform validation/ownership failed\n");
    return 48;
  }
  if (!test_graph_extract_induced_subgraph()) {
    fprintf(stderr, "graph induced subgraph extraction failed\n");
    return 57;
  }
  if (!test_csr_transform_validation()) {
    fprintf(stderr, "CSR transform validation/ownership failed\n");
    return 49;
  }
  if (!test_graph_make_symmetric()) {
    fprintf(stderr, "graph integer-weight symmetry ownership/AVG failed\n");
    return 25;
  }
  if (!test_csr_make_symmetric()) {
    fprintf(stderr, "CSR symmetry validation/AVG failed\n");
    return 36;
  }
  snprintf(overflow_metis, sizeof(overflow_metis), "1 %"PRIuMAX"\n",
      (uintmax_t)PTRDIFF_MAX/2+1);
  if (gk_csr_DetermineFormat((char *)"directory.with.dot/matrix.ijv",
                             GK_CSR_FMT_AUTO) != GK_CSR_FMT_IJV ||
      gk_csr_DetermineFormat((char *)"directory.ijv/matrix",
                             GK_CSR_FMT_AUTO) != GK_CSR_FMT_CSR ||
      gk_csr_DetermineFormat((char *)"directory.ijv\\matrix.binrow",
                             GK_CSR_FMT_AUTO) != GK_CSR_FMT_BINROW)
    return 37;
  if (!write_bytes("gklib-valid.graph", valid_graph, sizeof(valid_graph)-1) ||
      !write_bytes("gklib-invalid.graph", invalid_graph,
                   sizeof(invalid_graph)-1) ||
      !write_bytes("gklib-invalid-fmt.graph", invalid_format,
                   sizeof(invalid_format)-1) ||
      !write_bytes("gklib-extra.graph", extra_graph,
                   sizeof(extra_graph)-1) ||
      !write_bytes("gklib-nul.graph", nul_graph, sizeof(nul_graph)))
    return 1;

  graph = gk_graph_Read((char *)"gklib-valid.graph", GK_GRAPH_FMT_METIS,
                        0, 1, 0, 0, 0);
  if (graph == NULL || graph->nvtxs != 2 || graph->xadj[2] != 2)
    return 2;
  valid_graph_obj = graph;
  graph = gk_graph_Read((char *)"gklib-invalid.graph", GK_GRAPH_FMT_METIS,
                        0, 1, 0, 0, 0);
  if (graph != NULL)
    return 3;
  graph = gk_graph_Read((char *)"gklib-invalid-fmt.graph", GK_GRAPH_FMT_METIS,
                        0, 1, 0, 0, 0);
  if (graph != NULL)
    return 4;
  graph = gk_graph_Read((char *)"gklib-extra.graph", GK_GRAPH_FMT_METIS,
                        0, 1, 0, 0, 0);
  if (graph != NULL)
    return 12;
  graph = gk_graph_Read((char *)"gklib-nul.graph", GK_GRAPH_FMT_METIS,
                        0, 1, 0, 0, 0);
  if (graph != NULL)
    return 13;

  if (!write_bytes("gklib-truncated.binrow", &truncated_csr,
                   sizeof(truncated_csr)) ||
      !write_bytes("gklib-valid.ijv", valid_ijv, sizeof(valid_ijv)-1) ||
      !write_bytes("gklib-invalid.hijv", invalid_hijv,
                   sizeof(invalid_hijv)-1) ||
      !write_bytes("gklib-malformed.ijv", malformed_ijv,
                   sizeof(malformed_ijv)-1) ||
      !write_bytes("gklib-nul.ijv", nul_ijv, sizeof(nul_ijv)) ||
      !write_bytes("gklib-valid.csr", valid_csr, sizeof(valid_csr)-1) ||
      !write_bytes("gklib-overflow-column.csr", overflowing_csr_column,
                   sizeof(overflowing_csr_column)-1) ||
      !write_bytes("gklib-declared-columns.clu", declared_columns_cluto,
                   sizeof(declared_columns_cluto)-1) ||
      !write_bytes("gklib-isolated.metis", isolated_metis,
                   sizeof(isolated_metis)-1) ||
      !write_bytes("gklib-weighted-ncon.metis", weighted_ncon_metis,
                   sizeof(weighted_ncon_metis)-1) ||
      !write_bytes("gklib-unweighted-ncon.metis", unweighted_ncon_metis,
                   sizeof(unweighted_ncon_metis)-1) ||
      !write_bytes("gklib-negative-edge.metis", negative_edge_metis,
                   sizeof(negative_edge_metis)-1) ||
      !write_bytes("gklib-zero-edge.metis", zero_edge_metis,
                   sizeof(zero_edge_metis)-1) ||
      !write_bytes("gklib-negative-vertex-weight.metis",
                   negative_vertex_weight_metis,
                   sizeof(negative_vertex_weight_metis)-1) ||
      !write_bytes("gklib-negative-vertex-size.metis",
                   negative_vertex_size_metis,
                   sizeof(negative_vertex_size_metis)-1) ||
      !write_bytes("gklib-zero-vertex-values.metis",
                   zero_vertex_values_metis,
                   sizeof(zero_vertex_values_metis)-1) ||
      !write_bytes("gklib-empty.ijv", empty_ijv, sizeof(empty_ijv)-1) ||
      !write_bytes("gklib-empty.hijv", empty_hijv,
                   sizeof(empty_hijv)-1) ||
      !write_bytes("gklib-signed.ijv", signed_ijv, sizeof(signed_ijv)-1) ||
      !write_bytes("gklib-overflow.metis", overflow_metis,
                   strlen(overflow_metis)))
    return 5;
  if (!test_reader_overflow_signals((char *)"gklib-overflow.metis"))
    return 58;
  matrix = gk_csr_Read((char *)"gklib-truncated.binrow", GK_CSR_FMT_BINROW,
                       0, 0);
  if (matrix != NULL)
    return 6;
  matrix = gk_csr_Read((char *)"gklib-extra.graph", GK_CSR_FMT_METIS, 0, 1);
  if (matrix != NULL)
    return 14;
  matrix = gk_csr_Read((char *)"gklib-nul.graph", GK_CSR_FMT_METIS, 0, 1);
  if (matrix != NULL)
    return 15;
  matrix = gk_csr_Read((char *)"gklib-valid.ijv", GK_CSR_FMT_IJV, 0, 1);
  if (matrix == NULL || matrix->nrows != 2 || matrix->ncols != 2 ||
      matrix->rowptr[2] != 2)
    return 7;
  gk_csr_Free(&matrix);
  matrix = gk_csr_Read((char *)"gklib-malformed.ijv", GK_CSR_FMT_IJV, 0, 1);
  if (matrix != NULL)
    return 16;
  matrix = gk_csr_Read((char *)"gklib-nul.ijv", GK_CSR_FMT_IJV, 0, 1);
  if (matrix != NULL)
    return 17;
  graph = gk_graph_Read((char *)"gklib-valid.ijv", GK_GRAPH_FMT_IJV,
                        0, 1, 0, 0, 0);
  if (graph == NULL || graph->nvtxs != 2 || graph->xadj[2] != 2)
    return 20;
  gk_graph_Free(&graph);
  graph = gk_graph_Read((char *)"gklib-malformed.ijv", GK_GRAPH_FMT_IJV,
                        0, 1, 0, 0, 0);
  if (graph != NULL)
    return 18;
  graph = gk_graph_Read((char *)"gklib-nul.ijv", GK_GRAPH_FMT_IJV,
                        0, 1, 0, 0, 0);
  if (graph != NULL)
    return 19;
  graph = gk_graph_Read((char *)"gklib-invalid.hijv", GK_GRAPH_FMT_HIJV,
                        0, 1, 0, 0, 0);
  if (graph != NULL)
    return 21;
  errno = 0;
  graph = gk_graph_Read((char *)"gklib-empty.ijv", GK_GRAPH_FMT_IJV,
                        0, 1, 0, 0, 0);
  if (graph != NULL || errno != EINVAL)
    return 26;
  graph = gk_graph_Read((char *)"gklib-empty.hijv", GK_GRAPH_FMT_HIJV,
                        0, 0, 0, 0, 0);
  if (graph == NULL || graph->nvtxs != 0 || graph->xadj == NULL ||
      graph->xadj[0] != 0)
    return 38;
  gk_graph_Free(&graph);
  errno = 0;
  graph = gk_graph_Read((char *)"gklib-weighted-ncon.metis",
                        GK_GRAPH_FMT_METIS, 0, 1, 0, 0, 0);
  if (graph != NULL || errno != EINVAL)
    return 27;
  errno = 0;
  graph = gk_graph_Read((char *)"gklib-unweighted-ncon.metis",
                        GK_GRAPH_FMT_METIS, 0, 1, 0, 0, 0);
  if (graph != NULL || errno != EINVAL)
    return 39;
  errno = 0;
  graph = gk_graph_Read((char *)"gklib-negative-edge.metis",
                        GK_GRAPH_FMT_METIS, 0, 1, 0, 0, 0);
  if (graph != NULL || errno != EINVAL)
    return 32;
  errno = 0;
  graph = gk_graph_Read((char *)"gklib-negative-edge.metis",
                        GK_GRAPH_FMT_METIS, 0, 1, 1, 0, 0);
  if (graph != NULL || errno != EINVAL)
    return 40;
  errno = 0;
  graph = gk_graph_Read((char *)"gklib-zero-edge.metis",
                        GK_GRAPH_FMT_METIS, 0, 1, 0, 0, 0);
  if (graph != NULL || errno != EINVAL)
    return 51;
  errno = 0;
  graph = gk_graph_Read((char *)"gklib-zero-edge.metis",
                        GK_GRAPH_FMT_METIS, 0, 1, 1, 0, 0);
  if (graph != NULL || errno != EINVAL)
    return 52;
  errno = 0;
  graph = gk_graph_Read((char *)"gklib-negative-vertex-weight.metis",
                        GK_GRAPH_FMT_METIS, 0, 1, 0, 0, 0);
  if (graph != NULL || errno != EINVAL)
    return 41;
  errno = 0;
  graph = gk_graph_Read((char *)"gklib-negative-vertex-weight.metis",
                        GK_GRAPH_FMT_METIS, 0, 1, 0, 1, 0);
  if (graph != NULL || errno != EINVAL)
    return 42;
  errno = 0;
  graph = gk_graph_Read((char *)"gklib-negative-vertex-size.metis",
                        GK_GRAPH_FMT_METIS, 0, 1, 0, 0, 0);
  if (graph != NULL || errno != EINVAL)
    return 43;
  errno = 0;
  graph = gk_graph_Read((char *)"gklib-negative-vertex-size.metis",
                        GK_GRAPH_FMT_METIS, 0, 1, 0, 0, 1);
  if (graph != NULL || errno != EINVAL)
    return 44;
  graph = gk_graph_Read((char *)"gklib-zero-vertex-values.metis",
                        GK_GRAPH_FMT_METIS, 0, 1, 0, 0, 0);
  if (graph == NULL || graph->ivwgts == NULL || graph->ivsizes == NULL ||
      graph->ivwgts[0] != 0 || graph->ivsizes[0] != 0)
    return 53;
  gk_graph_Free(&graph);
  graph = gk_graph_Read((char *)"gklib-zero-vertex-values.metis",
                        GK_GRAPH_FMT_METIS, 0, 1, 1, 1, 1);
  if (graph == NULL || graph->fvwgts == NULL || graph->fvsizes == NULL ||
      graph->fvwgts[0] != 0.0f || graph->fvsizes[0] != 0.0f)
    return 54;
  gk_graph_Free(&graph);
  graph = gk_graph_Read((char *)"gklib-signed.ijv", GK_GRAPH_FMT_IJV,
                        1, 0, 0, 0, 0);
  if (graph == NULL || graph->iadjwgt == NULL ||
      graph->iadjwgt[0] != -5 || graph->iadjwgt[1] != -7)
    return 28;
  gk_graph_Free(&graph);

  matrix = gk_csr_Read((char *)"gklib-valid.csr", GK_CSR_FMT_CSR, 0, 0);
  if (matrix == NULL || matrix->nrows != 2 || matrix->ncols != 2 ||
      matrix->rowptr[2] != 2)
    return 8;

  gk_csr_Free(&matrix);
  matrix = gk_csr_Read((char *)"gklib-overflow-column.csr",
                       GK_CSR_FMT_CSR, 0, 0);
  if (matrix != NULL) {
    gk_csr_Free(&matrix);
    return 24;
  }
  matrix = gk_csr_Read((char *)"gklib-declared-columns.clu",
                       GK_CSR_FMT_CLUTO, 1, 1);
  if (matrix == NULL || matrix->nrows != 1 || matrix->ncols != 3 ||
      matrix->rowptr[1] != 1)
    return 22;
  gk_csr_Free(&matrix);
  matrix = gk_csr_Read((char *)"gklib-isolated.metis",
                       GK_CSR_FMT_METIS, 0, 1);
  if (matrix == NULL || matrix->nrows != 3 || matrix->ncols != 3 ||
      matrix->rowptr[3] != 2)
    return 23;
  gk_csr_Free(&matrix);
  errno = 0;
  matrix = gk_csr_Read((char *)"gklib-empty.ijv", GK_CSR_FMT_IJV, 0, 1);
  if (matrix != NULL || errno != EINVAL)
    return 29;
  errno = 0;
  matrix = gk_csr_Read((char *)"gklib-weighted-ncon.metis",
                       GK_CSR_FMT_METIS, 0, 1);
  if (matrix != NULL || errno != EINVAL)
    return 30;
  errno = 0;
  matrix = gk_csr_Read((char *)"gklib-unweighted-ncon.metis",
                       GK_CSR_FMT_METIS, 0, 1);
  if (matrix != NULL || errno != EINVAL)
    return 45;
  errno = 0;
  matrix = gk_csr_Read((char *)"gklib-negative-edge.metis",
                       GK_CSR_FMT_METIS, 0, 1);
  if (matrix != NULL || errno != EINVAL)
    return 33;
  errno = 0;
  matrix = gk_csr_Read((char *)"gklib-zero-edge.metis",
                       GK_CSR_FMT_METIS, 0, 1);
  if (matrix != NULL || errno != EINVAL)
    return 55;
  errno = 0;
  matrix = gk_csr_Read((char *)"gklib-negative-vertex-weight.metis",
                       GK_CSR_FMT_METIS, 0, 1);
  if (matrix != NULL || errno != EINVAL)
    return 46;
  errno = 0;
  matrix = gk_csr_Read((char *)"gklib-negative-vertex-size.metis",
                       GK_CSR_FMT_METIS, 0, 1);
  if (matrix != NULL || errno != EINVAL)
    return 47;
  matrix = gk_csr_Read((char *)"gklib-zero-vertex-values.metis",
                       GK_CSR_FMT_METIS, 0, 1);
  if (matrix == NULL || matrix->rwgts == NULL || matrix->rsizes == NULL ||
      matrix->rwgts[0] != 0.0f || matrix->rsizes[0] != 0.0f)
    return 56;
  gk_csr_Free(&matrix);

  matrix = gk_csr_Read((char *)"gklib-isolated.metis",
                       GK_CSR_FMT_METIS, 0, 1);
  if (matrix == NULL)
    return 31;

  if (gk_mkpath((char *)"gklib-writer-dir") != 0)
    return 9;
  gk_graph_Write(valid_graph_obj, (char *)"gklib-writer-dir",
                 GK_GRAPH_FMT_METIS, 1);
  gk_csr_Write(matrix, (char *)"gklib-writer-dir", GK_CSR_FMT_CSR, 0, 0);
  if (gk_rmpath((char *)"gklib-writer-dir") != 0)
    return 10;
  gk_graph_Free(&valid_graph_obj);
  gk_csr_Free(&matrix);

  if (gk_GetCurMemoryUsed() != 0)
    return 11;
  gk_malloc_cleanup(0);

  remove("gklib-valid.graph");
  remove("gklib-invalid.graph");
  remove("gklib-invalid-fmt.graph");
  remove("gklib-extra.graph");
  remove("gklib-nul.graph");
  remove("gklib-truncated.binrow");
  remove("gklib-valid.ijv");
  remove("gklib-invalid.hijv");
  remove("gklib-malformed.ijv");
  remove("gklib-nul.ijv");
  remove("gklib-valid.csr");
  remove("gklib-overflow-column.csr");
  remove("gklib-declared-columns.clu");
  remove("gklib-isolated.metis");
  remove("gklib-weighted-ncon.metis");
  remove("gklib-unweighted-ncon.metis");
  remove("gklib-negative-edge.metis");
  remove("gklib-zero-edge.metis");
  remove("gklib-negative-vertex-weight.metis");
  remove("gklib-negative-vertex-size.metis");
  remove("gklib-zero-vertex-values.metis");
  remove("gklib-empty.ijv");
  remove("gklib-empty.hijv");
  remove("gklib-signed.ijv");
  remove("gklib-overflow.metis");
  return 0;
}
