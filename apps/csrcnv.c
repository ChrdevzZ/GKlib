/*!
\file  
\brief A simple program to convert between different matrix formats that are supported
       by the gk_csr_Read/gk_csr_Write functions.

\date 5/30/2013
\author George
\version \verbatim $Id: csrcnv.c 15314 2013-10-05 16:50:50Z karypis $ \endverbatim
*/

#include <GKlib.h>

#include "app_parse.h"

/*************************************************************************/
/*! Data structures for the code */
/*************************************************************************/
typedef struct {
  int inf, outf;    /* input/output format */
  int numbering;    /* input numbering (output when applicable) */
  int readvals;     /* input values (output when applicable) */
  int writevals;    /* output values */
  int rshuf, cshuf; /* random shuffle of rows/columns */
  int symmetric;    /* a symmetric shuffle */
  int mincolfreq;   /* column prunning */
  int maxcolfreq;   /* column prunning */
  int minrowfreq;   /* row prunning */
  int maxrowfreq;   /* row prunning */
  float rownrmfltr; /* row-lowfilter threshold */
  int compactcols;  /* if to renumber columns to eliminate empty ones */
  int transpose;    /* transpose the output matrix */
  char *srenumber;  /* the iperm file for the symmetric renumbering */
  char *infile;     /* input file */
  char *outfile;    /* output file */
} params_t;


/*************************************************************************/
/*! Validates and normalizes a complete symmetric inverse permutation.

    Both zero-based and one-based files are accepted. The entries are changed
    to zero-based numbering only after their range and uniqueness have been
    checked.
*/
/*************************************************************************/
static int validate_iperm(int32_t *iperm, size_t n)
{
  size_t i, value;
  int base=1;
  int32_t *seen;

  if (iperm == NULL || n == 0 || n > INT32_MAX)
    return 0;
  for (i=0; i<n; i++) {
    if (iperm[i] == 0) {
      base = 0;
      break;
    }
  }

  seen = gk_i32smalloc(n, 0, "validate_iperm: seen");
  if (seen == NULL)
    return 0;
  for (i=0; i<n; i++) {
    if (iperm[i] < base || (size_t)(iperm[i]-base) >= n) {
      gk_free((void **)&seen, LTERM);
      return 0;
    }
    value = (size_t)(iperm[i]-base);
    if (seen[value]) {
      gk_free((void **)&seen, LTERM);
      return 0;
    }
    seen[value] = 1;
  }
  gk_free((void **)&seen, LTERM);

  if (base == 1) {
    for (i=0; i<n; i++)
      iperm[i]--;
  }

  return 1;
}


/*************************************************************************/
/*! Constants */
/*************************************************************************/
#define CMD_NUMONE        1
#define CMD_NOREADVALS    2
#define CMD_NOWRITEVALS   3
#define CMD_RSHUF         4
#define CMD_CSHUF         5
#define CMD_SYMMETRIC     6
#define CMD_MINCOLFREQ    7
#define CMD_MAXCOLFREQ    8
#define CMD_MINROWFREQ    9
#define CMD_MAXROWFREQ    10
#define CMD_ROWNRMFLTR    11
#define CMD_COMPACTCOLS   12
#define CMD_TRANSPOSE     13
#define CMD_SRENUMBER     14
#define CMD_HELP          100


/*************************************************************************/
/*! Local variables */
/*************************************************************************/
static struct gk_option long_options[] = {
  {"numone",      0,      0,      CMD_NUMONE},
  {"noreadvals",  0,      0,      CMD_NOREADVALS},
  {"nowritevals", 0,      0,      CMD_NOWRITEVALS},
  {"rshuf",       0,      0,      CMD_RSHUF},
  {"cshuf",       0,      0,      CMD_CSHUF},
  {"symmetric",   0,      0,      CMD_SYMMETRIC},
  {"mincolfreq",  1,      0,      CMD_MINCOLFREQ},
  {"maxcolfreq",  1,      0,      CMD_MAXCOLFREQ},
  {"minrowfreq",  1,      0,      CMD_MINROWFREQ},
  {"maxrowfreq",  1,      0,      CMD_MAXROWFREQ},
  {"rownrmfltr",  1,      0,      CMD_ROWNRMFLTR},
  {"compactcols", 0,      0,      CMD_COMPACTCOLS},
  {"transpose",   0,      0,      CMD_TRANSPOSE},
  {"srenumber",   1,      0,      CMD_SRENUMBER},
  {"help",        0,      0,      CMD_HELP},
  {0,             0,      0,      0}
};


