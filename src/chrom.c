static char rcsid[] = "$Id: chrom.c 223349 2020-10-28 02:49:25Z twu $";
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "chrom.h"
#include <stdio.h>
#include <stdlib.h>		/* For atoi */
#include <string.h>
#include "assert.h"
#include "mem.h"
#include "interval.h"


#ifdef DEBUG
#define debug(x) x
#else
#define debug(x)
#endif


/* This is the order for chrom sort */
typedef enum {PURE_NUMERIC, SEX_CHROMOSOME, MITOCHONDRIAL, NUMERIC_ALPHA, ALPHA_NUMERIC, PURE_ALPHA} Chromtype_T;

#ifdef DEBUG
static char *
Chromtype_string (Chromtype_T chromtype) {
  switch (chromtype) {
  case PURE_NUMERIC: return "numeric";
  case SEX_CHROMOSOME: return "sex";
  case MITOCHONDRIAL: return "mitochondrial";
  case NUMERIC_ALPHA: return "numeric_alpha";
  case ALPHA_NUMERIC: return "alpha_numeric";
  case PURE_ALPHA: return "alpha";
  default: abort();
  }
}
#endif


#define T Chrom_T
struct T {
  Univcoord_T order;            /* order used for sorting (can be universal coordinate) */
  bool numericp;
  char *string;			/* The original string */
  unsigned int num;		/* The initial numeric part; valid only if numericp == true */
  char *alpha;			/* The alphabetic part, possibly after the number; valid only if numericp == true */
  Chromtype_T chromtype;
  bool circularp;
  Chrpos_T alt_scaffold_start;
  Chrpos_T alt_scaffold_end;
};

void
Chrom_free (T *old) {
  FREE((*old)->alpha);
  FREE((*old)->string);
  FREE(*old);
  return;
}


char *
Chrom_string (T this) {
  return this->string;
}

bool
Chrom_circularp (T this) {
  return this->circularp;
}

bool
Chrom_altlocp (Chrpos_T *alt_scaffold_start, Chrpos_T *alt_scaffold_end, T this) {
  *alt_scaffold_start = this->alt_scaffold_start;
  *alt_scaffold_end = this->alt_scaffold_end;

  if (*alt_scaffold_end != *alt_scaffold_start) {
    return true;
  } else {
    return false;
  }
}



/* Largest number for an unsigned int is 4294967295, which is 10
   digits.  However, the organism with the most chromosomes is the
   Indian fern, with 1260.  Hence, more than 4 digits would suggest
   a non-chromosomal string.  Also, if first digit is '0', treat as
   a string. */

