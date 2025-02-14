static char rcsid[] = "$Id: dynprog.c 226315 2023-02-28 18:22:20Z twu $";
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "dynprog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>		/* For ceil, log, pow */
#include <ctype.h>		/* For tolower */

#include "simd.h"

#if 0
#ifdef HAVE_SSE2
#include <emmintrin.h>
#endif
#ifdef HAVE_SSE4_1
#include <smmintrin.h>
#endif
#endif


#include "bool.h"
#include "except.h"
#include "assert.h"
#include "mem.h"
#include "comp.h"
#include "pair.h"
#include "pairdef.h"
#include "boyer-moore.h"
#include "intron.h"
#include "complement.h"
#include "splicetrie.h"
#include "maxent.h"
#include "maxent_hr.h"
#include "fastlog.h"
#include "scores.h"


/* Tests whether get_genomic_nt == genomicseg in compute_scores procedures */
/* #define EXTRACT_GENOMICSEG 1 */


/* Prints parameters and results */
#ifdef DEBUG
#define debug(x) x
#else
#define debug(x)
#endif

/* Prints matrices */
#ifdef DEBUG2
#define debug2(x) x
#else
#define debug2(x)
#endif

/* F gap */
#ifdef DEBUG3
#define debug3(x) x
#else
#define debug3(x)
#endif

/* Getting genomic nt */
#ifdef DEBUG8
#define debug8(x) x
#else
#define debug8(x)
#endif

/* Old matrix computations, details */
#ifdef DEBUG12A
#define debug12a(x) x
#else
#define debug12a(x)
#endif

/* Comparing SIMD with standard code.  Define in dynprog.h */
#ifdef DEBUG_SIMD
#define debug_simd(x) x
#else
#define debug_simd(x)
#endif


/*
#define RIGHTANGLE 1
*/

#if defined(DEBUG2) || defined(DEBUG_SIMD)
#define NEG_INFINITY_DISPLAY (-99)
#endif


/* Previously allowed lower mismatch scores on end to allow more
   complete alignments to the end, and because ends are typically of
   lower quality.  Previously made equal to FULLMATCH, because
   criterion is nmatches >= nmismatches.  However, extensions at ends
   appear to defeat purpose of trimming, so increase mismatch at end
   from -3 to -4. */
#define MISMATCH_ENDQ -5


#define T Dynprog_T


char *
Dynprog_endalign_string (Endalign_T endalign) {
  switch (endalign) {
  case QUERYEND_GAP: return "queryend_gap";
  case QUERYEND_INDELS: return "queryend_indels";
  case QUERYEND_NOGAPS: return "queryend_nogaps";
  case BEST_LOCAL: return "best_local";
  default: 
    printf("endalign %d not recognized\n",endalign);
    return "";
  }
}



int
Dynprog_score (int matches, int mismatches, int qopens, int qindels, int topens, int tindels,
	       double defect_rate, int user_open, int user_extend, bool user_dynprog_p) {

  if (user_dynprog_p == true) {
    return FULLMATCH*matches + MISMATCH_HIGHQ*mismatches + user_open*qopens + user_extend*qindels
      + user_open*topens + user_extend*tindels;
  } else if (defect_rate < DEFECT_HIGHQ) {
    return FULLMATCH*matches + MISMATCH_HIGHQ*mismatches + SINGLE_OPEN_HIGHQ*qopens + SINGLE_EXTEND_HIGHQ*qindels
      + SINGLE_OPEN_HIGHQ*topens + SINGLE_EXTEND_HIGHQ*tindels;
  } else if (defect_rate < DEFECT_MEDQ) {
    return FULLMATCH*matches + MISMATCH_MEDQ*mismatches + SINGLE_OPEN_MEDQ*qopens + SINGLE_EXTEND_MEDQ*qindels
      + SINGLE_OPEN_MEDQ*topens + SINGLE_EXTEND_MEDQ*tindels;
  } else {
    return FULLMATCH*matches + MISMATCH_LOWQ*mismatches + SINGLE_OPEN_LOWQ*qopens + SINGLE_EXTEND_LOWQ*qindels
      + SINGLE_OPEN_LOWQ*topens + SINGLE_EXTEND_LOWQ*tindels;
  }
}


#if !defined(HAVE_SSE2) || defined(DEBUG2) || defined(DEBUG_SIMD)
static char complCode[128] = COMPLEMENT_LC;

static char
get_genomic_nt (char *g_alt, Genome_T genome, Genome_T genomealt, int genomicpos,
		Univcoord_T chroffset, Univcoord_T chrhigh, bool watsonp) {
  char c2, c2_alt;
  Univcoord_T pos;

#if 0
  /* If the read has a deletion, then we will extend beyond 0 or genomiclength, so do not restrict. */
  if (genomicpos < 0) {
    return '*';

  } else if (genomicpos >= genomiclength) {
    return '*';

  }
#endif

  if (watsonp) {
    if ((pos = chroffset + genomicpos) < chroffset) { /* Must be <, and not <=, or dynamic programming will fail */
      *g_alt = '*';
      return '*';

    } else if (pos >= chrhigh) {
      *g_alt = '*';
      return '*';

    } else {
      /* GMAP with user-supplied genomic segment */
      return Genome_get_char(&(*g_alt),genome,genomealt,pos);
    }

  } else {
    if ((pos = chrhigh - genomicpos) < chroffset) { /* Must be <, and not <=, or dynamic programming will fail */
      *g_alt = '*';
      return '*';

    } else if (pos >= chrhigh) {
      *g_alt = '*';
      return '*';

    } else {
      /* GMAP with user-supplied genomic segment */
      c2 = Genome_get_char(&c2_alt,genome,genomealt,pos);
    }
    debug8(printf("At %u, genomicnt is %c\n",genomicpos,complCode[(int) c2]));
    *g_alt = complCode[(int) c2_alt];
    return complCode[(int) c2];
  }
}
#endif


/************************************************************************
 * Matrix
 ************************************************************************/

#if !defined(HAVE_SSE2) || defined(DEBUG_SIMD)
/* Makes a matrix of dimensions 0..glength x 0..rlength inclusive */
static Score32_T **
Matrix32_alloc (int rlength, int glength, Score32_T **ptrs, Score32_T *space) {
  Score32_T **matrix;
  int i;

  if (glength <= 0 || rlength <= 0) {
    fprintf(stderr,"dynprog: lengths are negative: %d %d\n",rlength,glength);
    abort();
  }

  matrix = ptrs;
  matrix[0] = space;
  for (i = 1; i <= glength; i++) {
    matrix[i] = &(matrix[i-1][rlength + 1]);
  }

  memset((void *) space,0,(glength+1)*(rlength+1)*sizeof(Score32_T));

  return matrix;
}

#endif


#if defined(DEBUG2) || defined(DEBUG_SIMD)
void
Dynprog_Matrix32_print (Score32_T **matrix, int rlength, int glength, char *rsequence,
			char *gsequence, char *gsequencealt,
			int goffset, Univcoord_T chroffset, Univcoord_T chrhigh,
			bool watsonp, bool revp, int lband, int uband) {
  int i, j;
  char g_alt;

  /* j */
  printf("   ");		/* For i */
  printf("  ");
  for (j = 0; j <= glength; ++j) {
    printf(" %2d ",j);
  }
  printf("\n");

  if (gsequence) {
    printf("   ");		/* For i */
    printf("  ");
    for (j = 0; j <= glength; ++j) {
      if (j == 0) {
	printf("    ");
      } else {
	printf("  %c ",revp ? gsequence[-j+1] : gsequence[j-1]);
      }
    }
    printf("\n");
  }

  if (gsequencealt != gsequence) {
    printf("   ");		/* For i */
    printf("  ");
    for (j = 0; j <= glength; ++j) {
      if (j == 0) {
	printf("    ");
      } else {
	if (revp == false) {
	  printf("  %c ",get_genomic_nt(&g_alt,genome,genomealt,goffset+j-1,chroffset,chrhigh,watsonp));
	} else {
	  printf("  %c ",get_genomic_nt(&g_alt,genome,genomealt,goffset+1-j,chroffset,chrhigh,watsonp));
	}
      }
    }
    printf("\n");
  }

  for (i = 0; i <= rlength; ++i) {
    printf("%2d ",i);
    if (i == 0) {
      printf("  ");
    } else {
      printf("%c ",revp ? rsequence[-i+1] : rsequence[i-1]);
    }
    for (j = 0; j <= glength; ++j) {
      if (j < i - lband) {
	printf("  . ");
      } else if (j > i + uband) {
	printf("  . ");
      } else if (matrix[j][i] < NEG_INFINITY_DISPLAY) {
	printf("%3d ",NEG_INFINITY_DISPLAY);
      } else {
	printf("%3d ",matrix[j][i]);
      }
    }
    printf("\n");
  }
  printf("\n");

  return;
}
#endif



#ifdef DEBUG12
struct Int3_T {
  Score16_T Egap;
  Score16_T Fgap;
  Score16_T nogap;
};
#endif


#ifdef DEBUG12
/* Makes a matrix of dimensions 0..glength x 0..rlength inclusive */
static struct Int3_T **
Matrix3_alloc (int rlength, int glength, struct Int3_T **ptrs, struct Int3_T *space) {
  struct Int3_T **matrix;
  int i;

  if (glength <= 0 || rlength <= 0) {
    fprintf(stderr,"dynprog: lengths are negative: %d %d\n",rlength,glength);
    abort();
  }

  matrix = ptrs;
  matrix[0] = space;
  for (i = 1; i <= glength; i++) {
    matrix[i] = &(matrix[i-1][rlength + 1]);
  }

  memset((void *) space,0,(glength+1)*(rlength+1)*sizeof(struct Int3_T));

  return matrix;
}
#endif


