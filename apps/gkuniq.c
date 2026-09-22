/*!
\file  
\brief A program to test various implementations for unique.

\date 10/8/2020
\author George
*/

#include <GKlib.h>

#include "app_parse.h"

/* mem_flush() evicts an array from the cache between timed runs using the x86
   clflush/sfence instructions. These are exposed as the _mm_clflush/_mm_sfence
   intrinsics on every x86 compiler (GCC, Clang, MSVC, ICC), so we use those
   rather than GNU-specific inline asm. The intrinsics are x86-only, so the
   routine is enabled only when an x86 target is detected (and NO_X86 is not
   set); on other architectures mem_flush() compiles to a no-op. */
#if (defined(__x86_64__) || defined(_M_X64) || \
     defined(__i386__)   || defined(_M_IX86)) && !defined(NO_X86)
  #define GK_HAVE_X86_FLUSH 1
  #if defined(_MSC_VER)
    #include <intrin.h>
  #else
    #include <immintrin.h>
  #endif
#endif

/*************************************************************************/
/*! Data structures for the code */
/*************************************************************************/
typedef struct {
  ssize_t length, dupfactor;
} params_t;

/*************************************************************************/
/*! Constants */
/*************************************************************************/
#define CMD_HELP        10


/*************************************************************************/
/*! Local variables */
/*************************************************************************/
static struct gk_option long_options[] = {
  {"help",          0,      0,      CMD_HELP},
  {0,               0,      0,      0}
};


/*-------------------------------------------------------------------*/
/* Mini help  */
/*-------------------------------------------------------------------*/
static char helpstr[][100] = {
" ",
"Usage: gkuniq length dupfactor",
" ",
" Required parameters",
"  length",
"     The length of the base array.",
" ",
"  dupfactor",
"     The number of times the initial array is replicated.",
" ",
" Optional parameters",
"  -help",
"     Prints this message.",
""
};



/*************************************************************************/
/*! Function prototypes */
/*************************************************************************/
params_t *parse_cmdline(int argc, char *argv[]);
int unique_v1(int n, int *input, int *output);
int unique_v2(int n, int *input, int *output);
int unique_v3(int n, int *input, int *output, size_t *r_maxsize,
              int **r_hmap);
static int unique_hash_size(int n, size_t *r_size);
void mem_flush(const void *p, size_t allocation_size);

/*************************************************************************/
/*! A function to flush the cache associated with an array */
/**************************************************************************/
void mem_flush(const void *p, size_t allocation_size)
{
#ifdef GK_HAVE_X86_FLUSH
  const size_t cache_line = 64;
  const char *cp = (const char *)p;
  size_t i;

  if (p == NULL || allocation_size == 0)
    return;

  for (i = 0; i < allocation_size; i += cache_line)
    _mm_clflush(&cp[i]);

  _mm_sfence();
#else
  (void)p;
  (void)allocation_size;
#endif
}

/*************************************************************************/
/*! the entry point */
/**************************************************************************/
int main(int argc, char *argv[])
{
  int i, j, k;
  params_t *params;
  double tmr;
  int n, nunique, *input, *output;
  size_t maxsize=0;
  int *hmap=NULL;
 
  params = parse_cmdline(argc, argv);

  /* create the input data */
  n = params->length*params->dupfactor;
  input  = gk_imalloc(n, "input");
  output = gk_imalloc(n, "output");
  if (input == NULL || output == NULL) {
    gk_free((void **)&input, &output, LTERM);
    return EXIT_FAILURE;
  }
  for (i=0; i<params->length; i++) {
    k = RandomInRange(n);
    for (j=0; j<params->dupfactor; j++)
      input[j*params->length+i] = k;
  }

  gk_clearwctimer(tmr);
  gk_startwctimer(tmr);
  mem_flush(input, n*sizeof(int));
  mem_flush(output, n*sizeof(int));
  nunique = unique_v1(n, input, output);
  if (nunique < 0)
    goto failure;
  gk_stopwctimer(tmr);
  printf(" V1: nunique: %d, timer: %.5lf\n", nunique, gk_getwctimer(tmr));

  gk_clearwctimer(tmr);
  gk_startwctimer(tmr);
  mem_flush(input, n*sizeof(int));
  mem_flush(output, n*sizeof(int));
  nunique = unique_v2(n, input, output);
  if (nunique < 0)
    goto failure;
  gk_stopwctimer(tmr);
  printf(" V2: nunique: %d, timer: %.5lf\n", nunique, gk_getwctimer(tmr));

  gk_clearwctimer(tmr);
  gk_startwctimer(tmr);
  mem_flush(input, n*sizeof(int));
  mem_flush(output, n*sizeof(int));
  nunique = unique_v3(n, input, output, &maxsize, &hmap);
  if (nunique < 0)
    goto failure;
  gk_stopwctimer(tmr);
  printf("V3c: nunique: %d, timer: %.5lf\n", nunique, gk_getwctimer(tmr));

  gk_clearwctimer(tmr);
  gk_startwctimer(tmr);
  mem_flush(input, n*sizeof(int));
  mem_flush(output, n*sizeof(int));
  nunique = unique_v3(n, input, output, &maxsize, &hmap);
  if (nunique < 0)
    goto failure;
  gk_stopwctimer(tmr);
  printf("V3w: nunique: %d, timer: %.5lf\n", nunique, gk_getwctimer(tmr));

  gk_free((void **)&input, &output, &hmap, LTERM);

  return EXIT_SUCCESS;

failure:
  gk_free((void **)&input, &output, &hmap, LTERM);
  return EXIT_FAILURE;
}



