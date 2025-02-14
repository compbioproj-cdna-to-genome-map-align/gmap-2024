static char rcsid[] = "$Id: stage2.c 225436 2022-12-07 01:34:49Z twu $";
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "stage2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "assert.h"
#include "mem.h"
#include "comp.h"
#include "pair.h"
#include "pairdef.h"
#include "intlist.h"
#include "diag.h"
#include "genome_canonical.h"
#include "complement.h"
#include "maxent_hr.h"


/* Tests whether genomicseg == query in convert_to_nucleotides, and
   whether oligoindex_hr gives same results as oligoindex */
/* #define EXTRACT_GENOMICSEG 1 */

/* #define USE_DIAGPOOL 1 -- Defined in diagpool.h */

/* #define SQUARE 1 */

/* #define SLOW 1 */

#define MIN_TERMINAL_NCONSECUTIVE 8 /* Number of consecutive matches at the end of the path */

#define SUFF_PCTCOVERAGE_OLIGOINDEX 0.90

/* #define SUFF_PCTCOVERAGE_STAGE2 0.10 */
#define SUFF_NCOVERED 200
#define SUFF_MAXNCONSECUTIVE 20
#define GREEDY_NCONSECUTIVE 100

#define MAX_NACTIVE 100	/* 100 previously considered too low, but may
			   be okay in conjunction with
			   diagonalization */
#define MAX_GRAND_LOOKBACK 200

/* Penalty for genomic distances */

#define INTRON_PENALTY_UNKNOWN 8
#define INTRON_PENALTY_INCONSISTENT 16

/* Needs to be high to avoid short exons, but needs to be low to identify short exons. */
/* On an example by Nathan Weeks, needed to set this value to be 1 or
   0 to find a short exon.  Setting to 0 gives too many short exons.
   Also found querydist_credit to be a bad idea. */
#define NINTRON_PENALTY_MISMATCH 1
/* #define USE_QUERYDIST_CREDIT 1 */

#define NON_CANONICAL_PENALTY_ENDS 4
#define NON_CANONICAL_PENALTY_MIDDLE 4
#define MISS_BEHIND 16
#define GREEDY_ADVANCE 6
#define FINAL_SCORE_TOLERANCE 20 /* Was 8, but missed some paths on Y chromosome */
#define NONOVERLAPPING_SCORE_TOLERANCE 0.5

#define ENOUGH_CONSECUTIVE 32

#define INFINITE 1000000

/* EQUAL_DISTANCE used to be 3 for PMAP and 6 for GMAP, but that
   allowed indels in repetitive regions.  Now have separate
   variables. */
#ifdef PMAP
#define EQUAL_DISTANCE_FOR_CONSECUTIVE 0
#define EQUAL_DISTANCE_NOT_SPLICING 3
#else
#define EQUAL_DISTANCE_FOR_CONSECUTIVE 0
#define EQUAL_DISTANCE_NOT_SPLICING 9
#endif


#define INTRON_DEFN 9		/* Cannot exceed 9 */
#define NEAR_END_LENGTH 20	/* Determines whether to ignore EXON_DEFN at ends */
#define EXON_DEFN 30
#define MAX_SKIPPED 3

#define SCORE_FOR_RESTRICT 10
/* #define SUFFICIENT_ROOTNLINKS 10  */ /* Setting this too low can slow down program considerably */


#ifdef PMAP
#define SAMPLE_INTERVAL 1
#define NT_PER_MATCH 3
#define CONSEC_POINTS_PER_MATCH 3 /* Possible increase to reward consecutiveness */
#define NONCODON_INDEL_PENALTY 15
#else
#define SAMPLE_INTERVAL 2	/* For cases where adjacentp == false.
				   Means that we can find islands of
				   9-mers */
#define NT_PER_MATCH 1
#define NT_PER_CODON 3
#define CONSEC_POINTS_PER_MATCH 1 /* Possible increase to reward consecutiveness */
#define CONSEC_POINTS_PER_CODON 3 /* Possible increase to reward consecutiveness */
#endif

#define SHIFT_EXTRA 15

#define ONE 1
#define TEN_THOUSAND 8192	/* Power of 2 */
#define HUNDRED_THOUSAND 100000.0
#define ONE_MILLION 1000000.0



static bool splicingp;
static bool use_canonical_middle_p;
static bool use_canonical_ends_p;
static int suboptimal_score_end;
static int suboptimal_score_start;
static Mode_T mode;
static bool snps_p;
static int sufflookback;
static int nsufflookback;
static int maxintronlen;


void
Stage2_setup (bool splicingp_in, bool cross_species_p,
	      int suboptimal_score_start_in, int suboptimal_score_end_in,
	      int sufflookback_in, int nsufflookback_in, int maxintronlen_in,
	      Mode_T mode_in, bool snps_p_in) {
  splicingp = splicingp_in;
  if (splicingp == true) {
    use_canonical_ends_p = true;
  } else {
    use_canonical_ends_p = false;
  }
  if (cross_species_p == true) {
    use_canonical_middle_p = true;
  } else {
    use_canonical_middle_p = false;
  }
  suboptimal_score_start = suboptimal_score_start_in;
  suboptimal_score_end = suboptimal_score_end_in;

  sufflookback = sufflookback_in;
  nsufflookback = nsufflookback_in;
  maxintronlen = maxintronlen_in;

  mode = mode_in;
  snps_p = snps_p_in;
  return;
}


/* General */
#ifdef DEBUG
#define debug(x) x
#else 
#define debug(x)
#endif

/* Final results of stage 2 */
#ifdef DEBUG0
#define debug0(x) x
#else 
#define debug0(x)
#endif

/* Print all links */
#ifdef DEBUG1
#define debug1(x) x
#else 
#define debug1(x)
#endif

/* For generating a graph */
#ifdef DEBUG3
#define debug3(x) x
#else 
#define debug3(x)
#endif

/* Converting to nucleotides */
#ifdef DEBUG5
#define debug5(x) x
#else
#define debug5(x)
#endif

/* revise_active */
#ifdef DEBUG6
#define debug6(x) x
#else 
#define debug6(x)
#endif

/* Shifted canonical */
#ifdef DEBUG7
#define debug7(x) x
#else 
#define debug7(x)
#endif

/* find_canonical_dinucleotides */
#ifdef DEBUG8
#define debug8(x) x
#else 
#define debug8(x)
#endif

/* Dynamic programming */
/* Can also define debug9(x) as: if (curr_querypos == XX) {x;} */
#ifdef DEBUG9
#define debug9(x) x
#else 
#define debug9(x)
#endif

/* binary search */
#ifdef DEBUG10
#define debug10(x) x
#else 
#define debug10(x)
#endif

/* Multiple alignments */
#ifdef DEBUG11
#define debug11(x) x
#else 
#define debug11(x)
#endif

/* Grand winner */
#ifdef DEBUG12
#define debug12(x) x
#else 
#define debug12(x)
#endif


/* Filter unique */
#ifdef DEBUG13
#define debug13(x) x
#else 
#define debug13(x)
#endif

/* Filter unique, details of overlap */
#ifdef DEBUG13A
#define debug13a(x) x
#else 
#define debug13a(x)
#endif


struct Stage2_alloc_T {
  int max_querylength_alloc;

  bool *coveredp;
  Chrpos_T **mappings;
  int *npositions;
  unsigned int *minactive;
  unsigned int *maxactive;
  int *firstactive;
  int *nactive;
};

void
Stage2_alloc_free (Stage2_alloc_T *old) {
  FREE((*old)->firstactive);
  FREE((*old)->nactive);
  FREE((*old)->maxactive);
  FREE((*old)->minactive);
  FREE((*old)->npositions);
  FREE((*old)->mappings);
  FREE((*old)->coveredp);
  FREE(*old);
  return;
}

Stage2_alloc_T
Stage2_alloc_new (int max_querylength_alloc) {
  Stage2_alloc_T new = (Stage2_alloc_T) MALLOC(sizeof(*new));

  new->max_querylength_alloc = max_querylength_alloc;

  new->coveredp = (bool *) MALLOC(max_querylength_alloc * sizeof(bool));
  new->mappings = (Chrpos_T **) MALLOC(max_querylength_alloc * sizeof(Chrpos_T *));
  new->npositions = (int *) MALLOC(max_querylength_alloc * sizeof(int));
  new->minactive = (unsigned int *) MALLOC(max_querylength_alloc * sizeof(unsigned int));
  new->maxactive = (unsigned int *) MALLOC(max_querylength_alloc * sizeof(unsigned int));
  new->firstactive = (int *) MALLOC(max_querylength_alloc * sizeof(int));
  new->nactive = (int *) MALLOC(max_querylength_alloc * sizeof(int));

  return new;
}


#define T Stage2_T
struct T {
  List_T middle;
  List_T all_starts;
  List_T all_ends;
};


void
Stage2_free (T *old) {
  /* List_free(&(*old)->middle); -- Not necessary because of pairpool */
  /* List_free(&(*old)->all_starts); -- Handled by Stage3middle_free */
  /* List_free(&(*old)->all_ends); -- Handled by Stage3middle_free */
  FREE(*old);
  return;
}

static T
Stage2_new (List_T middle, List_T all_starts, List_T all_ends) {
  T new = (T) MALLOC(sizeof(*new));
#ifdef DEBUG0
  List_T p;
#endif

  new->middle = middle;
  new->all_starts = all_starts;
  new->all_ends = all_ends;

#ifdef DEBUG0
  printf("Starts:\n");
  for (p = all_starts; p != NULL; p = List_next(p)) {
    Pair_dump_list(List_head(p),true);
  }

  printf("Ends:\n");
  for (p = all_ends; p != NULL; p = List_next(p)) {
    Pair_dump_list(List_head(p),true);
  }
#endif

  return new;
}

List_T
Stage2_middle (T this) {
  return this->middle;
}

List_T
Stage2_all_starts (T this) {
  return this->all_starts;
}

List_T
Stage2_all_ends (T this) {
  return this->all_ends;
}


/************************************************************************/

typedef struct Link_T *Link_T;
struct Link_T {
  int fwd_consecutive;
  int fwd_rootposition;
  /*int fwd_rootnlinks;*/		/* Number of links in last branch */
  /* int fwd_score; */                  /* Kept as a separate structure */

  int fwd_pos;
  int fwd_hit;
  int fwd_tracei;		/* Corresponds to a distinct set of branches */

#ifdef DEBUG9
  int fwd_intronnfwd;
  int fwd_intronnrev;
  int fwd_intronnunk;
#endif

#ifdef SEPARATE_FWD_REV
  /* No longer checking separate fwd/rev directions */
  int rev_consecutive;
  int rev_rootposition;
  /*int rev_rootnlinks;*/		/* Number of links in last branch */
  int rev_score;

  int rev_pos;
  int rev_hit;

#ifdef DEBUG9
  int rev_tracei;		/* Corresponds to a distinct set of branches */
  int rev_intronnfwd;
  int rev_intronnrev;
  int rev_intronnunk;
#endif	/* rev */

#endif
};


/* lengths2 is has length1 entries.  Note that lengths2 may have
   negative entries */
static struct Link_T **
Linkmatrix_1d_new (int length1, int *lengths2, int totallength) {
  struct Link_T **links;
  int i;
  
  /* Outer dimension can be MALLOC, but inner one must be CALLOC */
  links = (struct Link_T **) MALLOC(length1 * sizeof(struct Link_T *));
  links[0] = (struct Link_T *) CALLOC(totallength,sizeof(struct Link_T));
  for (i = 1; i < length1; i++) {
    if (lengths2[i-1] < 0) {
      links[i] = links[i-1];
    } else {
      links[i] = &(links[i-1][lengths2[i-1]]);
    }
  }
  return links;
}

static void
Linkmatrix_1d_free (struct Link_T ***links) {
  FREE((*links)[0]);
  FREE(*links);
  return;
}


static struct Link_T **
Linkmatrix_2d_new (int length1, int *lengths2) {
  struct Link_T **links;
  int i;
  
  links = (struct Link_T **) CALLOC(length1,sizeof(struct Link_T *));
  for (i = 0; i < length1; i++) {
    if (lengths2[i] <= 0) {
      links[i] = (struct Link_T *) NULL;
    } else {
      links[i] = (struct Link_T *) CALLOC(lengths2[i],sizeof(struct Link_T));
    }
  }
  return links;
}

static void
Linkmatrix_2d_free (struct Link_T ***links, int length1) {
  int i;

  for (i = 0; i < length1; i++) {
    if ((*links)[i]) {
      FREE((*links)[i]);
    }
  }
  FREE(*links);
  return;
}



#ifdef DEBUG1
#ifdef SEPARATE_FWD_REV
static void
Linkmatrix_print_both (struct Link_T **links, Chrpos_T **mappings, int length1,
		       int *npositions, char *queryseq_ptr, int indexsize) {
  int i, j;
  char *oligo;

  oligo = (char *) MALLOCA((indexsize+1) * sizeof(char));
  for (i = 0; i <= length1-indexsize; i++) {
    strncpy(oligo,&(queryseq_ptr[i]),indexsize);
    oligo[indexsize] = '\0';

    printf("Querypos %d (%s, %d positions):",i,oligo,npositions[i]);
    for (j = 0; j < npositions[i]; j++) {
      printf(" %d.%u:%d(%d,%d)[%u]-%d(%d,%d)[%u]",
	     j,mappings[i][j],links[i][j].fwd_score,
	     links[i][j].fwd_pos,links[i][j].fwd_hit,links[i][j].fwd_tracei,
	     links[i][j].rev_score,
	     links[i][j].rev_pos,links[i][j].rev_hit,links[i][j].rev_tracei);
    }
    printf("\n");
  }
  printf("\n");

  FREEA(oligo);

  return;
}

#else

/* For PMAP, indexsize is in aa */
static void
print_fwd (struct Link_T **links, int **fwd_scores,
	   Chrpos_T **mappings, int length1,
	   int *npositions, char *queryseq_ptr, int indexsize) {
  int i, j, lastpos;
  char *oligo;

  oligo = (char *) MALLOCA((indexsize+1) * sizeof(char));
  lastpos = length1 - indexsize;

  for (i = 0; i <= lastpos; i++) {
    strncpy(oligo,&(queryseq_ptr[i]),indexsize);
    oligo[indexsize] = '\0';

    printf("Querypos %d (%s, %d positions):",i,oligo,npositions[i]);
    for (j = 0; j < npositions[i]; j++) {
      printf(" %d.%u:%d(%d,%d)[%u]",
	     j,mappings[i][j],fwd_scores[i][j],
	     links[i][j].fwd_pos,links[i][j].fwd_hit,links[i][j].fwd_tracei);
    }
    printf("\n");
  }
  printf("\n");

  FREEA(oligo);

  return;
}

#endif
#endif

#if 0
static void
mappings_dump_R (Chrpos_T **mappings, int *npositions, int length1,
		 int **active, int *firstactive, int indexsize, char *varname) {
  int querypos;
  int lastpos, hit;
  bool printp = false;

  lastpos = length1 - indexsize;
  printf("%s <- matrix(c(\n",varname);
  for (querypos = 0; querypos < lastpos; querypos++) {
    if (firstactive) {
      if (mappings[querypos] != NULL) {
	hit = firstactive[querypos];
	while (hit != -1) {
	  /* Last elt is for score */
	  if (printp == false) {
	    printp = true;
	  } else {
	    printf(",\n");
	  }
	  printf("%d,%d,%d,%d",querypos,mappings[querypos][hit],
		 hit,active[querypos][hit]);
	  hit = active[querypos][hit];
	}
      }
    } else {
      for (hit = 0; hit < npositions[querypos]; hit++) {
	if (printp == false) {
	  printp = true;
	} else {
	  printf(",\n");
	}
	printf("%d,%d,%d",querypos,mappings[querypos][hit],hit);
      }
    }
  }
  printf("),ncol=2,byrow=T)\n");

  return;
}
#endif    

#if 0
static void
best_path_dump_R (struct Link_T **links, Chrpos_T **mappings,
		  int querypos, int hit, bool fwdp, char *varname) {
  Chrpos_T position;
  int prev_querypos, prevhit, save_querypos, savehit;
  bool printp = false;

  save_querypos = querypos;
  savehit = hit;

  printf("%s <- matrix(c(\n",varname);
  prev_querypos = querypos+1;
  while (querypos >= 0) {
    position = mappings[querypos][hit];

    if (printp == false) {
      printp = true;
    } else {
      printf(",\n");
    }
    printf("%d,%d",querypos,position);

    prev_querypos = querypos;
    prevhit = hit;
    if (fwdp) {
      querypos = links[prev_querypos][prevhit].fwd_pos;
      hit = links[prev_querypos][prevhit].fwd_hit;
#ifdef SEPARATE_FWD_REV
    } else {
      querypos = links[prev_querypos][prevhit].rev_pos;
      hit = links[prev_querypos][prevhit].rev_hit;
#endif
    }
  }
  printf("),ncol=2,byrow=T)\n");

  querypos = save_querypos;
  hit = savehit;

  printp = false;
  printf("%s <- matrix(c(\n","scores");
  prev_querypos = querypos+1;
  while (querypos >= 0) {
    position = mappings[querypos][hit];

    if (printp == false) {
      printp = true;
    } else {
      printf(",\n");
    }
    if (fwdp == true) {
      printf("%d,%d",querypos,links[querypos][hit].fwd_score);
#ifdef SEPARATE_FWD_REV
    } else {
      printf("%d,%d",querypos,links[querypos][hit].rev_score);
#endif
    }

    prev_querypos = querypos;
    prevhit = hit;
    if (fwdp) {
      querypos = links[prev_querypos][prevhit].fwd_pos;
      hit = links[prev_querypos][prevhit].fwd_hit;
#ifdef SEPARATE_FWD_REV
    } else {
      querypos = links[prev_querypos][prevhit].rev_pos;
      hit = links[prev_querypos][prevhit].rev_hit;
#endif
    }
  }
  printf("),ncol=2,byrow=T)\n");

  return;
}
#endif

static void
active_bounds_dump_R (Chrpos_T *minactive, Chrpos_T *maxactive,
		      int querylength) {
  int querypos;
  bool printp = false;

  printf("querypos <- 0:%d\n",querylength-1);
  printf("%s <- c(\n","minactive");
  for (querypos = 0; querypos < querylength; querypos++) {
    if (printp == false) {
      printp = true;
    } else {
      printf(",\n");
    }
    printf("%d",minactive[querypos]);
  }
  printf(")\n");

  printp = false;
  printf("%s <- c(\n","maxactive");
  for (querypos = 0; querypos < querylength; querypos++) {
    if (printp == false) {
      printp = true;
    } else {
      printf(",\n");
    }
    printf("%d",maxactive[querypos]);
  }
  printf(")\n");

  return;
}


#ifdef PMAP
#define QUERYDIST_PENALTY_FACTOR 2
#else
#define QUERYDIST_PENALTY_FACTOR 8
#endif


/************************************************************************
 *  Procedures for finding canonical introns quickly
 ************************************************************************/

#ifdef DEBUG8

static void
print_last_dinucl (int *last_dinucl, int genomiclength) {
  int pos;

  for (pos = 0; pos < genomiclength - 3 + SHIFT_EXTRA; pos++) {
    printf("%d %d\n",pos,last_dinucl[pos]);
  }
  printf("\n");

  return;
}

#endif


#if 0
/* Need this procedure because we are skipping some oligomers */
static bool
find_shifted_canonical (Chrpos_T leftpos, Chrpos_T rightpos, int querydistance, 
			Chrpos_T (*genome_left_position)(Chrpos_T, Chrpos_T, Univcoord_T, Univcoord_T, bool),
			Chrpos_T (*genome_right_position)(Chrpos_T, Chrpos_T, Univcoord_T, Univcoord_T, bool),
			Univcoord_T chroffset, Univcoord_T chrhigh, bool plusp, bool skip_repetitive_p) {
  Chrpos_T leftdi, rightdi;
  Chrpos_T last_leftpos, last_rightpos;
  int shift, leftmiss, rightmiss;
  Chrpos_T left_chrbound, right_chrbound;
  
  /* leftpos = prevposition + querydistance + indexsize_nt - 1; */
  /* rightpos = position; */

  debug7(printf("Looking for shifted canonical at leftpos %u to rightpos %u, chroffset %u, chrhigh %u\n",
		leftpos,rightpos,chroffset,chrhigh));

#if 0
  /* previously checked against genomiclength */
  if (leftpos > genomiclength || rightpos > genomiclength) {
    return false;
  }
#else
  /* Checking just before call to genome_right_position */
#endif

  if (leftpos >= rightpos) {
    debug7(printf("leftpos %u >= rightpos %u, so returning false\n",leftpos,rightpos));
    return false;
  }

  if (leftpos < 103) {
    left_chrbound = 3;	/* Previously 0, but then can find splice site at beginning of segment */
  } else {
    left_chrbound = leftpos - 100;
  }

  if (rightpos < 103) {
    right_chrbound = 3;	/* Previously 0, but then can find splice site at beginning of segment */
  } else {
    right_chrbound = rightpos - 100;
  }

#if 0
  if (skip_repetitive_p == false) {

    last_leftpos = (*genome_left_position)(leftpos,left_chrbound,chroffset,chrhigh,plusp);
    last_rightpos = (*genome_right_position)(rightpos,right_chrbound,chroffset,chrhigh,plusp);
    debug7(printf("last_leftpos %u, last_rightpos %u\n",last_leftpos,last_rightpos));

    debug7(printf("skip_repetitive_p == false, so returning %u == %u && %u == %u\n",
		  leftpos,last_leftpos,rightpos,last_rightpos));
    return (leftpos == last_leftpos && rightpos == last_rightpos);
  }
#endif

  /* Allow canonical to be to right of match */
  leftpos += SHIFT_EXTRA;
  if (leftpos > chrhigh - 3) {
    leftpos = chrhigh - 3;
  }
  rightpos += SHIFT_EXTRA;
  if (rightpos > chrhigh - 3) {
    rightpos = chrhigh - 3;
  }
  debug7(printf("after shift, leftpos = %u, rightpos = %u\n",leftpos,rightpos));

  shift = 0;
  while (shift <= querydistance + SHIFT_EXTRA + SHIFT_EXTRA) {

#if 0
    if (leftpos < 0) {
      return false;
    } else if (rightpos < 0) {
      /* Shouldn't need to check if leftpos >= 0 and rightpos >= leftpos, in the other two conditions) */
      return false;
    } else if (rightpos >= chrlength) {
      return false;
    }
#endif
    if (leftpos < 3) {
      return false;
    } else if (leftpos > rightpos) {
      return false;
    }

    last_leftpos = (*genome_left_position)(leftpos,left_chrbound,chroffset,chrhigh,plusp);
    debug7(printf("last_leftpos %u\n",last_leftpos));
    assert(last_leftpos != 0U);
    if ((leftdi = last_leftpos) == -1) {
      debug7(printf("\n"));
      return false;
    } else {
      leftmiss = (int) (leftpos - leftdi);
    }

    last_rightpos = (*genome_right_position)(rightpos,right_chrbound,chroffset,chrhigh,plusp);
    debug7(printf("last_rightpos %u\n",last_rightpos));
    assert(last_rightpos != 0U);
    if ((rightdi = last_rightpos) == -1) {
      debug7(printf("\n"));
      return false;
    } else {
      rightmiss = (int) (rightpos - rightdi);
    }

    debug7(printf("shift %d/left %d (miss %d)/right %d (miss %d)\n",shift,leftpos,leftmiss,rightpos,rightmiss));
    if (leftmiss == rightmiss) {  /* was leftmiss == 0 && rightmiss == 0, which doesn't allow for a shift */
      debug7(printf(" => Success at %u..%u (fwd) or %u..%u (rev)\n\n",
		    leftpos-leftmiss+/*onebasedp*/1U,rightpos-rightmiss+/*onebasedp*/1U,
		    chrhigh-chroffset-(leftpos-leftmiss),chrhigh-chroffset-(rightpos-rightmiss)));
      return true;
    } else if (leftmiss >= rightmiss) {
      shift += leftmiss;
      leftpos -= leftmiss;
      rightpos -= leftmiss;
    } else {
      shift += rightmiss;
      leftpos -= rightmiss;
      rightpos -= rightmiss;
    }
  }

  debug7(printf("\n"));
  return false;
}
#endif




#if 0
/* General case for ranges in score_querypos */
while (prevhit != -1 && (prevposition = mappings[prev_querypos][prevhit]) + indexsize_nt <= position) {
  /* printf("fwd: prevposition %u, prevhit %d\n",prevposition,prevhit); */
  prevlink = &(links[prev_querypos][prevhit]);
  
  gendistance = position - prevposition - indexsize_nt;
  /* diffdistance = abs(gendistance - querydistance); */
  if (gendistance > querydistance) {
    diffdistance = gendistance - querydistance;
  } else {
    diffdistance = querydistance - gendistance;
  }

  if (diffdistance < maxintronlen) {
    if (diffdistance <= EQUAL_DISTANCE_NOT_SPLICING) {
      debug9(canonicalsgn = 9);
      fwd_score = prevlink->fwd_score + CONSEC_POINTS_PER_MATCH;
#ifdef PMAP
      if (diffdistance % 3 != 0) {
	fwd_score -= NONCODON_INDEL_PENALTY;
      }
#endif
    } else if (near_end_p == false && prevlink->fwd_consecutive < EXON_DEFN) {
      debug9(canonicalsgn = 0);
      if (splicingp == true) {
	fwd_score = prevlink->fwd_score - (diffdistance/TEN_THOUSAND + 1) - querydist_penalty - NINTRON_PENALTY_MISMATCH;
      } else {
	fwd_score = prevlink->fwd_score - (diffdistance/ONE + 1) - querydist_penalty - NINTRON_PENALTY_MISMATCH;
      }

    } else if (splicingp == false) {
      debug9(canonicalsgn = 0);
      fwd_score = prevlink->fwd_score - (diffdistance/ONE + 1) - querydist_penalty;

    } else if (use_shifted_canonical_p == true) {
      leftpos = prevposition + querydistance - 1;
      /* printf("leftpos %d, last_leftpos %d, rightpos %d\n",leftpos,last_leftpos,rightpos); */
      if (leftpos == last_leftpos) {
	canonicalp = last_canonicalp;
      } else {
	debug7(printf("Calling find_shift_canonical fwd\n"));
	canonicalp = find_shifted_canonical(leftpos,rightpos,querydistance-indexsize_nt,
					    /* &lastGT,&lastAG, */
					    Genome_prev_donor_position,Genome_prev_acceptor_position,
					    chroffset,chrhigh,plusp,skip_repetitive_p);
	/* And need to check for shift_canonical_rev */

	last_leftpos = leftpos;
	last_canonicalp = canonicalp;
      }
      if (canonicalp == true) {
	debug9(canonicalsgn = +1);
	fwd_score = prevlink->fwd_score - (diffdistance/TEN_THOUSAND + 1) - querydist_penalty;
      } else {
	debug9(canonicalsgn = 0);
	fwd_score = prevlink->fwd_score - (diffdistance/TEN_THOUSAND + 1) - querydist_penalty - NINTRON_PENALTY_MISMATCH;
      }

    } else {
      debug9(canonicalsgn = +1);
      fwd_score = prevlink->fwd_score - (diffdistance/TEN_THOUSAND + 1) - querydist_penalty;
    }

    debug9(printf("\tD. Fwd mismatch qpos %d,%d at %ux%d (score = %d -> %d, consec = %d (from #%d), intr = %d-%d-%d, gendist %u, querydist %d, canonicalsgn %d)",
		  prev_querypos,prevhit,prevposition,active[prev_querypos][prevhit],
		  prevlink->fwd_score,fwd_score,prevlink->fwd_consecutive,prevlink->fwd_tracei,
		  best_fwd_intronnfwd,best_fwd_intronnrev,best_fwd_intronnunk,
		  gendistance,querydistance,canonicalsgn));
	    
    /* Allow ties, which should favor shorter intron */
    if (fwd_score >= best_fwd_score) {
      if (diffdistance <= EQUAL_DISTANCE_FOR_CONSECUTIVE) {
	best_fwd_consecutive = prevlink->fwd_consecutive + (querydistance + indexsize_nt);
	/* best_fwd_rootnlinks = prevlink->fwd_rootnlinks + 1; */
      } else {
	best_fwd_consecutive = 0;
	/* best_fwd_rootnlinks = 1; */
      }
      best_fwd_score = fwd_score;
      best_fwd_prevpos = prev_querypos;
      best_fwd_prevhit = prevhit;
#ifdef DEBUG9
      best_fwd_tracei = ++*fwd_tracei;
      best_fwd_intronnfwd = prevlink->fwd_intronnfwd;
      best_fwd_intronnrev = prevlink->fwd_intronnrev;
      best_fwd_intronnunk = prevlink->fwd_intronnunk;
      switch (canonicalsgn) {
      case 1: best_fwd_intronnfwd++; break;
      case 0: best_fwd_intronnunk++; break;
      }
#endif
      debug9(printf(" => Best fwd at %d (consec = %d)\n",fwd_score,best_fwd_consecutive));
    } else {
      debug9(printf(" => Loses to %d\n",best_fwd_score));
    }
  }

  prevhit = active[prev_querypos][prevhit];
 }
#endif


#if 0
/* SIMD version */
_positions = _mm_set1_epi32(position - indexsize_nt);
_querydistance = _mm_set1_epi32(querydistance);
_splicing_querydist_penalty = _mm_set1_epi32(querydist_penalty+1+NINTRON_PENALTY_MISMATCH);
_max_scores = _mm_set1_epi32(-1000);

prevhit = low_hit;
while (prevhit + 4 < high_hit) {
  /* printf("fwd: prevposition %u, prevhit %d\n",prevposition,prevhit); */
  _prevpositions = _mm_loadu_epi32(&(mappings[prev_querypos][prevhit]));
  _gendistance = _mm_sub_epi32(_positions,_prevpositions);
  if (_mm_cmpgt_epi32(_gendistance,_zeroes) == 0) {
    break;
  } else {
    _diffdistance = _mm_abs_epi32(_mm_sub_epi32(_gendistance,_querydistance));
  
    _prev_scores = _mm_loadu_epi32(&(fwd_scores[prev_querypos][prevhit]));
  
    _scores_close = _mm_add_epi32(_prev_scores,_mm_set1_epi32(CONSEC_POINTS_PER_MATCH));
    /* Right shift of 13 bits gives division by 8192 */
    _scores_splice = _mm_add_epi32(_prev_scores,_mm_sub_epi32(_mm_srli_epi32(_diffdistance,13),_splicing_querydist_penalty));

    _scores = _mm_blendv_ps(_scores_close,_scores_splice,_mm_cmpgt_epi32(_diffdistance,_mm_set1_epi32(EQUAL_DISTANCE_NOT_SPLICING)));
    
    _mm_storeu_epi32(_scores);

    _max_scores = _mm_max_epi32(_max_scores,_scores);
    prevhit += 4;
  }
}

/* Take care of serial cases */




/* Compute overall max and return.  Caller can find prev_querypos with
   largest max and store in fwd_pos[curr_querypos][currhit] and max in
   fwd_max[curr_querypos][currhit].  During traceback, recompute at
   prev_querypos and find prevhit that gives the max.  */

  if (diffdistance < maxintronlen) {
    if (diffdistance <= EQUAL_DISTANCE_NOT_SPLICING) {
      debug9(canonicalsgn = 9);
      fwd_score = prevlink->fwd_score + CONSEC_POINTS_PER_MATCH;
#ifdef PMAP
      if (diffdistance % 3 != 0) {
	fwd_score -= NONCODON_INDEL_PENALTY;
      }
#endif
    } else if (near_end_p == false && prevlink->fwd_consecutive < EXON_DEFN) {
      debug9(canonicalsgn = 0);
      if (splicingp == true) {
	fwd_score = prevlink->fwd_score - (diffdistance/TEN_THOUSAND + 1) - querydist_penalty - NINTRON_PENALTY_MISMATCH;
      } else {
	fwd_score = prevlink->fwd_score - (diffdistance/ONE + 1) - querydist_penalty - NINTRON_PENALTY_MISMATCH;
      }

    } else if (splicingp == false) {
      debug9(canonicalsgn = 0);
      fwd_score = prevlink->fwd_score - (diffdistance/ONE + 1) - querydist_penalty;

    } else if (use_shifted_canonical_p == true) {
      leftpos = prevposition + querydistance - 1;
      /* printf("leftpos %d, last_leftpos %d, rightpos %d\n",leftpos,last_leftpos,rightpos); */
      if (leftpos == last_leftpos) {
	canonicalp = last_canonicalp;
      } else {
	debug7(printf("Calling find_shift_canonical fwd\n"));
	canonicalp = find_shifted_canonical(leftpos,rightpos,querydistance-indexsize_nt,
					    /* &lastGT,&lastAG, */
					    Genome_prev_donor_position,Genome_prev_acceptor_position,
					    chroffset,chrhigh,plusp,skip_repetitive_p);
	/* And need to check for shift_canonical_rev */

	last_leftpos = leftpos;
	last_canonicalp = canonicalp;
      }
      if (canonicalp == true) {
	debug9(canonicalsgn = +1);
	fwd_score = prevlink->fwd_score - (diffdistance/TEN_THOUSAND + 1) - querydist_penalty;
      } else {
	debug9(canonicalsgn = 0);
	fwd_score = prevlink->fwd_score - (diffdistance/TEN_THOUSAND + 1) - querydist_penalty - NINTRON_PENALTY_MISMATCH;
      }

    } else {
      debug9(canonicalsgn = +1);
      fwd_score = prevlink->fwd_score - (diffdistance/TEN_THOUSAND + 1) - querydist_penalty;
    }

    debug9(printf("\tD. Fwd mismatch qpos %d,%d at %ux%d (score = %d -> %d, consec = %d, intr = %d-%d-%d, gendist %u, querydist %d, canonicalsgn %d)",
		  prev_querypos,prevhit,prevposition,active[prev_querypos][prevhit],
		  prevlink->fwd_score,fwd_score,prevlink->fwd_consecutive,
		  best_fwd_intronnfwd,best_fwd_intronnrev,best_fwd_intronnunk,
		  gendistance,querydistance,canonicalsgn));
	    
    /* Allow ties, which should favor shorter intron */
    if (fwd_score >= best_fwd_score) {
      if (diffdistance <= EQUAL_DISTANCE_FOR_CONSECUTIVE) {
	best_fwd_consecutive = prevlink->fwd_consecutive + (querydistance + indexsize_nt);
	/* best_fwd_rootnlinks = prevlink->fwd_rootnlinks + 1; */
      } else {
	best_fwd_consecutive = 0;
	/* best_fwd_rootnlinks = 1; */
      }
      best_fwd_score = fwd_score;
      best_fwd_prevpos = prev_querypos;
      best_fwd_prevhit = prevhit;
#ifdef DEBUG9
      best_fwd_tracei = ++*fwd_tracei;
      best_fwd_intronnfwd = prevlink->fwd_intronnfwd;
      best_fwd_intronnrev = prevlink->fwd_intronnrev;
      best_fwd_intronnunk = prevlink->fwd_intronnunk;
      switch (canonicalsgn) {
      case 1: best_fwd_intronnfwd++; break;
      case 0: best_fwd_intronnunk++; break;
      }
#endif
      debug9(printf(" => Best fwd at %d (consec = %d)\n",fwd_score,best_fwd_consecutive));
    } else {
      debug9(printf(" => Loses to %d\n",best_fwd_score));
    }
  }

  prevhit = active[prev_querypos][prevhit];
 }
#endif


