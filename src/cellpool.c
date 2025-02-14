static char rcsid[] = "$Id: cellpool.c 223349 2020-10-28 02:49:25Z twu $";
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cellpool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>		/* For memcpy */
#include "assert.h"
#include "mem.h"


#define CHUNKSIZE 10000


#ifdef DEBUG
#define debug(x) x
#else
#define debug(x)
#endif

/* For mechanics of memory allocation and deallocation */
#ifdef DEBUG1
#define debug1(x) x
#else
#define debug1(x)
#endif

/* For popping */
#ifdef DEBUG2
#define debug2(x) x
#else
#define debug2(x)
#endif


#define T Cellpool_T
struct T {
  int nobjects;
  int objectctr;
  struct Cell_T *objectptr;
  List_T objectchunks;

  int nlistcells;
  int listcellctr;
  struct List_T *listcellptr;
  List_T listcellchunks;
};

void
Cellpool_free_memory (T this) {
  List_T p;
  struct Cell_T *objectptr;
  struct List_T *listcellptr;

  for (p = this->objectchunks; p != NULL; p = List_next(p)) {
    objectptr = (struct Cell_T *) List_head(p);
    FREE_KEEP(objectptr);
  }
  List_free_keep(&this->objectchunks);
  for (p = this->listcellchunks; p != NULL; p = List_next(p)) {
    listcellptr = (struct List_T *) List_head(p);
    FREE_KEEP(listcellptr);
  }
  List_free_keep(&this->listcellchunks);

  this->nobjects = 0;
  this->objectctr = 0;
  this->objectchunks = NULL;
  /* this->objectptr = add_new_objectchunk(this); */

  this->nlistcells = 0;
  this->listcellctr = 0;
  this->listcellchunks = NULL;
  /* this->listcellptr = add_new_listcellchunk(this); */

  return;
}

void
Cellpool_free (T *old) {
  Cellpool_free_memory(*old);
  FREE_KEEP(*old);
  return;
}


void
Cellpool_report_memory (T this) {
  printf("Cellpool has %d pairchunks and %d listcellchunks\n",
	 List_length(this->objectchunks),List_length(this->listcellchunks));
  return;
}


static struct Cell_T *
add_new_objectchunk (T this) {
  struct Cell_T *chunk;

  chunk = (struct Cell_T *) MALLOC_KEEP(CHUNKSIZE*sizeof(struct Cell_T));
  this->objectchunks = List_push_keep(this->objectchunks,(void *) chunk);
  debug1(printf("Adding a new chunk of objects.  Ptr for object %d is %p\n",
		this->nobjects,chunk));

  this->nobjects += CHUNKSIZE;

  return chunk;
}

static struct List_T *
add_new_listcellchunk (T this) {
  struct List_T *chunk;

  chunk = (struct List_T *) MALLOC_KEEP(CHUNKSIZE*sizeof(struct List_T));
  this->listcellchunks = List_push_keep(this->listcellchunks,(void *) chunk);
  debug1(printf("Adding a new chunk of listcells.  Ptr for listcell %d is %p\n",
	       this->nlistcells,chunk));

  this->nlistcells += CHUNKSIZE;

  return chunk;
}

T
Cellpool_new (void) {
  T new = (T) MALLOC_KEEP(sizeof(*new));

  new->nobjects = 0;
  new->objectctr = 0;
  new->objectchunks = NULL;
  /* new->objectptr = add_new_objectchunk(new); */

  new->nlistcells = 0;
  new->listcellctr = 0;
  new->listcellchunks = NULL;
  /* new->listcellptr = add_new_listcellchunk(new); */

  return new;
}

void
Cellpool_reset (T this) {
  this->objectctr = 0;
  this->listcellctr = 0;
  return;
}

List_T
Cellpool_push (List_T list, T this, int rootposition, int endposition,
	       int querypos, int hit, bool fwdp, int score) {
  List_T listcell;
  Cell_T new;
  List_T p;
  int n;

  if (this->objectctr >= this->nobjects) {
    this->objectptr = add_new_objectchunk(this);
  } else if ((this->objectctr % CHUNKSIZE) == 0) {
    for (n = this->nobjects - CHUNKSIZE, p = this->objectchunks;
	 n > this->objectctr; p = p->rest, n -= CHUNKSIZE) ;
    this->objectptr = (struct Cell_T *) p->first;
    debug1(printf("Located object %d at %p\n",this->objectctr,this->objectptr));
  }    
  new = this->objectptr++;
  this->objectctr++;


  new->rootposition = rootposition;
  new->endposition = endposition;
  new->querypos = querypos;
  new->hit = hit;
  new->fwdp = fwdp;
  new->score = score;
  new->pushedp = false;


  if (this->listcellctr >= this->nlistcells) {
    this->listcellptr = add_new_listcellchunk(this);
  } else if ((this->listcellctr % CHUNKSIZE) == 0) {
    for (n = this->nlistcells - CHUNKSIZE, p = this->listcellchunks;
	 n > this->listcellctr; p = p->rest, n -= CHUNKSIZE) ;
    this->listcellptr = (struct List_T *) p->first;
    debug1(printf("Located listcell %d at %p\n",this->listcellctr,this->listcellptr));
  }
  listcell = this->listcellptr++;
  this->listcellctr++;

  listcell->first = (void *) new;
  listcell->rest = list;

  return listcell;
}



/* Note: this does not free the list cell */
List_T
Cellpool_pop (List_T list, Cell_T *x) {
  List_T head;

  if (list != NULL) {
    head = list->rest;
    *x = (Cell_T) list->first;
    return head;
  } else {
    return list;
  }
}