/*************************************************************************/
/*! This is the entry point of the command-line argument parser */
/*************************************************************************/
params_t *parse_cmdline(int argc, char *argv[])
{
  int i;
  int c, option_index;
  params_t *params;

  params = (params_t *)gk_malloc(sizeof(params_t), "parse_cmdline: params");

  /* Parse the command line arguments  */
  while ((c = gk_getopt_long_only(argc, argv, "", long_options, &option_index)) != -1) {
    switch (c) {
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

  if (argc-gk_optind != 2) {
    printf("Unrecognized parameters.");
    for (i=0; strlen(helpstr[i]) > 0; i++)
      printf("%s\n", helpstr[i]);
    exit(EXIT_FAILURE);
  }

  params->length    = gk_app_parse_int(argv[gk_optind++], "length");
  params->dupfactor = gk_app_parse_int(argv[gk_optind++], "dupfactor");
  if (params->length <= 0 || params->dupfactor <= 0 ||
      (size_t)params->length > INT_MAX/(size_t)params->dupfactor ||
      (size_t)params->length >
          SIZE_MAX/sizeof(int)/(size_t)params->dupfactor)
    errexit("length and dupfactor must have a positive, representable product.\n");

  return params;
}


/*************************************************************************/
/*! gklib-sort based approach */
/*************************************************************************/
int unique_v1(int n, int *input, int *output)
{
  int i, j;

  if (n <= 0 || input == NULL || output == NULL)
    return -1;

  gk_isorti(n, input);

  output[0] = input[0];
  for (j=0, i=1; i<n; i++) {
    if (output[j] != input[i]) 
      output[++j] = input[i];
  }
  return j+1;
}


/*************************************************************************/
/*! Computes a power-of-two hash-table size for n integer entries.

    The returned capacity is at least twice the input length and is checked
    both for arithmetic overflow and for the byte size of the allocation.
*/
/*************************************************************************/
static int unique_hash_size(int n, size_t *r_size)
{
  size_t size, target;

  if (n <= 0 || r_size == NULL || (size_t)n > SIZE_MAX/2)
    return 0;
  target = 2*(size_t)n;
  for (size=1; size<target; size*=2) {
    if (size > SIZE_MAX/2)
      return 0;
  }
  if (size > SIZE_MAX/sizeof(int))
    return 0;

  *r_size = size;
  return 1;
}


/*************************************************************************/
/*! hash-table based approach */
/*************************************************************************/
int unique_v2(int n, int *input, int *output)
{
  int i, k, nuniq;
  size_t j, size, mask;
  int *hmap;

  if (input == NULL || output == NULL || !unique_hash_size(n, &size))
    return -1;
  mask = size-1;
  hmap = gk_ismalloc(size, -1, "hmap");
  if (hmap == NULL)
    return -1;

  for (nuniq=0, i=0; i<n; i++) {
    k = input[i];
    for (j=(k&mask); hmap[j]!=-1 && hmap[j]!=k; j=((j+1)&mask));
    if (hmap[j] == -1) {
      hmap[j] = k;
      output[nuniq++] = k;
    }
  }

  gk_free((void **)&hmap, LTERM);
  return nuniq;
}


/*************************************************************************/
/*! hash-table based approach, where the htable is most likely pre-allocated */
/*************************************************************************/
int unique_v3(int n, int *input, int *output, size_t *r_maxsize,
              int **r_hmap)
{
  int i, k, nuniq;
  size_t j, size, mask;
  int *hmap, *newhmap;

  if (input == NULL || output == NULL || r_maxsize == NULL ||
      r_hmap == NULL || !unique_hash_size(n, &size))
    return -1;
  mask = size-1;
  if (size > *r_maxsize) {
    newhmap = gk_ismalloc(size, -1, "hmap");
    if (newhmap == NULL)
      return -1;
    gk_free((void **)r_hmap, LTERM);
    hmap = *r_hmap = newhmap;
    *r_maxsize = size;
  }
  else {
    hmap = *r_hmap;
    gk_iset(size, -1, hmap);
  }

  for (nuniq=0, i=0; i<n; i++) {
    k = input[i];
    for (j=(k&mask); hmap[j]!=-1 && hmap[j]!=k; j=((j+1)&mask));
    if (hmap[j] == -1) {
      hmap[j] = k;
      output[nuniq++] = k;
    }
  }

  return nuniq;
}