static void
score_querypos_lookback_one (int *fwd_tracei, Link_T currlink, int curr_querypos, int currhit,
			     unsigned int position,
			     struct Link_T **links, int **fwd_scores, Chrpos_T **mappings,
			     int **active, int *firstactive, Genome_T genome, Genome_T genomealt,
			     Univcoord_T chroffset, Univcoord_T chrhigh, bool plusp,
			     int indexsize, Intlist_T processed,
#ifdef MOVE_TO_STAGE3
			     bool anchoredp,
#endif
			     bool localp, bool splicingp,
			     bool use_canonical_p, int non_canonical_penalty) {
  Link_T prevlink;
  struct Link_T *prev_links;
  Chrpos_T *prev_mappings;
  int *prev_active;

  int best_fwd_consecutive = indexsize*NT_PER_MATCH;
  int best_fwd_rootposition = position;
  /* int best_fwd_rootnlinks = 1; */
  int best_fwd_score = 0, fwd_score;
  int best_fwd_prevpos = -1, best_fwd_prevhit = -1;
  int best_fwd_tracei, last_tracei;
#ifdef DEBUG9
  int best_fwd_intronnfwd = 0, best_fwd_intronnrev = 0, best_fwd_intronnunk = 0;
  int canonicalsgn = 0;
#endif
  bool donep;
  int prev_querypos, prevhit;
  Chrpos_T prevposition;
  int gendistance;
  Univcoord_T prevpos, currpos;
  int querydistance, diffdistance, lookback, nlookback, nseen, indexsize_nt;
  /* int querydist_penalty; */
  int querydist_credit;
  /* bool near_end_p; */
  bool canonicalp;

#ifdef PMAP
  indexsize_nt = indexsize*3;	/* Use when evaluating across genomic positions */
#else
  indexsize_nt = indexsize;
#endif
#if 0
  indexsize_query = indexsize;	/* Use when evaluating across query positions */
#endif


  /* Parameters for section D, assuming adjacent is false */
  /* adjacentp = false; */
  nlookback = nsufflookback;
  lookback = sufflookback;

  /* A. Evaluate adjacent position (at last one processed) */
  if (processed != NULL) {
    prev_querypos = Intlist_head(processed);
    prev_links = links[prev_querypos];
    prev_mappings = mappings[prev_querypos];
    prev_active = active[prev_querypos];

#ifdef PMAP
    querydistance = (curr_querypos - prev_querypos)*3;
#else
    querydistance = curr_querypos - prev_querypos;
#endif
    prevhit = firstactive[prev_querypos];
    prevposition = position;	/* Prevents prevposition + querydistance == position */
    while (prevhit != -1 && (prevposition = /*mappings[prev_querypos]*/prev_mappings[prevhit]) + querydistance < position) {
      prevhit = /*active[prev_querypos]*/prev_active[prevhit];
    }
    if (prevposition + querydistance == position) {
      prevlink = &(/*links[prev_querypos]*/prev_links[prevhit]);
      best_fwd_consecutive = prevlink->fwd_consecutive + querydistance;
      best_fwd_rootposition = prevlink->fwd_rootposition;
      /* best_fwd_rootnlinks = prevlink->fwd_rootnlinks + 1; */
      best_fwd_score = fwd_scores[prev_querypos][prevhit] + CONSEC_POINTS_PER_MATCH*querydistance;

      best_fwd_prevpos = prev_querypos;
      best_fwd_prevhit = prevhit;
      best_fwd_tracei = prevlink->fwd_tracei;
#ifdef DEBUG9
      best_fwd_intronnfwd = prevlink->fwd_intronnfwd;
      best_fwd_intronnrev = prevlink->fwd_intronnrev;
      best_fwd_intronnunk = prevlink->fwd_intronnunk;
#endif
      /* adjacentp = true; */

      /* Parameters for section D when adjacent is true, so we don't look so far back */
      nlookback = 1;
      lookback = sufflookback/2;


      debug9(printf("\tA. Adjacent qpos %d,%d at %ux%d (scores = %d -> %d, consec = %d (from #%d), intr = %d-%d-%d)\n",
		    prev_querypos,prevhit,prevposition,active[prev_querypos][prevhit],fwd_scores[prev_querypos][prevhit],
		    best_fwd_score,best_fwd_consecutive,best_fwd_tracei,
		    best_fwd_intronnfwd,best_fwd_intronnrev,best_fwd_intronnunk));
    }
  }


#ifdef MOVE_TO_STAGE3
  /* Check work list */
  if (anchoredp && curr_querypos - indexsize_query <= querystart) {
    /* Allow close prevpositions that overlap with anchor */
    /* Can give rise to false positives, and increases amount of dynamic programming work */
  } else if (0 && anchoredp && curr_querypos == queryend) {
    /* Test first position */
  } else if (0) {
    while (processed != NULL && (prev_querypos = Intlist_head(processed)) > curr_querypos - indexsize_query) {
      debug9(printf("Skipping prev_querypos %d, because too close\n",prev_querypos));
      processed = Intlist_next(processed);
    }
  }
#endif

  /* D. Evaluate for mismatches (all other previous querypos) */
  donep = false;
  nseen = 0;
  last_tracei = -1;
  for ( ; processed != NULL && best_fwd_consecutive < ENOUGH_CONSECUTIVE && donep == false;
	processed = Intlist_next(processed), nseen++) {
    prev_querypos = Intlist_head(processed);

#ifdef PMAP
    querydistance = (curr_querypos - prev_querypos)*3;
#else
    querydistance = curr_querypos - prev_querypos;
#endif

    if (nseen > nlookback && querydistance - indexsize_nt > lookback) {
      donep = true;
    }

    if ((prevhit = firstactive[prev_querypos]) != -1) {
      /* querydist_penalty = (querydistance - indexsize_nt)/QUERYDIST_PENALTY_FACTOR; */
      /* Actually a querydist_penalty */
      querydist_credit = -querydistance/indexsize_nt;

      prev_mappings = mappings[prev_querypos];
      prev_links = links[prev_querypos];
      prev_active = active[prev_querypos];

      /* Range 0 */
      while (prevhit != -1 && prev_links[prevhit].fwd_tracei == last_tracei) {
	debug9(printf("Skipping querypos %d with tracei #%d\n",prev_querypos,prev_links[prevhit].fwd_tracei));
	prevhit = /*active[prev_querypos]*/prev_active[prevhit];
      }
      if (prevhit != -1) {
	last_tracei = prev_links[prevhit].fwd_tracei;
      }

      /* Range 1: From Infinity to maxintronlen */
      if (splicingp == true) {
	/* This is equivalent to diffdistance >= maxintronlen, where
	   diffdistance = abs(gendistance - querydistance) and
	   gendistance = (position - prevposition - indexsize_nt) */
	while (prevhit != -1 && (prevposition = /*mappings[prev_querypos]*/prev_mappings[prevhit]) + maxintronlen + querydistance <= position) {
	  /* Skip */
	  /* printf("fwd: prevposition %u, prevhit %d\n",prevposition,prevhit); */
	  prevhit = /*active[prev_querypos]*/prev_active[prevhit];
	}
      }

      /* Range 2: From maxintronlen to (prev_querypos + EQUAL_DISTANCE_NOT_SPLICING) */
      /* This is equivalent to +diffdistance > EQUAL_DISTANCE_NOT_SPLICING */
      while (prevhit != -1 && (prevposition = /*mappings[prev_querypos]*/prev_mappings[prevhit]) + EQUAL_DISTANCE_NOT_SPLICING + querydistance < position) {
	/* printf("fwd: prevposition %u, prevhit %d\n",prevposition,prevhit); */
	prevlink = &(/*links[prev_querypos]*/prev_links[prevhit]);

	gendistance = position - prevposition;
	assert(gendistance > querydistance); /* True because gendistance > EQUAL_DISTANCE_NOT_SPLICING + querydistance */
	diffdistance = gendistance - querydistance; /* No need for abs() */

	fwd_score = fwd_scores[prev_querypos][prevhit] + querydist_credit /*- querydist_penalty*/;
	if (splicingp == true) {
	  fwd_score -= (diffdistance/TEN_THOUSAND + 1);
	} else {
	  fwd_score -= (diffdistance/ONE + 1);
	}

	if (use_canonical_p == true) {

	  /* prevpos is lower genomic coordinate than currpos */
	  /* need to subtract from position and prevposition to compensate for greedy matches */
	  /* need to add to position and prevposition to compensate for missed matches */
	  if (plusp == true) {
	    prevpos = chroffset + prevposition + indexsize_nt;
	    currpos = chroffset + position - querydistance + indexsize_nt;
	    assert(prevpos < currpos);

	    if (prevpos < GREEDY_ADVANCE || currpos < GREEDY_ADVANCE) {
	      canonicalp = false;
	    } else if (Genome_sense_canonicalp(genome,genomealt,
					       /*donor_rightbound*/prevpos + MISS_BEHIND,
					       /*donor_leftbound*/prevpos - GREEDY_ADVANCE,
					       /*acceptor_rightbound*/currpos + MISS_BEHIND,
					       /*acceptor_leftbound*/currpos - GREEDY_ADVANCE,
					       chroffset) == true) {
	      debug9(printf("lookback plus: sense canonical\n"));
	      canonicalp = true;
	    } else if (Genome_antisense_canonicalp(genome,genomealt,
						   /*donor_rightbound*/currpos + MISS_BEHIND,
						   /*donor_leftbound*/currpos - GREEDY_ADVANCE,
						   /*acceptor_rightbound*/prevpos + MISS_BEHIND,
						   /*acceptor_leftbound*/prevpos - GREEDY_ADVANCE,
						   chroffset) == true) {
	      debug9(printf("lookback plus: antisense canonical\n"));
	      canonicalp = true;
	    } else {
	      debug9(printf("lookback plus: not canonical\n"));
	      canonicalp = false;
	    }

	  } else {
	    prevpos = chrhigh + 1 - prevposition - indexsize_nt;
	    currpos = chrhigh + 1 - position + querydistance - indexsize_nt;
	    assert(currpos < prevpos);

	    if (currpos < MISS_BEHIND || prevpos < MISS_BEHIND) {
	      canonicalp = false;
	    } else if (Genome_sense_canonicalp(genome,genomealt,
					       /*donor_rightbound*/currpos + GREEDY_ADVANCE,
					       /*donor_leftbound*/currpos - MISS_BEHIND,
					       /*acceptor_rightbound*/prevpos + GREEDY_ADVANCE,
					       /*acceptor_leftbound*/prevpos - MISS_BEHIND,
					       chroffset) == true) {
	      debug9(printf("lookback minus: sense canonical\n"));
	      canonicalp = true;
	    } else if (Genome_antisense_canonicalp(genome,genomealt,
						   /*donor_rightbound*/prevpos + GREEDY_ADVANCE,
						   /*donor_leftbound*/prevpos - MISS_BEHIND,
						   /*acceptor_rightbound*/currpos + GREEDY_ADVANCE,
						   /*acceptor_leftbound*/currpos - MISS_BEHIND,
						   chroffset) == true) {
	      debug9(printf("lookback minus: antisense canonical\n"));
	      canonicalp = true;
	    } else {
	      debug9(printf("lookback minus: not canonical\n"));
	      canonicalp = false;
	    }
	  }

	  if (canonicalp == true) {
	    debug9(canonicalsgn = +1);
	  } else {
	    debug9(canonicalsgn = 0);
	    fwd_score -= non_canonical_penalty;
	  }

	}

	debug9(printf("\tD2. Fwd mismatch qpos %d,%d at %ux%d (score = %d -> %d, consec = %d (from #%d), intr = %d-%d-%d, gendist %u, querydist %d, canonicalsgn %d)",
		      prev_querypos,prevhit,prevposition,active[prev_querypos][prevhit],
		      fwd_scores[prev_querypos][prevhit],fwd_score,prevlink->fwd_consecutive,prevlink->fwd_tracei,
		      best_fwd_intronnfwd,best_fwd_intronnrev,best_fwd_intronnunk,
		      gendistance-indexsize_nt,querydistance-indexsize_nt,canonicalsgn));
	    
	/* Disallow ties, which should favor adjacent */
	if (fwd_score > best_fwd_score) {
	  if (diffdistance <= EQUAL_DISTANCE_FOR_CONSECUTIVE) {
	    best_fwd_consecutive = prevlink->fwd_consecutive + querydistance;
	    /* best_fwd_rootnlinks = prevlink->fwd_rootnlinks + 1; */
	  } else {
	    best_fwd_consecutive = 0;
	    /* best_fwd_rootnlinks = 1; */
	  }
	  best_fwd_rootposition = prevlink->fwd_rootposition;
	  best_fwd_score = fwd_score;
	  best_fwd_prevpos = prev_querypos;
	  best_fwd_prevhit = prevhit;
	  best_fwd_tracei = ++*fwd_tracei;
#ifdef DEBUG9
	  best_fwd_intronnfwd = prevlink->fwd_intronnfwd;
	  best_fwd_intronnrev = prevlink->fwd_intronnrev;
	  best_fwd_intronnunk = prevlink->fwd_intronnunk;
	  switch (canonicalsgn) {
	  case 1: best_fwd_intronnfwd++; break;
	  case 0: best_fwd_intronnunk++; break;
	  }
#endif
	  debug9(printf(" => Best fwd at %d (consec = %d)\n",fwd_score,best_fwd_consecutive));
	} else {
	  debug9(printf(" => Loses to %d\n",best_fwd_score));
	}

	prevhit = /*active[prev_querypos]*/prev_active[prevhit];
      }

      /* Scoring appears to be the same as for range 4, which is rarely called, so including in range 4 */
      /* Range 3: From (querypos + EQUAL_DISTANCE_NOT_SPLICING) to (querypos - EQUAL_DISTANCE_NOT_SPLICING) */
      /* This is equivalent to -diffdistance > EQUAL_DISTANCE_NOT_SPLICING && prevposition + indexsize_nt <= position */

      /* Range 4: From (prev_querypos - EQUAL_DISTANCE_NOT_SPLICING) to indexsize_nt */
      while (prevhit != -1 && (prevposition = /*mappings[prev_querypos]*/prev_mappings[prevhit]) + indexsize_nt <= position) {
	/* printf("fwd: prevposition %u, prevhit %d\n",prevposition,prevhit); */
	prevlink = &(/*links[prev_querypos]*/prev_links[prevhit]);

	gendistance = position - prevposition;
	/* was abs(gendistance - querydistance) */
	diffdistance = gendistance > querydistance ? (gendistance - querydistance) : (querydistance - gendistance);

#ifdef BAD_GMAX
	fwd_score = prevlink->fwd_score + querydist_credit - (diffdistance/ONE + 1) /*- querydist_penalty*/;
#else
	/* diffdistance <= EQUAL_DISTANCE_NOT_SPLICING */
	/* This is how version 2013-08-14 did it */
	fwd_score = fwd_scores[prev_querypos][prevhit] + CONSEC_POINTS_PER_MATCH;
#endif

#if 0
	/* Used in range 4 but not in range 3 */
	if (/*near_end_p == false &&*/ prevlink->fwd_consecutive < EXON_DEFN) {
	  fwd_score -= NINTRON_PENALTY_MISMATCH;
	}
#endif

	debug9(printf("\tD4. Fwd mismatch qpos %d,%d at %ux%d (score = %d -> %d, consec = %d (from #%d), intr = %d-%d-%d, gendist %u, querydist %d, canonicalsgn %d)",
		      prev_querypos,prevhit,prevposition,active[prev_querypos][prevhit],
		      fwd_scores[prev_querypos][prevhit],fwd_score,prevlink->fwd_consecutive,prevlink->fwd_tracei,
		      best_fwd_intronnfwd,best_fwd_intronnrev,best_fwd_intronnunk,
		      gendistance-indexsize_nt,querydistance-indexsize_nt,canonicalsgn));
	    
	/* Disallow ties, which should favor adjacent */
	if (fwd_score > best_fwd_score) {
	  if (diffdistance <= EQUAL_DISTANCE_FOR_CONSECUTIVE) {
	    best_fwd_consecutive = prevlink->fwd_consecutive + querydistance;
	    /* best_fwd_rootnlinks = prevlink->fwd_rootnlinks + 1; */
	  } else {
	    best_fwd_consecutive = 0;
	    /* best_fwd_rootnlinks = 1; */
	  }
	  best_fwd_rootposition = prevlink->fwd_rootposition;
	  best_fwd_score = fwd_score;
	  best_fwd_prevpos = prev_querypos;
	  best_fwd_prevhit = prevhit;
	  /* best_fwd_tracei = ++*fwd_tracei; */
	  best_fwd_tracei = prevlink->fwd_tracei; /* Keep previous trace, as in range 3 */
#ifdef DEBUG9
	  best_fwd_intronnfwd = prevlink->fwd_intronnfwd;
	  best_fwd_intronnrev = prevlink->fwd_intronnrev;
	  best_fwd_intronnunk = prevlink->fwd_intronnunk;
	  switch (canonicalsgn) {
	  case 1: best_fwd_intronnfwd++; break;
	  case 0: best_fwd_intronnunk++; break;
	  }
#endif
	  debug9(printf(" => Best fwd at %d (consec = %d)\n",fwd_score,best_fwd_consecutive));
	} else {
	  debug9(printf(" => Loses to %d\n",best_fwd_score));
	}

	prevhit = /*active[prev_querypos]*/prev_active[prevhit];
      }
    }
  }

  /* Best_score needs to beat something positive to prevent a
     small local extension from beating a good canonical intron.
     If querypos is too small, don't insert an intron.  */
  /* linksconsecutive already assigned above */
  currlink->fwd_consecutive = best_fwd_consecutive;
  currlink->fwd_rootposition = best_fwd_rootposition;
  /* currlink->fwd_rootnlinks = best_fwd_rootnlinks; */
  currlink->fwd_pos = best_fwd_prevpos;
  currlink->fwd_hit = best_fwd_prevhit;
  if (currlink->fwd_pos >= 0) {
    currlink->fwd_tracei = best_fwd_tracei;
    fwd_scores[curr_querypos][currhit] = best_fwd_score;
#ifdef MOVE_TO_STAGE3
  } else if (anchoredp == true) {
    currlink->fwd_tracei = -1;
    fwd_scores[curr_querypos][currhit] = -100000;
#endif
  } else if (localp == true) {
    currlink->fwd_tracei = ++*fwd_tracei;
    fwd_scores[curr_querypos][currhit] = indexsize_nt;
  } else {
    currlink->fwd_tracei = ++*fwd_tracei;
    fwd_scores[curr_querypos][currhit] = best_fwd_score;
  }

#ifdef DEBUG9
  currlink->fwd_intronnfwd = best_fwd_intronnfwd;
  currlink->fwd_intronnrev = best_fwd_intronnrev;
  currlink->fwd_intronnunk = best_fwd_intronnunk;
#endif

  debug9(printf("\tChose %d,%d with score %d (fwd) => trace #%d\n",
		currlink->fwd_pos,currlink->fwd_hit,fwd_scores[curr_querypos][currhit],currlink->fwd_tracei));
  debug3(printf("%d %d  %d %d  1\n",querypos,hit,best_prevpos,best_prevhit));

  return;
}




static void
score_querypos_lookback_mult (int *fwd_tracei, int low_hit, int high_hit, int curr_querypos,
			      unsigned int *positions,
			      struct Link_T **links, int **fwd_scores,
			      Chrpos_T **mappings, int **active, int *firstactive,
			      Genome_T genome, Genome_T genomealt,
			      Univcoord_T chroffset, Univcoord_T chrhigh, bool plusp,
			      int indexsize, Intlist_T processed,
#ifdef MOVE_TO_STAGE3
			      bool anchoredp,
#endif
			      bool localp, bool splicingp,
			      bool use_canonical_p, int non_canonical_penalty) {
  Link_T prevlink, currlink;
  Intlist_T last_item, p;
  int nhits = high_hit - low_hit, nprocessed, hiti;

  struct Link_T *prev_links, *adj_links;
  Chrpos_T *prev_mappings, *adj_mappings;
  int *prev_active, *adj_active;

  int overall_fwd_consecutive, best_fwd_consecutive;
  int best_fwd_rootposition;
  int best_fwd_score, fwd_score;
  int best_fwd_prevpos, best_fwd_prevhit;
  int best_fwd_tracei, last_tracei;
#ifdef DEBUG9
  int best_fwd_intronnfwd, best_fwd_intronnrev, best_fwd_intronnunk;
  int canonicalsgn = 0;
#endif
  int adj_querypos, adj_querydistance, prev_querypos, prevhit, adj_frontier, *frontier;
  Chrpos_T prevposition, position;
  int gendistance;
  Univcoord_T prevpos, currpos;
  int querydistance, diffdistance, indexsize_nt;
  int max_nseen, max_adjacent_nseen, max_nonadjacent_nseen, nseen;
  int querydist_credit;
  bool canonicalp;

#ifdef PMAP
  indexsize_nt = indexsize*3;	/* Use when evaluating across genomic positions */
#else
  indexsize_nt = indexsize;
#endif
#if 0
  indexsize_query = indexsize;	/* Use when evaluating across query positions */
#endif


  /* Determine work load */
  /* printf("Work load (lookback): %s\n",Intlist_to_string(processed)); */
  last_item = processed;
#ifdef MOVE_TO_STAGE3
  if (anchoredp && curr_querypos - indexsize_query <= querystart) {
    /* Allow close prevpositions that overlap with anchor */
    /* Can give rise to false positives, and increases amount of dynamic programming work */
    /* debug9(printf("No skipping because close to anchor\n")); */
  } else if (0 && anchoredp && curr_querypos == queryend) {
    /* Test first position */
  } else if (0) {
    while (processed != NULL && (/*prev_querypos =*/ Intlist_head(processed)) > curr_querypos - indexsize_query) {
      debug9(printf("Skipping prev_querypos %d, because too close\n",Intlist_head(processed)));
      processed = Intlist_next(processed);
    }
  }
#endif

  if (last_item == NULL) {
    for (hiti = 0; hiti < nhits; hiti++) {
      currlink = &(links[curr_querypos][hiti + low_hit]);

      currlink->fwd_consecutive = /*best_fwd_consecutive =*/ indexsize*NT_PER_MATCH;
      currlink->fwd_rootposition = /*best_fwd_rootposition =*/ positions[hiti];
      currlink->fwd_pos = /*best_fwd_prevpos =*/ -1;
      currlink->fwd_hit = /*best_fwd_prevhit =*/ -1;

#ifdef MOVE_TO_STAGE3
      if (anchoredp == true) {
	currlink->fwd_tracei = -1;
	fwd_scores[curr_querypos][hiti + low_hit] = -100000;
      } else
#endif
      if (localp == true) {
	currlink->fwd_tracei = ++*fwd_tracei;
	fwd_scores[curr_querypos][hiti + low_hit] = indexsize_nt;
      } else {
	fwd_scores[curr_querypos][hiti + low_hit] = /*best_fwd_score =*/ 0;
      }
    }

  } else if (processed == NULL) {
    debug9(printf("processed is NULL\n"));
    /* A. Evaluate adjacent position (at last one processed, if available).  Don't evaluate for mismatches (D). */
    adj_querypos = Intlist_head(last_item);
    adj_links = links[adj_querypos];
    adj_mappings = mappings[adj_querypos];
    adj_active = active[adj_querypos];

#ifdef PMAP
    adj_querydistance = (curr_querypos - adj_querypos)*3;
#else
    adj_querydistance = curr_querypos - adj_querypos;
#endif

    /* Process prevhit and hiti in parallel.  Values are asscending along prevhit chain and from 0 to nhits-1. */
    prevhit = firstactive[adj_querypos];
    hiti = 0;
    while (prevhit != -1 && hiti < nhits) {
      if ((prevposition = /*mappings[adj_querypos]*/adj_mappings[prevhit]) + adj_querydistance < (position = positions[hiti])) {
	prevhit = /*active[adj_querypos]*/adj_active[prevhit];

      } else if (prevposition + adj_querydistance > position) {
	currlink = &(links[curr_querypos][hiti + low_hit]);

	currlink->fwd_consecutive = /*best_fwd_consecutive =*/ indexsize*NT_PER_MATCH;
	currlink->fwd_rootposition = /*best_fwd_rootposition =*/ positions[hiti];
	currlink->fwd_pos = /*best_fwd_prevpos =*/ -1;
	currlink->fwd_hit = /*best_fwd_prevhit =*/ -1;

#ifdef MOVE_TO_STAGE3
	if (anchoredp == true) {
	  currlink->fwd_tracei = -1;
	  fwd_scores[curr_querypos][hiti + low_hit] = -100000;
	} else 
#endif
        if (localp == true) {
	  currlink->fwd_tracei = ++*fwd_tracei;
	  fwd_scores[curr_querypos][hiti + low_hit] = indexsize_nt;
	} else {
	  fwd_scores[curr_querypos][hiti + low_hit] = /*best_fwd_score =*/ 0;
	}

	hiti++;

      } else {
	/* Adjacent position found for hiti */
	currlink = &(links[curr_querypos][hiti + low_hit]);
	prevlink = &(/*links[adj_querypos]*/adj_links[prevhit]);

	currlink->fwd_consecutive = /*best_fwd_consecutive =*/ prevlink->fwd_consecutive + adj_querydistance;
	currlink->fwd_rootposition = /*best_fwd_rootposition =*/ prevlink->fwd_rootposition;
	currlink->fwd_pos = /*best_fwd_prevpos =*/ adj_querypos;
	currlink->fwd_hit = /*best_fwd_prevhit =*/ prevhit;
	fwd_scores[curr_querypos][hiti + low_hit] = /*best_fwd_score =*/ fwd_scores[adj_querypos][prevhit] + CONSEC_POINTS_PER_MATCH*adj_querydistance;

#ifdef DEBUG9
	printf("\tA(1). For hit %d, adjacent qpos %d,%d at %ux%d (scores = %d -> %d, consec = %d (from #%d), intr = %d-%d-%d)\n",
	       hiti,adj_querypos,prevhit,prevposition,active[adj_querypos][prevhit],fwd_scores[adj_querypos][prevhit],
	       fwd_scores[curr_querypos][hiti + low_hit],currlink->fwd_consecutive,/*best_fwd_tracei*/prevlink->fwd_tracei,
	       /*best_fwd_intronnfwd*/prevlink->fwd_intronnfwd,
	       /*best_fwd_intronnrev*/prevlink->fwd_intronnrev,
	       /*best_fwd_intronnunk*/prevlink->fwd_intronnunk);
#endif

	prevhit = /*active[adj_querypos]*/adj_active[prevhit];
	hiti++;
      }
    }

    while (hiti < nhits) {
      currlink = &(links[curr_querypos][hiti + low_hit]);

      currlink->fwd_consecutive = /*best_fwd_consecutive =*/ indexsize*NT_PER_MATCH;
      currlink->fwd_rootposition = /*best_fwd_rootposition =*/ positions[hiti];
      currlink->fwd_pos = /*best_fwd_prevpos =*/ -1;
      currlink->fwd_hit = /*best_fwd_prevhit =*/ -1;

#ifdef MOVE_TO_STAGE3
      if (anchoredp == true) {
	currlink->fwd_tracei = -1;
	fwd_scores[curr_querypos][hiti + low_hit] = -100000;
      } else
#endif
      if (localp == true) {
	currlink->fwd_tracei = ++*fwd_tracei;
	fwd_scores[curr_querypos][hiti + low_hit] = indexsize_nt;
      } else {
	fwd_scores[curr_querypos][hiti + low_hit] = /*best_fwd_score =*/ 0;
      }

      hiti++;
    }

  } else {
    adj_querypos = Intlist_head(last_item);
    adj_links = links[adj_querypos];
    adj_mappings = mappings[adj_querypos];
    adj_active = active[adj_querypos];

#ifdef PMAP
    adj_querydistance = (curr_querypos - adj_querypos)*3;
#else
    adj_querydistance = curr_querypos - adj_querypos;
#endif
    nprocessed = Intlist_length(processed);
    frontier = (int *) MALLOCA(nprocessed * sizeof(int));

    nseen = 0;
    for (p = processed; p != NULL; p = Intlist_next(p)) {
      prev_querypos = Intlist_head(p);

      querydistance = curr_querypos - prev_querypos;
      if (nseen <= /*nlookback*/1 || querydistance - indexsize_nt <= /*lookback*/sufflookback/2) {
	max_adjacent_nseen = nseen;
      }
      if (nseen <= /*nlookback*/nsufflookback || querydistance - indexsize_nt <= /*lookback*/sufflookback) {
	max_nonadjacent_nseen = nseen;
      }

      frontier[nseen++] = firstactive[prev_querypos];
    }
    

    /* Look for overall_fwd_consecutive to see whether we can be greedy */
    overall_fwd_consecutive = 0;
    adj_frontier = firstactive[adj_querypos];
    for (hiti = 0; hiti < nhits; hiti++) {
      position = positions[hiti];

      /* A. Evaluate adjacent positions (at last one processed) */
      prevhit = adj_frontier;	/* Get information from last hiti */
      prevposition = position;	/* Prevents prevposition + adj_querydistance == position */
      while (prevhit != -1 && (prevposition = /*mappings[adj_querypos]*/adj_mappings[prevhit]) + adj_querydistance < position) {
	prevhit = /*active[adj_querypos]*/adj_active[prevhit];
      }
      adj_frontier = prevhit;	/* Save information for next hiti */

      if (prevposition + adj_querydistance == position) {
	/* Adjacent found */
	prevlink = &(/*links[adj_querypos]*/adj_links[prevhit]);
	if (prevlink->fwd_consecutive + adj_querydistance > overall_fwd_consecutive) {
	  overall_fwd_consecutive = prevlink->fwd_consecutive + adj_querydistance;
	}
      }
    }
    debug(printf("(1) Overall fwd consecutive is %d\n",overall_fwd_consecutive));


    /* Now process */
    adj_frontier = firstactive[adj_querypos];
    for (hiti = 0; hiti < nhits; hiti++) {
      position = positions[hiti];

      /* A. Evaluate adjacent positions (at last one processed) */
      prevhit = adj_frontier;	/* Get information from last hiti */
      prevposition = position;	/* Prevents prevposition + adj_querydistance == position */
      while (prevhit != -1 && (prevposition = /*mappings[adj_querypos]*/adj_mappings[prevhit]) + adj_querydistance < position) {
	prevhit = /*active[adj_querypos]*/adj_active[prevhit];
      }
      adj_frontier = prevhit;	/* Save information for next hiti */

      if (prevposition + adj_querydistance == position) {
	/* Adjacent found */
	prevlink = &(/*links[adj_querypos]*/adj_links[prevhit]);

	best_fwd_consecutive = prevlink->fwd_consecutive + adj_querydistance;
	best_fwd_rootposition = prevlink->fwd_rootposition;
	best_fwd_prevpos = adj_querypos;
	best_fwd_prevhit = prevhit;
	best_fwd_score = fwd_scores[adj_querypos][prevhit] + CONSEC_POINTS_PER_MATCH*adj_querydistance;
	max_nseen = max_adjacent_nseen;	/* Look not so far back */
	best_fwd_tracei = prevlink->fwd_tracei;

#ifdef DEBUG9
	best_fwd_intronnfwd = prevlink->fwd_intronnfwd;
	best_fwd_intronnrev = prevlink->fwd_intronnrev;
	best_fwd_intronnunk = prevlink->fwd_intronnunk;
#endif
	debug9(printf("\tA(2). For hit %d, adjacent qpos %d,%d at %ux%d (scores = %d -> %d, consec = %d (from #%d), intr = %d-%d-%d)\n",
		      hiti,adj_querypos,prevhit,prevposition,active[adj_querypos][prevhit],fwd_scores[adj_querypos][prevhit],
		      best_fwd_score,best_fwd_consecutive,/*best_fwd_tracei*/prevlink->fwd_tracei,
		      best_fwd_intronnfwd,best_fwd_intronnrev,best_fwd_intronnunk));

      } else {
	/* Adjacent not found */
	best_fwd_consecutive = indexsize*NT_PER_MATCH;
	best_fwd_rootposition = position;
	best_fwd_prevpos = -1;
	best_fwd_prevhit = -1;
	best_fwd_score = 0;
	max_nseen = max_nonadjacent_nseen; /* Look farther back */
	best_fwd_tracei = -1;

#ifdef DEBUG9
	best_fwd_intronnfwd = 0;
	best_fwd_intronnrev = 0;
	best_fwd_intronnunk = 0;
#endif
      }

      if (overall_fwd_consecutive < GREEDY_NCONSECUTIVE) {
	/* D. Evaluate for mismatches (all other previous querypos) */
	nseen = 0;
	last_tracei = -1;
	for (p = processed; p != NULL && best_fwd_consecutive < ENOUGH_CONSECUTIVE && nseen <= max_nseen;
	     p = Intlist_next(p), nseen++) {

	  /* Making this check helps with efficiency */
	  if ((prevhit = frontier[nseen]) != -1) { /* Retrieve starting point from last hiti */
	    prev_querypos = Intlist_head(p);
#ifdef PMAP
	    querydistance = (curr_querypos - prev_querypos)*3;
#else
	    querydistance = curr_querypos - prev_querypos;
#endif
	    /* Actually a querydist_penalty */
	    querydist_credit = -querydistance/indexsize_nt;

	    prev_mappings = mappings[prev_querypos];
	    prev_links = links[prev_querypos];
	    prev_active = active[prev_querypos];

	    /* Range 0 */
	    while (prevhit != -1 && prev_links[prevhit].fwd_tracei == last_tracei) {
	      debug9(printf("Skipping querypos %d with tracei #%d\n",prev_querypos,prev_links[prevhit].fwd_tracei));
	      prevhit = /*active[prev_querypos]*/prev_active[prevhit];
	    }
	    if (prevhit != -1) {
	      last_tracei = prev_links[prevhit].fwd_tracei;
	    }

	    /* Range 1: From Infinity to maxintronlen.  To be skipped.
	       This is equivalent to diffdistance >= maxintronlen, where
	       diffdistance = abs(gendistance - querydistance) and
	       gendistance = (position - prevposition - indexsize_nt) */
	    while (prevhit != -1 && (/*prevposition =*/ /*mappings[prev_querypos]*/prev_mappings[prevhit]) + maxintronlen + querydistance <= position) {
	      /* Accept within range 1 (ignore) */
	      prevhit = /*active[prev_querypos]*/prev_active[prevhit];
	    }
	    frontier[nseen] = prevhit;	/* Store as starting point for next hiti */

	    /* Range 2: From maxintronlen to (prev_querypos + EQUAL_DISTANCE_NOT_SPLICING) */
	    /* This is equivalent to +diffdistance > EQUAL_DISTANCE_NOT_SPLICING */
	    while (prevhit != -1 && (prevposition = /*mappings[prev_querypos]*/prev_mappings[prevhit]) + EQUAL_DISTANCE_NOT_SPLICING + querydistance < position) {
	      prevlink = &(/*links[prev_querypos]*/prev_links[prevhit]);

	      gendistance = position - prevposition;
	      assert(gendistance > querydistance); /* True because gendistance > EQUAL_DISTANCE_NOT_SPLICING + querydistance */
	      diffdistance = gendistance - querydistance; /* No need for abs() */

	      fwd_score = fwd_scores[prev_querypos][prevhit] + querydist_credit /*- querydist_penalty*/;
	      if (splicingp == true) {
		fwd_score -= (diffdistance/TEN_THOUSAND + 1);
	      } else {
		fwd_score -= (diffdistance/ONE + 1);
	      }

	      if (use_canonical_p == true) {
		/* prevpos is lower genomic coordinate than currpos */
		/* need to subtract from position and prevposition to compensate for greedy matches */
		/* need to add to position and prevposition to compensate for missed matches */
		if (plusp == true) {
		  prevpos = chroffset + prevposition + indexsize_nt;
		  currpos = chroffset + position - querydistance + indexsize_nt;
		  if (prevpos < GREEDY_ADVANCE || currpos < GREEDY_ADVANCE) {
		    canonicalp = false;
		  } else if (Genome_sense_canonicalp(genome,genomealt,
						     /*donor_rightbound*/prevpos + MISS_BEHIND,
						     /*donor_leftbound*/prevpos - GREEDY_ADVANCE,
						     /*acceptor_rightbound*/currpos + MISS_BEHIND,
						     /*acceptor_leftbound*/currpos - GREEDY_ADVANCE,
						     chroffset) == true) {
		    debug9(printf("lookback plus: sense canonical\n"));
		    canonicalp = true;
		  } else if (Genome_antisense_canonicalp(genome,genomealt,
							 /*donor_rightbound*/currpos + MISS_BEHIND,
							 /*donor_leftbound*/currpos - GREEDY_ADVANCE,
							 /*acceptor_rightbound*/prevpos + MISS_BEHIND,
							 /*acceptor_leftbound*/prevpos - GREEDY_ADVANCE,
							 chroffset) == true) {
		    debug9(printf("lookback plus: antisense canonical\n"));
		    canonicalp = true;
		  } else {
		    debug9(printf("lookback plus: not canonical\n"));
		    canonicalp = false;
		  }

		} else {
		  prevpos = chrhigh + 1 - prevposition - indexsize_nt;
		  currpos = chrhigh + 1 - position + querydistance - indexsize_nt;
		  if (currpos < MISS_BEHIND || prevpos < MISS_BEHIND) {
		    canonicalp = false;
		  } else if (Genome_sense_canonicalp(genome,genomealt,
						     /*donor_rightbound*/currpos + GREEDY_ADVANCE,
						     /*donor_leftbound*/currpos - MISS_BEHIND,
						     /*acceptor_rightbound*/prevpos + GREEDY_ADVANCE,
						     /*acceptor_leftbound*/prevpos - MISS_BEHIND,
						     chroffset) == true) {
		    debug9(printf("lookback minus: sense canonical\n"));
		    canonicalp = true;
		  } else if (Genome_antisense_canonicalp(genome,genomealt,
							 /*donor_rightbound*/prevpos + GREEDY_ADVANCE,
							 /*donor_leftbound*/prevpos - MISS_BEHIND,
							 /*acceptor_rightbound*/currpos + GREEDY_ADVANCE,
							 /*acceptor_leftbound*/currpos - MISS_BEHIND,
							 chroffset) == true) {
		    debug9(printf("lookback minus: antisense canonical\n"));
		    canonicalp = true;
		  } else {
		    debug9(printf("lookback minus: not canonical\n"));
		    canonicalp = false;
		  }
		}

		if (canonicalp == true) {
		  debug9(canonicalsgn = +1);
		} else {
		  debug9(canonicalsgn = 0);
		  fwd_score -= non_canonical_penalty;
		}
	      }

	      debug9(printf("\tD2, hit %d. Fwd mismatch qpos %d,%d at %ux%d (score = %d -> %d, consec = %d (from #%d), intr = %d-%d-%d, gendist %u, querydist %d, canonicalsgn %d)",
			    hiti,prev_querypos,prevhit,prevposition,active[prev_querypos][prevhit],
			    fwd_scores[prev_querypos][prevhit],fwd_score,prevlink->fwd_consecutive,prevlink->fwd_tracei,
			    best_fwd_intronnfwd,best_fwd_intronnrev,best_fwd_intronnunk,
			    gendistance-indexsize_nt,querydistance-indexsize_nt,canonicalsgn));
	    
	      /* Disallow ties, which should favor adjacent */
	      if (fwd_score > best_fwd_score) {
		if (diffdistance <= EQUAL_DISTANCE_FOR_CONSECUTIVE) {
		  best_fwd_consecutive = prevlink->fwd_consecutive + querydistance;
		} else {
		  best_fwd_consecutive = 0;
		}
		best_fwd_rootposition = prevlink->fwd_rootposition;
		best_fwd_score = fwd_score;
		best_fwd_prevpos = prev_querypos;
		best_fwd_prevhit = prevhit;
		best_fwd_tracei = ++*fwd_tracei;
#ifdef DEBUG9
		best_fwd_intronnfwd = prevlink->fwd_intronnfwd;
		best_fwd_intronnrev = prevlink->fwd_intronnrev;
		best_fwd_intronnunk = prevlink->fwd_intronnunk;
		switch (canonicalsgn) {
		case 1: best_fwd_intronnfwd++; break;
		case 0: best_fwd_intronnunk++; break;
		}
#endif
		debug9(printf(" => Best fwd at %d (consec = %d)\n",fwd_score,best_fwd_consecutive));
	      } else {
		debug9(printf(" => Loses to %d\n",best_fwd_score));
	      }
	      prevhit = /*active[prev_querypos]*/prev_active[prevhit];
	    }


	    /* Scoring appears to be the same as for range 4, which is rarely called, so including in range 4 */
	    /* Range 3: From (querypos + EQUAL_DISTANCE_NOT_SPLICING) to (querypos - EQUAL_DISTANCE_NOT_SPLICING) */
	    /* This is equivalent to -diffdistance > EQUAL_DISTANCE_NOT_SPLICING && prevposition + indexsize_nt <= position */


	    /* Range 4: From (prev_querypos - EQUAL_DISTANCE_NOT_SPLICING) to indexsize_nt */
	    while (prevhit != -1 && (prevposition = /*mappings[prev_querypos]*/prev_mappings[prevhit]) + indexsize_nt <= position) {
	      prevlink = &(/*links[prev_querypos]*/prev_links[prevhit]);

	      gendistance = position - prevposition;
	      /* was abs(gendistance - querydistance) */
	      diffdistance = gendistance > querydistance ? (gendistance - querydistance) : (querydistance - gendistance);

#ifdef BAD_GMAX
	      fwd_score = prevlink->fwd_score + querydist_credit - (diffdistance/ONE + 1) /*- querydist_penalty*/;
#else
	      /* diffdistance <= EQUAL_DISTANCE_NOT_SPLICING */
	      /* This is how version 2013-08-14 did it */
	      fwd_score = fwd_scores[prev_querypos][prevhit] + CONSEC_POINTS_PER_MATCH;
#endif
	  
	      debug9(printf("\tD4, hit %d. Fwd mismatch qpos %d,%d at %ux%d (score = %d -> %d, consec = %d (from #%d), intr = %d-%d-%d, gendist %u, querydist %d, canonicalsgn %d)",
			    hiti,prev_querypos,prevhit,prevposition,active[prev_querypos][prevhit],
			    fwd_scores[prev_querypos][prevhit],fwd_score,prevlink->fwd_consecutive,prevlink->fwd_tracei,
			    best_fwd_intronnfwd,best_fwd_intronnrev,best_fwd_intronnunk,
			    gendistance-indexsize_nt,querydistance-indexsize_nt,canonicalsgn));
	    
	      /* Disallow ties, which should favor adjacent */
	      if (fwd_score > best_fwd_score) {
		if (diffdistance <= EQUAL_DISTANCE_FOR_CONSECUTIVE) {
		  best_fwd_consecutive = prevlink->fwd_consecutive + querydistance;
		} else {
		  best_fwd_consecutive = 0;
		}
		best_fwd_rootposition = prevlink->fwd_rootposition;
		best_fwd_score = fwd_score;
		best_fwd_prevpos = prev_querypos;
		best_fwd_prevhit = prevhit;
		/* best_fwd_tracei = ++*fwd_tracei; */
		best_fwd_tracei = prevlink->fwd_tracei; /* Keep previous trace, as in range 3 */
#ifdef DEBUG9
		best_fwd_intronnfwd = prevlink->fwd_intronnfwd;
		best_fwd_intronnrev = prevlink->fwd_intronnrev;
		best_fwd_intronnunk = prevlink->fwd_intronnunk;
		switch (canonicalsgn) {
		case 1: best_fwd_intronnfwd++; break;
		case 0: best_fwd_intronnunk++; break;
		}
#endif
		debug9(printf(" => Best fwd at %d (consec = %d)\n",fwd_score,best_fwd_consecutive));
	      } else {
		debug9(printf(" => Loses to %d\n",best_fwd_score));
	      }

	      prevhit = /*active[prev_querypos]*/prev_active[prevhit];
	    }
	  }
	}
      }
      
      /* Best_score needs to beat something positive to prevent a
	 small local extension from beating a good canonical intron.
	 If querypos is too small, don't insert an intron.  */
      /* linksconsecutive already assigned above */
      currlink = &(links[curr_querypos][hiti + low_hit]);
      currlink->fwd_consecutive = best_fwd_consecutive;
      currlink->fwd_rootposition = best_fwd_rootposition;
      currlink->fwd_pos = best_fwd_prevpos;
      currlink->fwd_hit = best_fwd_prevhit;
      if (currlink->fwd_pos >= 0) {
	currlink->fwd_tracei = best_fwd_tracei;
	fwd_scores[curr_querypos][hiti + low_hit] = best_fwd_score;
#ifdef MOVE_TO_STAGE3
      } else if (anchoredp == true) {
	currlink->fwd_tracei = -1;
	fwd_scores[curr_querypos][hiti + low_hit] = -100000;
#endif
      } else if (localp == true) {
	currlink->fwd_tracei = ++*fwd_tracei;
	fwd_scores[curr_querypos][hiti + low_hit] = indexsize_nt;
      } else {
	currlink->fwd_tracei = ++*fwd_tracei;
	fwd_scores[curr_querypos][hiti + low_hit] = best_fwd_score;
      }

#ifdef DEBUG9
      currlink->fwd_intronnfwd = best_fwd_intronnfwd;
      currlink->fwd_intronnrev = best_fwd_intronnrev;
      currlink->fwd_intronnunk = best_fwd_intronnunk;
#endif

      debug9(printf("\tChose %d,%d with score %d (fwd) => trace #%d\n",
		    currlink->fwd_pos,currlink->fwd_hit,fwd_scores[curr_querypos][hiti + low_hit],currlink->fwd_tracei));
      debug3(printf("%d %d  %d %d  1\n",querypos,hit,best_prevpos,best_prevhit));
    }

    FREEA(frontier);
  }

  return;
}


