/* $Id: dynprog.h 226315 2023-02-28 18:22:20Z twu $ */
#ifndef DYNPROG_INCLUDED
#define DYNPROG_INCLUDED
#ifdef HAVE_CONFIG_H
#include "config.h"		/* For HAVE_SSE2, HAVE_SSE4_1 */
#endif


/* #define DEBUG_SIMD 1 */
/* #define DEBUG_AVX2 1 */

/* Used by dynprog_simd.c and dynprog.c when comparing using DEBUG_SIMD or DEBUG_AVX2 */
/* #define ZERO_INITIAL_GAP_PENALTY 1 */  /* Needed for bam_indelfix */
#define INFINITE_INITIAL_GAP_PENALTY 1   /* Prevents any gaps on row 0 or column 0 */

/* Note: If we select neither ZERO_INITIAL_GAP_PENALTY or
   INFINITE_INITIAL_GAP_PENALTY, we have standard behavior, which
   allows open+extend on row 0 or column 0 */



/* BEST_LOCAL is a local alignment, whereas QUERYEND_INDELS and
   QUERYEND_NOGAPS are global.  QUERYEND_GAP allows an intron at the
   end */
typedef enum {QUERYEND_GAP, QUERYEND_INDELS, QUERYEND_NOGAPS, BEST_LOCAL} Endalign_T;
typedef struct Dynprog_T *Dynprog_T; /* Needed before header files below */

#include "bool.h"
#include "list.h"
#include "genomicpos.h"
#include "pairpool.h"
#include "chrnum.h"
#include "iit-read.h"
#include "splicetrie_build.h"	/* For splicetype */
#include "genome.h"
#include "mode.h"

#ifdef GSNAP
#include "compress.h"
#endif


#define FULLMATCH 3
#define HALFMATCH 1
#define AMBIGUOUS 3		/* Previously set to 0, but then strings of N's cause problems */

/* These values were set to -5, -4, -3, but this led to chopped ends
   in GMAP alignments, and failure to find chimeras */
#define MISMATCH_HIGHQ -3
#define MISMATCH_MEDQ -2
#define MISMATCH_LOWQ -1


typedef enum {HIGHQ, MEDQ, LOWQ, ENDQ} Mismatchtype_T;
#define NMISMATCHTYPES 4

/* Note: HIGHQ, MEDQ, and LOWQ indicate parameters for high, medium,
   and low sequence quality, respectively */
#define DEFECT_HIGHQ 0.003
#define DEFECT_MEDQ 0.014

#define SINGLE_OPEN_HIGHQ -8
#define SINGLE_OPEN_MEDQ -7
#define SINGLE_OPEN_LOWQ -6

#define SINGLE_EXTEND_HIGHQ -3
#define SINGLE_EXTEND_MEDQ -2
#define SINGLE_EXTEND_LOWQ -1

#define PAIRED_OPEN_HIGHQ -8
#define PAIRED_OPEN_MEDQ -7
#define PAIRED_OPEN_LOWQ -6

#define PAIRED_EXTEND_HIGHQ -3
#define PAIRED_EXTEND_MEDQ -2
#define PAIRED_EXTEND_LOWQ -1


#define UNKNOWNJUMP -1000000

typedef char Score8_T;
typedef char Direction8_T;

typedef short Score16_T;
typedef short Direction16_T;

typedef short Pairdistance_T;

/* For standard dynamic programming.  Use ints, so NEG_INFINITY_32 works. */
typedef int Score32_T;
typedef int Direction32_T;

/* Genome is on the horizontal axis.  Query sequence is on the vertical axis.  Dynamic programming fills in matrices column by column */
/* The following values are for directions_nogap.  For directions_Egap, the choices are DIAG or not DIAG (meaning HORIZ). */
/*  For directions_Fgap, the choices are DIAG or not DIAG (meaning VERT) */
#define VERT -2			/* or VERT == -3 in SIMD code.  Don't check for dir == VERT.  Check instead if dir == DIAG or dir == HORIZ */
#define HORIZ -1
#define DIAG 0			/* Pre-dominant case.  Directions_alloc clears to this value. */