#ifdef DEBUG12A
static void
Matrix3_print (struct Int3_T **matrix, int rlength, int glength, char *rsequence,
	       char *gsequence, char *gsequencealt,
	       int goffset, Univcoord_T chroffset, Univcoord_T chrhigh,
	       bool watsonp, bool revp) {
  int i, j;
  char g_alt;

  printf("G1");
  if (gsequence) {
    for (j = 0; j <= glength; ++j) {
      if (j == 0) {
	printf("    ");
      } else {
	printf("  %c ",revp ? gsequence[-j+1] : gsequence[j-1]);
      }
    }
    printf("\n");
  }

  for (j = 0; j <= glength; ++j) {
    if (j == 0) {
      printf("    ");
    } else {
      if (revp == false) {
	printf("  %c ",get_genomic_nt(&g_alt,genome,genomealt,goffset+j-1,chroffset,chrhigh,watsonp));
      } else {
	printf("  %c ",get_genomic_nt(&g_alt,genome,genomealt,goffset+1-j,chroffset,chrhigh,watsonp));
      }
    }
  }
  printf("\n");

  for (i = 0; i <= rlength; ++i) {
    if (i == 0) {
      printf("  ");
    } else {
      printf("%c ",revp ? rsequence[-i+1] : rsequence[i-1]);
    }
    for (j = 0; j <= glength; ++j) {
      if (matrix[j][i].Egap < NEG_INFINITY) {
	printf("%3d ",NEG_INFINITY);
      } else {
	printf("%3d ",matrix[j][i].Egap);
      }
    }
    printf("\n");
  }
  printf("\n");


  printf("NG");
  if (gsequence) {
    for (j = 0; j <= glength; ++j) {
      if (j == 0) {
	printf("    ");
      } else {
	printf("  %c ",revp ? gsequence[-j+1] : gsequence[j-1]);
      }
    }
    printf("\n");
  }

  for (j = 0; j <= glength; ++j) {
    if (j == 0) {
      printf("    ");
    } else {
      if (revp == false) {
	printf("  %c ",get_genomic_nt(&g_alt,genome,genomealt,goffset+j-1,chroffset,chrhigh,watsonp));
      } else {
	printf("  %c ",get_genomic_nt(&g_alt,genome,genomealt,goffset+1-j,chroffset,chrhigh,watsonp));
      }
    }
  }
  printf("\n");

  for (i = 0; i <= rlength; ++i) {
    if (i == 0) {
      printf("  ");
    } else {
      printf("%c ",revp ? rsequence[-i+1] : rsequence[i-1]);
    }
    for (j = 0; j <= glength; ++j) {
      if (matrix[j][i].nogap < NEG_INFINITY) {
	printf("%3d ",NEG_INFINITY);
      } else {
	printf("%3d ",matrix[j][i].nogap);
      }
    }
    printf("\n");
  }
  printf("\n");


  printf("G2");
  if (gsequence) {
    for (j = 0; j <= glength; ++j) {
      if (j == 0) {
	printf("    ");
      } else {
	printf("  %c ",revp ? gsequence[-j+1] : gsequence[j-1]);
      }
    }
    printf("\n");
  }

  for (j = 0; j <= glength; ++j) {
    if (j == 0) {
      printf("    ");
    } else {
      if (revp == false) {
	printf("  %c ",get_genomic_nt(&g_alt,genome,genomealt,goffset+j-1,chroffset,chrhigh,watsonp));
      } else {
	printf("  %c ",get_genomic_nt(&g_alt,genome,genomealt,goffset+1-j,chroffset,chrhigh,watsonp));
      }
    }
  }
  printf("\n");

  for (i = 0; i <= rlength; ++i) {
    if (i == 0) {
      printf("  ");
    } else {
      printf("%c ",revp ? rsequence[-i+1] : rsequence[i-1]);
    }
    for (j = 0; j <= glength; ++j) {
      if (matrix[j][i].Fgap < NEG_INFINITY) {
	printf("%3d ",NEG_INFINITY);
      } else {
	printf("%3d ",matrix[j][i].Fgap);
      }
    }
    printf("\n");
  }
  printf("\n");

  return;
}
#endif


/************************************************************************/
/*  Directions  */
/************************************************************************/

#if !defined(HAVE_SSE2) || defined(DEBUG_SIMD)
/* Makes a matrix of dimensions 0..glength x 0..rlength inclusive */
static Direction32_T **
Directions32_alloc (int rlength, int glength, Direction32_T **ptrs, Direction32_T *space) {
  Direction32_T **directions;
  int i;

  directions = ptrs;
  directions[0] = space;
  for (i = 1; i <= glength; i++) {
    directions[i] = &(directions[i-1][rlength + 1]);
  }

  memset((void *) space,/*DIAG*/0,(glength+1)*(rlength+1)*sizeof(Direction32_T));

  return directions;
}
#endif



#if defined(DEBUG2) || defined(DEBUG_SIMD)
void
Dynprog_Directions32_print (Direction32_T **directions_nogap, Direction32_T **directions_Egap, Direction32_T **directions_Fgap,
			    int rlength, int glength, char *rsequence, char *gsequence, char *gsequence_alt,
			    int goffset, Univcoord_T chroffset, Univcoord_T chrhigh,
			    bool watsonp, bool revp, int lband, int uband) {
  int i, j;
  char g_alt;

  /* j */
  printf("   ");		/* For i */
  printf("  ");
  for (j = 0; j <= glength; ++j) {
    printf(" %2d   ",j);
  }
  printf("\n");

  if (gsequence) {
    printf("   ");		/* For i */
    printf("  ");
    for (j = 0; j <= glength; ++j) {
      if (j == 0) {
	printf("      ");
      } else {
	printf("  %c   ",revp ? gsequence[-j+1] : gsequence[j-1]);
      }
    }
    printf("\n");
  }

  if (gsequence_alt != gsequence) {
    printf("   ");		/* For i */
    printf("  ");
    for (j = 0; j <= glength; ++j) {
      if (j == 0) {
	printf("      ");
      } else {
	if (revp == false) {
	  printf("  %c   ",get_genomic_nt(&g_alt,genome,genomealt,goffset+j-1,chroffset,chrhigh,watsonp));
	} else {
	  printf("  %c   ",get_genomic_nt(&g_alt,genome,genomealt,goffset+1-j,chroffset,chrhigh,watsonp));
	}
      }
    }
    printf("\n");
  }

  for (i = 0; i <= rlength; ++i) {
    printf("%2d ",i);
    if (i == 0) {
      printf("  ");
    } else {
      printf("%c ",revp ? rsequence[-i+1] : rsequence[i-1]);
    }
    for (j = 0; j <= glength; ++j) {
      if (j < i - lband) {
	printf("     ");
      } else if (j > i + uband) {
	printf("     ");
      } else {
	if (directions_Egap[j][i] == DIAG) {
	  printf("D");
	} else {
	  /* Must be HORIZ */
	  printf("H");
	}
	printf("|");
	if (directions_nogap[j][i] == DIAG) {
	  printf("D");
	} else if (directions_nogap[j][i] == HORIZ) {
	  printf("H");
	} else {
	  /* Must be VERT */
	  printf("V");
	}
	printf("|");
	if (directions_Fgap[j][i] == DIAG) {
	  printf("D");
	} else {
	  /* Must be VERT */
	  printf("V");
	}
      }
      printf(" ");
    }
    printf("\n");
  }
  printf("\n");

  return;
}
#endif




#define QUERY_MAXLENGTH 500
#define GENOMIC_MAXLENGTH 2000


static void
compute_maxlengths (int *max_rlength, int *max_glength,
		    int maxlookback, int extraquerygap, int maxpeelback,
		    int extramaterial_end, int extramaterial_paired) {
  *max_rlength = maxlookback + maxpeelback;
  if (*max_rlength < QUERY_MAXLENGTH) {
    *max_rlength = QUERY_MAXLENGTH;
  }

  *max_glength = *max_rlength + extraquerygap;
  if (extramaterial_end > extramaterial_paired) {
    *max_glength += extramaterial_end;
  } else {
    *max_glength += extramaterial_paired;
  }

  if (*max_glength < GENOMIC_MAXLENGTH) {
    *max_glength = GENOMIC_MAXLENGTH;
  }

  return;
}