static void
score_querypos_lookforward_one (int *fwd_tracei, Link_T currlink, int curr_querypos, int currhit,
				unsigned int position,
				struct Link_T **links, int **fwd_scores,
				Chrpos_T **mappings, int **active, int *firstactive,
				Genome_T genome, Genome_T genomealt,
				Univcoord_T chroffset, Univcoord_T chrhigh, bool plusp,
				int indexsize, Intlist_T processed,
#ifdef MOVE_TO_STAGE3
				bool anchoredp,
#endif
				bool localp, bool splicingp,
				bool use_canonical_p, int non_canonical_penalty) {
  Link_T prevlink;
  struct Link_T *prev_links;
  Chrpos_T *prev_mappings;
  int *prev_active;

  int best_fwd_consecutive = indexsize*NT_PER_MATCH;
  int best_fwd_rootposition = position;
  int best_fwd_score = 0, fwd_score;
  int best_fwd_prevpos = -1, best_fwd_prevhit = -1;
  int best_fwd_tracei, last_tracei;
#ifdef DEBUG9
  int best_fwd_intronnfwd = 0, best_fwd_intronnrev = 0, best_fwd_intronnunk = 0;
  int canonicalsgn = 0;
#endif
  bool donep;
  int prev_querypos, prevhit;
  Chrpos_T prevposition;
  int gendistance;
  Univcoord_T prevpos, currpos;
  int querydistance, diffdistance, lookback, nlookback, nseen, indexsize_nt;
  /* int querydist_penalty; */
  int querydist_credit;
  /* bool near_end_p; */
  bool canonicalp;

#ifdef PMAP
  indexsize_nt = indexsize*3;
#else
  indexsize_nt = indexsize;
#endif
  /* indexsize_query = indexsize; */	/* Use when evaluating across query positions */


  /* Parameters for section D, assuming adjacent is false */
  /* adjacentp = false; */
  nlookback = nsufflookback;
  lookback = sufflookback;

  /* A. Evaluate adjacent position (at last one processed) */
  if (processed != NULL) {
    prev_querypos = Intlist_head(processed);
    prev_mappings = mappings[prev_querypos];
    prev_links = links[prev_querypos];
    prev_active = active[prev_querypos];

#ifdef PMAP
    querydistance = (prev_querypos - curr_querypos)*3;
#else
    querydistance = prev_querypos - curr_querypos;
#endif
    prevhit = firstactive[prev_querypos];
    prevposition = position;	/* Prevents prevposition == position + querydistance */
    while (prevhit != -1 && (prevposition = /*mappings[prev_querypos]*/prev_mappings[prevhit]) > position + querydistance) {
      prevhit = /*active[prev_querypos]*/prev_active[prevhit];
    }
    if (prevposition == position + querydistance) {
      prevlink = &(/*links[prev_querypos]*/prev_links[prevhit]);
      best_fwd_consecutive = prevlink->fwd_consecutive + querydistance;
      /* best_fwd_rootnlinks = prevlink->fwd_rootnlinks + 1; */
      best_fwd_rootposition = prevlink->fwd_rootposition;
      best_fwd_score = fwd_scores[prev_querypos][prevhit] + CONSEC_POINTS_PER_MATCH*querydistance;
      
      best_fwd_prevpos = prev_querypos;
      best_fwd_prevhit = prevhit;
      best_fwd_tracei = prevlink->fwd_tracei;
#ifdef DEBUG9
      best_fwd_intronnfwd = prevlink->fwd_intronnfwd;
      best_fwd_intronnrev = prevlink->fwd_intronnrev;
      best_fwd_intronnunk = prevlink->fwd_intronnunk;
#endif
      /* adjacentp = true; */
      /* Parameters for section D when adjacent is true */
      nlookback = 1;
      lookback = sufflookback/2;

      debug9(printf("\tA. Adjacent qpos %d,%d at %ux%d (scores = %d -> %d, consec = %d (from #%d), intr = %d-%d-%d)\n",
		    prev_querypos,prevhit,prevposition,active[prev_querypos][prevhit],fwd_scores[prev_querypos][prevhit],
		    best_fwd_score,best_fwd_consecutive,best_fwd_tracei,
		    best_fwd_intronnfwd,best_fwd_intronnrev,best_fwd_intronnunk));
    }
  }

  /* Check work list */
#ifdef MOVE_TO_STAGE3
  if (anchoredp && curr_querypos + indexsize_query >= queryend) {
    /* Allow close prevpositions that overlap with anchor */
    /* Can give rise to false positives, and increases amount of dynamic programming work */
    debug9(printf("No skipping because close to anchor\n"));
  } else if (0 && anchoredp && curr_querypos == querystart) {
    /* Test end position */
  } else if (0) {
    while (processed != NULL && (prev_querypos = Intlist_head(processed)) < curr_querypos + indexsize_query) {
      debug9(printf("Skipping prev_querypos %d, because too close\n",prev_querypos));
      processed = Intlist_next(processed);
    }
  }
#endif

  /* D. Evaluate for mismatches (all other previous querypos) */
  donep = false;
  nseen = 0; 
  last_tracei = -1;
  for ( ; processed != NULL && best_fwd_consecutive < ENOUGH_CONSECUTIVE && donep == false;
	processed = Intlist_next(processed), nseen++) {
    prev_querypos = Intlist_head(processed);

#ifdef PMAP
    querydistance = (prev_querypos - curr_querypos)*3;
#else
    querydistance = prev_querypos - curr_querypos;
#endif

    if (nseen > nlookback && querydistance - indexsize_nt > lookback) {
      donep = true;
    }

    if ((prevhit = firstactive[prev_querypos]) != -1) {
      /* querydist_penalty = (querydistance - indexsize_nt)/QUERYDIST_PENALTY_FACTOR; */
      /* Actually a querydist_penalty */
      querydist_credit = -querydistance/indexsize_nt;

      prev_mappings = mappings[prev_querypos];
      prev_links = links[prev_querypos];
      prev_active = active[prev_querypos];

      /* Range 0 */
      while (prevhit != -1 && prev_links[prevhit].fwd_tracei == last_tracei) {
	debug9(printf("Skipping querypos %d with tracei #%d\n",prev_querypos,prev_links[prevhit].fwd_tracei));
	prevhit = /*active[prev_querypos]*/prev_active[prevhit];
      }
      if (prevhit != -1) {
	last_tracei = prev_links[prevhit].fwd_tracei;
      }

      /* Range 1: From Infinity to maxintronlen */
      if (splicingp == true) {
	/* This is equivalent to diffdistance >= maxintronlen, where
	   diffdistance = abs(gendistance - querydistance) and
	   gendistance = (position - prevposition - indexsize_nt) */
	while (prevhit != -1 && (prevposition = /*mappings[prev_querypos]*/prev_mappings[prevhit]) >= position + maxintronlen + querydistance) {
	  /* Skip */
	  /* printf("fwd: prevposition %u, prevhit %d\n",prevposition,prevhit); */
	  prevhit = /*active[prev_querypos]*/prev_active[prevhit];
	}
      }

      /* Range 2: From maxintronlen to (prev_querypos + EQUAL_DISTANCE_NOT_SPLICING) */
      /* This is equivalent to +diffdistance > EQUAL_DISTANCE_NOT_SPLICING */
      while (prevhit != -1 && (prevposition = /*mappings[prev_querypos]*/prev_mappings[prevhit]) > position + EQUAL_DISTANCE_NOT_SPLICING + querydistance) {
	/* printf("fwd: prevposition %u, prevhit %d\n",prevposition,prevhit); */
	prevlink = &(/*links[prev_querypos]*/prev_links[prevhit]);

	gendistance = prevposition - position;
	assert(gendistance > querydistance); /* True because gendistance > EQUAL_DISTANCE_NOT_SPLICING + querydistance */
	diffdistance = gendistance - querydistance; /* No need for abs() */

	fwd_score = fwd_scores[prev_querypos][prevhit] + querydist_credit /*- querydist_penalty*/;
	if (splicingp == true) {
	  fwd_score -= (diffdistance/TEN_THOUSAND + 1);
	} else {
	  fwd_score -= (diffdistance/ONE + 1);
	}

	if (use_canonical_p == true) {

	  /* prevpos is higher genomic coordinate than currpos */
	  /* need to add to position and prevposition to compensate for greedy matches */
	  /* need to subtract from position and prevposition to compensate for missed matches */
	  if (plusp == true) {
	    prevpos = chroffset + prevposition;
	    currpos = chroffset + position + querydistance;
	    if (currpos < MISS_BEHIND || prevpos < MISS_BEHIND) {
	      canonicalp = false;
	    } else if (Genome_sense_canonicalp(genome,genomealt,
					       /*donor_rightbound*/currpos + GREEDY_ADVANCE,
					       /*donor_leftbound*/currpos - MISS_BEHIND,
					       /*acceptor_rightbound*/prevpos + GREEDY_ADVANCE,
					       /*acceptor_leftbound*/prevpos - MISS_BEHIND,
					       chroffset) == true) {
	      debug9(printf("lookforward plus: sense canonical\n"));
	      canonicalp = true;
	    } else if (Genome_antisense_canonicalp(genome,genomealt,
						   /*donor_rightbound*/prevpos + GREEDY_ADVANCE,
						   /*donor_leftbound*/prevpos - MISS_BEHIND,
						   /*acceptor_rightbound*/currpos + GREEDY_ADVANCE,
						   /*acceptor_leftbound*/currpos - MISS_BEHIND,
						   chroffset) == true) {
	      debug9(printf("lookforward plus: antisense canonical\n"));
	      canonicalp = true;
	    } else {
	      debug9(printf("lookforward plus: not canonical\n"));
	      canonicalp = false;
	    }
	      
	  } else {
	    prevpos = chrhigh + 1 - prevposition;
	    currpos = chrhigh + 1 - position - querydistance;
	    if (prevpos < GREEDY_ADVANCE || currpos < GREEDY_ADVANCE) {
	      canonicalp = false;
	    } else if (Genome_sense_canonicalp(genome,genomealt,
					       /*donor_rightbound*/prevpos + MISS_BEHIND,
					       /*donor_leftbound*/prevpos - GREEDY_ADVANCE,
					       /*acceptor_rightbound*/currpos + MISS_BEHIND,
					       /*acceptor_leftbound*/currpos - GREEDY_ADVANCE,
					       chroffset) == true) {
	      debug9(printf("lookforward minus: sense canonical\n"));
	      canonicalp = true;
	    } else if (Genome_antisense_canonicalp(genome,genomealt,
						   /*donor_rightbound*/currpos + MISS_BEHIND,
						   /*donor_leftbound*/currpos - GREEDY_ADVANCE,
						   /*acceptor_rightbound*/prevpos + MISS_BEHIND,
						   /*acceptor_leftbound*/prevpos - GREEDY_ADVANCE,
						   chroffset) == true) {
	      debug9(printf("lookforward minus: antisense canonical\n"));
	      canonicalp = true;
	    } else {
	      debug9(printf("lookforward minus: not canonical\n"));
	      canonicalp = false;
	    }
	  }

	  if (canonicalp == true) {
	    debug9(canonicalsgn = +1);
	  } else {
	    debug9(canonicalsgn = 0);
	    fwd_score -= non_canonical_penalty;
	  }
	}

	debug9(printf("\tD2. Fwd mismatch qpos %d,%d at %ux%d (score = %d -> %d, consec = %d (from #%d), intr = %d-%d-%d, gendist %u, querydist %d, canonicalsgn %d)",
		      prev_querypos,prevhit,prevposition,active[prev_querypos][prevhit],
		      fwd_scores[prev_querypos][prevhit],fwd_score,prevlink->fwd_consecutive,prevlink->fwd_tracei,
		      best_fwd_intronnfwd,best_fwd_intronnrev,best_fwd_intronnunk,
		      gendistance-indexsize_nt,querydistance-indexsize_nt,canonicalsgn));
	    
	/* Disallow ties, which should favor adjacent */
	if (fwd_score > best_fwd_score) {
	  if (diffdistance <= EQUAL_DISTANCE_FOR_CONSECUTIVE) {
	    best_fwd_consecutive = prevlink->fwd_consecutive + querydistance;
	    /* best_fwd_rootnlinks = prevlink->fwd_rootnlinks + 1; */
	  } else {
	    best_fwd_consecutive = 0;
	    /* best_fwd_rootnlinks = 1; */
	  }
	  best_fwd_rootposition = prevlink->fwd_rootposition;
	  best_fwd_score = fwd_score;
	  best_fwd_prevpos = prev_querypos;
	  best_fwd_prevhit = prevhit;
	  best_fwd_tracei = ++*fwd_tracei;
#ifdef DEBUG9
	  best_fwd_intronnfwd = prevlink->fwd_intronnfwd;
	  best_fwd_intronnrev = prevlink->fwd_intronnrev;
	  best_fwd_intronnunk = prevlink->fwd_intronnunk;
	  switch (canonicalsgn) {
	  case 1: best_fwd_intronnfwd++; break;
	  case 0: best_fwd_intronnunk++; break;
	  }
#endif
	  debug9(printf(" => Best fwd at %d (consec = %d)\n",fwd_score,best_fwd_consecutive));
	} else {
	  debug9(printf(" => Loses to %d\n",best_fwd_score));
	}

	prevhit = /*active[prev_querypos]*/prev_active[prevhit];
      }

      /* Scoring appears to be the same as for range 4, which is rarely called, so including in range 4 */
      /* Range 3: From (querypos + EQUAL_DISTANCE_NOT_SPLICING) to (querypos - EQUAL_DISTANCE_NOT_SPLICING) */
      /* This is equivalent to -diffdistance > EQUAL_DISTANCE_NOT_SPLICING && prevposition + indexsize_nt <= position */

      /* Range 4: From (prev_querypos - EQUAL_DISTANCE_NOT_SPLICING) to indexsize_nt */
      while (prevhit != -1 && (prevposition = /*mappings[prev_querypos]*/prev_mappings[prevhit]) >= position + indexsize_nt) {
	/* printf("fwd: prevposition %u, prevhit %d\n",prevposition,prevhit); */
	prevlink = &(/*links[prev_querypos]*/prev_links[prevhit]);

	gendistance = prevposition - position;
	/* was abs(gendistance - querydistance) */
	diffdistance = gendistance > querydistance ? (gendistance - querydistance) : (querydistance - gendistance);

#ifdef BAD_GMAX
	fwd_score = prevlink->fwd_score + querydist_credit - (diffdistance/ONE + 1) /*- querydist_penalty*/;
#else
	/* diffdistance <= EQUAL_DISTANCE_NOT_SPLICING */
	/* This is how version 2013-08-14 did it */
	fwd_score = fwd_scores[prev_querypos][prevhit] + CONSEC_POINTS_PER_MATCH;
#endif
#if 0
	if (/*near_end_p == false &&*/ prevlink->fwd_consecutive < EXON_DEFN) {
	  fwd_score -= NINTRON_PENALTY_MISMATCH;
	}
#endif

	debug9(printf("\tD4. Fwd mismatch qpos %d,%d at %ux%d (score = %d -> %d, consec = %d (from #%d), intr = %d-%d-%d, gendist %u, querydist %d, canonicalsgn %d)",
		      prev_querypos,prevhit,prevposition,active[prev_querypos][prevhit],
		      fwd_scores[prev_querypos][prevhit],fwd_score,prevlink->fwd_consecutive,prevlink->fwd_tracei,
		      best_fwd_intronnfwd,best_fwd_intronnrev,best_fwd_intronnunk,
		      gendistance-indexsize_nt,querydistance-indexsize_nt,canonicalsgn));
	    
	/* Disallow ties, which should favor adjacent */
	if (fwd_score > best_fwd_score) {
	  if (diffdistance <= EQUAL_DISTANCE_FOR_CONSECUTIVE) {
	    best_fwd_consecutive = prevlink->fwd_consecutive + querydistance;
	    /* best_fwd_rootnlinks = prevlink->fwd_rootnlinks + 1; */
	  } else {
	    best_fwd_consecutive = 0;
	    /* best_fwd_rootnlinks = 1; */
	  }
	  best_fwd_rootposition = prevlink->fwd_rootposition;
	  best_fwd_score = fwd_score;
	  best_fwd_prevpos = prev_querypos;
	  best_fwd_prevhit = prevhit;
	  /* best_fwd_tracei = ++*fwd_tracei; */
	  best_fwd_tracei = prevlink->fwd_tracei; /* Keep previous trace, as in range 3 */
#ifdef DEBUG9
	  best_fwd_intronnfwd = prevlink->fwd_intronnfwd;
	  best_fwd_intronnrev = prevlink->fwd_intronnrev;
	  best_fwd_intronnunk = prevlink->fwd_intronnunk;
	  switch (canonicalsgn) {
	  case 1: best_fwd_intronnfwd++; break;
	  case 0: best_fwd_intronnunk++; break;
	  }
#endif
	  debug9(printf(" => Best fwd at %d (consec = %d)\n",fwd_score,best_fwd_consecutive));
	} else {
	  debug9(printf(" => Loses to %d\n",best_fwd_score));
	}

	prevhit = /*active[prev_querypos]*/prev_active[prevhit];
      }
    }
  }

  /* Best_score needs to beat something positive to prevent a
     small local extension from beating a good canonical intron.
     If querypos is too small, don't insert an intron.  */
  /* linksconsecutive already assigned above */
  currlink->fwd_consecutive = best_fwd_consecutive;
  currlink->fwd_rootposition = best_fwd_rootposition;
  /* currlink->fwd_rootnlinks = best_fwd_rootnlinks; */
  currlink->fwd_pos = best_fwd_prevpos;
  currlink->fwd_hit = best_fwd_prevhit;
  if (currlink->fwd_pos >= 0) {
    currlink->fwd_tracei = best_fwd_tracei;
    fwd_scores[curr_querypos][currhit] = best_fwd_score;
#ifdef MOVE_TO_STAGE3
  } else if (anchoredp == true) {
    currlink->fwd_tracei = -1;
    fwd_scores[curr_querypos][currhit] = -100000;
#endif
  } else if (localp == true) {
    currlink->fwd_tracei = ++*fwd_tracei;
    fwd_scores[curr_querypos][currhit] = indexsize_nt;
  } else {
    currlink->fwd_tracei = ++*fwd_tracei;
    fwd_scores[curr_querypos][currhit] = best_fwd_score;
  }

#ifdef DEBUG9
  currlink->fwd_intronnfwd = best_fwd_intronnfwd;
  currlink->fwd_intronnrev = best_fwd_intronnrev;
  currlink->fwd_intronnunk = best_fwd_intronnunk;
#endif

  debug9(printf("\tChose %d,%d with score %d (fwd) => trace #%d\n",
		currlink->fwd_pos,currlink->fwd_hit,fwd_scores[curr_querypos][currhit],currlink->fwd_tracei));
  debug3(printf("%d %d  %d %d  1\n",querypos,hit,best_prevpos,best_prevhit));

  return;
}