/*-------------------------------------------------------------------*/
/* Mini help  */
/*-------------------------------------------------------------------*/
static char helpstr[][100] = {
" ",
"Usage: csrconv [options] <infile> <inf> <outfile> <outf>",
" ",
" Required parameters",
"  infile, outfile",
"     The name of the input/output CSR file.",
" ",
"  inf/outf",
"     The format of the input/output file.",
"     Supported values are:",
"        1  GK_CSR_FMT_CLUTO",
"        2  GK_CSR_FMT_CSR",
"        3  GK_CSR_FMT_METIS",
"        4  GK_CSR_FMT_BINROW",
"        6  GK_CSR_FMT_IJV",
"        7  GK_CSR_FMT_BIJV",
" ",
" Optional parameters",
"  -numone",
"     Specifies that the numbering of the input file starts from 1. ",
"     It only applies to CSR/IJV formats.",
" ",
"  -nowritevals",
"     Specifies that no values will be output.",
" ",
"  -noreadvals",
"     Specifies that the values will not be read when applicable.",
" ",
"  -rshuf",
"     Specifies that the rows will be randmly shuffled prior to output.",
" ",
"  -cshuf",
"     Specifies that the columns will be randmly shuffled prior to output.",
" ",
"  -symmetric",
"     Specifies that the row+column shuffling will be symmetric.",
" ",
"  -mincolfreq=int",
"     Used to prune infrequent columns.",
" ",
"  -maxcolfreq=int",
"     Used to prune frequent columns.",
" ",
"  -minrowfreq=int",
"     Used to prune infrequent rows.",
" ",
"  -maxrowfreq=int",
"     Used to prune frequent.",
" ",
"  -rownrmfltr=float",
"     The parameter to use for the row-wise low filter.",
" ",
"  -compactcols",
"     Specifies if empty columns will be removed and the columns renumbered.",
" ",
"  -transpose",
"     Specifies that the transposed matrix will be written.",
" ",
"  -srenumber=iperm-file",
"     Performs a symmetric renumbering based on the provided iperm file.",
" ",
"  -help",
"     Prints this message.",
""
};

static char shorthelpstr[][100] = {
" ",
"   Usage: csrconv [options] <infile> <inf> <outfile> <outf>",
"          use 'csrconv -help' for a summary of the options.",
""
};
 

/*************************************************************************/
/*! This is the entry point of the command-line argument parser */
/*************************************************************************/
params_t *parse_cmdline(int argc, char *argv[])
{
  int i;
  int c, option_index;
  params_t *params;

  params = (params_t *)gk_malloc(sizeof(params_t), "parse_cmdline: params");

  /* initialize the params data structure */
  params->numbering = 0;
  params->readvals  = 1;
  params->writevals = 1;
  params->rshuf     = 0;
  params->cshuf     = 0;
  params->symmetric = 0;
  params->transpose = 0;
  params->srenumber = NULL;

  params->mincolfreq  = -1;
  params->minrowfreq  = -1;
  params->maxcolfreq  = -1;
  params->maxrowfreq  = -1;
  params->rownrmfltr  = -1;
  params->compactcols = 0;

  params->inf       = -1;
  params->outf      = -1;
  params->infile    = NULL;
  params->outfile   = NULL;


  /* Parse the command line arguments  */
  while ((c = gk_getopt_long_only(argc, argv, "", long_options, &option_index)) != -1) {
    switch (c) {
      case CMD_NUMONE:
        params->numbering = 1;
        break;
      case CMD_NOREADVALS:
        params->readvals = 0;
        break;
      case CMD_NOWRITEVALS:
        params->writevals = 0;
        break;
      case CMD_RSHUF:
        params->rshuf = 1;
        break;
      case CMD_CSHUF:
        params->cshuf = 1;
        break;
      case CMD_SYMMETRIC:
        params->symmetric = 1;
        break;
      case CMD_TRANSPOSE:
        params->transpose = 1;
        break;


      case CMD_MINCOLFREQ:
        params->mincolfreq = gk_app_parse_int(gk_optarg, "mincolfreq");
        break;
      case CMD_MINROWFREQ:
        params->minrowfreq = gk_app_parse_int(gk_optarg, "minrowfreq");
        break;
      case CMD_MAXCOLFREQ:
        params->maxcolfreq = gk_app_parse_int(gk_optarg, "maxcolfreq");
        break;
      case CMD_MAXROWFREQ:
        params->maxrowfreq = gk_app_parse_int(gk_optarg, "maxrowfreq");
        break;
      case CMD_ROWNRMFLTR:
        params->rownrmfltr = gk_app_parse_float(gk_optarg, "rownrmfltr");
        break;
      case CMD_COMPACTCOLS:
        params->compactcols = 1;
        break;

      case CMD_SRENUMBER:
        if (gk_optarg) {
          params->srenumber = gk_strdup(gk_optarg);
          if (!gk_fexists(params->srenumber))
            errexit("srenumber file %s does not exist.\n", params->srenumber);
        }
        break;

      case CMD_HELP:
        for (i=0; strlen(helpstr[i]) > 0; i++)
          printf("%s\n", helpstr[i]);
        exit(EXIT_SUCCESS);
        break;
      case '?':
      default:
        printf("Illegal command-line option(s)\nUse %s -help for a summary of the options.\n", argv[0]);
        exit(EXIT_FAILURE);
    }
  }

  if (argc-gk_optind != 4) {
    printf("Unrecognized parameters.");
    for (i=0; strlen(shorthelpstr[i]) > 0; i++)
      printf("%s\n", shorthelpstr[i]);
    exit(EXIT_FAILURE);
  }

  params->infile  = gk_strdup(argv[gk_optind++]);
  params->inf     = gk_app_parse_int(argv[gk_optind++], "inf");
  params->outfile = gk_strdup(argv[gk_optind++]);
  params->outf    = gk_app_parse_int(argv[gk_optind++], "outf");

  if (!gk_fexists(params->infile))
    errexit("input file %s does not exist.\n", params->infile);

  return params;
}


