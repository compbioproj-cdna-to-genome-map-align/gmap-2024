#ifndef DYNPROG_SIMD_INCLUDED
#define DYNPROG_SIMD_INCLUDED

#include "dynprog.h"

#if 0
/* Now determined by mismatchtype: highq 41, medq 63, lowq 127, endq 24 */
#define SIMD_MAXLENGTH_EPI8 30  /* Previously had 40 = 128/3, but have seen 7-bit overflow empirically at matrices of size 30 */
#endif


/* Define DEBUG_SIMD and DEBUG_AVX2 in dynprog.h */

#define T Dynprog_T

extern Score8_T **
Dynprog_simd_8 (Direction8_T ***directions_nogap, Direction8_T ***directions_Egap,
		Direction8_T ***directions_Fgap,
		T this, char *rsequence, char *gsequence, char *gsequence_alt,
		int rlength, int glength,
#if defined(DEBUG_AVX2) || defined(DEBUG_SIMD)
		int goffset, Univcoord_T chroffset, Univcoord_T chrhigh, bool watsonp,
#endif
		Mismatchtype_T mismatchtype, int open, int extend,
		int lband, int uband, bool jump_late_p, bool revp);

extern Score8_T **
Dynprog_simd_8_upper (Direction8_T ***directions_nogap, Direction8_T ***directions_Egap,
		      T this, char *rsequence, char *gsequence, char *gsequence_alt,
		      int rlength, int glength,
#if defined(DEBUG_AVX2) || defined(DEBUG_SIMD)
		      int goffset, Univcoord_T chroffset, Univcoord_T chrhigh, bool watsonp,
#endif
		      Mismatchtype_T mismatchtype, int open, int extend,
		      int uband, bool jump_late_p, bool revp);

extern Score8_T **
Dynprog_simd_8_lower (Direction8_T ***directions_nogap, Direction8_T ***directions_Egap,
		      T this, char *rsequence, char *gsequence, char *gsequence_alt,
		      int rlength, int glength,
#if defined(DEBUG_AVX2) || defined(DEBUG_SIMD)
		      int goffset, Univcoord_T chroffset, Univcoord_T chrhigh, bool watsonp,
#endif
		      Mismatchtype_T mismatchtype, int open, int extend,
		      int lband, bool jump_late_p, bool revp);

extern Score16_T **
Dynprog_simd_16 (Direction16_T ***directions_nogap, Direction16_T ***directions_Egap,
		 Direction16_T ***directions_Fgap,
		 T this, char *rsequence, char *gsequence, char *gsequence_alt,
		 int rlength, int glength,
#if defined(DEBUG_AVX2) || defined(DEBUG_SIMD)
		 int goffset, Univcoord_T chroffset, Univcoord_T chrhigh, bool watsonp,
#endif
		 Mismatchtype_T mismatchtype, int open, int extend,
		 int lband, int uband, bool jump_late_p, bool revp);

extern Score16_T **
Dynprog_simd_16_upper (Direction16_T ***directions_nogap, Direction16_T ***directions_Egap,
		       T this, char *rsequence, char *gsequence, char *gsequence_alt,
		       int rlength, int glength,
#if defined(DEBUG_AVX2) || defined(DEBUG_SIMD)
		       int goffset, Univcoord_T chroffset, Univcoord_T chrhigh, bool watsonp,
#endif
		       Mismatchtype_T mismatchtype, int open, int extend,
		       int uband, bool jump_late_p, bool revp);

extern Score16_T **
Dynprog_simd_16_lower (Direction16_T ***directions_nogap, Direction16_T ***directions_Egap,
		       T this, char *rsequence, char *gsequence, char *gsequence_alt,
		       int rlength, int glength,
#if defined(DEBUG_AVX2) || defined(DEBUG_SIMD)
		       int goffset, Univcoord_T chroffset, Univcoord_T chrhigh, bool watsonp,
#endif
		       Mismatchtype_T mismatchtype, int open, int extend,
		       int lband, bool jump_late_p, bool revp);

extern List_T
Dynprog_traceback_8 (List_T pairs, int *traceback_score, int *nmatches, int *nmismatches, int *nopens, int *nindels,
		     Direction8_T **directions_nogap, Direction8_T **directions_Egap, Direction8_T **directions_Fgap,
		     int r, int c, char *rsequence, char *rsequenceuc, char *gsequence, char *gsequence_alt,
		     int queryoffset, int genomeoffset, Genome_T genome, Genome_T genomealt,
		     Pairpool_T pairpool, bool revp,
		     Univcoord_T chroffset, Univcoord_T chrhigh, bool watsonp, int genestrand,
		     int dynprogindex);

extern List_T
Dynprog_traceback_8_upper (List_T pairs, int *traceback_score, int *nmatches, int *nmismatches, int *nopens, int *nindels,
			   Direction8_T **directions_nogap, Direction8_T **directions_Egap,
			   int r, int c, char *rsequence, char *rsequenceuc, char *gsequence, char *gsequence_alt,
			   int queryoffset, int genomeoffset, Genome_T genome, Genome_T genomealt,
			   Pairpool_T pairpool, bool revp,
			   Univcoord_T chroffset, Univcoord_T chrhigh, bool watsonp, int genestrand,
			   int dynprogindex);

extern List_T
Dynprog_traceback_8_lower (List_T pairs, int *traceback_score, int *nmatches, int *nmismatches, int *nopens, int *nindels,
			   Direction8_T **directions_nogap, Direction8_T **directions_Egap,
			   int r, int c, char *rsequence, char *rsequenceuc, char *gsequence, char *gsequence_alt,
			   int queryoffset, int genomeoffset,
			   Pairpool_T pairpool, int genestrand, bool revp, int dynprogindex);

extern List_T
Dynprog_traceback_16 (List_T pairs, int *traceback_score, int *nmatches, int *nmismatches, int *nopens, int *nindels,
		      Direction16_T **directions_nogap, Direction16_T **directions_Egap, Direction16_T **directions_Fgap,
		      int r, int c, char *rsequence, char *rsequenceuc, char *gsequence, char *gsequence_alt,
		      int queryoffset, int genomeoffset, Genome_T genome, Genome_T genomealt,
		      Pairpool_T pairpool, bool revp,
		      Univcoord_T chroffset, Univcoord_T chrhigh, bool watsonp, int genestrand,
		      int dynprogindex);

extern List_T
Dynprog_traceback_16_upper (List_T pairs, int *traceback_score, int *nmatches, int *nmismatches, int *nopens, int *nindels,
			    Direction16_T **directions_nogap, Direction16_T **directions_Egap,
			    int r, int c, char *rsequence, char *rsequenceuc, char *gsequence, char *gsequence_alt,
			    int queryoffset, int genomeoffset, Genome_T genome, Genome_T genomealt,
			    Pairpool_T pairpool, bool revp,
			    Univcoord_T chroffset, Univcoord_T chrhigh, bool watsonp, int genestrand,
			    int dynprogindex);

extern List_T
Dynprog_traceback_16_lower (List_T pairs, int *traceback_score, int *nmatches, int *nmismatches, int *nopens, int *nindels,
			    Direction16_T **directions_nogap, Direction16_T **directions_Egap,
			    int r, int c, char *rsequence, char *rsequenceuc, char *gsequence, char *gsequence_alt,
			    int queryoffset, int genomeoffset,
			    Pairpool_T pairpool, int genestrand, bool revp, int dynprogindex);
#undef T
#endif