static void
score_querypos_lookforward_mult (int *fwd_tracei, int low_hit, int high_hit, int curr_querypos,
				 unsigned int *positions,
				 struct Link_T **links, int **fwd_scores,
				 Chrpos_T **mappings, int **active, int *firstactive,
				 Genome_T genome, Genome_T genomealt,
				 Univcoord_T chroffset, Univcoord_T chrhigh, bool plusp,
				 int indexsize, Intlist_T processed,
#ifdef MOVE_TO_STAGE3
				 bool anchoredp,
#endif
				 bool localp, bool splicingp,
				 bool use_canonical_p, int non_canonical_penalty) {
  Link_T prevlink, currlink;
  Intlist_T last_item, p;
  int nhits = high_hit - low_hit, nprocessed, hiti;

  struct Link_T *prev_links, *adj_links;
  Chrpos_T *prev_mappings, *adj_mappings;
  int *prev_active, *adj_active;

  int overall_fwd_consecutive, best_fwd_consecutive;
  int best_fwd_rootposition;
  int best_fwd_score, fwd_score;
  int best_fwd_prevpos, best_fwd_prevhit;
  int best_fwd_tracei, last_tracei;
#ifdef DEBUG9
  int best_fwd_intronnfwd, best_fwd_intronnrev, best_fwd_intronnunk;
  int canonicalsgn = 0;
#endif
  int adj_querypos, adj_querydistance, prev_querypos, prevhit, adj_frontier, *frontier;
  Chrpos_T prevposition, position;
  int gendistance;
  Univcoord_T prevpos, currpos;
  int querydistance, diffdistance, indexsize_nt;
  int max_nseen, max_adjacent_nseen, max_nonadjacent_nseen, nseen;
  int querydist_credit;
  bool canonicalp;

#ifdef PMAP
  indexsize_nt = indexsize*3;
#else
  indexsize_nt = indexsize;
#endif
  /* indexsize_query = indexsize; */  /* Use when evaluating across query positions */


  /* Determine work load */
  /* printf("Work load (lookforward): %s\n",Intlist_to_string(processed)); */
  last_item = processed;
#ifdef MOVE_TO_STAGE3
  if (anchoredp && curr_querypos + indexsize_query >= queryend) {
    /* Allow close prevpositions that overlap with anchor */
    /* Can give rise to false positives, and increases amount of dynamic programming work */
    /* debug9(printf("No skipping because close to anchor\n")); */
  } else if (0 && anchoredp && curr_querypos == querystart) {
    /* Test end position */
  } else if (0) {
    while (processed != NULL && (prev_querypos = Intlist_head(processed)) < curr_querypos + indexsize_query) {
      debug9(printf("Skipping prev_querypos %d, because too close\n",prev_querypos));
      processed = Intlist_next(processed);
    }
  }
#endif

  if (last_item == NULL) {
    for (hiti = nhits - 1; hiti >= 0; hiti--) {
      currlink = &(links[curr_querypos][hiti + low_hit]);

      currlink->fwd_consecutive = /*best_fwd_consecutive =*/ indexsize*NT_PER_MATCH;
      currlink->fwd_rootposition = /*best_fwd_rootposition =*/ positions[hiti];
      currlink->fwd_pos = /*best_fwd_prevpos =*/ -1;
      currlink->fwd_hit = /*best_fwd_prevhit =*/ -1;

#ifdef MOVE_TO_STAGE3
      if (anchoredp == true) {
	currlink->fwd_tracei = -1;
	fwd_scores[curr_querypos][hiti + low_hit] = -100000;
      } else
#endif
      if (localp == true) {
	currlink->fwd_tracei = ++*fwd_tracei;
	fwd_scores[curr_querypos][hiti + low_hit] = indexsize_nt;
      } else {
	fwd_scores[curr_querypos][hiti + low_hit] = /*best_fwd_score =*/ 0;
      }
    }

  } else if (processed == NULL) {
    /* A. Evaluate adjacent position (at last one processed, if available).  Don't evaluate for mismatches (D). */
    adj_querypos = Intlist_head(last_item);
    adj_links = links[adj_querypos];
    adj_mappings = mappings[adj_querypos];
    adj_active = active[adj_querypos];

#ifdef PMAP
    adj_querydistance = (adj_querypos - curr_querypos)*3;
#else
    adj_querydistance = adj_querypos - curr_querypos;
#endif

    /* Process prevhit and hiti in parallel.  Values are descending along prevhit chain and from nhits-1 to 0. */
    prevhit = firstactive[adj_querypos];
    hiti = nhits - 1;
    while (prevhit != -1 && hiti >= 0) {
      if ((prevposition = /*mappings[adj_querypos]*/adj_mappings[prevhit]) > (position = positions[hiti]) + adj_querydistance) {
	prevhit = /*active[adj_querypos]*/adj_active[prevhit];

      } else if (prevposition < position + adj_querydistance) {
	/* Adjacent position not found for hiti */
	currlink = &(links[curr_querypos][hiti + low_hit]);

	currlink->fwd_consecutive = /*best_fwd_consecutive =*/ indexsize*NT_PER_MATCH;
	currlink->fwd_rootposition = /*best_fwd_rootposition =*/ positions[hiti];
	currlink->fwd_pos = /*best_fwd_prevpos =*/ -1;
	currlink->fwd_hit = /*best_fwd_prevhit =*/ -1;

#ifdef MOVE_TO_STAGE3
	if (anchoredp == true) {
	  currlink->fwd_tracei = -1;
	  fwd_scores[curr_querypos][hiti + low_hit] = -100000;
	} else
#endif
        if (localp == true) {
	  currlink->fwd_tracei = ++*fwd_tracei;
	  fwd_scores[curr_querypos][hiti + low_hit] = indexsize_nt;
	} else {
	  fwd_scores[curr_querypos][hiti + low_hit] = /*best_fwd_score =*/ 0;
	}

	hiti--;
	
      } else {
	/* Adjacent position found for hiti */
	currlink = &(links[curr_querypos][hiti + low_hit]);
	prevlink = &(/*links[adj_querypos]*/adj_links[prevhit]);

	currlink->fwd_consecutive = /*best_fwd_consecutive =*/ prevlink->fwd_consecutive + adj_querydistance;
	currlink->fwd_rootposition = /*best_fwd_rootposition =*/ prevlink->fwd_rootposition;
	currlink->fwd_pos = /*best_fwd_prevpos =*/ adj_querypos;
	currlink->fwd_hit = /*best_fwd_prevhit =*/ prevhit;
	fwd_scores[curr_querypos][hiti + low_hit] = /*best_fwd_score =*/ fwd_scores[adj_querypos][prevhit] + CONSEC_POINTS_PER_MATCH*adj_querydistance;

#ifdef DEBUG9
	printf("\tA(3). For hit %d, adjacent qpos %d,%d at %ux%d (scores = %d -> %d, consec = %d (from #%d), intr = %d-%d-%d)\n",
	       hiti,adj_querypos,prevhit,prevposition,active[adj_querypos][prevhit],fwd_scores[adj_querypos][prevhit],
	       fwd_scores[curr_querypos][hiti + low_hit],currlink->fwd_consecutive,/*best_fwd_tracei*/prevlink->fwd_tracei,
	       /*best_fwd_intronnfwd*/prevlink->fwd_intronnfwd,
	       /*best_fwd_intronnrev*/prevlink->fwd_intronnrev,
	       /*best_fwd_intronnunk*/prevlink->fwd_intronnunk);
#endif

	prevhit = /*active[adj_querypos]*/adj_active[prevhit];
	hiti--;
      }
    }

    while (hiti >= 0) {
      /* Adjacent position not found for hiti */
      currlink = &(links[curr_querypos][hiti + low_hit]);

      currlink->fwd_consecutive = /*best_fwd_consecutive =*/ indexsize*NT_PER_MATCH;
      currlink->fwd_rootposition = /*best_fwd_rootposition =*/ positions[hiti];
      currlink->fwd_pos = /*best_fwd_prevpos =*/ -1;
      currlink->fwd_hit = /*best_fwd_prevhit =*/ -1;

#ifdef MOVE_TO_STAGE3
      if (anchoredp == true) {
	currlink->fwd_tracei = -1;
	fwd_scores[curr_querypos][hiti + low_hit] = -100000;
      } else
#endif
      if (localp == true) {
	currlink->fwd_tracei = ++*fwd_tracei;
	fwd_scores[curr_querypos][hiti + low_hit] = indexsize_nt;
      } else {
	fwd_scores[curr_querypos][hiti + low_hit] = /*best_fwd_score =*/ 0;
      }

      hiti--;
    }

  } else {
    adj_querypos = Intlist_head(last_item);
    adj_links = links[adj_querypos];
    adj_mappings = mappings[adj_querypos];
    adj_active = active[adj_querypos];

#ifdef PMAP
    adj_querydistance = (adj_querypos - curr_querypos)*3;
#else
    adj_querydistance = adj_querypos - curr_querypos;
#endif

    nprocessed = Intlist_length(processed);
    frontier = (int *) MALLOCA(nprocessed * sizeof(int));

    nseen = 0;
    for (p = processed; p != NULL; p = Intlist_next(p)) {
      prev_querypos = Intlist_head(p);

      querydistance = prev_querypos - curr_querypos;
      if (nseen <= /*nlookback*/1 || querydistance - indexsize_nt <= /*lookback*/sufflookback/2) {
	max_adjacent_nseen = nseen;
      }
      if (nseen <= /*nlookback*/nsufflookback || querydistance - indexsize_nt <= /*lookback*/sufflookback) {
	max_nonadjacent_nseen = nseen;
      }

      frontier[nseen++] = firstactive[prev_querypos];
    }


    /* Look for overall_fwd_consecutive to see whether we can be greedy */
    overall_fwd_consecutive = 0;
    adj_frontier = firstactive[adj_querypos];
    for (hiti = nhits - 1; hiti >= 0; hiti--) {
      position = positions[hiti];

      /* A. Evaluate adjacent position (at last one processed) */
      prevhit = adj_frontier;	/* Get information from last hiti */
      prevposition = position;	/* Prevents prevposition == position + adj_querydistance */
      while (prevhit != -1 && (prevposition = /*mappings[adj_querypos]*/adj_mappings[prevhit]) > position + adj_querydistance) {
	prevhit = /*active[adj_querypos]*/adj_active[prevhit];
      }
      adj_frontier = prevhit;	/* Save information for next hiti */

      if (prevposition == position + adj_querydistance) {
	/* Adjacent found */
	prevlink = &(/*links[adj_querypos]*/adj_links[prevhit]);
	if (prevlink->fwd_consecutive + adj_querydistance > overall_fwd_consecutive) {
	  overall_fwd_consecutive = prevlink->fwd_consecutive + adj_querydistance;
	}
      }
    }
    debug(printf("(2) Overall fwd consecutive is %d\n",overall_fwd_consecutive));


    /* Now process */
    adj_frontier = firstactive[adj_querypos];
    for (hiti = nhits - 1; hiti >= 0; hiti--) {
      position = positions[hiti];

      /* A. Evaluate adjacent position (at last one processed) */
      prevhit = adj_frontier;	/* Get information from last hiti */
      prevposition = position;	/* Prevents prevposition == position + adj_querydistance */
      while (prevhit != -1 && (prevposition = /*mappings[adj_querypos]*/adj_mappings[prevhit]) > position + adj_querydistance) {
	prevhit = /*active[adj_querypos]*/adj_active[prevhit];
      }
      adj_frontier = prevhit;	/* Save information for next hiti */

      if (prevposition == position + adj_querydistance) {
	/* Adjacent found */
	prevlink = &(/*links[adj_querypos]*/adj_links[prevhit]);

	best_fwd_consecutive = prevlink->fwd_consecutive + adj_querydistance;
	best_fwd_rootposition = prevlink->fwd_rootposition;
	best_fwd_prevpos = adj_querypos;
	best_fwd_prevhit = prevhit;
	best_fwd_score = fwd_scores[adj_querypos][prevhit] + CONSEC_POINTS_PER_MATCH*adj_querydistance;
	max_nseen = max_adjacent_nseen;	/* Look not so far back */
	best_fwd_tracei = prevlink->fwd_tracei;

#ifdef DEBUG9
	best_fwd_intronnfwd = prevlink->fwd_intronnfwd;
	best_fwd_intronnrev = prevlink->fwd_intronnrev;
	best_fwd_intronnunk = prevlink->fwd_intronnunk;
#endif
	debug9(printf("\tA(4). For hit %d, adjacent qpos %d,%d at %ux%d (scores = %d -> %d, consec = %d (from #%d), intr = %d-%d-%d)\n",
		      hiti,adj_querypos,prevhit,prevposition,active[adj_querypos][prevhit],fwd_scores[adj_querypos][prevhit],
		      best_fwd_score,best_fwd_consecutive,/*best_fwd_tracei*/prevlink->fwd_tracei,
		      best_fwd_intronnfwd,best_fwd_intronnrev,best_fwd_intronnunk));
      } else {
	/* Adjacent not found */
	best_fwd_consecutive = indexsize*NT_PER_MATCH;
	best_fwd_rootposition = position;
	best_fwd_prevpos = -1;
	best_fwd_prevhit = -1;
	best_fwd_score = 0;
	max_nseen = max_nonadjacent_nseen; /* Look farther back */
	best_fwd_tracei = -1;

#ifdef DEBUG9
	best_fwd_intronnfwd = 0;
	best_fwd_intronnrev = 0;
	best_fwd_intronnunk = 0;
#endif
      }

      if (overall_fwd_consecutive < GREEDY_NCONSECUTIVE) {
	/* D. Evaluate for mismatches (all other previous querypos) */
	nseen = 0;
	last_tracei = -1;
	for (p = processed; p != NULL && best_fwd_consecutive < ENOUGH_CONSECUTIVE && nseen <= max_nseen;
	     p = Intlist_next(p), nseen++) {

	  /* Making this check helps with efficiency */
	  if ((prevhit = frontier[nseen]) != -1) {	/* Retrieve starting point from last hiti */
	    prev_querypos = Intlist_head(p);
#ifdef PMAP
	    querydistance = (prev_querypos - curr_querypos)*3;
#else
	    querydistance = prev_querypos - curr_querypos;
#endif
	    /* Actually a querydist_penalty */
	    querydist_credit = -querydistance/indexsize_nt;
	    
	    prev_mappings = mappings[prev_querypos];
	    prev_links = links[prev_querypos];
	    prev_active = active[prev_querypos];

	    /* Range 0 */
	    while (prevhit != -1 && prev_links[prevhit].fwd_tracei == last_tracei) {
	      debug9(printf("Skipping querypos %d with tracei #%d\n",prev_querypos,prev_links[prevhit].fwd_tracei));
	      prevhit = /*active[prev_querypos]*/prev_active[prevhit];
	    }
	    if (prevhit != -1) {
	      last_tracei = prev_links[prevhit].fwd_tracei;
	    }

	    /* Range 1: From Infinity to maxintronlen.  To be skipped.
	       This is equivalent to diffdistance >= maxintronlen, where
	       diffdistance = abs(gendistance - querydistance) and
	       gendistance = (position - prevposition - indexsize_nt) */
	    while (prevhit != -1 && (/*prevposition =*/ /*mappings[prev_querypos]*/prev_mappings[prevhit]) >= position + maxintronlen + querydistance) {
	      /* Accept within range 1 (ignore) */
	      prevhit = /*active[prev_querypos]*/prev_active[prevhit];
	    }
	    frontier[nseen] = prevhit;	/* Store as starting point for next hiti */
	    
	    
	    /* Range 2: From maxintronlen to (prev_querypos + EQUAL_DISTANCE_NOT_SPLICING) */
	    /* This is equivalent to +diffdistance > EQUAL_DISTANCE_NOT_SPLICING */
	    while (prevhit != -1 && (prevposition = /*mappings[prev_querypos]*/prev_mappings[prevhit]) > position + EQUAL_DISTANCE_NOT_SPLICING + querydistance) {
	      prevlink = &(/*links[prev_querypos]*/prev_links[prevhit]);
	      
	      gendistance = prevposition - position;
	      assert(gendistance > querydistance); /* True because gendistance > EQUAL_DISTANCE_NOT_SPLICING + querydistance */
	      diffdistance = gendistance - querydistance; /* No need for abs() */
	      
	      fwd_score = fwd_scores[prev_querypos][prevhit] + querydist_credit /*- querydist_penalty*/;
	      if (splicingp == true) {
		fwd_score -= (diffdistance/TEN_THOUSAND + 1);
	      } else {
		fwd_score -= (diffdistance/ONE + 1);
	      }
	      
	      if (use_canonical_p == true) {
		/* prevpos is higher genomic coordinate than currpos */
		/* need to add to position and prevposition to compensate for greedy matches */
		/* need to subtract from position and prevposition to compensate for missed matches */
		if (plusp == true) {
		  prevpos = chroffset + prevposition;
		  currpos = chroffset + position + querydistance;
		  if (currpos < MISS_BEHIND || prevpos < MISS_BEHIND) {
		    canonicalp = false;
		  } else if (Genome_sense_canonicalp(genome,genomealt,
						     /*donor_rightbound*/currpos + GREEDY_ADVANCE,
						     /*donor_leftbound*/currpos - MISS_BEHIND,
						     /*acceptor_rightbound*/prevpos + GREEDY_ADVANCE,
						     /*acceptor_leftbound*/prevpos - MISS_BEHIND,
						     chroffset) == true) {
		    debug9(printf("lookforward plus: sense canonical\n"));
		    canonicalp = true;
		  } else if (Genome_antisense_canonicalp(genome,genomealt,
							 /*donor_rightbound*/prevpos + GREEDY_ADVANCE,
							 /*donor_leftbound*/prevpos - MISS_BEHIND,
							 /*acceptor_rightbound*/currpos + GREEDY_ADVANCE,
							 /*acceptor_leftbound*/currpos - MISS_BEHIND,
							 chroffset) == true) {
		    debug9(printf("lookforward plus: antisense canonical\n"));
		    canonicalp = true;
		  } else {
		    debug9(printf("lookforward plus: not canonical\n"));
		    canonicalp = false;
		  }
	      
		} else {
		  prevpos = chrhigh + 1 - prevposition;
		  currpos = chrhigh + 1 - position - querydistance;
		  if (prevpos < GREEDY_ADVANCE || currpos < GREEDY_ADVANCE) {
		    canonicalp = false;
		  } else if (Genome_sense_canonicalp(genome,genomealt,
						     /*donor_rightbound*/prevpos + MISS_BEHIND,
						     /*donor_leftbound*/prevpos - GREEDY_ADVANCE,
						     /*acceptor_rightbound*/currpos + MISS_BEHIND,
						     /*acceptor_leftbound*/currpos - GREEDY_ADVANCE,
						     chroffset) == true) {
		    debug9(printf("lookforward minus: sense canonical\n"));
		    canonicalp = true;
		  } else if (Genome_antisense_canonicalp(genome,genomealt,
							 /*donor_rightbound*/currpos + MISS_BEHIND,
							 /*donor_leftbound*/currpos - GREEDY_ADVANCE,
							 /*acceptor_rightbound*/prevpos + MISS_BEHIND,
							 /*acceptor_leftbound*/prevpos - GREEDY_ADVANCE,
							 chroffset) == true) {
		    debug9(printf("lookforward minus: antisense canonical\n"));
		    canonicalp = true;
		  } else {
		    debug9(printf("lookforward minus: not canonical\n"));
		    canonicalp = false;
		  }
		}

		if (canonicalp == true) {
		  debug9(canonicalsgn = +1);
		} else {
		  debug9(canonicalsgn = 0);
		  fwd_score -= non_canonical_penalty;
		}
	      }

	      debug9(printf("\tD2, hit %d. Fwd mismatch qpos %d,%d at %ux%d (score = %d -> %d, consec = %d (from #%d), intr = %d-%d-%d, gendist %u, querydist %d, canonicalsgn %d)",
			    hiti,prev_querypos,prevhit,prevposition,active[prev_querypos][prevhit],
			    fwd_scores[prev_querypos][prevhit],fwd_score,prevlink->fwd_consecutive,prevlink->fwd_tracei,
			    best_fwd_intronnfwd,best_fwd_intronnrev,best_fwd_intronnunk,
			    gendistance-indexsize_nt,querydistance-indexsize_nt,canonicalsgn));
	    
	      /* Disallow ties, which should favor adjacent */
	      if (fwd_score > best_fwd_score) {
		if (diffdistance <= EQUAL_DISTANCE_FOR_CONSECUTIVE) {
		  best_fwd_consecutive = prevlink->fwd_consecutive + querydistance;
		} else {
		  best_fwd_consecutive = 0;
		}
		best_fwd_rootposition = prevlink->fwd_rootposition;
		best_fwd_score = fwd_score;
		best_fwd_prevpos = prev_querypos;
		best_fwd_prevhit = prevhit;
		best_fwd_tracei = ++*fwd_tracei;
#ifdef DEBUG9
		best_fwd_intronnfwd = prevlink->fwd_intronnfwd;
		best_fwd_intronnrev = prevlink->fwd_intronnrev;
		best_fwd_intronnunk = prevlink->fwd_intronnunk;
		switch (canonicalsgn) {
		case 1: best_fwd_intronnfwd++; break;
		case 0: best_fwd_intronnunk++; break;
		}
#endif
		debug9(printf(" => Best fwd at %d (consec = %d)\n",fwd_score,best_fwd_consecutive));
	      } else {
		debug9(printf(" => Loses to %d\n",best_fwd_score));
	      }

	      prevhit = /*active[prev_querypos]*/prev_active[prevhit];
	    }


	    /* Scoring appears to be the same as for range 4, which is rarely called, so including in range 4 */
	    /* Range 3: From (querypos + EQUAL_DISTANCE_NOT_SPLICING) to (querypos - EQUAL_DISTANCE_NOT_SPLICING) */
	    /* This is equivalent to -diffdistance > EQUAL_DISTANCE_NOT_SPLICING && prevposition + indexsize_nt <= position */


	    /* Range 4: From (prev_querypos - EQUAL_DISTANCE_NOT_SPLICING) to indexsize_nt */
	    while (prevhit != -1 && (prevposition = /*mappings[prev_querypos]*/prev_mappings[prevhit]) >= position + indexsize_nt) {
	      prevlink = &(/*links[prev_querypos]*/prev_links[prevhit]);

	      gendistance = prevposition - position;
	      /* was abs(gendistance - querydistance) */
	      diffdistance = gendistance > querydistance ? (gendistance - querydistance) : (querydistance - gendistance);

#ifdef BAD_GMAX
	      fwd_score = fwd_scores[prev_querypos][prevhit] + querydist_credit - (diffdistance/ONE + 1) /*- querydist_penalty*/;
#else
	      /* diffdistance <= EQUAL_DISTANCE_NOT_SPLICING */
	      /* This is how version 2013-08-14 did it */
	      fwd_score = fwd_scores[prev_querypos][prevhit] + CONSEC_POINTS_PER_MATCH;
#endif

	      debug9(printf("\tD4, hit %d. Fwd mismatch qpos %d,%d at %ux%d (score = %d -> %d, consec = %d (from #%d), intr = %d-%d-%d, gendist %u, querydist %d, canonicalsgn %d)",
			    hiti,prev_querypos,prevhit,prevposition,active[prev_querypos][prevhit],
			    fwd_scores[prev_querypos][prevhit],fwd_score,prevlink->fwd_consecutive,prevlink->fwd_tracei,
			    best_fwd_intronnfwd,best_fwd_intronnrev,best_fwd_intronnunk,
			    gendistance-indexsize_nt,querydistance-indexsize_nt,canonicalsgn));
	    
	      /* Disallow ties, which should favor adjacent */
	      if (fwd_score > best_fwd_score) {
		if (diffdistance <= EQUAL_DISTANCE_FOR_CONSECUTIVE) {
		  best_fwd_consecutive = prevlink->fwd_consecutive + querydistance;
		} else {
		  best_fwd_consecutive = 0;
		}
		best_fwd_rootposition = prevlink->fwd_rootposition;
		best_fwd_score = fwd_score;
		best_fwd_prevpos = prev_querypos;
		best_fwd_prevhit = prevhit;
		/* best_fwd_tracei = ++*fwd_tracei; */
		best_fwd_tracei = prevlink->fwd_tracei; /* Keep previous trace, as in range 3 */

#ifdef DEBUG9
		best_fwd_intronnfwd = prevlink->fwd_intronnfwd;
		best_fwd_intronnrev = prevlink->fwd_intronnrev;
		best_fwd_intronnunk = prevlink->fwd_intronnunk;
		switch (canonicalsgn) {
		case 1: best_fwd_intronnfwd++; break;
		case 0: best_fwd_intronnunk++; break;
		}
#endif
		debug9(printf(" => Best fwd at %d (consec = %d)\n",fwd_score,best_fwd_consecutive));
	      } else {
		debug9(printf(" => Loses to %d\n",best_fwd_score));
	      }

	      prevhit = /*active[prev_querypos]*/prev_active[prevhit];
	    }
	  }
	}
      }

      /* Best_score needs to beat something positive to prevent a
	 small local extension from beating a good canonical intron.
	 If querypos is too small, don't insert an intron.  */
      /* linksconsecutive already assigned above */
      currlink = &(links[curr_querypos][hiti + low_hit]);
      currlink->fwd_consecutive = best_fwd_consecutive;
      currlink->fwd_rootposition = best_fwd_rootposition;
      currlink->fwd_pos = best_fwd_prevpos;
      currlink->fwd_hit = best_fwd_prevhit;
      if (currlink->fwd_pos >= 0) {
	currlink->fwd_tracei = best_fwd_tracei;
	fwd_scores[curr_querypos][hiti + low_hit] = best_fwd_score;
#ifdef MOVE_TO_STAGE3
      } else if (anchoredp == true) {
	currlink->fwd_tracei = -1;
	fwd_scores[curr_querypos][hiti + low_hit] = -100000;
#endif
      } else if (localp == true) {
	currlink->fwd_tracei = ++*fwd_tracei;
	fwd_scores[curr_querypos][hiti + low_hit] = indexsize_nt;
      } else {
	currlink->fwd_tracei = ++*fwd_tracei;
	fwd_scores[curr_querypos][hiti + low_hit] = best_fwd_score;
      }

#ifdef DEBUG9
      currlink->fwd_intronnfwd = best_fwd_intronnfwd;
      currlink->fwd_intronnrev = best_fwd_intronnrev;
      currlink->fwd_intronnunk = best_fwd_intronnunk;
#endif

      debug9(printf("\tChose %d,%d with score %d (fwd) => trace #%d\n",
		    currlink->fwd_pos,currlink->fwd_hit,fwd_scores[curr_querypos][hiti + low_hit],currlink->fwd_tracei));
      debug3(printf("%d %d  %d %d  1\n",querypos,hit,best_prevpos,best_prevhit));
    }

    FREEA(frontier);
  }

  return;
}


static void
revise_active_lookback (int **active, int *firstactive, int *nactive, 
			int low_hit, int high_hit, int **fwd_scores, int querypos) {
  int best_score, threshold, score;
  int hit, *ptr;

  debug6(printf("Revising querypos %d from low_hit %d to high_hit %d.  Scores:\n",querypos,low_hit,high_hit));
  if ((hit = low_hit) >= high_hit) {
    debug6(printf("1.  Initializing firstactive for querypos %d to be -1\n",querypos));
    firstactive[querypos] = -1;
    nactive[querypos] = 0;

  } else {
    debug6(printf("At hit %d, fwd_score is %d",hit,fwd_scores[querypos][hit]));
    best_score = fwd_scores[querypos][hit];
#ifdef SEPARATE_FWD_REV
    debug6(printf(" and rev_score is %d",rev_scores[querypos][hit]));
    if ((score = rev_scores[querypos][hit]) > best_score) {
      best_score = score;
    }
#endif
    debug6(printf("\n"));

    for (hit++; hit < high_hit; hit++) {
      debug6(printf("At hit %d, fwd_score is %d",hit,fwd_scores[querypos][hit]));
      if ((score = fwd_scores[querypos][hit]) > best_score) {
	best_score = score;
      }
#ifdef SEPARATE_FWD_REV
      debug6(printf(" and rev_score is %d",rev_scores[querypos][hit]));
      if ((score = rev_scores[querypos][hit]) > best_score) {
	best_score = score;
      }
#endif
      debug6(printf("\n"));
    }

    threshold = best_score - SCORE_FOR_RESTRICT;
    if (threshold < 0) {
      threshold = 0;
    }

    nactive[querypos] = 0;
    firstactive[querypos] = -1;
    ptr = &(firstactive[querypos]);
    hit = low_hit;
    while (hit < high_hit) {
      while (hit < high_hit && fwd_scores[querypos][hit] <= threshold
#ifdef SEPARATE_FWD_REV
	     && rev_scores[querypos][hit] <= threshold
#endif
	     ) {
	hit++;
      }
      *ptr = hit;
      if (hit < high_hit) {
	nactive[querypos] += 1;
	ptr = &(active[querypos][hit]);
	hit++;
      }
    }
    *ptr = -1;
  }

  debug6(
	 printf("Valid hits (%d) at querypos %d (firstactive %d):",nactive[querypos],querypos,firstactive[querypos]);
	 hit = firstactive[querypos];
	 while (hit != -1) {
	   printf(" %d",hit);
	   hit = active[querypos][hit];
	 }
	 printf("\n");
	 );

  return;
}


static void
revise_active_lookforward (int **active, int *firstactive, int *nactive, 
			   int low_hit, int high_hit, int **fwd_scores, int querypos) {
  int best_score, threshold, score;
  int hit, *ptr;

  debug6(printf("Revising querypos %d from high_hit %d to low_hit %d.  Scores:\n",querypos,high_hit,low_hit));
  if ((hit = high_hit - 1) < low_hit) {
    debug6(printf("2.  Initializing firstactive for querypos %d to be -1\n",querypos));
    firstactive[querypos] = -1;
    nactive[querypos] = 0;
  } else {
    debug6(printf("At hit %d, fwd_score is %d",hit,fwd_scores[querypos][hit]));
    best_score = fwd_scores[querypos][hit];
#ifdef SEPARATE_FWD_REV
    debug6(printf(" and rev_score is %d",rev_scores[querypos][hit]));
    if ((score = rev_scores[querypos][hit]) > best_score) {
      best_score = score;
    }
#endif
    debug6(printf("\n"));

    for (--hit; hit >= low_hit; --hit) {
      debug6(printf("At hit %d, fwd_score is %d",hit,fwd_scores[querypos][hit]));
      if ((score = fwd_scores[querypos][hit]) > best_score) {
	best_score = score;
      }
#ifdef SEPARATE_FWD_REV
      debug6(printf(" and rev_score is %d",rev_scores[querypos][hit]));
      if ((score = rev_scores[querypos][hit]) > best_score) {
	best_score = score;
      }
#endif
      debug6(printf("\n"));
    }

    threshold = best_score - SCORE_FOR_RESTRICT;
    if (threshold < 0) {
      threshold = 0;
    }

    nactive[querypos] = 0;
    firstactive[querypos] = -1;
    ptr = &(firstactive[querypos]);
    hit = high_hit - 1;
    while (hit >= low_hit) {
      while (hit >= low_hit && fwd_scores[querypos][hit] <= threshold
#ifdef SEPARATE_FWD_REV
	     && rev_scores[querypos][hit] <= threshold
#endif
	     ) {
	--hit;
      }
      *ptr = hit;
      if (hit >= low_hit) {
	nactive[querypos] += 1;
	ptr = &(active[querypos][hit]);
	--hit;
      }
    }
    *ptr = -1;
  }

  debug6(
	 printf("Valid hits (%d) at querypos %d (firstactive %d):",nactive[querypos],querypos,firstactive[querypos]);
	 hit = firstactive[querypos];
	 while (hit != -1) {
	   printf(" %d",hit);
	   hit = active[querypos][hit];
	 }
	 printf("\n");
	 );

  return;
}



static int **
intmatrix_1d_new (int length1, int *lengths2, int totallength) {
  int **matrix;
  int i;
  
  matrix = (int **) CALLOC(length1,sizeof(int *));
  matrix[0] = (int *) CALLOC(totallength,sizeof(int));
  for (i = 1; i < length1; i++) {
    if (lengths2[i-1] <= 0) {
      matrix[i] = matrix[i-1];
    } else {
      matrix[i] = &(matrix[i-1][lengths2[i-1]]);
    }
  }
  return matrix;
}

static void
intmatrix_1d_free (int ***matrix) {
  FREE((*matrix)[0]);
  FREE(*matrix);
  return;
}


static int **
intmatrix_2d_new (int length1, int *lengths2) {
  int **matrix;
  int i;
  
  matrix = (int **) CALLOC(length1,sizeof(int *));
  for (i = 0; i < length1; i++) {
    if (lengths2[i] <= 0) {
      matrix[i] = (int *) NULL;
    } else {
      matrix[i] = (int *) CALLOC(lengths2[i],sizeof(int));
    }
  }
  return matrix;
}

static void
intmatrix_2d_free (int ***matrix, int length1) {
  int i;

  for (i = 0; i < length1; i++) {
    if ((*matrix)[i]) {
      FREE((*matrix)[i]);
    }
  }
  FREE(*matrix);
  return;
}


/************************************************************************
 *   Cells used for ranking hits
 ************************************************************************/

#if 0
typedef struct Cell_T *Cell_T;
struct Cell_T {
  int rootposition;
  int endposition;
  int querypos;
  int hit;
  bool fwdp;
  int score;
};

/* Replaced by Cellpool_T routines */
static void
Cell_free (Cell_T *old) {
  FREE(*old);
  return;
}


static Cell_T
Cell_new (int rootposition, int endposition, int querypos, int hit, bool fwdp, int score) {
  Cell_T new = (Cell_T) MALLOC(sizeof(*new));

  new->rootposition = rootposition;
  new->endposition = endposition;
  new->querypos = querypos;
  new->hit = hit;
  new->fwdp = fwdp;
  new->score = score;
  return new;
}
#endif


#ifdef SLOW
/* Used for the final set of cells, to see if we have non-overlapping paths */
static int
Cell_interval_cmp (const void *a, const void *b) {
  Cell_T x = * (Cell_T *) a;
  Cell_T y = * (Cell_T *) b;

  if (x->rootposition < y->rootposition) {
    return -1;
  } else if (y->rootposition < x->rootposition) {
    return +1;

  } else if (x->endposition > y->endposition) {
    return -1;
  } else if (y->endposition > x->endposition) {
    return +1;

  } else {
    return 0;
  }
}
#endif


/* Used for the initial set of cells, to get the end cell for each rootposition */
static int
Cell_rootposition_left_cmp (const void *a, const void *b) {
  Cell_T x = * (Cell_T *) a;
  Cell_T y = * (Cell_T *) b;

  if (x->rootposition < y->rootposition) {
    return -1;
  } else if (y->rootposition < x->rootposition) {
    return +1;

#if 0
    /* Want score ranking, rather than interval ranking here.  Otherwise, we don't get the final endposition */
  } else if (x->endposition < y->endposition) {
    return -1;
  } else if (y->endposition < x->endposition) {
    return +1;
#endif

#if 0
  } else if (x->tracei < y->tracei) {
    return -1;
  } else if (y->tracei < x->tracei) {
    return +1;
#endif
  } else if (x->score > y->score) {
    return -1;
  } else if (y->score > x->score) {
    return +1;
  } else if (x->querypos > y->querypos) {
    return -1;
  } else if (y->querypos > x->querypos) {
    return +1;
  } else if (x->hit < y->hit) {
    return -1;
  } else if (y->hit < x->hit) {
    return +1;
  } else if (x->fwdp == true && y->fwdp == false) {
    return -1;
  } else if (y->fwdp == true && x->fwdp == false) {
    return +1;
  } else {
    return 0;
  }
}


/* Used for the initial set of cells, to get the end cell for each rootposition */
static int
Cell_rootposition_right_cmp (const void *a, const void *b) {
  Cell_T x = * (Cell_T *) a;
  Cell_T y = * (Cell_T *) b;

  if (x->rootposition < y->rootposition) {
    return -1;
  } else if (y->rootposition < x->rootposition) {
    return +1;

#if 0
    /* Want score ranking, rather than interval ranking here.  Otherwise, we don't get the final endposition */
  } else if (x->endposition < y->endposition) {
    return -1;
  } else if (y->endposition < x->endposition) {
    return +1;
#endif

#if 0
  } else if (x->tracei < y->tracei) {
    return -1;
  } else if (y->tracei < x->tracei) {
    return +1;
#endif
  } else if (x->score > y->score) {
    return -1;
  } else if (y->score > x->score) {
    return +1;
  } else if (x->querypos > y->querypos) {
    return -1;
  } else if (y->querypos > x->querypos) {
    return +1;
  } else if (x->hit > y->hit) {
    return -1;
  } else if (y->hit > x->hit) {
    return +1;
  } else if (x->fwdp == true && y->fwdp == false) {
    return -1;
  } else if (y->fwdp == true && x->fwdp == false) {
    return +1;
  } else {
    return 0;
  }
}


static int
Cell_score_cmp (const void *a, const void *b) {
  Cell_T x = * (Cell_T *) a;
  Cell_T y = * (Cell_T *) b;

  if (x->score > y->score) {
    return -1;
  } else if (y->score > x->score) {
    return +1;
  } else {
    return 0;
  }
}


#ifdef USE_THRESHOLD_SCORE
/* Doesn't work well for short dynamic programming at the ends of a read */
static Cell_T *
Linkmatrix_get_cells_fwd (int *nunique, struct Link_T **links, int querystart, int queryend, int *npositions,
			  int indexsize, int bestscore, bool favor_right_p, Cellpool_T cellpool) {
  Cell_T *sorted, *cells;
  List_T celllist = NULL;
  int querypos, hit;
  int rootposition, last_rootposition;
  int threshold_score, best_score_for_root;
  int ngood, ncells, i, k;

  if (bestscore > 2*suboptimal_score_end) {
    threshold_score = bestscore - suboptimal_score_end;
  } else {
    threshold_score = bestscore/2;
  }
  if (threshold_score <= indexsize) {
    threshold_score = indexsize + 1;
  }

  ncells = 0;
  for (querypos = querystart; querypos <= queryend; querypos++) {
    ngood = 0;
    for (hit = 0; hit < npositions[querypos]; hit++) {
      if (links[querypos][hit].fwd_score >= threshold_score) {
	ngood++;
      }
    }
    if (ngood > 0 && ngood <= 10) {
      for (hit = 0; hit < npositions[querypos]; hit++) {
	debug11(printf("  At %d,%d, comparing score %d with threshold_score %d\n",
		       querypos,hit,links[querypos][hit].fwd_score,threshold_score));
	if (links[querypos][hit].fwd_score >= threshold_score) {
	  rootposition = links[querypos][hit].fwd_rootposition;
	  /* tracei = links[querypos][hit].fwd_tracei; */
	  celllist = Cellpool_push(celllist,cellpool,rootposition,
				   /*endposition*/(int) mappings[querypos][hit],
				   querypos,hit,/*fwdp*/true,links[querypos][hit].fwd_score);
	  ncells++;
	}
      }
    }
  }

  if (ncells == 0) {
    *nunique = 0;
    return (Cell_T *) NULL;

  } else {
    /* Take best result for each tracei */
    /* Using alloca can give a stack overflow */
    cells = (Cell_T *) List_to_array(celllist,NULL);
    /* List_free(&celllist); -- No need with cellpool */

    if (favor_right_p == true) {
      qsort(cells,ncells,sizeof(Cell_T),Cell_rootposition_right_cmp);
    } else {
      /* favor_right_p is always false for GMAP */
      qsort(cells,ncells,sizeof(Cell_T),Cell_rootposition_left_cmp);
    }

    sorted = (Cell_T *) MALLOC(ncells * sizeof(Cell_T)); /* Return value */
    k = 0;

    last_rootposition = -1;
    best_score_for_root = -1;
    for (i = 0; i < ncells; i++) {
      if (cells[i]->rootposition != last_rootposition) {
	debug11(printf("Pushing rootposition %d, trace #%d, score %d, pos %d, hit %d\n",
		       cells[i]->rootposition,cells[i]->tracei,cells[i]->score,cells[i]->querypos,cells[i]->hit));
	sorted[k++] = cells[i];
	last_rootposition = cells[i]->rootposition;
	best_score_for_root = cells[i]->score;

      } else if (cells[i]->querypos == best_score_for_root) {
	debug11(printf("Equivalent cell for rootposition %d, trace #%d, score %d, pos %d, hit %d\n",
		       cells[i]->rootposition,cells[i]->tracei,cells[i]->score,cells[i]->querypos,cells[i]->hit));
	sorted[k++] = cells[i];
	/* last_rootposition = cells[i]->rootposition;*/
	/* best_score_for_root = cells[i]->score; */

      } else {
	/* Cell_free(&(cells[i])); -- no need with cellpool */

      }
    }
    debug11(printf("\n"));
    FREE(cells);
  
    *nunique = k;
    qsort(sorted,*nunique,sizeof(Cell_T),Cell_score_cmp);

    return sorted;
  }
}

#else

static Cell_T *
get_cells_fwd (int *nunique, struct Link_T **links, int **fwd_scores, Chrpos_T **mappings,
	       int querystart, int queryend, int *npositions,
	       bool favor_right_p, Cellpool_T cellpool) {
  Cell_T *sorted, *cells;
  List_T celllist = NULL;
  int querypos, hit;
  int rootposition, last_rootposition;
  int best_score_for_root;
  int ncells, i, k;

  ncells = 0;
  for (querypos = querystart; querypos <= queryend; querypos++) {
    for (hit = 0; hit < npositions[querypos]; hit++) {
      if (fwd_scores[querypos][hit] > 0) {
	rootposition = links[querypos][hit].fwd_rootposition;
	/* tracei = links[querypos][hit].fwd_tracei; */
	celllist = Cellpool_push(celllist,cellpool,rootposition,
				 /*endposition*/(int) mappings[querypos][hit],
				 querypos,hit,/*fwdp*/true,fwd_scores[querypos][hit]);
	ncells++;
      }
    }
  }

  debug12(printf("Have %d cells\n",ncells));
  if (ncells == 0) {
    *nunique = 0;
    return (Cell_T *) NULL;

  } else {
    /* Take best result for each tracei */
    /* Using alloca can give a stack overflow */
    cells = (Cell_T *) List_to_array(celllist,NULL);
    /* List_free(&celllist); -- No need with cellpool */

    if (favor_right_p == true) {
      qsort(cells,ncells,sizeof(Cell_T),Cell_rootposition_right_cmp);
    } else {
      /* favor_right_p is always false for GMAP */
      qsort(cells,ncells,sizeof(Cell_T),Cell_rootposition_left_cmp);
    }

    sorted = (Cell_T *) MALLOC(ncells * sizeof(Cell_T)); /* Return value */
    k = 0;

    last_rootposition = -1;
    best_score_for_root = -1;
    for (i = 0; i < ncells; i++) {
      if (cells[i]->rootposition != last_rootposition) {
	/* Take best cell at this rootposition */
	debug11(printf("Pushing rootposition %d, score %d, pos %d, hit %d\n",
		       cells[i]->rootposition,cells[i]->score,cells[i]->querypos,cells[i]->hit));
	sorted[k++] = cells[i];
	last_rootposition = cells[i]->rootposition;
	best_score_for_root = cells[i]->score;
	
      } else if (cells[i]->score == best_score_for_root) {
	/* Take equivalent cell for this rootposition */
	debug11(printf("Pushing equivalent end for rootposition %d, score %d, pos %d, hit %d\n",
		       cells[i]->rootposition,cells[i]->score,cells[i]->querypos,cells[i]->hit));
	sorted[k++] = cells[i];
	/* last_rootposition = cells[i]->rootposition; */
	/* best_score_for_root = cells[i]->score; */

      } else {
	/* Cell_free(&(cells[i])); -- no need with cellpool */
      }
    }
    debug11(printf("\n"));
    FREE(cells);
  
    *nunique = k;
    qsort(sorted,*nunique,sizeof(Cell_T),Cell_score_cmp);

    return sorted;
  }
}

#endif

#if 0
static Cell_T *
Linkmatrix_get_cells_both (int *nunique, struct Link_T **links, int querystart, int queryend, int *npositions,
			   int indexsize, int bestscore, bool favor_right_p, Cellpool_T cellpool) {
  Cell_T *sorted, *cells;
  List_T celllist = NULL;
  int querypos, hit;
  int rootposition, last_rootposition;
  int threshold_score, best_score_for_root;
  int ngood, ncells, i, k;

  if (bestscore > 2*suboptimal_score_end) {
    threshold_score = bestscore - suboptimal_score_end;
  } else {
    threshold_score = bestscore/2;
  }
  if (threshold_score <= indexsize) {
    threshold_score = indexsize + 1;
  }

  debug11(printf("Entered Linkmatrix_get_cells_both with querystart %d, queryend %d, threshold score %d\n",
		 querystart,queryend,threshold_score));

  ncells = 0;
  for (querypos = querystart; querypos <= queryend; querypos++) {
    ngood = 0;
    for (hit = 0; hit < npositions[querypos]; hit++) {
      if (links[querypos][hit].fwd_score >= threshold_score) {
	ngood++;
      }
#ifdef SEPARATE_FWD_REV
      if (links[querypos][hit].rev_score >= threshold_score) {
	ngood++;
      }
#endif
    }
    if (ngood > 0 && ngood <= 10) {
      for (hit = 0; hit < npositions[querypos]; hit++) {
	if (links[querypos][hit].fwd_score >= threshold_score) {
	  rootposition = links[querypos][hit].fwd_rootposition;
	  /* tracei = links[querypos][hit].fwd_tracei; */
	  celllist = Cellpool_push(celllist,cellpool,rootposition,
				   /*endposition*/(int) mappings[querypos][hit],
				   querypos,hit,/*fwdp*/true,links[querypos][hit].fwd_score);
	  ncells++;
	}
#ifdef SEPARATE_FWD_REV
	if (links[querypos][hit].rev_score >= threshold_score) {
	  rootposition = links[querypos][hit].rev_rootposition;
	  /* tracei = links[querypos][hit].rev_tracei; */
	  celllist = Cellpool_push(celllist,cellpool,rootposition,
				   /*endposition*/(int) mappings[querypos][hit],
				   querypos,hit,/*fwdp*/false,links[querypos][hit].rev_score);
	  ncells++;
	}
#endif
      }
    }
  }

  debug12(printf("Have %d cells\n",ncells));
  if (ncells == 0) {
    *nunique = 0;
    return (Cell_T *) NULL;

  } else {
    /* Take best result for each tracei */
    /* Using alloca can give a stack overflow */
    cells = (Cell_T *) List_to_array(celllist,NULL);
    /* List_free(&celllist); -- no need with cellpool */

    if (favor_right_p == true) {
      qsort(cells,ncells,sizeof(Cell_T),Cell_rootposition_right_cmp);
    } else {
      /* favor_right_p is always false for GMAP */
      qsort(cells,ncells,sizeof(Cell_T),Cell_rootposition_left_cmp);
    }

    sorted = (Cell_T *) MALLOC(ncells * sizeof(Cell_T)); /* Return value */
    k = 0;

    last_rootposition = -1;
    best_score_for_root = -1;
    for (i = 0; i < ncells; i++) {
      if (cells[i]->rootposition != last_rootposition) {
	/* Take best cell at this rootposition */
	debug11(printf("rootposition %d, score %d, pos %d, hit %d\n",
		       cells[i]->rootposition,cells[i]->score,cells[i]->querypos,cells[i]->hit));
	sorted[k++] = cells[i];
	last_rootposition = cells[i]->rootposition;
	best_score_for_root = cells[i]->score;

      } else if (cells[i]->score == best_score_for_root) {
	/* Take equivalent end cell for this rootposition */
	debug11(printf("equivalent end for rootposition %d, score %d, pos %d, hit %d\n",
		       cells[i]->rootposition,cells[i]->score,cells[i]->querypos,cells[i]->hit));
	sorted[k++] = cells[i];
	/* last_rootposition = cells[i]->rootposition; */
	/* best_score_for_root = cells[i]->score; */
	
      } else {
	/* Cell_free(&(cells[i])); -- no need with cellpool */
      }
    }
    debug11(printf("\n"));
    FREE(cells);

    *nunique = k;
    qsort(sorted,*nunique,sizeof(Cell_T),Cell_score_cmp);

    return sorted;
  }
}
#endif


#ifdef MOVE_TO_STAGE3
static int
binary_search (int lowi, int highi, Chrpos_T *mappings, Chrpos_T goal) {
  int middlei;

  debug10(printf("entered binary search with lowi=%d, highi=%d, goal=%u\n",lowi,highi,goal));
  if (mappings == NULL) {
    return -1;
  } else {
    while (lowi < highi) {
      middlei = lowi + ((highi - lowi) / 2);
      debug10(printf("  binary: %d:%u %d:%u %d:%u   vs. %u\n",
		     lowi,mappings[lowi],middlei,mappings[middlei],
		     highi,mappings[highi],goal));
      if (goal < mappings[middlei]) {
	highi = middlei;
      } else if (goal > mappings[middlei]) {
	lowi = middlei + 1;
      } else {
	debug10(printf("binary search returns %d\n",middlei));
	return middlei;
      }
    }

    debug10(printf("binary search returns %d\n",highi));
    return highi;
  }
}
#endif


