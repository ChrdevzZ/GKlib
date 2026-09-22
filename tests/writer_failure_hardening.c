#include <GKlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif


static int write_contents(const char *filename, const char *contents)
{
  FILE *stream;
  size_t length;

  stream = fopen(filename, "wb");
  if (stream == NULL)
    return 0;
  length = strlen(contents);
  if (fwrite(contents, 1, length, stream) != length) {
    fclose(stream);
    return 0;
  }
  return fclose(stream) == 0;
}


static int contents_equal(const char *filename, const char *expected)
{
  char buffer[64];
  FILE *stream;
  size_t count, length;

  stream = fopen(filename, "rb");
  if (stream == NULL)
    return 0;
  count = fread(buffer, 1, sizeof(buffer), stream);
  if (fclose(stream) != 0)
    return 0;
  length = strlen(expected);
  return count == length && memcmp(buffer, expected, length) == 0;
}


static int create_symbolic_link(const char *linkname, const char *target)
{
#ifdef _WIN32
  DWORD error;

  if (CreateSymbolicLinkA(linkname, target,
      SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE))
    return 1;
  error = GetLastError();
  if (error == ERROR_INVALID_PARAMETER &&
      CreateSymbolicLinkA(linkname, target, 0))
    return 1;
  if (error == ERROR_PRIVILEGE_NOT_HELD || error == ERROR_INVALID_PARAMETER ||
      error == ERROR_NOT_SUPPORTED)
    return 0;
  return -1;
#else
  if (symlink(target, linkname) == 0)
    return 1;
  return errno == EPERM || errno == ENOSYS ? 0 : -1;
#endif
}


static int create_hard_link(const char *linkname, const char *target)
{
#ifdef _WIN32
  DWORD error;

  if (CreateHardLinkA(linkname, target, NULL))
    return 1;
  error = GetLastError();
  if (error == ERROR_NOT_SUPPORTED || error == ERROR_INVALID_FUNCTION ||
      error == ERROR_ACCESS_DENIED)
    return 0;
  return -1;
#else
  if (link(target, linkname) == 0)
    return 1;
  return errno == EPERM || errno == ENOSYS || errno == EOPNOTSUPP ? 0 : -1;
#endif
}