T
Dynprog_new (int maxlookback, int extraquerygap, int maxpeelback,
	     int extramaterial_end, int extramaterial_paired,
	     bool doublep) {
  T new = (T) MALLOC(sizeof(*new));
  int max_rlength, max_glength;

  compute_maxlengths(&max_rlength,&max_glength,
		     maxlookback,extraquerygap,maxpeelback,
		     extramaterial_end,extramaterial_paired);
  new->max_rlength = max_rlength;
  new->max_glength = max_glength;

#ifdef DEBUG12
  new->matrix3_ptrs = (struct Int3_T **) CALLOC(max_glength+1,sizeof(struct Int3_T *));
  new->matrix3_space = (struct Int3_T *) CALLOC((max_glength+1)*(max_rlength+1),sizeof(struct Int3_T));
#endif
#if !defined(HAVE_SSE2) || defined(DEBUG_SIMD)
  new->matrix_ptrs = (Score32_T **) CALLOC(max_glength+1,sizeof(Score32_T *));
  new->matrix_space = (Score32_T *) CALLOC((max_glength+1)*(max_glength+1),sizeof(Score32_T));
  new->directions_ptrs_0 = (Direction32_T **) CALLOC(max_glength+1,sizeof(Direction32_T *));
  new->directions_space_0 = (Direction32_T *) CALLOC((max_glength+1)*(max_rlength+1),sizeof(Direction32_T));
  new->directions_ptrs_1 = (Direction32_T **) CALLOC(max_glength+1,sizeof(Direction32_T *));
  new->directions_space_1 = (Direction32_T *) CALLOC((max_glength+1)*(max_rlength+1),sizeof(Direction32_T));
  new->directions_ptrs_2 = (Direction32_T **) CALLOC(max_glength+1,sizeof(Direction32_T *));
  new->directions_space_2 = (Direction32_T *) CALLOC((max_glength+1)*(max_rlength+1),sizeof(Direction32_T));
#endif

#if defined(HAVE_ARM)
  /* Use SIMD_NCHARS > SIMD_NSHORTS and sizeof(Score16_T) > sizeof(Score8_T) */
  if (doublep == true) {
    new->aligned.two.upper_matrix_ptrs = (void **) CALLOC(max_glength+1,sizeof(void *));
    posix_memalign((void **) &(new->aligned.two.upper_matrix_space),ALIGN_SIZE,
		   (max_glength+1)*(max_rlength+SIMD_NCHARS+SIMD_NCHARS)*sizeof(Score16_T));

    new->aligned.two.upper_directions_ptrs_0 = (void **) CALLOC(max_glength+1,sizeof(void *));
    posix_memalign((void **) &(new->aligned.two.upper_directions_space_0),ALIGN_SIZE,
		   (max_glength+1)*(max_rlength+SIMD_NCHARS+SIMD_NCHARS)*sizeof(Score16_T));

    new->aligned.two.upper_directions_ptrs_1 = (void **) CALLOC(max_glength+1,sizeof(void *));
    posix_memalign((void **) &(new->aligned.two.upper_directions_space_1),ALIGN_SIZE,
		   (max_glength+1)*(max_rlength+SIMD_NCHARS+SIMD_NCHARS)*sizeof(Score16_T));

    new->aligned.two.lower_matrix_ptrs = (void **) CALLOC(max_rlength+1,sizeof(void *));
    posix_memalign((void **) &(new->aligned.two.lower_matrix_space),ALIGN_SIZE,
		   (max_rlength+1)*(max_glength+SIMD_NCHARS+SIMD_NCHARS)*sizeof(Score16_T));

    new->aligned.two.lower_directions_ptrs_0 = (void **) CALLOC(max_rlength+1,sizeof(void *));
    posix_memalign((void **) &(new->aligned.two.lower_directions_space_0),ALIGN_SIZE,
		   (max_rlength+1)*(max_glength+SIMD_NCHARS+SIMD_NCHARS)*sizeof(Score16_T));

    new->aligned.two.lower_directions_ptrs_1 = (void **) CALLOC(max_rlength+1,sizeof(void *));
    posix_memalign((void **) &(new->aligned.two.lower_directions_space_1),ALIGN_SIZE,
		   (max_rlength+1)*(max_glength+SIMD_NCHARS+SIMD_NCHARS)*sizeof(Score16_T));

    new->nspaces = 2;

  } else {
    new->aligned.one.matrix_ptrs = (void **) CALLOC(max_glength+1,sizeof(void *));
    posix_memalign((void **) &(new->aligned.one.matrix_space),ALIGN_SIZE,
		   (max_glength+1)*(max_rlength+SIMD_NCHARS+SIMD_NCHARS)*sizeof(Score16_T));

    new->aligned.one.directions_ptrs_0 = (void **) CALLOC(max_glength+1,sizeof(void *));
    posix_memalign((void **) &(new->aligned.one.directions_space_0),ALIGN_SIZE,
		   (max_glength+1)*(max_rlength+SIMD_NCHARS+SIMD_NCHARS)*sizeof(Score16_T));

    new->aligned.one.directions_ptrs_1 = (void **) CALLOC(max_glength+1,sizeof(void *));
    posix_memalign((void **) &(new->aligned.one.directions_space_1),ALIGN_SIZE,
		   (max_glength+1)*(max_rlength+SIMD_NCHARS+SIMD_NCHARS)*sizeof(Score16_T));

    new->aligned.one.directions_ptrs_2 = (void **) CALLOC(max_glength+1,sizeof(void *));
    posix_memalign((void **) &(new->aligned.one.directions_space_2),ALIGN_SIZE,
		   (max_glength+1)*(max_rlength+SIMD_NCHARS+SIMD_NCHARS)*sizeof(Score16_T));

    new->nspaces = 1;
  }

#elif defined(HAVE_SSE2)	/* Was HAVE_SSE4_1 || HAVE_SSE2, but the former implies the latter */
  /* Use SIMD_NCHARS > SIMD_NSHORTS and sizeof(Score16_T) > sizeof(Score8_T) */
  if (doublep == true) {
    new->aligned.two.upper_matrix_ptrs = (void **) CALLOC(max_glength+1,sizeof(void *));
    new->aligned.two.upper_matrix_space = (void *) _mm_malloc((max_glength+1)*(max_rlength+SIMD_NCHARS+SIMD_NCHARS)*sizeof(Score16_T),ALIGN_SIZE);
    new->aligned.two.upper_directions_ptrs_0 = (void **) CALLOC(max_glength+1,sizeof(void *));
    new->aligned.two.upper_directions_space_0 = (void *) _mm_malloc((max_glength+1)*(max_rlength+SIMD_NCHARS+SIMD_NCHARS)*sizeof(Score16_T),ALIGN_SIZE);
    new->aligned.two.upper_directions_ptrs_1 = (void **) CALLOC(max_glength+1,sizeof(void *));
    new->aligned.two.upper_directions_space_1 = (void *) _mm_malloc((max_glength+1)*(max_rlength+SIMD_NCHARS+SIMD_NCHARS)*sizeof(Score16_T),ALIGN_SIZE);

    new->aligned.two.lower_matrix_ptrs = (void **) CALLOC(max_rlength+1,sizeof(void *));
    new->aligned.two.lower_matrix_space = (void *) _mm_malloc((max_rlength+1)*(max_glength+SIMD_NCHARS+SIMD_NCHARS)*sizeof(Score16_T),ALIGN_SIZE);
    new->aligned.two.lower_directions_ptrs_0 = (void **) CALLOC(max_rlength+1,sizeof(void *));
    new->aligned.two.lower_directions_space_0 = (void *) _mm_malloc((max_rlength+1)*(max_glength+SIMD_NCHARS+SIMD_NCHARS)*sizeof(Score16_T),ALIGN_SIZE);
    new->aligned.two.lower_directions_ptrs_1 = (void **) CALLOC(max_rlength+1,sizeof(void *));
    new->aligned.two.lower_directions_space_1 = (void *) _mm_malloc((max_rlength+1)*(max_glength+SIMD_NCHARS+SIMD_NCHARS)*sizeof(Score16_T),ALIGN_SIZE);

    new->nspaces = 2;

  } else {
    new->aligned.one.matrix_ptrs = (void **) CALLOC(max_glength+1,sizeof(void *));
    new->aligned.one.matrix_space = (void *) _mm_malloc((max_glength+1)*(max_rlength+SIMD_NCHARS+SIMD_NCHARS)*sizeof(Score16_T),ALIGN_SIZE);
    new->aligned.one.directions_ptrs_0 = (void **) CALLOC(max_glength+1,sizeof(void *));
    new->aligned.one.directions_space_0 = (void *) _mm_malloc((max_glength+1)*(max_rlength+SIMD_NCHARS+SIMD_NCHARS)*sizeof(Score16_T),ALIGN_SIZE);
    new->aligned.one.directions_ptrs_1 = (void **) CALLOC(max_glength+1,sizeof(void *));
    new->aligned.one.directions_space_1 = (void *) _mm_malloc((max_glength+1)*(max_rlength+SIMD_NCHARS+SIMD_NCHARS)*sizeof(Score16_T),ALIGN_SIZE);
    new->aligned.one.directions_ptrs_2 = (void **) CALLOC(max_glength+1,sizeof(void *));
    new->aligned.one.directions_space_2 = (void *) _mm_malloc((max_glength+1)*(max_rlength+SIMD_NCHARS+SIMD_NCHARS)*sizeof(Score16_T),ALIGN_SIZE);

    new->nspaces = 1;
  }
#endif

#if defined(DEBUG_AVX2)
  /* Use SIMD_NCHARS > SIMD_NSHORTS and sizeof(Score16_T) > sizeof(Score8_T) */
  if (doublep == true) {
    new->aligned_std.two.upper_matrix_ptrs = (void **) CALLOC(max_glength+1,sizeof(void *));
    new->aligned_std.two.upper_matrix_space = (void *) _mm_malloc((max_glength+1)*(max_rlength+SIMD_NCHARS+SIMD_NCHARS)*sizeof(Score16_T),ALIGN_SIZE);
    new->aligned_std.two.upper_directions_ptrs_0 = (void **) CALLOC(max_glength+1,sizeof(void *));
    new->aligned_std.two.upper_directions_space_0 = (void *) _mm_malloc((max_glength+1)*(max_rlength+SIMD_NCHARS+SIMD_NCHARS)*sizeof(Score16_T),ALIGN_SIZE);
    new->aligned_std.two.upper_directions_ptrs_1 = (void **) CALLOC(max_glength+1,sizeof(void *));
    new->aligned_std.two.upper_directions_space_1 = (void *) _mm_malloc((max_glength+1)*(max_rlength+SIMD_NCHARS+SIMD_NCHARS)*sizeof(Score16_T),ALIGN_SIZE);

    new->aligned_std.two.lower_matrix_ptrs = (void **) CALLOC(max_rlength+1,sizeof(void *));
    new->aligned_std.two.lower_matrix_space = (void *) _mm_malloc((max_rlength+1)*(max_glength+SIMD_NCHARS+SIMD_NCHARS)*sizeof(Score16_T),ALIGN_SIZE);
    new->aligned_std.two.lower_directions_ptrs_0 = (void **) CALLOC(max_rlength+1,sizeof(void *));
    new->aligned_std.two.lower_directions_space_0 = (void *) _mm_malloc((max_rlength+1)*(max_glength+SIMD_NCHARS+SIMD_NCHARS)*sizeof(Score16_T),ALIGN_SIZE);
    new->aligned_std.two.lower_directions_ptrs_1 = (void **) CALLOC(max_rlength+1,sizeof(void *));
    new->aligned_std.two.lower_directions_space_1 = (void *) _mm_malloc((max_rlength+1)*(max_glength+SIMD_NCHARS+SIMD_NCHARS)*sizeof(Score16_T),ALIGN_SIZE);

    new->nspaces = 2;

  } else {
    new->aligned_std.one.matrix_ptrs = (void **) CALLOC(max_glength+1,sizeof(void *));
    new->aligned_std.one.matrix_space = (void *) _mm_malloc((max_glength+1)*(max_rlength+SIMD_NCHARS+SIMD_NCHARS)*sizeof(Score16_T),ALIGN_SIZE);
    new->aligned_std.one.directions_ptrs_0 = (void **) CALLOC(max_glength+1,sizeof(void *));
    new->aligned_std.one.directions_space_0 = (void *) _mm_malloc((max_glength+1)*(max_rlength+SIMD_NCHARS+SIMD_NCHARS)*sizeof(Score16_T),ALIGN_SIZE);
    new->aligned_std.one.directions_ptrs_1 = (void **) CALLOC(max_glength+1,sizeof(void *));
    new->aligned_std.one.directions_space_1 = (void *) _mm_malloc((max_glength+1)*(max_rlength+SIMD_NCHARS+SIMD_NCHARS)*sizeof(Score16_T),ALIGN_SIZE);
    new->aligned_std.one.directions_ptrs_2 = (void **) CALLOC(max_glength+1,sizeof(void *));
    new->aligned_std.one.directions_space_2 = (void *) _mm_malloc((max_glength+1)*(max_rlength+SIMD_NCHARS+SIMD_NCHARS)*sizeof(Score16_T),ALIGN_SIZE);

    new->nspaces = 1;
  }
#endif
  return new;
}