#define NEG_INFINITY_8 (-128)
#define POS_INFINITY_8 (127)
#define MAX_CHAR (127)

#define NEG_INFINITY_16 (-32768)
#define POS_INFINITY_16 (32767)
#define MAX_SHORT (32767)

/* We can allow -128 and -32768 for NEG_INFINITY in SIMD procedures,
   because we are using saturation */
#ifdef HAVE_ARM
#define ALIGN_SIZE 16
#define ONE_CHAR 1
#define LAST_CHAR_SHIFT 15
#define SIMD_NCHARS 16		/* 16 8-bit chars in 128 bits */

#define ONE_SHORT 2
#define LAST_SHORT_SHIFT 14	/* (8 - 1) * 2 */
#define SIMD_NSHORTS 8		/* 8 16-bit shorts in 128 bits */

#define LAST_CHAR_NONAVX2 15	/* For DEBUG_SIMD */
#define LAST_SHORT_NONAVX2 14	/* For DEBUG_SIMD */

#elif defined(HAVE_AVX2)
#define ALIGN_SIZE 32
#define ONE_CHAR 1
#define MID_CHAR_INSERT 16
#define LAST_CHAR_INSERT 0
#define SIMD_NCHARS 32		/* 32 8-bit chars in 256 bits */

#define ONE_SHORT 2
#define MID_SHORT_INSERT 8
#define LAST_SHORT_INSERT 0
#define SIMD_NSHORTS 16		/* 16 16-bit shorts in 256 bits */

#define LAST_CHAR_NONAVX2 15	/* For DEBUG_AVX2 */
#define LAST_SHORT_NONAVX2 14	/* For DEBUG_AVX2.  (8 - 1) * 2 */

#ifdef DEBUG_AVX2
#define SIMD_NCHARS_NONAVX2 16
#define SIMD_NSHORTS_NONAVX2 8
#endif

#elif defined(HAVE_SSE2) /* Was HAVE_SSE4_1 or HAVE_SSE2, but the former implies the latter */
#define ALIGN_SIZE 16
#define ONE_CHAR 1
#define LAST_CHAR_SHIFT 15
#define SIMD_NCHARS 16		/* 16 8-bit chars in 128 bits */

#define ONE_SHORT 2
#define LAST_SHORT_SHIFT 14	/* (8 - 1) * 2 */
#define SIMD_NSHORTS 8		/* 8 16-bit shorts in 128 bits */

#define LAST_CHAR_NONAVX2 15	/* For DEBUG_SIMD */
#define LAST_SHORT_NONAVX2 14	/* For DEBUG_SIMD */

#endif

/* Can allow -32768 in non-SIMD procedures, because we are using ints */
#define NEG_INFINITY_32 (-32768)
#define NEG_INFINITY_INT (-32768)


/* This is still needed, because sequences passed into compute_scores
   might be lower-case */
#define PREUC 1			

extern int use8p_size[NMISMATCHTYPES];
extern Pairdistance_T **pairdistance_array[NMISMATCHTYPES];
#ifndef HAVE_SSE4_1
extern Pairdistance_T **pairdistance_array_plus_128[NMISMATCHTYPES];
#endif
extern bool **consistent_array[3];
extern int *nt_to_int_array;


struct Space_single_T {
  void **matrix_ptrs, *matrix_space;
  void **directions_ptrs_0, *directions_space_0;
  void **directions_ptrs_1, *directions_space_1;
  void **directions_ptrs_2, *directions_space_2;
};

