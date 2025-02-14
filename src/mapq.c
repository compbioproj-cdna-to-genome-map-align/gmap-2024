static char rcsid[] = "$Id: mapq.c 226345 2023-03-04 14:44:29Z twu $";
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mapq.h"
#include "bool.h"
#include "assert.h"
#include "mem.h"
#include "genomebits_mismatches.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>		/* For islower() */
#include <math.h>


static int quality_score_adj = 33; /* Default is Sanger */


#ifdef DEBUG
#define debug(x) x
#else
#define debug(x)
#endif


void
MAPQ_init (int quality_score_adj_in) {
  quality_score_adj = quality_score_adj_in;
  return;
}


/* Duplicated in pair.c */
static float
mismatch_logprob[MAX_QUALITY_SCORE+1] =
  /* log(1/3*10^(-Q/10)) */
  {-1.098612,
   -1.328871, -1.559129, -1.789388, -2.019646, -2.249905,
   -2.480163, -2.710422, -2.940680, -3.170939, -3.401197,
   -3.631456, -3.861714, -4.091973, -4.322231, -4.552490,
   -4.782748, -5.013007, -5.243265, -5.473524, -5.703782,
   -5.934041, -6.164299, -6.394558, -6.624817, -6.855075,
   -7.085334, -7.315592, -7.545851, -7.776109, -8.006368,
   -8.236626, -8.466885, -8.697143, -8.927402, -9.157660,
   -9.387919, -9.618177, -9.848436, -10.078694, -10.308953};

#ifdef USE_MATCHES
static float
match_logprob[MAX_QUALITY_SCORE+1] = 
  /* log(1 - 10^(-Q/10)) */
  {/*-Inf*/-1.58,
   -1.5814737534, -0.9968430440, -0.6955244713, -0.5076758737, -0.3801304081,
   -0.2892681872, -0.2225515160, -0.1725565729, -0.1345519603, -0.1053605157,
   -0.0827653027, -0.0651741732, -0.0514182742, -0.0406248442, -0.0321335740,
   -0.0254397275, -0.0201543648, -0.0159758692, -0.0126691702, -0.0100503359,
   -0.0079749983, -0.0063295629, -0.0050244739, -0.0039890173, -0.0031672882,
   -0.0025150465, -0.0019972555, -0.0015861505, -0.0012597185, -0.0010005003,
   -0.0007946439, -0.0006311565, -0.0005013129, -0.0003981864, -0.0003162778,
   -0.0002512202, -0.0001995461, -0.0001585019, -0.0001259005, -0.0001000050};
#endif


int
MAPQ_max_quality_score (char *quality_string, int querylength) {
  int max_quality_score = 1;
  int Q;
  int querypos;

  if (quality_string == NULL) {
    return MAX_QUALITY_SCORE;
  } else {
    for (querypos = 0; querypos < querylength; querypos++) {
      Q = quality_string[querypos] - quality_score_adj;
      if (Q < 0) {
	fprintf(stderr,"Warning: quality score %c (ASCII %d) - %d (quality-zero-score) = %d, which is less than 0.  May need to specify --quality-protocol=sanger or set --quality-zero-score\n",
		quality_string[querypos],(int) quality_string[querypos],quality_score_adj, Q);
	fprintf(stderr,"Position %d/%d of %s\n",querypos,querylength,quality_string);
	Q = 0;
      } else if (Q > MAX_QUALITY_SCORE_INPUT) {
	fprintf(stderr,"Warning: quality score %c (ASCII %d) - %d (quality-zero-score) = %d, which exceeds %d.  May need to specify --quality-protocol or --quality-zero-score\n",
		quality_string[querypos],(int) quality_string[querypos],quality_score_adj,Q,MAX_QUALITY_SCORE_INPUT);
	fprintf(stderr,"Position %d/%d of %s\n",querypos,querylength,quality_string);
	Q = MAX_QUALITY_SCORE;
      } else if (Q > MAX_QUALITY_SCORE) {
	Q = MAX_QUALITY_SCORE;
      }

      if (Q > max_quality_score) {
	max_quality_score = Q;
      }
    }
    return max_quality_score;
  }
}