void
Dynprog_free (T *old) {
  if (*old) {
#ifdef DEBUG12
    FREE((*old)->matrix3_ptrs);
    FREE((*old)->matrix3_space);
#endif
#if !defined(HAVE_SSE2) || defined(DEBUG_SIMD)
    FREE((*old)->matrix_ptrs);
    FREE((*old)->matrix_space);
    FREE((*old)->directions_ptrs_2);
    FREE((*old)->directions_space_2);
    FREE((*old)->directions_ptrs_1);
    FREE((*old)->directions_space_1);
    FREE((*old)->directions_ptrs_0);
    FREE((*old)->directions_space_0);
#endif

#ifdef HAVE_ARM
    if ((*old)->nspaces == 1) {
      FREE((*old)->aligned.one.matrix_ptrs);
      FREE((*old)->aligned.one.matrix_space);
      FREE((*old)->aligned.one.directions_ptrs_2);
      FREE((*old)->aligned.one.directions_space_2);
      FREE((*old)->aligned.one.directions_ptrs_1);
      FREE((*old)->aligned.one.directions_space_1);
      FREE((*old)->aligned.one.directions_ptrs_0);
      FREE((*old)->aligned.one.directions_space_0);
    } else {
      FREE((*old)->aligned.two.upper_matrix_ptrs);
      FREE((*old)->aligned.two.upper_matrix_space);
      FREE((*old)->aligned.two.upper_directions_ptrs_1);
      FREE((*old)->aligned.two.upper_directions_space_1);
      FREE((*old)->aligned.two.upper_directions_ptrs_0);
      FREE((*old)->aligned.two.upper_directions_space_0);

      FREE((*old)->aligned.two.lower_matrix_ptrs);
      FREE((*old)->aligned.two.lower_matrix_space);
      FREE((*old)->aligned.two.lower_directions_ptrs_1);
      FREE((*old)->aligned.two.lower_directions_space_1);
      FREE((*old)->aligned.two.lower_directions_ptrs_0);
      FREE((*old)->aligned.two.lower_directions_space_0);
    }

#elif defined(HAVE_SSE2) /* Was HAVE_SSE4_1 || HAVE_SSE2, but the former implies the latter */
    if ((*old)->nspaces == 1) {
      FREE((*old)->aligned.one.matrix_ptrs);
      _mm_free((*old)->aligned.one.matrix_space);
      FREE((*old)->aligned.one.directions_ptrs_2);
      _mm_free((*old)->aligned.one.directions_space_2);
      FREE((*old)->aligned.one.directions_ptrs_1);
      _mm_free((*old)->aligned.one.directions_space_1);
      FREE((*old)->aligned.one.directions_ptrs_0);
      _mm_free((*old)->aligned.one.directions_space_0);
    } else {
      FREE((*old)->aligned.two.upper_matrix_ptrs);
      _mm_free((*old)->aligned.two.upper_matrix_space);
      FREE((*old)->aligned.two.upper_directions_ptrs_1);
      _mm_free((*old)->aligned.two.upper_directions_space_1);
      FREE((*old)->aligned.two.upper_directions_ptrs_0);
      _mm_free((*old)->aligned.two.upper_directions_space_0);

      FREE((*old)->aligned.two.lower_matrix_ptrs);
      _mm_free((*old)->aligned.two.lower_matrix_space);
      FREE((*old)->aligned.two.lower_directions_ptrs_1);
      _mm_free((*old)->aligned.two.lower_directions_space_1);
      FREE((*old)->aligned.two.lower_directions_ptrs_0);
      _mm_free((*old)->aligned.two.lower_directions_space_0);
    }
#endif

#if defined(DEBUG_AVX2)
    if ((*old)->nspaces == 1) {
      FREE((*old)->aligned_std.one.matrix_ptrs);
      _mm_free((*old)->aligned_std.one.matrix_space);
      FREE((*old)->aligned_std.one.directions_ptrs_2);
      _mm_free((*old)->aligned_std.one.directions_space_2);
      FREE((*old)->aligned_std.one.directions_ptrs_1);
      _mm_free((*old)->aligned_std.one.directions_space_1);
      FREE((*old)->aligned_std.one.directions_ptrs_0);
      _mm_free((*old)->aligned_std.one.directions_space_0);
    } else {
      FREE((*old)->aligned_std.two.upper_matrix_ptrs);
      _mm_free((*old)->aligned_std.two.upper_matrix_space);
      FREE((*old)->aligned_std.two.upper_directions_ptrs_1);
      _mm_free((*old)->aligned_std.two.upper_directions_space_1);
      FREE((*old)->aligned_std.two.upper_directions_ptrs_0);
      _mm_free((*old)->aligned_std.two.upper_directions_space_0);

      FREE((*old)->aligned_std.two.lower_matrix_ptrs);
      _mm_free((*old)->aligned_std.two.lower_matrix_space);
      FREE((*old)->aligned_std.two.lower_directions_ptrs_1);
      _mm_free((*old)->aligned_std.two.lower_directions_space_1);
      FREE((*old)->aligned_std.two.lower_directions_ptrs_0);
      _mm_free((*old)->aligned_std.two.lower_directions_space_0);
    }
#endif

    FREE(*old);
  }

  return;
}

/************************************************************************/

/* TODO: Replace pairdistance_array with reference to consistent_array */

/* These are extern arrays, used by all dynprog procedures */
int use8p_size[NMISMATCHTYPES];
Pairdistance_T **pairdistance_array[NMISMATCHTYPES];
#ifndef HAVE_SSE4_1
Pairdistance_T **pairdistance_array_plus_128[NMISMATCHTYPES];
#endif
bool **consistent_array[3];	/* For genestrands 0, +1, and +2 */
int *nt_to_int_array;


bool
Dynprog_consistent_p (int c, int g, int g_alt, int genestrand) {
  if (consistent_array[genestrand][c][g] == true) {
    return true;
  } else {
    return consistent_array[genestrand][c][g_alt];
  }
}

static void
permute_cases (int NA1, int NA2, Pairdistance_T score, Mode_T mode) {
  int i;

#ifdef PREUC
  int na1, na2;

  na1 = tolower(NA1);
  na2 = tolower(NA2);
#endif

  if (mode == STANDARD || mode == CMET_STRANDED || mode == ATOI_STRANDED || mode == TTOC_STRANDED) {
#ifdef PREUC
    consistent_array[0][na1][na2] = true;
    consistent_array[0][na1][NA2] = true;
    consistent_array[0][NA1][na2] = true;
#endif
    consistent_array[0][NA1][NA2] = true;

  } else {
#ifdef PREUC
    consistent_array[+1][na1][na2] = true;
    consistent_array[+1][na1][NA2] = true;
    consistent_array[+1][NA1][na2] = true;
    consistent_array[+2][na1][na2] = true;
    consistent_array[+2][na1][NA2] = true;
    consistent_array[+2][NA1][na2] = true;
#endif
    consistent_array[+1][NA1][NA2] = true;
    consistent_array[+2][NA1][NA2] = true;
  }

  if (mode == STANDARD || mode == CMET_STRANDED || mode == ATOI_STRANDED || mode == TTOC_STRANDED) {
#ifdef PREUC
    consistent_array[0][na2][na1] = true;
    consistent_array[0][na2][NA1] = true;
    consistent_array[0][NA2][na1] = true;
#endif
    consistent_array[0][NA2][NA1] = true;

  } else {
#ifdef PREUC
    consistent_array[+1][na2][na1] = true;
    consistent_array[+1][na2][NA1] = true;
    consistent_array[+1][NA2][na1] = true;
    consistent_array[+2][na2][na1] = true;
    consistent_array[+2][na2][NA1] = true;
    consistent_array[+2][NA2][na1] = true;
#endif
    consistent_array[+1][NA2][NA1] = true;
    consistent_array[+2][NA2][NA1] = true;
  }


  for (i = 0; i < NMISMATCHTYPES; i++) {
#ifdef PREUC
    pairdistance_array[i][na1][na2] = score;
    pairdistance_array[i][na1][NA2] = score;
    pairdistance_array[i][NA1][na2] = score;
#endif
    pairdistance_array[i][NA1][NA2] = score;

#ifdef PREUC
    pairdistance_array[i][na2][na1] = score;
    pairdistance_array[i][na2][NA1] = score;
    pairdistance_array[i][NA2][na1] = score;
#endif
    pairdistance_array[i][NA2][NA1] = score;
  }

  return;
}

static void
permute_cases_oneway (int NA1, int NA2, Pairdistance_T score, int genestrand) {
  int i;

#ifdef PREUC
  int na1, na2;

  na1 = tolower(NA1);
  na2 = tolower(NA2);
#endif

#ifdef PREUC
  consistent_array[genestrand][na1][na2] = true;
  consistent_array[genestrand][na1][NA2] = true;
  consistent_array[genestrand][NA1][na2] = true;
#endif
  consistent_array[genestrand][NA1][NA2] = true;

  for (i = 0; i < NMISMATCHTYPES; i++) {
#ifdef PREUC
    pairdistance_array[i][na1][na2] = score;
    pairdistance_array[i][na1][NA2] = score;
    pairdistance_array[i][NA1][na2] = score;
#endif
    pairdistance_array[i][NA1][NA2] = score;
  }

  return;
}