struct Space_double_T {
  void **upper_matrix_ptrs, *upper_matrix_space;
  void **upper_directions_ptrs_0, *upper_directions_space_0;
  void **upper_directions_ptrs_1, *upper_directions_space_1;
  void **lower_matrix_ptrs, *lower_matrix_space;
  void **lower_directions_ptrs_0, *lower_directions_space_0;
  void **lower_directions_ptrs_1, *lower_directions_space_1;
};


#define T Dynprog_T
struct T {
  int max_rlength;
  int max_glength;

#ifdef DEBUG12
  struct Int3_T **matrix3_ptrs, *matrix3_space;
#endif

#if !defined(HAVE_SSE2) || defined(DEBUG_SIMD)
  Score32_T **matrix_ptrs, *matrix_space;
  Direction32_T **directions_ptrs_0, *directions_space_0;
  Direction32_T **directions_ptrs_1, *directions_space_1;
  Direction32_T **directions_ptrs_2, *directions_space_2;
#endif
#ifdef HAVE_SSE2
  union {
    struct Space_single_T one;
    struct Space_double_T two;
  } aligned;
#ifdef DEBUG_AVX2
  union {
    struct Space_single_T one;
    struct Space_double_T two;
  } aligned_std;
#endif
#endif
  int nspaces;
};


extern char *
Dynprog_endalign_string (Endalign_T endalign);

extern int
Dynprog_score (int matches, int mismatches, int qopens, int qindels, int topens, int tindels,
	       double defect_rate, int user_open, int user_extend, bool user_dynprog_p);

extern T
Dynprog_new (int maxlookback, int extraquerygap, int maxpeelback,
	     int extramaterial_end, int extramaterial_paired,
	     bool doublep);
extern void
Dynprog_free (T *old);

extern bool
Dynprog_consistent_p (int c, int g, int g_alt, int genestrand);

extern void
Dynprog_term (Mode_T mode);
extern void
Dynprog_init (Mode_T mode);

extern void
Dynprog_compute_bands (int *lband, int *uband, int rlength, int glength, int extraband, bool widebandp);


extern void
Dynprog_Matrix32_print (Score32_T **matrix, int rlength, int glength, char *rsequence,
			char *gsequence, char *gsequencealt,
			int goffset, Univcoord_T chroffset, Univcoord_T chrhigh,
			bool watsonp, bool revp, int lband, int uband);

extern void
Dynprog_Directions32_print (Direction32_T **directions_nogap, Direction32_T **directions_Egap, Direction32_T **directions_Fgap,
			    int rlength, int glength, char *rsequence, char *gsequence, char *gsequence_alt,
			    int goffset, Univcoord_T chroffset, Univcoord_T chrhigh,
			    bool watsonp, bool revp, int lband, int uband);

extern Score32_T **
Dynprog_standard (Direction32_T ***directions_nogap, Direction32_T ***directions_Egap, Direction32_T ***directions_Fgap,
		  T this, char *rsequence, char *gsequence, char *gsequence_alt,
		  int rlength, int glength, int goffset,
		  Genome_T genome, Genome_T genomealt,
		  Univcoord_T chroffset, Univcoord_T chrhigh, bool watsonp,
		  Mismatchtype_T mismatchtype, Score32_T open, Score32_T extend,
		  int lband, int uband, bool jump_late_p, bool revp, int saturation,
		  bool upperp, bool lowerp);

extern List_T
Dynprog_traceback_std (List_T pairs, int *traceback_score, int *nmatches, int *nmismatches, int *nopens, int *nindels,
		       Direction32_T **directions_nogap, Direction32_T **directions_Egap, Direction32_T **directions_Fgap,
		       int r, int c, char *rsequence, char *rsequenceuc, char *gsequence, char *gsequence_alt,
		       int queryoffset, int genomeoffset, Genome_T genome, Genome_T genomealt, Pairpool_T pairpool, bool revp,
		       Univcoord_T chroffset, Univcoord_T chrhigh, bool watsonp, int genestrand,
		       int dynprogindex);


#undef T
#endif