/* Returns celllist */
/* For PMAP, indexsize is in aa. */
static Cell_T *
align_compute_scores_lookback (int *ncells, struct Link_T **links, int **fwd_scores,
			       Chrpos_T **mappings, int *npositions, int totalpositions,
			       bool oned_matrix_p, Chrpos_T *minactive, Chrpos_T *maxactive,
			       int *firstactive, int *nactive, Cellpool_T cellpool,
			       int querystart, int queryend, int querylength,
			       Genome_T genome, Genome_T genomealt,
			       Univcoord_T chroffset, Univcoord_T chrhigh, bool plusp,

			       int indexsize,
#ifdef DEBUG9
			       char *queryseq_ptr,
#endif
#ifdef MOVE_TO_STAGE3
			       bool anchoredp, int anchor_querypos, Chrpos_T anchor_position,
#endif
			       bool localp, bool skip_repetitive_p, 
			       bool use_canonical_p, int non_canonical_penalty,
			       bool favor_right_p, bool middlep) {
#if 0
  bool anchoredp = false;
  int anchor_querypos = 0;
  Chrpos_T anchor_position = 0;
#endif

  Cell_T *cells;
  Link_T currlink;
  int curr_querypos, indexsize_nt, indexsize_query, hit, nhits, low_hit, high_hit;
  int nskipped, min_hits, specific_querypos, specific_low_hit, specific_high_hit, next_querypos;
  Intlist_T processed = NULL;
  int best_overall_score = 0;
  int grand_fwd_score, grand_fwd_querypos, grand_fwd_hit, best_fwd_hit, best_fwd_score;
#ifdef SEPARATE_FWD_REV
  int grand_rev_score, grand_rev_querypos, grand_rev_hit, best_rev_hit, best_rev_score;
#ifdef DEBUG9
  int rev_tracei = 0;
#endif
#endif
  int **active;
  Chrpos_T position, prevposition;
  int fwd_tracei = 0;
#if 0
  int *lastGT, *lastAG;
#ifndef PMAP
  int *lastCT, *lastAC;
#endif
#endif
#ifdef DEBUG9
  Link_T prevlink;
  char *oligo;
#endif
#ifdef DEBUG12
  Link_T termlink = NULL;
#endif

#ifdef PMAP
  indexsize_nt = indexsize*3;
#else
  indexsize_nt = indexsize;
#endif
  indexsize_query = indexsize;	/* Use when evaluating across query positions */


#ifdef DEBUG9
  oligo = (char *) CALLOC(indexsize+1,sizeof(char));
#endif
  debug0(printf("Lookback: querystart = %d, queryend = %d, indexsize = %d\n",querystart,queryend,indexsize));

  assert(oned_matrix_p == true);
  if (oned_matrix_p == true) {
    active = intmatrix_1d_new(querylength,npositions,totalpositions);
  } else {
    active = intmatrix_2d_new(querylength,npositions);
  }

#if 0
  firstactive = (int *) MALLOC(querylength * sizeof(int));
  nactive = (int *) MALLOC(querylength * sizeof(int));
#endif

  /* Initialize */
  for (curr_querypos = 0; curr_querypos < querystart; curr_querypos++) {
    debug6(printf("3.  Initializing firstactive for querypos %d to be -1\n",curr_querypos));
    firstactive[curr_querypos] = -1;
    nactive[curr_querypos] = 0;
  }
  while (curr_querypos <= queryend && npositions[curr_querypos] <= 0) {
    debug6(printf("4.  Initializing firstactive for querypos %d to be -1\n",curr_querypos));
    debug9(printf("Skipping querypos %d which has no positions\n",curr_querypos));
    firstactive[curr_querypos] = -1;
    nactive[curr_querypos] = 0;
    curr_querypos++;
  }

#ifdef MOVE_TO_STAGE3
  if (anchoredp == true) {
    /* Guaranteed to find a hit */
    hit = binary_search(0,npositions[anchor_querypos],mappings[anchor_querypos],/*goal*/anchor_position);
    if (mappings[anchor_querypos] == NULL) {
      printf("mappings at anchor_querypos %d is NULL.  mappings = %p\n",anchor_querypos,mappings);
      abort();
    }

    currlink = &(links[anchor_querypos][hit]);	
#ifndef SEPARATE_FWD_REV
    currlink->fwd_pos = currlink->fwd_hit = -1;
    currlink->fwd_consecutive = EXON_DEFN;
    currlink->fwd_tracei = 0;
    fwd_scores[anchor_querypos][hit] = indexsize_nt;
#else
    fprintf(stderr,"Not implemented yet\n");
    abort();
#endif

    debug6(printf("Setting firstactive for anchorpos %d to be %d\n",anchor_querypos,hit));
    firstactive[anchor_querypos] = hit;
    nactive[anchor_querypos] = 1;
    active[anchor_querypos][hit] = -1;

    debug6(printf("Pushing anchorpos %d as processed\n",anchor_querypos));
    processed = Intlist_push(processed,anchor_querypos);

  } else
#endif

  if (curr_querypos <= queryend) {
    for (hit = 0; hit < npositions[curr_querypos]; hit++) {
      currlink = &(links[curr_querypos][hit]);
#ifndef SEPARATE_FWD_REV
      currlink->fwd_pos = currlink->fwd_hit = -1;
      currlink->fwd_consecutive = indexsize_nt;
      currlink->fwd_tracei = -1;
      /* currlink->fwd_rootnlinks = 1; */
      fwd_scores[curr_querypos][hit] = indexsize_nt;
#else
      currlink->fwd_pos = currlink->fwd_hit = -1;
      currlink->fwd_consecutive = indexsize_nt;
      currlink->fwd_tracei = -1;
      /* currlink->fwd_rootnlinks = 1; */
      fwd_scores[curr_querypos][hit] = indexsize_nt;
      if (splicingp == true) {
	currlink->rev_pos = currlink->rev_hit = -1;
	currlink->rev_consecutive = indexsize_nt;
	currlink->rev_tracei = -1;
	/* currlink->rev_rootnlinks = 1; */
	rev_scores[curr_querypos][hit] = indexsize_nt;
      }
#endif
    }
    revise_active_lookback(active,firstactive,nactive,0,npositions[curr_querypos],fwd_scores,curr_querypos);
  }

  grand_fwd_score = 0;
  grand_fwd_querypos = -1;
  grand_fwd_hit = -1;
#ifdef SEPARATE_FWD_REV
  if (splicingp == true) {
    grand_rev_score = 0;
    grand_rev_querypos = -1;
    grand_rev_hit = -1;
  }
#endif
  
  nskipped = 0;
  min_hits = 1000000;
  specific_querypos = -1;

  /* curr_querypos += 1; -- this causes curr_querypos at querystart to be ignored */
  while (curr_querypos <= queryend) {
    best_fwd_score = 0;
    best_fwd_hit = -1;
#ifdef SEPARATE_FWD_REV
    best_rev_score = 0;
    best_rev_hit = -1;
#endif
    
    debug9(printf("Positions at querypos %d (forward order):",curr_querypos);
	   for (hit = 0; hit < npositions[curr_querypos]; hit++) {
	     printf(" %u",mappings[curr_querypos][hit]);
	   }
	   printf("\n");
	   );

    hit = 0;
    while (hit < npositions[curr_querypos] && mappings[curr_querypos][hit] < minactive[curr_querypos]) {
      hit++;
    }
    low_hit = hit;
    while (hit < npositions[curr_querypos] && mappings[curr_querypos][hit] <= maxactive[curr_querypos]) {
      hit++;
    }
    high_hit = hit;
    debug9(printf("Querypos %d has hit %d..%d out of %d (minactive = %u, maxactive = %u)\n",
		  curr_querypos,low_hit,high_hit-1,npositions[curr_querypos],minactive[curr_querypos],maxactive[curr_querypos]));

    /* Can't use nactive yet, so use high_hit - low_hit */
    if (skip_repetitive_p && high_hit - low_hit >= MAX_NACTIVE && nskipped <= MAX_SKIPPED) { /* Previously turned off */
      debug6(printf("Too many active (%d - %d) at querypos %d.  Setting firstactive to be -1\n",high_hit,low_hit,curr_querypos));
      firstactive[curr_querypos] = -1;
      nactive[curr_querypos] = 0;
      nskipped++;
      debug9(printf("  %d skipped because of %d hits\n",nskipped,high_hit - low_hit + 1));

      /* Store most specific querypos in section of skipped */
      if (high_hit - low_hit < min_hits) {
	min_hits = high_hit - low_hit;
	specific_querypos = curr_querypos;
	specific_low_hit = low_hit;
	specific_high_hit = high_hit;
      }
      curr_querypos++;

    } else {
      if (nskipped > MAX_SKIPPED) {
	debug9(printf("Too many skipped.  Going back to specific querypos %d\n",specific_querypos));
	next_querypos = curr_querypos;
	curr_querypos = specific_querypos;
	low_hit = specific_low_hit;
	high_hit = specific_high_hit;
      } else {
	next_querypos = curr_querypos + 1;
      }

      if ((nhits = high_hit - low_hit) > 0) {
	if (nhits == 1) {
	  currlink = &(links[curr_querypos][low_hit]);
	  position = mappings[curr_querypos][low_hit];

	  debug9(strncpy(oligo,&(queryseq_ptr[curr_querypos]),indexsize));
	  debug9(oligo[indexsize] = '\0');
	  debug9(printf("Finding link looking back from querypos %d,%d at %ux%d (%s).  prev_querypos was %d\n",
			curr_querypos,low_hit,position,active[curr_querypos][low_hit],oligo,processed ? Intlist_head(processed) : -1));
	  
	  score_querypos_lookback_one(&fwd_tracei,currlink,curr_querypos,low_hit,position,
				      links,fwd_scores,mappings,active,firstactive,
				      genome,genomealt,chroffset,chrhigh,plusp,
				      indexsize,processed,localp,splicingp,use_canonical_p,
				      non_canonical_penalty);

	  if (fwd_scores[curr_querypos][low_hit] > 0) {
	    debug9(printf("Single hit at low_hit %d has score %d\n",low_hit,fwd_scores[curr_querypos][low_hit]));
	    best_fwd_score = fwd_scores[curr_querypos][low_hit];
	    best_fwd_hit = low_hit;
	  }

	} else {
	  debug9(strncpy(oligo,&(queryseq_ptr[curr_querypos]),indexsize));
	  debug9(oligo[indexsize] = '\0');
	  debug9(printf("Finding links looking back from querypos %d,%d..%d at (%u..%u) (%s).  prev_querypos was %d\n",
			curr_querypos,low_hit,high_hit-1,mappings[curr_querypos][low_hit],mappings[curr_querypos][high_hit-1],
			oligo,processed ? Intlist_head(processed) : -1));

	  score_querypos_lookback_mult(&fwd_tracei,low_hit,high_hit,curr_querypos,
				       /*positions*/&(mappings[curr_querypos][low_hit]),
				       links,fwd_scores,mappings,active,firstactive,
				       genome,genomealt,chroffset,chrhigh,plusp,
				       indexsize,processed,localp,splicingp,use_canonical_p,
				       non_canonical_penalty);

	  debug9(printf("Checking hits from low_hit %d to high_hit %d\n",low_hit,high_hit));
	  for (hit = low_hit; hit < high_hit; hit++) {
	    debug9(printf("Hit %d has score %d\n",hit,fwd_scores[curr_querypos][hit]));
	    if (fwd_scores[curr_querypos][hit] > best_fwd_score) {
	      best_fwd_score = fwd_scores[curr_querypos][hit];
	      best_fwd_hit = hit;
	    }
	  }
	}

	if (best_fwd_score > best_overall_score) {
	  best_overall_score = best_fwd_score;
	}

	nskipped = 0;
	min_hits = 1000000;
	specific_querypos = -1;
      
#ifndef SEPARATE_FWD_REV
	debug9(printf("Overall result at querypos %d yields best_fwd_hit %d\n",
		      curr_querypos,best_fwd_hit));
#else
	debug9(printf("Overall result at querypos %d yields best_fwd_hit %d and best_rev_hit %d\n",
		      curr_querypos,best_fwd_hit,best_rev_hit));
#endif

#if 1
	/* Previously, thought that using this code causes misses in
	   some alignments, but not using it causes missing end
	   exons */
	if (middlep == false && best_fwd_hit < 0) {
	  /* Allow for a new start, to test different ends */
	  for (hit = 0; hit < npositions[curr_querypos]; hit++) {
	    currlink = &(links[curr_querypos][hit]);
#ifndef SEPARATE_FWD_REV
	    currlink->fwd_pos = currlink->fwd_hit = -1;
	    currlink->fwd_consecutive = indexsize_nt;
	    currlink->fwd_tracei = -1;
	    /* currlink->fwd_rootnlinks = 1; */
	    fwd_scores[curr_querypos][hit] = indexsize_nt;
#else
	    currlink->fwd_pos = currlink->fwd_hit = -1;
	    currlink->fwd_consecutive = indexsize_nt;
	    currlink->fwd_tracei = -1;
	    /* currlink->fwd_rootnlinks = 1; */
	    fwd_scores[curr_querypos][hit] = indexsize_nt;
	    if (splicingp == true) {
	      currlink->rev_pos = currlink->rev_hit = -1;
	      currlink->rev_consecutive = indexsize_nt;
	      currlink->rev_tracei = -1;
	      /* currlink->rev_rootnlinks = 1; */
	      rev_scores[curr_querypos][hit] = indexsize_nt;
	    }
#endif
	  }
	}
#endif

	if (splicingp == true && best_fwd_hit >= 0 && links[curr_querypos][best_fwd_hit].fwd_hit < 0 && 
	    grand_fwd_querypos >= 0 && curr_querypos >= grand_fwd_querypos + indexsize_query) {
	  if ((best_fwd_score = fwd_scores[grand_fwd_querypos][grand_fwd_hit] - (curr_querypos - grand_fwd_querypos)) > 0) {
	    prevposition = mappings[grand_fwd_querypos][grand_fwd_hit];
	    debug12(printf("Considering prevposition %u to position %u as a grand fwd lookback\n",prevposition,position));
	    for (hit = low_hit; hit < high_hit; hit++) {
	      if ((position = mappings[curr_querypos][hit]) > prevposition + maxintronlen) {
		debug12(printf("  => Too long\n"));
	      } else if (position >= prevposition + indexsize_nt) {
		currlink = &(links[curr_querypos][hit]);
		currlink->fwd_consecutive = indexsize_nt;
		currlink->fwd_pos = grand_fwd_querypos;
		currlink->fwd_hit = grand_fwd_hit;
		currlink->fwd_tracei = ++fwd_tracei;
		fwd_scores[curr_querypos][hit] = best_fwd_score;
#ifdef DEBUG9
		prevlink = &(links[grand_fwd_querypos][grand_fwd_hit]);
		currlink->fwd_intronnfwd = prevlink->fwd_intronnfwd;
		currlink->fwd_intronnrev = prevlink->fwd_intronnrev;
		currlink->fwd_intronnunk = prevlink->fwd_intronnunk + 1;
#endif
	      }
	    }
	    debug12(printf("At querypos %d, setting all fwd hits to point back to grand_fwd %d,%d with a score of %d\n",
			   curr_querypos,grand_fwd_querypos,grand_fwd_hit,fwd_scores[grand_fwd_querypos][grand_fwd_hit]));
	  }
	}

	/* Use >= to favor longer path in case of ties */
	if (best_fwd_hit >= 0 && best_fwd_score >= grand_fwd_score && 
	    links[curr_querypos][best_fwd_hit].fwd_consecutive > EXON_DEFN) {
	  grand_fwd_score = best_fwd_score;
	  grand_fwd_querypos = curr_querypos;
	  grand_fwd_hit = best_fwd_hit;
	  debug12(termlink = &(links[curr_querypos][best_fwd_hit]));
	  debug12(printf("At querypos %d, revising grand fwd to be hit %d with score of %d (pointing back to %d,%d)\n",
			 curr_querypos,best_fwd_hit,best_fwd_score,termlink->fwd_pos,termlink->fwd_hit));
	}

#ifdef SEPARATE_FWD_REV
	if (best_rev_score > best_overall_score) {
	  best_overall_score = best_rev_score;
	}

	if (splicingp == false || use_canonical_p == false) {
	  /* rev scores should be the same as the fwd scores */
	} else {
	  if (best_rev_hit >= 0 && links[curr_querypos][best_rev_hit].rev_hit < 0 && 
	      grand_rev_querypos >= 0 && curr_querypos >= grand_rev_querypos + indexsize_query) {
	    prevlink = &(links[grand_rev_querypos][grand_rev_hit]);
	    if ((best_rev_score = prevlink->rev_score - (curr_querypos - grand_rev_querypos)) > 0) {
	      prevposition = mappings[grand_rev_querypos][grand_rev_hit];
	      debug12(printf("Considering prevposition %u to position %u as a grand rev lookback\n",prevposition,position));
	      for (hit = low_hit; hit < high_hit; hit++) {
		if ((position = mappings[curr_querypos][hit]) > prevposition + maxintronlen) {
		  debug12(printf("  => Too long\n"));
		} else if (position >= prevposition + indexsize_nt) {
		  currlink = &(links[curr_querypos][hit]);
		  currlink->rev_consecutive = indexsize_nt;
		  /* currlink->rev_rootnlinks = 1; */
		  currlink->rev_pos = grand_rev_querypos;
		  currlink->rev_hit = grand_rev_hit;
		  currlink->rev_score = best_rev_score;
#ifdef DEBUG9
		  currlink->rev_tracei = ++rev_tracei;
		  currlink->rev_intronnrev = prevlink->rev_intronnfwd;
		  currlink->rev_intronnrev = prevlink->rev_intronnrev;
		  currlink->rev_intronnunk = prevlink->rev_intronnunk + 1;
#endif
		}
	      }
	      debug12(printf("At querypos %d, setting all rev hits to point back to grand_rev %d,%d with a score of %d\n",
			     curr_querypos,grand_rev_querypos,grand_rev_hit,prevlink->rev_score));
	    }
	  }

	  /* Use >= to favor longer path in case of ties */
	  if (best_rev_hit >= 0 && best_rev_score >= grand_rev_score &&
	      links[curr_querypos][best_rev_hit].rev_consecutive > EXON_DEFN) {
	    grand_rev_score = best_rev_score;
	    grand_rev_querypos = curr_querypos;
	    grand_rev_hit = best_rev_hit;
	  }
	}
#endif
      }

      revise_active_lookback(active,firstactive,nactive,low_hit,high_hit,fwd_scores,curr_querypos);

      /* Need to push querypos, even if firstactive[curr_querypos] == -1 */
      /* Want to skip npositions[curr_querypos] == 0, so we can find adjacent despite mismatch or overabundance */
      if (npositions[curr_querypos] > 0) {
	debug6(printf("Pushing querypos %d onto processed\n",curr_querypos));
	processed = Intlist_push(processed,curr_querypos);
      }
      curr_querypos = next_querypos;
    }
  }
  debug9(printf("End of loop lookback\n"));

  Intlist_free(&processed);

#if 0
  FREE(nactive);
  FREE(firstactive);
#endif

  if (oned_matrix_p == true) {
    intmatrix_1d_free(&active);
  } else {
    intmatrix_2d_free(&active,querylength);
  }


  /* Grand winners */
  debug12(printf("Finding grand winners, using root position method\n"));
#ifdef SEPARATE_FWD_REV
  if (splicingp == false || use_canonical_p == false) {
    cells = Linkmatrix_get_cells_fwd(&(*ncells),links,querystart,queryend,npositions,
				     favor_right_p,cellpool);
  } else {
    cells = Linkmatrix_get_cells_both(&(*ncells),links,querystart,queryend,npositions,
				      indexsize,best_overall_score,favor_right_p,cellpool);
  }
#else
  cells = get_cells_fwd(&(*ncells),links,fwd_scores,mappings,querystart,queryend,npositions,
			favor_right_p,cellpool);
#endif

  debug9(FREE(oligo));

  return cells;
}


static char complCode[128] = COMPLEMENT_LC;

/* genomicstart == chroffset + chrpos */
/* arguments were genomicpos, genomicstart, genomiclength */

static char
get_genomic_nt (char *g_alt, Genome_T genome, Genome_T genomealt,
		Chrpos_T chrpos, Univcoord_T chroffset, Univcoord_T chrhigh, bool watsonp) {
  char c2, c2_alt;

  if (watsonp) {
    return Genome_get_char(&(*g_alt),genome,genomealt,chroffset + chrpos);

  } else {
    c2 = Genome_get_char(&c2_alt,genome,genomealt,chrhigh - chrpos);
    *g_alt = complCode[(int) c2_alt];
    return complCode[(int) c2];
  }
}


static List_T
traceback_one (int curr_querypos, int hit, struct Link_T **links, Chrpos_T **mappings,
	       char *queryseq_ptr, char *queryuc_ptr,
#ifdef PMAP
	       Genome_T genome, Genome_T genomealt,
	       Univcoord_T chroffset, Univcoord_T chrhigh, bool watsonp, bool lookbackp,
#endif
#ifdef DEBUG0
	       int **fwd_scores, int indexsize,
#endif	       
	       Pairpool_T pairpool, bool fwdp) {
  List_T path = NULL;
  Chrpos_T position;
  int prev_querypos, prevhit;
  char c2;
#ifdef PMAP
  char c2_alt;
#endif

#ifdef DEBUG0
  char *oligo;
#endif


  /* 1.  Prune matches at the 3' end that have low values for nconsecutive */
  if (fwdp == true) {
    while (curr_querypos >= 0 && links[curr_querypos][hit].fwd_consecutive < MIN_TERMINAL_NCONSECUTIVE) {
      position = mappings[curr_querypos][hit];

#if 0
      c2 = queryuc_ptr[curr_querypos];
      path = Pairpool_push(path,pairpool,curr_querypos,position,queryseq_ptr[curr_querypos],MATCH_COMP,
			   c2,/*genomealt*/c2,/*dynprogindex*/0);
#endif

#ifdef DEBUG0
      oligo = (char *) MALLOC((indexsize+1)*sizeof(char));
      strncpy(oligo,&(queryseq_ptr[curr_querypos]),indexsize);
      oligo[indexsize] = '\0';
      printf("Not pushing %d,%d (%s) at %u, score = %d, consec = %d",
	     curr_querypos,hit,oligo,position,
	     fwd_scores[curr_querypos][hit],links[curr_querypos][hit].fwd_consecutive);
      debug9(printf(" (from #%d), intr = %d(+)/%d(-)/%d(?)",
		    links[curr_querypos][hit].fwd_tracei,links[curr_querypos][hit].fwd_intronnfwd,links[curr_querypos][hit].fwd_intronnrev,
		    links[curr_querypos][hit].fwd_intronnunk));
      printf("\n");
#endif
  
      /* prevposition = position; */
      prev_querypos = curr_querypos;
      prevhit = hit;
      curr_querypos = links[prev_querypos][prevhit].fwd_pos;
      hit = links[prev_querypos][prevhit].fwd_hit;
      debug3(printf("%d %d  %d %d  3\n",prev_querypos,prevhit,curr_querypos,hit));
    }

#ifdef SEPARATE_FWD_REV
  } else {
    while (curr_querypos >= 0 && links[curr_querypos][hit].fwd_consecutive < MIN_TERMINAL_NCONSECUTIVE) {
      position = mappings[curr_querypos][hit];

#if 0
      c2 = queryuc_ptr[curr_querypos];
      path = Pairpool_push(path,pairpool,curr_querypos,position,queryseq_ptr[curr_querypos],MATCH_COMP,
			   c2,/*genomealt*/c2,/*dynprogindex*/0);
#endif

#ifdef DEBUG0
      oligo = (char *) MALLOC((indexsize+1)*sizeof(char));
      strncpy(oligo,&(queryseq_ptr[curr_querypos]),indexsize);
      oligo[indexsize] = '\0';
      printf("Not pushing %d,%d (%s) at %u, score = %d, consec = %d",
	     curr_querypos,hit,oligo,position,
	     links[curr_querypos][hit].rev_score,links[curr_querypos][hit].rev_consecutive);
      debug9(printf(" (from #%d), intr = %d(+)/%d(-)/%d(?)",
		    links[curr_querypos][hit].rev_tracei,links[curr_querypos][hit].rev_intronnfwd,links[curr_querypos][hit].rev_intronnrev,
		    links[curr_querypos][hit].rev_intronnunk));
      printf("\n");
#endif
  
      /* prevposition = position; */
      prev_querypos = curr_querypos;
      prevhit = hit;
      curr_querypos = links[prev_querypos][prevhit].rev_pos;
      hit = links[prev_querypos][prevhit].rev_hit;
      debug3(printf("%d %d  %d %d  3\n",prev_querypos,prevhit,curr_querypos,hit));
    }
#endif	/* SEPARATE_FWD_REV */
  }


  /* 2.  Continue with the rest of the path */
  while (curr_querypos >= 0) {
    position = mappings[curr_querypos][hit];

#ifdef PMAP
    /* Change querypos positions from protein to nucleotide */
    if (lookbackp == true) {
      c2 = get_genomic_nt(&c2_alt,genome,genomealt,position+2,chroffset,chrhigh,watsonp);
      path = Pairpool_push(path,pairpool,curr_querypos*3+2,position+2,/*cdna*/c2,MATCH_COMP,c2,c2_alt,
			   /*dynprogindex*/0);
      c2 = get_genomic_nt(&c2_alt,genome,genomealt,position+1,chroffset,chrhigh,watsonp);
      path = Pairpool_push(path,pairpool,curr_querypos*3+1,position+1,/*cdna*/c2,MATCH_COMP,c2,c2_alt,
			   /*dynprogindex*/0);
      c2 = get_genomic_nt(&c2_alt,genome,genomealt,position,chroffset,chrhigh,watsonp);
      path = Pairpool_push(path,pairpool,curr_querypos*3,position,/*cdna*/c2,MATCH_COMP,c2,c2_alt,
			   /*dynprogindex*/0);
    } else {
      c2 = get_genomic_nt(&c2_alt,genome,genomealt,position,chroffset,chrhigh,watsonp);
      path = Pairpool_push(path,pairpool,curr_querypos*3,position,/*cdna*/c2,MATCH_COMP,c2,c2_alt,
			   /*dynprogindex*/0);
      c2 = get_genomic_nt(&c2_alt,genome,genomealt,position+1,chroffset,chrhigh,watsonp);
      path = Pairpool_push(path,pairpool,curr_querypos*3+1,position+1,/*cdna*/c2,MATCH_COMP,c2,c2_alt,
			   /*dynprogindex*/0);
      c2 = get_genomic_nt(&c2_alt,genome,genomealt,position+2,chroffset,chrhigh,watsonp);
      path = Pairpool_push(path,pairpool,curr_querypos*3+2,position+2,/*cdna*/c2,MATCH_COMP,c2,c2_alt,
			   /*dynprogindex*/0);
    }
#else
    /* genomic nucleotide same as queryseq */
    c2 = queryuc_ptr[curr_querypos];
    path = Pairpool_push(path,pairpool,curr_querypos,position,queryseq_ptr[curr_querypos],MATCH_COMP,
			 c2,/*genomealt*/c2,/*dynprogindex*/0);
#endif


#ifdef DEBUG0
    debug0(oligo = (char *) MALLOC((indexsize+1)*sizeof(char)));
    debug0(strncpy(oligo,&(queryseq_ptr[curr_querypos]),indexsize));
    debug0(oligo[indexsize] = '\0');
    if (fwdp == true) {
      debug0(printf("Pushing %d,%d (%s) at %u, score = %d, consec = %d",
		    curr_querypos,hit,oligo,position,
		    fwd_scores[curr_querypos][hit],links[curr_querypos][hit].fwd_consecutive));
      debug9(printf(" (from #%d), intr = %d(+)/%d(-)/%d(?)",
		    links[curr_querypos][hit].fwd_tracei,links[curr_querypos][hit].fwd_intronnfwd,links[curr_querypos][hit].fwd_intronnrev,
		    links[curr_querypos][hit].fwd_intronnunk));
      debug0(printf("\n"));

#ifdef SEPARATE_FWD_REV
    } else {
      debug0(printf("Pushing %d,%d (%s) at %u, score = %d, consec = %d",
		    curr_querypos,hit,oligo,position,
		    links[curr_querypos][hit].rev_score,links[curr_querypos][hit].rev_consecutive));
      debug9(printf(" (from #%d), intr = %d(+)/%d(-)/%d(?)",
		    links[curr_querypos][hit].rev_tracei,links[curr_querypos][hit].rev_intronnfwd,links[curr_querypos][hit].rev_intronnrev,
		    links[curr_querypos][hit].rev_intronnunk));
      debug0(printf("\n"));

#endif
    }
#endif
    debug0(FREE(oligo));

    /* prevposition = position; */
    prev_querypos = curr_querypos;
    prevhit = hit;
    if (fwdp == true) {
      curr_querypos = links[prev_querypos][prevhit].fwd_pos;
      hit = links[prev_querypos][prevhit].fwd_hit;
#ifdef SEPARATE_FWD_REV
    } else {
      curr_querypos = links[prev_querypos][prevhit].rev_pos;
      hit = links[prev_querypos][prevhit].rev_hit;
#endif
    }
    debug3(printf("%d %d  %d %d  3\n",prev_querypos,prevhit,curr_querypos,hit));
  }
  debug0(printf("Done\n\n"));

  return path;
}


static List_T
traceback_one_snps (int curr_querypos, int hit, struct Link_T **links, Chrpos_T **mappings,
		    char *queryseq_ptr, Genome_T genome, Genome_T genomealt,
		    Univcoord_T chroffset, Univcoord_T chrhigh, bool watsonp,
#ifdef DEBUG0
		    int **fwd_scores, int indexsize,
#endif
		    Pairpool_T pairpool, bool fwdp) {
  List_T path = NULL;
  Chrpos_T position;
  int prev_querypos, prevhit;
  char c2, c2_alt;

#ifdef DEBUG0
  char *oligo;
#endif


  while (curr_querypos >= 0) {
    position = mappings[curr_querypos][hit];

#ifdef PMAP
    /* Change querypos positions from protein to nucleotide */
    c2 = get_genomic_nt(&c2_alt,genome,genomealt,position+2,chroffset,chrhigh,watsonp);
    path = Pairpool_push(path,pairpool,curr_querypos*3+2,position+2,/*cdna*/c2,MATCH_COMP,c2,c2_alt,
			 /*dynprogindex*/0);
    c2 = get_genomic_nt(&c2_alt,genome,genomealt,position+1,chroffset,chrhigh,watsonp);
    path = Pairpool_push(path,pairpool,curr_querypos*3+1,position+1,/*cdna*/c2,MATCH_COMP,c2,c2_alt,
			 /*dynprogindex*/0);
    c2 = get_genomic_nt(&c2_alt,genome,genomealt,position,chroffset,chrhigh,watsonp);
    path = Pairpool_push(path,pairpool,curr_querypos*3,position,/*cdna*/c2,MATCH_COMP,c2,c2_alt,
			 /*dynprogindex*/0);
#else
    /* genomic nucleotide or SNP same as queryseq */
    c2 = get_genomic_nt(&c2_alt,genome,genomealt,position,chroffset,chrhigh,watsonp);
    path = Pairpool_push(path,pairpool,curr_querypos,position,queryseq_ptr[curr_querypos],MATCH_COMP,c2,c2_alt,
			 /*dynprogindex*/0);
#endif


#ifdef DEBUG0
    debug0(oligo = (char *) MALLOC((indexsize+1)*sizeof(char)));
    debug0(strncpy(oligo,&(queryseq_ptr[curr_querypos]),indexsize));
    debug0(oligo[indexsize] = '\0');
    if (fwdp == true) {
      debug0(printf("Pushing %d,%d (%s) at %u, score = %d, consec = %d",
		    curr_querypos,hit,oligo,position,
		    fwd_scores[curr_querypos][hit],links[curr_querypos][hit].fwd_consecutive));
      debug9(printf(" (from #%d), intr = %d(+)/%d(-)/%d(?)",
		    links[curr_querypos][hit].fwd_tracei,links[curr_querypos][hit].fwd_intronnfwd,links[curr_querypos][hit].fwd_intronnrev,
		    links[curr_querypos][hit].fwd_intronnunk));
      debug0(printf("\n"));

#ifdef SEPARATE_FWD_REV
    } else {
      debug0(printf("Pushing %d,%d (%s) at %u, score = %d, consec = %d",
		    curr_querypos,hit,oligo,position,
		    links[curr_querypos][hit].rev_score,links[curr_querypos][hit].rev_consecutive));
      debug9(printf(" (from #%d), intr = %d(+)/%d(-)/%d(?)",
		    links[curr_querypos][hit].rev_tracei,links[curr_querypos][hit].rev_intronnfwd,links[curr_querypos][hit].rev_intronnrev,
		    links[curr_querypos][hit].rev_intronnunk));
      debug0(printf("\n"));
#endif
    }
#endif
    debug0(FREE(oligo));

    /* prevposition = position; */
    prev_querypos = curr_querypos;
    prevhit = hit;
    if (fwdp == true) {
      curr_querypos = links[prev_querypos][prevhit].fwd_pos;
      hit = links[prev_querypos][prevhit].fwd_hit;
#ifdef SEPARATE_FWD_REV
    } else {
      curr_querypos = links[prev_querypos][prevhit].rev_pos;
      hit = links[prev_querypos][prevhit].rev_hit;
#endif
    }
    debug3(printf("%d %d  %d %d  3\n",prev_querypos,prevhit,curr_querypos,hit));
  }
  debug0(printf("Done\n\n"));

  return path;
}


/* Performs dynamic programming.  For PMAP, indexsize is in aa. */
static List_T
align_compute_lookback (Chrpos_T **mappings, int *npositions, int totalpositions,
			bool oned_matrix_p, Chrpos_T *minactive, Chrpos_T *maxactive,
			int *firstactive, int *nactive, Cellpool_T cellpool,
			char *queryseq_ptr, char *queryuc_ptr, int querylength, int querystart, int queryend,
			Genome_T genome, Genome_T genomealt,
			Univcoord_T chroffset, Univcoord_T chrhigh, bool plusp,
			int indexsize, Pairpool_T pairpool,
#ifdef MOVE_TO_STAGE3
			bool anchoredp, int anchor_querypos, Chrpos_T anchor_position,
#endif
			bool localp, bool skip_repetitive_p, bool use_canonical_p, int non_canonical_penalty,
			bool favor_right_p, bool middlep, int max_nalignments) {
  List_T all_paths = NULL;
  int npaths = 0;
  struct Link_T **links;
  int **fwd_scores;

#if 0
  bool anchoredp = false;
  int anchor_querypos = 0;
  Chrpos_T anchor_position = 0;
#endif

  Cell_T *cells, cell;
  int ncells, i;

  bool fwdp;
  int querypos, hit;
  int bestscore;
#ifdef SLOW
  int last_endposition;
#endif


  if (oned_matrix_p == true) {
    links = Linkmatrix_1d_new(querylength,npositions,totalpositions);
    fwd_scores = intmatrix_1d_new(querylength,npositions,totalpositions);
  } else {
    links = Linkmatrix_2d_new(querylength,npositions);
    fwd_scores = intmatrix_2d_new(querylength,npositions);
  }

  cells = align_compute_scores_lookback(&ncells,links,fwd_scores,
					mappings,npositions,totalpositions,
					oned_matrix_p,minactive,maxactive,firstactive,nactive,cellpool,
					querystart,queryend,querylength,
					genome,genomealt,chroffset,chrhigh,plusp,

					indexsize,
#ifdef DEBUG9
					queryseq_ptr,
#endif
					localp,skip_repetitive_p,use_canonical_p,non_canonical_penalty,
					favor_right_p,middlep);
  /* cells are currently sorted by Cell_score_cmp in get_cells_fwd */


#ifdef SEPARATE_FWD_REV
  debug1(Linkmatrix_print_both(links,mappings,querylength,npositions,queryseq_ptr,indexsize));
#else
  debug1(print_fwd(links,fwd_scores,mappings,querylength,npositions,queryseq_ptr,indexsize));
#endif

  if (ncells == 0) {
    all_paths = (List_T) NULL;

  } else {
    /* High-scoring paths */
    bestscore = cells[0]->score;
    debug11(printf("** Looping on %d cells, allowing up to %d alignments, plus any with best score %d\n",
		   ncells,max_nalignments,bestscore));

    if (snps_p == true) {
      for (i = 0; i < ncells && (i < max_nalignments || cells[i]->score == bestscore)
	     && cells[i]->score > bestscore - FINAL_SCORE_TOLERANCE; i++) {
	cell = cells[i];
	querypos = cell->querypos;
	hit = cell->hit;
	fwdp = cell->fwdp;
	debug11(printf("Starting subpath %d for rootposition %d with score %d, querypos %d, hit %d, endposition %d\n",
		       i,cell->rootposition,cell->score,querypos,hit,cell->endposition));
	
	all_paths = List_push(all_paths,(void *) traceback_one_snps(querypos,hit,links,mappings,queryseq_ptr,
								    genome,genomealt,chroffset,chrhigh,/*watsonp*/plusp,
#ifdef DEBUG0
								    fwd_scores,indexsize,
#endif
								    pairpool,fwdp));
	npaths++;
	cell->pushedp = true;
      }

    } else {
      for (i = 0; i < ncells && (i < max_nalignments || cells[i]->score == bestscore)
	     && cells[i]->score > bestscore - FINAL_SCORE_TOLERANCE; i++) {
	cell = cells[i];
	querypos = cell->querypos;
	hit = cell->hit;
	fwdp = cell->fwdp;
	debug11(printf("Starting subpath %d for rootposition %d with score %d, querypos %d, hit %d, endposition %d\n",
		       i,cell->rootposition,cell->score,querypos,hit,cell->endposition));

	all_paths = List_push(all_paths,(void *) traceback_one(querypos,hit,links,mappings,queryseq_ptr,queryuc_ptr,	
#ifdef PMAP
							       genome,genomealt,chroffset,chrhigh,/*watsonp*/plusp,/*lookbackp*/true,
#endif
#ifdef DEBUG0
							       fwd_scores,indexsize,
#endif
							       pairpool,fwdp));
	npaths++;
	cell->pushedp = true;
      }
    }


#ifdef SLOW
    if (npaths < max_nalignments) {
      /* Non-overlapping paths */
      debug11(printf("** Looping on %d cells, looking for non-overlapping paths.  Total paths so far: %d\n",
		     ncells,npaths));
      qsort(cells,ncells,sizeof(Cell_T),Cell_interval_cmp);
      last_endposition = 0;
      if (snps_p == true) {
	for (i = 0; i < ncells && npaths < max_nalignments; i++) {
	  cell = cells[i];
	  if (cell->score > bestscore * NONOVERLAPPING_SCORE_TOLERANCE &&
	      cell->rootposition > last_endposition && cell->pushedp == false) {
	    debug11(printf("Starting subpath %d for rootposition %d with score %d, querypos %d, hit %d, endposition %d\n",
			   i,cell->rootposition,cell->score,querypos,hit,cell->endposition));
	    querypos = cell->querypos;
	    hit = cell->hit;
	    fwdp = cell->fwdp;
	    all_paths = List_push(all_paths,(void *) traceback_one_snps(querypos,hit,links,mappings,queryseq_ptr,
									genome,genomealt,chroffset,chrhigh,/*watsonp*/plusp,
#ifdef DEBUG0
									fwd_scores,indexsize,
#endif
									pairpool,fwdp));
	    npaths++;
	    cell->pushedp = true;
	    last_endposition = cell->endposition;
	  }
	}

      } else {
	for (i = 0; i < ncells && npaths < max_nalignments; i++) {
	  cell = cells[i];
	  if (cell->score > bestscore * NONOVERLAPPING_SCORE_TOLERANCE &&
	      cell->rootposition > last_endposition && cell->pushedp == false) {
	    debug11(printf("Starting subpath %d for rootposition %d with score %d, querypos %d, hit %d, endposition %d\n",
			   i,cell->rootposition,cell->score,querypos,hit,cell->endposition));
	    querypos = cell->querypos;
	    hit = cell->hit;
	    fwdp = cell->fwdp;
	    all_paths = List_push(all_paths,(void *) traceback_one(querypos,hit,links,mappings,queryseq_ptr,queryuc_ptr,	
#ifdef PMAP
								   genome,genomealt,chroffset,chrhigh,/*watsonp*/plusp,/*lookbackp*/true,
#endif
#ifdef DEBUG0
								   fwd_scores,indexsize,
#endif
								   pairpool,fwdp));
	    npaths++;
	    cell->pushedp = true;
	    last_endposition = cell->endposition;
	  }
	}
      }
    }
#endif

    debug11(printf("\n"));

#if 0
    /* No need with cellpool */
    for (i = 0; i < ncells; i++) {
      cell = cells[i];
      Cell_free(&cell);
    }
#endif
    FREE(cells);
  }


  if (oned_matrix_p == true) {
    Linkmatrix_1d_free(&links);
    intmatrix_1d_free(&fwd_scores);
  } else {
    Linkmatrix_2d_free(&links,querylength);
    intmatrix_2d_free(&fwd_scores,querylength);
  }

#if 0
  for (p = all_paths; p != NULL; p = List_next(p)) {
    Pair_dump_list(List_head(p),/*zerobasedp*/true);
    printf("\n");
  }
#endif

  return all_paths;
}



