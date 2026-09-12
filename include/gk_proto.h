/*!
\file gk_proto.h
\brief This file contains function prototypes

\date   Started 3/27/2007
\author George
\version\verbatim $Id: gk_proto.h 22010 2018-05-14 20:20:26Z karypis $ \endverbatim
*/

#ifndef _GK_PROTO_H_
#define _GK_PROTO_H_

#ifdef __cplusplus
extern "C" {
#endif

/*-------------------------------------------------------------
 * blas.c 
 *-------------------------------------------------------------*/
GK_MKBLAS_PROTO_EX(gk_c,   char,     int,      GKLIB_EXPORT)
GK_MKBLAS_PROTO_EX(gk_i,   int,      int,      GKLIB_EXPORT)
GK_MKBLAS_PROTO_EX(gk_i8,  int8_t,   int8_t,   GKLIB_EXPORT)
GK_MKBLAS_PROTO_EX(gk_i16, int16_t,  int16_t,  GKLIB_EXPORT)
GK_MKBLAS_PROTO_EX(gk_i32, int32_t,  int32_t,  GKLIB_EXPORT)
GK_MKBLAS_PROTO_EX(gk_i64, int64_t,  int64_t,  GKLIB_EXPORT)
GK_MKBLAS_PROTO_EX(gk_z,   ssize_t,  ssize_t,  GKLIB_EXPORT)
GK_MKBLAS_PROTO_EX(gk_zu,  size_t,   size_t,   GKLIB_EXPORT)
GK_MKBLAS_PROTO_EX(gk_f,   float,    float,    GKLIB_EXPORT)
GK_MKBLAS_PROTO_EX(gk_d,   double,   double,   GKLIB_EXPORT)
GK_MKBLAS_PROTO_EX(gk_idx, gk_idx_t, gk_idx_t, GKLIB_EXPORT)




/*-------------------------------------------------------------
 * io.c
 *-------------------------------------------------------------*/
GKLIB_EXPORT FILE *gk_fopen(char *, char *, const char *);
GKLIB_EXPORT void gk_fclose(FILE *);
GKLIB_EXPORT ssize_t gk_read(int fd, void *vbuf, size_t count);
GKLIB_EXPORT ssize_t gk_write(int fd, void *vbuf, size_t count);
GKLIB_EXPORT ssize_t gk_getline(char **lineptr, size_t *n, FILE *stream);
GKLIB_EXPORT char **gk_readfile(char *fname, size_t *r_nlines);
GKLIB_EXPORT int32_t *gk_i32readfile(char *fname, size_t *r_nlines);
GKLIB_EXPORT int64_t *gk_i64readfile(char *fname, size_t *r_nlines);
GKLIB_EXPORT ssize_t *gk_zreadfile(char *fname, size_t *r_nlines);
GKLIB_EXPORT char *gk_creadfilebin(char *fname, size_t *r_nelmnts);
GKLIB_EXPORT size_t gk_cwritefilebin(char *fname, size_t n, char *a);
GKLIB_EXPORT int32_t *gk_i32readfilebin(char *fname, size_t *r_nelmnts);
GKLIB_EXPORT size_t gk_i32writefilebin(char *fname, size_t n, int32_t *a);
GKLIB_EXPORT int64_t *gk_i64readfilebin(char *fname, size_t *r_nelmnts);
GKLIB_EXPORT size_t gk_i64writefilebin(char *fname, size_t n, int64_t *a);
GKLIB_EXPORT ssize_t *gk_zreadfilebin(char *fname, size_t *r_nelmnts);
GKLIB_EXPORT size_t gk_zwritefilebin(char *fname, size_t n, ssize_t *a);
GKLIB_EXPORT float *gk_freadfilebin(char *fname, size_t *r_nelmnts);
GKLIB_EXPORT size_t gk_fwritefilebin(char *fname, size_t n, float *a);
GKLIB_EXPORT double *gk_dreadfilebin(char *fname, size_t *r_nelmnts);
GKLIB_EXPORT size_t gk_dwritefilebin(char *fname, size_t n, double *a);




/*-------------------------------------------------------------
 * fs.c
 *-------------------------------------------------------------*/
GKLIB_EXPORT int gk_fexists(char *);
GKLIB_EXPORT int gk_dexists(char *);
GKLIB_EXPORT ssize_t gk_getfsize(char *);
GKLIB_EXPORT void gk_getfilestats(char *fname, size_t *r_nlines, size_t *r_ntokens,
          size_t *r_max_nlntokens, size_t *r_nbytes);
GKLIB_EXPORT char *gk_getbasename(char *path);
GKLIB_EXPORT char *gk_getextname(char *path);
GKLIB_EXPORT char *gk_getfilename(char *path);
GKLIB_EXPORT char *gk_getpathname(char *path);
GKLIB_EXPORT int gk_mkpath(char *);
GKLIB_EXPORT int gk_rmpath(char *);



/*-------------------------------------------------------------
 * memory.c
 *-------------------------------------------------------------*/
GK_MKALLOC_PROTO_EX(gk_c,    char,       GKLIB_EXPORT)
GK_MKALLOC_PROTO_EX(gk_i,    int,        GKLIB_EXPORT)
GK_MKALLOC_PROTO_EX(gk_i8,   int8_t,     GKLIB_EXPORT)
GK_MKALLOC_PROTO_EX(gk_i16,  int16_t,    GKLIB_EXPORT)
GK_MKALLOC_PROTO_EX(gk_i32,  int32_t,    GKLIB_EXPORT)
GK_MKALLOC_PROTO_EX(gk_i64,  int64_t,    GKLIB_EXPORT)
GK_MKALLOC_PROTO_EX(gk_ui8,  uint8_t,    GKLIB_EXPORT)
GK_MKALLOC_PROTO_EX(gk_ui16, uint16_t,   GKLIB_EXPORT)
GK_MKALLOC_PROTO_EX(gk_ui32, uint32_t,   GKLIB_EXPORT)
GK_MKALLOC_PROTO_EX(gk_ui64, uint64_t,   GKLIB_EXPORT)
GK_MKALLOC_PROTO_EX(gk_z,    ssize_t,    GKLIB_EXPORT)
GK_MKALLOC_PROTO_EX(gk_zu,   size_t,     GKLIB_EXPORT)
GK_MKALLOC_PROTO_EX(gk_f,    float,      GKLIB_EXPORT)
GK_MKALLOC_PROTO_EX(gk_d,    double,     GKLIB_EXPORT)
GK_MKALLOC_PROTO_EX(gk_idx,  gk_idx_t,   GKLIB_EXPORT)

GK_MKALLOC_PROTO_EX(gk_ckv,   gk_ckv_t,   GKLIB_EXPORT)
GK_MKALLOC_PROTO_EX(gk_ikv,   gk_ikv_t,   GKLIB_EXPORT)
GK_MKALLOC_PROTO_EX(gk_i8kv,  gk_i8kv_t,  GKLIB_EXPORT)
GK_MKALLOC_PROTO_EX(gk_i16kv, gk_i16kv_t, GKLIB_EXPORT)
GK_MKALLOC_PROTO_EX(gk_i32kv, gk_i32kv_t, GKLIB_EXPORT)
GK_MKALLOC_PROTO_EX(gk_i64kv, gk_i64kv_t, GKLIB_EXPORT)
GK_MKALLOC_PROTO_EX(gk_zkv,   gk_zkv_t,   GKLIB_EXPORT)
GK_MKALLOC_PROTO_EX(gk_zukv,  gk_zukv_t,  GKLIB_EXPORT)
GK_MKALLOC_PROTO_EX(gk_fkv,   gk_fkv_t,   GKLIB_EXPORT)
GK_MKALLOC_PROTO_EX(gk_dkv,   gk_dkv_t,   GKLIB_EXPORT)
GK_MKALLOC_PROTO_EX(gk_skv,   gk_skv_t,   GKLIB_EXPORT)
GK_MKALLOC_PROTO_EX(gk_idxkv, gk_idxkv_t, GKLIB_EXPORT)

GKLIB_EXPORT void   gk_AllocMatrix(void ***, size_t, size_t , size_t);
GKLIB_EXPORT void   gk_FreeMatrix(void ***, size_t, size_t);
GKLIB_EXPORT int    gk_malloc_init(void);
GKLIB_EXPORT void   gk_malloc_cleanup(int showstats);
GKLIB_EXPORT void  *gk_malloc(size_t nbytes, char *msg);
GKLIB_EXPORT void  *gk_realloc(void *oldptr, size_t nbytes, char *msg);
GKLIB_EXPORT void   gk_free(void **ptr1,...);
GKLIB_EXPORT size_t gk_GetCurMemoryUsed(void);
GKLIB_EXPORT size_t gk_GetMaxMemoryUsed(void);
GKLIB_EXPORT void   gk_GetVMInfo(size_t *vmsize, size_t *vmrss);
GKLIB_EXPORT size_t gk_GetProcVmPeak(void);



/*-------------------------------------------------------------
 * seq.c
 *-------------------------------------------------------------*/
GKLIB_EXPORT gk_seq_t *gk_seq_ReadGKMODPSSM(char *file_name);
GKLIB_EXPORT gk_i2cc2i_t *gk_i2cc2i_create_common(char *alphabet);
GKLIB_EXPORT void gk_seq_init(gk_seq_t *seq);



/*-------------------------------------------------------------
 * error.c
 *-------------------------------------------------------------*/
GKLIB_EXPORT void gk_set_exit_on_error(int value);
GKLIB_EXPORT void errexit(char *,...);
GKLIB_EXPORT void gk_errexit(int signum, char *,...);
GKLIB_EXPORT int gk_sigtrap(void);
GKLIB_EXPORT int gk_siguntrap(void);
GKLIB_EXPORT void gk_sigthrow(int signum);
GKLIB_EXPORT void gk_SetSignalHandlers(void);
GKLIB_EXPORT void gk_UnsetSignalHandlers(void);
GKLIB_EXPORT void gk_NonLocalExit_Handler(int signum);
GKLIB_EXPORT char *gk_strerror(int errnum);
GKLIB_EXPORT void PrintBackTrace(void);


/*-------------------------------------------------------------
 * util.c
 *-------------------------------------------------------------*/
GKLIB_EXPORT void  gk_RandomPermute(size_t, int *, int);
GKLIB_EXPORT void  gk_array2csr(size_t n, size_t range, int *array, int *ptr, int *ind);
GKLIB_EXPORT int   gk_log2(int);
GKLIB_EXPORT int   gk_ispow2(int);
GKLIB_EXPORT float gk_flog2(float);


/*-------------------------------------------------------------
 * time.c
 *-------------------------------------------------------------*/
GKLIB_EXPORT gk_wclock_t gk_WClockSeconds(void);
GKLIB_EXPORT double gk_CPUSeconds(void);

/*-------------------------------------------------------------
 * string.c
 *-------------------------------------------------------------*/
GKLIB_EXPORT char   *gk_strchr_replace(char *str, char *fromlist, char *tolist);
GKLIB_EXPORT int     gk_strstr_replace(char *str, char *pattern, char *replacement, char *options, char **new_str);
GKLIB_EXPORT char   *gk_strtprune(char *, char *);
GKLIB_EXPORT char   *gk_strhprune(char *, char *);
GKLIB_EXPORT char   *gk_strtoupper(char *);
GKLIB_EXPORT char   *gk_strtolower(char *);
GKLIB_EXPORT char   *gk_strdup(char *orgstr);
GKLIB_EXPORT int     gk_strcasecmp(char *s1, char *s2);
GKLIB_EXPORT int     gk_strrcmp(char *s1, char *s2);
GKLIB_EXPORT char   *gk_time2str(time_t time);
GKLIB_EXPORT time_t  gk_str2time(char *str);
GKLIB_EXPORT int     gk_GetStringID(gk_StringMap_t *strmap, char *key);



/*-------------------------------------------------------------
 * sort.c 
 *-------------------------------------------------------------*/
GKLIB_EXPORT void gk_csorti(size_t, char *);
GKLIB_EXPORT void gk_csortd(size_t, char *);
GKLIB_EXPORT void gk_isorti(size_t, int *);
GKLIB_EXPORT void gk_isortd(size_t, int *);
GKLIB_EXPORT void gk_i32sorti(size_t, int32_t *);
GKLIB_EXPORT void gk_i32sortd(size_t, int32_t *);
GKLIB_EXPORT void gk_i64sorti(size_t, int64_t *);
GKLIB_EXPORT void gk_i64sortd(size_t, int64_t *);
GKLIB_EXPORT void gk_ui32sorti(size_t, uint32_t *);
GKLIB_EXPORT void gk_ui32sortd(size_t, uint32_t *);
GKLIB_EXPORT void gk_ui64sorti(size_t, uint64_t *);
GKLIB_EXPORT void gk_ui64sortd(size_t, uint64_t *);
GKLIB_EXPORT void gk_fsorti(size_t, float *);
GKLIB_EXPORT void gk_fsortd(size_t, float *);
GKLIB_EXPORT void gk_dsorti(size_t, double *);
GKLIB_EXPORT void gk_dsortd(size_t, double *);
GKLIB_EXPORT void gk_idxsorti(size_t, gk_idx_t *);
GKLIB_EXPORT void gk_idxsortd(size_t, gk_idx_t *);
GKLIB_EXPORT void gk_ckvsorti(size_t, gk_ckv_t *);
GKLIB_EXPORT void gk_ckvsortd(size_t, gk_ckv_t *);
GKLIB_EXPORT void gk_ikvsorti(size_t, gk_ikv_t *);
GKLIB_EXPORT void gk_ikvsortd(size_t, gk_ikv_t *);
GKLIB_EXPORT void gk_i32kvsorti(size_t, gk_i32kv_t *);
GKLIB_EXPORT void gk_i32kvsortd(size_t, gk_i32kv_t *);
GKLIB_EXPORT void gk_i64kvsorti(size_t, gk_i64kv_t *);
GKLIB_EXPORT void gk_i64kvsortd(size_t, gk_i64kv_t *);
GKLIB_EXPORT void gk_zkvsorti(size_t, gk_zkv_t *);
GKLIB_EXPORT void gk_zkvsortd(size_t, gk_zkv_t *);
GKLIB_EXPORT void gk_zukvsorti(size_t, gk_zukv_t *);
GKLIB_EXPORT void gk_zukvsortd(size_t, gk_zukv_t *);
GKLIB_EXPORT void gk_fkvsorti(size_t, gk_fkv_t *);
GKLIB_EXPORT void gk_fkvsortd(size_t, gk_fkv_t *);
GKLIB_EXPORT void gk_dkvsorti(size_t, gk_dkv_t *);
GKLIB_EXPORT void gk_dkvsortd(size_t, gk_dkv_t *);
GKLIB_EXPORT void gk_skvsorti(size_t, gk_skv_t *);
GKLIB_EXPORT void gk_skvsortd(size_t, gk_skv_t *);
GKLIB_EXPORT void gk_idxkvsorti(size_t, gk_idxkv_t *);
GKLIB_EXPORT void gk_idxkvsortd(size_t, gk_idxkv_t *);


/*-------------------------------------------------------------
 * Selection routines
 *-------------------------------------------------------------*/
GKLIB_EXPORT int  gk_dfkvkselect(size_t, int, gk_fkv_t *);
GKLIB_EXPORT int  gk_ifkvkselect(size_t, int, gk_fkv_t *);


/*-------------------------------------------------------------
 * Priority queue 
 *-------------------------------------------------------------*/
GK_MKPQUEUE_PROTO_EX(gk_ipq,   gk_ipq_t,   int,      gk_idx_t, GKLIB_EXPORT)
GK_MKPQUEUE_PROTO_EX(gk_i32pq, gk_i32pq_t, int32_t,  gk_idx_t, GKLIB_EXPORT)
GK_MKPQUEUE_PROTO_EX(gk_i64pq, gk_i64pq_t, int64_t,  gk_idx_t, GKLIB_EXPORT)
GK_MKPQUEUE_PROTO_EX(gk_fpq,   gk_fpq_t,   float,    gk_idx_t, GKLIB_EXPORT)
GK_MKPQUEUE_PROTO_EX(gk_dpq,   gk_dpq_t,   double,   gk_idx_t, GKLIB_EXPORT)
GK_MKPQUEUE_PROTO_EX(gk_idxpq, gk_idxpq_t, gk_idx_t, gk_idx_t, GKLIB_EXPORT)


/*-------------------------------------------------------------
 * HTable routines
 *-------------------------------------------------------------*/
GKLIB_EXPORT gk_HTable_t *HTable_Create(int nelements);
GKLIB_EXPORT void         HTable_Reset(gk_HTable_t *htable);
GKLIB_EXPORT void         HTable_Resize(gk_HTable_t *htable, int nelements);
GKLIB_EXPORT void         HTable_Insert(gk_HTable_t *htable, int key, int val);
GKLIB_EXPORT void         HTable_Delete(gk_HTable_t *htable, int key);
GKLIB_EXPORT int          HTable_Search(gk_HTable_t *htable, int key);
GKLIB_EXPORT int          HTable_GetNext(gk_HTable_t *htable, int key, int *val, int type);
GKLIB_EXPORT int          HTable_SearchAndDelete(gk_HTable_t *htable, int key);
GKLIB_EXPORT void         HTable_Destroy(gk_HTable_t *htable);
GKLIB_EXPORT int          HTable_HFunction(int nelements, int key);
 

/*-------------------------------------------------------------
 * Tokenizer routines
 *-------------------------------------------------------------*/
GKLIB_EXPORT void gk_strtokenize(char *line, char *delim, gk_Tokens_t *tokens);
GKLIB_EXPORT void gk_freetokenslist(gk_Tokens_t *tokens);

/*-------------------------------------------------------------
 * Encoder/Decoder
 *-------------------------------------------------------------*/
GKLIB_EXPORT void encodeblock(unsigned char *in, unsigned char *out);
GKLIB_EXPORT void decodeblock(unsigned char *in, unsigned char *out);
GKLIB_EXPORT void GKEncodeBase64(int nbytes, unsigned char *inbuffer, unsigned char *outbuffer);
GKLIB_EXPORT void GKDecodeBase64(int nbytes, unsigned char *inbuffer, unsigned char *outbuffer);


/*-------------------------------------------------------------
 * random.c
 *-------------------------------------------------------------*/
GK_MKRANDOM_PROTO_EX(gk_c,   size_t, char,     GKLIB_EXPORT)
GK_MKRANDOM_PROTO_EX(gk_i,   size_t, int,      GKLIB_EXPORT)
GK_MKRANDOM_PROTO_EX(gk_i32, size_t, int32_t,  GKLIB_EXPORT)
GK_MKRANDOM_PROTO_EX(gk_f,   size_t, float,    GKLIB_EXPORT)
GK_MKRANDOM_PROTO_EX(gk_d,   size_t, double,   GKLIB_EXPORT)
GK_MKRANDOM_PROTO_EX(gk_idx, size_t, gk_idx_t, GKLIB_EXPORT)
GK_MKRANDOM_PROTO_EX(gk_z,   size_t, ssize_t,  GKLIB_EXPORT)
GK_MKRANDOM_PROTO_EX(gk_zu,  size_t, size_t,   GKLIB_EXPORT)
GKLIB_EXPORT void gk_randinit(uint64_t);
GKLIB_EXPORT uint64_t gk_randint64(void);
GKLIB_EXPORT uint32_t gk_randint32(void);


/*-------------------------------------------------------------
 * CSR-related functions
 *-------------------------------------------------------------*/
GKLIB_EXPORT gk_csr_t *gk_csr_Create(void);
GKLIB_EXPORT void gk_csr_Init(gk_csr_t *mat);
GKLIB_EXPORT void gk_csr_Free(gk_csr_t **mat);
GKLIB_EXPORT void gk_csr_FreeContents(gk_csr_t *mat);
GKLIB_EXPORT gk_csr_t *gk_csr_Dup(gk_csr_t *mat);
GKLIB_EXPORT gk_csr_t *gk_csr_ExtractSubmatrix(gk_csr_t *mat, int rstart, int nrows);
GKLIB_EXPORT gk_csr_t *gk_csr_ExtractRows(gk_csr_t *mat, int nrows, int *rind);
GKLIB_EXPORT gk_csr_t *gk_csr_ExtractPartition(gk_csr_t *mat, int *part, int pid);
GKLIB_EXPORT gk_csr_t **gk_csr_Split(gk_csr_t *mat, int *color);
GKLIB_EXPORT int gk_csr_DetermineFormat(char *filename, int format);
GKLIB_EXPORT gk_csr_t *gk_csr_Read(char *filename, int format, int readvals, int numbering);
GKLIB_EXPORT void gk_csr_Write(gk_csr_t *mat, char *filename, int format, int writevals, int numbering);
GKLIB_EXPORT gk_csr_t *gk_csr_Prune(gk_csr_t *mat, int what, int minf, int maxf);
GKLIB_EXPORT gk_csr_t *gk_csr_LowFilter(gk_csr_t *mat, int what, int norm, float fraction);
GKLIB_EXPORT gk_csr_t *gk_csr_TopKPlusFilter(gk_csr_t *mat, int what, int topk, float keepval);
GKLIB_EXPORT gk_csr_t *gk_csr_ZScoreFilter(gk_csr_t *mat, int what, float zscore);
GKLIB_EXPORT void gk_csr_CompactColumns(gk_csr_t *mat);
GKLIB_EXPORT void gk_csr_SortIndices(gk_csr_t *mat, int what);
GKLIB_EXPORT void gk_csr_CreateIndex(gk_csr_t *mat, int what);
GKLIB_EXPORT void gk_csr_Normalize(gk_csr_t *mat, int what, int norm);
GKLIB_EXPORT void gk_csr_Scale(gk_csr_t *mat, int type);
GKLIB_EXPORT void gk_csr_ComputeSums(gk_csr_t *mat, int what);
GKLIB_EXPORT void gk_csr_ComputeNorms(gk_csr_t *mat, int what);
GKLIB_EXPORT void gk_csr_ComputeSquaredNorms(gk_csr_t *mat, int what);
GKLIB_EXPORT gk_csr_t *gk_csr_Shuffle(gk_csr_t *mat, int what, int summetric);
GKLIB_EXPORT gk_csr_t *gk_csr_Transpose(gk_csr_t *mat);
GKLIB_EXPORT float gk_csr_ComputeSimilarity(gk_csr_t *mat, int i1, int i2, int what, int simtype);
GKLIB_EXPORT float gk_csr_ComputePairSimilarity(gk_csr_t *mat_a, gk_csr_t *mat_b, int i1, int i2, int what, int simtype);
GKLIB_EXPORT int gk_csr_GetSimilarRows(gk_csr_t *mat, int nqterms, int *qind, float *qval,
        int simtype, int nsim, float minsim, gk_fkv_t *hits, int *_imarker,
        gk_fkv_t *i_cand);
GKLIB_EXPORT int gk_csr_FindConnectedComponents(gk_csr_t *mat, int32_t *cptr, int32_t *cind,
        int32_t *cids);
GKLIB_EXPORT gk_csr_t *gk_csr_MakeSymmetric(gk_csr_t *mat, int op);
GKLIB_EXPORT gk_csr_t *gk_csr_ReorderSymmetric(gk_csr_t *mat, int32_t *perm, int32_t *iperm);
GKLIB_EXPORT void gk_csr_ComputeBFSOrderingSymmetric(gk_csr_t *mat, int maxdegree, int v,
          int32_t **r_perm, int32_t **r_iperm);
GKLIB_EXPORT void gk_csr_ComputeBestFOrderingSymmetric(gk_csr_t *mat, int v, int type,
          int32_t **r_perm, int32_t **r_iperm);


/* itemsets.c */
GKLIB_EXPORT void gk_find_frequent_itemsets(int ntrans, ssize_t *tranptr, int *tranind,
        int minfreq, int maxfreq, int minlen, int maxlen,
        void (*process_itemset)(void *stateptr, int nitems, int *itemind,
                                int ntrans, int *tranind),
        void *stateptr);


/* evaluate.c */
GKLIB_EXPORT float ComputeAccuracy(int n, gk_fkv_t *list);
GKLIB_EXPORT float ComputeROCn(int n, int maxN, gk_fkv_t *list);
GKLIB_EXPORT float ComputeMedianRFP(int n, gk_fkv_t *list);
GKLIB_EXPORT float ComputeMean (int n, float *values);
GKLIB_EXPORT float ComputeStdDev(int  n, float *values);


/* mcore.c */
GKLIB_EXPORT gk_mcore_t *gk_mcoreCreate(size_t coresize);
GKLIB_EXPORT gk_mcore_t *gk_gkmcoreCreate(void);
GKLIB_EXPORT void gk_mcoreDestroy(gk_mcore_t **r_mcore, int showstats);
GKLIB_EXPORT void gk_gkmcoreDestroy(gk_mcore_t **r_mcore, int showstats);
GKLIB_EXPORT void *gk_mcoreMalloc(gk_mcore_t *mcore, size_t nbytes);
GKLIB_EXPORT void gk_mcorePush(gk_mcore_t *mcore);
GKLIB_EXPORT void gk_gkmcorePush(gk_mcore_t *mcore);
GKLIB_EXPORT void gk_mcorePop(gk_mcore_t *mcore);
GKLIB_EXPORT void gk_gkmcorePop(gk_mcore_t *mcore);
GKLIB_EXPORT void gk_mcoreAdd(gk_mcore_t *mcore, int type, size_t nbytes, void *ptr);
GKLIB_EXPORT void gk_gkmcoreAdd(gk_mcore_t *mcore, int type, size_t nbytes, void *ptr);
GKLIB_EXPORT void gk_mcoreDel(gk_mcore_t *mcore, void *ptr);
GKLIB_EXPORT void gk_gkmcoreDel(gk_mcore_t *mcore, void *ptr);

/* rw.c */
GKLIB_EXPORT int gk_rw_PageRank(gk_csr_t *mat, float lamda, float eps, int max_niter, float *pr);


/* graph.c */
GKLIB_EXPORT gk_graph_t *gk_graph_Create(void);
GKLIB_EXPORT void gk_graph_Init(gk_graph_t *graph);
GKLIB_EXPORT void gk_graph_Free(gk_graph_t **graph);
GKLIB_EXPORT void gk_graph_FreeContents(gk_graph_t *graph);
GKLIB_EXPORT gk_graph_t *gk_graph_Read(char *filename, int format, int hasvals,
                 int numbering, int isfewgts, int isfvwgts, int isfvsizes);
GKLIB_EXPORT void gk_graph_Write(gk_graph_t *graph, char *filename, int format, int numbering);
GKLIB_EXPORT gk_graph_t *gk_graph_Dup(gk_graph_t *graph);
GKLIB_EXPORT gk_graph_t *gk_graph_Transpose(gk_graph_t *graph);
GKLIB_EXPORT gk_graph_t *gk_graph_ExtractSubgraph(gk_graph_t *graph, int vstart, int nvtxs);
GKLIB_EXPORT gk_graph_t *gk_graph_Reorder(gk_graph_t *graph, int32_t *perm, int32_t *iperm);
GKLIB_EXPORT int gk_graph_FindComponents(gk_graph_t *graph, int32_t *cptr, int32_t *cind);
GKLIB_EXPORT void gk_graph_ComputeBFSOrdering(gk_graph_t *graph, int v, int32_t **r_perm,
         int32_t **r_iperm);
GKLIB_EXPORT void gk_graph_ComputeBestFOrdering0(gk_graph_t *graph, int v, int type,
              int32_t **r_perm, int32_t **r_iperm);
GKLIB_EXPORT void gk_graph_ComputeBestFOrdering(gk_graph_t *graph, int v, int type,
              int32_t **r_perm, int32_t **r_iperm);
GKLIB_EXPORT void gk_graph_SingleSourceShortestPaths(gk_graph_t *graph, int v, void **r_sps);
GKLIB_EXPORT void gk_graph_SortAdjacencies(gk_graph_t *graph);
GKLIB_EXPORT gk_graph_t *gk_graph_MakeSymmetric(gk_graph_t *graph, int op);


/* cache.c */
GKLIB_EXPORT gk_cache_t *gk_cacheCreate(uint32_t nway, uint32_t lnbits, size_t cnbits);
GKLIB_EXPORT void gk_cacheReset(gk_cache_t *cache);
GKLIB_EXPORT void gk_cacheDestroy(gk_cache_t **r_cache);
GKLIB_EXPORT int gk_cacheLoad(gk_cache_t *cache, size_t addr);
GKLIB_EXPORT double gk_cacheGetHitRate(gk_cache_t *cache);


#ifdef __cplusplus
}
#endif


#endif