T
Chrom_from_string (char *string, char *mitochondrial_string, Univcoord_T order,
		   bool circularp, Chrpos_T alt_scaffold_start, Chrpos_T alt_scaffold_end) {
  T new = (T) MALLOC(sizeof(*new));
  int ndigits = 0;
  char *p;
  bool mitochondrial_p = false, sex_p = false;

  debug(printf("string is %s\n",string));

  new->order = order;
  new->circularp = circularp;
  new->alt_scaffold_start = alt_scaffold_start;
  new->alt_scaffold_end = alt_scaffold_end;

  new->string = (char *) MALLOC((strlen(string)+1) * sizeof(char));
  strcpy(new->string,string);

  if (mitochondrial_string != NULL && !strcmp(string,mitochondrial_string)) {
    mitochondrial_p = true;
  }

  if (!strncmp(string,"chr",3) || !strncmp(string,"Chr",3)) {
    /* Ignore leading chr or Chr for sorting purposes */
    string += 3;
    debug(printf("  => chop chr to yield %s\n",string));
  }

  if (!strcmp(string,"X")) {
    sex_p = true;
  } else if (!strcmp(string,"Y")) {
    sex_p = true;
  } else if (!strcmp(string,"M")) {
    mitochondrial_p = true;
  } else if (!strcmp(string,"MT")) {
    mitochondrial_p = true;
  } else if (mitochondrial_string != NULL && !strcmp(string,mitochondrial_string)) {
    mitochondrial_p = true;
  }

  p = string;
  while (*p != '\0' && *p >= '0' && *p <= '9') {
    ndigits++;
    p++;
  }
  debug(printf("  => saw %d digits\n",ndigits));


  if (ndigits > 0 && ndigits <= 4 && string[0] != '0') {
    new->numericp = true;
    new->num = atoi(string);
    new->alpha = (char *) CALLOC(strlen(p)+1,sizeof(char));
    strcpy(new->alpha,p);

    if (mitochondrial_p == true) {
      new->chromtype = MITOCHONDRIAL;
    } else if (strlen(new->alpha) == 0) {
      new->chromtype = PURE_NUMERIC;
    } else {
      new->chromtype = NUMERIC_ALPHA;
    }

    debug(printf("  => numeric with value %d and then alpha %s, type %s\n",
		 new->num,new->alpha,Chromtype_string(new->chromtype)));

  } else {
    new->numericp = false;
    new->num = 0;
    new->alpha = (char *) NULL;

    if (mitochondrial_p == true) {
      new->chromtype = MITOCHONDRIAL;
    } else if (sex_p == true) {
      new->chromtype = SEX_CHROMOSOME;
    } else {
      while (*p != '\0' && (*p < '1' || *p > '9')) {
	/* Stop at initial '1' through '9'.  An initial '0' must be alphabetic, not numeric. */
	p++;
      }
      if (*p != '\0') {
	new->chromtype = ALPHA_NUMERIC;
	new->num = atoi(p);
	new->alpha = (char *) MALLOC((p - string + 1)*sizeof(char));
	strncpy(new->alpha,string,(p - string)*sizeof(char));
	new->alpha[p - string] = '\0';
	debug(printf("  => alpha_numeric with alpha %s and then num %d, type %s\n",
		     new->alpha,new->num,Chromtype_string(new->chromtype)));
      } else {
	new->chromtype = PURE_ALPHA;
	debug(printf("  => alphabetical %s, type %s\n",
		     new->string,Chromtype_string(new->chromtype)));
      }
    }
  }

  return new;
}


int
Chrom_cmp_alpha (T a, T b) {

  return strcmp(a->string,b->string);
}


int
Chrom_cmp_numeric_alpha (T a, T b) {
  int cmp;

  if (a->numericp == true && b->numericp == false) {
    /* 1 and X */
    return -1;
  } else if (a->numericp == false && b->numericp == true) {
    /* X and 1 */
    return +1;
  } else if (a->numericp == true && b->numericp == true) {
    if (a->num < b->num) {
      /* 1 and 2U */
      return -1;
    } else if (a->num > b->num) {
      /* 2U and 1 */
      return +1;
    } else {
      return strcmp(a->alpha,b->alpha);
    }
  } else {
    cmp = strcmp(a->string,b->string);
    if (cmp < 0) {
      return -1;
    } else if (cmp > 0) {
      return +1;
    } else if (a->chromtype == PURE_ALPHA && b->chromtype == ALPHA_NUMERIC) {
      return -1;
    } else if (a->chromtype == ALPHA_NUMERIC && b->chromtype == PURE_ALPHA) {
      return +1;
    } else if (a->chromtype == PURE_ALPHA && b->chromtype == PURE_ALPHA) {
      return 0;
    } else if (a->num < b->num) {
      /* Chr2 and Chr10 */
      return -1;
    } else if (a->num > b->num) {
      /* Chr10 and Chr2 */
      return +1;
    } else {
      return 0;
    }
  }
}