/* Returns celllist */
/* For PMAP, indexsize is in aa. */
static Cell_T *
align_compute_scores_lookforward (int *ncells, struct Link_T **links, int **fwd_scores,
				  Chrpos_T **mappings, int *npositions, int totalpositions,
				  bool oned_matrix_p, Chrpos_T *minactive, Chrpos_T *maxactive,
				  int *firstactive, int *nactive, Cellpool_T cellpool,
				  int querystart, int queryend, int querylength,
				  Genome_T genome, Genome_T genomealt,
				  Univcoord_T chroffset, Univcoord_T chrhigh, bool plusp,
				  int indexsize,
#ifdef DEBUG9
				  char *queryseq_ptr,
#endif
#ifdef MOVE_TO_STAGE3
				  bool anchoredp, int anchor_querypos, Chrpos_T anchor_position,
#endif
				  bool localp, bool skip_repetitive_p, 
				  bool use_canonical_p, int non_canonical_penalty,
				  bool favor_right_p, bool middlep) {
#if 0
  bool anchoredp = false;
  int anchor_querypos = 0;
  Chrpos_T anchor_position = 0;
#endif

  Cell_T *cells;
  Link_T currlink;
  int curr_querypos, indexsize_nt, indexsize_query, hit, nhits, low_hit, high_hit;
  int nskipped, min_hits, specific_querypos, specific_low_hit, specific_high_hit, next_querypos;
  Intlist_T processed = NULL;
  int best_overall_score = 0;
  int grand_fwd_score, grand_fwd_querypos, grand_fwd_hit, best_fwd_hit, best_fwd_score;
#ifdef SEPARATE_FWD_REV
  int grand_rev_score, grand_rev_querypos, grand_rev_hit, best_rev_hit, best_rev_score;
#ifdef DEBUG9
  int rev_tracei = 0;
#endif
#endif
  int **active;
  Chrpos_T position, prevposition;
  int fwd_tracei = 0;
#if 0
  int *lastGT, *lastAG;
#ifndef PMAP
  int *lastCT, *lastAC;
#endif
#endif
#ifdef DEBUG9
  Link_T prevlink;
  char *oligo;
#endif
#ifdef DEBUG12
  Link_T termlink = NULL;
#endif

#ifdef PMAP
  indexsize_nt = indexsize*3;
#else
  indexsize_nt = indexsize;
#endif
  indexsize_query = indexsize;	/* Use when evaluating across query positions */


#ifdef DEBUG9
  oligo = (char *) CALLOC(indexsize+1,sizeof(char));
#endif
  debug0(printf("Lookforward: querystart = %d, queryend = %d, indexsize = %d\n",querystart,queryend,indexsize));

  if (oned_matrix_p == true) {
    active = intmatrix_1d_new(querylength,npositions,totalpositions);
  } else {
    active = intmatrix_2d_new(querylength,npositions);
  }

#if 0
  firstactive = (int *) MALLOC(querylength * sizeof(int));
  nactive = (int *) MALLOC(querylength * sizeof(int));
#endif

  /* Initialize */
  for (curr_querypos = querylength - 1; curr_querypos > queryend; curr_querypos--) {
    debug6(printf("5.  Initializing firstactive for querypos %d to be -1\n",curr_querypos));
    firstactive[curr_querypos] = -1;
    nactive[curr_querypos] = 0;
  }
  while (curr_querypos >= querystart && npositions[curr_querypos] <= 0) {
    debug6(printf("6.  Initializing firstactive for querypos %d to be -1\n",curr_querypos));
    debug9(printf("Skipping querypos %d which has no positions\n",curr_querypos));
    firstactive[curr_querypos] = -1;
    nactive[curr_querypos] = 0;
    curr_querypos--;
  }

#ifdef MOVE_TO_STAGE3
  if (anchoredp == true) {
    /* Guaranteed to find a hit */
    hit = binary_search(0,npositions[anchor_querypos],mappings[anchor_querypos],/*goal*/anchor_position);
    if (mappings[anchor_querypos] == NULL) {
      printf("mappings at anchor_querypos %d is NULL.  mappings = %p\n",anchor_querypos,mappings);
      abort();
    }

    currlink = &(links[anchor_querypos][hit]);	
#ifndef SEPARATE_FWD_REV
    currlink->fwd_pos = currlink->fwd_hit = -1;
    currlink->fwd_consecutive = EXON_DEFN;
    currlink->fwd_tracei = 0;
    fwd_scores[anchor_querypos][hit] = indexsize_nt;
#else
    fprintf(stderr,"Not implemented yet\n");
    abort();
#endif

    debug6(printf("Setting firstactive for anchorpos %d to be %d\n",anchor_querypos,hit));
    firstactive[anchor_querypos] = hit;
    nactive[anchor_querypos] = 1;
    active[anchor_querypos][hit] = -1;

    debug6(printf("Pushing anchorpos %d as processed\n",anchor_querypos));
    processed = Intlist_push(processed,anchor_querypos);

  } else
#endif

  if (curr_querypos >= querystart) {
    for (hit = npositions[curr_querypos] - 1; hit >= 0; --hit) {
      currlink = &(links[curr_querypos][hit]);
#ifndef SEPARATE_FWD_REV
      currlink->fwd_pos = currlink->fwd_hit = -1;
      currlink->fwd_consecutive = indexsize_nt;
      currlink->fwd_tracei = -1;
      /* currlink->fwd_rootnlinks = 1; */
      fwd_scores[curr_querypos][hit] = indexsize_nt;
#else
      currlink->fwd_pos = currlink->fwd_hit = -1;
      currlink->fwd_score = indexsize_nt;
      currlink->fwd_consecutive = indexsize_nt;
      currlink->fwd_tracei = -1;
      /* currlink->fwd_rootnlinks = 1; */
      if (splicingp == true) {
	currlink->rev_pos = currlink->rev_hit = -1;
	currlink->rev_consecutive = indexsize_nt;
	currlink->rev_tracei = -1;
	/* currlink->rev_rootnlinks = 1; */
	rev_scores[curr_querypos][hit] = indexsize_nt;
      }
#endif
    }
    revise_active_lookforward(active,firstactive,nactive,0,npositions[curr_querypos],fwd_scores,curr_querypos);
  }


  grand_fwd_score = 0;
  grand_fwd_querypos = -1;
  grand_fwd_hit = -1;
#ifdef SEPARATE_FWD_REV
  if (splicingp == true) {
    grand_rev_score = 0;
    grand_rev_querypos = -1;
    grand_rev_hit = -1;
  }
#endif

  nskipped = 0;
  min_hits = 1000000;
  specific_querypos = -1;

  /* curr_querypos -= 1; -- this causes curr_querypos at queryend to be ignored */
  while (curr_querypos >= querystart) {
    best_fwd_score = 0;
    best_fwd_hit = -1;
#ifdef SEPARATE_FWD_REV
    best_rev_score = 0;
    best_rev_hit = -1;
#endif
    
    debug9(printf("Positions at querypos %d (reverse order):",curr_querypos);
	   for (hit = npositions[curr_querypos] - 1; hit >= 0; --hit) {
	     printf(" %u",mappings[curr_querypos][hit]);
	   }
	   printf("\n");
	   );

    hit = npositions[curr_querypos] - 1;
    while (hit >= 0 && mappings[curr_querypos][hit] > maxactive[curr_querypos]) {
      --hit;
    }
    high_hit = hit + 1;
    while (hit >= 0 && mappings[curr_querypos][hit] >= minactive[curr_querypos]) {
      --hit;
    }
    low_hit = hit + 1;
    debug9(printf("Querypos %d has hit %d..%d out of %d (minactive = %u, maxactive = %u)\n",
		  curr_querypos,high_hit-1,low_hit,npositions[curr_querypos],minactive[curr_querypos],maxactive[curr_querypos]));

    /* Can't use nactive yet, so use high_hit - low_hit */
    if (skip_repetitive_p && high_hit - low_hit >= MAX_NACTIVE && nskipped <= MAX_SKIPPED) { /* Previously turned off */
      debug6(printf("Too many active (%d - %d) at querypos %d.  Setting firstactive to be -1\n",high_hit,low_hit,curr_querypos));
      firstactive[curr_querypos] = -1;
      nactive[curr_querypos] = 0;
      nskipped++;
      debug9(printf("  %d skipped because of %d hits\n",nskipped,high_hit - low_hit + 1));

      /* Store most specific querypos in section of skipped */
      if (high_hit - low_hit < min_hits) {
	min_hits = high_hit - low_hit;
	specific_querypos = curr_querypos;
	specific_low_hit = low_hit;
	specific_high_hit = high_hit;
      }
      curr_querypos--;

    } else {
      if (nskipped > MAX_SKIPPED) {
	debug9(printf("Too many skipped.  Going back to specific querypos %d\n",specific_querypos));
	next_querypos = curr_querypos;
	curr_querypos = specific_querypos;
	low_hit = specific_low_hit;
	high_hit = specific_high_hit;
      } else {
	next_querypos = curr_querypos - 1;
      }

      if ((nhits = high_hit - low_hit) > 0) {
	if (nhits == 1) {
	  currlink = &(links[curr_querypos][low_hit]);
	  position = mappings[curr_querypos][low_hit];

	  debug9(strncpy(oligo,&(queryseq_ptr[curr_querypos]),indexsize));
	  debug9(oligo[indexsize] = '\0');
	  debug9(printf("Finding link looking forward from querypos %d,%d at %ux%d (%s).  prev_querypos was %d\n",
			curr_querypos,low_hit,position,active[curr_querypos][low_hit],oligo,processed ? Intlist_head(processed) : -1));
	  score_querypos_lookforward_one(&fwd_tracei,currlink,curr_querypos,low_hit,position,
					 links,fwd_scores,mappings,active,firstactive,
					 genome,genomealt,chroffset,chrhigh,plusp,
					 indexsize,processed,localp,splicingp,use_canonical_p,
					 non_canonical_penalty);
	  if (fwd_scores[curr_querypos][low_hit] > 0) {
	    debug9(printf("Single hit at low_hit %d has score %d\n",low_hit,fwd_scores[curr_querypos][low_hit]));
	    best_fwd_score = fwd_scores[curr_querypos][low_hit];
	    best_fwd_hit = low_hit;
	  }

	} else {
	  debug9(strncpy(oligo,&(queryseq_ptr[curr_querypos]),indexsize));
	  debug9(oligo[indexsize] = '\0');
	  debug9(printf("Finding links looking forward from querypos %d,%d..%d at (%u..%u) (%s).  prev_querypos was %d\n",
			curr_querypos,high_hit-1,low_hit,mappings[curr_querypos][high_hit-1],mappings[curr_querypos][low_hit],
			oligo,processed ? Intlist_head(processed) : -1));
	
	  score_querypos_lookforward_mult(&fwd_tracei,low_hit,high_hit,curr_querypos,
					  /*positions*/&(mappings[curr_querypos][low_hit]),
					  links,fwd_scores,mappings,active,firstactive,
					  genome,genomealt,chroffset,chrhigh,plusp,
					  indexsize,processed,localp,splicingp,use_canonical_p,
					  non_canonical_penalty);

	  debug9(printf("Checking hits from high_hit %d to low_hit %d\n",high_hit,low_hit));
	  for (hit = high_hit - 1; hit >= low_hit; hit--) {
	    debug9(printf("Hit %d has score %d\n",hit,fwd_scores[curr_querypos][hit]));
	    if (fwd_scores[curr_querypos][hit] > best_fwd_score) {
	      best_fwd_score = fwd_scores[curr_querypos][hit];
	      best_fwd_hit = hit;
	    }
	  }
	}

	if (best_fwd_score > best_overall_score) {
	  best_overall_score = best_fwd_score;
	}

	nskipped = 0;
	min_hits = 1000000;
	specific_querypos = -1;
      
#ifndef SEPARATE_FWD_REV
	debug9(printf("Overall result at querypos %d yields best_fwd_hit %d\n",
		      curr_querypos,best_fwd_hit));
#else
	debug9(printf("Overall result at querypos %d yields best_fwd_hit %d and best_rev_hit %d\n",
		      curr_querypos,best_fwd_hit,best_rev_hit));
#endif

#if 1
	/* Previously, thought that using this code causes misses in
	   some alignments, but not using it causes missing end
	   exons */
	if (middlep == false && best_fwd_hit < 0) {
	  /* Allow for a new start */
	  for (hit = 0; hit < npositions[curr_querypos]; hit++) {
	    currlink = &(links[curr_querypos][hit]);
#ifndef SEPARATE_FWD_REV
	    currlink->fwd_pos = currlink->fwd_hit = -1;
	    currlink->fwd_consecutive = indexsize_nt;
	    currlink->fwd_tracei = -1;
	    /* currlink->fwd_rootnlinks = 1; */
	    fwd_scores[curr_querypos][hit] = indexsize_nt;
#else
	    currlink->fwd_pos = currlink->fwd_hit = -1;
	    currlink->fwd_consecutive = indexsize_nt;
	    currlink->fwd_tracei = -1;
	    /* currlink->fwd_rootnlinks = 1; */
	    fwd_scores[curr_querypos][hit] = indexsize_nt;
	    if (splicingp == true) {
	      currlink->rev_pos = currlink->rev_hit = -1;
	      currlink->rev_consecutive = indexsize_nt;
	      currlink->rev_tracei = -1;
	      /* currlink->rev_rootnlinks = 1; */
	      rev_scores[curr_querypos][hit] = indexsize_nt;
	    }
#endif
	  }
	}
#endif

	if (splicingp == true && best_fwd_hit >= 0 && links[curr_querypos][best_fwd_hit].fwd_hit < 0 && 
	    grand_fwd_querypos <= querylength - indexsize_query && curr_querypos + indexsize_query <= grand_fwd_querypos) {
	  if ((best_fwd_score = fwd_scores[grand_fwd_querypos][grand_fwd_hit] - (grand_fwd_querypos - curr_querypos)) > 0) {
	    prevposition = mappings[grand_fwd_querypos][grand_fwd_hit];
	    debug12(printf("Considering prevposition %u to position %u as a grand fwd lookforward\n",prevposition,position));
	    for (hit = high_hit - 1; hit >= low_hit; --hit) {
	      if ((position = mappings[curr_querypos][hit]) + maxintronlen < prevposition) {
		debug12(printf("  => Too long\n"));
	      } else if (position + indexsize_nt <= prevposition) {
		currlink = &(links[curr_querypos][hit]);
		currlink->fwd_consecutive = indexsize_nt;
		currlink->fwd_pos = grand_fwd_querypos;
		currlink->fwd_hit = grand_fwd_hit;
		currlink->fwd_tracei = ++fwd_tracei;
		/* currlink->fwd_rootnlinks = 1; */
		fwd_scores[curr_querypos][hit] = best_fwd_score;
#ifdef DEBUG9
		prevlink = &(links[grand_fwd_querypos][grand_fwd_hit]);
		currlink->fwd_intronnfwd = prevlink->fwd_intronnfwd;
		currlink->fwd_intronnrev = prevlink->fwd_intronnrev;
		currlink->fwd_intronnunk = prevlink->fwd_intronnunk + 1;
#endif
	      }
	    }
	    debug12(printf("At querypos %d, setting all fwd hits to point back to grand_fwd %d,%d with a score of %d\n",
			   curr_querypos,grand_fwd_querypos,grand_fwd_hit,fwd_scores[grand_fwd_querypos][grand_fwd_hit]));
	  }
	}

	/* Use >= to favor longer path in case of ties */
	if (best_fwd_hit >= 0 && best_fwd_score >= grand_fwd_score && 
	    links[curr_querypos][best_fwd_hit].fwd_consecutive > EXON_DEFN) {
	  grand_fwd_score = best_fwd_score;
	  grand_fwd_querypos = curr_querypos;
	  grand_fwd_hit = best_fwd_hit;
	  debug12(termlink = &(links[curr_querypos][best_fwd_hit]));
	  debug12(printf("At querypos %d, revising grand fwd to be hit %d with score of %d (pointing back to %d,%d)\n",
			 curr_querypos,best_fwd_hit,best_fwd_score,termlink->fwd_pos,termlink->fwd_hit));
	}

#ifdef SEPARATE_FWD_REV
	if (best_rev_score > best_overall_score) {
	  best_overall_score = best_rev_score;
	}

	if (splicingp == false || use_canonical_p == false) {
	  /* rev scores should be the same as the fwd scores */
	} else {
	  if (best_rev_hit >= 0 && links[curr_querypos][best_rev_hit].rev_hit < 0 && 
	      grand_rev_querypos <= querylength - indexsize_query && curr_querypos + indexsize_query <= grand_rev_querypos) {
	    prevlink = &(links[grand_rev_querypos][grand_rev_hit]);
	    if ((best_rev_score = prevlink->rev_score - (grand_rev_querypos - curr_querypos)) > 0) {
	      prevposition = mappings[grand_rev_querypos][grand_rev_hit];
	      debug12(printf("Considering prevposition %u to position %u as a grand rev lookforward\n",prevposition,position));
	      for (hit = high_hit - 1; hit >= low_hit; --hit) {
		if ((position = mappings[curr_querypos][hit]) + maxintronlen < prevposition) {
		  debug12(printf("  => Too long\n"));
		} else if (position + indexsize_nt <= prevposition) {
		  currlink = &(links[curr_querypos][hit]);
		  currlink->rev_consecutive = indexsize_nt;
		  /* currlink->rev_rootnlinks = 1; */
		  currlink->rev_pos = grand_rev_querypos;
		  currlink->rev_hit = grand_rev_hit;
		  currlink->rev_score = best_rev_score;
#ifdef DEBUG9
		  currlink->rev_tracei = ++rev_tracei;
		  currlink->rev_intronnrev = prevlink->rev_intronnfwd;
		  currlink->rev_intronnrev = prevlink->rev_intronnrev;
		  currlink->rev_intronnunk = prevlink->rev_intronnunk + 1;
#endif
		}
	      }
	      debug12(printf("At querypos %d, setting all rev hits to point back to grand_rev %d,%d with a score of %d\n",
			     curr_querypos,grand_rev_querypos,grand_rev_hit,prevlink->rev_score));
	    }
	  }

	  /* Use >= to favor longer path in case of ties */
	  if (best_rev_hit >= 0 && best_rev_score >= grand_rev_score &&
	      links[curr_querypos][best_rev_hit].rev_consecutive > EXON_DEFN) {
	    grand_rev_score = best_rev_score;
	    grand_rev_querypos = curr_querypos;
	    grand_rev_hit = best_rev_hit;
	  }
	}
#endif
      }

      revise_active_lookforward(active,firstactive,nactive,low_hit,high_hit,fwd_scores,curr_querypos);

      /* Need to push curr_querypos, even if firstactive[curr_querypos] == -1 */
      /* Want to skip npositions[curr_querypos] == 0, so we can find adjacent despite mismatch or overabundance */
      if (npositions[curr_querypos] > 0) {
	debug6(printf("Pushing querypos %d onto processed\n",curr_querypos));
	processed = Intlist_push(processed,curr_querypos);
      }
      curr_querypos = next_querypos;
    }
  }
  debug9(printf("End of loop lookforward\n"));


  Intlist_free(&processed);

#if 0
  FREE(nactive);
  FREE(firstactive);
#endif

  if (oned_matrix_p == true) {
    intmatrix_1d_free(&active);
  } else {
    intmatrix_2d_free(&active,querylength);
  }


  /* Grand winners */
  debug12(printf("Finding grand winners, using root position method\n"));
#ifdef SEPARATE_FWD_REV
  if (splicingp == false || use_canonical_p == false) {
    cells = Linkmatrix_get_cells_fwd(&(*ncells),links,querystart,queryend,npositions,
				     favor_right_p,cellpool);
  } else {
    cells = Linkmatrix_get_cells_both(&(*ncells),links,querystart,queryend,npositions,
				      indexsize,best_overall_score,favor_right_p,cellpool);
  }
#else
  cells = get_cells_fwd(&(*ncells),links,fwd_scores,mappings,querystart,queryend,npositions,
			favor_right_p,cellpool);
#endif

  debug9(FREE(oligo));

  return cells;
}


/* Performs dynamic programming.  For PMAP, indexsize is in aa. */
static List_T
align_compute_lookforward (Chrpos_T **mappings, int *npositions, int totalpositions,
			   bool oned_matrix_p, Chrpos_T *minactive, Chrpos_T *maxactive,
			   int *firstactive, int *nactive, Cellpool_T cellpool,
			   char *queryseq_ptr, char *queryuc_ptr, int querylength, int querystart, int queryend,
			   Genome_T genome, Genome_T genomealt,
			   Univcoord_T chroffset, Univcoord_T chrhigh, bool plusp,
			   int indexsize, Pairpool_T pairpool,
#ifdef MOVE_TO_STAGE3
			   bool anchoredp, int anchor_querypos, Chrpos_T anchor_position,
#endif
			   bool localp, bool skip_repetitive_p, bool use_canonical_p, int non_canonical_penalty,
			   bool favor_right_p, bool middlep, int max_nalignments) {
  List_T all_paths = NULL;
  int npaths = 0;
  struct Link_T **links;
  int **fwd_scores;

#if 0
  bool anchoredp = false;
  int anchor_querypos = 0;
  Chrpos_T anchor_position = 0;
#endif

  Cell_T *cells, cell;
  int ncells, i;

  bool fwdp;
  int querypos, hit;
  int bestscore;
#ifdef SLOW
  int last_endposition;
#endif


  if (oned_matrix_p == true) {
    links = Linkmatrix_1d_new(querylength,npositions,totalpositions);
    fwd_scores = intmatrix_1d_new(querylength,npositions,totalpositions);
  } else {
    links = Linkmatrix_2d_new(querylength,npositions);
    fwd_scores = intmatrix_2d_new(querylength,npositions);
  }

  cells = align_compute_scores_lookforward(&ncells,links,fwd_scores,
					   mappings,npositions,totalpositions,
					   oned_matrix_p,minactive,maxactive,firstactive,nactive,cellpool,
					   querystart,queryend,querylength,
					   genome,genomealt,chroffset,chrhigh,plusp,

					   indexsize,
#ifdef DEBUG9
					   queryseq_ptr,
#endif
					   localp,skip_repetitive_p,use_canonical_p,non_canonical_penalty,
					   favor_right_p,middlep);
  /* cells are currently sorted by Cell_score_cmp in get_cells_fwd */

#ifdef SEPARATE_FWD_REV
  debug1(Linkmatrix_print_both(links,mappings,querylength,npositions,queryseq_ptr,indexsize));
#else
  debug1(print_fwd(links,fwd_scores,mappings,querylength,npositions,queryseq_ptr,indexsize));
#endif

  if (ncells == 0) {
    all_paths = (List_T) NULL;

  } else {
    /* High-scoring paths */
    bestscore = cells[0]->score;
    debug11(printf("** Looping on %d cells, allowing up to %d alignments, plus any with best score %d\n",
		   ncells,max_nalignments,bestscore));

    if (snps_p == true) {
      for (i = 0; i < ncells && (i < max_nalignments || cells[i]->score == bestscore)
	     && cells[i]->score > bestscore - FINAL_SCORE_TOLERANCE; i++) {
	cell = cells[i];
	if (cell->pushedp == false) {
	  querypos = cell->querypos;
	  hit = cell->hit;
	  fwdp = cell->fwdp;
	  debug11(printf("Starting subpath %d at rootposition %d with score %d, querypos %d, hit %d, endposition %d\n",
			 i,cell->rootposition,cell->score,querypos,hit,cell->endposition));
	  all_paths = List_push(all_paths,(void *) traceback_one_snps(querypos,hit,links,mappings,queryseq_ptr,
								      genome,genomealt,chroffset,chrhigh,/*watsonp*/plusp,
#ifdef DEBUG0
								      fwd_scores,indexsize,
#endif
								      pairpool,fwdp));
	  npaths++;
	  cell->pushedp = true;
	}
      }

    } else {
      for (i = 0; i < ncells && (i < max_nalignments || cells[i]->score == bestscore)
	     && cells[i]->score > bestscore - FINAL_SCORE_TOLERANCE; i++) {
	cell = cells[i];
	if (cell->pushedp == false) {
	  querypos = cell->querypos;
	  hit = cell->hit;
	  fwdp = cell->fwdp;
	  debug11(printf("Starting subpath %d at rootposition %d with score %d, querypos %d, hit %d, endposition %d\n",
			 i,cell->rootposition,cell->score,querypos,hit,cell->endposition));

	  all_paths = List_push(all_paths,(void *) traceback_one(querypos,hit,links,mappings,queryseq_ptr,queryuc_ptr,	
#ifdef PMAP
								 genome,genomealt,chroffset,chrhigh,/*watsonp*/plusp,/*lookbackp*/false,
#endif
#ifdef DEBUG0
								 fwd_scores,indexsize,
#endif
								 pairpool,fwdp));
	  npaths++;
	  cell->pushedp = true;
	}
      }

    }

#ifdef SLOW
    if (npaths < max_nalignments) {
      /* Non-overlapping paths */
      debug11(printf("** Looping on %d cells, looking for non-overlapping paths.  Total paths so far: %d\n",
		     ncells,npaths));
      qsort(cells,ncells,sizeof(Cell_T),Cell_interval_cmp);
      last_endposition = 0;
      if (snps_p == true) {
	for (i = 0; i < ncells && npaths < max_nalignments; i++) {
	  cell = cells[i];
	  if (cell->score > bestscore * NONOVERLAPPING_SCORE_TOLERANCE &&
	      cell->rootposition > last_endposition && cell->pushedp == false) {
	    debug11(printf("Starting subpath %d for rootposition %d with score %d, querypos %d, hit %d, endposition %d\n",
			   i,cell->rootposition,cell->score,querypos,hit,cell->endposition));
	    querypos = cell->querypos;
	    hit = cell->hit;
	    fwdp = cell->fwdp;
	    all_paths = List_push(all_paths,(void *) traceback_one_snps(querypos,hit,links,mappings,queryseq_ptr,
									genome,genomealt,chroffset,chrhigh,/*watsonp*/plusp,
#ifdef DEBUG0
									fwd_scores,indexsize,
#endif
									pairpool,fwdp));
	    npaths++;
	    cell->pushedp = true;
	    last_endposition = cell->endposition;
	  }
	}

      } else {
	for (i = 0; i < ncells && npaths < max_nalignments; i++) {
	  cell = cells[i];
	  if (cell->score > bestscore * NONOVERLAPPING_SCORE_TOLERANCE &&
	      cell->rootposition > last_endposition && cell->pushedp == false) {
	    debug11(printf("Starting subpath %d for rootposition %d with score %d, querypos %d, hit %d, endposition %d\n",
			   i,cell->rootposition,cell->score,querypos,hit,cell->endposition));
	    querypos = cell->querypos;
	    hit = cell->hit;
	    fwdp = cell->fwdp;
	    all_paths = List_push(all_paths,(void *) traceback_one(querypos,hit,links,mappings,queryseq_ptr,queryuc_ptr,	
#ifdef PMAP
								   genome,genomealt,chroffset,chrhigh,/*watsonp*/plusp,/*lookbackp*/false,
#endif
#ifdef DEBUG0
								   fwd_scores,indexsize,
#endif
								   pairpool,fwdp));
	    npaths++;
	    cell->pushedp = true;
	    last_endposition = cell->endposition;
	  }
	}
      }
    }
#endif

    debug11(printf("\n"));

#if 0
    /* No need with cellpool */
    for (i = 0; i < ncells; i++) {
      cell = cells[i];
      Cell_free(&cell);
    }
#endif
    FREE(cells);
  }


  if (oned_matrix_p == true) {
    Linkmatrix_1d_free(&links);
    intmatrix_1d_free(&fwd_scores);
  } else {
    Linkmatrix_2d_free(&links,querylength);
    intmatrix_2d_free(&fwd_scores,querylength);
  }

#if 0
  for (p = all_paths; p != NULL; p = List_next(p)) {
    Pair_dump_list(List_head(p),/*zerobasedp*/true);
    printf("\n");
  }
#endif

  return all_paths;
}


#if 0
/* Modified from stage3.c */
static void
get_splicesite_probs (double *left_prob, double *right_prob,
		      Chrpos_T left_genomepos, Chrpos_T right_genomepos,
		      Univcoord_T chroffset, Univcoord_T chrhigh, int genestrand, bool watsonp) {
  Univcoord_T splicesitepos;
  
  if (watsonp == true) {
    splicesitepos = chroffset + left_genomepos;
    if (genestrand > 0) {
      *left_prob = Maxent_hr_donor_prob(splicesitepos /*?*/+ 1,chroffset);
      debug5(printf("1. donor splicesitepos is %u (%u), prob %f\n",
		    splicesitepos,splicesitepos-chroffset,*left_prob));

    } else {
      *left_prob = Maxent_hr_antiacceptor_prob(splicesitepos /**/+ 1,chroffset);
      debug5(printf("2. antiacceptor splicesitepos is %u (%u), prob %f\n",
		    splicesitepos,splicesitepos-chroffset,*left_prob));

    }
  } else {
    splicesitepos = chrhigh - left_genomepos + 1;
    if (genestrand > 0) {
      *left_prob = Maxent_hr_acceptor_prob(splicesitepos /*?*/- 1,chroffset);
      debug5(printf("4. acceptor splicesitepos is %u (%u), prob %f\n",
		    splicesitepos,splicesitepos-chroffset,*left_prob));
    } else {
      *left_prob = Maxent_hr_antidonor_prob(splicesitepos /**/- 1,chroffset);
      debug5(printf("3. antidonor splicesitepos is %u (%u), prob %f\n",
		    splicesitepos,splicesitepos-chroffset,*left_prob));
    }
  }

  if (watsonp == true) {
    splicesitepos = chroffset + right_genomepos + 1;
    if (genestrand > 0) {
      *right_prob = Maxent_hr_acceptor_prob(splicesitepos /*?*/- 1,chroffset);
      debug5(printf("5. acceptor splicesitepos is %u (%u), prob %f\n",
		    splicesitepos,splicesitepos-chroffset,*right_prob));
    } else {
      *right_prob = Maxent_hr_antidonor_prob(splicesitepos /**/- 1,chroffset);
      debug5(printf("6. antidonor splicesitepos is %u (%u), prob %f\n",
		    splicesitepos,splicesitepos-chroffset,*right_prob));

    }
  } else {
    splicesitepos = chrhigh - right_genomepos;
    if (genestrand > 0) {
      *right_prob = Maxent_hr_donor_prob(splicesitepos /*?*/+ 1,chroffset);
      debug5(printf("8. donor splicesitepos is %u (%u), prob %f\n",
		    splicesitepos,splicesitepos-chroffset,*right_prob));
    } else {
      *right_prob = Maxent_hr_antiacceptor_prob(splicesitepos /**/+ 1,chroffset);
      debug5(printf("7. antiacceptor splicesitepos is %u (%u), prob %f\n",
		    splicesitepos,splicesitepos-chroffset,*right_prob));
    }
  }

  return;
}
#endif