/*************************************************************************/
/*! the entry point */
/**************************************************************************/
int main(int argc, char *argv[])
{
  int what;
  params_t *params;
  gk_csr_t *mat, *mat1, *smat;
 
  /* get command-line options */
  params = parse_cmdline(argc, argv);

  /* read the data */
  mat = gk_csr_Read(params->infile, params->inf, params->readvals, params->numbering);
  if (mat == NULL)
    return EXIT_FAILURE;

  /* deal with weird transformations */
  if (params->mincolfreq != -1 || params->maxcolfreq != -1) {
    params->mincolfreq = (params->mincolfreq == -1 ? 0 : params->mincolfreq);
    params->maxcolfreq = (params->maxcolfreq == -1 ? mat->nrows : params->maxcolfreq);

    printf("Column prune: %d %d; nnz: %zd => ", 
        params->mincolfreq, params->maxcolfreq, mat->rowptr[mat->nrows]);
    mat1 = gk_csr_Prune(mat, GK_CSR_COL, params->mincolfreq, params->maxcolfreq);
    gk_csr_Free(&mat);
    mat = mat1;
    mat1 = NULL;

    printf("%zd\n", mat->rowptr[mat->nrows]);
  }
  
  if (params->minrowfreq != -1 || params->maxrowfreq != -1) {
    params->minrowfreq = (params->minrowfreq == -1 ? 0 : params->minrowfreq);
    params->maxrowfreq = (params->maxrowfreq == -1 ? mat->ncols : params->maxrowfreq);

    printf("Row prune: %d %d; nnz: %zd => ", 
        params->minrowfreq, params->maxrowfreq, mat->rowptr[mat->nrows]);
    mat1 = gk_csr_Prune(mat, GK_CSR_ROW, params->minrowfreq, params->maxrowfreq);
    gk_csr_Free(&mat);
    mat = mat1;
    mat1 = NULL;

    printf("%zd\n", mat->rowptr[mat->nrows]);
  }

  if (params->rownrmfltr >= 0.0) {
    //gk_csr_Scale(mat, GK_CSR_LOG);
    //gk_csr_Scale(mat, GK_CSR_IDF2);

    printf("Row low filter: %f; nnz: %zd => ", params->rownrmfltr, mat->rowptr[mat->nrows]);
    mat1 = gk_csr_LowFilter(mat, GK_CSR_ROW, 2, params->rownrmfltr);
    gk_csr_Normalize(mat1, GK_CSR_ROW, 2);

    gk_csr_Free(&mat);
    mat = mat1;
    mat1 = NULL;

    printf("%zd\n", mat->rowptr[mat->nrows]);
  }

  if (params->compactcols) {
    printf("Compacting columns: %d => ", mat->ncols);
    gk_csr_CompactColumns(mat);
    printf("%d\n", mat->ncols);
  }


  if (params->rshuf || params->cshuf) {
    if (params->rshuf && params->cshuf)
      what = GK_CSR_ROWCOL;
    else if (params->rshuf)
      what = GK_CSR_ROW;
    else
      what = GK_CSR_COL;

    smat = gk_csr_Shuffle(mat, what, params->symmetric);
    gk_csr_Free(&mat);
    mat = smat;
  }


  if (params->srenumber) {
    size_t nlines;
    int32_t *iperm;
    gk_csr_t *smat;

    iperm = gk_i32readfile(params->srenumber, &nlines);
    if (iperm == NULL || mat->nrows != mat->ncols ||
        nlines != (size_t)mat->nrows || !validate_iperm(iperm, nlines)) {
      gk_free((void **)&iperm, LTERM);
      gk_csr_Free(&mat);
      errexit("The srenumber file is not a complete matrix permutation.\n");
      return EXIT_FAILURE;
    }

    smat = gk_csr_ReorderSymmetric(mat, iperm, NULL);
    if (smat == NULL) {
      gk_free((void **)&iperm, LTERM);
      gk_csr_Free(&mat);
      return EXIT_FAILURE;
    }
    gk_csr_Free(&mat);
    mat = smat;

    gk_free((void **)&iperm, LTERM);
  }

  if (params->writevals && mat->rowval == NULL) 
    mat->rowval = gk_fsmalloc(mat->rowptr[mat->nrows], 1.0, "mat->rowval");

  if (params->transpose) {
    mat1 = gk_csr_Transpose(mat);
    gk_csr_Free(&mat);
    mat = mat1;
    mat1 = NULL;
  }



  gk_csr_Write(mat, params->outfile, params->outf, params->writevals, 0);

  gk_csr_Free(&mat);

}