void
Dynprog_init (Mode_T mode) {
  int i, j, ptr;
  int c, c1, c2;

  nt_to_int_array = (int *) CALLOC(128,sizeof(int));
  for (j = 0; j < 128; j++) {
    nt_to_int_array[j] = 4;
  }
  nt_to_int_array['A'] = nt_to_int_array['a'] = 0;
  nt_to_int_array['C'] = nt_to_int_array['c'] = 1;
  nt_to_int_array['G'] = nt_to_int_array['g'] = 2;
  nt_to_int_array['T'] = nt_to_int_array['t'] = 3;


  use8p_size[HIGHQ] = NEG_INFINITY_8 / MISMATCH_HIGHQ - 1;
  use8p_size[MEDQ] = NEG_INFINITY_8 / MISMATCH_MEDQ - 1;
  use8p_size[LOWQ] = NEG_INFINITY_8 / MISMATCH_LOWQ - 1;
  use8p_size[ENDQ] = NEG_INFINITY_8 / MISMATCH_ENDQ - 1;
  /* printf("use8p_sizes: %d %d %d %d\n",use8p_size[HIGHQ],use8p_size[MEDQ],use8p_size[LOWQ],use8p_size[ENDQ]); */

  if (mode == STANDARD || mode == CMET_STRANDED || mode == ATOI_STRANDED || mode == TTOC_STRANDED) {
    consistent_array[0] = (bool **) CALLOC(128,sizeof(bool *));
    consistent_array[0][0] = (bool *) CALLOC(128*128,sizeof(bool));

    ptr = 0;
    for (j = 1; j < 128; j++) {
      ptr += 128;
      consistent_array[0][j] = &(consistent_array[0][0][ptr]);
    }

  } else {
    consistent_array[+1] = (bool **) CALLOC(128,sizeof(bool *));
    consistent_array[+1][0] = (bool *) CALLOC(128*128,sizeof(bool));
    consistent_array[+2] = (bool **) CALLOC(128,sizeof(bool *));
    consistent_array[+2][0] = (bool *) CALLOC(128*128,sizeof(bool));

    ptr = 0;
    for (j = 1; j < 128; j++) {
      ptr += 128;
      consistent_array[+1][j] = &(consistent_array[+1][0][ptr]);
      consistent_array[+2][j] = &(consistent_array[+2][0][ptr]);
    }
  }

  for (i = 0; i < NMISMATCHTYPES; i++) {
    pairdistance_array[i] = (Pairdistance_T **) CALLOC(128,sizeof(Pairdistance_T *));
    pairdistance_array[i][0] = (Pairdistance_T *) CALLOC(128*128,sizeof(Pairdistance_T));
#ifndef HAVE_SSE4_1
    pairdistance_array_plus_128[i] = (Pairdistance_T **) CALLOC(128,sizeof(Pairdistance_T *));
    pairdistance_array_plus_128[i][0] = (Pairdistance_T *) CALLOC(128*128,sizeof(Pairdistance_T));
#endif
    ptr = 0;
    for (j = 1; j < 128; j++) {
      ptr += 128;
      pairdistance_array[i][j] = &(pairdistance_array[i][0][ptr]);
#ifndef HAVE_SSE4_1
      pairdistance_array_plus_128[i][j] = &(pairdistance_array_plus_128[i][0][ptr]);
#endif
    }
  }

#ifdef PREUC
  for (c1 = 'A'; c1 <= 'z'; c1++) {
    for (c2 = 'A'; c2 < 'z'; c2++) {
      pairdistance_array[HIGHQ][c1][c2] = MISMATCH_HIGHQ;
      pairdistance_array[MEDQ][c1][c2] = MISMATCH_MEDQ;
      pairdistance_array[LOWQ][c1][c2] = MISMATCH_LOWQ;
      pairdistance_array[ENDQ][c1][c2] = MISMATCH_ENDQ;
    }
  }
#else
  for (c1 = 'A'; c1 <= 'Z'; c1++) {
    for (c2 = 'A'; c2 < 'Z'; c2++) {
      pairdistance_array[HIGHQ][c1][c2] = MISMATCH_HIGHQ;
      pairdistance_array[MEDQ][c1][c2] = MISMATCH_MEDQ;
      pairdistance_array[LOWQ][c1][c2] = MISMATCH_LOWQ;
      pairdistance_array[ENDQ][c1][c2] = MISMATCH_ENDQ;
    }
  }
#endif

  for (c = 'A'; c < 'Z'; c++) {
    permute_cases(c,c,FULLMATCH,mode);
  }

  /* Exceptions */
  permute_cases('U','T',FULLMATCH,mode);

  permute_cases('R','A',HALFMATCH,mode);
  permute_cases('R','G',HALFMATCH,mode);

  permute_cases('Y','T',HALFMATCH,mode);
  permute_cases('Y','C',HALFMATCH,mode);

  permute_cases('W','A',HALFMATCH,mode);
  permute_cases('W','T',HALFMATCH,mode);

  permute_cases('S','G',HALFMATCH,mode);
  permute_cases('S','C',HALFMATCH,mode);

  permute_cases('M','A',HALFMATCH,mode);
  permute_cases('M','C',HALFMATCH,mode);

  permute_cases('K','G',HALFMATCH,mode);
  permute_cases('K','T',HALFMATCH,mode);

  permute_cases('H','A',AMBIGUOUS,mode);
  permute_cases('H','T',AMBIGUOUS,mode);
  permute_cases('H','C',AMBIGUOUS,mode);

  permute_cases('B','G',AMBIGUOUS,mode);
  permute_cases('B','C',AMBIGUOUS,mode);
  permute_cases('B','T',AMBIGUOUS,mode);

  permute_cases('V','G',AMBIGUOUS,mode);
  permute_cases('V','A',AMBIGUOUS,mode);
  permute_cases('V','C',AMBIGUOUS,mode);

  permute_cases('D','G',AMBIGUOUS,mode);
  permute_cases('D','A',AMBIGUOUS,mode);
  permute_cases('D','T',AMBIGUOUS,mode);

  permute_cases('N','T',AMBIGUOUS,mode);
  permute_cases('N','C',AMBIGUOUS,mode);
  permute_cases('N','A',AMBIGUOUS,mode);
  permute_cases('N','G',AMBIGUOUS,mode);

  permute_cases('X','T',AMBIGUOUS,mode);
  permute_cases('X','C',AMBIGUOUS,mode);
  permute_cases('X','A',AMBIGUOUS,mode);
  permute_cases('X','G',AMBIGUOUS,mode);

  permute_cases('N','N',AMBIGUOUS,mode); /* Needed to start dynprog procedures with 0 at (0,0) */
  permute_cases('X','X',AMBIGUOUS,mode); /* Needed to start dynprog procedures with 0 at (0,0) */


  /* GMAP keeps query in same order and inverts genome, so need to keep only one direction for STRANDED */
  if (mode == STANDARD) {
    /* Skip */

  } else if (mode == CMET_STRANDED) {
    /* Query-T can match Genomic-C */
    permute_cases_oneway('T','C',FULLMATCH,/*genestrand*/0);

  } else if (mode == CMET_NONSTRANDED) {
    permute_cases_oneway('T','C',FULLMATCH,/*genestrand*/+1);
    permute_cases_oneway('A','G',FULLMATCH,/*genestrand*/+2);

  } else if (mode == ATOI_STRANDED) {
    permute_cases_oneway('G','A',FULLMATCH,/*genestrand*/0);
 
  } else if (mode == ATOI_NONSTRANDED) {
    permute_cases_oneway('G','A',FULLMATCH,/*genestrand*/+1);
    permute_cases_oneway('C','T',FULLMATCH,/*genestrand*/+2);

  } else if (mode == TTOC_STRANDED) {
    permute_cases_oneway('C','T',FULLMATCH,/*genestrand*/0);

  } else if (mode == TTOC_NONSTRANDED) {
    permute_cases_oneway('C','T',FULLMATCH,/*genestrand*/+1);
    permute_cases_oneway('G','A',FULLMATCH,/*genestrand*/+2);

  } else {
    fprintf(stderr,"Mode %d not recognized\n",mode);
    exit(9);
  }


#ifndef HAVE_SSE4_1
#ifdef PREUC
  for (i = 0; i < NMISMATCHTYPES; i++) {
    for (c1 = 'A'; c1 <= 'z'; c1++) {
      for (c2 = 'A'; c2 < 'z'; c2++) {
	pairdistance_array_plus_128[i][c1][c2] = 128 + pairdistance_array[i][c1][c2];
      }
    }
  }
#else
  for (i = 0; i < NMISMATCHTYPES; i++) {
    for (c1 = 'A'; c1 <= 'Z'; c1++) {
      for (c2 = 'A'; c2 < 'Z'; c2++) {
	pairdistance_array_plus_128[i][c1][c2] = 128 + pairdistance_array[i][c1][c2];
      }
    }
  }
#endif
#endif

  return;
}


/************************************************************************/

void
Dynprog_term (Mode_T mode) {
  int i;

#if 0
  for (i = 0; i < NJUMPTYPES; i++) {
    FREE(jump_penalty_array[i]);
  }
#endif

  for (i = 0; i < NMISMATCHTYPES; i++) {
    /*
    for (j = 0; j < 128; j++) {
      FREE(pairdistance_array[i][j]);
    }
    */
#ifndef HAVE_SSE4_1
    FREE(pairdistance_array_plus_128[i][0]);
    FREE(pairdistance_array_plus_128[i]);
#endif
    FREE(pairdistance_array[i][0]);
    FREE(pairdistance_array[i]);
  }
  /*
  for (j = 0; j < 128; j++) {
    FREE(consistent_array[j]);
  }
  */
  if (mode == STANDARD || mode == CMET_STRANDED || mode == ATOI_STRANDED || mode == TTOC_STRANDED) {
    FREE(consistent_array[0][0]);
    FREE(consistent_array[0]);
  } else {
    FREE(consistent_array[+1][0]);
    FREE(consistent_array[+1]);
    FREE(consistent_array[+2][0]);
    FREE(consistent_array[+2]);
  }
  FREE(nt_to_int_array);

  return;
}