static int check_binary_overflow_signal(void)
{
  volatile int signum=0;
  double value=1.0;

  if (!gk_sigtrap())
    return 0;
  gk_set_exit_on_error(1);
  errno = 0;
  switch (gk_sigcatch()) {
    case 0:
      (void)gk_dwritefilebin((char *)"gklib-overflow.bin",
          SIZE_MAX/sizeof(double)+1, &value);
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

  return signum == SIGMEM && errno == EOVERFLOW &&
      !gk_fexists((char *)"gklib-overflow.bin");
}


int main(void)
{
  ssize_t xadj[] = {0, 1, 2};
  ssize_t odd_xadj[] = {0, 1};
  int32_t adjncy[] = {1, 0};
  int32_t odd_adjncy[] = {0};
  ssize_t rowptr[] = {0, 1, 2};
  int32_t rowind[] = {1, 0};
  float rowval[] = {1.0f, 1.0f};
  ssize_t directed_ptr[] = {0, 1, 2, 2};
  int32_t directed_ind[] = {1, 2};
  int32_t unequal_weights[] = {1, 2};
  int32_t signed_weights[] = {-5, -7};
  int32_t zero_weights[] = {0, 0};
  int32_t positive_vertex_values[] = {1, 1};
  int32_t negative_vertex_values[] = {-1, 1};
  float float_weights[] = {1.0f, 1.0f};
  float zero_float_values[] = {0.0f, 0.0f};
  float negative_float_values[] = {-1.0f, 1.0f};
  double value = 1.0;
  char *empty;
  size_t count;
  gk_graph_t graph;
  gk_graph_t *read_graph;
  gk_csr_t matrix;
  gk_csr_t *read_matrix;
  FILE *stream;
  int link_status;

  gk_set_exit_on_error(0);

  if (gk_cwritefilebin((char *)"gklib-empty.bin", 0, NULL) != 0)
    return 15;
  count = 1;
  empty = gk_creadfilebin((char *)"gklib-empty.bin", &count);
  if (empty == NULL || count != 0)
    return 16;
  gk_free((void **)&empty, LTERM);

  remove("gklib-overflow.bin");
  errno = 0;
  if (gk_dwritefilebin((char *)"gklib-overflow.bin",
      SIZE_MAX/sizeof(double)+1, &value) != 0 || errno != EOVERFLOW ||
      gk_fexists((char *)"gklib-overflow.bin"))
    return 17;
  if (!check_binary_overflow_signal())
    return 44;
  gk_graph_Init(&graph);
  graph.nvtxs = 2;
  graph.xadj = xadj;
  graph.adjncy = adjncy;
  gk_csr_Init(&matrix);
  matrix.nrows = 2;
  matrix.ncols = 2;
  matrix.rowptr = rowptr;
  matrix.rowind = rowind;
  matrix.rowval = rowval;
  matrix.colptr = rowptr;
  matrix.colind = rowind;
  matrix.colval = rowval;

  gk_graph_Write(&graph, (char *)"gklib-writer.graph",
                 GK_GRAPH_FMT_METIS, 1);
  read_graph = gk_graph_Read((char *)"gklib-writer.graph",
                             GK_GRAPH_FMT_METIS, 0, 1, 0, 0, 0);
  if (read_graph == NULL || read_graph->nvtxs != 2 ||
      read_graph->xadj[2] != 2)
    return 1;
  gk_graph_Free(&read_graph);

  gk_graph_Write(&graph, (char *)"gklib-writer.graph.ijv",
                 GK_GRAPH_FMT_IJV, 0);
  read_graph = gk_graph_Read((char *)"gklib-writer.graph.ijv",
                             GK_GRAPH_FMT_IJV, 1, 0, 0, 0, 0);
  if (read_graph == NULL || read_graph->nvtxs != 2 ||
      read_graph->xadj[2] != 2)
    return 2;
  gk_graph_Free(&read_graph);

  gk_csr_Write(&matrix, (char *)"gklib-writer.csr",
               GK_CSR_FMT_CSR, 1, 0);
  read_matrix = gk_csr_Read((char *)"gklib-writer.csr",
                            GK_CSR_FMT_CSR, 1, 0);
  if (read_matrix == NULL || read_matrix->nrows != 2 ||
      read_matrix->rowptr[2] != 2)
    return 3;
  gk_csr_Free(&read_matrix);

  gk_csr_Write(&matrix, (char *)"gklib-writer.binrow",
               GK_CSR_FMT_BINROW, 1, 0);
  read_matrix = gk_csr_Read((char *)"gklib-writer.binrow",
                            GK_CSR_FMT_BINROW, 1, 0);
  if (read_matrix == NULL || read_matrix->rowptr[2] != 2)
    return 4;
  gk_csr_Free(&read_matrix);
  stream = fopen("gklib-writer.binrow", "ab");
  if (stream == NULL || fputc(0, stream) == EOF || fclose(stream) != 0)
    return 18;
  read_matrix = gk_csr_Read((char *)"gklib-writer.binrow",
                            GK_CSR_FMT_BINROW, 1, 0);
  if (read_matrix != NULL)
    return 19;

  gk_csr_Write(&matrix, (char *)"gklib-writer.bincol",
               GK_CSR_FMT_BINCOL, 1, 0);
  read_matrix = gk_csr_Read((char *)"gklib-writer.bincol",
                            GK_CSR_FMT_BINCOL, 1, 0);
  if (read_matrix == NULL || read_matrix->colptr[2] != 2)
    return 5;
  gk_csr_Free(&read_matrix);

  gk_csr_Write(&matrix, (char *)"gklib-writer.ijv",
               GK_CSR_FMT_IJV, 1, 0);
  read_matrix = gk_csr_Read((char *)"gklib-writer.ijv",
                            GK_CSR_FMT_IJV, 1, 0);
  if (read_matrix == NULL || read_matrix->rowptr[2] != 2)
    return 6;
  gk_csr_Free(&read_matrix);

  gk_csr_Write(&matrix, (char *)"gklib-writer.bijv",
               GK_CSR_FMT_BIJV, 1, 0);
  read_matrix = gk_csr_Read((char *)"gklib-writer.bijv",
                            GK_CSR_FMT_BIJV, 1, 0);
  if (read_matrix == NULL || read_matrix->rowptr[2] != 2)
    return 7;
  gk_csr_Free(&read_matrix);
  stream = fopen("gklib-writer.bijv", "ab");
  if (stream == NULL)
    return 13;
  if (fputc(0, stream) == EOF) {
    fclose(stream);
    return 13;
  }
  if (fclose(stream) != 0)
    return 13;
  read_matrix = gk_csr_Read((char *)"gklib-writer.bijv",
                            GK_CSR_FMT_BIJV, 1, 0);
  if (read_matrix != NULL)
    return 14;

  gk_csr_Write(&matrix, (char *)"gklib-writer.metis",
               GK_CSR_FMT_METIS, 0, 0);
  read_matrix = gk_csr_Read((char *)"gklib-writer.metis",
                            GK_CSR_FMT_METIS, 0, 0);
  if (read_matrix == NULL || read_matrix->rowptr[2] != 2)
    return 8;
  gk_csr_Free(&read_matrix);

  gk_csr_Write(&matrix, (char *)"gklib-writer.clu",
               GK_CSR_FMT_CLUTO, 1, 1);
  read_matrix = gk_csr_Read((char *)"gklib-writer.clu",
                            GK_CSR_FMT_CLUTO, 1, 1);
  if (read_matrix == NULL || read_matrix->rowptr[2] != 2)
    return 9;
  gk_csr_Free(&read_matrix);

  if (gk_mkpath((char *)"gklib-writer-failure-dir") != 0)
    return 10;
  gk_graph_Write(&graph, (char *)"gklib-writer-failure-dir",
                 GK_GRAPH_FMT_METIS, 1);
  gk_csr_Write(&matrix, (char *)"gklib-writer-failure-dir",
               GK_CSR_FMT_CSR, 0, 0);
  if (gk_rmpath((char *)"gklib-writer-failure-dir") != 0)
    return 11;
  gk_graph_Write(&graph, (char *)"gklib-partial-output.graph", -1, 1);
  if (gk_fexists((char *)"gklib-partial-output.graph"))
    return 12;
  stream = fopen("gklib-existing-output.graph", "wb");
  if (stream == NULL || fwrite("keep", 1, 4, stream) != 4 ||
      fclose(stream) != 0)
    return 21;
  gk_graph_Write(&graph, (char *)"gklib-existing-output.graph", -1, 1);
  stream = fopen("gklib-existing-output.graph", "rb");
  if (stream == NULL)
    return 22;
  {
    char preserved[5] = {0};
    if (fread(preserved, 1, 4, stream) != 4 || fclose(stream) != 0 ||
        strcmp(preserved, "keep") != 0)
      return 23;
  }

  /* Transactional output cannot preserve link identity, so reject linked
     destinations before creating or committing a temporary file. */
  remove("gklib-output-symbolic");
  remove("gklib-output-hard");
  remove("gklib-output-link-target");
  if (!write_contents("gklib-output-link-target", "keep"))
    return 28;
  link_status = create_symbolic_link("gklib-output-symbolic",
      "gklib-output-link-target");
  if (link_status < 0)
    return 29;
  if (link_status == 1) {
    errno = 0;
    gk_graph_Write(&graph, (char *)"gklib-output-symbolic",
                   GK_GRAPH_FMT_METIS, 1);
    if (errno == 0 ||
        !contents_equal("gklib-output-link-target", "keep"))
      return 30;
  }
  link_status = create_hard_link("gklib-output-hard",
      "gklib-output-link-target");
  if (link_status < 0)
    return 31;
  if (link_status == 1) {
    char replacement='x';

    errno = 0;
    if (gk_cwritefilebin((char *)"gklib-output-hard", 1,
                         &replacement) != 0 || errno == 0 ||
        !contents_equal("gklib-output-link-target", "keep") ||
        !contents_equal("gklib-output-hard", "keep"))
      return 32;
  }

  matrix.nrows = -1;
  remove("gklib-invalid-writer.csr");
  remove("gklib-existing-output.graph");
  gk_csr_Write(&matrix, (char *)"gklib-invalid-writer.csr",
               GK_CSR_FMT_CSR, 0, 0);
  if (gk_fexists((char *)"gklib-invalid-writer.csr"))
    return 20;

  graph.nvtxs = 1;
  graph.xadj = odd_xadj;
  graph.adjncy = odd_adjncy;
  remove("gklib-invalid-writer.graph");
  gk_graph_Write(&graph, (char *)"gklib-invalid-writer.graph",
                 GK_GRAPH_FMT_METIS, 1);
  if (gk_fexists((char *)"gklib-invalid-writer.graph"))
    return 24;

  graph.nvtxs = 3;
  graph.xadj = directed_ptr;
  graph.adjncy = directed_ind;
  remove("gklib-directed-writer.graph");
  gk_graph_Write(&graph, (char *)"gklib-directed-writer.graph",
                 GK_GRAPH_FMT_METIS, 1);
  if (gk_fexists((char *)"gklib-directed-writer.graph"))
    return 25;

  matrix.nrows = 3;
  matrix.ncols = 3;
  matrix.rowptr = directed_ptr;
  matrix.rowind = directed_ind;
  matrix.rowval = NULL;
  remove("gklib-directed-writer.metis");
  gk_csr_Write(&matrix, (char *)"gklib-directed-writer.metis",
               GK_CSR_FMT_METIS, 0, 0);
  if (gk_fexists((char *)"gklib-directed-writer.metis"))
    return 26;

  graph.nvtxs = 2;
  graph.xadj = xadj;
  graph.adjncy = adjncy;
  graph.iadjwgt = signed_weights;
  errno = 0;
  gk_graph_Write(&graph, (char *)"gklib-signed-writer.ijv",
                 GK_GRAPH_FMT_IJV, 0);
  if (errno != 0 || !gk_fexists((char *)"gklib-signed-writer.ijv"))
    return 33;
  read_graph = gk_graph_Read((char *)"gklib-signed-writer.ijv",
                             GK_GRAPH_FMT_IJV, 1, 0, 0, 0, 0);
  if (read_graph == NULL || read_graph->iadjwgt == NULL ||
      read_graph->iadjwgt[0] != -5 || read_graph->iadjwgt[1] != -7)
    return 34;
  gk_graph_Free(&read_graph);

  remove("gklib-negative-weight.graph");
  errno = 0;
  gk_graph_Write(&graph, (char *)"gklib-negative-weight.graph",
                 GK_GRAPH_FMT_METIS, 1);
  if (errno != EINVAL || gk_fexists((char *)"gklib-negative-weight.graph"))
    return 35;

  graph.iadjwgt = NULL;
  graph.fadjwgt = negative_float_values;
  errno = 0;
  gk_graph_Write(&graph, (char *)"gklib-negative-weight.graph",
                 GK_GRAPH_FMT_METIS, 1);
  if (errno != EINVAL || gk_fexists((char *)"gklib-negative-weight.graph"))
    return 39;

  graph.fadjwgt = NULL;
  graph.iadjwgt = zero_weights;
  errno = 0;
  gk_graph_Write(&graph, (char *)"gklib-negative-weight.graph",
                 GK_GRAPH_FMT_METIS, 1);
  if (errno != EINVAL || gk_fexists((char *)"gklib-negative-weight.graph"))
    return 44;
  graph.iadjwgt = NULL;
  graph.fadjwgt = zero_float_values;
  errno = 0;
  gk_graph_Write(&graph, (char *)"gklib-negative-weight.graph",
                 GK_GRAPH_FMT_METIS, 1);
  if (errno != EINVAL || gk_fexists((char *)"gklib-negative-weight.graph"))
    return 45;

  graph.fadjwgt = NULL;
  graph.ivwgts = negative_vertex_values;
  remove("gklib-negative-vertex.graph");
  errno = 0;
  gk_graph_Write(&graph, (char *)"gklib-negative-vertex.graph",
                 GK_GRAPH_FMT_METIS, 1);
  if (errno != EINVAL || gk_fexists((char *)"gklib-negative-vertex.graph"))
    return 36;
  graph.ivwgts = NULL;
  graph.fvwgts = negative_float_values;
  errno = 0;
  gk_graph_Write(&graph, (char *)"gklib-negative-vertex.graph",
                 GK_GRAPH_FMT_METIS, 1);
  if (errno != EINVAL || gk_fexists((char *)"gklib-negative-vertex.graph"))
    return 40;
  graph.fvwgts = NULL;
  graph.ivsizes = negative_vertex_values;
  errno = 0;
  gk_graph_Write(&graph, (char *)"gklib-negative-vertex.graph",
                 GK_GRAPH_FMT_METIS, 1);
  if (errno != EINVAL || gk_fexists((char *)"gklib-negative-vertex.graph"))
    return 37;
  graph.ivsizes = NULL;
  graph.fvsizes = negative_float_values;
  errno = 0;
  gk_graph_Write(&graph, (char *)"gklib-negative-vertex.graph",
                 GK_GRAPH_FMT_METIS, 1);
  if (errno != EINVAL || gk_fexists((char *)"gklib-negative-vertex.graph"))
    return 41;
  graph.fvsizes = NULL;

  graph.ivwgts = zero_weights;
  graph.ivsizes = zero_weights;
  errno = 0;
  gk_graph_Write(&graph, (char *)"gklib-zero-vertex.graph",
                 GK_GRAPH_FMT_METIS, 1);
  if (errno != 0 || !gk_fexists((char *)"gklib-zero-vertex.graph"))
    return 46;
  read_graph = gk_graph_Read((char *)"gklib-zero-vertex.graph",
                             GK_GRAPH_FMT_METIS, 0, 1, 0, 0, 0);
  if (read_graph == NULL || read_graph->ivwgts[0] != 0 ||
      read_graph->ivsizes[0] != 0)
    return 47;
  gk_graph_Free(&read_graph);
  graph.ivwgts = NULL;
  graph.ivsizes = NULL;
  graph.fvwgts = zero_float_values;
  graph.fvsizes = zero_float_values;
  errno = 0;
  gk_graph_Write(&graph, (char *)"gklib-zero-vertex.graph",
                 GK_GRAPH_FMT_METIS, 1);
  if (errno != 0)
    return 48;
  graph.fvwgts = NULL;
  graph.fvsizes = NULL;

  graph.iadjwgt = unequal_weights;
  graph.fadjwgt = float_weights;
  remove("gklib-ambiguous-weight.ijv");
  errno = 0;
  gk_graph_Write(&graph, (char *)"gklib-ambiguous-weight.ijv",
                 GK_GRAPH_FMT_IJV, 0);
  if (errno != EINVAL || gk_fexists((char *)"gklib-ambiguous-weight.ijv"))
    return 38;
  graph.iadjwgt = NULL;
  graph.fadjwgt = NULL;

  graph.ivwgts = positive_vertex_values;
  graph.fvwgts = float_weights;
  errno = 0;
  gk_graph_Write(&graph, (char *)"gklib-ambiguous-weight.ijv",
                 GK_GRAPH_FMT_IJV, 0);
  if (errno != EINVAL || gk_fexists((char *)"gklib-ambiguous-weight.ijv"))
    return 42;
  graph.ivwgts = NULL;
  graph.fvwgts = NULL;

  graph.ivsizes = positive_vertex_values;
  graph.fvsizes = float_weights;
  errno = 0;
  gk_graph_Write(&graph, (char *)"gklib-ambiguous-weight.ijv",
                 GK_GRAPH_FMT_IJV, 0);
  if (errno != EINVAL || gk_fexists((char *)"gklib-ambiguous-weight.ijv"))
    return 43;
  graph.ivsizes = NULL;
  graph.fvsizes = NULL;

  graph.nvtxs = 2;
  graph.xadj = xadj;
  graph.adjncy = adjncy;
  graph.iadjwgt = unequal_weights;
  remove("gklib-unequal-weight.graph");
  remove("gklib-signed-writer.ijv");
  remove("gklib-negative-weight.graph");
  remove("gklib-negative-vertex.graph");
  remove("gklib-zero-vertex.graph");
  remove("gklib-ambiguous-weight.ijv");
  gk_graph_Write(&graph, (char *)"gklib-unequal-weight.graph",
                 GK_GRAPH_FMT_METIS, 1);
  if (gk_fexists((char *)"gklib-unequal-weight.graph"))
    return 27;

  remove("gklib-writer.graph");
  remove("gklib-writer.graph.ijv");
  remove("gklib-writer.csr");
  remove("gklib-writer.binrow");
  remove("gklib-writer.bincol");
  remove("gklib-writer.ijv");
  remove("gklib-writer.bijv");
  remove("gklib-writer.metis");
  remove("gklib-writer.clu");
  remove("gklib-empty.bin");
  remove("gklib-invalid-writer.csr");
  remove("gklib-invalid-writer.graph");
  remove("gklib-directed-writer.graph");
  remove("gklib-directed-writer.metis");
  remove("gklib-unequal-weight.graph");
  remove("gklib-output-symbolic");
  remove("gklib-output-hard");
  remove("gklib-output-link-target");

  return 0;
}
