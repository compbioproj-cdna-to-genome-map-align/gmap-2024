/* $Id: stage2.h 223511 2020-11-14 15:50:08Z twu $ */
#ifndef STAGE2_INCLUDED
#define STAGE2_INCLUDED

typedef struct Stage2_alloc_T *Stage2_alloc_T;
typedef struct Stage2_T *Stage2_T;

#include "bool.h"
#include "list.h"
#include "genomicpos.h"
#include "pairpool.h"
#include "diagpool.h"
#include "cellpool.h"
#include "stopwatch.h"
#include "mode.h"
#ifdef PMAP
#include "oligoindex_pmap.h"
#else
#include "oligoindex_hr.h"
/* #include "oligoindex_localdb.h" */
#endif

extern void
Stage2_alloc_free (Stage2_alloc_T *old);
extern Stage2_alloc_T
Stage2_alloc_new (int max_querylength_alloc);



#define T Stage2_T

extern List_T
Stage2_middle (T this);
extern List_T
Stage2_all_starts (T this);
extern List_T
Stage2_all_ends (T this);
extern void
Stage2_free (T *old);

extern void
Stage2_setup (bool splicingp_in, bool cross_species_p,
	      int suboptimal_score_start_in, int suboptimal_score_end_in,	
	      int sufflookback_in, int nsufflookback_in, int maxintronlen_in,
	      Mode_T mode_in, bool snps_p_in);
	    
extern void
Stage2_free (T *old);

extern int
Stage2_scan (int *stage2_source, char *queryuc_ptr, int querylength, Genome_T genome,
	     Chrpos_T chrstart, Chrpos_T chrend, 
	     Univcoord_T chroffset, Univcoord_T chrhigh, bool plusp,
	     int genestrand, Stage2_alloc_T stage2_alloc, Oligoindex_array_T oligoindices,
	     Diagpool_T diagpool);

extern List_T
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
		Stopwatch_T stopwatch, bool diag_debug);

extern List_T
Stage2_compute_one (char *queryseq_ptr, char *queryuc_ptr, int querylength, int query_offset,	
		    Chrpos_T chrstart, Chrpos_T chrend,
		    Univcoord_T chroffset, Univcoord_T chrhigh, bool plusp, int genestrand,
		    Oligoindex_array_T oligoindices,
		    Genome_T genome, Genome_T genomealt, Pairpool_T pairpool, Diagpool_T diagpool, Cellpool_T cellpool,
		    bool localp, bool skip_repetitive_p, bool favor_right_p);

extern List_T
Stage2_compute_starts (char *queryseq_ptr, char *queryuc_ptr, int querylength, int query_offset,	
		       Chrpos_T chrstart, Chrpos_T chrend,
		       Univcoord_T chroffset, Univcoord_T chrhigh, bool plusp, int genestrand,
		       Oligoindex_array_T oligoindices,
		       Genome_T genome, Genome_T genomealt, Pairpool_T pairpool,
		       Diagpool_T diagpool, Cellpool_T cellpool, bool localp, bool skip_repetitive_p,
		       bool favor_right_p, int max_nalignments);

extern List_T
Stage2_compute_ends (char *queryseq_ptr, char *queryuc_ptr, int querylength, int query_offset,	
		     Chrpos_T chrstart, Chrpos_T chrend,
		     Univcoord_T chroffset, Univcoord_T chrhigh, bool plusp, int genestrand,
		     Oligoindex_array_T oligoindices,
		     Genome_T genome, Genome_T genomealt,Pairpool_T pairpool,
		     Diagpool_T diagpool, Cellpool_T cellpool,
		     bool localp, bool skip_repetitive_p,
		     bool favor_right_p, int max_nalignments);

#undef T
#endif