#if 0
float
MAPQ_loglik_exact (char *quality_string, int querystart, int queryend) {

#ifdef USE_MATCHES
  float loglik = 0.0;
  int Q;
  int querypos;

  if (quality_string == NULL) {
    for (querypos = querystart; querypos < queryend; querypos++) {
      loglik += match_logprob[MAX_QUALITY_SCORE];
    }
  } else {
    for (querypos = querystart; querypos < queryend; querypos++) {
      Q = quality_string[querypos] - quality_score_adj;
      if (Q < 0) {
	fprintf(stderr,"Warning: quality score %c (ASCII %d) - %d (quality-zero-score) = %d, which is less than 0.  May need to specify --quality-protocol=sanger or set --quality-zero-score\n",
		quality_string[querypos],(int) quality_string[querypos],quality_score_adj, Q);
	fprintf(stderr,"Position %d/%d of %s\n",querypos,querylength,quality_string);
	Q = 0;
      } else if (Q > MAX_QUALITY_SCORE_INPUT) {
	fprintf(stderr,"Warning: quality score %c (ASCII %d) - %d (quality-zero-score) = %d, which exceeds %d.  May need to specify --quality-protocol or --quality-zero-score\n",
		quality_string[querypos],(int) quality_string[querypos],quality_score_adj,Q,MAX_QUALITY_SCORE_INPUT);
	fprintf(stderr,"Position %d/%d of %s\n",querypos,querylength,quality_string);
	Q = MAX_QUALITY_SCORE;
      } else if (Q > MAX_QUALITY_SCORE) {
	Q = MAX_QUALITY_SCORE;
      }

      loglik += match_logprob[Q];
    }
  }
  return loglik;
#else
  debug(printf("mapq exact loglik = 0.0\n"));
  return 0.0;
#endif

}
#endif


#if 0
static bool
check_badchar (char c) {
  if (c == 'A' || c == 'a' || c == 'C' || c == 'c' || c == 'G' || c == 'g' || c == 'T' || c == 't' || c == 'N' || c == 'n') {
    return false;
  } else {
    return true;
  }
}
#endif


float
MAPQ_loglik_string (char *genomic_diff, char *quality_string, int querylength, bool plusp) {
  float loglik = 0.0;
  int Q;
  int querypos, qpos;

  if (plusp == true) {
    for (querypos = 0; querypos < querylength; querypos++) {
      if (islower(genomic_diff[querypos])) {
	Q = (quality_string == NULL) ? MAX_QUALITY_SCORE : quality_string[querypos] - quality_score_adj;
	if (Q < 0) {
	  fprintf(stderr,"Warning: quality score %c (ASCII %d) - %d (quality-zero-score) = %d, which is less than 0.  May need to specify --quality-protocol or --quality-zero-score\n",
		  quality_string[querypos],(int) quality_string[querypos],quality_score_adj, Q);
	  fprintf(stderr,"Position %d of %s\n",querypos,quality_string);
	  Q = 0;
	} else if (Q > MAX_QUALITY_SCORE_INPUT) {
	  fprintf(stderr,"Warning: quality score %c (ASCII %d) - %d (quality-zero-score) = %d, which exceeds %d.  May need to specify --quality-protocol or --quality-zero-score\n",
		  quality_string[querypos],(int) quality_string[querypos],quality_score_adj,Q,MAX_QUALITY_SCORE_INPUT);
	  fprintf(stderr,"Position %d of %s\n",querypos,quality_string);
	  Q = MAX_QUALITY_SCORE;
	} else if (Q > MAX_QUALITY_SCORE) {
	  Q = MAX_QUALITY_SCORE;
	}

	loglik += mismatch_logprob[Q];
      }
    }

  } else {
    for (qpos = 0; qpos < querylength; qpos++) {
      if (islower(genomic_diff[qpos])) {
	querypos = (querylength - 1) - qpos;
	Q = (quality_string == NULL) ? MAX_QUALITY_SCORE : quality_string[querypos] - quality_score_adj;
	if (Q < 0) {
	  fprintf(stderr,"Warning: quality score %c (ASCII %d) - %d (quality-zero-score) = %d, which is less than 0.  May need to specify --quality-protocol or --quality-zero-score\n",
		  quality_string[querypos],(int) quality_string[querypos],quality_score_adj, Q);
	  fprintf(stderr,"Position %d of %s\n",querypos,quality_string);
	  Q = 0;
	} else if (Q > MAX_QUALITY_SCORE_INPUT) {
	  fprintf(stderr,"Warning: quality score %c (ASCII %d) - %d (quality-zero-score) = %d, which exceeds %d.  May need to specify --quality-protocol or --quality-zero-score\n",
		  quality_string[querypos],(int) quality_string[querypos],quality_score_adj,Q,MAX_QUALITY_SCORE_INPUT);
	  fprintf(stderr,"Position %d of %s\n",querypos,quality_string);
	  Q = MAX_QUALITY_SCORE;
	} else if (Q > MAX_QUALITY_SCORE) {
	  Q = MAX_QUALITY_SCORE;
	}
	
	loglik += mismatch_logprob[Q];
      }
    }
  }

  debug(printf("returning loglik %f\n",loglik));
  return loglik;
}

