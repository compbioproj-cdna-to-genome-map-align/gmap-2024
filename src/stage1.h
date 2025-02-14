/* $Id: stage1.h 218185 2019-01-17 13:05:01Z twu $ */
#ifndef STAGE1_INCLUDED
#define STAGE1_INCLUDED

#include "bool.h"
#include "genomicpos.h"
#include "indexdb.h"
#include "sequence.h"
#include "list.h"
#include "match.h"
#include "matchpool.h"
#include "iit-read-univ.h"
#include "genome.h"
#include "stopwatch.h"
#include "diagnostic.h"


#define T Stage1_T
typedef struct T *T;

#ifdef PMAP
extern void
Stage1_setup (int index1part_aa_in, Chrpos_T maxextension_in, Chrpos_T maxtotallen_bound_in,
	      int circular_typeint_in);
#else
extern void
Stage1_setup (int index1part_in, Chrpos_T maxextension_in, Chrpos_T maxtotallen_bound_in,
	      int circular_typeint_in);
#endif


extern List_T
Stage1_compute (bool *lowidentityp, Sequence_T queryuc,
		Indexdb_T indexdb_fwd, Indexdb_T indexdb_rev, int genestrand,
		Univ_IIT_T chromosome_iit, Univcoord_T chrsubset_start, Univcoord_T chrsubset_end,
		Matchpool_T matchpool, int stutterhits, Diagnostic_T diagnostic, Stopwatch_T stopwatch,
		int nbest);

extern List_T
Stage1_compute_nonstranded (bool *lowidentityp, Sequence_T queryuc,
			    Indexdb_T indexdb_fwd, Indexdb_T indexdb_rev,
			    Univ_IIT_T chromosome_iit, Univcoord_T chrsubset_start, Univcoord_T chrsubset_end,
			    Matchpool_T matchpool, int stutterhits, Diagnostic_T diagnostic, Stopwatch_T stopwatch,
			    int nbest);

#undef T
#endif