int
Chrom_cmp_chrom (T a, T b) {
  int cmp;

  debug(printf("Comparing %s and %s => ",a->string,b->string));

  if (a->chromtype < b->chromtype) {
    debug(printf("chromtype %d < %d => -1\n",a->chromtype,b->chromtype));
    return -1;

  } else if (b->chromtype < a->chromtype) {
    debug(printf("chromtype %d > %d => +1\n",a->chromtype,b->chromtype));
    return +1;

  } else if (a->numericp == true && b->numericp == true) {
    if (a->num < b->num) {
      /* 1 and 2U */
      debug(printf("numeric %d < %d => -1\n",a->num,b->num));
      return -1;
    } else if (a->num > b->num) {
      /* 2U and 1 */
      debug(printf("numeric %d > %d => +1\n",a->num,b->num));
      return +1;
    } else {
      debug(printf("numeric_alpha %s cmp %s => %d\n",
		   a->alpha,b->alpha,strcmp(a->alpha,b->alpha)));
      return strcmp(a->alpha,b->alpha);
    }

  } else if (a->chromtype == ALPHA_NUMERIC) {
    cmp = strcmp(a->alpha,b->alpha);
    if (cmp < 0) {
      debug(printf("alpha_numeric %s cmp %s => %d\n",
		   a->alpha,b->alpha,strcmp(a->alpha,b->alpha)));
      return -1;
    } else if (cmp > 0) {
      debug(printf("alpha_numeric %s cmp %s => %d\n",
		   a->alpha,b->alpha,strcmp(a->alpha,b->alpha)));
      return +1;
    } else if (a->num < b->num) {
      /* Chr2 and Chr10 */
      debug(printf("alpha_numeric %d < %d => -1\n",a->num,b->num));
      return -1;
    } else if (a->num > b->num) {
      /* Chr10 and Chr2 */
      debug(printf("alpha_numeric %d > %d => +1\n",a->num,b->num));
      return +1;
    } else {
      debug(printf("alpha_numeric %d == %d => %d\n",a->num,b->num,strcmp(a->string,b->string)));
      return strcmp(a->string,b->string);
    }

  } else {
    /* assert(a->chromtype == PURE_ALPHA); or MITOCHONDRIAL or SEX_CHROMOSOME */
    debug(printf("pure alpha or mitochondrial or sex chromosome %s cmp %s => %d\n",
		 a->string,b->string,strcmp(a->string,b->string)));
    return strcmp(a->string,b->string);
  }
}


int
Chrom_cmp_order (T a, T b) {

  if (a->order < b->order) {
    return -1;
  } else if (b->order < a->order) {
    return +1;
  } else {
    return 0;
  }
}


/* For use by qsorting an array */
int
Chrom_compare_order (const void *x, const void *y) {
  T a = * (T *) x;
  T b = * (T *) y;

  if (a->order < b->order) {
    return -1;
  } else if (b->order < a->order) {
    return +1;
  } else {
    return 0;
  }
}

/* For use by qsorting an array */
int
Chrom_compare_alpha (const void *x, const void *y) {
  T a = * (T *) x;
  T b = * (T *) y;

  return strcmp(a->string,b->string);
}

/* For use by qsorting an array */
int
Chrom_compare_numeric_alpha (const void *x, const void *y) {
  T a = * (T *) x;
  T b = * (T *) y;

  return Chrom_cmp_numeric_alpha(a,b);
}

/* For use by qsorting an array */
int
Chrom_compare_chrom (const void *x, const void *y) {
  T a = * (T *) x;
  T b = * (T *) y;

  return Chrom_cmp_chrom(a,b);
}


/* For use in table comparisons */
int
Chrom_compare_table (const void *x, const void *y) {
  T a = (T) x;
  T b = (T) y;

  return Chrom_cmp_chrom(a,b);
}

/* This is the X31 hash function */
static unsigned int
string_hash (char *a) {
  unsigned int h = 0U;
  char *p;
  
  for (p = a; *p != '\0'; p++) {
    h = (h << 5) - h + *p;
  }
  return h;
}

unsigned int
Chrom_hash_table (const void *key) {
  T this = (T) key;

  return string_hash(this->string);
}
