/*!
\file gk_macros.h
\brief This file contains various macros

\date   Started 3/27/2007
\author George
\version\verbatim $Id: gk_macros.h 15048 2013-08-31 19:38:14Z karypis $ \endverbatim
*/

#ifndef _GK_MACROS_H_
#define _GK_MACROS_H_

/*-------------------------------------------------------------
 * Usefull commands 
 *-------------------------------------------------------------*/
#define gk_max(a, b) ((a) >= (b) ? (a) : (b))
#define gk_min(a, b) ((a) >= (b) ? (b) : (a))
#define gk_max3(a, b, c) ((a) >= (b) && (a) >= (c) ? (a) : ((b) >= (a) && (b) >= (c) ? (b) : (c)))
#define gk_SWAP(a, b, tmp) do {(tmp) = (a); (a) = (b); (b) = (tmp);} while(0) 
#define INC_DEC(a, b, val) do {(a) += (val); (b) -= (val);} while(0)
#define sign(a, b) ((a >= 0 ? b : -b))

#define ONEOVERRANDMAX (1.0/(RAND_MAX+1.0))
#define RandomInRange(u) ((int) (ONEOVERRANDMAX*(u)*rand()))
#define RandomInRange_r(s, u) ((int) (ONEOVERRANDMAX*(u)*rand_r(s)))

#define gk_abs(x) ((x) >= 0 ? (x) : -(x))


/*-------------------------------------------------------------
 * Timing macros
 *-------------------------------------------------------------*/
#define gk_clearcputimer(tmr) (tmr = 0.0)
#define gk_startcputimer(tmr) (tmr -= gk_CPUSeconds())
#define gk_stopcputimer(tmr)  (tmr += gk_CPUSeconds())
#define gk_getcputimer(tmr)   (tmr)

#define gk_clearwctimer(tmr) (tmr = 0.0)
#define gk_startwctimer(tmr) (tmr -= gk_WClockSeconds())
#define gk_stopwctimer(tmr)  (tmr += gk_WClockSeconds())
#define gk_getwctimer(tmr)   (tmr)

/*-------------------------------------------------------------
 * dbglvl handling macros
 *-------------------------------------------------------------*/
#define IFSET(a, flag, cmd) if ((a)&(flag)) (cmd);


/*-------------------------------------------------------------
 * gracefull library exit macro
 *-------------------------------------------------------------*/
#define GKSETJMP() (setjmp(gk_return_to_entry))
#define gk_sigcatch() (setjmp(gk_jbufs[gk_cur_jbufs]))
 

/*-------------------------------------------------------------
 * Debuging memory leaks
 *-------------------------------------------------------------*/
#ifdef DMALLOC
#   define MALLOC_CHECK(ptr)                                          \
    if (malloc_verify((ptr)) == DMALLOC_VERIFY_ERROR) {  \
        printf("***MALLOC_CHECK failed on line %d of file %s: " #ptr "\n", \
              __LINE__, __FILE__);                               \
        abort();                                                \
    }
#else
#   define MALLOC_CHECK(ptr) ;
#endif 


/*-------------------------------------------------------------
 * CSR conversion macros
 *-------------------------------------------------------------*/
#define MAKECSR(i, n, a) \
   do { \
     for (i=1; i<n; i++) a[i] += a[i-1]; \
     for (i=n; i>0; i--) a[i] = a[i-1]; \
     a[0] = 0; \
   } while(0) 

#define SHIFTCSR(i, n, a) \
   do { \
     for (i=n; i>0; i--) a[i] = a[i-1]; \
     a[0] = 0; \
   } while(0) 


/*-------------------------------------------------------------
 * ASSERTS that cannot be turned off!
 *-------------------------------------------------------------*/
#define GKASSERT(expr)                                          \
    if (!(expr)) {                                               \
        printf("***ASSERTION failed on line %d of file %s: " #expr "\n", \
              __LINE__, __FILE__);                               \
        abort();                                                \
    }

#define GKASSERTP(expr,msg)                                          \
    if (!(expr)) {                                               \
        printf("***ASSERTION failed on line %d of file %s: " #expr "\n", \
              __LINE__, __FILE__);                               \
        printf msg ; \
        printf("\n"); \
        abort();                                                \
    }

#define GKCUASSERT(expr)                                          \
    if (!(expr)) {                                               \
        printf("***ASSERTION failed on line %d of file %s: " #expr "\n", \
              __LINE__, __FILE__);                               \
    }

#define GKWARN(expr)                                          \
    if (!(expr)) {                                               \
        printf("***ASSERTION failed on line %d of file %s: " #expr "\n", \
              __LINE__, __FILE__);                               \
    }

#define GKCUASSERTP(expr,msg)                                          \
    if (!(expr)) {                                               \
        printf("***ASSERTION failed on line %d of file %s: " #expr "\n", \
              __LINE__, __FILE__);                               \
        printf msg ; \
        printf("\n"); \
    }

#define GKWARNP(expr,msg)                                          \
    if (!(expr)) {                                               \
        printf("***ASSERTION failed on line %d of file %s: " #expr "\n", \
              __LINE__, __FILE__);                               \
        printf msg ; \
        printf("\n"); \
    }


/*-------------------------------------------------------------
 * Program Assertions
 *-------------------------------------------------------------*/
#ifndef GKLIB_ASSERTIONS_ENABLED
#  ifdef NDEBUG
#    define GKLIB_ASSERTIONS_ENABLED 0
#  else
#    define GKLIB_ASSERTIONS_ENABLED 1
#  endif
#endif

#   define GKLIB_ASSERT(expr)                                          \
    if (!(expr)) {                                               \
        printf("***ASSERTION failed on line %d of file %s: " #expr "\n", \
              __LINE__, __FILE__);                               \
        abort();                                                \
    }

#   define GKLIB_ASSERTP(expr,msg)                                          \
    if (!(expr)) {                                               \
        printf("***ASSERTION failed on line %d of file %s: " #expr "\n", \
              __LINE__, __FILE__);                               \
        printf msg ; \
        printf("\n"); \
        abort();                                                \
    }
#if GKLIB_ASSERTIONS_ENABLED
#   define ASSERT GKLIB_ASSERT
#   define ASSERTP GKLIB_ASSERTP
#else
#   define ASSERT(expr) ;
#   define ASSERTP(expr,msg) ;
#endif 

#ifndef GKLIB_ASSERTIONS_EXPENSIVE_ENABLED
#  ifdef NDEBUG2
#    define GKLIB_ASSERTIONS_EXPENSIVE_ENABLED 0
#  else
#    define GKLIB_ASSERTIONS_EXPENSIVE_ENABLED 1
#  endif
#endif

#if GKLIB_ASSERTIONS_EXPENSIVE_ENABLED
#   define ASSERT2 GKLIB_ASSERT
#   define ASSERTP2 GKLIB_ASSERTP
#else
#   define ASSERT2(expr) ;
#   define ASSERTP2(expr,msg) ;
#endif


#endif