/************************************************************************/

void
Dynprog_compute_bands (int *lband, int *uband, int rlength, int glength, int extraband, bool widebandp) {
  if (widebandp == false) {
    /* Just go along main diagonal */
    *lband = extraband;
    *uband = extraband;
  } else if (glength >= rlength) {
    /* Widen band to right to reach destination */
    *uband = glength - rlength + extraband;
    *lband = extraband;
  } else {
    /* Widen band to left to reach destination */
    *lband = rlength - glength + extraband;
    *uband = extraband;
  }
  return;
}


/* For upper triangular, we allow horizontal (E) jumps.  For lower triangular, we allow vertical (F) jumps */
#if !defined(HAVE_SSE2) || defined(DEBUG_SIMD)
Score32_T **
Dynprog_standard (Direction32_T ***directions_nogap, Direction32_T ***directions_Egap, Direction32_T ***directions_Fgap,
		  T this, char *rsequence, char *gsequence, char *gsequence_alt,
		  int rlength, int glength, int goffset,
		  Genome_T genome, Genome_T genomealt,
		  Univcoord_T chroffset, Univcoord_T chrhigh, bool watsonp,
		  Mismatchtype_T mismatchtype, Score32_T open, Score32_T extend,
		  int lband, int uband, bool jump_late_p, bool revp, int saturation,
		  bool upperp, bool lowerp) {
#ifdef DEBUG12
  Score32_T bestscore;
  Direction32_T bestdir;
  struct Int3_T **matrix3;
#endif
  Score32_T penalty;
  Score32_T **matrix;
  Score32_T c_gap, *r_gap, *nogap, last_nogap, prev_nogap, first_nogap;
  int r, c, na1, na2;
  char na2_alt;
  Score32_T score, pairscore;
  int rlo, rhigh;
  Pairdistance_T **pairdistance_array_type;

  pairdistance_array_type = pairdistance_array[mismatchtype];

  debug2(printf("Dynprog_standard.  jump_late_p %d, open %d, extend %d\n",jump_late_p,open,extend));
  debug(printf("compute_scores_standard: "));
  debug(printf("Lengths are %d and %d, so bands are %d on left and %d on right\n",rlength,glength,lband,uband));

  matrix = Matrix32_alloc(rlength,glength,this->matrix_ptrs,this->matrix_space);
  *directions_nogap = Directions32_alloc(rlength,glength,this->directions_ptrs_0,this->directions_space_0);
  *directions_Egap = Directions32_alloc(rlength,glength,this->directions_ptrs_1,this->directions_space_1);
  *directions_Fgap = Directions32_alloc(rlength,glength,this->directions_ptrs_2,this->directions_space_2);
  /* (*directions_nogap)[0][0] = STOP; -- Check for r > 0 && c > 0 instead */

#ifdef ZERO_INITIAL_GAP_PENALTY
  /* Row 0 initialization */
  for (c = 1; c <= uband && c <= glength; c++) {
    matrix[c][0] = 0;
    (*directions_Egap)[c][0] = HORIZ;
    (*directions_nogap)[c][0] = HORIZ;
  }

  /* Column 0 initialization */
  for (r = 1; r <= lband && r <= rlength; r++) {
    matrix[0][r] = 0;
    (*directions_Fgap)[0][r] = VERT;
    (*directions_nogap)[0][r] = VERT;
  }

#else
  /* Row 0 initialization */
  penalty = open;
  for (c = 1; c <= uband && c <= glength; c++) {
    penalty += extend;
    matrix[c][0] = penalty;
    (*directions_Egap)[c][0] = HORIZ;
    (*directions_nogap)[c][0] = HORIZ;
  }
#if 0
  /* Already initialized to DIAG */
  (*directions_Egap)[1][0] = DIAG; /* previously used STOP */
#endif

  /* Column 0 initialization */
  penalty = open;
  for (r = 1; r <= lband && r <= rlength; r++) {
    penalty += extend;
    matrix[0][r] = penalty;
    (*directions_Fgap)[0][r] = VERT;
    (*directions_nogap)[0][r] = VERT;
  }
#if 0
  /* Already initialized to DIAG */
  (*directions_Fgap)[0][1] = DIAG; /* previously used STOP */
#endif
#endif

  r_gap = (Score32_T *) MALLOCA((rlength+1) * sizeof(Score32_T));
  nogap = (Score32_T *) MALLOCA((rlength+1) * sizeof(Score32_T));

#ifdef ZERO_INITIAL_GAP_PENALTY
  nogap[0] = 0;
  for (r = 1; r <= lband && r <= rlength; r++) {
    r_gap[r] = NEG_INFINITY_32;
    nogap[r] = 0;
  }
  for ( ; r <= rlength; r++) {
    r_gap[r] = NEG_INFINITY_32;
    nogap[r] = NEG_INFINITY_32;
  }
#else
  nogap[0] = 0;
  penalty = open;
  for (r = 1; r <= lband && r <= rlength; r++) {
    penalty += extend;
    r_gap[r] = NEG_INFINITY_32;
    nogap[r] = penalty;
  }
  for ( ; r <= rlength; r++) {
    r_gap[r] = NEG_INFINITY_32;
    nogap[r] = NEG_INFINITY_32;
  }
#endif


#ifdef DEBUG12
  matrix3 = Matrix3_alloc(rlength,glength,this->matrix3_ptrs,this->matrix3_space);
  matrix3[0][0].nogap = 0;
  matrix3[0][0].Egap = matrix3[0][0].Fgap = NEG_INFINITY_32;

  /* Row 0 initialization */
  penalty = open;
  for (c = 1; c <= uband && c <= glength; c++) {
    penalty += extend;
    matrix3[c][0].nogap = penalty;
    matrix3[c][0].Egap = penalty;
    matrix3[c][0].Fgap = NEG_INFINITY_32;
  }

  /* Column 0 initialization */
  penalty = open;
  for (r = 1; r <= lband && r <= rlength; r++) {
    penalty += extend;
    matrix3[0][r].nogap = penalty;
    matrix3[0][r].Egap = NEG_INFINITY_32;
    matrix3[0][r].Fgap = penalty;
  }
#endif


  first_nogap = 0;
  if (jump_late_p) {
    penalty = open + extend;
    for (c = 1; c <= glength; c++) {
      if (gsequence) {
	na2 = revp ? gsequence[1-c] : gsequence[c-1];
	na2_alt = revp ? gsequence_alt[1-c] : gsequence_alt[c-1];
      } else if (revp == false) {
	na2 = get_genomic_nt(&na2_alt,genome,genomealt,goffset+c-1,chroffset,chrhigh,watsonp);
      } else {
	na2 = get_genomic_nt(&na2_alt,genome,genomealt,goffset+1-c,chroffset,chrhigh,watsonp);
      }

      c_gap = NEG_INFINITY_32;
      if (c == 1) {
	rlo = 1;
	prev_nogap = 0; /* was nogap[rlo-1] */
#ifdef ZERO_INITIAL_GAP_PENALTY
	last_nogap = 0;
#else
	last_nogap = NEG_INFINITY_32 - open + 1;  /* making c_gap < last_nogap + open */
#endif
      } else if ((rlo = c - uband) < 1) {
	rlo = 1;
#ifdef ZERO_INITIAL_GAP_PENALTY
	prev_nogap = 0;
	last_nogap = 0;
#else
	prev_nogap = penalty;
	penalty += extend;
	last_nogap = penalty;
#endif
      } else if (rlo == 1) {
#ifdef ZERO_INITIAL_GAP_PENALTY
        prev_nogap = 0;
#else
	prev_nogap = penalty;
#endif
	/* penalty += extend; */
	last_nogap = NEG_INFINITY_32;
#ifdef DEBUG12
	matrix3[c][rlo-1].Fgap = NEG_INFINITY_32;
	matrix3[c][rlo-1].nogap = NEG_INFINITY_32;
#endif
      } else {
	prev_nogap = first_nogap;
	last_nogap = NEG_INFINITY_32;
#ifdef DEBUG12
	matrix3[c][rlo-1].Fgap = NEG_INFINITY_32;
	matrix3[c][rlo-1].nogap = NEG_INFINITY_32;
#endif
      }

      if ((rhigh = c + lband) > rlength) {
	rhigh = rlength;
#ifdef DEBUG12
      } else {
	matrix3[c-1][rhigh].Egap = NEG_INFINITY_32;
	matrix3[c-1][rhigh].nogap = NEG_INFINITY_32;
#endif
      }

      for (r = rlo; r <= rhigh; r++) {
	na1 = revp ? rsequence[1-r] : rsequence[r-1];

#ifdef DEBUG12
	/* FGAP */
	bestscore = matrix3[c][r-1].nogap + open /* + extend */;
	bestdir = DIAG;

	if ((score = matrix3[c][r-1].Fgap /* + extend */) >= bestscore) {  /* Use >= for jump late */
	  bestscore = score;
	  bestdir = VERT;
	}

	matrix3[c][r].Fgap = bestscore + extend;
#endif

	/* FGAP alt */
#ifdef DEBUG12A
	printf("Fgap at r %d, c %d: matrix3[c][r-1].nogap %d vs last_nogap %d\n",r,c,matrix3[c][r-1].nogap,last_nogap);
	printf("Fgap at r %d, c %d: matrix3[c][r-1].Fgap %d vs c_gap %d\n",r,c,matrix3[c][r-1].Fgap,c_gap);
#endif
#ifdef DEBUG12
	assert(matrix3[c][r-1].nogap == last_nogap);
	assert(matrix3[c][r-1].Fgap == c_gap);
#endif
	debug3(printf("std Fgap at r %d, c %d: c_gap + extend %d vs last_nogap + open + extend %d\n",r,c,c_gap + extend,last_nogap + open + extend));
	if (lowerp == true && c_gap /* + extend */ >= (score = last_nogap + open /* + extend */)) {  /* Use >= for jump late */
	  c_gap += extend;
	  (*directions_Fgap)[c][r] = VERT;
	} else {
	  c_gap = score + extend;
	  /* bestdir2 = DIAG; -- Already initialized to DIAG */
	}


#ifdef DEBUG12
	/* EGAP */
	bestscore = matrix3[c-1][r].nogap + open /* + extend */;
	bestdir = DIAG;

	if ((score = matrix3[c-1][r].Egap /* + extend */) >= bestscore) {  /* Use >= for jump late */
	  bestscore = score;
	  bestdir = HORIZ;
	}

	matrix3[c][r].Egap = bestscore + extend;
#endif

	/* EGAP alt */
#ifdef DEBUG12A
	printf("Egap at r %d, c %d: matrix3[c-1][r].nogap %d vs nogap[r] %d\n",r,c,matrix3[c-1][r].nogap,nogap[r]);
	printf("Egap at r %d, c %d: matrix3[c-1][r].Egap %d vs r_gap[r] %d\n",r,c,matrix3[c-1][r].Egap,r_gap[r]);
#endif
#ifdef DEBUG12
	assert(matrix3[c-1][r].nogap == nogap[r]);
	assert(matrix3[c-1][r].Egap == r_gap[r]);
#endif
	/* debug3(printf("Egap at r %d, c %d: r_gap[r] %d vs nogap[r] + open %d\n",r,c,r_gap[r],nogap[r]+open)); */
	if (upperp == true && r_gap[r] /* + extend */ >= (score = nogap[r] + open /* + extend */)) {  /* Use >= for jump late */
	  r_gap[r] += extend;
	  (*directions_Egap)[c][r] = HORIZ;
	} else {
	  r_gap[r] = score + extend;
	  /* bestdir2 = DIAG; -- Already initialized to DIAG */
	}


	/* NOGAP */
	pairscore = pairdistance_array_type[na1][na2];
	if ((score = pairdistance_array_type[na1][(int) na2_alt]) > pairscore) {
	  pairscore = score;
	}
#ifdef DEBUG12
	bestscore = matrix3[c-1][r-1].nogap + pairscore;
	bestdir = DIAG;
      
	if ((score = matrix3[c][r].Egap) >= bestscore) {  /* Use >= for jump late */
	  bestscore = score;
	  bestdir = HORIZ;
	}

	if ((score = matrix3[c][r].Fgap) >= bestscore) {  /* Use >= for jump late */
	  bestscore = score;
	  bestdir = VERT;
	}

	matrix3[c][r].nogap = bestscore;
#endif

	/* NOGAP alt */
#ifdef DEBUG12A
	printf("nogap at r %d, c %d: matrix3[c-1][r-1].nogap %d vs prev_nogap %d\n",r,c,matrix3[c-1][r-1].nogap,prev_nogap);
	printf("nogap at r %d, c %d: matrix3[c][r].Fgap %d vs c_gap %d\n",r,c,matrix3[c][r].Fgap,c_gap);
	printf("nogap at r %d, c %d: matrix3[c][r].Egap %d vs r_gap[r] %d\n",r,c,matrix3[c][r].Egap,r_gap[r]);
#endif
#ifdef DEBUG12
	assert(matrix3[c-1][r-1].nogap == prev_nogap);
	assert(matrix3[c][r].Fgap == c_gap);
	assert(matrix3[c][r].Egap == r_gap[r]);
#endif
	last_nogap = prev_nogap + pairscore;
	/* bestdir2 = DIAG; -- Already initialized to DIAG */
	/* debug3(printf("assign nogap at r %d, c %d: H + pairscore %d vs r_horiz + extend %d vs vert + extend %d\n",
	   r,c,last_nogap,r_gap[r],c_gap)); */
	if (upperp == true && r_gap[r] >= last_nogap) {  /* Use >= for jump late */
	  last_nogap = r_gap[r];
	  (*directions_nogap)[c][r] = HORIZ;
	}
	if (lowerp == true && c_gap >= last_nogap) {  /* Use >= for jump late */
	  last_nogap = c_gap;
	  (*directions_nogap)[c][r] = VERT;
	}
	/* (*directions_nogap)[c][r] = bestdir2; */

	prev_nogap = nogap[r];	/* Save for next inner loop, before we wipe it out */
	matrix[c][r] = nogap[r] =
	  (last_nogap < saturation) ? saturation : last_nogap;	/* Save for next outer loop */
	if (r == rlo) {
	  debug12a(printf("At row %d, storing first_nogap to be nogap[r] %d\n",r,nogap[r]));
	  first_nogap = last_nogap;
	}
      }
      debug12a(printf("\n"));
    }

  } else {
    /* Do not jump late */
    penalty = open + extend;
    for (c = 1; c <= glength; c++) {
      if (gsequence) {
	na2 = revp ? gsequence[1-c] : gsequence[c-1];
	na2_alt = revp ? gsequence_alt[1-c] : gsequence_alt[c-1];
      } else if (revp == false) {
	na2 = get_genomic_nt(&na2_alt,genome,genomealt,goffset+c-1,chroffset,chrhigh,watsonp);
      } else {
	na2 = get_genomic_nt(&na2_alt,genome,genomealt,goffset+1-c,chroffset,chrhigh,watsonp);
      }

      c_gap = NEG_INFINITY_32;
      if (c == 1) {
	rlo = 1;
	prev_nogap = 0; /* was nogap[rlo-1] */
#ifdef ZERO_INITIAL_GAP_PENALTY
	last_nogap = 0;
#else
	last_nogap = NEG_INFINITY_32 - open + 1; /* making c_gap < last_nogap + open */
#endif
      } else if ((rlo = c - uband) < 1) {
	rlo = 1;
#ifdef ZERO_INITIAL_GAP_PENALTY
	prev_nogap = 0;
	last_nogap = 0;
#else
	prev_nogap = penalty;
	penalty += extend;
	last_nogap = penalty;
#endif
      } else if (rlo == 1) {
#ifdef ZERO_INITIAL_GAP_PENALTY
	prev_nogap = 0;
#else
	prev_nogap = penalty;
#endif
	/* penalty += extend; */
	last_nogap = NEG_INFINITY_32;
#ifdef DEBUG12
	matrix3[c][rlo-1].Fgap = NEG_INFINITY_32;
	matrix3[c][rlo-1].nogap = NEG_INFINITY_32;
#endif
      } else {
	prev_nogap = first_nogap;
	last_nogap = NEG_INFINITY_32;
#ifdef DEBUG12
	matrix3[c][rlo-1].Fgap = NEG_INFINITY_32;
	matrix3[c][rlo-1].nogap = NEG_INFINITY_32;
#endif
      }

      if ((rhigh = c + lband) > rlength) {
	rhigh = rlength;
#ifdef DEBUG12
      } else {
	matrix3[c-1][rhigh].Egap = NEG_INFINITY_32;
	matrix3[c-1][rhigh].nogap = NEG_INFINITY_32;
#endif
      }

      for (r = rlo; r <= rhigh; r++) {
	na1 = revp ? rsequence[1-r] : rsequence[r-1];

#ifdef DEBUG12
	/* FGAP */
	bestscore = matrix3[c][r-1].nogap + open /* + extend */;
	bestdir = DIAG;

	if ((score = matrix3[c][r-1].Fgap /* + extend */) > bestscore) {  /* Use > for jump early */
	  bestscore = score;
	  bestdir = VERT;
	}

	matrix3[c][r].Fgap = bestscore + extend;
#endif

	/* FGAP alt */
#ifdef DEBUG12A
	printf("Fgap at r %d, c %d: matrix3[c][r-1].nogap %d vs last_nogap %d\n",r,c,matrix3[c][r-1].nogap,last_nogap);
	printf("Fgap at r %d, c %d: matrix3[c][r-1].Fgap %d vs c_gap %d\n",r,c,matrix3[c][r-1].Fgap,c_gap);
#endif
#ifdef DEBUG12
	assert(matrix3[c][r-1].nogap == last_nogap);
	assert(matrix3[c][r-1].Fgap == c_gap);
#endif
	debug3(printf("std Fgap at r %d, c %d: c_gap + extend %d vs last_nogap + open + extend %d\n",r,c,c_gap + extend,last_nogap + open + extend));
	if (lowerp == true && c_gap /* + extend */ > (score = last_nogap + open /* + extend */)) {  /* Use > for jump early */
	  c_gap += extend;
	  (*directions_Fgap)[c][r] = VERT;
	} else {
	  c_gap = score + extend;
	  /* bestdir2 = DIAG; -- Already initialized to DIAG */
	}


#ifdef DEBUG12
	/* EGAP */
	bestscore = matrix3[c-1][r].nogap + open /* + extend */;
	bestdir = DIAG;

	if ((score = matrix3[c-1][r].Egap /* + extend */) > bestscore) {  /* Use > for jump early */
	  bestscore = score;
	  bestdir = HORIZ;
	}

	matrix3[c][r].Egap = bestscore + extend;
#endif

	/* EGAP alt */
#ifdef DEBUG12A
	printf("Egap at r %d, c %d: matrix3[c-1][r].nogap %d vs nogap[r] %d\n",r,c,matrix3[c-1][r].nogap,nogap[r]);
	printf("Egap at r %d, c %d: matrix3[c-1][r].Egap %d vs r_gap[r] %d\n",r,c,matrix3[c-1][r].Egap,r_gap[r]);
#endif
#ifdef DEBUG12
	assert(matrix3[c-1][r].nogap == nogap[r]);
	assert(matrix3[c-1][r].Egap == r_gap[r]);
#endif
	/* debug3(printf("Egap at r %d, c %d: r_gap[r] %d vs nogap[r] + open %d\n",r,c,r_gap[r],nogap[r]+open)); */
	if (upperp == true && r_gap[r] /* + extend */ > (score = nogap[r] + open /* + extend */)) {  /* Use > for jump early */
	  r_gap[r] += extend;
	  (*directions_Egap)[c][r] = HORIZ;
	} else {
	  r_gap[r] = score + extend;
	  /* bestdir2 = DIAG; -- Already initialized to DIAG */
	}


	/* NOGAP */
	pairscore = pairdistance_array_type[na1][na2];
	if ((score = pairdistance_array_type[na1][(int) na2_alt]) > pairscore) {
	  pairscore = score;
	}
#ifdef DEBUG12
	bestscore = matrix3[c-1][r-1].nogap + pairscore;
	bestdir = DIAG;
      
	if ((score = matrix3[c][r].Egap) > bestscore) {  /* Use > for jump early */
	  bestscore = score;
	  bestdir = HORIZ;
	}

	if ((score = matrix3[c][r].Fgap) > bestscore) {  /* Use > for jump early */
	  bestscore = score;
	  bestdir = VERT;
	}

	matrix3[c][r].nogap = bestscore;
#endif

	/* NOGAP alt */
#ifdef DEBUG12A
	printf("nogap at r %d, c %d: matrix3[c-1][r-1].nogap %d vs prev_nogap %d\n",r,c,matrix3[c-1][r-1].nogap,prev_nogap);
	printf("nogap at r %d, c %d: matrix3[c][r].Fgap %d vs c_gap %d\n",r,c,matrix3[c][r].Fgap,c_gap);
	printf("nogap at r %d, c %d: matrix3[c][r].Egap %d vs r_gap[r] %d\n",r,c,matrix3[c][r].Egap,r_gap[r]);
#endif
#ifdef DEBUG12
	assert(matrix3[c-1][r-1].nogap == prev_nogap);
	assert(matrix3[c][r].Fgap == c_gap);
	assert(matrix3[c][r].Egap == r_gap[r]);
#endif
	last_nogap = prev_nogap + pairscore;
	/* bestdir2 = DIAG; -- Already initialized to DIAG */
	/* debug3(printf("assign nogap at r %d, c %d: H + pairscore %d vs r_horiz + extend %d vs vert + extend %d\n",
	   r,c,last_nogap,r_gap[r],c_gap)); */
	if (upperp == true && r_gap[r] > last_nogap) {  /* Use > for jump early */
	  last_nogap = r_gap[r];
	  (*directions_nogap)[c][r] = HORIZ;
	}
	if (lowerp == true && c_gap > last_nogap) {  /* Use > for jump early */
	  last_nogap = c_gap;
	  (*directions_nogap)[c][r] = VERT;
	}
	/* (*directions_nogap)[c][r] = bestdir2; */

	prev_nogap = nogap[r];	/* Save for next inner loop, before we wipe it out */
	matrix[c][r] = nogap[r] =
	  (last_nogap < saturation) ? saturation : last_nogap;	/* Save for next outer loop */
	if (r == rlo) {
	  debug12a(printf("At row %d, storing first_nogap to be nogap[r] %d\n",r,nogap[r]));
	  first_nogap = last_nogap;
	}
      }
      debug12a(printf("\n"));
    }
  }

  debug2(printf("STD: Dynprog_standard\n"));
  debug2(Dynprog_Matrix32_print(matrix,rlength,glength,rsequence,gsequence,gsequence_alt,
				goffset,chroffset,chrhigh,watsonp,revp,lband,uband));
  debug2(Dynprog_Directions32_print(*directions_nogap,*directions_Egap,*directions_Fgap,
				    rlength,glength,rsequence,gsequence,gsequence_alt,
				    goffset,chroffset,chrhigh,watsonp,revp,lband,uband));
  debug12a(Matrix3_print(matrix3,rlength,glength,rsequence,gsequence,gsequence_alt,
			 goffset,chroffset,chrhigh,watsonp,revp));

  FREEA(r_gap);
  FREEA(nogap);

  return matrix;
}
#endif