/* queryseq_ptr is NULL for PMAP.  querypos here is in nt. */
static List_T
convert_to_nucleotides (List_T path,
#ifndef PMAP
			char *queryseq_ptr, char *queryuc_ptr, 
#endif
			Genome_T genome, Genome_T genomealt,
			Univcoord_T chroffset, Univcoord_T chrhigh, bool watsonp,
			int query_offset, Pairpool_T pairpool, int indexsize_nt,
			bool include_gapholders_p) {
  List_T pairs = NULL;
  Pair_T pair;
  int querypos, lastquerypos, queryjump, genomejump, fill, default_fill;
  int genomepos, lastgenomepos;
  char c, c_alt;

  debug5(printf("Beginning convert_to_nucleotides with %d pairs.  query_offset = %d, indexsize_nt = %d\n",
		List_length(path),query_offset,indexsize_nt));

  if (path == NULL) {
    return (List_T) NULL;
  } else {
    /* pairptr = path; */
    /* path = Pairpool_pop(path,&pair); */
    pair = (Pair_T) path->first;
    querypos = pair->querypos;
    genomepos = pair->genomepos;
  }

#ifdef PMAP
  default_fill = indexsize_nt - 3;
#else
  default_fill = indexsize_nt - 1;
#endif

  lastquerypos = querypos + default_fill;
  lastgenomepos = genomepos + default_fill;
  while (lastquerypos > querypos) {
    debug5(printf("querypos %d vs lastquerypos %d, lastgenomepos %d\n",querypos,lastquerypos,lastgenomepos));

#ifdef PMAP
    c = get_genomic_nt(&c_alt,genome,genomealt,lastgenomepos,chroffset,chrhigh,watsonp);
    pairs = Pairpool_push(pairs,pairpool,lastquerypos+query_offset,lastgenomepos,/*cdna*/c,MATCH_COMP,c,c_alt,
			  /*dynprogindex*/0);
    debug5(printf("Pushing %c | %c at %d,%d\n",c,c,lastquerypos,lastgenomepos));
#elif defined(EXTRACT_GENOMICSEG)
    if (queryuc_ptr[lastquerypos] == genomicuc_ptr[lastgenomepos]) {
      pairs = Pairpool_push(pairs,pairpool,lastquerypos+query_offset,lastgenomepos,
			    queryseq_ptr[lastquerypos],MISMATCH_COMP,
			    genomicseg_ptr[lastgenomepos],/*genomealt*/GENOMEALT_DEFERRED,
			    /*dynprogindex*/0);
      debug5(printf("Pushing %c | %c at %d,%d\n",queryseq_ptr[lastquerypos],queryuc_ptr[lastquerypos],
		    lastquerypos+query_offset,lastgenomepos));
    } else {
      abort();
      pairs = Pairpool_push(pairs,pairpool,lastquerypos+query_offset,lastgenomepos,
			    queryseq_ptr[lastquerypos],MISMATCH_COMP,
			    genomicseg_ptr[lastgenomepos],/*genomealt*/GENOMEALT_DEFERRED,
			    /*dynprogindex*/0);
      debug5(printf("Pushing %c   %c at %d,%d\n",queryseq_ptr[lastquerypos],genomicseg_ptr[lastgenomepos],
		    lastquerypos+query_offset,lastgenomepos));
    }
#else
    if (mode == STANDARD) {
      /* assert(queryuc_ptr[lastquerypos] == get_genomic_nt(&c_alt,lastgenomepos,chroffset,chrhigh,watsonp)); */
      c = get_genomic_nt(&c_alt,genome,genomealt,lastgenomepos,chroffset,chrhigh,watsonp);
      pairs = Pairpool_push(pairs,pairpool,lastquerypos+query_offset,lastgenomepos,
			    queryseq_ptr[lastquerypos],MATCH_COMP,c,c_alt,
			    /*dynprogindex*/0);
      debug5(printf("Pushing %c | %c at %d,%d\n",queryseq_ptr[lastquerypos],queryuc_ptr[lastquerypos],
		    lastquerypos+query_offset,lastgenomepos));
    } else {
      c = get_genomic_nt(&c_alt,genome,genomealt,lastgenomepos,chroffset,chrhigh,watsonp);
      if (queryuc_ptr[lastquerypos] == c) {
	pairs = Pairpool_push(pairs,pairpool,lastquerypos+query_offset,lastgenomepos,
			      queryseq_ptr[lastquerypos],MATCH_COMP,c,c_alt,/*dynprogindex*/0);
	debug5(printf("Pushing %c | %c at %d,%d\n",queryseq_ptr[lastquerypos],c,
		      lastquerypos+query_offset,lastgenomepos));
      } else {
	pairs = Pairpool_push(pairs,pairpool,lastquerypos+query_offset,lastgenomepos,
			      queryseq_ptr[lastquerypos],AMBIGUOUS_COMP,c,c_alt,/*dynprogindex*/0);
	debug5(printf("Pushing %c : %c at %d,%d\n",queryseq_ptr[lastquerypos],c,
		      lastquerypos+query_offset,lastgenomepos));
      }
    }
#endif
    --lastquerypos;
    --lastgenomepos;
  }

  /* Take care of observed first pair in oligomer */
  if (mode == STANDARD) {
    pair->querypos += query_offset; /* Revise coordinates */
    /*pair->genomepos += genomic_offset;*/ /* Revise coordinates */
#ifdef WASTE
    pairs = Pairpool_push_existing(pairs,pairpool,pair);
#else
    pairs = List_transfer_one(pairs,&path);
#endif
    debug5(printf("Transferring %c : %c at %d,%d (first pair)\n",pair->cdna,c,
		    pair->querypos+query_offset,pair->genomepos));
  } else {
    c = get_genomic_nt(&c_alt,genome,genomealt,pair->genomepos,chroffset,chrhigh,watsonp);
    if (pair->cdna == c) {
      pair->querypos += query_offset; /* Revise coordinates */
      /*pair->genomepos += genomic_offset;*/ /* Revise coordinates */
#ifdef WASTE
      pairs = Pairpool_push_existing(pairs,pairpool,pair);
#else
      pairs = List_transfer_one(pairs,&path);
#endif
      debug5(printf("Transferring %c : %c at %d,%d (first pair)\n",pair->cdna,c,
		    pair->querypos+query_offset,pair->genomepos));
    } else {
      path = Pairpool_pop(path,&pair);
      pairs = Pairpool_push(pairs,pairpool,pair->querypos+query_offset,pair->genomepos,
			    pair->cdna,AMBIGUOUS_COMP,c,c_alt,/*dynprogindex*/0);
      debug5(printf("Pushing %c : %c at %d,%d (first pair)\n",pair->cdna,c,
		    pair->querypos+query_offset,pair->genomepos));
    }
  }

  lastquerypos = querypos;
  lastgenomepos = genomepos;

  while (path != NULL) {
    /* pairptr = path; */
    /* path = Pairpool_pop(path,&pair); */
    pair = (Pair_T) path->first;
    querypos = pair->querypos;
    genomepos = pair->genomepos;
    
    queryjump = lastquerypos - 1 - querypos;
    genomejump = lastgenomepos - 1 - genomepos;

    if (queryjump == 0 && genomejump == 0) {
      /* Do nothing */
    } else {
      debug5(printf("At querypos %d, saw queryjump of %d and genomejump of %d\n",querypos,queryjump,genomejump));

      if (querypos + default_fill >= lastquerypos || genomepos + default_fill >= lastgenomepos) {
	if (lastquerypos - querypos < (int) (lastgenomepos - genomepos)) {
#if 0
	  /* This can occur with wobble mask */
	  fprintf(stderr,"Partial fill from querypos %d to %d (genomepos goes from %u to %u)\n",
		  querypos,lastquerypos,genomepos,lastgenomepos);
	  abort();
#endif
	  fill = lastquerypos - querypos - 1;
	} else {
#if 0
	  /* This can occur with wobble mask */
	  fprintf(stderr,"Partial fill from genomepos %u to %u (querypos goes from %d to %d)\n",
		  genomepos,lastgenomepos,querypos,lastquerypos);
	  abort();
#endif
	  fill = lastgenomepos - genomepos - 1;
	}
      } else {
	fill = default_fill;
      }

      /* Recompute queryjump and genomejump */
      queryjump -= fill;
      genomejump -= fill;
      debug5(printf("  Revised queryjump of %d and genomejump of %d\n",queryjump,genomejump));
      if (include_gapholders_p == true && (genomejump > 0 || queryjump > 0)) {
	debug5(printf("  Pushing gapholder\n"));
	pairs = Pairpool_push_gapholder(pairs,pairpool,queryjump,genomejump,
					/*leftpair*/NULL,/*rightpair*/NULL,/*knownp*/false);
#if 0
	/* Need to run on both genestrands, and save both results in pair */
	if (queryjump == 0) {
	  get_splicesite_probs(&left_prob,&right_prob,genomepos+fill,lastgenomepos,
			       chroffset,chrhigh,/*genestrand*/+1,watsonp);
	}
#endif
      }

      /* Fill rest of oligomer */
      lastquerypos = querypos + fill;
      lastgenomepos = genomepos + fill;
      debug5(printf("  Fill from querypos %d down to %d\n",lastquerypos,querypos));
      while (lastquerypos > querypos) {
#ifdef PMAP
	c = get_genomic_nt(&c_alt,genome,genomealt,lastgenomepos,chroffset,chrhigh,watsonp);
	pairs = Pairpool_push(pairs,pairpool,lastquerypos+query_offset,lastgenomepos,/*cdna*/c,MATCH_COMP,c,c_alt,
			      /*dynprogindex*/0);
	debug5(printf("Pushing %c | %c at %d,%d\n",c,c,lastquerypos+query_offset,lastgenomepos));
#elif defined(EXTRACT_GENOMICSEG)
	if (queryuc_ptr[lastquerypos] == genomicuc_ptr[lastgenomepos]) {
	  pairs = Pairpool_push(pairs,pairpool,lastquerypos+query_offset,lastgenomepos,
				queryseq_ptr[lastquerypos],MATCH_COMP,
				queryuc_ptr[lastquerypos],/*genomealt*/GENOMEALT_DEFERRED,
				/*dynprogindex*/0);
	  debug5(printf("Pushing %c | %c at %d,%d\n",queryseq_ptr[lastquerypos],genomicseg_ptr[lastgenomepos],
			lastquerypos+query_offset,lastgenomepos));
	} else {
	  abort();
	  pairs = Pairpool_push(pairs,pairpool,lastquerypos+query_offset,lastgenomepos,
				queryseq_ptr[lastquerypos],MISMATCH_COMP,
				genomicseg_ptr[lastgenomepos],/*genomealt*/GENOMEALT_DEFERRED,
				/*dynprogindex*/0);
	  debug5(printf("Pushing %c   %c at %d,%d\n",queryseq_ptr[lastquerypos],genomicseg_ptr[lastgenomepos],
			lastquerypos+query_offset,lastgenomepos));
	}
#else
	if (mode == STANDARD) {
	  c = get_genomic_nt(&c_alt,genome,genomealt,lastgenomepos,chroffset,chrhigh,watsonp);
	  pairs = Pairpool_push(pairs,pairpool,lastquerypos+query_offset,lastgenomepos,
				queryseq_ptr[lastquerypos],MATCH_COMP,c,c_alt,
				/*dynprogindex*/0);
	  debug5(printf("Pushing %c | %c at %d,%d\n",queryseq_ptr[lastquerypos],queryuc_ptr[lastquerypos],
			lastquerypos+query_offset,lastgenomepos));
	} else {
	  c = get_genomic_nt(&c_alt,genome,genomealt,lastgenomepos,chroffset,chrhigh,watsonp);
	  if (queryuc_ptr[lastquerypos] == c) {
	    pairs = Pairpool_push(pairs,pairpool,lastquerypos+query_offset,lastgenomepos,
				  queryseq_ptr[lastquerypos],MATCH_COMP,c,c_alt,/*dynprogindex*/0);
	    debug5(printf("Pushing %c | %c at %d,%d\n",queryseq_ptr[lastquerypos],c,
			  lastquerypos+query_offset,lastgenomepos));
	  } else {
	    pairs = Pairpool_push(pairs,pairpool,lastquerypos+query_offset,lastgenomepos,
				  queryseq_ptr[lastquerypos],AMBIGUOUS_COMP,c,c_alt,/*dynprogindex*/0);
	    debug5(printf("Pushing %c : %c at %d,%d\n",queryseq_ptr[lastquerypos],c,
			  lastquerypos+query_offset,lastgenomepos));
	  }
	}
#endif
	--lastquerypos;
	--lastgenomepos;
      }
    }

    /* Take care of observed match */
    if (mode == STANDARD) {
      pair->querypos += query_offset; /* Revise coordinates */
      /*pair->genomepos += genomic_offset;*/ /* Revise coordinates */
#ifdef WASTE
      pairs = Pairpool_push_existing(pairs,pairpool,pair);
#else
      pairs = List_transfer_one(pairs,&path);
#endif
      debug5(printf("Transferring %c : %c at %d,%d\n",pair->cdna,c,
		    pair->querypos+query_offset,pair->genomepos));
    } else {
      c = get_genomic_nt(&c_alt,genome,genomealt,pair->genomepos,chroffset,chrhigh,watsonp);
      if (pair->cdna == c) {
	pair->querypos += query_offset; /* Revise coordinates */
	/*pair->genomepos += genomic_offset;*/ /* Revise coordinates */
#ifdef WASTE
	pairs = Pairpool_push_existing(pairs,pairpool,pair);
#else
	pairs = List_transfer_one(pairs,&path);
#endif
	debug5(printf("Transferring %c : %c at %d,%d\n",pair->cdna,c,
		      pair->querypos+query_offset,pair->genomepos));
      } else {
	path = Pairpool_pop(path,&pair);
	pairs = Pairpool_push(pairs,pairpool,pair->querypos+query_offset,pair->genomepos,
			      pair->cdna,AMBIGUOUS_COMP,c,c_alt,/*dynprogindex*/0);
	debug5(printf("Pushing %c : %c at %d,%d (observed)\n",pair->cdna,c,
		      pair->querypos+query_offset,pair->genomepos));
      }
    }

    lastquerypos = querypos;
    lastgenomepos = genomepos;
  }

  debug5(Pair_dump_list(pairs,true));
  /* pairs is in ascending querypos order */
  return pairs;		      /* Used to return List_reverse(pairs) */
}


/* Returns ncovered */
int
Stage2_scan (int *stage2_source, char *queryuc_ptr, int querylength, Genome_T genome,
	     Chrpos_T chrstart, Chrpos_T chrend,
	     Univcoord_T chroffset, Univcoord_T chrhigh, bool plusp,
	     int genestrand, Stage2_alloc_T stage2_alloc, Oligoindex_array_T oligoindices,
	     Diagpool_T diagpool) {
  int ncovered;
  int source;
  /* int indexsize; */
  Oligoindex_T oligoindex;
  Chrpos_T **mappings;
  bool *coveredp, oned_matrix_p;
  int *npositions, totalpositions;
  double pct_coverage;
  int maxnconsecutive;
  /* double diag_runtime; */
  List_T diagonals;
#ifndef USE_DIAGPOOL
  List_p;
  Diag_T diag;
#endif
#ifdef EXTRACT_GENOMICSEG
  Count_T *counts;
#endif

  if (querylength > stage2_alloc->max_querylength_alloc) {
    coveredp = (bool *) CALLOC(querylength,sizeof(bool));
    mappings = (Chrpos_T **) MALLOC(querylength * sizeof(Chrpos_T *));
    npositions = (int *) CALLOC(querylength,sizeof(int));
  } else {
    coveredp = stage2_alloc->coveredp;
    mappings = stage2_alloc->mappings;
    npositions = stage2_alloc->npositions;

    memset(coveredp,0,querylength * sizeof(bool));
    memset(npositions,0,querylength * sizeof(int));
  }

  totalpositions = 0;
  maxnconsecutive = 0;

  source = 0;
  pct_coverage = 0.0;
  Diagpool_reset(diagpool);
  diagonals = (List_T) NULL;
  while (source < Oligoindex_array_length(oligoindices) && pct_coverage < SUFF_PCTCOVERAGE_OLIGOINDEX) {
    oligoindex = Oligoindex_array_elt(oligoindices,source);
    /* indexsize = Oligoindex_indexsize(oligoindex); */ /* Different sources can have different indexsizes */
#ifdef PMAP
    if (plusp == true) {
      Oligoindex_pmap_tally(oligoindex,/*mappingstart*/chroffset+chrstart,
			    /*mappingend*/chroffset+chrend,/*plusp*/true,
			    queryuc_ptr,/*querystart*/0,/*queryend*/querylength,
			    /*chrpos*/chrstart);
    } else {
      /* Need to add 1 to mappingend to cover same range as plusp */
      Oligoindex_pmap_tally(oligoindex,/*mappingstart*/chroffset+chrstart,
			    /*mappingend*/chroffset+chrend+1,/*plusp*/false,
			    queryuc_ptr,/*querystart*/0,/*queryend*/querylength,
			    /*chrpos*/(chrhigh-chroffset)-chrend);
    }

#else

#ifdef EXTRACT_GENOMICSEG
    Oligoindex_hr_tally(oligoindex,genomicuc_ptr,/*genomiclength*/chrend-chrstart,queryuc_ptr,querylength,
			/*sequencepos*/0);
    counts = Oligoindex_counts_copy(oligoindex);
#endif

    if (plusp == true) {
      Oligoindex_hr_tally(oligoindex,/*mappingstart*/chroffset+chrstart,
			  /*mappingend*/chroffset+chrend,/*plusp*/true,
			  queryuc_ptr,/*querystart*/0,/*queryend*/querylength,
			  /*chrpos*/chrstart,genome,genestrand);
    } else {
      /* Need to add 1 to mappingend to cover same range as plusp */
      Oligoindex_hr_tally(oligoindex,/*mappingstart*/chroffset+chrstart,
			  /*mappingend*/chroffset+chrend+1,/*plusp*/false,
			  queryuc_ptr,/*querystart*/0,/*queryend*/querylength,
			  /*chrpos*/(chrhigh-chroffset)-chrend,genome,genestrand);
    }

#ifdef EXTRACT_GENOMICSEG
    assert(Oligoindex_counts_equal(oligoindex,counts));
    /* Oligoindex_counts_dump(oligoindex,counts); */
    FREE(counts);
#endif

#endif

    diagonals = Oligoindex_get_mappings(diagonals,coveredp,mappings,npositions,&totalpositions,
					&oned_matrix_p,&maxnconsecutive,oligoindices,oligoindex,queryuc_ptr,
					/*querystart*/0,/*queryend*/querylength,querylength,
					chrstart,chrend,chroffset,chrhigh,plusp,diagpool);
    pct_coverage = Diag_update_coverage(coveredp,&ncovered,diagonals,querylength);
    debug(printf("Stage2_scan: source = %d, ncovered = %d, pct_coverage = %f\n",source,ncovered,pct_coverage));

    source++;
  }
  *stage2_source = source;

#ifdef USE_DIAGPOOL
  /* No need to free diagonals */
#else
  for (p = diagonals; p != NULL; p = List_next(p)) {
    diag = (Diag_T) List_head(p);
    Diag_free(&diag);
  }
  List_free(&diagonals);
#endif

  if (querylength > stage2_alloc->max_querylength_alloc) {
    FREE(npositions);
    FREE(coveredp);
    FREE(mappings);		/* Don't need to free contents of mappings */
  }

#if 1
  for (source = 0; source < Oligoindex_array_length(oligoindices); source++) {
    oligoindex = Oligoindex_array_elt(oligoindices,source);
    Oligoindex_untally(oligoindex);
  }
#endif

  return ncovered;
}



static int
stage2_cmp (const void *a, const void *b) {
  Stage2_T xx = * (Stage2_T *) a;
  Stage2_T yy = * (Stage2_T *) b;
  List_T x = xx->middle, y = yy->middle;
  Chrpos_T x_chrstart, x_chrend, y_chrstart, y_chrend;

  x_chrstart = ((Pair_T) x->first)->genomepos;
  x_chrend = ((Pair_T) List_last_value(x,NULL))->genomepos;
  assert(x_chrstart <= x_chrend); /* Equal if there is only one pair in the list */

  y_chrstart = ((Pair_T) y->first)->genomepos;
  y_chrend = ((Pair_T) List_last_value(y,NULL))->genomepos;
  assert(y_chrstart <= y_chrend); /* Equal if there is only one pair in the list */

  if (x_chrstart < y_chrstart) {
    return -1;
  } else if (y_chrstart < x_chrstart) {
    return +1;

    /* Want most compact representation */
  } else if (x_chrend < y_chrend) {
    return -1;
  } else if (y_chrend < x_chrend) {
    return +1;
    
  } else {
    return 0;
  }
}


/* paths, so chrend is first */
static int
stage2pairs_start_cmp (const void *a, const void *b) {
  List_T x = * (List_T *) a;
  List_T y = * (List_T *) b;
  Chrpos_T x_chrstart, x_chrend, y_chrstart, y_chrend;

  x_chrend = ((Pair_T) x->first)->genomepos;
  x_chrstart = ((Pair_T) List_last_value(x,NULL))->genomepos;
  assert(x_chrstart <= x_chrend); /* Equal if there is only one pair in the list */

  y_chrend = ((Pair_T) y->first)->genomepos;
  y_chrstart = ((Pair_T) List_last_value(y,NULL))->genomepos;
  assert(y_chrstart <= y_chrend); /* Equal if there is only one pair in the list */

  if (x_chrend > y_chrend) {
    return -1;
  } else if (y_chrend > x_chrend) {
    return +1;

    /* Want most compact representation */
  } else if (x_chrstart > y_chrstart) {
    return -1;
  } else if (y_chrstart > x_chrstart) {
    return +1;
    
  } else {
    return 0;
  }
}


static int
stage2pairs_end_cmp (const void *a, const void *b) {
  List_T x = * (List_T *) a;
  List_T y = * (List_T *) b;

  Chrpos_T x_chrstart, x_chrend, y_chrstart, y_chrend;

  x_chrstart = ((Pair_T) x->first)->genomepos;
  x_chrend = ((Pair_T) List_last_value(x,NULL))->genomepos;
  assert(x_chrstart <= x_chrend); /* Equal if there is only one pair in the list */

  y_chrstart = ((Pair_T) y->first)->genomepos;
  y_chrend = ((Pair_T) List_last_value(y,NULL))->genomepos;
  assert(y_chrstart <= y_chrend); /* Equal if there is only one pair in the list */

  if (x_chrstart < y_chrstart) {
    return -1;
  } else if (y_chrstart < x_chrstart) {
    return +1;

    /* Want most compact representation */
  } else if (x_chrend < y_chrend) {
    return -1;
  } else if (y_chrend < x_chrend) {
    return +1;
    
  } else {
    return 0;
  }
}


/* Modified from gregion_overlap_p */
static bool
stage2path_overlap_p (List_T x, List_T y) {
  Chrpos_T x_chrstart, x_chrend, y_chrstart, y_chrend;
  Chrpos_T overlap;
  double fraction;

  x_chrend = ((Pair_T) x->first)->genomepos;
  x_chrstart = ((Pair_T) List_last_value(x,NULL))->genomepos;
  assert(x_chrstart <= x_chrend); /* Equal if there is only one pair in the list */

  y_chrend = ((Pair_T) y->first)->genomepos;
  y_chrstart = ((Pair_T) List_last_value(y,NULL))->genomepos;
  assert(y_chrstart <= y_chrend); /* Equal if there is only one pair in the list */

  if (y_chrstart > x_chrend || x_chrstart > y_chrend) {
    debug13a(printf("x %u..%u, y %u..%u => no overlap\n",x_chrstart,x_chrend,y_chrstart,y_chrend));
    /*
      /-- x --/ /-- y --/ or /-- y --/ /-- x --/
    */
    return false;		/* No overlap */

  } else if (y_chrstart < x_chrstart) {
    debug13a(printf("x %u..%u, y %u..%u",x_chrstart,x_chrend,y_chrstart,y_chrend));
    if (y_chrend < x_chrend) {
      /*
	/-- x --/
	/-- y --/
      */
      overlap = y_chrend - x_chrstart;
      if (y_chrend - y_chrstart < x_chrend - x_chrstart) {
	fraction = (double) overlap/(double) (y_chrend - y_chrstart);
      } else {
	fraction = (double) overlap/(double) (x_chrend - x_chrstart);
      }
      debug13a(printf(" => fraction %f",fraction));
      if (fraction > 0.5) {
	debug13a(printf(" => overlap\n",fraction));
	return true;
      } else {
	debug13a(printf(" => no overlap\n",fraction));
	return false;
      }

    } else {
      /*
	/-- x --/
	/----- y -----/
      */
      debug13a(printf(" => subsumption\n"));
      return true;
    }

  } else {
    debug13a(printf("x %u..%u, y %u..%u\n",x_chrstart,x_chrend,y_chrstart,y_chrend));
    if (y_chrend < x_chrend) {
      /*
	/----- x -----/
	/-- y --/
      */
      debug13a(printf(" => subsumption\n"));
      return true;

    } else {
      /*
	/-- x --/
	/-- y --/
      */
      overlap = x_chrend - y_chrstart;
      if (y_chrend - y_chrstart < x_chrend - x_chrstart) {
	fraction = (double) overlap/(double) (y_chrend - y_chrstart);
      } else {
	fraction = (double) overlap/(double) (x_chrend - x_chrstart);
      }
      debug13a(printf(" => fraction %f",fraction));
      if (fraction > 0.5) {
	debug13a(printf(" => overlap\n",fraction));
	return true;
      } else {
	debug13a(printf(" => no overlap\n",fraction));
	return false;
      }

    }
  }
}


/* Modified from gregion_overlap_p */
static bool
stage2pairs_overlap_p (List_T x, List_T y) {
  Chrpos_T x_chrstart, x_chrend, y_chrstart, y_chrend;
  Chrpos_T overlap;
  double fraction;

  x_chrstart = ((Pair_T) x->first)->genomepos;
  x_chrend = ((Pair_T) List_last_value(x,NULL))->genomepos;
  assert(x_chrstart <= x_chrend); /* Equal if there is only one pair in the list */

  y_chrstart = ((Pair_T) y->first)->genomepos;
  y_chrend = ((Pair_T) List_last_value(y,NULL))->genomepos;
  assert(y_chrstart <= y_chrend); /* Equal if there is only one pair in the list */

  if (y_chrstart > x_chrend || x_chrstart > y_chrend) {
    debug13a(printf("x %u..%u, y %u..%u => no overlap\n",x_chrstart,x_chrend,y_chrstart,y_chrend));
    /*
      /-- x --/ /-- y --/ or /-- y --/ /-- x --/
    */
    return false;		/* No overlap */

  } else if (y_chrstart < x_chrstart) {
    debug13a(printf("x %u..%u, y %u..%u",x_chrstart,x_chrend,y_chrstart,y_chrend));
    if (y_chrend < x_chrend) {
      /*
	/-- x --/
	/-- y --/
      */
      overlap = y_chrend - x_chrstart;
      if (y_chrend - y_chrstart < x_chrend - x_chrstart) {
	fraction = (double) overlap/(double) (y_chrend - y_chrstart);
      } else {
	fraction = (double) overlap/(double) (x_chrend - x_chrstart);
      }
      debug13a(printf(" => fraction %f",fraction));
      if (fraction > 0.5) {
	debug13a(printf(" => overlap\n",fraction));
	return true;
      } else {
	debug13a(printf(" => no overlap\n",fraction));
	return false;
      }

    } else {
      /*
	/-- x --/
	/----- y -----/
      */
      debug13a(printf(" => subsumption\n"));
      return true;
    }

  } else {
    debug13a(printf("x %u..%u, y %u..%u\n",x_chrstart,x_chrend,y_chrstart,y_chrend));
    if (y_chrend < x_chrend) {
      /*
	/----- x -----/
	/-- y --/
      */
      debug13a(printf(" => subsumption\n"));
      return true;

    } else {
      /*
	/-- x --/
	/-- y --/
      */
      overlap = x_chrend - y_chrstart;
      if (y_chrend - y_chrstart < x_chrend - x_chrstart) {
	fraction = (double) overlap/(double) (y_chrend - y_chrstart);
      } else {
	fraction = (double) overlap/(double) (x_chrend - x_chrstart);
      }
      debug13a(printf(" => fraction %f",fraction));
      if (fraction > 0.5) {
	debug13a(printf(" => overlap\n",fraction));
	return true;
      } else {
	debug13a(printf(" => no overlap\n",fraction));
	return false;
      }

    }
  }
}



static List_T
Stage2_filter_unique (List_T all_stage2results) {
  List_T unique = NULL;
  Stage2_T *array, stage2, xx, yy;
  int n, i, j;
  bool *eliminate;
#ifdef DEBUG
  List_T p, q;
#endif

  n = List_length(all_stage2results);
  debug13(printf("Entering Stage2_filter_unique with %d results\n",n));

  if (n == 0) {
    return NULL;
  }

#ifdef DEBUG13
  for (p = all_stage2results; p != NULL; p = List_next(p)) {
    stage2 = (Stage2_T) List_head(p);
    stage2pairs = stage2->middle;
    printf("Stage 2 list at chrstart %u, chrend %u)\n",
	   ((Pair_T) stage2pairs->first)->genomepos,
	   ((Pair_T) List_last_value(stage2pairs,NULL))->genomepos);
  }
#endif

  eliminate = (bool *) CALLOC(n,sizeof(bool));
  array = (Stage2_T *) List_to_array(all_stage2results,NULL);
  List_free(&all_stage2results);
  qsort(array,n,sizeof(Stage2_T),stage2_cmp);

#ifdef DEBUG13
  for (i = 0; i < n; i++) {
    stage2 = array[i];
    stage2pairs = stage2->middle;
    printf("%d: Stage 2 list at chrstart %u, chrend %u)\n",
	   i,((Pair_T) stage2pairs->first)->genomepos,
	   ((Pair_T) List_last_value(stage2pairs,NULL))->genomepos);
  }
#endif


  for (i = 0; i < n; i++) {
    xx = array[i];
    for (j = i+1; j < n; j++) {
      yy = array[j];
      if (stage2pairs_overlap_p(xx->middle,yy->middle) == true) {
#if 0
	printf("Found overlap between these regions:\n");
	printf("   ");
	printf("chrstart %u, chrend %u",
	       ((Pair_T) xx->middle->first)->genomepos,
	       ((Pair_T) List_last_value(xx->middle,NULL))->genomepos);
	printf("   ");
	printf("chrstart %u, chrend %u",
	       ((Pair_T) yy->middle->first)->genomepos,
	       ((Pair_T) List_last_value(yy->middle,NULL))->genomepos);
	printf("\n");
#endif
	eliminate[j] = true;
      }
    }
  }

  for (i = n-1; i >= 0; i--) {
    stage2 = array[i];
    if (eliminate[i] == false) {
#if 0
      debug13(printf("Keeping chrstart %u, chrend %u",
		     ((Pair_T) stage2pairs->first)->genomepos,
		     ((Pair_T) List_last_value(stage2pairs,NULL))->genomepos));
#endif
      unique = List_push(unique,(void *) stage2);
    } else {
#if 0
      debug13(printf("Eliminating chrstart %u, chrend %u",
		     ((Pair_T) stage2pairs->first)->genomepos,
		     ((Pair_T) List_last_value(stage2pairs,NULL))->genomepos));
#endif
      Stage2_free(&stage2);
    }
  }

  FREE(eliminate);
  FREE(array);

#ifdef DEBUG13
  for (p = unique, i = 0; p != NULL; p = p->rest, i++) {
    stage2 = (Stage2_T) p->first;
    stage2pairs = stage2->middle;
    printf("Final: chrstart %u, chrend %u\n",
	   ((Pair_T) stage2pairs->first)->genomepos,
	   ((Pair_T) List_last_value(stage2pairs,NULL))->genomepos);
  }
#endif

  return unique;
}


static List_T
Stage2pairs_filter_unique_starts (List_T all_results) {
  List_T unique = NULL;
  List_T *array, stage2pairs, x, y;
  int n, i, j;
  bool *eliminate, eliminatep = false;
#ifdef DEBUG
  List_T p, q;
#endif

  n = List_length(all_results);
  debug13(printf("Entering Stage2_filter_unique_starts with %d results\n",n));

  if (n == 0) {
    return NULL;
  }

#ifdef DEBUG13
  for (p = all_results; p != NULL; p = List_next(p)) {
    stage2pairs = (List_T) List_head(p);
    printf("Stage 2 list at chrstart %u, chrend %u)\n",
	   ((Pair_T) stage2pairs->first)->genomepos,
	   ((Pair_T) List_last_value(stage2pairs,NULL))->genomepos);
  }
#endif

  eliminate = (bool *) CALLOC(n,sizeof(bool));
  array = (List_T *) List_to_array(all_results,NULL);
  List_free(&all_results);
  qsort(array,n,sizeof(List_T),stage2pairs_start_cmp);

#ifdef DEBUG13
  for (i = 0; i < n; i++) {
    stage2pairs = array[i];
    printf("%d: Stage 2 list at chrstart %u, chrend %u)\n",
	   i,((Pair_T) stage2pairs->first)->genomepos,
	   ((Pair_T) List_last_value(stage2pairs,NULL))->genomepos);
  }
#endif


  for (i = 0; i < n; i++) {
    x = array[i];
    for (j = i+1; j < n; j++) {
      y = array[j];
      if (stage2path_overlap_p(x,y) == true) {
#if 0
	printf("Found overlap between these regions:\n");
	printf("   ");
	printf("chrstart %u, chrend %u",
	       ((Pair_T) x->first)->genomepos,
	       ((Pair_T) List_last_value(x,NULL))->genomepos);
	printf("   ");
	printf("chrstart %u, chrend %u",
	       ((Pair_T) y->first)->genomepos,
	       ((Pair_T) List_last_value(y,NULL))->genomepos);
	printf("\n");
#endif
	eliminate[j] = true;
	eliminatep = true;
      }
    }
  }

  if (eliminatep == false) {
    /* All are identical, so take the first one only */
    unique = List_push(unique,(void *) array[0]);
  } else {
    for (i = n-1; i >= 0; i--) {
      stage2pairs = array[i];
      if (eliminate[i] == false) {
#if 0
	debug13(printf("Keeping chrstart %u, chrend %u",
		       ((Pair_T) stage2pairs->first)->genomepos,
		       ((Pair_T) List_last_value(stage2pairs,NULL))->genomepos));
#endif
	unique = List_push(unique,(void *) stage2pairs);
      } else {
#if 0
	debug13(printf("Eliminating chrstart %u, chrend %u",
		       ((Pair_T) stage2pairs->first)->genomepos,
		       ((Pair_T) List_last_value(stage2pairs,NULL))->genomepos));
#endif
	/* List_free(&stage2pairs); */
      }
    }
  }

  FREE(eliminate);
  FREE(array);

#ifdef DEBUG13
  for (p = unique, i = 0; p != NULL; p = p->rest, i++) {
    stage2pairs = (List_T) p->first;
    printf("Final: chrstart %u, chrend %u\n",
	   ((Pair_T) stage2pairs->first)->genomepos,
	   ((Pair_T) List_last_value(stage2pairs,NULL))->genomepos);
  }
#endif

  return unique;
}


static List_T
Stage2pairs_filter_unique_ends (List_T all_results) {
  List_T unique = NULL;
  List_T *array, stage2pairs, x, y;
  int n, i, j;
  bool *eliminate, eliminatep = false;
#ifdef DEBUG
  List_T p, q;
#endif

  n = List_length(all_results);
  debug13(printf("Entering Stage2_filter_unique_ends with %d results\n",n));

  if (n == 0) {
    return NULL;
  }

#ifdef DEBUG13
  for (p = all_results; p != NULL; p = List_next(p)) {
    stage2pairs = (List_T) List_head(p);
    printf("Stage 2 list at chrstart %u, chrend %u)\n",
	   ((Pair_T) stage2pairs->first)->genomepos,
	   ((Pair_T) List_last_value(stage2pairs,NULL))->genomepos);
  }
#endif

  eliminate = (bool *) CALLOC(n,sizeof(bool));
  array = (List_T *) List_to_array(all_results,NULL);
  List_free(&all_results);
  qsort(array,n,sizeof(List_T),stage2pairs_end_cmp);

#ifdef DEBUG13
  for (i = 0; i < n; i++) {
    stage2pairs = array[i];
    printf("%d: Stage 2 list at chrstart %u, chrend %u)\n",
	   i,((Pair_T) stage2pairs->first)->genomepos,
	   ((Pair_T) List_last_value(stage2pairs,NULL))->genomepos);
  }
#endif


  for (i = 0; i < n; i++) {
    x = array[i];
    for (j = i+1; j < n; j++) {
      y = array[j];
      if (stage2pairs_overlap_p(x,y) == true) {
#if 0
	printf("Found overlap between these regions:\n");
	printf("   ");
	printf("chrstart %u, chrend %u",
	       ((Pair_T) x->first)->genomepos,
	       ((Pair_T) List_last_value(x,NULL))->genomepos);
	printf("   ");
	printf("chrstart %u, chrend %u",
	       ((Pair_T) y->first)->genomepos,
	       ((Pair_T) List_last_value(y,NULL))->genomepos);
	printf("\n");
#endif
	eliminate[j] = true;
	eliminatep = true;
      }
    }
  }

  if (eliminatep == false) {
    /* All are identical, so take the first one only */
    unique = List_push(unique,(void *) array[0]);
  } else {
    for (i = n-1; i >= 0; i--) {
      stage2pairs = array[i];
      if (eliminate[i] == false) {
#if 0
	debug13(printf("Keeping chrstart %u, chrend %u",
		       ((Pair_T) stage2pairs->first)->genomepos,
		       ((Pair_T) List_last_value(stage2pairs,NULL))->genomepos));
#endif
	unique = List_push(unique,(void *) stage2pairs);
      } else {
#if 0
	debug13(printf("Eliminating chrstart %u, chrend %u",
		       ((Pair_T) stage2pairs->first)->genomepos,
		       ((Pair_T) List_last_value(stage2pairs,NULL))->genomepos));
#endif
	/* List_free(&stage2pairs); */
      }
    }
  }

  FREE(eliminate);
  FREE(array);

#ifdef DEBUG13
  for (p = unique, i = 0; p != NULL; p = p->rest, i++) {
    stage2pairs = (List_T) p->first;
    printf("Final: chrstart %u, chrend %u\n",
	   ((Pair_T) stage2pairs->first)->genomepos,
	   ((Pair_T) List_last_value(stage2pairs,NULL))->genomepos);
  }
#endif

  return unique;
}





