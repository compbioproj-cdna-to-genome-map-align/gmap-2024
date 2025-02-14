/* $Id: result.h 218147 2019-01-16 21:28:41Z twu $ */
#ifndef RESULT_INCLUDED
#define RESULT_INCLUDED

typedef struct Result_T *Result_T;

#include "chimera.h"
#include "diagnostic.h"
#include "stage3.h"

typedef enum {NO_FAILURE, EMPTY_SEQUENCE, SHORT_SEQUENCE, POOR_SEQUENCE, REPETITIVE} Failure_T;

#define T Result_T

extern int
Result_id (T this);
extern bool
Result_mergedp (T this);
extern Chimera_T
Result_chimera (T this);
extern Stage3_T *
Result_array (int *npaths_primary, int *npaths_altloc, int *first_absmq, int *second_absmq, T this);
extern List_T
Result_gregionlist (T this);
extern List_T
Result_diagonals (T this);
extern Diagnostic_T
Result_diagnostic (T this);
extern Failure_T
Result_failuretype (T this);

extern T
Result_new (int id, bool mergedp, Chimera_T chimera, Stage3_T *array,
	    int npaths_primary, int npaths_altloc, int first_absmq, int second_absmq,
	    Diagnostic_T diagnostic, Failure_T failuretype);
extern void
Result_free (T *old);

#undef T
#endif