#define LAZY_INDEL 1		/* Don't advance to next coordinate on final indel, since could go over chromosome bounds. */

/* Identical to Dynprog_traceback_8 and Dynprog_traceback_16, except for types of directions matrices */

List_T
Dynprog_traceback_std (List_T pairs, int *traceback_score, int *nmatches, int *nmismatches, int *nopens, int *nindels,
		       Direction32_T **directions_nogap, Direction32_T **directions_Egap, Direction32_T **directions_Fgap,
		       int r, int c, char *rsequence, char *rsequenceuc, char *gsequence, char *gsequence_alt,
		       int queryoffset, int genomeoffset, Genome_T genome, Genome_T genomealt, Pairpool_T pairpool, bool revp,
		       Univcoord_T chroffset, Univcoord_T chrhigh, bool watsonp, int genestrand, int dynprogindex) {
  char c1, c1_uc, c2, c2_alt;
  int dist;
  bool add_dashes_p;
  int querycoord, genomecoord;
  Direction32_T dir;
#ifdef DEBUG_SIMD
  char c2_single;
#endif

  debug(printf("Starting traceback at r=%d,c=%d (roffset=%d, goffset=%d)\n",r,c,queryoffset,genomeoffset));

  while (r > 0 && c > 0) {  /* dir != STOP */
    if ((dir = directions_nogap[c][r]) == HORIZ) {
      dist = 1;
      while (c > 0 && directions_Egap[c--][r] != DIAG) {
	dist++;
      }
#if 0
      if (c == 0) {
	/* Directions in column 0 can sometimes be DIAG */
	dir = VERT;
      } else {
	dir = directions_nogap[c][r];
      }
#endif

      debug(printf("H%d: ",dist));
      pairs = Pairpool_add_genomeskip(&add_dashes_p,pairs,r,c+dist,dist,/*genomesequence*/NULL,
				      queryoffset,genomeoffset,genome,genomealt,
				      pairpool,revp,chroffset,chrhigh,watsonp,dynprogindex);
      if (add_dashes_p == true) {
	*traceback_score += TOPEN + dist*TINDEL;
	*nopens += 1;
	*nindels += dist;
      }
      debug(printf("\n"));

    } else if (dir == VERT) {
      dist = 1;
      while (r > 0 && directions_Fgap[c][r--] != DIAG) {
	dist++;
      }
#if 0
      if (r == 0) {
	/* Directions in row 0 can sometimes be DIAG */
	dir = HORIZ;
      } else {
	dir = directions_nogap[c][r];
      }
#endif

      debug(printf("V%d: ",dist));
      pairs = Pairpool_add_queryskip(pairs,r+dist,c,dist,rsequence,
				     queryoffset,genomeoffset,pairpool,revp,
				     dynprogindex);
      *traceback_score += QOPEN + dist*QINDEL;
      *nopens += 1;
      *nindels += dist;
      debug(printf("\n"));

    } else {
      querycoord = r-1;
      genomecoord = c-1;
      if (revp == true) {
	querycoord = -querycoord;
	genomecoord = -genomecoord;
      }

      c1 = rsequence[querycoord];
      c1_uc = rsequenceuc[querycoord];
      c2 = gsequence[genomecoord];
      c2_alt = gsequence_alt[genomecoord];
#ifdef DEBUG_SIMD
      c2_single = get_genomic_nt(&c2_alt,genome,genomealt,genomeoffset+genomecoord,chroffset,chrhigh,watsonp);
      if (c2 != c2_single) {
	abort();
      }
#endif

#ifdef EXTRACT_GENOMICSEG
      assert(c2 == genomesequence[genomecoord]);
#endif

      if (c2 == '*') {
	/* Don't push pairs past end of chromosome */
	debug(printf("Don't push pairs past end of chromosome: genomeoffset %u, genomecoord %u, chroffset %u, chrhigh %u, watsonp %d\n",
		     genomeoffset,genomecoord,chroffset,chrhigh,watsonp));

      } else if (/*querysequenceuc[querycoord]*/c1_uc == c2 || c1_uc == c2_alt) {
	debug(printf("Pushing %d,%d [%d,%d] (%c,%c) - match\n",
		     r,c,queryoffset+querycoord,genomeoffset+genomecoord,c1_uc,c2));
	*traceback_score += MATCH;
	*nmatches += 1;
	pairs = Pairpool_push(pairs,pairpool,queryoffset+querycoord,genomeoffset+genomecoord,
			      c1,DYNPROG_MATCH_COMP,c2,c2_alt,dynprogindex);
	
      } else if (Dynprog_consistent_p(c1_uc,/*g*/c2,/*g_alt*/c2_alt,genestrand) == true) {
	debug(printf("Pushing %d,%d [%d,%d] (%c,%c) - ambiguous\n",
		     r,c,queryoffset+querycoord,genomeoffset+genomecoord,c1_uc,c2));
	*traceback_score += MATCH;
	*nmatches += 1;
	pairs = Pairpool_push(pairs,pairpool,queryoffset+querycoord,genomeoffset+genomecoord,
			      c1,AMBIGUOUS_COMP,c2,c2_alt,dynprogindex);

      } else {
	debug(printf("Pushing %d,%d [%d,%d] (%c,%c) - mismatch\n",
		     r,c,queryoffset+querycoord,genomeoffset+genomecoord,c1_uc,c2));
	*traceback_score += MISMATCH;
	*nmismatches += 1;
	pairs = Pairpool_push(pairs,pairpool,queryoffset+querycoord,genomeoffset+genomecoord,
			      c1,MISMATCH_COMP,c2,c2_alt,dynprogindex);
      }

      r--; c--;
    }
  }

  if (r == 0 && c == 0) {
    /* Finished with a diagonal step */

  } else if (c == 0) {
    dist = r;
    debug(printf("V%d: ",dist));
    pairs = Pairpool_add_queryskip(pairs,r,/*c*/0+LAZY_INDEL,dist,rsequence,
				   queryoffset,genomeoffset,pairpool,revp,
				   dynprogindex);
    *traceback_score += QOPEN + dist*QINDEL;
    *nopens += 1;
    *nindels += dist;
    debug(printf("\n"));

  } else {
    assert(r == 0);
    dist = c;
    debug(printf("H%d: ",dist));
    pairs = Pairpool_add_genomeskip(&add_dashes_p,pairs,/*r*/0+LAZY_INDEL,c,dist,/*genomesequence*/NULL,
				    queryoffset,genomeoffset,genome,genomealt,
				    pairpool,revp,chroffset,chrhigh,watsonp,dynprogindex);
    if (add_dashes_p == true) {
      *traceback_score += TOPEN + dist*TINDEL;
      *nopens += 1;
      *nindels += dist;
    }
    debug(printf("\n"));
  }

  return pairs;
}