List_T
Stage2_compute (char *queryseq_ptr, char *queryuc_ptr, int querylength, int query_offset,	
		Chrpos_T chrstart, Chrpos_T chrend,
		Univcoord_T chroffset, Univcoord_T chrhigh, bool plusp, int genestrand,
#ifndef GSNAP
		Stage2_alloc_T stage2_alloc, double proceed_pctcoverage,
#endif
		Oligoindex_array_T oligoindices,
		Genome_T genome, Genome_T genomealt, Pairpool_T pairpool,
		Diagpool_T diagpool, Cellpool_T cellpool,
		bool localp, bool skip_repetitive_p,
		bool favor_right_p, int max_nalignments,
		Stopwatch_T stopwatch, bool diag_debug) {
  List_T all_stage2results = NULL, all_paths, all_ends, all_starts, path, pairs, p;
  List_T middle;
  /* Pair_T firstpair, lastpair; */
  int diag_querystart, diag_queryend;
  int indexsize, indexsize_nt;
  Oligoindex_T oligoindex;
  Chrpos_T **mappings;
  bool *coveredp, oned_matrix_p;
  int source;
  int *npositions, totalpositions;
  Chrpos_T *minactive, *maxactive;
  int *firstactive, *nactive;
  int maxnconsecutive;
  /* double diag_runtime; */
  List_T diagonals;
  /* int anchor_querypos, querystart, queryend; */
  /* Chrpos_T anchor_position; */
#ifdef GSNAP
  Univcoord_T mappingstart, mappingend;
  Chrpos_T chrpos, mappinglength;
#else
  double pct_coverage;
  int ncovered;
#endif


#ifndef USE_DIAGPOOL
  List_T p;
  Diag_T diag;
#endif
#ifdef DEBUG
  int nunique;
#endif
#ifdef DEBUG0
  int i;
#endif

#ifdef EXTRACT_GENOMICSEG
  Count_T *counts;
#endif

  debug(printf("Entered Stage2_compute with chrstart %u and chrend %u\n",chrstart,chrend));

  Stopwatch_start(stopwatch);

#ifdef GSNAP
  coveredp = (bool *) CALLOCA(querylength,sizeof(bool));
  mappings = (Chrpos_T **) MALLOCA(querylength * sizeof(Chrpos_T *));
  npositions = (int *) CALLOCA(querylength,sizeof(int));
  minactive = (unsigned int *) MALLOCA(querylength * sizeof(unsigned int));
  maxactive = (unsigned int *) MALLOCA(querylength * sizeof(unsigned int));
  firstactive = (int *) MALLOCA(querylength * sizeof(int));
  nactive = (int *) MALLOCA(querylength * sizeof(int));
#else
  if (querylength > stage2_alloc->max_querylength_alloc) {
    coveredp = (bool *) CALLOC(querylength,sizeof(bool));
    mappings = (Chrpos_T **) MALLOC(querylength * sizeof(Chrpos_T *));
    npositions = (int *) CALLOC(querylength,sizeof(int));
    minactive = (unsigned int *) MALLOC(querylength * sizeof(unsigned int));
    maxactive = (unsigned int *) MALLOC(querylength * sizeof(unsigned int));
    firstactive = (int *) MALLOC(querylength * sizeof(int));
    nactive = (int *) MALLOC(querylength * sizeof(int));
  } else {
    coveredp = stage2_alloc->coveredp;
    mappings = stage2_alloc->mappings;
    npositions = stage2_alloc->npositions;
    minactive = stage2_alloc->minactive;
    maxactive = stage2_alloc->maxactive;
    firstactive = stage2_alloc->firstactive;
    nactive = stage2_alloc->nactive;

    memset(coveredp,0,querylength * sizeof(bool));
    memset(npositions,0,querylength * sizeof(int));
  }
#endif

  totalpositions = 0;
  maxnconsecutive = 0;

  source = 0;
#ifdef USE_DIAGPOOL
  Diagpool_reset(diagpool);
#endif
  Cellpool_reset(cellpool);
  diagonals = (List_T) NULL;


#ifdef GSNAP
  mappingstart = chroffset + chrstart;
  if (plusp == true) {
    mappingend = chroffset + chrend;
    chrpos = chrstart;
  } else {
    mappingend = chroffset + chrend + 1;
    chrpos = (chrhigh - chroffset) - chrend;
  }
  mappinglength = (Chrpos_T) (mappingend - mappingstart);

  if (mappinglength > 100000) {
    /* 9-mers */
    source = 0;
  } else if (mappinglength > 10000) {
    /* 8-mers */
    source = 1;
  } else {
    /* 7-mers */
    source = 2;
  }

  oligoindex = Oligoindex_array_elt(oligoindices,source);
  indexsize = Oligoindex_indexsize(oligoindex); /* Different sources can have different indexsizes */
  /* printf("indexsize = %d\n",indexsize); */


  Oligoindex_hr_tally(oligoindex,mappingstart,mappingend,plusp,
		      queryuc_ptr,/*querystart*/0,/*queryend*/querylength,
		      chrpos,genome,genestrand);

  diagonals = Oligoindex_get_mappings(diagonals,coveredp,mappings,npositions,&totalpositions,
				      &oned_matrix_p,&maxnconsecutive,oligoindices,oligoindex,queryuc_ptr,
				      /*querystart*/0,/*queryend*/querylength,querylength,
				      chrstart,chrend,chroffset,chrhigh,plusp,diagpool);
#else
  /* GMAP */
  pct_coverage = 0.0;
  while (source < Oligoindex_array_length(oligoindices) && pct_coverage < SUFF_PCTCOVERAGE_OLIGOINDEX) {
    oligoindex = Oligoindex_array_elt(oligoindices,source);
    indexsize = Oligoindex_indexsize(oligoindex); /* Different sources can have different indexsizes */

#ifdef PMAP
    if (plusp == true) {
      Oligoindex_pmap_tally(oligoindex,/*mappingstart*/chroffset+chrstart,
			    /*mappingend*/chroffset+chrend,/*plusp*/true,
			    queryuc_ptr,/*querystart*/0,/*queryend*/querylength,
			    /*chrpos*/chrstart);
    } else {
      Oligoindex_pmap_tally(oligoindex,/*mappingstart*/chroffset+chrstart,
			    /*mappingend*/chroffset+chrend+1,/*plusp*/false,
			    queryuc_ptr,/*querystart*/0,/*queryend*/querylength,
			    /*chrpos*/(chrhigh-chroffset)-chrend);
    }
#else
    if (plusp == true) {
      Oligoindex_hr_tally(oligoindex,/*mappingstart*/chroffset+chrstart,
			  /*mappingend*/chroffset+chrend,/*plusp*/true,
			  queryuc_ptr,/*querystart*/0,/*queryend*/querylength,
			  /*chrpos*/chrstart,genome,genestrand);
    } else {
      Oligoindex_hr_tally(oligoindex,/*mappingstart*/chroffset+chrstart,
			  /*mappingend*/chroffset+chrend+1,/*plusp*/false,
			  queryuc_ptr,/*querystart*/0,/*queryend*/querylength,
			  /*chrpos*/(chrhigh-chroffset)-chrend,genome,genestrand);
    }
#endif

    diagonals = Oligoindex_get_mappings(diagonals,coveredp,mappings,npositions,&totalpositions,
					&oned_matrix_p,&maxnconsecutive,oligoindices,oligoindex,queryuc_ptr,
					/*querystart*/0,/*queryend*/querylength,querylength,
					chrstart,chrend,chroffset,chrhigh,plusp,diagpool);
    pct_coverage = Diag_update_coverage(coveredp,&ncovered,diagonals,querylength);
    debug(printf("Stage2_compute: source = %d, ndiagonals = %d, ncovered = %d, pct_coverage = %f\n",
		 source,List_length(diagonals),ncovered,pct_coverage));

    source++;
  }

#endif

  /* *stage2_source = source; */
  /* *stage2_indexsize = indexsize; */
#ifdef PMAP
  indexsize_nt = 3*indexsize;
#else
  indexsize_nt = indexsize;
#endif

  /* diag_runtime = */ Stopwatch_stop(stopwatch);

  Stopwatch_start(stopwatch);

  if (diag_debug == true) {
    /* Do nothing */
    middle = (List_T) NULL;

  } else if (totalpositions == 0) {
    debug(printf("Quitting because totalpositions is zero\n"));
    middle = (List_T) NULL;

#ifndef GSNAP
  } else if (querylength > 150 && pct_coverage < proceed_pctcoverage && ncovered < SUFF_NCOVERED) {
    /* Filter only on long queries */
    debug(printf("Quitting because querylength %d > 150, and pct_coverage is only %f < %f, and ncovered is only %d < %d, maxnconsecutive = %d\n",
		 querylength,pct_coverage,proceed_pctcoverage,ncovered,SUFF_NCOVERED,maxnconsecutive));
    middle = (List_T) NULL;
#endif

  } else {
    debug(printf("Proceeding because maxnconsecutive is %d and pct_coverage is %f > %f or ncovered = %d > %d\n",
		 maxnconsecutive,pct_coverage,proceed_pctcoverage,ncovered,SUFF_NCOVERED));

    debug(printf("Performing diag on genomiclength %u\n",chrend-chrstart));
    Diag_compute_bounds(&diag_querystart,&diag_queryend,minactive,maxactive,diagonals,querylength,
			chrstart,chrend,chroffset,chrhigh,plusp);
    
    debug(
	  nunique = Diag_compute_bounds(&diag_querystart,&diag_queryend,minactive,maxactive,diagonals,querylength,
					chrstart,chrend,chroffset,chrhigh,plusp);
	  fprintf(stderr,"%d diagonals (%d not dominated), maxnconsecutive = %d\n",
		  List_length(diagonals),nunique,maxnconsecutive);
	  );

    all_paths = align_compute_lookback(mappings,npositions,totalpositions,
				       oned_matrix_p,minactive,maxactive,firstactive,nactive,cellpool,
				       queryseq_ptr,queryuc_ptr,querylength,
				       /*querystart*/diag_querystart,/*queryend*/diag_queryend,
				       genome,genomealt,chroffset,chrhigh,plusp,indexsize,pairpool,
				       localp,skip_repetitive_p,use_canonical_middle_p,NON_CANONICAL_PENALTY_MIDDLE,
				       favor_right_p,/*middlep*/true,max_nalignments);
    for (p = all_paths; p != NULL; p = List_next(p)) {
      path = (List_T) p->first;
#ifdef MOVE_TO_STAGE3
      firstpair = path->first;
#endif
      pairs = List_reverse(path);
#ifdef MOVE_TO_STAGE3
      lastpair = pairs->first;
#endif

      debug5(printf("Converting middle\n"));
      middle = convert_to_nucleotides(pairs,
#ifndef PMAP
				      queryseq_ptr,queryuc_ptr,
#endif
				      genome,genomealt,chroffset,chrhigh,/*watsonp*/plusp,
				      query_offset,pairpool,indexsize_nt,
				      /*include_gapholders_p*/true);

      all_ends = (List_T) NULL;

#ifdef MOVE_TO_STAGE3
#ifdef PMAP
      anchor_querypos = lastpair->querypos/3;
      /* anchor_position = lastpair->genomepos - 2; */
#else
      anchor_querypos = lastpair->querypos;
      /* anchor_position = lastpair->genomepos; */
#endif
      querystart = anchor_querypos + 1;
      queryend = querylength - 1;
      debug0(printf("For end, anchor querypos %d\n",anchor_querypos));

      end_paths = align_compute_lookback(mappings,npositions,totalpositions,
					 oned_matrix_p,minactive,maxactive,firstactive,nactive,cellpool,
					 queryseq_ptr,queryuc_ptr,querylength,querystart,queryend,
					 genome,genomealt,chroffset,chrhigh,plusp,indexsize,pairpool,
					 /*anchoredp*/true,anchor_querypos,anchor_position,
					 localp,skip_repetitive_p,use_canonical_ends_p,NON_CANONICAL_PENALTY_ENDS,
					 favor_right_p,/*middlep*/false,max_nalignments);

      /* fprintf(stderr,"%d ends\n",List_length(end_paths)); */
      if (List_length(end_paths) == 1) {
	pairs = (List_T) List_head(end_paths);
	path = List_reverse(pairs);
	debug5(printf("Converting single end\n"));
	pairs = convert_to_nucleotides(path,
#ifndef PMAP
				       queryseq_ptr,queryuc_ptr,
#endif
				       genome,genomealt,chroffset,chrhigh,/*watsonp*/plusp,
				       query_offset,pairpool,indexsize_nt,
				       /*include_gapholders_p*/false);
	middle = Pairpool_remove_gapholders(middle);
	middle = List_reverse(Pairpool_join_end3(List_reverse(middle),pairs,pairpool,/*copy_end_p*/false));
	debug0(printf("ATTACHING SINGLE END TO MIDDLE\n"));
	debug0(Pair_dump_list(middle,true));

      } else {
	debug0(i = 0);
	for (q = end_paths; q != NULL; q = List_next(q)) {
	  pairs = (List_T) List_head(q);
	  path = List_reverse(pairs);
	  debug5(printf("Converting one end\n"));
	  pairs = convert_to_nucleotides(path,
#ifndef PMAP
					 queryseq_ptr,queryuc_ptr,
#endif
					 genome,genomealt,chroffset,chrhigh,/*watsonp*/plusp,
					 query_offset,pairpool,indexsize_nt,
					 /*include_gapholders_p*/false);
	  debug0(printf("END %d/%d\n",i++,List_length(end_paths)));
	  debug0(Pair_dump_list(pairs,true));
	  all_ends = List_push(all_ends,(void *) pairs);
	}
      }
      List_free(&end_paths);
#endif

    
      all_starts = (List_T) NULL;

#ifdef MOVE_TO_STAGE3
#ifdef PMAP
      anchor_querypos = firstpair->querypos/3;
      anchor_position = firstpair->genomepos;
#else
      anchor_querypos = firstpair->querypos;
      anchor_position = firstpair->genomepos;
#endif
      debug0(printf("For start, anchor querypos %d\n",anchor_querypos));

      querystart = 0;
      queryend = anchor_querypos - 1;
      start_paths = align_compute_lookforward(mappings,npositions,totalpositions,
					      oned_matrix_p,minactive,maxactive,firstactive,nactive,cellpool,
					      queryseq_ptr,queryuc_ptr,querylength,querystart,queryend,
					      genome,genomealt,chroffset,chrhigh,plusp,indexsize,pairpool,
					      /*anchoredp*/true,anchor_querypos,anchor_position,
					      localp,skip_repetitive_p,use_canonical_ends_p,NON_CANONICAL_PENALTY_ENDS,
					      favor_right_p,/*middlep*/false,max_nalignments);

      /* fprintf(stderr,"%d starts\n",List_length(start_paths)); */
      if (List_length(start_paths) == 1) {
	path = (List_T) List_head(start_paths);
	debug5(printf("Converting single start\n"));
	pairs = convert_to_nucleotides(path,
#ifndef PMAP
				       queryseq_ptr,queryuc_ptr,
#endif
				       genome,genomealt,chroffset,chrhigh,/*watsonp*/plusp,
				       query_offset,pairpool,indexsize_nt,
				       /*include_gapholders_p*/false);
	path = List_reverse(pairs);
	middle = Pairpool_remove_gapholders(middle);
	middle = Pairpool_join_end5(middle,path,pairpool,/*copy_end_p*/false);
	debug0(printf("ATTACHING SINGLE START TO MIDDLE\n"));
	debug0(Pair_dump_list(middle,true));
	
      } else {
	debug0(i = 0);
	for (q = start_paths; q != NULL; q = List_next(q)) {
	  path = (List_T) List_head(q);
	  debug5(printf("Converting one start\n"));
	  pairs = convert_to_nucleotides(path,
#ifndef PMAP
					 queryseq_ptr,queryuc_ptr,
#endif
					 genome,genomealt,chroffset,chrhigh,/*watsonp*/plusp,
					 query_offset,pairpool,indexsize_nt,
					 /*include_gapholders_p*/false);
	  path = List_reverse(pairs);
	  debug0(printf("START %d/%d\n",i++,List_length(start_paths)));
	  debug0(Pair_dump_list(path,true));
	  all_starts = List_push(all_starts,(void *) path);
	}
      }
      List_free(&start_paths);
#endif

      all_stage2results = List_push(all_stage2results,(void *) Stage2_new(middle,all_starts,all_ends));
    }

    List_free(&all_paths);
  }

#ifdef GSNAP
  FREEA(nactive);
  FREEA(firstactive);
  FREEA(maxactive);
  FREEA(minactive);
  FREEA(npositions);
  FREEA(coveredp);
  FREEA(mappings);		/* Don't need to free contents of mappings */
#else
  if (querylength > stage2_alloc->max_querylength_alloc) {
    FREE(nactive);
    FREE(firstactive);
    FREE(maxactive);
    FREE(minactive);
    FREE(npositions);
    FREE(coveredp);
    FREE(mappings);		/* Don't need to free contents of mappings */
  }
#endif

#if 1
  for (source = 0; source < Oligoindex_array_length(oligoindices); source++) {
    oligoindex = Oligoindex_array_elt(oligoindices,source);
    Oligoindex_untally(oligoindex);
  }
#endif

  Stopwatch_stop(stopwatch);

  if (diag_debug == true) {
    return diagonals;
  } else {

#ifdef USE_DIAGPOOL
  /* No need to free diagonals */
#else
    for (p = diagonals; p != NULL; p = List_next(p)) {
      diag = (Diag_T) List_head(p);
      Diag_free(&diag);
    }
    List_free(&diagonals);
#endif
  }

  all_stage2results = Stage2_filter_unique(all_stage2results);
  debug0(printf("Done with stage2.  Returning %d results\n",List_length(all_stage2results)));
  return all_stage2results;
}


/* Since this stage2 is called from stage3 with a small segment of the
   query, we can use alloca instead of stage2_alloc */
List_T
Stage2_compute_one (char *queryseq_ptr, char *queryuc_ptr, int querylength, int query_offset,	
		    Chrpos_T chrstart, Chrpos_T chrend,
		    Univcoord_T chroffset, Univcoord_T chrhigh, bool plusp, int genestrand,
		    Oligoindex_array_T oligoindices,
		    Genome_T genome, Genome_T genomealt, Pairpool_T pairpool,
		    Diagpool_T diagpool, Cellpool_T cellpool,
		    bool localp, bool skip_repetitive_p, bool favor_right_p) {
  List_T pairs, all_paths;
  List_T middle, path;
  int indexsize, indexsize_nt;
  Oligoindex_T oligoindex;
  Chrpos_T **mappings;
  bool *coveredp, oned_matrix_p;
  int source;
  int *npositions, totalpositions;
  Chrpos_T *minactive, *maxactive;
  int *firstactive, *nactive;
  int ncovered;
  double pct_coverage;
  int maxnconsecutive;
  /* double diag_runtime; */
  List_T diagonals;


  debug(printf("Entered Stage2_compute_one with chrstart %u and chrend %u\n",chrstart,chrend));

  coveredp = (bool *) CALLOCA(querylength,sizeof(bool));
  mappings = (Chrpos_T **) MALLOCA(querylength * sizeof(Chrpos_T *));
  npositions = (int *) CALLOCA(querylength,sizeof(int));
  minactive = (unsigned int *) MALLOCA(querylength * sizeof(unsigned int));
  maxactive = (unsigned int *) MALLOCA(querylength * sizeof(unsigned int));
  firstactive = (int *) MALLOCA(querylength * sizeof(int));
  nactive = (int *) MALLOCA(querylength * sizeof(int));

  totalpositions = 0;
  maxnconsecutive = 0;

  source = 0;
  pct_coverage = 0.0;
#ifdef USE_DIAGPOOL
  Diagpool_reset(diagpool);
#endif
  Cellpool_reset(cellpool);
  diagonals = (List_T) NULL;
  while (source < Oligoindex_array_length(oligoindices) && pct_coverage < SUFF_PCTCOVERAGE_OLIGOINDEX) {
    oligoindex = Oligoindex_array_elt(oligoindices,source);
    indexsize = Oligoindex_indexsize(oligoindex); /* Different sources can have different indexsizes */

#ifdef PMAP
    if (plusp == true) {
      Oligoindex_pmap_tally(oligoindex,/*mappingstart*/chroffset+chrstart,
			    /*mappingend*/chroffset+chrend,/*plusp*/true,
			    queryuc_ptr,/*querystart*/0,/*queryend*/querylength,
			    /*chrpos*/chrstart);
    } else {
      Oligoindex_pmap_tally(oligoindex,/*mappingstart*/chroffset+chrstart,
			    /*mappingend*/chroffset+chrend+1,/*plusp*/false,
			    queryuc_ptr,/*querystart*/0,/*queryend*/querylength,
			    /*chrpos*/(chrhigh-chroffset)-chrend);
    }

#else
    if (plusp == true) {
      Oligoindex_hr_tally(oligoindex,/*mappingstart*/chroffset+chrstart,
			  /*mappingend*/chroffset+chrend,/*plusp*/true,
			  queryuc_ptr,/*querystart*/0,/*queryend*/querylength,
			  /*chrpos*/chrstart,genome,genestrand);
    } else {
      Oligoindex_hr_tally(oligoindex,/*mappingstart*/chroffset+chrstart,
			  /*mappingend*/chroffset+chrend+1,/*plusp*/false,
			  queryuc_ptr,/*querystart*/0,/*queryend*/querylength,
			  /*chrpos*/(chrhigh-chroffset)-chrend,genome,genestrand);
    }

#endif

    diagonals = Oligoindex_get_mappings(diagonals,coveredp,mappings,npositions,&totalpositions,
					&oned_matrix_p,&maxnconsecutive,oligoindices,oligoindex,queryuc_ptr,
					/*querystart*/0,/*queryend*/querylength,querylength,
					chrstart,chrend,chroffset,chrhigh,plusp,diagpool);
    pct_coverage = Diag_update_coverage(coveredp,&ncovered,diagonals,querylength);
    debug(printf("Stage2_compute: source = %d, ncovered = %d, pct_coverage = %f\n",source,ncovered,pct_coverage));

    source++;
  }
  /* *stage2_source = source; */
  /* *stage2_indexsize = indexsize; */
#ifdef PMAP
  indexsize_nt = 3*indexsize;
#else
  indexsize_nt = indexsize;
#endif


  if (totalpositions == 0) {
    debug(printf("Quitting because totalpositions is zero\n"));
    middle = (List_T) NULL;

  } else {
    debug(printf("Proceeding because pct_coverage is %f > %f or ncovered = %d > %d\n",
		 maxnconsecutive,pct_coverage,ncovered,SUFF_NCOVERED));

    debug(printf("Performing diag on genomiclength %u\n",chrend-chrstart));
    Diag_max_bounds(minactive,maxactive,querylength,chrstart,chrend,chroffset,chrhigh,plusp);
    
    if ((all_paths = align_compute_lookback(mappings,npositions,totalpositions,
					    oned_matrix_p,minactive,maxactive,firstactive,nactive,cellpool,
					    queryseq_ptr,queryuc_ptr,querylength,
					    /*querystart*/0,/*queryend*/querylength-1,
					    genome,genomealt,chroffset,chrhigh,plusp,indexsize,pairpool,
					    localp,skip_repetitive_p,use_canonical_middle_p,NON_CANONICAL_PENALTY_MIDDLE,
					    favor_right_p,/*middlep*/true,/*max_nalignments*/1)) == NULL) {
      middle = (List_T) NULL;
    } else if ((path = (List_T) List_head(all_paths)) == NULL) {
      middle = (List_T) NULL;
    } else {
      pairs = List_reverse(path);
      middle = convert_to_nucleotides(pairs,
#ifndef PMAP
				      queryseq_ptr,queryuc_ptr,
#endif
				      genome,genomealt,chroffset,chrhigh,/*watsonp*/plusp,
				      query_offset,pairpool,indexsize_nt,
				      /*include_gapholders_p*/true);
    }

    List_free(&all_paths);
  }

  FREEA(nactive);
  FREEA(firstactive);
  FREEA(maxactive);
  FREEA(minactive);
  FREEA(npositions);
  FREEA(coveredp);
  FREEA(mappings);		/* Don't need to free contents of mappings */

#if 1
  for (source = 0; source < Oligoindex_array_length(oligoindices); source++) {
    oligoindex = Oligoindex_array_elt(oligoindices,source);
    Oligoindex_untally(oligoindex);
  }
#endif

#ifdef USE_DIAGPOOL
  /* No need to free diagonals */
#else
  for (p = diagonals; p != NULL; p = List_next(p)) {
    diag = (Diag_T) List_head(p);
    Diag_free(&diag);
  }
  List_free(&diagonals);
#endif

  return List_reverse(middle);
}


/* Called by Stage3_compute_starts for ends of substring alignments */
List_T
Stage2_compute_starts (char *queryseq_ptr, char *queryuc_ptr, int querylength, int query_offset,	
		       Chrpos_T chrstart, Chrpos_T chrend,
		       Univcoord_T chroffset, Univcoord_T chrhigh, bool plusp, int genestrand,
		       Oligoindex_array_T oligoindices,
		       Genome_T genome, Genome_T genomealt, Pairpool_T pairpool,
		       Diagpool_T diagpool, Cellpool_T cellpool, bool localp,
		       bool skip_repetitive_p, bool favor_right_p, int max_nalignments) {
  List_T all_results;
  List_T pairs, all_paths, p;
  List_T path;
  int indexsize, indexsize_nt;
  Oligoindex_T oligoindex;
  Chrpos_T **mappings;
  bool *coveredp, oned_matrix_p;
  int source;
  int *npositions, totalpositions;
  Chrpos_T *minactive, *maxactive;
  int *firstactive, *nactive;
  int ncovered;
  double pct_coverage;
  int maxnconsecutive;
  /* double diag_runtime; */
  List_T diagonals;


  debug(printf("Entered Stage2_compute_starts with chrstart %u and chrend %u\n",chrstart,chrend));

  coveredp = (bool *) CALLOCA(querylength,sizeof(bool));
  mappings = (Chrpos_T **) MALLOCA(querylength * sizeof(Chrpos_T *));
  npositions = (int *) CALLOCA(querylength,sizeof(int));
  minactive = (unsigned int *) MALLOCA(querylength * sizeof(unsigned int));
  maxactive = (unsigned int *) MALLOCA(querylength * sizeof(unsigned int));
  firstactive = (int *) MALLOCA(querylength * sizeof(int));
  nactive = (int *) MALLOCA(querylength * sizeof(int));

  totalpositions = 0;
  maxnconsecutive = 0;

  source = 0;
  pct_coverage = 0.0;
#ifdef USE_DIAGPOOL
  Diagpool_reset(diagpool);
#endif
  Cellpool_reset(cellpool);
  diagonals = (List_T) NULL;
  while (source < Oligoindex_array_length(oligoindices) && pct_coverage < SUFF_PCTCOVERAGE_OLIGOINDEX) {
    oligoindex = Oligoindex_array_elt(oligoindices,source);
    indexsize = Oligoindex_indexsize(oligoindex); /* Different sources can have different indexsizes */

#ifdef PMAP
    if (plusp == true) {
      Oligoindex_pmap_tally(oligoindex,/*mappingstart*/chroffset+chrstart,
			    /*mappingend*/chroffset+chrend,/*plusp*/true,
			    queryuc_ptr,/*querystart*/0,/*queryend*/querylength,
			    /*chrpos*/chrstart);
    } else {
      Oligoindex_pmap_tally(oligoindex,/*mappingstart*/chroffset+chrstart,
			    /*mappingend*/chroffset+chrend+1,/*plusp*/false,
			    queryuc_ptr,/*querystart*/0,/*queryend*/querylength,
			    /*chrpos*/(chrhigh-chroffset)-chrend);
    }

#else
    if (plusp == true) {
      Oligoindex_hr_tally(oligoindex,/*mappingstart*/chroffset+chrstart,
			  /*mappingend*/chroffset+chrend,/*plusp*/true,
			  queryuc_ptr,/*querystart*/0,/*queryend*/querylength,
			  /*chrpos*/chrstart,genome,genestrand);
    } else {
      Oligoindex_hr_tally(oligoindex,/*mappingstart*/chroffset+chrstart,
			  /*mappingend*/chroffset+chrend+1,/*plusp*/false,
			  queryuc_ptr,/*querystart*/0,/*queryend*/querylength,
			  /*chrpos*/(chrhigh-chroffset)-chrend,genome,genestrand);
    }

#endif

    diagonals = Oligoindex_get_mappings(diagonals,coveredp,mappings,npositions,&totalpositions,
					&oned_matrix_p,&maxnconsecutive,oligoindices,oligoindex,queryuc_ptr,
					/*querystart*/0,/*queryend*/querylength,querylength,
					chrstart,chrend,chroffset,chrhigh,plusp,diagpool);
    pct_coverage = Diag_update_coverage(coveredp,&ncovered,diagonals,querylength);
    debug(printf("Stage2_compute: source = %d, ncovered = %d, pct_coverage = %f\n",source,ncovered,pct_coverage));

    source++;
  }
  /* *stage2_source = source; */
  /* *stage2_indexsize = indexsize; */
#ifdef PMAP
  indexsize_nt = 3*indexsize;
#else
  indexsize_nt = indexsize;
#endif


  if (totalpositions == 0) {
    debug(printf("Quitting because totalpositions is zero\n"));
    all_results = (List_T) NULL;

  } else {
    debug(printf("Proceeding because maxnconsecutive is %d and pct_coverage is %f or ncovered = %d > %d\n",
		 maxnconsecutive,pct_coverage,ncovered,SUFF_NCOVERED));

    debug(printf("Performing diag on genomiclength %u\n",chrend-chrstart));
    Diag_max_bounds(minactive,maxactive,querylength,chrstart,chrend,chroffset,chrhigh,plusp);
    
    if ((all_paths = align_compute_lookforward(mappings,npositions,totalpositions,
					       oned_matrix_p,minactive,maxactive,firstactive,nactive,cellpool,
					       queryseq_ptr,queryuc_ptr,querylength,
					       /*querystart*/0,/*queryend*/querylength-1,
					       genome,genomealt,chroffset,chrhigh,plusp,indexsize,pairpool,
					       localp,skip_repetitive_p,use_canonical_middle_p,NON_CANONICAL_PENALTY_MIDDLE,
					       favor_right_p,/*middlep*/false,max_nalignments)) == NULL) {
      all_results = (List_T) NULL;

    } else {
      all_results = (List_T) NULL;
      for (p = all_paths; p != NULL; p = List_next(p)) {
	path = List_head(p);
	pairs = convert_to_nucleotides(path,
#ifndef PMAP
				       queryseq_ptr,queryuc_ptr,
#endif
				       genome,genomealt,chroffset,chrhigh,/*watsonp*/plusp,
				       query_offset,pairpool,indexsize_nt,
				       /*include_gapholders_p*/false);
	path = List_reverse(pairs);
	debug0(printf("START\n"));
	debug0(Pair_dump_list(path,true));
	if (path != NULL) {
	  all_results = List_push(all_results,(void *) path);
	}
      }
    }

    List_free(&all_paths);
  }

  FREEA(nactive);
  FREEA(firstactive);
  FREEA(maxactive);
  FREEA(minactive);
  FREEA(npositions);
  FREEA(coveredp);
  FREEA(mappings);		/* Don't need to free contents of mappings */

#if 1
  for (source = 0; source < Oligoindex_array_length(oligoindices); source++) {
    oligoindex = Oligoindex_array_elt(oligoindices,source);
    Oligoindex_untally(oligoindex);
  }
#endif

#ifdef USE_DIAGPOOL
  /* No need to free diagonals */
#else
  for (p = diagonals; p != NULL; p = List_next(p)) {
    diag = (Diag_T) List_head(p);
    Diag_free(&diag);
  }
  List_free(&diagonals);
#endif

  debug0(printf("Before filtering starts, %d\n",List_length(all_results)));
  all_results = Stage2pairs_filter_unique_starts(all_results);
  debug0(printf("After filtering starts, %d\n",List_length(all_results)));

  return all_results;
}


/* Called by Stage3_compute_ends for ends of substring alignments */
List_T
Stage2_compute_ends (char *queryseq_ptr, char *queryuc_ptr, int querylength, int query_offset,	
		     Chrpos_T chrstart, Chrpos_T chrend,
		     Univcoord_T chroffset, Univcoord_T chrhigh, bool plusp, int genestrand,
		     Oligoindex_array_T oligoindices,
		     Genome_T genome, Genome_T genomealt, Pairpool_T pairpool,
		     Diagpool_T diagpool, Cellpool_T cellpool, bool localp,
		     bool skip_repetitive_p, bool favor_right_p, int max_nalignments) {
  List_T all_results;
  List_T pairs, all_paths, p;
  List_T path;
  int indexsize, indexsize_nt;
  Oligoindex_T oligoindex;
  Chrpos_T **mappings;
  bool *coveredp, oned_matrix_p;
  int source;
  int *npositions, totalpositions;
  Chrpos_T *minactive, *maxactive;
  int *firstactive, *nactive;
  int ncovered;
  double pct_coverage;
  int maxnconsecutive;
  /* double diag_runtime; */
  List_T diagonals;


  debug(printf("Entered Stage2_compute_ends with chrstart %u and chrend %u\n",chrstart,chrend));

  coveredp = (bool *) CALLOCA(querylength,sizeof(bool));
  mappings = (Chrpos_T **) MALLOCA(querylength * sizeof(Chrpos_T *));
  npositions = (int *) CALLOCA(querylength,sizeof(int));
  minactive = (unsigned int *) MALLOCA(querylength * sizeof(unsigned int));
  maxactive = (unsigned int *) MALLOCA(querylength * sizeof(unsigned int));
  firstactive = (int *) MALLOCA(querylength * sizeof(int));
  nactive = (int *) MALLOCA(querylength * sizeof(int));

  totalpositions = 0;
  maxnconsecutive = 0;

  source = 0;
  pct_coverage = 0.0;
#ifdef USE_DIAGPOOL
  Diagpool_reset(diagpool);
#endif
  Cellpool_reset(cellpool);
  diagonals = (List_T) NULL;
  while (source < Oligoindex_array_length(oligoindices) && pct_coverage < SUFF_PCTCOVERAGE_OLIGOINDEX) {
    oligoindex = Oligoindex_array_elt(oligoindices,source);
    indexsize = Oligoindex_indexsize(oligoindex); /* Different sources can have different indexsizes */

#ifdef PMAP
    if (plusp == true) {
      Oligoindex_pmap_tally(oligoindex,/*mappingstart*/chroffset+chrstart,
			    /*mappingend*/chroffset+chrend,/*plusp*/true,
			    queryuc_ptr,/*querystart*/0,/*queryend*/querylength,
			    /*chrpos*/chrstart);
    } else {
      Oligoindex_pmap_tally(oligoindex,/*mappingstart*/chroffset+chrstart,
			    /*mappingend*/chroffset+chrend+1,/*plusp*/false,
			    queryuc_ptr,/*querystart*/0,/*queryend*/querylength,
			    /*chrpos*/(chrhigh-chroffset)-chrend);
    }

#else

    if (plusp == true) {
      Oligoindex_hr_tally(oligoindex,/*mappingstart*/chroffset+chrstart,
			  /*mappingend*/chroffset+chrend,/*plusp*/true,
			  queryuc_ptr,/*querystart*/0,/*queryend*/querylength,
			  /*chrpos*/chrstart,genome,genestrand);
    } else {
      Oligoindex_hr_tally(oligoindex,/*mappingstart*/chroffset+chrstart,
			  /*mappingend*/chroffset+chrend+1,/*plusp*/false,
			  queryuc_ptr,/*querystart*/0,/*queryend*/querylength,
			  /*chrpos*/(chrhigh-chroffset)-chrend,genome,genestrand);
    }

#endif

    diagonals = Oligoindex_get_mappings(diagonals,coveredp,mappings,npositions,&totalpositions,
					&oned_matrix_p,&maxnconsecutive,oligoindices,oligoindex,queryuc_ptr,
					/*querystart*/0,/*queryend*/querylength,querylength,
					chrstart,chrend,chroffset,chrhigh,plusp,diagpool);
    pct_coverage = Diag_update_coverage(coveredp,&ncovered,diagonals,querylength);
    debug(printf("Stage2_compute: source = %d, ncovered = %d, pct_coverage = %f\n",source,ncovered,pct_coverage));

    source++;
  }
  /* *stage2_source = source; */
  /* *stage2_indexsize = indexsize; */
#ifdef PMAP
  indexsize_nt = 3*indexsize;
#else
  indexsize_nt = indexsize;
#endif


  if (totalpositions <= 0) {
    debug(printf("Quitting because totalpositions is zero\n"));
    all_results = (List_T) NULL;

  } else {
    debug(printf("Proceeding because maxnconsecutive is %d and pct_coverage is %f or ncovered = %d > %d\n",
		 maxnconsecutive,pct_coverage,ncovered,SUFF_NCOVERED));

    debug(printf("Performing diag on genomiclength %u\n",chrend-chrstart));
    Diag_max_bounds(minactive,maxactive,querylength,chrstart,chrend,chroffset,chrhigh,plusp);
    
    if ((all_paths = align_compute_lookback(mappings,npositions,totalpositions,
					    oned_matrix_p,minactive,maxactive,firstactive,nactive,cellpool,
					    queryseq_ptr,queryuc_ptr,querylength,
					    /*querystart*/0,/*queryend*/querylength-1,
					    genome,genomealt,chroffset,chrhigh,plusp,indexsize,pairpool,
					    localp,skip_repetitive_p,use_canonical_middle_p,NON_CANONICAL_PENALTY_MIDDLE,
					    favor_right_p,/*middlep*/false,max_nalignments)) == NULL) {
      all_results = (List_T) NULL;

    } else {
      all_results = (List_T) NULL;
      for (p = all_paths; p != NULL; p = List_next(p)) {
	pairs = List_head(p);
	path = List_reverse(pairs);
	pairs = convert_to_nucleotides(path,
#ifndef PMAP
				       queryseq_ptr,queryuc_ptr,
#endif
				       genome,genomealt,chroffset,chrhigh,/*watsonp*/plusp,
				       query_offset,pairpool,indexsize_nt,
				       /*include_gapholders_p*/false);
	debug0(printf("END\n"));
	debug0(Pair_dump_list(pairs,true));
	if (pairs != NULL) {
	  all_results = List_push(all_results,(void *) pairs);
	}
      }
    }

    List_free(&all_paths);
  }

  FREEA(nactive);
  FREEA(firstactive);
  FREEA(maxactive);
  FREEA(minactive);
  FREEA(npositions);
  FREEA(coveredp);
  FREEA(mappings);		/* Don't need to free contents of mappings */

#if 1
  for (source = 0; source < Oligoindex_array_length(oligoindices); source++) {
    oligoindex = Oligoindex_array_elt(oligoindices,source);
    Oligoindex_untally(oligoindex);
  }
#endif

#ifdef USE_DIAGPOOL
  /* No need to free diagonals */
#else
  for (p = diagonals; p != NULL; p = List_next(p)) {
    diag = (Diag_T) List_head(p);
    Diag_free(&diag);
  }
  List_free(&diagonals);
#endif

  debug0(printf("Before filtering ends, %d\n",List_length(all_results)));
  all_results = Stage2pairs_filter_unique_ends(all_results);
  debug0(printf("After filtering ends, %d\n",List_length(all_results)));

  return all_results;
}

