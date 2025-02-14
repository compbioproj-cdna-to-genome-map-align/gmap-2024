static char rcsid[] = "$Id: d513dc483af03ef293908c77a094697173421f57 $";
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifndef HAVE_MEMCPY
# define memcpy(d,s,n) bcopy((s),(d),(n))
#endif
#ifndef HAVE_MEMMOVE
# define memmove(d,s,n) bcopy((s),(d),(n))
#endif

#include "shortread.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>		/* For rindex */
#include <ctype.h>		/* For iscntrl and isspace */

#ifdef HAVE_ZLIB
#include <zlib.h>
#define GZBUFFER_SIZE 131072
#endif

#define PAIRED_ADAPTER_NMISMATCHES_ALLOWED 1
#define PAIRED_ADAPTER_MINLENGTH 20 /* Must exceed 14 or stage1 will complain */

#define OVERLAP_NMISMATCHES_ALLOWED 1
#define OVERLAP_MINLENGTH 10

#include "assert.h"
#include "mem.h"
#include "access.h"
#include "complement.h"
#include "intlist.h"
#include "fopen.h"
#include "getline.h"


#ifdef DEBUG
#define debug(x) x
#else
#define debug(x)
#endif

/* Pointers for first half and second half */
#ifdef DEBUG1
#define debug1(x) x
#else
#define debug1(x)
#endif

/* Adapter stripping */
#ifdef DEBUG2
#define debug2(x) x
#else
#define debug2(x)
#endif

/* Shortread_polya */
#ifdef DEBUG3
#define debug3(x) x
#else
#define debug3(x)
#endif

/* File open/close.  Want to turn on in gsnap.c also. */
#ifdef DEBUGF
#define debugf(x) x
#else
#define debugf(x)
#endif



/***********************************************************************
 *    Definitions:
 *
 *   XXXXXXXXXX  TTTTTT ACGT .................. ACGT AAAAAA
 *   barcode-->  <-------- fulllength --------> <-- endtrim
 *               ^contents
 *               ^quality
 *
 ************************************************************************/


#define T Shortread_T
struct T {
  char *acc;			/* Accession */
  char *restofheader;		/* Rest of header */
  bool filterp;			/* If true, then sequence should be skipped */
  bool invertedp;

  char *contents;		/* Original sequence, ends with '\0' */
  char *contents_alloc;		/* Allocation */
  int fulllength;		/* Full length (not including chopped sequence) */

  /* GSNAP-specific fields */
  char *contents_uc;		/* Original sequence, ends with '\0' */
  char *contents_uc_alloc;	/* Allocation */
  char *queryrc;
  char *queryrc_alloc;

  char *barcode;
  int barcode_length;
  char *endtrim;
  int endtrim_length;

  int overlap;

  char *left_chop;
  char *left_chop_quality;
  int left_choplength;

  char *right_chop;
  char *right_chop_quality;
  int right_choplength;

  char *quality;		/* For Illumina short reads read via FASTQ */
  char *quality_alloc;		/* Allocation */

  /* bool free_contents_p; */
};

char *
Shortread_accession (T this) {
  return this->acc;
}

char *
Shortread_header (T this) {
  return this->restofheader;
}

bool
Shortread_filterp (T this) {
  return this->filterp;
}

bool
Shortread_invertedp (T this) {
  return this->invertedp;
}

char *
Shortread_fullpointer (T this) {
  return this->contents;
}

char *
Shortread_fullpointer_uc (T this) {
  return this->contents_uc;
}

char *
Shortread_contents_uc (T this) {
  return this->contents_uc;
}

char *
Shortread_queryuc_ptr (T this) {
  return this->contents_uc;
}

char *
Shortread_queryrc (T this) {
  return this->queryrc;
}


int
Shortread_barcode_length (T this) {
  return this->barcode_length;
}

char *
Shortread_barcode (T this) {
  return this->barcode;
}

int
Shortread_endtrim_length (T this) {
  return this->endtrim_length;
}

char *
Shortread_endtrim (T this) {
  return this->endtrim;
}

int
Shortread_left_choplength (T this) {
  return this->left_choplength;
}

int
Shortread_right_choplength (T this) {
  return this->right_choplength;
}

char *
Shortread_quality_string (T this) {
  return this->quality;
}

int
Shortread_fulllength (T this) {
  return this->fulllength;
}


void
Shortread_free (T *old) {

  if (*old) {
    if ((*old)->restofheader != NULL) {
      FREE_IN((*old)->restofheader);
    }
    if ((*old)->acc != NULL) {
      FREE_IN((*old)->acc);
    }

    FREE_IN((*old)->queryrc_alloc);
    FREE_IN((*old)->contents_alloc);

    FREE_IN((*old)->barcode);
    FREE_IN((*old)->endtrim);
    FREE_IN((*old)->right_chop_quality);
    FREE_IN((*old)->right_chop);
    FREE_IN((*old)->left_chop_quality);
    FREE_IN((*old)->left_chop);
    FREE_IN((*old)->quality_alloc);
    FREE_IN((*old)->contents_uc_alloc);

    FREE_IN(*old);
  }

  return;
}


#define HEADERLEN 512
#define DISCARDLEN 8192

static char Header[HEADERLEN];
static char Discard[DISCARDLEN];


/* input_oneline() can actually read longer than this */
#define MAX_EXPECTED_READLENGTH 300

static char Read1[MAX_EXPECTED_READLENGTH+1];
static char Read2[MAX_EXPECTED_READLENGTH+1];
static char Quality[MAX_EXPECTED_READLENGTH+1];


/* The first element of Sequence is always the null character, to mark
   the end of the string */

/* Skipping of dashes might still be buggy */
/*
#define DASH '-'
*/


int
Shortread_input_init (int *nchars, FILE *fp) {
  int c;
  bool okayp = false;

  debugf(fprintf(stderr,"Calling Shortread_input_init on %p\n",fp));
  Header[0] = '\0';

  while (okayp == false && (c = fgetc(fp)) != EOF) {
    *nchars += 1;
    debug(printf("nchars %d: Read character %c\n",*nchars,c));
    if (iscntrl(c)) {
#ifdef DASH
    } else if (c == DASH) {
#endif
    } else if (isspace(c)) {
    } else {
      okayp = true;
    }
  }
  if (okayp == false) {
    *nchars += 1;
  }

  debug(printf("nchars %d: Returning initial character %c\n",*nchars,c));
  return c;
}



#ifdef HAVE_ZLIB
/* Returns '>' if FASTA file, first sequence char if not */
int
Shortread_input_init_gzip (gzFile fp) {
  int c;
  bool okayp = false;

  debugf(fprintf(stderr,"Calling Shortread_input_init_gzip on %p\n",fp));
  Header[0] = '\0';

  while (okayp == false && (c = gzgetc(fp)) != EOF) {
    debug(printf("Read character %c\n",c));
    if (iscntrl(c)) {
#ifdef DASH
    } else if (c == DASH) {
#endif
    } else if (isspace(c)) {
    } else {
      okayp = true;
    }
  }

  debug(printf("Returning initial character %c\n",c));
  return c;
}
#endif


#ifdef HAVE_BZLIB
/* Returns '>' if FASTA file, first sequence char if not */
int
Shortread_input_init_bzip2 (Bzip2_T fp) {
  int c;
  bool okayp = false;

  debugf(fprintf(stderr,"Calling Shortread_input_init_bzip2 on %p\n",fp));
  Header[0] = '\0';

  while (okayp == false && (c = bzgetc(fp)) != EOF) {
    debug(printf("Read character %c\n",c));
    if (iscntrl(c)) {
#ifdef DASH
    } else if (c == DASH) {
#endif
    } else if (isspace(c)) {
    } else {
      okayp = true;
    }
  }

  debug(printf("Returning initial character %c\n",c));
  return c;
}
#endif

static int acc_fieldi_start = 0;
static int acc_fieldi_end = 0;
static bool force_single_end_p = false;
static bool filter_chastity_p = false;
static bool keep_chastity_p = false;
static bool allow_paired_end_mismatch_p = false;
static bool fastq_format_p;
static int barcode_length;
static int endtrim_length;
static bool invert_first_p;
static bool invert_second_p;

static bool chop_poly_at_first_p;
static bool chop_poly_at_second_p;


void
Shortread_setup (int acc_fieldi_start_in, int acc_fieldi_end_in,
		 bool force_single_end_p_in, bool filter_chastity_p_in, bool keep_chastity_p_in,
		 bool allow_paired_end_mismatch_p_in, bool fastq_format_p_in,
		 int barcode_length_in, int endtrim_length_in,
		 bool invert_first_p_in, bool invert_second_p_in,
		 bool chop_poly_at_first_p_in, bool chop_poly_at_second_p_in) {
  acc_fieldi_start = acc_fieldi_start_in;
  acc_fieldi_end = acc_fieldi_end_in;
  force_single_end_p = force_single_end_p_in;
  filter_chastity_p = filter_chastity_p_in;
  keep_chastity_p = keep_chastity_p_in;
  allow_paired_end_mismatch_p = allow_paired_end_mismatch_p_in;
  fastq_format_p = fastq_format_p_in;
  barcode_length = barcode_length_in;
  endtrim_length = endtrim_length_in;
  invert_first_p = invert_first_p_in;
  invert_second_p = invert_second_p_in;
  chop_poly_at_first_p = chop_poly_at_first_p_in;
  chop_poly_at_second_p = chop_poly_at_second_p_in;

  return;
}


static char *skipped_acc = "Skipped";

static char *
input_header (int *nchars, bool *filterp, char **restofheader, int nextchar,
	      FILE *fp, bool skipp) {
  char *acc = NULL, *p, *q;
  size_t length;

  *filterp = false;

  if (nextchar == EOF) {	/* Was feof(fp) */
    return (char *) NULL;
  } else if ((p = fgets(&(Header[0]),HEADERLEN,fp)) == NULL) {
    /* File must terminate after > */
    return (char *) NULL;
  } else {
    *nchars += strlen(p);
  }

  if (Header[0] == '\n') {
    Header[0] = '\0';
  } else if ((p = rindex(&(Header[0]),'\n')) != NULL) {
    if (p[-1] == '\r') {
      p--;
    }
    *p = '\0';
  } else {
    /* Eliminate rest of header from input */
    while ((p = fgets(&(Discard[0]),DISCARDLEN,fp)) != NULL &&
	   rindex(&(Discard[0]),'\n') == NULL) {
      *nchars += strlen(p);
    }
  }

  if (skipp == true) {
    return (char *) skipped_acc;
  } else {
    p = &(Header[0]);
    while (*p != '\0' && !isspace((int) *p)) {
      p++;
    }

    if (filter_chastity_p == true || keep_chastity_p == true) {
      q = p;
      /* Expecting <read>:<is filtered>:<control number>:<index sequence>, e.g., 1:Y:0:CTTGTA */
      while (*q != '\0' && *q != ':') {
	q++;
      }
      if (*q != '\0') {
	q++;
	if (filter_chastity_p == true && *q == 'Y') {
	  *filterp = true;
	}
      }
    }

    if (*p == '\0') {
      /* Accession only */
      length = (p - &(Header[0]))/sizeof(char);
      acc = (char *) CALLOC_IN(length+1,sizeof(char));
      strcpy(acc,Header);
      (*restofheader) = (char *) CALLOC_IN(1,sizeof(char));
      (*restofheader)[0] = '\0';
    } else {
      *p = '\0';
      length = (p - &(Header[0]))/sizeof(char);
      acc = (char *) CALLOC_IN(length+1,sizeof(char));
      strcpy(acc,Header);
      p++;
      *restofheader = (char *) CALLOC_IN(strlen(p)+1,sizeof(char));
      strcpy(*restofheader,p);
    }
  
    return acc;
  }
} 


#ifdef HAVE_ZLIB
static char *
input_header_gzip (bool *filterp, char **restofheader, int nextchar,
		   gzFile fp, bool skipp) {
  char *acc = NULL, *p, *q;
  size_t length;

  *filterp = false;

  if (nextchar == EOF) {	/* Was gzeof(fp) */
    return NULL;
  } else if ((p = gzgets(fp,&(Header[0]),HEADERLEN)) == NULL) {
    /* File must terminate after > */
    return NULL;
  }

  if (Header[0] == '\n') {
    Header[0] = '\0';
  } else if ((p = rindex(&(Header[0]),'\n')) != NULL) {
    if (p[-1] == '\r') {
      p--;
    }
    *p = '\0';
  } else {
    /* Eliminate rest of header from input */
    while (gzgets(fp,&(Discard[0]),DISCARDLEN) != NULL &&
	   rindex(&(Discard[0]),'\n') == NULL) ;
  }

  if (skipp) {
    return (char *) skipped_acc;
  } else {
    p = &(Header[0]);
    while (*p != '\0' && !isspace((int) *p)) {
      p++;
    }

    if (filter_chastity_p == true || keep_chastity_p == true) {
      q = p;
      /* Expecting <read>:<is filtered>:<control number>:<index sequence>, e.g., 1:Y:0:CTTGTA */
      while (*q != '\0' && *q != ':') {
	q++;
      }
      if (*q != '\0') {
	q++;
	if (filter_chastity_p == true && *q == 'Y') {
	  *filterp = true;
	}
      }
    }

    if (*p == '\0') {
      /* Accession only */
      length = (p - &(Header[0]))/sizeof(char);
      acc = (char *) CALLOC_IN(length+1,sizeof(char));
      strcpy(acc,Header);
      *restofheader = (char *) CALLOC_IN(1,sizeof(char));
      (*restofheader)[0] = '\0';
    } else {
      *p = '\0';
      length = (p - &(Header[0]))/sizeof(char);
      acc = (char *) CALLOC_IN(length+1,sizeof(char));
      strcpy(acc,Header);
      p++;
      *restofheader = (char *) CALLOC_IN(strlen(p)+1,sizeof(char));
      strcpy(*restofheader,p);
    }

    return acc;
  }
} 
#endif


#ifdef HAVE_BZLIB
static char *
input_header_bzip2 (bool *filterp, char **restofheader, int nextchar,
		    Bzip2_T fp, bool skipp) {
  char *acc = NULL, *p, *q;
  size_t length;

  *filterp = false;

  if (nextchar == EOF) {	/* Was bzeof(fp) */
    return NULL;
  } else if ((p = bzgets(fp,&(Header[0]),HEADERLEN)) == NULL) {
    /* File must terminate after > */
    return NULL;
  }

  if (Header[0] == '\n') {
    Header[0] = '\0';
  } else if ((p = rindex(&(Header[0]),'\n')) != NULL) {
    if (p[-1] == '\r') {
      p--;
    }
    *p = '\0';
  } else {
    /* Eliminate rest of header from input */
    while (bzgets(fp,&(Discard[0]),DISCARDLEN) != NULL &&
	   rindex(&(Discard[0]),'\n') == NULL) ;
  }

  if (skipp) {
    return (char *) skipped_acc;
  } else {
    p = &(Header[0]);
    while (*p != '\0' && !isspace((int) *p)) {
      p++;
    }

    if (filter_chastity_p == true || keep_chastity_p == true) {
      q = p;
      /* Expecting <read>:<is filtered>:<control number>:<index sequence>, e.g., 1:Y:0:CTTGTA */
      while (*q != '\0' && *q != ':') {
	q++;
      }
      if (*q != '\0') {
	q++;
	if (filter_chastity_p == true && *q == 'Y') {
	  *filterp = true;
	}
      }
    }

    if (*p == '\0') {
      /* Accession only */
      length = (p - &(Header[0]))/sizeof(char);
      acc = (char *) CALLOC_IN(length+1,sizeof(char));
      strcpy(acc,Header);
      *restofheader = (char *) CALLOC_IN(1,sizeof(char));
      (*restofheader)[0] = '\0';
    } else {
      *p = '\0';
      length = (p - &(Header[0]))/sizeof(char);
      acc = (char *) CALLOC_IN(length+1,sizeof(char));
      strcpy(acc,Header);
      p++;
      *restofheader = (char *) CALLOC_IN(strlen(p)+1,sizeof(char));
      strcpy(*restofheader,p);
    }

    return acc;
  }
} 
#endif


#if FILE_CONSISTENT
static bool stripp = true;
#endif

/* Deletes /\D1/ or /\D2 or 3/ endings. */
static void
strip_illumina_acc_ending (char *acc1, char *acc2) {
  char *p, *q;
  char slash1, slash2;

#if FILE_CONSISTENT
  if (stripp == true) {
#endif

    p = acc1;
    while (*p != '\0') {
      p++;
    }

    q = acc2;
    while (*q != '\0') {
      q++;
    }

    if (p - acc1 <= 2 || q - acc2 <= 2) {
      /* Don't check p[-2] or q[-2] */
#if FILE_CONSISTENT
	/* fprintf(stderr,"At least one read name is too short to have /1 or /2 endings in header fields %s and %s.  Will no longer look for them.\n",acc1,acc2); */
	stripp = false;
#endif

    } else {
      if (p[-2] == ';' && q[-2] == ';') {
	/* Handle Casava data that ends in ";1" */
	p -= 2;
	q -= 2;
      } else if (p[-2] == ':' &&	q[-2] == ':' && p[-1] == q[-1]) {
	/* Handle old style Illumina data that ends in ":0" or ":1" */
	p -= 2;
	q -= 2;
      }

      /* Delete "/1" or "/2 or /3" endings */
      slash1 = p[-2];
      slash2 = q[-2];

      if (slash1 != slash2 || isdigit(slash1)) {
#if FILE_CONSISTENT
	/* fprintf(stderr,"Do not see /1 or /2 endings in header fields %s and %s.  Will no longer look for them.\n",acc1,acc2); */
	stripp = false;
#endif

      } else if (p[-1] != '1' || (q[-1] != '2' && q[-1] != '3')) {
#if FILE_CONSISTENT
	/* fprintf(stderr,"Do not see /1 or /2 endings in header fields %s and %s.  Will no longer look for them.\n",acc1,acc2); */
	stripp = false;
#endif

      } else {
	/* Saw slashes /1, /2, or /3 */
	p[-2] = '\0';
	q[-2] = '\0';
      }
    }

#if FILE_CONSISTENT
  }
#endif

  return;
}


static char *
input_header_fastq (int *nchars, bool *filterp, char **restofheader, int nextchar,
		    FILE *fp, bool skipp) {
  char *acc, *p, *q, *start;
  size_t length;
  int fieldi = 0;

  *filterp = false;

  if (nextchar == EOF) { /* Was feof(fp) */
    return (char *) NULL;
  } else if ((p = fgets(&(Header[0]),HEADERLEN,fp)) == NULL) {
    /* File must terminate after > */
    return (char *) NULL;
  } else {
    *nchars += strlen(p);
  }

  if (Header[0] == '\n') {
    Header[0] = '\0';
  } else if ((p = rindex(&(Header[0]),'\n')) != NULL) {
    if (p[-1] == '\r') {
      p--;
    }
    *p = '\0';
  } else {
    /* Eliminate rest of header from input */
    while ((p = fgets(&(Discard[0]),DISCARDLEN,fp)) != NULL &&
	   rindex(&(Discard[0]),'\n') == NULL) {
      *nchars += strlen(p);
    }
  }

  if (skipp == true) {
    return (char *) skipped_acc;
  } else {
    p = start = &(Header[0]);
    while (fieldi < acc_fieldi_start) {
      while (*p != '\0' && !isspace((int) *p)) {
	p++;
      }
      if (*p != '\0') {
	p++;
      }
      start = p;
      fieldi++;
    }

    while (fieldi < acc_fieldi_end) {
      while (*p != '\0' && !isspace((int) *p)) {
	p++;
      }
      if (*p != '\0') {
	p++;
      }
      fieldi++;
    }

    while (*p != '\0' && !isspace((int) *p)) {
      p++;
    }

    /* Get accession */
    length = (p - start)/sizeof(char);
    acc = (char *) CALLOC_IN(length+1,sizeof(char));
    strncpy(acc,start,length);
    acc[length] = '\0';

    /* Get rest of header */
    q = p;
    while (*q != '\0' && isspace((int) *q)) {
      q++;
    }

    if (filter_chastity_p == true || keep_chastity_p == true) {
      /* Expecting <read>:<is filtered>:<control number>:<index sequence>, e.g., 1:Y:0:CTTGTA */
      while (*q != '\0' && *q != ':') {
	q++;
      }
      if (*q != '\0') {
	q++;
	if (filter_chastity_p == true && *q == 'Y') {
	  *filterp = true;
	}
      }
    }

    if (keep_chastity_p == true) {
      q += 4;			/* Skip Y:0: */
    }

    length = strlen(q);
    *restofheader = (char *) MALLOC_IN((length+1)*sizeof(char));
    strcpy(*restofheader,q);

    return acc;
  }
} 


#ifdef HAVE_ZLIB
static char *
input_header_fastq_gzip (bool *filterp, char **restofheader, int nextchar,
			 gzFile fp, bool skipp) {
  char *acc, *p, *q, *start;
  size_t length;
  int fieldi = 0;

  *filterp = false;

  if (nextchar == EOF) {	/* Was gzeof(fp) */
    return NULL;
  } else if ((p = gzgets(fp,&(Header[0]),HEADERLEN)) == NULL) {
    /* File must terminate after > */
    return NULL;
  }

  if (Header[0] == '\n') {
    Header[0] = '\0';
  } else if ((p = rindex(&(Header[0]),'\n')) != NULL) {
    if (p[-1] == '\r') {
      p--;
    }
    *p = '\0';
  } else {
    /* Eliminate rest of header from input */
    while (gzgets(fp,&(Discard[0]),DISCARDLEN) != NULL &&
	   rindex(&(Discard[0]),'\n') == NULL) ;
  }

  if (skipp) {
    return (char *) skipped_acc;
  } else {
    p = start = &(Header[0]);

    while (fieldi < acc_fieldi_start) {
      while (*p != '\0' && !isspace((int) *p)) {
	p++;
      }
      if (*p != '\0') {
	p++;
      }
      start = p;
      fieldi++;
    }

    while (fieldi < acc_fieldi_end) {
      while (*p != '\0' && !isspace((int) *p)) {
	p++;
      }
      if (*p != '\0') {
	p++;
      }
      fieldi++;
    }

    while (*p != '\0' && !isspace((int) *p)) {
      p++;
    }

    /* Get accession */
    length = (p - start)/sizeof(char);
    acc = (char *) CALLOC_IN(length+1,sizeof(char));
    strncpy(acc,start,length);
    acc[length] = '\0';

    /* Get rest of header */
    q = p;
    while (*q != '\0' && isspace((int) *q)) {
      q++;
    }

    if (filter_chastity_p == true || keep_chastity_p == true) {
      /* Expecting <read>:<is filtered>:<control number>:<index sequence>, e.g., 1:Y:0:CTTGTA */
      while (*q != '\0' && *q != ':') {
	q++;
      }
      if (*q != '\0') {
	q++;
	if (filter_chastity_p == true && *q == 'Y') {
	  *filterp = true;
	}
      }
    }

    if (keep_chastity_p == true) {
      q += 4;			/* Skip Y:0: */
    }

    length = strlen(q);
    *restofheader = (char *) MALLOC_IN((length+1)*sizeof(char));
    strcpy(*restofheader,q);

    return acc;
  }
} 
#endif


#ifdef HAVE_BZLIB
static char *
input_header_fastq_bzip2 (bool *filterp, char **restofheader, int nextchar,
			  Bzip2_T fp, bool skipp) {
  char *acc, *p, *q, *start;
  size_t length;
  int fieldi = 0;

  *filterp = false;

  if (nextchar == EOF) {	/* Was bzeof(fp) */
    return NULL;
  } else if ((p = bzgets(fp,&(Header[0]),HEADERLEN)) == NULL) {
    /* File must terminate after > */
    return NULL;
  }

  if (Header[0] == '\n') {
    Header[0] = '\0';
  } else if ((p = rindex(&(Header[0]),'\n')) != NULL) {
    if (p[-1] == '\r') {
      p--;
    }
    *p = '\0';
  } else {
    /* Eliminate rest of header from input */
    while (bzgets(fp,&(Discard[0]),DISCARDLEN) != NULL &&
	   rindex(&(Discard[0]),'\n') == NULL) ;
  }

  if (skipp) {
    return (char *) skipped_acc;
  } else {
    p = start = &(Header[0]);
    while (fieldi < acc_fieldi_start) {
      while (*p != '\0' && !isspace((int) *p)) {
	p++;
      }
      if (*p != '\0') {
	p++;
      }
      start = p;
      fieldi++;
    }

    while (fieldi < acc_fieldi_end) {
      while (*p != '\0' && !isspace((int) *p)) {
	p++;
      }
      if (*p != '\0') {
	p++;
      }
      fieldi++;
    }

    while (*p != '\0' && !isspace((int) *p)) {
      p++;
    }

    if (filter_chastity_p == true || keep_chastity_p == true) {
      q = p;
      /* Expecting <read>:<is filtered>:<control number>:<index sequence>, e.g., 1:Y:0:CTTGTA */
      while (*q != '\0' && *q != ':') {
	q++;
      }
      if (*q != '\0') {
	q++;
	if (filter_chastity_p == true && *q == 'Y') {
	  *filterp = true;
	}
      }
    }

    if (keep_chastity_p == false) {
      *p = '\0';
      length = (p - start)/sizeof(char);
      acc = (char *) CALLOC_IN(length+1,sizeof(char));
      strcpy(acc,start);
      *restofheader = (char *) CALLOC_IN(1,sizeof(char));
      (*restofheader)[0] = '\0';

    } else {
      length = (p - start)/sizeof(char);
      acc = (char *) CALLOC_IN(length+1,sizeof(char));
      strncpy(acc,start,length);
      acc[length] = '\0';

      q += 4;			/* Skip Y:0: */
      length = strlen(q);
      *restofheader = (char *) MALLOC_IN((length+1)*sizeof(char));
      strcpy(*restofheader,q);
    }

    return acc;
  }
} 
#endif


static bool
skip_header (int *nchars, FILE *fp, int nextchar) {
  char *p;

  if (nextchar == EOF) {
    return false;
  } else if ((p = fgets(&(Header[0]),HEADERLEN,fp)) == NULL) {
    /* File must terminate after > */
    return false;
  } else {
    *nchars += strlen(p);
  }

  if (rindex(&(Header[0]),'\n') == NULL) {
    /* Eliminate rest of header from input */
    while ((p = fgets(&(Discard[0]),DISCARDLEN,fp)) != NULL &&
	   rindex(&(Discard[0]),'\n') == NULL) {
      *nchars += strlen(p);
    }
  }

  return true;
} 

#ifdef HAVE_ZLIB
static bool
skip_header_gzip (gzFile fp, int nextchar) {

  if (nextchar == EOF) {	/* was gzeof(fp) */
    return false;
  } else if (gzgets(fp,&(Header[0]),HEADERLEN) == NULL) {
    /* File must terminate after > */
    return false;
  }

  if (rindex(&(Header[0]),'\n') == NULL) {
    /* Eliminate rest of header from input */
    while (gzgets(fp,&(Discard[0]),DISCARDLEN) != NULL &&
	   rindex(&(Discard[0]),'\n') == NULL) ;
  }

  return true;
} 
#endif

#ifdef HAVE_BZLIB
static bool
skip_header_bzip2 (Bzip2_T fp, int nextchar) {

  if (nextchar == EOF) {	/* was bzeof(fp) */
    return false;
  } else if (bzgets(fp,&(Header[0]),HEADERLEN) == NULL) {
    /* File must terminate after > */
    return false;
  }

  if (rindex(&(Header[0]),'\n') == NULL) {
    /* Eliminate rest of header from input */
    while (bzgets(fp,&(Discard[0]),DISCARDLEN) != NULL &&
	   rindex(&(Discard[0]),'\n') == NULL) ;
  }

  return true;
} 
#endif



#define CONTROLM 13		/* From PC */
#define SPACE 32

#if 0
static char *
find_bad_char (char *line) {
  char *first, *p1, *p2;
#ifdef DASH
  char *p3;
#endif

  p1 = index(line,CONTROLM);
  p2 = index(line,SPACE);
  /* p3 = index(line,DASH); */

  if (p1 == NULL && p2 == NULL /* && p3 == NULL*/) {
    return NULL;
  } else {
    if (p1) {
      first = p1;
    }
    if (p2) {
      first = p2;
    }
    /*
    if (p3) {
      first = p3;
    }
    */
    if (p1 && p1 < first) {
      first = p1;
    }
    if (p2 && p2 < first) {
      first = p2;
    }
    /*
    if (p3 && p3 < first) {
      first = p3;
    }
    */
    return first;
  }
}
#endif

static char *
find_spaces (int *nspaces, char *line) {
  char *first, *p2;

  p2 = index(line,SPACE);

  if (p2 == NULL /* && p3 == NULL*/) {
    return NULL;
  } else {
    first = p2++;
    while (*p2 != '\0' && *p2 == SPACE) {
      p2++;
    }

    *nspaces = p2 - first;
    return first;
  }
}


static int
input_oneline (int *nextchar, int *nchars, char **longstring, char *Start,
	       FILE *fp, bool possible_fasta_header_p) {
  int remainder;
  char *ptr, *p = NULL;
  int strlenp, nspaces;

  int i;
  Intlist_T intlist;

  debug(printf("Entering input_oneline with nextchar = %c\n",*nextchar));
  *longstring = (char *) NULL;

  ptr = &(Start[0]);
  remainder = (&(Start[MAX_EXPECTED_READLENGTH]) - ptr)/sizeof(char);
  if (*nextchar == EOF || (possible_fasta_header_p == true && (*nextchar == '>' || *nextchar == '+'))) {
    debug(printf("nchars %d: EOF or > or +: Returning 0\n",*nchars));
    return 0;
  } else if (*nextchar == '\n') {
    debug(printf("nchars %d: Blank line: Returning 0\n",*nchars));
    return 0;
  } else {
    *ptr++ = (char) *nextchar;
    if ((p = fgets(ptr,remainder+1,fp)) == NULL) {
      /* NULL if file ends with a blank line */
      debug(printf("Blank line. read %s.\n",ptr));
    } else {
      *nchars += strlen(p);
      debug(printf("nchars %d: Read %s.\n",*nchars,ptr));
#if 0
      if (pc_linefeeds_p == true) {
#endif
	while ((p = find_spaces(&nspaces,ptr)) != NULL) {
	  ptr = p;
	  p += nspaces;
	  strlenp = strlen(p);
	  memmove(ptr,p,strlenp);
	  ptr[strlenp] = '\0';
	  debug(printf("Found %d spaces.  Did memmove of %d chars at %p to %p\n",nspaces,strlenp,p,ptr));
	}
#if 0
      }
#endif

      if (*ptr == '\n') {
	*ptr = '\0';
	debug(printf("Now string is %s.\n",ptr));
      } else if ((p = index(ptr,'\n')) != NULL) {
	if (p[-1] == '\r') {
	  p--;
	}
	*p = '\0';
	debug(printf("nchars %d: Now string is %s.\n",*nchars,ptr));
      } else if (*ptr == EOF) {
	/* No line feed, but end of file.  Handle below. */
	debug(printf("nchars %d: End of file seen\n",*nchars));
      } else {
	/* No line feed, but not end of file.  Read too long, so using another method. */
	debug(printf("No line feed, but not end of file.  Using Intlist_T.\n"));
	intlist = (Intlist_T) NULL;
	i = 0;
	while (i <= MAX_EXPECTED_READLENGTH && Start[i] != '\0') {
	  debug(printf("Pushing %c\n",Start[i]));
	  intlist = Intlist_push_in(intlist,Start[i]);
	  i++;
	}
	while ((*nextchar = fgetc(fp)) != EOF && *nextchar != '\n') {
	  *nchars += 1;
	  intlist = Intlist_push_in(intlist,*nextchar);
	}
	*nchars += 1;
	if (*nextchar == '\n') {
	  *nextchar = fgetc(fp);
	  *nchars += 1;
	}

	intlist = Intlist_reverse(intlist);
	*longstring = Intlist_to_char_array_in(&i,intlist);
	Intlist_free_in(&intlist);

	debug(printf("nchars %d: Intlist method returning %d\n",*nchars,i));
	return i;
      }
    }

    ptr += strlen(ptr);

    /* Peek at character after eoln */
    *nextchar = fgetc(fp);
    *nchars += 1;

    debug(printf("nchars %d: Returning %ld with nextchar %c\n",*nchars,(ptr - &(Start[0]))/sizeof(char),*nextchar));
    return (ptr - &(Start[0]))/sizeof(char);
  }
}


#ifdef HAVE_ZLIB
static int
input_oneline_gzip (int *nextchar, char **longstring, char *Start,
		    gzFile fp, bool possible_fasta_header_p) {
  int remainder;
  char *ptr, *p = NULL;
  int strlenp, nspaces;

  int i;
  Intlist_T intlist;

  debug(printf("Entering input_oneline with nextchar = %c\n",*nextchar));
  *longstring = (char *) NULL;

  ptr = &(Start[0]);
  remainder = (&(Start[MAX_EXPECTED_READLENGTH]) - ptr)/sizeof(char);
  if (*nextchar == EOF || (possible_fasta_header_p == true && (*nextchar == '>' || *nextchar == '+'))) {
    debug(printf("EOF or > or +: Returning 0\n"));
    return 0;
  } else if (*nextchar == '\n') {
    debug(printf("Blank line: Returning 0\n"));
    return 0;
  } else {
    *ptr++ = (char) *nextchar;
    if ((p = gzgets(fp,ptr,remainder+1)) == NULL) {
      /* NULL if file ends with a blank line */
      debug(printf("Blank line. read %s.\n",ptr));
    } else {
      debug(printf("Read %s.\n",ptr));
#if 0
      if (pc_linefeeds_p == true) {
#endif
	while ((p = find_spaces(&nspaces,ptr)) != NULL) {
	  ptr = p;
	  p += nspaces;
	  strlenp = strlen(p);
	  memmove(ptr,p,strlenp);
	  ptr[strlenp] = '\0';
	  debug(printf("Found %d spaces.  Did memmove of %d chars at %p to %p\n",nspaces,strlenp,p,ptr));
	}
#if 0
      }
#endif

      if (*ptr == '\n') {
	*ptr = '\0';
	debug(printf("Now string is %s.\n",ptr));
      } else if ((p = index(ptr,'\n')) != NULL) {
	if (p[-1] == '\r') {
	  p--;
	}
	*p = '\0';
	debug(printf("Now string is %s.\n",ptr));
      } else if (*ptr == EOF) {
	/* No line feed, but end of file.  Handle below. */
	debug(printf("End of file seen\n"));
      } else {
	/* No line feed, but not end of file.  Read too long, so using another method. */
	debug(printf("No line feed, but not end of file.  Using Intlist_T.\n"));
	intlist = (Intlist_T) NULL;
	i = 0;
	while (i <= MAX_EXPECTED_READLENGTH && Start[i] != '\0') {
	  debug(printf("Pushing %c\n",Start[i]));
	  intlist = Intlist_push_in(intlist,Start[i]);
	  i++;
	}
	while ((*nextchar = gzgetc(fp)) != EOF && *nextchar != '\n') {
	  intlist = Intlist_push_in(intlist,*nextchar);
	}
	if (*nextchar == '\n') {
	  *nextchar = gzgetc(fp);
	}

	intlist = Intlist_reverse(intlist);
	*longstring = Intlist_to_char_array_in(&i,intlist);
	Intlist_free_in(&intlist);

	debug(printf("Intlist method returning %d\n",i));
	return i;
      }
    }
    ptr += strlen(ptr);

    /* Peek at character after eoln */
    *nextchar = gzgetc(fp);

    debug(printf("Returning %ld with nextchar %c\n",(ptr - &(Start[0]))/sizeof(char),*nextchar));
    return (ptr - &(Start[0]))/sizeof(char);
  }
}
#endif

#ifdef HAVE_BZLIB
static int
input_oneline_bzip2 (int *nextchar, char **longstring, char *Start,
		     Bzip2_T fp, bool possible_fasta_header_p) {
  int remainder;
  char *ptr, *p = NULL;
  int strlenp, nspaces;

  int i;
  Intlist_T intlist;

  debug(printf("Entering input_oneline with nextchar = %c\n",*nextchar));
  *longstring = (char *) NULL;

  ptr = &(Start[0]);
  remainder = (&(Start[MAX_EXPECTED_READLENGTH]) - ptr)/sizeof(char);
  if (*nextchar == EOF || (possible_fasta_header_p == true && (*nextchar == '>' || *nextchar == '+'))) {
    debug(printf("EOF or > or +: Returning 0\n"));
    return 0;
  } else if (*nextchar == '\n') {
    debug(printf("Blank line: Returning 0\n"));
    return 0;
  } else {
    *ptr++ = (char) *nextchar;
    if ((p = bzgets(fp,ptr,remainder+1)) == NULL) {
      /* NULL if file ends with a blank line */
      debug(printf("Blank line. read %s.\n",ptr));
    } else {
      debug(printf("Read %s.\n",ptr));
#if 0
      if (pc_linefeeds_p == true) {
#endif
	while ((p = find_spaces(&nspaces,ptr)) != NULL) {
	  ptr = p;
	  p += nspaces;
	  strlenp = strlen(p);
	  memmove(ptr,p,strlenp);
	  ptr[strlenp] = '\0';
	  debug(printf("Found %d spaces.  Did memmove of %d chars at %p to %p\n",nspaces,strlenp,p,ptr));
	}
#if 0
      }
#endif

      if (*ptr == '\n') {
	*ptr = '\0';
	debug(printf("Now string is %s.\n",ptr));
      } else if ((p = index(ptr,'\n')) != NULL) {
	if (p[-1] == '\r') {
	  p--;
	}
	*p = '\0';
	debug(printf("Now string is %s.\n",ptr));
      } else if (*ptr == EOF) {
	/* No line feed, but end of file.  Handle below. */
	debug(printf("End of file seen\n"));
      } else {
	/* No line feed, but not end of file.  Read too long, so using another method. */
	debug(printf("No line feed, but not end of file.  Using Intlist_T.\n"));
	intlist = (Intlist_T) NULL;
	i = 0;
	while (i <= MAX_EXPECTED_READLENGTH && Start[i] != '\0') {
	  debug(printf("Pushing %c\n",Start[i]));
	  intlist = Intlist_push_in(intlist,Start[i]);
	  i++;
	}
	while ((*nextchar = bzgetc(fp)) != EOF && *nextchar != '\n') {
	  intlist = Intlist_push_in(intlist,*nextchar);
	}
	if (*nextchar == '\n') {
	  *nextchar = bzgetc(fp);
	}

	intlist = Intlist_reverse(intlist);
	*longstring = Intlist_to_char_array_in(&i,intlist);
	Intlist_free_in(&intlist);

	debug(printf("Intlist method returning %d\n",i));
	return i;
      }
    }
    ptr += strlen(ptr);

    /* Peek at character after eoln */
    *nextchar = bzgetc(fp);

    debug(printf("Returning %ld with nextchar %c\n",(ptr - &(Start[0]))/sizeof(char),*nextchar));
    return (ptr - &(Start[0]))/sizeof(char);
  }
}
#endif


static char complCode[128] = COMPLEMENT_LC;

static char *
make_complement (char *sequence, unsigned int length) {
  char *complement;
  int i, j;

  complement = (char *) CALLOC_IN(length+1,sizeof(char));
  for (i = length-1, j = 0; i >= 0; i--, j++) {
    complement[j] = complCode[(int) sequence[i]];
  }
  complement[length] = '\0';
  return complement;
}


#if 0
static char *
make_complement_uppercase (char *sequence, unsigned int length) {
  char *complement;
  char uppercaseCode[128] = UPPERCASE_U2T;
  int i, j;

  complement = (char *) CALLOC_IN(length+1,sizeof(char));
  for (i = length-1, j = 0; i >= 0; i--, j++) {
    complement[j] = uppercaseCode[(int) complCode[(int) sequence[i]]];
  }
  complement[length] = '\0';
  return complement;
}
#endif


static char *
make_reverse (char *sequence, unsigned int length) {
  char *reverse;
  int i, j;

  if (sequence == NULL) {
    return (char *) NULL;
  } else {
    reverse = (char *) CALLOC_IN(length+1,sizeof(char));
    for (i = length-1, j = 0; i >= 0; i--, j++) {
      reverse[j] = sequence[i];
    }
    reverse[length] = '\0';
    return reverse;
  }
}

static char *
make_uppercase (char *sequence, unsigned int length) {
  char *uppercase;
#ifdef PMAP
  char uppercaseCode[128] = UPPERCASE_STD;
#else
  char uppercaseCode[128] = UPPERCASE_U2T;
#endif
  unsigned int i;

  uppercase = (char *) CALLOC_IN(length+1,sizeof(char));
  for (i = 0; i < length; i++) {
    uppercase[i] = uppercaseCode[(int) sequence[i]];
  }
  uppercase[length] = '\0';
  return uppercase;
}


/************************************************************************
 *   Dynamic programming for paired-end short reads
 ************************************************************************/

#ifdef USE_DYNAMIC_PROGRAMMING

typedef char Direction_T;
#define VERT 4
#define HORIZ 2
#define DIAG 1
#define STOP 0

static int **matrix_ptrs;
static int *matrix_space;
static Direction_T **directions_ptrs;
static Direction_T *directions_space;
static int **jump_ptrs;
static int *jump_space;


#define MATCH 1
#define MISMATCH -3
#define END_OPEN -14
#define OPEN -10
#define EXTEND -3

static int **pairdistance_array;
static int *jump_penalty_array;

static void
pairdistance_init () {
  int j, ptr;
  int c, c1, c2;

  pairdistance_array = (int **) CALLOC(128,sizeof(int *));
  pairdistance_array[0] = (int *) CALLOC(128*128,sizeof(int));
  ptr = 0;
  for (j = 1; j < 128; j++) {
    ptr += 128;
    pairdistance_array[j] = &(pairdistance_array[0][ptr]);
  }

  for (c1 = 'A'; c1 <= 'Z'; c1++) {
    for (c2 = 'A'; c2 < 'Z'; c2++) {
      pairdistance_array[c1][c2] = MISMATCH;
    }
  }

  for (c = 'A'; c < 'Z'; c++) {
    pairdistance_array[c][c] = MATCH;
  }

  return;
}

static void
jump_penalty_init (int maxlength) {
  int length, penalty;

  jump_penalty_array = (int *) CALLOC(maxlength+1,sizeof(int));

  penalty = OPEN;
  for (length = 0; length <= maxlength; length++) {
    jump_penalty_array[length] = penalty;
    penalty += EXTEND;
  }

  return;
}



#if 0
/* Called by reads_store.c */
void
Shortread_dynprog_init (int maxlength) {

  matrix_ptrs = (int **) CALLOC(maxlength+1,sizeof(int *));
  matrix_space = (int *) CALLOC((maxlength+1)*(maxlength+1),sizeof(int));
  directions_ptrs = (Direction_T **) CALLOC(maxlength+1,sizeof(Direction_T *));
  directions_space = (Direction_T *) CALLOC((maxlength+1)*(maxlength+1),sizeof(Direction_T));
  jump_ptrs = (int **) CALLOC(maxlength+1,sizeof(int *));
  jump_space = (int *) CALLOC((maxlength+1)*(maxlength+1),sizeof(int));

  pairdistance_init();
  jump_penalty_init(maxlength);

  return;
}

void
Shortread_dynprog_term () {
  FREE(jump_penalty_array);
  FREE(pairdistance_array[0]);
  FREE(pairdistance_array);

  FREE(matrix_ptrs);
  FREE(matrix_space);
  FREE(directions_ptrs);
  FREE(directions_space);
  FREE(jump_ptrs);
  FREE(jump_space);
  return;
}
#endif


/* Makes a matrix of dimensions 0..length1 x 0..length2 inclusive */
static int **
Matrix_alloc (int length1, int length2, int **ptrs, int *space) {
  int **matrix, i;

  if (length1 < 0 || length2 < 0) {
    fprintf(stderr,"shortread: lengths are negative: %d %d\n",length1,length2);
    abort();
  }

  matrix = ptrs;
  matrix[0] = space;
  for (i = 1; i <= length1; i++) {
    matrix[i] = &(matrix[i-1][length2 + 1]);
  }

  /* Clear memory only around the band, but doesn't work with Gotoh P1
     and Q1 matrices */
  /*
  for (r = 0; r <= length1; r++) {
    if ((clo = r - lband - 1) >= 0) {
      matrix[r][clo] = 0;
    }
    if ((chigh = r + rband + 1) <= length2) {
      matrix[r][chigh] = 0;
    }
  }
  */
  memset((void *) space,0,(length1+1)*(length2+1)*sizeof(int));

  return matrix;
}


#ifdef DEBUG2
static void
Matrix_print (int **matrix, int length1, int length2, char *sequence1, char *sequence2,
	      bool revp) {
  int i, j;

  printf("  ");
  for (j = 0; j <= length2; ++j) {
    if (j == 0) {
      printf("    ");
    } else {
      printf("  %c ",revp ? sequence2[-j+1] : sequence2[j-1]);
    }
  }
  printf("\n");

  for (i = 0; i <= length1; ++i) {
    if (i == 0) {
      printf("  ");
    } else {
      printf("%c ",revp ? sequence1[-i+1] : sequence1[i-1]);
    }
    for (j = 0; j <= length2; ++j) {
      printf("%3d ",matrix[i][j]);
    }
    printf("\n");
  }
  printf("\n");

  return;
}
#endif


/* Makes a matrix of dimensions 0..length1 x 0..length2 inclusive */
static Direction_T **
Directions_alloc (int length1, int length2, Direction_T **ptrs, Direction_T *space) {
  Direction_T **directions;
  int i;

  directions = ptrs;
  directions[0] = space;
  for (i = 1; i <= length1; i++) {
    directions[i] = &(directions[i-1][length2 + 1]);
  }

  /* Clear memory only around the band, but may not work with Gotoh
     method */
  /*
    for (r = 0; r <= length1; r++) {
    if ((clo = r - lband - 1) >= 0) {
    directions[r][clo] = STOP;
    }
    if ((chigh = r + rband + 1) <= length2) {
    directions[r][chigh] = STOP;
    }
    }
  */
  memset((void *) space,0,(length1+1)*(length2+1)*sizeof(Direction_T));

  return directions;
}


#ifdef DEBUG2
static void
Directions_print (Direction_T **directions, int **jump, int length1, int length2, 
		  char *sequence1, char *sequence2, bool revp) {
  int i, j;
  char buffer[4];

  printf("  ");
  for (j = 0; j <= length2; ++j) {
    if (j == 0) {
      printf("    ");
    } else {
      printf("  %c ",revp ? sequence2[-j+1] : sequence2[j-1]);
    }
  }
  printf("\n");

  for (i = 0; i <= length1; ++i) {
    if (i == 0) {
      printf("  ");
    } else {
      printf("%c ",revp ? sequence1[-i+1] : sequence1[i-1]);
    }
    for (j = 0; j <= length2; ++j) {
      if (directions[i][j] == DIAG) {
	sprintf(buffer,"D%d",jump[i][j]);
      } else if (directions[i][j] == HORIZ) {
	sprintf(buffer,"H%d",jump[i][j]);
      } else if (directions[i][j] == VERT) {
	sprintf(buffer,"V%d",jump[i][j]);
      } else {
	sprintf(buffer,"S%d",0);
      }
      printf("%3s ",buffer);
    }
    printf("\n");
  }
  printf("\n");
  return;
}
#endif


static int **
compute_scores_lookup (Direction_T ***directions, int ***jump, char *sequence1, char *sequence2, 
		       int length1, int length2) {
  int **matrix;
  int r, c, r1, c1;
  int bestscore, score, bestjump, j;
  Direction_T bestdir;

  matrix = Matrix_alloc(length1,length2,matrix_ptrs,matrix_space);
  *directions = Directions_alloc(length1,length2,directions_ptrs,directions_space);
  *jump = Matrix_alloc(length1,length2,jump_ptrs,jump_space);

  matrix[0][0] = 0;
  (*directions)[0][0] = STOP;
  (*jump)[0][0] = 0;

  /* Row 0 initialization */
  for (c = 1; c <= length2; c++) {
    matrix[0][c] = END_OPEN;
    (*directions)[0][c] = HORIZ;
    (*jump)[0][c] = c;
  }

  /* Column 0 initialization */
  for (r = 1; r <= length1; r++) {
    matrix[r][0] = END_OPEN;
    (*directions)[r][0] = VERT;
    (*jump)[r][0] = r;
  }

  for (r = 1; r <= length1; r++) {
    /* na1 = sequence1[r-1]; */

    for (c = 1; c <= length2; c++) {
      /* na2 = sequence2[c-1]; */

      /* Diagonal case */
      bestscore = matrix[r-1][c-1] + pairdistance_array[(int) sequence1[r-1]][(int) sequence2[c-1]];
      bestdir = DIAG;
      bestjump = 1;
      
      /* Horizontal case */
      for (c1 = c-1, j = 1; c1 >= 1; c1--, j++) {
	if ((*directions)[r][c1] == DIAG) {
	  score = matrix[r][c1] + jump_penalty_array[j];
	  if (score > bestscore) {
	    bestscore = score;
	    bestdir = HORIZ;
	    bestjump = j;
	  }
	}
      }

      /* Vertical case */
      for (r1 = r-1, j = 1; r1 >= 1; r1--, j++) {
	if ((*directions)[r1][c] == DIAG) {
	  score = matrix[r1][c] + jump_penalty_array[j];
	  if (score > bestscore) {
	    bestscore = score;
	    bestdir = VERT;
	    bestjump = j;
	  }
	}
      }

      /*
	debug(printf("At %d,%d, scoreV = %d, scoreH = %d, scoreD = %d\n",
	r,c,scoreV,scoreH,scoreD));
      */
      
      /* Update */
      matrix[r][c] = bestscore;
      (*directions)[r][c] = bestdir;
      (*jump)[r][c] = bestjump;
    }
  }

  /* Final cell */
  bestscore = matrix[length1-1][length2-1] + pairdistance_array[(int) sequence1[length1-1]][(int) sequence2[length2-1]];
  bestdir = DIAG;
  bestjump = 1;

  for (c = 1; c < length2; c++) {
    if ((score = matrix[length1][c] + END_OPEN) > bestscore) {
      bestscore = score;
      bestdir = HORIZ;
      bestjump = length2 - c;
    }
  }

  for (r = 1; r < length1; r++) {
    if ((score = matrix[r][length2] + END_OPEN) > bestscore) {
      bestscore = score;
      bestdir = VERT;
      bestjump = length1 - r;
    }
  }

  /* Update */
  matrix[length1][length2] = bestscore;
  (*directions)[length1][length2] = bestdir;
  (*jump)[length1][length2] = bestjump;

  /*
  debug2(Matrix_print(P0,length1,length2));
  debug2(Matrix_print(P1,length1,length2));
  debug2(Matrix_print(Q0,length1,length2));
  debug2(Matrix_print(Q1,length1,length2));
  */
  debug2(Matrix_print(matrix,length1,length2,sequence1,sequence2,/*revp*/false));
  debug2(Directions_print(*directions,*jump,length1,length2,
			  sequence1,sequence2,/*revp*/false));

  return matrix;
}


static void
traceback (int *nmatches, int *nmismatches, int *chop1, int *chop2,
	   Direction_T **directions, int **jump, char *sequence1, char *sequence2,
	   int length1, int length2) {
  int lastjump;
  int r, c;
  Direction_T lastdir;

  if (directions[length1][length2] != VERT) {
    *chop1 = *chop2 = 0;
  } else {
    *chop1 = jump[length1][length2];

    r = length1;
    c = length2;
    while (directions[r][c] != STOP) {
      if ((lastdir = directions[r][c]) == DIAG) {
	if (sequence1[r-1] == sequence2[c-1]) {
	  (*nmatches)++;
	} else {
	  (*nmismatches)++;
	}

	lastjump = 1;
	r--; c--;
	
      } else if (lastdir == HORIZ) {
	lastjump = jump[r][c];
	c -= lastjump;
	
      } else if (directions[r][c] == VERT) {
	lastjump = jump[r][c];
	r -= lastjump;
	
      } else {
	abort();
      }
    }

    if (lastdir != HORIZ) {
      *chop1 = *chop2 = 0;
    } else {
      *chop2 = lastjump;
    }
  }

  return;
}

/* Modeled after Dynprog_single_gap */
void
Shortread_chop_primers_slow (T queryseq1, T queryseq2) {
  int finalscore;
  int **matrix, **jump;
  Direction_T **directions;
  int chop1, chop2;
  int nmatches, nmismatches;

  matrix = compute_scores_lookup(&directions,&jump,queryseq1->contents_uc,queryseq2->contents_uc,
				 queryseq1->fulllength,queryseq2->fulllength);
  finalscore = matrix[queryseq1->fulllength][queryseq2->fulllength];

  if (finalscore <= 0) {
    chop1 = chop2 = 0;
    debug2(nmatches = 0);
    debug2(nmismatches = 0);
  } else {
    nmatches = nmismatches = 0;
    traceback(&nmatches,&nmismatches,&chop1,&chop2,directions,jump,
	      queryseq1->contents_uc,queryseq2->contents_uc,
	      queryseq1->fulllength,queryseq2->fulllength);
    if (nmismatches > PAIRED_ADAPTER_NMISMATCHES_ALLOWED) {
      chop1 = chop2 = 0;
    } else {
      if (chop1 > 0) {
	queryseq1->choplength = chop1;

	queryseq1->chop = (char *) MALLOC_IN((chop1+1)*sizeof(char));
	strncpy(queryseq1->chop,&(queryseq1->contents[queryseq1->fulllength - chop1]),chop1);
	queryseq1->contents[queryseq1->fulllength - chop1] = '\0';
	queryseq1->contents_uc[queryseq1->fulllength - chop1] = '\0';

	if (queryseq1->quality != NULL) {
	  queryseq1->chop_quality = (char *) MALLOC_IN((chop1+1)*sizeof(char));
	  strncpy(queryseq1->chop_quality,&(queryseq1->quality[queryseq1->fulllength - chop1]),chop1);
	  queryseq1->quality[queryseq1->fulllength - chop1] = '\0';
	}

	queryseq1->fulllength -= chop1;
      }
      if (chop2 > 0) {
	queryseq2->choplength = chop2;

	queryseq2->chop = (char *) MALLOC_IN((chop2+1)*sizeof(char));
	strncpy(queryseq2->chop,queryseq2->contents,chop2);
	queryseq2->chop[chop2] = '\0';

	queryseq2->contents = &(queryseq2->contents[chop2]);
	queryseq2->contents_uc = &(queryseq2->contents_uc[chop2]);

	if (queryseq2->quality != NULL) {
	  queryseq2->chop_quality = (char *) MALLOC_IN((chop2+1)*sizeof(char));
	  strncpy(queryseq2->chop_quality,queryseq2->quality,chop2);
	  queryseq2->chop_quality[chop2] = '\0';

	  queryseq2->quality = &(queryseq2->quality[chop2]);
	}

	queryseq2->fulllength -= chop2;
      }
    }
  }

  debug2(printf("finalscore = %d, nmatches = %d, nmismatches = %d, chop1 = %d, chop2 = %d\n",
		finalscore,nmatches,nmismatches,chop1,chop2));

  return;
}

#endif


#if 0
static void
chop_left (T this, int choplength) {

  debug3(printf("Chopping %d from left end\n",choplength));

  this->left_choplength = choplength;
  this->left_chop = (char *) MALLOC_IN((choplength+1)*sizeof(char));
  this->fulllength -= choplength;

  strncpy(this->left_chop,&(this->contents_uc[0]),choplength);
  this->left_chop[choplength] = '\0';

  this->contents += choplength;
  this->contents_uc += choplength;
  this->queryrc[this->fulllength] = '\0'; /* Chop from other end */
    
  if (this->quality != NULL) {
    this->left_chop_quality = (char *) MALLOC_IN((choplength+1)*sizeof(char));
    strncpy(this->left_chop_quality,&(this->quality[0]),choplength);
    this->left_chop_quality[choplength] = '\0';

    this->quality += choplength;
  }

  return;
}
#endif


#if 0
static void
chop_right (T this, int choplength) {
  int prev_fulllength = this->fulllength;

  debug3(printf("Chopping %d from right end\n",choplength));

  this->right_choplength = choplength;
  this->right_chop = (char *) MALLOC_IN((choplength+1)*sizeof(char));
  this->fulllength -= choplength;

  strncpy(this->right_chop,&(this->contents_uc[prev_fulllength - choplength]),choplength);
  this->right_chop[choplength] = '\0';

  this->contents[this->fulllength] = '\0';
  this->contents_uc[this->fulllength] = '\0';
  this->queryrc += choplength;	/* Chop from other end */
    
  if (this->quality != NULL) {
    this->right_chop_quality = (char *) MALLOC_IN((choplength+1)*sizeof(char));
    strncpy(this->right_chop_quality,&(this->quality[prev_fulllength - choplength]),choplength);
    this->right_chop_quality[choplength] = '\0';

    this->quality[this->fulllength] = '\0';
  }

  return;
}
#endif


static void
chop_ends (T this, int left_choplength, int right_choplength) {
  int prev_fulllength = this->fulllength;
  
  if (left_choplength > 0) {
    this->left_choplength = left_choplength;
    this->left_chop = (char *) MALLOC_IN((left_choplength+1)*sizeof(char));
    strncpy(this->left_chop,&(this->contents_uc[0]),left_choplength);
    this->left_chop[left_choplength] = '\0';
    this->fulllength -= left_choplength;
  }

  if (right_choplength > 0) {
    this->right_choplength = right_choplength;
    this->right_chop = (char *) MALLOC_IN((right_choplength+1)*sizeof(char));
    strncpy(this->right_chop,&(this->contents_uc[prev_fulllength - right_choplength]),right_choplength);
    this->right_chop[right_choplength] = '\0';
    this->fulllength -= right_choplength;
  }

  this->contents += left_choplength;
  this->contents_uc += left_choplength;
  this->queryrc += right_choplength; /* Chop from other end */
  this->contents[this->fulllength] = '\0';
  this->contents_uc[this->fulllength] = '\0';
  this->queryrc[this->fulllength] = '\0';

  if (this->quality != NULL) {
    if (left_choplength > 0) {
      this->left_chop_quality = (char *) MALLOC_IN((left_choplength+1)*sizeof(char));
      strncpy(this->left_chop_quality,&(this->quality[0]),left_choplength);
      this->left_chop_quality[left_choplength] = '\0';
    }
    if (right_choplength > 0) {
      this->right_chop_quality = (char *) MALLOC_IN((right_choplength+1)*sizeof(char));
      strncpy(this->right_chop_quality,&(this->quality[prev_fulllength - right_choplength]),right_choplength);
      this->right_chop_quality[right_choplength] = '\0';
    }
    
    this->quality += left_choplength;
    this->quality[this->fulllength] = '\0';
  }

  debug(printf("Left: %s\n",this->left_chop));
  debug(printf("Full: %s\n",this->contents));
  debug(printf("Right: %s\n",this->right_chop));

  debug(printf("Left quality: %s\n",this->left_chop_quality));
  debug(printf("Quality: %s\n",this->contents));
  debug(printf("Right quality: %s\n",this->right_chop_quality));

  return;
}


#define MIN_POLY_AT 15

static bool
chop_poly_at (int *left_choplength, int *right_choplength, char *contents, int fulllength) {
  int left_apos = -1, left_tpos = -1,
    right_apos = fulllength, right_tpos = fulllength;
  int min_left_ascore = 0, min_left_tscore = 0, min_right_ascore = 0, min_right_tscore = 0,
    left_ascore, left_tscore, right_ascore, right_tscore;
  int querypos;
  char *p, c;

  debug(printf("Entering chop_poly_at\n"));

  left_ascore = left_tscore = MIN_POLY_AT;
  p = contents;
  for (querypos = 0; querypos < fulllength; querypos++) {
    if ((c = *p++) == 'A') {
      left_tscore += 3;
      if (--left_ascore < min_left_ascore) {
	left_apos = querypos;
	min_left_ascore = left_ascore;
      }
    } else if (c == 'T') {
      left_ascore += 3;
      if (--left_tscore < min_left_tscore) {
	left_tpos = querypos;
	min_left_tscore = left_tscore;
      }
    } else {
      left_ascore += 3;
      left_tscore += 3;
    }
  }

  right_ascore = right_tscore = MIN_POLY_AT;
  p = &(contents[fulllength - 1]);
  for (querypos = fulllength - 1; querypos >= 0; querypos--) {
    if ((c = *p--) == 'A') {
      right_tscore += 3;
      if (--right_ascore < min_right_ascore) {
	right_apos = querypos;
	min_right_ascore = right_ascore;
      }
    } else if (c == 'T') {
      right_ascore += 3;
      if (--right_tscore < min_right_tscore) {
	right_tpos = querypos;
	min_right_tscore = right_tscore;
      }
    } else {
      right_ascore += 3;
      right_tscore += 3;
    }
  }

  debug(printf("left_apos %d, left_tpos %d\n",left_apos,left_tpos));
  debug(printf("right_apos %d, right_tpos %d\n",right_apos,right_tpos));

  if (left_apos >= right_apos) {
    /* Entire read is poly-A */
    *left_choplength = fulllength;
    *right_choplength = 0;
    return true;

  } else if (left_tpos >= right_tpos) {
    /* Entire read is poly-T */
    *left_choplength = fulllength;
    *right_choplength = 0;
    return true;

  } else if (left_apos >= 0 && left_apos > fulllength - right_apos) {
    *left_choplength = left_apos + 1;
    *right_choplength = 0;
    return true;
    
  } else if (right_apos < fulllength && fulllength - right_apos > left_apos) {
    *left_choplength = 0;
    *right_choplength = fulllength - right_apos;
    return true;

  } else if (left_tpos >= 0 && left_tpos > fulllength - right_tpos) {
    *left_choplength = left_tpos + 1;
    *right_choplength = 0;
    return true;
    
  } else if (right_tpos < fulllength && fulllength - right_tpos > left_tpos) {
    *left_choplength = 0;
    *right_choplength = fulllength - right_tpos;
    return true;

  } else {
    *left_choplength = *right_choplength = 0;
    return false;
  }
}




bool
Shortread_chop_primers (T queryseq1, T queryseq2) {
  bool choppedp = false;
  int chop1 = 0, chop2 = 0;
  int nmatches, nmismatches;
  int fulllength1 = queryseq1->fulllength;
  int fulllength2 = queryseq2->fulllength;
  int minlength;
  char *contents1 = queryseq1->contents_uc;
  char *contents2 = queryseq2->contents_uc;

  int best_score = 0, score;
  int jstart, j, i;

  if (fulllength1 < fulllength2) {
    minlength = fulllength1;
  } else {
    minlength = fulllength2;
  }

  debug2(printf("jstart must be < %d - %d and < %d - %d\n",
		fulllength2,PAIRED_ADAPTER_MINLENGTH,fulllength1,PAIRED_ADAPTER_MINLENGTH));
  for (jstart = 0; jstart < minlength - PAIRED_ADAPTER_MINLENGTH; jstart++) {
    debug2(printf("jstart = %d\n",jstart));
    i = 0;
    j = jstart;
    nmismatches = 0;
    while (nmismatches <= PAIRED_ADAPTER_NMISMATCHES_ALLOWED && i < minlength - jstart) {
      debug2(printf("At i = %d, j = %d, comparing %c and %c\n",i,j,contents1[i],contents2[j]));
      assert(i < fulllength1);
      assert(j < fulllength2);
      if (contents1[i] != contents2[j]) {
	nmismatches++;
      }
      i++;
      j++;
    }
    if (nmismatches <= PAIRED_ADAPTER_NMISMATCHES_ALLOWED) {
      nmatches = j - nmismatches;
      if ((score = nmatches*3 - nmismatches) > best_score) {
	best_score = score;
	chop1 = jstart;
	chop2 = jstart;
      }
    }
  }

  debug2(printf("chop1 = %d, chop2 = %d\n",chop1,chop2));

  if (chop1 > 0) {
    choppedp = true;
    queryseq1->right_choplength = chop1;
    queryseq1->right_chop = (char *) MALLOC_IN((chop1+1)*sizeof(char));
    queryseq1->fulllength -= chop1;
    fulllength1 = queryseq1->fulllength;

    strncpy(queryseq1->right_chop,&(queryseq1->contents_uc[fulllength1]),chop1);
    queryseq1->right_chop[chop1] = '\0';

    queryseq1->contents[fulllength1] = '\0';
    queryseq1->contents_uc[fulllength1] = '\0';
    
    if (queryseq1->quality != NULL) {
      queryseq1->right_chop_quality = (char *) MALLOC_IN((chop1+1)*sizeof(char));
      strncpy(queryseq1->right_chop_quality,&(queryseq1->quality[fulllength1]),chop1);
      queryseq1->right_chop_quality[chop1] = '\0';

      queryseq1->quality[fulllength1] = '\0';
    }

  }

  if (chop2 > 0) {
    choppedp = true;
    queryseq2->right_choplength = chop2;
    queryseq2->right_chop = (char *) MALLOC_IN((chop2+1)*sizeof(char));
    strncpy(queryseq2->right_chop,queryseq2->contents_uc,chop2);
    queryseq2->right_chop[chop2] = '\0';

    queryseq2->contents = &(queryseq2->contents[chop2]);
    queryseq2->contents_uc = &(queryseq2->contents_uc[chop2]);
    
    if (queryseq2->quality != NULL) {
      queryseq2->right_chop_quality = (char *) MALLOC_IN((chop2+1)*sizeof(char));
      strncpy(queryseq2->right_chop_quality,queryseq2->quality,chop2);
      queryseq2->right_chop_quality[chop2] = '\0';

      queryseq2->quality = &(queryseq2->quality[chop2]);
    }
    queryseq2->fulllength -= chop2;

  }

  return choppedp;
}


bool
Shortread_find_primers (T queryseq1, T queryseq2) {
  int chop1 = 0, chop2 = 0;
  int nmatches, nmismatches;
  int fulllength1 = queryseq1->fulllength;
  int fulllength2 = queryseq2->fulllength;
  int minlength;
  char *contents1 = queryseq1->contents_uc;
  char *contents2 = queryseq2->contents_uc;

  int best_score = 0, score;
  int jstart, j, i;

  if (fulllength1 < fulllength2) {
    minlength = fulllength1;
  } else {
    minlength = fulllength2;
  }

  debug2(printf("jstart must be < %d - %d and < %d - %d\n",
		fulllength2,PAIRED_ADAPTER_MINLENGTH,fulllength1,PAIRED_ADAPTER_MINLENGTH));
  for (jstart = 0; jstart < minlength - PAIRED_ADAPTER_MINLENGTH; jstart++) {
    debug2(printf("jstart = %d\n",jstart));
    i = 0;
    j = jstart;
    nmismatches = 0;
    while (nmismatches <= PAIRED_ADAPTER_NMISMATCHES_ALLOWED && i < minlength - jstart) {
      debug2(printf("At i = %d, j = %d, comparing %c and %c\n",i,j,contents1[i],contents2[j]));
      assert(i < fulllength1);
      assert(j < fulllength2);
      if (contents1[i] != contents2[j]) {
	nmismatches++;
      }
      i++;
      j++;
    }
    if (nmismatches <= PAIRED_ADAPTER_NMISMATCHES_ALLOWED) {
      nmatches = j - nmismatches;
      if ((score = nmatches*3 - nmismatches) > best_score) {
	best_score = score;
	chop1 = jstart;
	chop2 = jstart;
      }
    }
  }

  debug2(printf("chop1 = %d, chop2 = %d\n",chop1,chop2));

  if (chop1 > 0 || chop2 > 0) {
    return true;
  } else {
    return false;
  }
}



int
Shortread_max_overlap (T queryseq1, T queryseq2) {
  if (queryseq1->fulllength < queryseq2->fulllength) {
    return queryseq1->fulllength;
  } else {
    return queryseq2->fulllength;
  }
}


int
Shortread_find_overlap (T queryseq1, T queryseq2) {
  int overlap = 0;
  int nmatches, nmismatches;
  int fulllength1 = queryseq1->fulllength;
  int fulllength2 = queryseq2->fulllength;
  char *contents1 = queryseq1->contents_uc;
  char *contents2 = queryseq2->contents_uc;

  int best_score = 0, score;
  int istart, i, j;

  if (queryseq1->overlap >= 0) {
    return queryseq1->overlap;
  } else {
    debug2(printf("istart must be < %d - %d and < %d - %d\n",
		  fulllength1,OVERLAP_MINLENGTH,fulllength2,OVERLAP_MINLENGTH));
    for (istart = 0; istart < fulllength1 - OVERLAP_MINLENGTH && istart < fulllength2 - OVERLAP_MINLENGTH; istart++) {
      debug2(printf("istart = %d\n",istart));
      i = istart;
      j = 0;
      nmismatches = 0;
      while (nmismatches <= OVERLAP_NMISMATCHES_ALLOWED && i < fulllength1 - istart) {
	debug2(printf("At i = %d, j = %d, comparing %c and %c\n",i,j,contents1[i],contents2[j]));
	if (contents1[i] != contents2[j]) {
	  nmismatches++;
	}
	i++;
	j++;
      }
      if (nmismatches <= OVERLAP_NMISMATCHES_ALLOWED) {
	nmatches = j - nmismatches;
	if ((score = nmatches*3 - nmismatches) > best_score) {
	  best_score = score;
	  overlap = istart;
	}
      }
    }

    debug2(printf("overlap = %d\n",overlap));

    queryseq1->overlap = queryseq2->overlap = overlap;
    return overlap;
  }
}


/************************************************************************
 *   Short reads
 ************************************************************************/

#define SKIPPED 1

/* Previously needed to be extern for uniqscan.c.  But now without
   uniqscan, we could make this static and get barcode_length and
   endtrim_length from above */
static T
Shortread_new (char *acc, char *restofheader, bool filterp,
	       char *short_sequence, char *long_sequence, int sequence_length,
	       char *short_quality, char *long_quality, int quality_length,
	       int barcode_length, int endtrim_length, bool chop_poly_at_p, bool invertp,
	       bool copy_acc_p, bool skipp) {
  T new;
  char *sequence, *quality;
  int left_choplength, right_choplength;

  if (sequence_length == 0) {
    return (T) NULL;

  } else if (skipp == true) {
    if (long_quality != NULL) {
      FREE_IN(long_quality);
    }
    if (long_sequence != NULL) {
      FREE_IN(long_sequence);
    }
    return (T) SKIPPED;
  }

  if (long_sequence != NULL) {
    sequence = long_sequence;
  } else {
    sequence = short_sequence;
  }

  if (long_quality != NULL) {
    quality = long_quality;
  } else {
    quality = short_quality;
  }


  new = (T) MALLOC_IN(sizeof(*new));

  if (acc == NULL) {
    new->acc = (char *) NULL;
  } else if (copy_acc_p == false) {
    new->acc = acc;
  } else {
    new->acc = (char *) CALLOC_IN(strlen(acc)+1,sizeof(char));
    strcpy(new->acc,acc);
  }

  if (restofheader == NULL) {
    new->restofheader = (char *) NULL;
  } else if (copy_acc_p == false) {
    new->restofheader = restofheader;
  } else {
    new->restofheader = (char *) CALLOC_IN(strlen(restofheader)+1,sizeof(char));
    strcpy(new->restofheader,restofheader);
  }

  new->filterp = filterp;
  new->invertedp = invertp;

  /* printf("Before: sequence %s, quality %s\n",sequence,quality); */

  new->barcode_length = barcode_length;
  if (barcode_length == 0) {
    new->barcode = (char *) NULL;
  } else {
    new->barcode = (char *) MALLOC_IN((barcode_length+1)*sizeof(char));
    strncpy(new->barcode,&(sequence[0]),barcode_length);
    new->barcode[barcode_length] = '\0';
  }

  new->endtrim_length = endtrim_length;
  if (endtrim_length == 0) {
    new->endtrim = (char *) NULL;
  } else {
    new->endtrim = (char *) MALLOC_IN((endtrim_length+1)*sizeof(char));
    strncpy(new->endtrim,&(sequence[sequence_length - endtrim_length]),endtrim_length);
    new->endtrim[endtrim_length] = '\0';
  }

  sequence_length -= barcode_length;
  sequence_length -= endtrim_length;
  sequence = &(sequence[barcode_length]);
  if (quality != NULL) {
    quality_length -= barcode_length;
    quality_length -= endtrim_length;
    quality = &(quality[barcode_length]);
  }

  new->fulllength = sequence_length; /* After barcode_length was removed */


  /* printf("After: barcode %s, sequence %s, quality %s\n",new->barcode,sequence,quality); */

  if (invertp == true) {
    new->contents = new->contents_alloc = make_complement(sequence,sequence_length);
    new->contents_uc = new->contents_uc_alloc = 
      make_uppercase(new->contents,sequence_length);

    new->queryrc = new->queryrc_alloc = make_complement(new->contents_uc,sequence_length);

    if (quality_length == 0) {
      new->quality = new->quality_alloc = (char *) NULL;
    } else {
      new->quality = new->quality_alloc = make_reverse(quality,quality_length);
    }

  } else {
    new->contents = new->contents_alloc = (char *) MALLOC_IN((sequence_length+1)*sizeof(char));
    strncpy(new->contents,sequence,sequence_length);
    new->contents[sequence_length] = '\0';

    new->contents_uc = new->contents_uc_alloc =
      make_uppercase(new->contents,sequence_length);

    new->queryrc = new->queryrc_alloc = make_complement(new->contents_uc,sequence_length);

    if (quality_length == 0) {
      new->quality = new->quality_alloc = (char *) NULL;
    } else {
      new->quality = new->quality_alloc = (char *) MALLOC_IN((quality_length+1)*sizeof(char));
      strncpy(new->quality,quality,quality_length);
      new->quality[quality_length] = '\0';
    }
  }

  new->overlap = -1;		/* Indicates not computed yet */

  new->left_chop = (char *) NULL;
  new->left_chop_quality = (char *) NULL;
  new->left_choplength = 0;

  new->right_chop = (char *) NULL;
  new->right_chop_quality = (char *) NULL;
  new->right_choplength = 0;

  if (long_quality != NULL) {
    FREE_IN(long_quality);
  }
  if (long_sequence != NULL) {
    FREE_IN(long_sequence);
  }

  if (chop_poly_at_p == false) {
    /* Don't chop single-cell info */
  } else if (chop_poly_at(&left_choplength,&right_choplength,new->contents,new->fulllength) == false) {
    /* Skip */
  } else {
    chop_ends(new,left_choplength,right_choplength);
  }

  return new;
}


#if 0
bool
Shortread_left_polya_p (T this, int overhang) {
  int pos;
  int n_a = 0, n_other = 0;

  printf("Entered Shortread_polya_p with overhang %d\n",overhang);
  printf("Left: %s\n",this->left_chop);
  printf("Full: %s\n",this->contents_uc);
  printf("Right: %s\n",this->right_chop);

  printf("Looking at central %.*s\n",overhang,this->contents_uc);
  for (pos = 0; pos < overhang; pos++) {
    if (this->contents_uc[pos] == 'A') {
      n_a++;
    } else {
      n_other++;
    }
  }
  
  printf("Have %d As and %d others\n",n_a,n_other);
  if (n_a > n_other) {
    return true;
  } else {
    return false;
  }
}
#endif


bool
Shortread_left_polyt_p (T this, int overhang) {
  int pos;
  int n_t = 0, n_other = 0;

#ifdef DEBUG3
  printf("Entered Shortread_polyt_p with overhang %d\n",overhang);
  printf("Left: %s\n",this->left_chop);
  printf("Full: %s\n",this->contents_uc);
  printf("Right: %s\n",this->right_chop);
  printf("Looking at central %.*s\n",overhang,this->contents_uc);
#endif

  for (pos = 0; pos < overhang; pos++) {
    if (this->contents_uc[pos] == 'T') {
      n_t++;
    } else {
      n_other++;
    }
  }
  
  debug3(printf("Have %d Ts and %d others\n",n_t,n_other));
  if (n_t > n_other) {
    return true;
  } else {
    return false;
  }
}


bool
Shortread_right_polya_p (T this, int overhang) {
  int pos;
  int n_a = 0, n_other = 0;

#ifdef DEBUG3
  printf("Entered Shortread_polya_p with overhang %d\n",overhang);
  printf("Left: %s\n",this->left_chop);
  printf("Full: %s\n",this->contents_uc);
  printf("Right: %s\n",this->right_chop);
  printf("Looking at central %s\n",&(this->contents_uc[this->fulllength - overhang]));
#endif

  for (pos = this->fulllength - overhang; pos < this->fulllength; pos++) {
    if (this->contents_uc[pos] == 'A') {
      n_a++;
    } else {
      n_other++;
    }
  }
  
  debug3(printf("Have %d As and %d others\n",n_a,n_other));
  if (n_a > n_other) {
    return true;
  } else {
    return false;
  }
}


#if 0
bool
Shortread_right_polyt_p (T this, int overhang) {
  int pos;
  int n_t = 0, n_other = 0;

  printf("Entered Shortread_polyt_p with overhang %d\n",overhang);
  printf("Left: %s\n",this->left_chop);
  printf("Full: %s\n",this->contents_uc);
  printf("Right: %s\n",this->right_chop);

  printf("Looking at central %s\n",&(this->contents_uc[this->fulllength - overhang]));
  for (pos = this->fulllength - overhang; pos < this->fulllength; pos++) {
    if (this->contents_uc[pos] == 'T') {
      n_t++;
    } else {
      n_other++;
    }
  }
  
  printf("Have %d Ts and %d others\n",n_t,n_other);
  if (n_t > n_other) {
    return true;
  } else {
    return false;
  }
}
#endif


T
Shortread_read_fasta_text (int *nextchar, int *nchars1, int *nchars2, T *queryseq2,
			   FILE **input1, FILE **input2, char *read_files_command,
			   char ***files, int *nfiles, bool single_cell_p, bool skipp) {
  T queryseq1;
  int nextchar2 = '\0';		/* Can be anything but EOF */
  char *acc, *restofheader, *acc2, *restofheader2;
  char *long_read_1, *long_read_2, *long_quality;
  int fulllength1, fulllength2, quality_length;
  bool filterp;

  while (1) {
    queryseq1 = *queryseq2 = (T) NULL;
    if (*input1 == NULL	|| *nextchar == EOF) { /* was feof(*input1) */
      if (*input1 != NULL) {
	debugf(fprintf(stderr,"Master closing input file 1 using fclose\n"));
	if (read_files_command != NULL) {
	  pclose(*input1);
	} else {
	  fclose(*input1);
	}
	*input1 = NULL;
      }
      if (*input2 != NULL) {
	debugf(fprintf(stderr,"Master closing input file 2 using fclose\n"));
	if (read_files_command != NULL) {
	  pclose(*input2);
	} else {
	  fclose(*input2);
	}
	*input2 = NULL;
      }

      if (*nfiles == 0) {
	*nextchar = EOF;
	return (T) NULL;

      } else if (*nfiles == 1 || force_single_end_p == true) {
	if ((*input1 = Fopen_read_text(read_files_command,(*files)[0])) == NULL) {
	  (*files) += 1;
	  (*nfiles) -= 1;
	  *nextchar = EOF;
	  return (T) NULL;
	} else {
	  debugf(fprintf(stderr,"Master opening input file 1\n"));
	  *input2 = NULL;
	  (*files) += 1;
	  (*nfiles) -= 1;
	  *nextchar = '\0';	/* Was nextchar2 = '\0', which is incorrect */
	}

      } else {
	while (*nfiles > 0 &&
	       (*input1 = Fopen_read_text(read_files_command,(*files)[0])) == NULL) {
	  (*files) += 1;
	  (*nfiles) -= 1;
	}
	if (*input1 == NULL) {
	  *nextchar = EOF;
	  return (T) NULL;
	} else {
	  (*files) += 1;
	  (*nfiles) -= 1;
	  *nextchar = '\0';
	}
      }
    }

    if (*nextchar == '\0') {
      if ((*nextchar = Shortread_input_init(&(*nchars1),*input1)) == EOF) {
	return (T) NULL;
      }
    }

    debug(printf("** Getting header for fasta text\n"));
    if ((acc = input_header(&(*nchars1),&filterp,&restofheader,*nextchar,*input1,skipp)) == NULL) {
      /* fprintf(stderr,"No header\n"); */
      /* File ends after >.  Don't process, but loop again */
      *nextchar = EOF;
    } else if ((*nextchar = fgetc(*input1)) == '\r' || *nextchar == '\n') {
      /* Process blank lines and loop again */
      while (*nextchar != EOF && ((*nextchar = fgetc(*input1)) != '>')) {
	*nchars1 += 1;
      }
      if (*nextchar != EOF) {
	*nchars1 += 1;
      }
    } else if ((fulllength1 = input_oneline(&(*nextchar),&(*nchars1),&long_read_1,&(Read1[0]),*input1,
					    /*possible_fasta_header_p*/true)) == 0) {
      *nchars1 += 1;			    /* For first "else if" clause */
      /* fprintf(stderr,"length is zero\n"); */
      /* No sequence1.  Don't process, but loop again */
      /* *nextchar = EOF; */
    } else {
      *nchars1 += 1;			    /* For first "else if" clause */
      /* queryseq1 is in Read1 */
      /* See what is in next line */
      if ((fulllength2 = input_oneline(&(*nextchar),&(*nchars1),&long_read_2,&(Read2[0]),*input1,
				       /*possible_fasta_header_p*/true)) > 0) {
	/* Paired-end, single file.  queryseq1 is in Read1 and queryseq2 is in Read2 */
	if (*nextchar == '+') {
	  /* Paired-end with quality strings */
	  skip_header(&(*nchars1),*input1,*nextchar);
	  *nextchar = fgetc(*input1);
	  *nchars1 += 1;
	  quality_length = input_oneline(&(*nextchar),&(*nchars1),&long_quality,&(Quality[0]),*input1,
					 /*possible_fasta_header_p*/false);
	  if (quality_length != fulllength1) {
	    fprintf(stderr,"Length %d of quality score differs from length %d of nucleotides in sequence %s\n",
		    quality_length,fulllength1,acc);
	    abort();
	  }

	  queryseq1 = Shortread_new(acc,restofheader,filterp,Read1,long_read_1,fulllength1,
				    Quality,long_quality,quality_length,barcode_length,endtrim_length,
				    chop_poly_at_first_p,invert_first_p,/*copy_acc_p*/false,skipp);
	  quality_length = input_oneline(&(*nextchar),&(*nchars1),&long_quality,&(Quality[0]),*input1,
					 /*possible_fasta_header_p*/false);
	  if (quality_length != fulllength2) {
	    fprintf(stderr,"Length %d of quality score differs from length %d of nucleotides in sequence %s\n",
		    quality_length,fulllength2,acc);
	    abort();
	  }
	      
	  (*queryseq2) = Shortread_new(/*acc*/NULL,/*restofheader*/NULL,filterp,Read2,long_read_2,fulllength2,
				       Quality,long_quality,quality_length,barcode_length,endtrim_length,
				       /*chop_poly_at_p*/true,invert_second_p,/*copy_acc_p*/false,skipp);
	} else {
	  queryseq1 = Shortread_new(acc,restofheader,filterp,Read1,long_read_1,fulllength1,
				    /*quality*/NULL,/*long_quality*/NULL,/*quality_length*/0,
				    barcode_length,endtrim_length,chop_poly_at_first_p,
				    invert_first_p,/*copy_acc_p*/false,skipp);
	  (*queryseq2) = Shortread_new(/*acc*/NULL,/*restofheader*/NULL,filterp,Read2,long_read_2,fulllength2,
				       /*quality*/NULL,/*long_quality*/NULL,/*quality_length*/0,
				       barcode_length,endtrim_length,/*chop_poly_at_p*/true,
				       invert_second_p,/*copy_acc_p*/false,skipp);
	}

      } else {
	if (*input2 == NULL && *nfiles > 0 && force_single_end_p == false &&
	    (*input2 = Fopen_read_text(read_files_command,(*files)[0])) != NULL) {
	  debugf(fprintf(stderr,"Master opening input file 2\n"));
	  (*files) += 1;
	  (*nfiles) -= 1;
	  if ((nextchar2 = Shortread_input_init(&(*nchars2),*input2)) == EOF) {
	    if (read_files_command != NULL) {
	      pclose(*input2);
	    } else {
	      fclose(*input2);
	    }
	    *input2 = NULL;
	  }
	}

	if (*input2 != NULL) {
	  /* Paired-end in two files */
	  if ((acc2 = input_header(&(*nchars2),&filterp,&restofheader2,nextchar2,*input2,skipp)) == NULL) {
	    /* fprintf(stderr,"No header\n"); */
	    /* File ends after >.  Don't process, but loop again */
	    (*queryseq2) = (T) NULL;
	    nextchar2 = EOF;
	  } else if ((nextchar2 = fgetc(*input2)) == '\r' || nextchar2 == '\n') {
	    /* Process blank lines and loop again */
	    while (nextchar2 != EOF && ((nextchar2 = fgetc(*input2)) != '>')) {
	      *nchars2 += 1;
	    }
	    if (nextchar2 != EOF) {
	      *nchars2 += 1;
	    }
	    (*queryseq2) = (T) NULL;
	  } else if ((fulllength2 = input_oneline(&nextchar2,&(*nchars2),&long_read_2,&(Read2[0]),*input2,
						  /*possible_fasta_header_p*/true)) == 0) {
	    *nchars2 += 1;			  /* For first "else if" clause */
	    /* fprintf(stderr,"length is zero\n"); */
	    /* No sequence1.  Don't process, but loop again */
	    /* nextchar2 = EOF; */
	    (*queryseq2) = (T) NULL;
	  } else {
	    *nchars2 += 1;			  /* For first "else if" clause */
	    if (*nextchar == '+') {
	      /* End 1 with a quality string */
	      skip_header(&(*nchars1),*input1,*nextchar);
	      *nextchar = fgetc(*input1);
	      *nchars1 += 1;
	      quality_length = input_oneline(&(*nextchar),&(*nchars1),&long_quality,&(Quality[0]),*input1,
					     /*possible_fasta_header_p*/false);
	      if (quality_length != fulllength1) {
		fprintf(stderr,"Length %d of quality score differs from length %d of nucleotides in sequence %s\n",
			quality_length,fulllength1,acc);
		abort();
	      } else {
		queryseq1 = Shortread_new(acc,restofheader,filterp,Read1,long_read_1,fulllength1,
					  Quality,long_quality,quality_length,barcode_length,endtrim_length,
					  chop_poly_at_first_p,invert_first_p,/*copy_acc_p*/false,skipp);
	      }
	    } else {
	      /* End 1 without quality string */
	      queryseq1 = Shortread_new(acc,restofheader,filterp,Read1,long_read_1,fulllength1,
					/*quality*/NULL,/*long_quality*/NULL,/*quality_length*/0,
					barcode_length,endtrim_length,chop_poly_at_first_p,
					invert_first_p,/*copy_acc_p*/false,skipp);
	    }

	    if (nextchar2 == '+') {
	      /* End 2 with a quality string */
	      skip_header(&(*nchars2),*input2,nextchar2);
	      nextchar2 = fgetc(*input2);
	      *nchars2 += 1;
	      quality_length = input_oneline(&nextchar2,&(*nchars2),&long_quality,&(Quality[0]),*input2,
					     /*possible_fasta_header_p*/false);
	      if (quality_length != fulllength2) {
		fprintf(stderr,"Length %d of quality score differs from length %d of nucleotides in sequence %s\n",
			quality_length,fulllength2,acc2);
		abort();
	      } else {
		/* For FASTA, drop second accession */
		(*queryseq2) = Shortread_new(/*acc2*/NULL,/*restofheader2*/NULL,filterp,Read2,long_read_2,fulllength2,
					     Quality,long_quality,quality_length,barcode_length,endtrim_length,
					     /*chop_poly_at_p*/true,invert_second_p,/*copy_acc_p*/false,skipp);
		if (skipp == false) {
		  FREE_IN(acc2);
		  FREE_IN(restofheader2);
		}
	      }
	    } else {
	      /* End 2 without quality string */
	      (*queryseq2) = Shortread_new(/*acc2*/NULL,/*restofheader2*/NULL,filterp,Read2,long_read_2,fulllength2,
					   /*quality*/NULL,/*long_quality*/NULL,/*quality_length*/0,
					   barcode_length,endtrim_length,/*chop_poly_at_p*/true,
					   invert_second_p,/*copy_acc_p*/false,skipp);
	      if (skipp == false) {
		FREE_IN(acc2);
		FREE_IN(restofheader2);
	      }
	    }
	  }

	} else {
	  /* Single-end: Either EOF, '>', or '+' */
	  if (*nextchar == '+') {
	    /* Single-end with a quality string */
	    skip_header(&(*nchars1),*input1,*nextchar);
	    *nextchar = fgetc(*input1);
	    *nchars1 += 1;
	    quality_length = input_oneline(&(*nextchar),&(*nchars1),&long_quality,&(Quality[0]),*input1,
					   /*possible_fasta_header_p*/false);
	    if (quality_length != fulllength1) {
	      fprintf(stderr,"Length %d of quality score differs from length %d of nucleotides in sequence %s\n",
		      quality_length,fulllength1,acc);
	      abort();
	    } else {
	      queryseq1 = Shortread_new(acc,restofheader,filterp,Read1,long_read_1,fulllength1,
					Quality,long_quality,quality_length,barcode_length,endtrim_length,
					chop_poly_at_first_p,invert_first_p,/*copy_acc_p*/false,skipp);
	    }
	  } else {
	    /* Single-end without quality string */
	    queryseq1 = Shortread_new(acc,restofheader,filterp,Read1,long_read_1,fulllength1,
				      /*quality*/NULL,/*long_quality*/NULL,/*quality_length*/0,
				      barcode_length,endtrim_length,chop_poly_at_first_p,
				      invert_first_p,/*copy_acc_p*/false,skipp);
	  }
	  (*queryseq2) = (T) NULL;
	}
      }

      if (queryseq1 == (T) SKIPPED) {
	return (T) SKIPPED;
      } else if (queryseq1 != NULL && queryseq1->acc != NULL /*&& queryseq1->fulllength > 0*/) {
	debug(printf("nchars %d: Returning queryseq with contents %s\n",*nchars1,queryseq1->contents));
	return queryseq1;
      }
    }
  }
}


#ifdef HAVE_ZLIB
T
Shortread_read_fasta_gzip (int *nextchar, T *queryseq2,
			   gzFile *input1, gzFile *input2,
			   char ***files, int *nfiles,
			   bool single_cell_p, bool skipp) {
  T queryseq1;
  int nextchar2 = '\0';		/* Can be anything but EOF */
  char *acc, *restofheader, *acc2, *restofheader2;
  char *long_read_1, *long_read_2, *long_quality;
  int fulllength1, fulllength2, quality_length;
  bool filterp;

  while (1) {
    queryseq1 = *queryseq2 = (T) NULL;
    if (*input1 == NULL || *nextchar == EOF) { /* was gzeof(*input1) */
      if (*input1 != NULL) {
	debugf(fprintf(stderr,"Master closing input 1 using gzclose\n"));
	gzclose(*input1);
	*input1 = NULL;
      }
      if (*input2 != NULL) {
	debugf(fprintf(stderr,"Master closing input 2 using gzclose\n"));
	gzclose(*input2);
	*input2 = NULL;
      }

      if (*nfiles == 0) {
	*nextchar = EOF;
	return (T) NULL;

      } else if (*nfiles == 1 || force_single_end_p == true) {
	if ((*input1 = gzopen((*files)[0],"rb")) == NULL) {
	  fprintf(stderr,"Can't open file %s => skipping it.\n",(*files)[0]);
	  (*files) += 1;
	  (*nfiles) -= 1;
	  *nextchar = EOF;
	  return (T) NULL;
	} else {
	  debugf(fprintf(stderr,"Master opening input file 1\n"));
#ifdef HAVE_ZLIB_GZBUFFER
	  gzbuffer(*input1,GZBUFFER_SIZE);
#endif
	  *input2 = NULL;
	  (*files) += 1;
	  (*nfiles) -= 1;
	  *nextchar = '\0';	/* Was nextchar2 = '\0', which is incorrect */
	}

      } else {
	while (*nfiles > 0 && (*input1 = gzopen((*files)[0],"rb")) == NULL) {
	  fprintf(stderr,"Can't open file %s => skipping it.\n",(*files)[0]);
	  (*files) += 1;
	  (*nfiles) -= 1;
	}
	if (*input1 == NULL) {
	  *nextchar = EOF;
	  return (T) NULL;
	} else {
#ifdef HAVE_ZLIB_GZBUFFER
	  gzbuffer(*input1,GZBUFFER_SIZE);
#endif
	  (*files) += 1;
	  (*nfiles) -= 1;
	  *nextchar = '\0';
	}
      }
    }

    if (*nextchar == '\0') {
      if ((*nextchar = Shortread_input_init_gzip(*input1)) == EOF) {
	return (T) NULL;
      }
    }

    debug(printf("** Getting header for fasta gzip\n"));
    if ((acc = input_header_gzip(&filterp,&restofheader,*nextchar,
				 *input1,skipp)) == NULL) {
      /* fprintf(stderr,"No header\n"); */
      /* File ends after >.  Don't process, but loop again */
      *nextchar = EOF;
    } else if ((*nextchar = gzgetc(*input1)) == '\r' || *nextchar == '\n') {
      /* Process blank lines and loop again */
      while (*nextchar != EOF && ((*nextchar = gzgetc(*input1)) != '>')) {
      }
    } else if ((fulllength1 = input_oneline_gzip(&(*nextchar),&long_read_1,&(Read1[0]),
						 *input1,/*possible_fasta_header_p*/true)) == 0) {
      /* fprintf(stderr,"length is zero\n"); */
      /* No sequence1.  Don't process, but loop again */
      /* *nextchar = EOF; */
    } else {
      /* queryseq1 is in Read1 */
      /* See what is in next line */
      if ((fulllength2 = input_oneline_gzip(&(*nextchar),&long_read_2,&(Read2[0]),
					    *input1,/*possible_fasta_header_p*/true)) > 0) {
	/* Paired-end, single file.  queryseq1 is in Read1 and queryseq2 is in Read2 */
	if (*nextchar == '+') {
	  /* Paired-end with quality strings */
	  skip_header_gzip(*input1,*nextchar);
	  *nextchar = gzgetc(*input1);
	  quality_length = input_oneline_gzip(&(*nextchar),&long_quality,&(Quality[0]),
					      *input1,/*possible_fasta_header_p*/false);
	  if (quality_length != fulllength1) {
	    fprintf(stderr,"Length %d of quality score differs from length %d of nucleotides in sequence %s\n",
		    quality_length,fulllength1,acc);
	    abort();
	  }

	  queryseq1 = Shortread_new(acc,restofheader,filterp,Read1,long_read_1,fulllength1,
				    Quality,long_quality,quality_length,barcode_length,endtrim_length,
				    chop_poly_at_first_p,invert_first_p,/*copy_acc_p*/false,skipp);
	  quality_length = input_oneline_gzip(&(*nextchar),&long_quality,&(Quality[0]),
					      *input1,/*possible_fasta_header_p*/false);
	  if (quality_length != fulllength2) {
	    fprintf(stderr,"Length %d of quality score differs from length %d of nucleotides in sequence %s\n",
		    quality_length,fulllength2,acc);
	    abort();
	  }

	  (*queryseq2) = Shortread_new(/*acc*/NULL,/*restofheader*/NULL,filterp,Read2,long_read_2,fulllength2,
				       Quality,long_quality,quality_length,barcode_length,endtrim_length,
				       /*chop_poly_at_p*/true,invert_second_p,/*copy_acc_p*/false,skipp);
	} else {
	  queryseq1 = Shortread_new(acc,restofheader,filterp,Read1,long_read_1,fulllength1,
				    /*quality*/NULL,/*long_quality*/NULL,/*quality_length*/0,
				    barcode_length,endtrim_length,chop_poly_at_first_p,
				    invert_first_p,/*copy_acc_p*/false,skipp);
	  (*queryseq2) = Shortread_new(/*acc*/NULL,/*restofheader*/NULL,filterp,Read2,long_read_2,fulllength2,
				       /*quality*/NULL,/*long_quality*/NULL,/*quality_length*/0,
				       barcode_length,endtrim_length,/*chop_poly_at_p*/true,
				       invert_second_p,/*copy_acc_p*/false,skipp);
	}

      } else {
	if (*input2 == NULL && *nfiles > 0 && force_single_end_p == false &&
	    (*input2 = gzopen((*files)[0],"rb")) != NULL) {
#ifdef HAVE_ZLIB_GZBUFFER
	  gzbuffer(*input2,GZBUFFER_SIZE);
#endif
	  (*files) += 1;
	  (*nfiles) -= 1;
	  if ((nextchar2 = Shortread_input_init_gzip(*input2)) == EOF) {
	    gzclose(*input2);
	    *input2 = NULL;
	  }
	}

	if (*input2 != NULL) {
	  /* Paired-end in two files */
	  if ((acc2 = input_header_gzip(&filterp,&restofheader2,nextchar2,
					*input2,skipp)) == NULL) {
	    /* fprintf(stderr,"No header\n"); */
	    /* File ends after >.  Don't process, but loop again */
	    (*queryseq2) = (T) NULL;
	    nextchar2 = EOF;
	  } else if ((nextchar2 = gzgetc(*input2)) == '\r' || nextchar2 == '\n') {
	    /* Process blank lines and loop again */
	    while (nextchar2 != EOF && ((nextchar2 = gzgetc(*input2)) != '>')) {
	    }
	    (*queryseq2) = (T) NULL;
	  } else if ((fulllength2 = input_oneline_gzip(&nextchar2,&long_read_2,&(Read2[0]),
						       *input2,/*possible_fasta_header_p*/true)) == 0) {
	    /* fprintf(stderr,"length is zero\n"); */
	    /* No sequence1.  Don't process, but loop again */
	    /* nextchar2 = EOF; */
	    (*queryseq2) = (T) NULL;
	  } else {
	    if (*nextchar == '+') {
	      /* End 1 with a quality string */
	      skip_header_gzip(*input1,*nextchar);
	      *nextchar = gzgetc(*input1);
	      quality_length = input_oneline_gzip(&(*nextchar),&long_quality,&(Quality[0]),
						  *input1,/*possible_fasta_header_p*/false);
	      if (quality_length != fulllength1) {
		fprintf(stderr,"Length %d of quality score differs from length %d of nucleotides in sequence %s\n",
			quality_length,fulllength1,acc);
		abort();
	      } else {
		queryseq1 = Shortread_new(acc,restofheader,filterp,Read1,long_read_1,fulllength1,
					  Quality,long_quality,quality_length,barcode_length,endtrim_length,
					  chop_poly_at_first_p,invert_first_p,/*copy_acc_p*/false,skipp);
	      }
	    } else {
	      /* End 1 without quality string */
	      queryseq1 = Shortread_new(acc,restofheader,filterp,Read1,long_read_1,fulllength1,
					/*quality*/NULL,/*long_quality*/NULL,/*quality_length*/0,
					barcode_length,endtrim_length,chop_poly_at_first_p,
					invert_first_p,/*copy_acc_p*/false,skipp);
	    }

	    if (nextchar2 == '+') {
	      /* End 2 with a quality string */
	      skip_header_gzip(*input2,nextchar2);
	      nextchar2 = gzgetc(*input2);
	      quality_length = input_oneline_gzip(&nextchar2,&long_quality,&(Quality[0]),
						  *input2,/*possible_fasta_header_p*/false);
	      if (quality_length != fulllength2) {
		fprintf(stderr,"Length %d of quality score differs from length %d of nucleotides in sequence %s\n",
			quality_length,fulllength2,acc2);
		abort();
	      } else {
		/* For FASTA, drop second accession */
		(*queryseq2) = Shortread_new(/*acc2*/NULL,/*restofheader2*/NULL,filterp,Read2,long_read_2,fulllength2,
					     Quality,long_quality,quality_length,barcode_length,endtrim_length,
					     /*chop_poly_at_p*/true,invert_second_p,/*copy_acc_p*/false,skipp);
		if (skipp == false) {
		  FREE_IN(acc2);
		  FREE_IN(restofheader2);
		}
	      }
	    } else {
	      /* End 2 without quality string */
	      (*queryseq2) = Shortread_new(/*acc2*/NULL,/*restofheader2*/NULL,filterp,Read2,long_read_2,fulllength2,
					   /*quality*/NULL,/*long_quality*/NULL,/*quality_length*/0,barcode_length,endtrim_length,
					   /*chop_poly_at_p*/true,invert_second_p,/*copy_acc_p*/false,skipp);
	      if (skipp == false) {
		FREE_IN(acc2);
		FREE_IN(restofheader2);
	      }
	    }
	  }

	} else {
	  /* Single-end: Either EOF, '>', or '+' */
	  if (*nextchar == '+') {
	    /* Single-end with a quality string */
	    skip_header_gzip(*input1,*nextchar);
	    *nextchar = gzgetc(*input1);
	    quality_length = input_oneline_gzip(&(*nextchar),&long_quality,&(Quality[0]),
						*input1,/*possible_fasta_header_p*/false);
	    if (quality_length != fulllength1) {
	      fprintf(stderr,"Length %d of quality score differs from length %d of nucleotides in sequence %s\n",
		      quality_length,fulllength1,acc);
	      abort();
	    } else {
	      queryseq1 = Shortread_new(acc,restofheader,filterp,Read1,long_read_1,fulllength1,
					Quality,long_quality,quality_length,barcode_length,endtrim_length,
					chop_poly_at_first_p,invert_first_p,/*copy_acc_p*/false,skipp);
	    }
	  } else {
	    /* Single-end without quality string */
	    queryseq1 = Shortread_new(acc,restofheader,filterp,Read1,long_read_1,fulllength1,
				      /*quality*/NULL,/*long_quality*/NULL,/*quality_length*/0,
				      barcode_length,endtrim_length,chop_poly_at_first_p,
				      invert_first_p,/*copy_acc_p*/false,skipp);
	  }
	  (*queryseq2) = (T) NULL;
	}
      }

      if (queryseq1 == (T) SKIPPED) {
	return (T) SKIPPED;
      } else if (queryseq1 != NULL && queryseq1->acc != NULL /*&& queryseq1->fulllength > 0*/) {
	debug(printf("Returning queryseq with contents %s\n",queryseq1->contents));
	return queryseq1;
      }
    }

  }
}
#endif


#ifdef HAVE_BZLIB
T
Shortread_read_fasta_bzip2 (int *nextchar, T *queryseq2,
			    Bzip2_T *input1, Bzip2_T *input2,
			    char ***files, int *nfiles,
			    bool single_cell_p, bool skipp) {
  T queryseq1;
  int nextchar2 = '\0';		/* Can be anything but EOF */
  char *acc, *restofheader, *acc2, *restofheader2;
  char *long_read_1, *long_read_2, *long_quality;
  int fulllength1, fulllength2, quality_length;
  bool filterp;

  while (1) {
    queryseq1 = *queryseq2 = (T) NULL;
    if (*input1 == NULL || *nextchar == EOF) { /* Was bzeof(*input1) */
      if (*input1 != NULL) {
	debugf(fprintf(stderr,"Master closing input 1 using Bzip2_free\n"));
	Bzip2_free(&(*input1));
	*input1 = NULL;
      }
      if (*input2 != NULL) {
	debugf(fprintf(stderr,"Master closing input 2 using Bzip2_free\n"));
	Bzip2_free(&(*input2));
	*input2 = NULL;
      }

      if (*nfiles == 0) {
	*nextchar = EOF;
	return (T) NULL;

      } else if (*nfiles == 1 || force_single_end_p == true) {
	if ((*input1 = Bzip2_new((*files)[0])) == NULL) {
	  fprintf(stderr,"Can't open file %s => skipping it.\n",(*files)[0]);
	  (*files) += 1;
	  (*nfiles) -= 1;
	  *nextchar = EOF;
	  return (T) NULL;
	} else {
	  debugf(fprintf(stderr,"Master opening input file 1\n"));
	  *input2 = NULL;
	  (*files) += 1;
	  (*nfiles) -= 1;
	  *nextchar = '\0';	/* Was nextchar2 = '\0', which is incorrect */
	}

      } else {
	while (*nfiles > 0 && (*input1 = Bzip2_new((*files)[0])) == NULL) {
	  fprintf(stderr,"Can't open file %s => skipping it.\n",(*files)[0]);
	  (*files) += 1;
	  (*nfiles) -= 1;
	}
	if (*input1 == NULL) {
	  *nextchar = EOF;
	  return (T) NULL;
	} else {
	  (*files) += 1;
	  (*nfiles) -= 1;
	  *nextchar = '\0';
	}
      }
    }

    if (*nextchar == '\0') {
      if ((*nextchar = Shortread_input_init_bzip2(*input1)) == EOF) {
	return (T) NULL;
      }
    }

    debug(printf("** Getting header for fasta bzip2\n"));
    if ((acc = input_header_bzip2(&filterp,&restofheader,*nextchar,
				  *input1,skipp)) == NULL) {
      /* fprintf(stderr,"No header\n"); */
      /* File ends after >.  Don't process, but loop again */
      *nextchar = EOF;
    } else if ((*nextchar = bzgetc(*input1)) == '\r' || *nextchar == '\n') {
      /* Process blank lines and loop again */
      while (*nextchar != EOF && ((*nextchar = bzgetc(*input1)) != '>')) {
      }
    } else if ((fulllength1 = input_oneline_bzip2(&(*nextchar),&long_read_1,&(Read1[0]),
						  *input1,/*possible_fasta_header_p*/true)) == 0) {
      /* fprintf(stderr,"length is zero\n"); */
      /* No sequence1.  Don't process, but loop again */
      /* *nextchar = EOF */
    } else {
      /* queryseq1 is in Read1 */
      /* See what is in next line */
      if ((fulllength2 = input_oneline_bzip2(&(*nextchar),&long_read_2,&(Read2[0]),
					     *input1,/*possible_fasta_header_p*/true)) > 0) {
	/* Paired-end, single file.  queryseq1 is in Read1 and queryseq2 is in Read2 */
	if (*nextchar == '+') {
	  /* Paired-end with quality strings */
	  skip_header_bzip2(*input1,*nextchar);
	  *nextchar = bzgetc(*input1);
	  quality_length = input_oneline_bzip2(&(*nextchar),&long_quality,&(Quality[0]),
					       *input1,/*possible_fasta_header_p*/false);
	  if (quality_length != fulllength1) {
	    fprintf(stderr,"Length %d of quality score differs from length %d of nucleotides in sequence %s\n",
		    quality_length,fulllength1,acc);
	    abort();
	  }

	  queryseq1 = Shortread_new(acc,restofheader,filterp,Read1,long_read_1,fulllength1,
				    Quality,long_quality,quality_length,barcode_length,endtrim_length,
				    chop_poly_at_first_p,invert_first_p,/*copy_acc_p*/false,skipp);
	  quality_length = input_oneline_bzip2(&(*nextchar),&long_quality,&(Quality[0]),
					      *input1,/*possible_fasta_header_p*/false);
	  if (quality_length != fulllength2) {
	    fprintf(stderr,"Length %d of quality score differs from length %d of nucleotides in sequence %s\n",
		    quality_length,fulllength2,acc);
	    abort();
	  }

	  (*queryseq2) = Shortread_new(/*acc*/NULL,/*restofheader*/NULL,filterp,Read2,long_read_2,fulllength2,
				       Quality,long_quality,quality_length,barcode_length,endtrim_length,
				       /*chop_poly_at_p*/true,invert_second_p,/*copy_acc_p*/false,skipp);
	} else {
	  queryseq1 = Shortread_new(acc,restofheader,filterp,Read1,long_read_1,fulllength1,
				    /*quality*/NULL,/*long_quality*/NULL,/*quality_length*/0,
				    barcode_length,endtrim_length,chop_poly_at_first_p,
				    invert_first_p,/*copy_acc_p*/false,skipp);
	  (*queryseq2) = Shortread_new(/*acc*/NULL,/*restofheader*/NULL,filterp,Read2,long_read_2,fulllength2,
				       /*quality*/NULL,/*long_quality*/NULL,/*quality_length*/0,
				       barcode_length,endtrim_length,/*chop_poly_at_p*/true,
				       invert_second_p,/*copy_acc_p*/false,skipp);
	}

      } else {
	if (*input2 == NULL && *nfiles > 0 && force_single_end_p == false &&
	    (*input2 = Bzip2_new((*files)[0])) != NULL) {
	  (*files) += 1;
	  (*nfiles) -= 1;
	  if ((nextchar2 = Shortread_input_init_bzip2(*input2)) == EOF) {
	    Bzip2_free(&(*input2));
	    *input2 = NULL;
	  }
	}

	if (*input2 != NULL) {
	  /* Paired-end in two files */
	  if ((acc2 = input_header_bzip2(&filterp,&restofheader2,nextchar2,
					 *input2,skipp)) == NULL) {
	    /* fprintf(stderr,"No header\n"); */
	    /* File ends after >.  Don't process, but loop again */
	    (*queryseq2) = (T) NULL;
	    nextchar2 = EOF;
	  } else if ((nextchar2 = bzgetc(*input2)) == '\r' || nextchar2 == '\n') {
	    /* Process blank lines and loop again */
	    while (nextchar2 != EOF && ((nextchar2 = bzgetc(*input2)) != '>')) {
	    }
	    (*queryseq2) = (T) NULL;
	  } else if ((fulllength2 = input_oneline_bzip2(&nextchar2,&long_read_2,&(Read2[0]),
							*input2,/*possible_fasta_header_p*/true)) == 0) {
	    /* fprintf(stderr,"length is zero\n"); */
	    /* No sequence1.  Don't process, but loop again */
	    /* nextchar2 = EOF; */
	    (*queryseq2) = (T) NULL;
	  } else {
	    if (*nextchar == '+') {
	      /* End 1 with a quality string */
	      skip_header_bzip2(*input1,*nextchar);
	      *nextchar = bzgetc(*input1);
	      quality_length = input_oneline_bzip2(&(*nextchar),&long_quality,&(Quality[0]),
						   *input1,/*possible_fasta_header_p*/false);
	       if (quality_length != fulllength1) {
		 fprintf(stderr,"Length %d of quality score differs from length %d of nucleotides in sequence %s\n",
			 quality_length,fulllength1,acc);
		 abort();
	       } else {
		 queryseq1 = Shortread_new(acc,restofheader,filterp,Read1,long_read_1,fulllength1,
					   Quality,long_quality,quality_length,barcode_length,endtrim_length,
					   chop_poly_at_first_p,invert_first_p,/*copy_acc_p*/false,skipp);
	       }
	    } else {
	      /* End 1 without quality string */
	      queryseq1 = Shortread_new(acc,restofheader,filterp,Read1,long_read_1,fulllength1,
					/*quality*/NULL,/*long_quality*/NULL,/*quality_length*/0,
					barcode_length,endtrim_length,chop_poly_at_first_p,
					invert_first_p,/*copy_acc_p*/false,skipp);
	    }

	    if (nextchar2 == '+') {
	      /* End 2 with a quality string */
	      skip_header_bzip2(*input2,nextchar2);
	      nextchar2 = bzgetc(*input2);
	      quality_length = input_oneline_bzip2(&nextchar2,&long_quality,&(Quality[0]),
						   *input2,/*possible_fasta_header_p*/false);
	       if (quality_length != fulllength2) {
		 fprintf(stderr,"Length %d of quality score differs from length %d of nucleotides in sequence %s\n",
			 quality_length,fulllength2,acc2);
		 abort();
	       } else {
		 /* For FASTA, drop second accession */
		 (*queryseq2) = Shortread_new(/*acc2*/NULL,/*restofheader2*/NULL,filterp,Read2,long_read_2,fulllength2,
					      Quality,long_quality,quality_length,barcode_length,endtrim_length,
					      /*chop_poly_at_p*/true,invert_second_p,/*copy_acc_p*/false,skipp);
		 if (skipp == false) {
		   FREE_IN(acc2);
		   FREE_IN(restofheader2);
		 }
	       }
	    } else {
	      /* End 2 without quality string */
	      (*queryseq2) = Shortread_new(/*acc2*/NULL,/*restofheader2*/NULL,filterp,Read2,long_read_2,fulllength2,
					   /*quality*/NULL,/*long_quality*/NULL,/*quality_length*/0,
					   barcode_length,endtrim_length,/*chop_poly_at_p*/true,
					   invert_second_p,/*copy_acc_p*/false,skipp);
	      if (skipp == false) {
		FREE_IN(acc2);
		FREE_IN(restofheader2);
	      }
	    }
	  }

	} else {
	  /* Single-end: Either EOF, '>', or '+' */
	  if (*nextchar == '+') {
	    /* Single-end with a quality string */
	    skip_header_bzip2(*input1,*nextchar);
	    *nextchar = bzgetc(*input1);
	    quality_length = input_oneline_bzip2(&(*nextchar),&long_quality,&(Quality[0]),
						*input1,/*possible_fasta_header_p*/false);
	    if (quality_length != fulllength1) {
	      fprintf(stderr,"Length %d of quality score differs from length %d of nucleotides in sequence %s\n",
		      quality_length,fulllength1,acc);
	      abort();
	    } else {
	      queryseq1 = Shortread_new(acc,restofheader,filterp,Read1,long_read_1,fulllength1,
					Quality,long_quality,quality_length,barcode_length,endtrim_length,
					chop_poly_at_first_p,invert_first_p,/*copy_acc_p*/false,skipp);
	    }
	  } else {
	    /* Single-end without quality string */
	    queryseq1 = Shortread_new(acc,restofheader,filterp,Read1,long_read_1,fulllength1,
				      /*quality*/NULL,/*long_quality*/NULL,/*quality_length*/0,
				      barcode_length,endtrim_length,chop_poly_at_first_p,
				      invert_first_p,/*copy_acc_p*/false,skipp);
	  }
	  (*queryseq2) = (T) NULL;
	}
      }

      if (queryseq1 == (T) SKIPPED) {
	return (T) SKIPPED;
      } else if (queryseq1 != NULL && queryseq1->acc != NULL /*&& queryseq1->fulllength > 0*/) {
	debug(printf("Returning queryseq with contents %s\n",queryseq1->contents));
	return queryseq1;
      }
    }

  }
}
#endif


/* nextchar is either \0 or EOF */
static T
Shortread_read_interleaved_text (int *nextchar, T *queryseq2, FILE **input1, char *read_files_command,
				 char ***files, int *nfiles, bool single_cell_p, bool skipp) {
  T queryseq1;
  char *acc;
  char *long_read_1, *long_read_2, *long_quality, *line = NULL, *p, *q;
  int length, fulllength, quality_length;

  *nextchar = '\0';
  while (*input1 != NULL && (line = Getline(*input1)) == NULL) {
    if (read_files_command != NULL) {
      pclose(*input1);
    } else {
      fclose(*input1);
    }
    *input1 = NULL;
    
    if (*nfiles == 0) {
      *nextchar = EOF;
      return (T) NULL;
    } else {
      *input1 = Fopen_read_text(read_files_command,(*files)[0]);
      (*files) += 1;
      (*nfiles) -= 1;
    }
  }

  if (*input1 == NULL) {
    *nextchar = EOF;
    return (T) NULL;
  }

  p = line;
  while (*p != '\0' && *p != '\t') p++;
  length = (p - line)/sizeof(char);
  acc = (char *) MALLOC((length+1)*sizeof(char));
  strncpy(acc,line,length);
  acc[length] = '\0';

  if (*p == '\0') {
    /* Premature end of line in middle of acc: abort */
    fprintf(stderr,"Warning: Interleaved file ends in the middle of a read accession\n");
    FREE(acc);
    FREE(line);
    return (T) NULL;

  } else {
    q = ++p;
    while (*p != '\0' && *p != '\t') p++;
    fulllength = (p - q)/sizeof(char);
    long_read_1 = (char *) MALLOC((fulllength+1)*sizeof(char));
    strncpy(long_read_1,q,fulllength);
    long_read_1[fulllength] = '\0';

    if (*p == '\0') {
      /* Premature end of line in middle of read 1: Process the given read*/
      long_quality = (char *) NULL;
      quality_length = 0;
      
    } else {
      q = ++p;
      while (*p != '\0' && *p != '\t') p++;
      quality_length = (p - q)/sizeof(char);
      if (quality_length != fulllength) {
	long_quality = (char *) NULL;
	quality_length = 0;
      } else {
	long_quality = (char *) MALLOC((quality_length+1)*sizeof(char));
	strncpy(long_quality,q,quality_length);
	long_quality[quality_length] = '\0';
      }
    }

    queryseq1 = Shortread_new(acc,/*restofheader*/NULL,/*filterp*/false,/*short_sequence*/NULL,
			      long_read_1,fulllength,/*short_quality*/NULL,long_quality,quality_length,
			      /*barcode_length*/0,/*endtrim_length*/0,
			      chop_poly_at_first_p,invert_first_p,/*copy_acc_p*/false,skipp);
  }


  if (*p != '\t') {
    *queryseq2 = (T) NULL;
  } else {
    q = ++p;
    while (*p != '\0' && *p != '\t') p++;
    fulllength = (p - q)/sizeof(char);
    long_read_2 = (char *) MALLOC((fulllength+1)*sizeof(char));
    strncpy(long_read_2,q,fulllength);
    long_read_2[fulllength] = '\0';

    if (*p == '\0') {
      /* Premature end of line in middle of read 2: Process the given read*/
      long_quality = (char *) NULL;
      quality_length = 0;
    } else {
      q = ++p;
      while (*p != '\0' && *p != '\t') p++;
      quality_length = (p - q)/sizeof(char);
      if (quality_length != fulllength) {
	long_quality = (char *) NULL;
	quality_length = 0;
      } else {
	long_quality = (char *) MALLOC((quality_length+1)*sizeof(char));
	strncpy(long_quality,q,quality_length);
	long_quality[quality_length] = '\0';
      }
    }

    *queryseq2 = Shortread_new(/*acc*/NULL,/*restofheader*/NULL,/*filterp*/false,/*short_sequence*/NULL,
			       long_read_2,fulllength,/*short_quality*/NULL,long_quality,quality_length,
			       /*barcode_length*/0,/*endtrim_length*/0,
			       chop_poly_at_second_p,invert_second_p,/*copy_acc_p*/false,skipp);
  }

  FREE(line);
  return queryseq1;
}


#ifdef HAVE_ZLIB
static T
Shortread_read_interleaved_gzip (int *nextchar, T *queryseq2, gzFile *input1,
				 char ***files, int *nfiles, bool single_cell_p, bool skipp) {
  T queryseq1;
  char *acc;
  char *long_read_1, *long_read_2, *long_quality, *line, *p, *q;
  int length, fulllength, quality_length;

  *nextchar = '\0';
  while (*input1 != NULL && (line = Getline_gzip(*input1)) == NULL) {
    printf("Closing %p\n",*input1);
    gzclose(*input1);
    *input1 = NULL;
    
    if (*nfiles == 0) {
      *nextchar = EOF;
      return (T) NULL;
    } else {
      if ((*input1 = gzopen((*files)[0],"rb")) != NULL) {
#ifdef HAVE_ZLIB_GBUFFER
	gzbuffer(*input1,GZBUFFER_SIZE);
#endif
      }
      (*files) += 1;
      (*nfiles) -= 1;
    }
  }

  if (*input1 == NULL) {
    *nextchar = EOF;
    return (T) NULL;
  }

  p = line;
  while (*p != '\0' && *p != '\t') p++;
  length = (p - line)/sizeof(char);
  acc = (char *) MALLOC((length+1)*sizeof(char));
  strncpy(acc,line,length);
  acc[length] = '\0';

  if (*p == '\0') {
    /* Premature end of line in middle of acc: abort */
    fprintf(stderr,"Warning: Interleaved file ends in the middle of a read accession\n");
    FREE(acc);
    FREE(line);
    return (T) NULL;

  } else {
    q = ++p;
    while (*p != '\0' && *p != '\t') p++;
    fulllength = (p - q)/sizeof(char);
    long_read_1 = (char *) MALLOC((fulllength+1)*sizeof(char));
    strncpy(long_read_1,q,fulllength);
    long_read_1[fulllength] = '\0';

    if (*p == '\0') {
      /* Premature end of line in middle of read 1: Process the given read*/
      long_quality = (char *) NULL;
      quality_length = 0;
      
    } else {
      q = ++p;
      while (*p != '\0' && *p != '\t') p++;
      quality_length = (p - q)/sizeof(char);
      if (quality_length != fulllength) {
	long_quality = (char *) NULL;
	quality_length = 0;
      } else {
	long_quality = (char *) MALLOC((quality_length+1)*sizeof(char));
	strncpy(long_quality,q,quality_length);
	long_quality[quality_length] = '\0';
      }
    }

    queryseq1 = Shortread_new(acc,/*restofheader*/NULL,/*filterp*/false,/*short_sequence*/NULL,
			      long_read_1,fulllength,/*short_quality*/NULL,long_quality,quality_length,
			      /*barcode_length*/0,/*endtrim_length*/0,
			      chop_poly_at_first_p,invert_first_p,/*copy_acc_p*/false,skipp);
  }


  if (*p != '\t') {
    *queryseq2 = (T) NULL;
  } else {
    q = ++p;
    while (*p != '\0' && *p != '\t') p++;
    fulllength = (p - q)/sizeof(char);
    long_read_2 = (char *) MALLOC((fulllength+1)*sizeof(char));
    strncpy(long_read_2,q,fulllength);
    long_read_2[fulllength] = '\0';

    if (*p == '\0') {
      /* Premature end of line in middle of read 2: Process the given read*/
      long_quality = (char *) NULL;
      quality_length = 0;
    } else {
      q = ++p;
      while (*p != '\0' && *p != '\t') p++;
      quality_length = (p - q)/sizeof(char);
      if (quality_length != fulllength) {
	long_quality = (char *) NULL;
	quality_length = 0;
      } else {
	long_quality = (char *) MALLOC((quality_length+1)*sizeof(char));
	strncpy(long_quality,q,quality_length);
	long_quality[quality_length] = '\0';
      }
    }

    *queryseq2 = Shortread_new(/*acc*/NULL,/*restofheader*/NULL,/*filterp*/false,/*short_sequence*/NULL,
			       long_read_2,fulllength,/*short_quality*/NULL,long_quality,quality_length,
			       /*barcode_length*/0,/*endtrim_length*/0,
			       chop_poly_at_second_p,invert_second_p,/*copy_acc_p*/false,skipp);
  }

  FREE(line);
  return queryseq1;
}
#endif


#ifdef HAVE_BZLIB
static T
Shortread_read_interleaved_bzip2 (int *nextchar, T *queryseq2, Bzip2_T *input1,
				  char ***files, int *nfiles, bool single_cell_p, bool skipp) {
  T queryseq1;
  char *acc;
  char *long_read_1, *long_read_2, *long_quality, *line, *p, *q;
  int length, fulllength, quality_length;

  *nextchar = '\0';
  while (*input1 != NULL && (line = Getline_bzip2(*input1)) == NULL) {
    printf("Freeing %p\n",*input1);
    Bzip2_free(&(*input1));
    *input1 = NULL;
    
    if (*nfiles == 0) {
      *nextchar = EOF;
      return (T) NULL;
    } else {
      *input1 = Bzip2_new((*files)[0]);
      (*files) += 1;
      (*nfiles) -= 1;
    }
  }

  if (*input1 == NULL) {
    *nextchar = EOF;
    return (T) NULL;
  }

  p = line;
  while (*p != '\0' && *p != '\t') p++;
  length = (p - line)/sizeof(char);
  acc = (char *) MALLOC((length+1)*sizeof(char));
  strncpy(acc,line,length);
  acc[length] = '\0';

  if (*p == '\0') {
    /* Premature end of line in middle of acc: abort */
    fprintf(stderr,"Warning: Interleaved file ends in the middle of a read accession\n");
    FREE(acc);
    FREE(line);
    return (T) NULL;

  } else {
    q = ++p;
    while (*p != '\0' && *p != '\t') p++;
    fulllength = (p - q)/sizeof(char);
    long_read_1 = (char *) MALLOC((fulllength+1)*sizeof(char));
    strncpy(long_read_1,q,fulllength);
    long_read_1[fulllength] = '\0';

    if (*p == '\0') {
      /* Premature end of line in middle of read 1: Process the given read*/
      long_quality = (char *) NULL;
      quality_length = 0;
      
    } else {
      q = ++p;
      while (*p != '\0' && *p != '\t') p++;
      quality_length = (p - q)/sizeof(char);
      if (quality_length != fulllength) {
	long_quality = (char *) NULL;
	quality_length = 0;
      } else {
	long_quality = (char *) MALLOC((quality_length+1)*sizeof(char));
	strncpy(long_quality,q,quality_length);
	long_quality[quality_length] = '\0';
      }
    }

    queryseq1 = Shortread_new(acc,/*restofheader*/NULL,/*filterp*/false,/*short_sequence*/NULL,
			      long_read_1,fulllength,/*short_quality*/NULL,long_quality,quality_length,
			      /*barcode_length*/0,/*endtrim_length*/0,
			      chop_poly_at_first_p,invert_first_p,/*copy_acc_p*/false,skipp);
  }


  if (*p != '\t') {
    *queryseq2 = (T) NULL;
  } else {
    q = ++p;
    while (*p != '\0' && *p != '\t') p++;
    fulllength = (p - q)/sizeof(char);
    long_read_2 = (char *) MALLOC((fulllength+1)*sizeof(char));
    strncpy(long_read_2,q,fulllength);
    long_read_2[fulllength] = '\0';

    if (*p == '\0') {
      /* Premature end of line in middle of read 2: Process the given read*/
      long_quality = (char *) NULL;
      quality_length = 0;
    } else {
      q = ++p;
      while (*p != '\0' && *p != '\t') p++;
      quality_length = (p - q)/sizeof(char);
      if (quality_length != fulllength) {
	long_quality = (char *) NULL;
	quality_length = 0;
      } else {
	long_quality = (char *) MALLOC((quality_length+1)*sizeof(char));
	strncpy(long_quality,q,quality_length);
	long_quality[quality_length] = '\0';
      }
    }

    *queryseq2 = Shortread_new(/*acc*/NULL,/*restofheader*/NULL,/*filterp*/false,/*short_sequence*/NULL,
			       long_read_2,fulllength,/*short_quality*/NULL,long_quality,quality_length,
			       /*barcode_length*/0,/*endtrim_length*/0,
			       chop_poly_at_second_p,invert_second_p,/*copy_acc_p*/false,skipp);
  }

  FREE(line);
  return queryseq1;
}
#endif



T
Shortread_read_fastq_text (int *nextchar, int *nchars1, int *nchars2, T *queryseq2,
			   FILE **input1, FILE **input2, char *read_files_command,
			   char ***files, int *nfiles, bool single_cell_p, bool skipp) {
  T queryseq1;
  int nextchar2 = '\0';		/* Can be anything but EOF */
  char *acc, *restofheader;
  char *long_read_1, *long_read_2, *long_quality;
  int fulllength, quality_length;
  bool filterp;

  while (1) {
    queryseq1 = *queryseq2 = (T) NULL;
    if (*input1 == NULL || *nextchar == EOF) { /* was feof(input1) */
      if (*input1 != NULL) {
	debugf(fprintf(stderr,"Master closing input 1 using fclose\n"));
	if (read_files_command != NULL) {
	  pclose(*input1);
	} else {
	  fclose(*input1);
	}
	*input1 = NULL;
      }
      if (*input2 != NULL) {
	debugf(fprintf(stderr,"Master closing input 2 using fclose\n"));
	if (read_files_command != NULL) {
	  pclose(*input2);
	} else {
	  fclose(*input2);
	}
	*input2 = NULL;
      }

      if (*nfiles == 0) {
	*nextchar = EOF;
	return (T) NULL;

      } else if (*nfiles == 1 || force_single_end_p == true) {
	if ((*input1 = Fopen_read_text(read_files_command,(*files)[0])) == NULL) {
	  (*files) += 1;
	  (*nfiles) -= 1;
	  *nextchar = EOF;
	  return (T) NULL;
	} else {
	  debugf(fprintf(stderr,"Master opening input file 1\n"));
	  *input2 = NULL;
	  (*files) += 1;
	  (*nfiles) -= 1;
	  *nextchar = '\0';	/* Was nextchar2 = '\0', which is incorrect */
	}

      } else {
	while (*nfiles > 0 &&
	       ((*input1 = Fopen_read_text(read_files_command,(*files)[0])) == NULL ||
		(*input2 = Fopen_read_text(read_files_command,(*files)[1])) == NULL)) {
	  fprintf(stderr,"Can't open file %s or %s => skipping both.\n",
		  (*files)[0],(*files)[1]);
	  (*files) += 2;
	  (*nfiles) -= 2;
	}
	if (*input1 == NULL) {
	  *nextchar = EOF;
	  return (T) NULL;
	} else {
	  debugf(fprintf(stderr,"Master opening input files 1 and 2\n"));
	  (*files) += 2;
	  (*nfiles) -= 2;
	  *nextchar = '\0';
	  if ((nextchar2 = Shortread_input_init(&(*nchars2),*input2)) == EOF) {
	    return (T) NULL;
	  }
	}
      }
    }

    if (*nextchar == '\0') {
      if ((*nextchar = Shortread_input_init(&(*nchars1),*input1)) == EOF) {
	return (T) NULL;
      }
    }

    debug(printf("** Getting header for fastq text\n"));
    if ((acc = input_header_fastq(&(*nchars1),&filterp,&restofheader,*nextchar,*input1,skipp)) == NULL) {
      /* fprintf(stderr,"No header\n"); */
      /* File ends after >.  Don't process, but loop again */
      *nextchar = EOF;
    } else {
      *nextchar = fgetc(*input1);
      *nchars1 += 1;
      if ((fulllength = input_oneline(&(*nextchar),&(*nchars1),&long_read_1,&(Read1[0]),*input1,
				      /*possible_fasta_header_p*/true)) == 0) {
	FREE_IN(acc);
	FREE_IN(restofheader);
	/* fprintf(stderr,"length is zero\n"); */
	/* No sequence1.  Don't process, but loop again */
	/* *nextchar = EOF; */

      } else if (*nextchar != '+') {
	/* No quality */
	queryseq1 = Shortread_new(acc,restofheader,filterp,Read1,long_read_1,fulllength,
				  /*quality*/NULL,/*long_quality*/NULL,/*quality_length*/0,
				  barcode_length,endtrim_length,chop_poly_at_first_p,
				  invert_first_p,/*copy_acc_p*/false,skipp);
      } else {
	skip_header(&(*nchars1),*input1,*nextchar);
	*nextchar = fgetc(*input1);
	*nchars1 += 1;
	quality_length = input_oneline(&(*nextchar),&(*nchars1),&long_quality,&(Quality[0]),*input1,
				       /*possible_fasta_header_p*/false);
	if (quality_length != fulllength) {
	  fprintf(stderr,"Length %d of quality score differs from length %d of nucleotides in sequence %s\n",
		  quality_length,fulllength,acc);
	  abort();
	} else {
	  /* Has quality */
	  queryseq1 = Shortread_new(acc,restofheader,filterp,Read1,long_read_1,fulllength,
				    Quality,long_quality,quality_length,barcode_length,endtrim_length,
				    chop_poly_at_first_p,invert_first_p,/*copy_acc_p*/false,skipp);
	}
      }
    }

    if (acc == NULL || fulllength == 0) {
      /* Skip */
    } else if (*input2 == NULL) {
      *queryseq2 = (T) NULL;
    } else {
      if ((acc = input_header_fastq(&(*nchars2),&filterp,&restofheader,nextchar2,*input2,skipp)) == NULL) {
	/* fprintf(stderr,"No header\n"); */
	/* File ends after >.  Don't process, but loop again */
	nextchar2 = EOF;
      } else {
	if (skipp == true) {
	  /* Do not check endings */
	} else if (allow_paired_end_mismatch_p == true) {
	  /* Do not strip endings, and keep second accession */
	  FREE_IN(restofheader);
	} else {
	  strip_illumina_acc_ending(queryseq1->acc,acc);
	  if (strcmp(queryseq1->acc,acc)) {
	    fprintf(stderr,"Paired-end accessions %s and %s do not match\n",queryseq1->acc,acc);
	    exit(9);
	  } else {
	    FREE_IN(acc);
	    FREE_IN(restofheader);
	    acc = (char *) NULL;
	  }
	}
	nextchar2 = fgetc(*input2);
	*nchars2 += 1;
	if ((fulllength = input_oneline(&nextchar2,&(*nchars2),&long_read_2,&(Read2[0]),*input2,
					/*possible_fasta_header_p*/true)) == 0) {
	  FREE_IN(acc);
	  FREE_IN(restofheader);
	  /* fprintf(stderr,"length is zero\n"); */
	  /* No sequence2.  Don't process, but loop again */
	  /* nextchar2 = EOF; */

	} else if (nextchar2 != '+') {
	  /* No quality */
	  (*queryseq2) = Shortread_new(acc,/*restofheader*/NULL,filterp,Read2,long_read_2,fulllength,
				       /*quality*/NULL,/*long_quality*/NULL,/*quality_length*/0,
				       barcode_length,endtrim_length,chop_poly_at_second_p,
				       invert_second_p,/*copy_acc_p*/false,skipp);
	} else {
	  skip_header(&(*nchars2),*input2,nextchar2);
	  nextchar2 = fgetc(*input2);
	  *nchars2 += 1;
	  quality_length = input_oneline(&nextchar2,&(*nchars2),&long_quality,&(Quality[0]),*input2,
					 /*possible_fasta_header_p*/false);
	  if (quality_length != fulllength) {
	    fprintf(stderr,"Length %d of quality score differs from length %d of nucleotides in sequence %s\n",
		    quality_length,fulllength,acc);
	    abort();
	  } else {
	    /* Has quality */
	    (*queryseq2) = Shortread_new(acc,/*restofheader*/NULL,filterp,Read2,long_read_2,fulllength,
					 Quality,long_quality,quality_length,barcode_length,endtrim_length,
					 chop_poly_at_second_p,invert_second_p,/*copy_acc_p*/false,skipp);

	  }
	}
      }
    }

    if (queryseq1 == (T) SKIPPED) {
      return (T) SKIPPED;
    } else if (queryseq1 != NULL && queryseq1->acc != NULL /*&& queryseq1->fulllength > 0*/) {
      return queryseq1;
    }
  }
}


#ifdef HAVE_ZLIB
T
Shortread_read_fastq_gzip (int *nextchar, T *queryseq2,
			   gzFile *input1, gzFile *input2,
			   char ***files, int *nfiles, bool single_cell_p, bool skipp) {
  T queryseq1;
  int nextchar2 = '\0';		/* Can be anything but EOF */
  char *acc, *restofheader;
  char *long_read_1, *long_read_2, *long_quality;
  int fulllength, quality_length;
  bool filterp;

  while (1) {
    queryseq1 = *queryseq2 = (T) NULL;
    if (*input1 == NULL || *nextchar == EOF) { /* was gzeof(*input1) */
      if (*input1 != NULL) {
	debugf(fprintf(stderr,"Master closing input 1 using gzclose\n"));
	gzclose(*input1);
	*input1 = NULL;
      }
      if (*input2 != NULL) {
	debugf(fprintf(stderr,"Master closing input 2 using gzclose\n"));
	gzclose(*input2);
	*input2 = NULL;
      }

      if (*nfiles == 0) {
	*nextchar = EOF;
	return (T) NULL;

      } else if (*nfiles == 1 || force_single_end_p == true) {
	if ((*input1 = gzopen((*files)[0],"rb")) == NULL) {
	  fprintf(stderr,"Cannot open gzipped file %s => skipping.\n",(*files)[0]);
	  (*files) += 1;
	  (*nfiles) -= 1;
	  *nextchar = EOF;
	  return (T) NULL;
	} else {
	  debugf(fprintf(stderr,"Master opening input file 1\n"));
#ifdef HAVE_ZLIB_GZBUFFER
	  gzbuffer(*input1,GZBUFFER_SIZE);
#endif
	  *input2 = NULL;
	  (*files) += 1;
	  (*nfiles) -= 1;
	  *nextchar = '\0';	/* Was nextchar2 = '\0', which is incorrect */
	}
	
      } else {
	while (*nfiles > 0 &&
	       ((*input1 = gzopen((*files)[0],"rb")) == NULL ||
		(*input2 = gzopen((*files)[1],"rb")) == NULL)) {
	  fprintf(stderr,"Can't open file %s or %s => skipping both.\n",
		  (*files)[0],(*files)[1]);
	  (*files) += 2;
	  (*nfiles) -= 2;
	}
	if (*input1 == NULL) {
	  *nextchar = EOF;
	  return (T) NULL;
	} else {
	  debugf(fprintf(stderr,"Master opening input files 1 and 2\n"));
#ifdef HAVE_ZLIB_GZBUFFER
	  gzbuffer(*input1,GZBUFFER_SIZE);
	  gzbuffer(*input2,GZBUFFER_SIZE);
#endif
	  (*files) += 2;
	  (*nfiles) -= 2;
	  *nextchar = '\0';
	  if ((nextchar2 = Shortread_input_init_gzip(*input2)) == EOF) {
	    return (T) NULL;
	  }
	}
      }
    }

    if (*nextchar == '\0') {
      if ((*nextchar = Shortread_input_init_gzip(*input1)) == EOF) {
	return (T) NULL;
      }
    }

    debug(printf("** Getting header for fastq gzip\n"));
    if ((acc = input_header_fastq_gzip(&filterp,&restofheader,*nextchar,
				       *input1,skipp)) == NULL) {
      /* fprintf(stderr,"No header\n"); */
      /* File ends after >.  Don't process. */
      *nextchar = EOF;
    } else {
      *nextchar = gzgetc(*input1);
      if ((fulllength = input_oneline_gzip(&(*nextchar),&long_read_1,&(Read1[0]),
					   *input1,/*possible_fasta_header_p*/true)) == 0) {
	FREE_IN(acc);
	FREE_IN(restofheader);
	/* fprintf(stderr,"length is zero\n"); */
	/* No sequence1.  Don't process, but loop again */
	/* *nextchar = EOF; */

      } else if (*nextchar != '+') {
	/* No quality */
	queryseq1 = Shortread_new(acc,restofheader,filterp,Read1,long_read_1,fulllength,
				  /*quality*/NULL,/*long_quality*/NULL,/*quality_length*/0,
				  barcode_length,endtrim_length,chop_poly_at_first_p,
				  invert_first_p,/*copy_acc_p*/false,skipp);
      } else {
	skip_header_gzip(*input1,*nextchar);
	*nextchar = gzgetc(*input1);
	quality_length = input_oneline_gzip(&(*nextchar),&long_quality,&(Quality[0]),
					    *input1,/*possible_fasta_header_p*/false);
	if (quality_length != fulllength) {
	  fprintf(stderr,"Length %d of quality score differs from length %d of nucleotides in sequence %s\n",
		  quality_length,fulllength,acc);
	  abort();
	} else {
	  /* Has quality */
	  queryseq1 = Shortread_new(acc,restofheader,filterp,Read1,long_read_1,fulllength,
				    Quality,long_quality,quality_length,barcode_length,endtrim_length,
				    chop_poly_at_first_p,invert_first_p,/*copy_acc_p*/false,skipp);
	}
      }
    }

    if (acc == NULL || fulllength == 0) {
      /* Skip */
    } else if (*input2 == NULL) {
      *queryseq2 = (T) NULL;
    } else {
      if ((acc = input_header_fastq_gzip(&filterp,&restofheader,nextchar2,
					 *input2,skipp)) == NULL) {
	/* fprintf(stderr,"No header\n"); */
	/* File ends after >.  Don't process, but loop again */
	nextchar2 = EOF;
      } else {
	if (skipp == true) {
	  /* Do not check endings */
	} else if (allow_paired_end_mismatch_p == true) {
	  /* Do not strip endings, and keep second accession */
	  FREE_IN(restofheader);
	} else {
	  strip_illumina_acc_ending(queryseq1->acc,acc);
	  if (strcmp(queryseq1->acc,acc)) {
	    fprintf(stderr,"Paired-end accessions %s and %s do not match\n",queryseq1->acc,acc);
	    exit(9);
	  } else {
	    FREE_IN(acc);
	    FREE_IN(restofheader);
	    acc = (char *) NULL;
	  }
	}
	nextchar2 = gzgetc(*input2);
	if ((fulllength = input_oneline_gzip(&nextchar2,&long_read_2,&(Read2[0]),
					     *input2,/*possible_fasta_header_p*/true)) == 0) {
	  FREE_IN(acc);
	  FREE_IN(restofheader);
	  /* fprintf(stderr,"length is zero\n"); */
	  /* No sequence2.  Don't process, but loop again */
	  /* nextchar2 = EOF; */

	} else if (nextchar2 != '+') {
	  /* No quality */
	  (*queryseq2) = Shortread_new(acc,/*restofheader*/NULL,filterp,Read2,long_read_2,fulllength,
				       /*quality*/NULL,/*long_quality*/NULL,/*quality_length*/0,
				       barcode_length,endtrim_length,chop_poly_at_second_p,
				       invert_second_p,/*copy_acc_p*/false,skipp);
	} else {
	  skip_header_gzip(*input2,nextchar2);
	  nextchar2 = gzgetc(*input2);
	  quality_length = input_oneline_gzip(&nextchar2,&long_quality,&(Quality[0]),
					      *input2,/*possible_fasta_header_p*/false);
	  if (quality_length != fulllength) {
	    fprintf(stderr,"Length %d of quality score differs from length %d of nucleotides in sequence %s\n",
		    quality_length,fulllength,acc);
	    abort();
	  } else {
	    /* Has quality */
	    (*queryseq2) = Shortread_new(acc,/*restofheader*/NULL,filterp,Read2,long_read_2,fulllength,
					 Quality,long_quality,quality_length,barcode_length,endtrim_length,
					 chop_poly_at_second_p,invert_second_p,/*copy_acc_p*/false,skipp);
	  }
	}
      }
    }

    if (queryseq1 == (T) SKIPPED) {
      return (T) SKIPPED;
    } else if (queryseq1 != NULL && queryseq1->acc != NULL /*&& queryseq1->fulllength > 0*/) {
      return queryseq1;
    }
  }
}
#endif


#ifdef HAVE_BZLIB
T
Shortread_read_fastq_bzip2 (int *nextchar, T *queryseq2,
			    Bzip2_T *input1, Bzip2_T *input2,
			    char ***files, int *nfiles, bool single_cell_p, bool skipp) {
  T queryseq1;
  int nextchar2 = '\0';		/* Can be anything but EOF */
  char *acc, *restofheader;
  char *long_read_1, *long_read_2, *long_quality;
  int fulllength, quality_length;
  bool filterp;

  while (1) {
    queryseq1 = *queryseq2 = (T) NULL;
    if (*input1 == NULL || *nextchar == EOF) { /* Was bzeof(*input1) */
      if (*input1 != NULL) {
	debugf(fprintf(stderr,"Master closing input 1 using Bzip2_free\n"));
	Bzip2_free(&(*input1));
	*input1 = NULL;
      }
      if (*input2 != NULL) {
	debugf(fprintf(stderr,"Master closing input 2 using Bzip2_free\n"));
	Bzip2_free(&(*input2));
	*input2 = NULL;
      }

      if (*nfiles == 0) {
	*nextchar = EOF;
	return (T) NULL;

      } else if (*nfiles == 1 || force_single_end_p == true) {
	if ((*input1 = Bzip2_new((*files)[0])) == NULL) {
	  fprintf(stderr,"Can't open file %s => skipping.\n",(*files)[0]);
	  (*files) += 1;
	  (*nfiles) -= 1;
	  *nextchar = EOF;
	  return (T) NULL;
	} else {
	  debugf(fprintf(stderr,"Master opening input file 1\n"));
	  *input2 = NULL;
	  (*files) += 1;
	  (*nfiles) -= 1;
	  *nextchar = '\0';	/* Was nextchar2 = '\0', which is incorrect */
	}

      } else {
	while (*nfiles > 0 &&
	       ((*input1 = Bzip2_new((*files)[0])) == NULL ||
		(*input2 = Bzip2_new((*files)[1])) == NULL)) {
	  fprintf(stderr,"Can't open file %s or %s => skipping both.\n",
		  (*files)[0],(*files)[1]);
	  (*files) += 2;
	  (*nfiles) -= 2;
	}
	if (*input1 == NULL) {
	  *nextchar = EOF;
	  return (T) NULL;
	} else {
	  debugf(fprintf(stderr,"Master opening input files 1 and 2\n"));
	  (*files) += 2;
	  (*nfiles) -= 2;
	  *nextchar = '\0';
	  if ((nextchar2 = Shortread_input_init_bzip2(*input2)) == EOF) {
	    return (T) NULL;
	  }
	}
      }
    }

    if (*nextchar == '\0') {
      if ((*nextchar = Shortread_input_init_bzip2(*input1)) == EOF) {
	return (T) NULL;
      }
    }

    debug(printf("** Getting header for fastq bzip2\n"));
    if ((acc = input_header_fastq_bzip2(&filterp,&restofheader,*nextchar,
					*input1,skipp)) == NULL) {
      /* fprintf(stderr,"No header\n"); */
      /* File ends after >.  Don't process. */
      *nextchar = EOF;
    } else {
      *nextchar = bzgetc(*input1);
      if ((fulllength = input_oneline_bzip2(&(*nextchar),&long_read_1,&(Read1[0]),
					    *input1,/*possible_fasta_header_p*/true)) == 0) {
	FREE_IN(acc);
	FREE_IN(restofheader);
	/* fprintf(stderr,"length is zero\n"); */
	/* No sequence1.  Don't process, but loop again */
	/* *nextchar = EOF; */

      } else if (*nextchar != '+') {
	/* No quality */
	queryseq1 = Shortread_new(acc,restofheader,filterp,Read1,long_read_1,fulllength,
				  /*quality*/NULL,/*long_quality*/NULL,/*quality_length*/0,
				  barcode_length,endtrim_length,chop_poly_at_first_p,
				  invert_first_p,/*copy_acc_p*/false,skipp);
      } else {
	skip_header_bzip2(*input1,*nextchar);
	*nextchar = bzgetc(*input1);
	quality_length = input_oneline_bzip2(&(*nextchar),&long_quality,&(Quality[0]),
					     *input1,/*possible_fasta_header_p*/false);
	if (quality_length != fulllength) {
	  fprintf(stderr,"Length %d of quality score differs from length %d of nucleotides in sequence %s\n",
		  quality_length,fulllength,acc);
	  abort();
	} else {
	  /* Has quality */
	  queryseq1 = Shortread_new(acc,restofheader,filterp,Read1,long_read_1,fulllength,
				    Quality,long_quality,quality_length,barcode_length,endtrim_length,
				    chop_poly_at_first_p,invert_first_p,/*copy_acc_p*/false,skipp);
	}
      }
    }

    if (acc == NULL || fulllength == 0) {
      /* Skip */
    } else if (*input2 == NULL) {
      *queryseq2 = (T) NULL;
    } else {
      if ((acc = input_header_fastq_bzip2(&filterp,&restofheader,nextchar2,
					  *input2,skipp)) == NULL) {
	/* fprintf(stderr,"No header\n"); */
	/* File ends after >.  Don't process, but loop again */
	nextchar2 = EOF;
      } else {
	if (skipp == true) {
	  /* Do not check endings */
	} else if (allow_paired_end_mismatch_p == true) {
	  /* Do not strip endings, and keep second accession */
	  FREE_IN(restofheader);
	} else {
	  strip_illumina_acc_ending(queryseq1->acc,acc);
	  if (strcmp(queryseq1->acc,acc)) {
	    fprintf(stderr,"Paired-end accessions %s and %s do not match\n",queryseq1->acc,acc);
	    exit(9);
	  } else {
	    FREE_IN(acc);
	    FREE_IN(restofheader);
	    acc = (char *) NULL;
	  }
	}
	nextchar2 = bzgetc(*input2);
	if ((fulllength = input_oneline_bzip2(&nextchar2,&long_read_2,&(Read2[0]),
					      *input2,/*possible_fasta_header_p*/true)) == 0) {
	  FREE_IN(acc);
	  FREE_IN(restofheader);
	  /* fprintf(stderr,"length is zero\n"); */
	  /* No sequence2.  Don't process, but loop again */
	  /* nextchar2 = EOF; */

	} else if (nextchar2 != '+') {
	  /* No quality */
	  (*queryseq2) = Shortread_new(acc,/*restofheader*/NULL,filterp,Read2,long_read_2,fulllength,
				       /*quality*/NULL,/*long_quality*/NULL,/*quality_length*/0,
				       barcode_length,endtrim_length,chop_poly_at_second_p,
				       invert_second_p,/*copy_acc_p*/false,skipp);
	} else {
	  skip_header_bzip2(*input2,nextchar2);
	  nextchar2 = bzgetc(*input2);
	  quality_length = input_oneline_bzip2(&nextchar2,&long_quality,&(Quality[0]),
					       *input2,/*possible_fasta_header_p*/false);
	  if (quality_length != fulllength) {
	    fprintf(stderr,"Length %d of quality score differs from length %d of nucleotides in sequence %s\n",
		    quality_length,fulllength,acc);
	    abort();
	  } else {
	    /* Has quality */
	    (*queryseq2) = Shortread_new(acc,/*restofheader*/NULL,filterp,Read2,long_read_2,fulllength,
					 Quality,long_quality,quality_length,barcode_length,endtrim_length,
					 chop_poly_at_second_p,invert_second_p,/*copy_acc_p*/false,skipp);
	  }
	}
      }
    }

    if (queryseq1 == (T) SKIPPED) {
      return (T) SKIPPED;
    } else if (queryseq1 != NULL && queryseq1->acc != NULL /*&& queryseq1->fulllength > 0*/) {
      return queryseq1;
    }
  }
}
#endif


T
Shortread_read (int *nextchar, int *nchars1, int *nchars2, T *queryseq2,
		FILE **input1, FILE **input2,
#ifdef HAVE_ZLIB
		gzFile *gzipped1, gzFile *gzipped2,
#endif
#ifdef HAVE_BZLIB
		Bzip2_T *bzipped1, Bzip2_T *bzipped2,
#endif
		bool interleavedp,
		char *read_files_command, char ***files, int *nfiles,
		bool single_cell_p, bool skipp) {
#ifdef DEBUG
  T queryseq1;
#endif


  if (interleavedp) {
    /* Interleaved format */
#ifdef HAVE_ZLIB    
    if (*gzipped1 != NULL) {
      return Shortread_read_interleaved_gzip(&(*nextchar),&(*queryseq2),
					     &(*gzipped1),&(*files),&(*nfiles),single_cell_p,skipp);
    }
#endif

#ifdef HAVE_BZLIB
    if (*bzipped1 != NULL) {
      return Shortread_read_interleaved_bzip2(&(*nextchar),&(*queryseq2),
					      &(*bzipped1),&(*files),&(*nfiles),single_cell_p,skipp);
    }
#endif

    return Shortread_read_interleaved_text(&(*nextchar),&(*queryseq2),&(*input1),
					   read_files_command,&(*files),&(*nfiles),single_cell_p,skipp);


  } else if (fastq_format_p) {
#ifdef HAVE_ZLIB    
    if (*gzipped1 != NULL) {
      return Shortread_read_fastq_gzip(&(*nextchar),&(*queryseq2),
				       &(*gzipped1),&(*gzipped2),&(*files),&(*nfiles),single_cell_p,skipp);
    }
#endif

#ifdef HAVE_BZLIB
    if (*bzipped1 != NULL) {
      return Shortread_read_fastq_bzip2(&(*nextchar),&(*queryseq2),
					&(*bzipped1),&(*bzipped2),&(*files),&(*nfiles),single_cell_p,skipp);
    }
#endif

#ifdef DEBUG
    queryseq1 = Shortread_read_fastq_text(&(*nextchar),&(*nchars1),&(*nchars2),&(*queryseq2),
					  &(*input1),&(*input2),read_files_command,&(*files),&(*nfiles),
					  single_cell_p,skipp);
    printf("nchars1 %d, nchars2 %d, nextchar %c\n",*nchars1,*nchars2,*nextchar);
    return queryseq1;
#else
    return Shortread_read_fastq_text(&(*nextchar),&(*nchars1),&(*nchars2),&(*queryseq2),
				     &(*input1),&(*input2),read_files_command,&(*files),&(*nfiles),
				     single_cell_p,skipp);
#endif

  } else {
    /* FASTA input */
#ifdef HAVE_ZLIB    
    if (*gzipped1 != NULL) {
      return Shortread_read_fasta_gzip(&(*nextchar),&(*queryseq2),
				       &(*gzipped1),&(*gzipped2),&(*files),&(*nfiles),
				       single_cell_p,skipp);
    }
#endif

#ifdef HAVE_BZLIB
    if (*bzipped1 != NULL) {
      return Shortread_read_fasta_bzip2(&(*nextchar),&(*queryseq2),
					&(*bzipped1),&(*bzipped2),&(*files),&(*nfiles),
					single_cell_p,skipp);
    }
#endif

#ifdef DEBUG
    queryseq1 = Shortread_read_fasta_text(&(*nextchar),&(*nchars1),&(*nchars2),&(*queryseq2),
					  &(*input1),&(*input2),read_files_command,&(*files),&(*nfiles),
					  single_cell_p,skipp);
    printf("nchars1 %d, nchars2 %d, nextchar %c\n",*nchars1,*nchars2,*nextchar);
    return queryseq1;
#else
    return Shortread_read_fasta_text(&(*nextchar),&(*nchars1),&(*nchars2),&(*queryseq2),
				     &(*input1),&(*input2),read_files_command,&(*files),&(*nfiles),
				     single_cell_p,skipp);
#endif
  }
}


/* Calling procedure needs to print the initial ">", if desired */
void
Shortread_print_header (Filestring_T fp, T queryseq1, T queryseq2) {

  if (queryseq2 == NULL || queryseq2->acc == NULL) {
    FPRINTF(fp,"%s",queryseq1->acc);
  } else {
    FPRINTF(fp,"%s,%s",queryseq1->acc,queryseq2->acc);
  }

  if (queryseq1->restofheader == NULL || queryseq1->restofheader[0] == '\0') {
    /* Don't print restofheader */
  } else {
    FPRINTF(fp," %s",queryseq1->restofheader);
  }

  FPRINTF(fp,"\n");

  return;
}


static void
stderr_header (T queryseq1, T queryseq2) {

  if (queryseq2 == NULL || queryseq2->acc == NULL) {
    fprintf(stderr,"%s",queryseq1->acc);
  } else {
    fprintf(stderr,"%s,%s",queryseq1->acc,queryseq2->acc);
  }

  if (queryseq1->restofheader == NULL || queryseq1->restofheader[0] == '\0') {
    /* Don't print restofheader */
  } else {
    fprintf(stderr," %s",queryseq1->restofheader);
  }

  fprintf(stderr,"\n");

  return;
}

static void
stderr_oneline (T this) {
  int i = 0;

  for (i = 0; i < this->left_choplength; i++) {
    fprintf(stderr,"%c",this->left_chop[i]);
  }
  for (i = 0; i < this->fulllength; i++) {
    fprintf(stderr,"%c",this->contents[i]);
  }
  for (i = 0; i < this->right_choplength; i++) {
    fprintf(stderr,"%c",this->right_chop[i]);
  }

  return;
}

static void
stderr_oneline_revcomp (T this) {
  int i = 0;

  for (i = this->right_choplength-1; i >= 0; --i) {
    fprintf(stderr,"%c",complCode[(int) this->right_chop[i]]);
  }
  for (i = this->fulllength-1; i >= 0; --i) {
    fprintf(stderr,"%c",complCode[(int) this->contents[i]]);
  }
  for (i = this->left_choplength-1; i >= 0; --i) {
    fprintf(stderr,"%c",complCode[(int) this->left_chop[i]]);
  }

  return;
}



void
Shortread_stderr_query_singleend_fasta (T queryseq, T headerseq) {
  fprintf(stderr,">");
  stderr_header(headerseq,/*queryseq2*/NULL);
  /* fprintf(stderr,"\n"); -- included in header */
  stderr_oneline(queryseq);
  fprintf(stderr,"\n");

  return;
}

void
Shortread_stderr_query_pairedend_fasta (T queryseq1, T queryseq2,
					bool invert_first_p, bool invert_second_p) {
  fprintf(stderr,">");
  stderr_header(queryseq1,queryseq2);
  /* fprintf(stderr,"\n"); -- included in header */

  if (invert_first_p == true) {
    stderr_oneline_revcomp(queryseq1);
    fprintf(stderr,"\n");
  } else {
    stderr_oneline(queryseq1);
    fprintf(stderr,"\n");
  }

  if (invert_second_p == true) {
    stderr_oneline_revcomp(queryseq2);
    fprintf(stderr,"\n");
  } else {
    stderr_oneline(queryseq2);
    fprintf(stderr,"\n");
  }

  return;
}


void
Shortread_print_query_singleend (Filestring_T fp, T queryseq, T headerseq) {
  if (fastq_format_p == true) {
    /* FASTQ format */
    FPRINTF(fp,"@");
    Shortread_print_header(fp,headerseq,/*queryseq2*/NULL);
    /* FPRINTF(fp,"\n"); -- included in header */
    Shortread_print_oneline(fp,queryseq);
    FPRINTF(fp,"\n");
    
    if (queryseq->quality != NULL) {
      FPRINTF(fp,"+\n");
      Shortread_print_quality(fp,queryseq,/*hardclip_low*/0,/*hardclip_high*/0,
			      /*shift*/0,/*choppedp*/false);
      FPRINTF(fp,"\n");
    }

  } else {
    /* FASTA format */
    FPRINTF(fp,">");
    Shortread_print_header(fp,headerseq,/*queryseq2*/NULL);
    Shortread_print_oneline(fp,queryseq);
    FPRINTF(fp,"\n");
  }

  return;
}

void
Shortread_print_query_pairedend (Filestring_T fp1, Filestring_T fp2, T queryseq1, T queryseq2) {
  if (fastq_format_p == true) {
    /* FASTQ format */

    /* First end */
    if (queryseq2->acc == NULL) {
      FPRINTF(fp1,"@%s/1\n",queryseq1->acc);
    } else {
      FPRINTF(fp2,"@%s\n",queryseq1->acc); /* Allowing paired-end name mismatch */
    }

    if (invert_first_p == true) {
      Shortread_print_oneline_revcomp(fp1,queryseq1);
      FPRINTF(fp1,"\n");
      if (queryseq1->quality != NULL) {
	FPRINTF(fp1,"+\n");
	Shortread_print_quality_revcomp(fp1,queryseq1,/*hardclip_low*/0,/*hardclip_high*/0,
					/*shift*/0,/*choppedp*/false);
	FPRINTF(fp1,"\n");
      }
    } else {
      Shortread_print_oneline(fp1,queryseq1);
      FPRINTF(fp1,"\n");
      if (queryseq1->quality != NULL) {
	FPRINTF(fp1,"+\n");
	Shortread_print_quality(fp1,queryseq1,/*hardclip_low*/0,/*hardclip_high*/0,
				/*shift*/0,/*choppedp*/false);
	FPRINTF(fp1,"\n");
      }
    }

    /* Second end */
    if (queryseq2->acc == NULL) {
      FPRINTF(fp2,"@%s/2\n",queryseq1->acc); /* Acc stored only for first end, not second end */
    } else {
      FPRINTF(fp2,"@%s\n",queryseq2->acc); /* Allowing paired-end name mismatch */
    }

    if (invert_second_p == true) {
      Shortread_print_oneline_revcomp(fp2,queryseq2);
      FPRINTF(fp2,"\n");
      if (queryseq2->quality != NULL) {
	FPRINTF(fp2,"+\n");
	Shortread_print_quality_revcomp(fp2,queryseq2,/*hardclip_low*/0,/*hardclip_high*/0,
					/*shift*/0,/*chopped*/false);
	FPRINTF(fp2,"\n");
      }
    } else {
      Shortread_print_oneline(fp2,queryseq2);
      FPRINTF(fp2,"\n");
      if (queryseq2->quality != NULL) {
	FPRINTF(fp2,"+\n");
	Shortread_print_quality(fp2,queryseq2,/*hardclip_low*/0,/*hardclip_high*/0,
				/*shift*/0,/*choppedp*/false);
	FPRINTF(fp2,"\n");
      }
    }

  } else {
    /* FASTA format */
    FPRINTF(fp1,">");
    Shortread_print_header(fp1,queryseq1,queryseq2);
    
    if (invert_first_p == true) {
      Shortread_print_oneline_revcomp(fp1,queryseq1);
      FPRINTF(fp1,"\n");
    } else {
      Shortread_print_oneline(fp1,queryseq1);
      FPRINTF(fp1,"\n");
    }

    if (invert_second_p == true) {
      Shortread_print_oneline_revcomp(fp1,queryseq2);
      FPRINTF(fp1,"\n");
    } else {
      Shortread_print_oneline(fp1,queryseq2);
      FPRINTF(fp1,"\n");
    }
  }

  return;
}


void
Shortread_print_oneline (Filestring_T fp, T this) {
  int i = 0;

  for (i = 0; i < this->left_choplength; i++) {
    FPRINTF(fp,"%c",this->left_chop[i]);
  }
  for (i = 0; i < this->fulllength; i++) {
    FPRINTF(fp,"%c",this->contents[i]);
  }
  for (i = 0; i < this->right_choplength; i++) {
    FPRINTF(fp,"%c",this->right_chop[i]);
  }

  return;
}

void
Shortread_print_oneline_revcomp (Filestring_T fp, T this) {

  FPRINTF(fp,"%.*R",this->right_choplength,this->right_chop);

#if 0
  for (i = this->fulllength-1; i >= 0; --i) {
    FPRINTF(fp,"%c",complCode[(int) this->contents[i]]);
  }
#else
  FPRINTF(fp,"%.*R",this->fulllength,this->contents);
#endif

#if 0
  for (i = this->choplength-1; i >= 0; --i) {
    FPRINTF(fp,"%c",complCode[(int) this->chop[i]]);
  }
#else
  FPRINTF(fp,"%.*R",this->left_choplength,this->left_chop);
#endif

  return;
}


void
Shortread_print_chopped_sam (Filestring_T fp, T this, int hardclip_low, int hardclip_high) {
#ifdef PRINT_INDIVIDUAL_CHARS
  int i;
#endif

#ifdef PRINT_INDIVIDUAL_CHARS
  FPRINTF(fp,"\t");
  for (i = hardclip_low; i < this->fulllength - hardclip_high; i++) {
    FPRINTF(fp,"%c",this->contents[i]);
  }
  FPRINTF(fp,"\t");
#else
  FPRINTF(fp,"\t%.*s\t",this->fulllength - hardclip_high - hardclip_low,&(this->contents[hardclip_low]));
#endif

  return;
}

void
Shortread_print_chopped_revcomp_sam (Filestring_T fp, T this, int hardclip_low, int hardclip_high) {

#ifdef PRINT_INDIVIDUAL_CHARS
  FPRINTF(fp,"\t");
  for (i = this->fulllength - 1 - hardclip_low; i >= hardclip_high; --i) {
    FPRINTF(fp,"%c",complCode[(int) this->contents[i]]);
  }
  FPRINTF(fp,"\t");
#else
  FPRINTF(fp,"\t%.*R\t",this->fulllength - hardclip_high - hardclip_low,&(this->contents[hardclip_high]));
#endif

  return;
}


void
Shortread_print_hardclipped_sam (Filestring_T fp, T this, int hardclip_low, int hardclip_high) {

  /* Hardclip on same side as chop includes choplength */
  if (this->left_chop != NULL && hardclip_low > 0) {
    hardclip_low -= this->left_choplength;
  }
  if (this->right_chop != NULL && hardclip_high > 0) {
    hardclip_high -= this->right_choplength;
  }

  FPRINTF(fp,"\t");
  if (this->left_chop != NULL && hardclip_low == 0) {
    FPRINTF(fp,"%s",this->left_chop);
  }
  FPRINTF(fp,"%.*s",this->fulllength - hardclip_high - hardclip_low,&(this->contents[hardclip_low]));
  if (this->right_chop != NULL && hardclip_high == 0) {
    FPRINTF(fp,"%s",this->right_chop);
  }

  return;
}


void
Shortread_print_hardclipped_revcomp_sam (Filestring_T fp, T this, int hardclip_low, int hardclip_high) {

  /* Hardclip on same side as chop includes choplength */
  if (this->right_chop != NULL && hardclip_low > 0) {
    hardclip_low -= this->right_choplength;
  }
  if (this->left_chop != NULL && hardclip_high > 0) {
    hardclip_high -= this->left_choplength;
  }

  FPRINTF(fp,"\t");
  if (this->right_chop != NULL && hardclip_low == 0) {
    FPRINTF(fp,"%R",this->right_chop);
  }
  FPRINTF(fp,"%.*R",this->fulllength - hardclip_high - hardclip_low,&(this->contents[hardclip_high]));
  if (this->left_chop != NULL && hardclip_high == 0) {
    FPRINTF(fp,"%R",this->left_chop);
  }

  return;
}


/* Matches Shortread_print_hardclipped_sam */
void
Shortread_print_hardclipped_quality (Filestring_T fp, T this, int hardclip_low, int hardclip_high,
				     int shift) {
  int i;
  int c;

  if (this->quality == NULL) {
    FPRINTF(fp,"\t*");

  } else {
    if (this->left_chop_quality != NULL && hardclip_low > 0) {
      hardclip_low -= this->left_choplength;
    }
    if (this->right_chop_quality != NULL && hardclip_high > 0) {
      hardclip_high -= this->right_choplength;
    }

    if (shift == 0) {
      FPRINTF(fp,"\t");
      if (this->left_chop_quality != NULL && hardclip_low == 0) {
	FPRINTF(fp,"%s",this->left_chop_quality);
      }
      FPRINTF(fp,"%.*s",this->fulllength - hardclip_high - hardclip_low,&(this->quality[hardclip_low]));
      if (this->right_chop_quality != NULL && hardclip_high == 0) {
	FPRINTF(fp,"%s",this->right_chop_quality);
      }

    } else {
      FPRINTF(fp,"\t");
      if (this->left_chop_quality != NULL && hardclip_low == 0) {
	for (i = 0; i < this->left_choplength; i++) {
	  if ((c = this->left_chop_quality[i] + shift) <= 32) {
	    abort();
	  } else {
	    FPRINTF(fp,"%c",c);
	  }
	}
      }

      for (i = hardclip_low; i < this->fulllength - hardclip_high; i++) {
	if ((c = this->quality[i] + shift) <= 32) {
	  abort();
	} else {
	  FPRINTF(fp,"%c",c);
	}
      }

      if (this->right_chop_quality != NULL && hardclip_high == 0) {
	for (i = 0; i < this->right_choplength; i++) {
	  if ((c = this->right_chop_quality[i] + shift) <= 32) {
	    abort();
	  } else {
	    FPRINTF(fp,"%c",c);
	  }
	}
      }
    }
  }

  return;
}


/* Matches Shortread_print_hardclipped_revcomp_sam */
void
Shortread_print_hardclipped_reverse_quality (Filestring_T fp, T this, int hardclip_low, int hardclip_high,
					     int shift) {
  int i;
  int c;

  if (this->quality == NULL) {
    FPRINTF(fp,"\t*");

  } else {
    /* Hardclip on same side as chop includes choplength */
    if (this->right_chop_quality != NULL && hardclip_low > 0) {
      hardclip_low -= this->right_choplength;
    }
    if (this->left_chop_quality != NULL && hardclip_high > 0) {
      hardclip_high -= this->left_choplength;
    }

    if (shift == 0) {
      FPRINTF(fp,"\t");
      if (this->right_chop != NULL && hardclip_low == 0) {
	FPRINTF(fp,"%r",this->right_chop_quality);
      }
      FPRINTF(fp,"%.*r",this->fulllength - hardclip_high - hardclip_low,&(this->quality[hardclip_high]));
      if (this->left_chop != NULL && hardclip_high == 0) {
	FPRINTF(fp,"%r",this->left_chop_quality);
      }

    } else {
      FPRINTF(fp,"\t");
      if (this->right_chop_quality != NULL && hardclip_low == 0) {
	for (i = this->right_choplength - 1; i >= 0; --i) {
	  if ((c = this->right_chop_quality[i] + shift) <= 32) {
	    abort();
	  } else {
	    FPRINTF(fp,"%c",c);
	  }
	}
      }

      for (i = this->fulllength - hardclip_low - 1; i >= hardclip_high; --i) {
	if ((c = this->quality[i] + shift) <= 32) {
	  abort();
	} else {
	  FPRINTF(fp,"%c",c);
	}
      }

      if (this->left_chop_quality != NULL && hardclip_high == 0) {
	for (i = this->left_choplength - 1; i >= 0; --i) {
	  if ((c = this->left_chop_quality[i] + shift) <= 32) {
	    abort();
	  } else {
	    FPRINTF(fp,"%c",c);
	  }
	}
      }
    }
  }

  return;
}



/* For samprint XH field and print_fusion */
void
Shortread_print_chopped_end (Filestring_T fp, T this, int hardclip_low, int hardclip_high) {

  if (hardclip_low > 0) {
    if (this->left_choplength == 0) {
      FPRINTF(fp,"%.*s",hardclip_low,&(this->contents[0]));
    } else {
      hardclip_low -= this->left_choplength;
      FPRINTF(fp,"%s",this->left_chop);
      FPRINTF(fp,"%.*s",hardclip_low,&(this->contents[0]));
    }
    
  } else {
    if (this->right_choplength == 0) {
      FPRINTF(fp,"%.*s",hardclip_high,&(this->contents[this->fulllength - hardclip_high]));
    } else {
      hardclip_high -= this->right_choplength;
      FPRINTF(fp,"%.*s",hardclip_high,&(this->contents[this->fulllength - hardclip_high]));
      FPRINTF(fp,"%s",this->right_chop);
    }
  }

  return;
}

/* For samprint XH field and print_fusion */
void
Shortread_print_chopped_end_revcomp (Filestring_T fp, T this, int hardclip_low, int hardclip_high) {

  if (hardclip_low > 0) {
    if (this->right_choplength == 0) {
      FPRINTF(fp,"%.*R",hardclip_low,&(this->contents[this->fulllength - hardclip_low]));
    } else {
      hardclip_low -= this->right_choplength;
      FPRINTF(fp,"%R",this->right_chop);
      FPRINTF(fp,"%.*R",hardclip_low,&(this->contents[this->fulllength - hardclip_low]));
    }
  } else {
    if (this->left_choplength == 0) {
      FPRINTF(fp,"%.*R",hardclip_high,&(this->contents[0]));
    } else {
      hardclip_high -= this->left_choplength;
      FPRINTF(fp,"%.*R",hardclip_high,&(this->contents[0]));
      FPRINTF(fp,"%R",this->left_chop);
    }
  }

  return;
}


/* For samprint XI field and print_fusion */
void
Shortread_print_chopped_end_quality (Filestring_T fp, T this, int hardclip_low, int hardclip_high,
				     int shift) {
  int i;
  int c;

  if (this->quality == NULL) {
    FPRINTF(fp,"*");
    return;

  } else if (hardclip_low > 0) {
    if (shift == 0) {
      if (this->left_choplength == 0) {
	FPRINTF(fp,"%.*s",hardclip_low,&(this->quality[0]));
      } else {
	hardclip_low -= this->left_choplength;
	FPRINTF(fp,"%s",this->left_chop_quality);
	FPRINTF(fp,"%.*s",hardclip_low,&(this->quality[0]));
      }

    } else {
      hardclip_low -= this->left_choplength;
      for (i = 0; i < this->left_choplength; i++) {
	if ((c = this->left_chop_quality[i] + shift) <= 32) {
	  abort();
	} else {
	  FPRINTF(fp,"%c",c);
	}
      }
      for (i = 0; i < hardclip_low; i++) {
	if ((c = this->quality[i] + shift) <= 32) {
	  abort();
	} else {
	  FPRINTF(fp,"%c",c);
	}
      }
    }

  } else {
    if (shift == 0) {
      if (this->right_choplength == 0) {
	FPRINTF(fp,"%.*s",hardclip_high,&(this->quality[this->fulllength - hardclip_high]));
      } else {
	hardclip_high -= this->right_choplength;
	FPRINTF(fp,"%.*s",hardclip_high,&(this->quality[this->fulllength - hardclip_high]));
	FPRINTF(fp,"%s",this->right_chop_quality);
      }
    } else {
      hardclip_high -= this->right_choplength;
      for (i = this->fulllength - hardclip_high; i < this->fulllength; i++) {
	if ((c = this->quality[i] + shift) <= 32) {
	  abort();
	} else {
	  FPRINTF(fp,"%c",c);
	}
      }
      for (i = 0; i < this->right_choplength; i++) {
	if ((c = this->right_chop_quality[i] + shift) <= 32) {
	  abort();
	} else {
	  FPRINTF(fp,"%c",c);
	}
      }
    }
  }

  return;
}


/* For samprint XI field and print_fusion */
void
Shortread_print_chopped_end_quality_reverse (Filestring_T fp, T this, int hardclip_low, int hardclip_high,
					     int shift) {
  int i;
  int c;

  if (this->quality == NULL) {
    FPRINTF(fp,"*");
    return;

  } else if (hardclip_low > 0) {
    if (shift == 0) {
      if (this->right_choplength == 0) {
	FPRINTF(fp,"%.*r",hardclip_low,&(this->quality[this->fulllength - hardclip_low]));
      } else {
	hardclip_low -= this->right_choplength;
	FPRINTF(fp,"%r",this->right_chop_quality);
	FPRINTF(fp,"%.*r",hardclip_low,&(this->quality[this->fulllength - hardclip_low]));
      }

    } else {
      hardclip_low -= this->right_choplength;
      for (i = this->right_choplength - 1; i >= 0; --i) {
	if ((c = this->right_chop_quality[i] + shift) <= 32) {
	  abort();
	} else {
	  FPRINTF(fp,"%c",c);
	}
      }
      for (i = this->fulllength - 1; i >= this->fulllength - hardclip_low; --i) {
	if ((c = this->quality[i] + shift) <= 32) {
	  abort();
	} else {
	  FPRINTF(fp,"%c",c);
	}
      }
    }

  } else {
    if (shift == 0) {
      if (this->left_choplength == 0) {
	FPRINTF(fp,"%.*r",hardclip_high,&(this->quality[0]));
      } else {
	hardclip_high -= this->left_choplength;
	FPRINTF(fp,"%.*r",hardclip_high,&(this->quality[0]));
	FPRINTF(fp,"%r",this->left_chop_quality);
      }

    } else {
      hardclip_high -= this->left_choplength;
      for (i = hardclip_high - 1; i >= 0; --i) {
	if ((c = this->quality[i] + shift) <= 32) {
	  abort();
	} else {
	  FPRINTF(fp,"%c",c);
	}
      }
      for (i = this->left_choplength - 1; i >= 0; --i) {
	if ((c = this->left_chop_quality[i] + shift) <= 32) {
	  abort();
	} else {
	  FPRINTF(fp,"%c",c);
	}
      }
    }
  }

  return;
}


void
Shortread_print_barcode (Filestring_T fp, T this) {

  if (this->barcode != NULL) {
    FPRINTF(fp,"\tXB:Z:%s",this->barcode);
  }
    
  return;
}

void
Shortread_print_chop (Filestring_T fp, T this, bool invertp) {

  if (this->right_chop != NULL) {
    FPRINTF(fp,"\tXP:Z:");
    if (invertp == false) {
      FPRINTF(fp,"%.*s",this->right_choplength,this->right_chop);
    } else {
#ifdef PRINT_INDIVIDUAL_CHARS
      for (i = this->right_choplength - 1; i >= 0; i--) {
	FPRINTF(fp,"%c",complCode[(int) this->right_chop[i]]);
      }
#else
      FPRINTF(fp,"%.*R",this->right_choplength,this->right_chop);
#endif
    }
  }
    
  return;
}


void
Shortread_print_left_chop_symbols (Filestring_T fp, T this) {
  int i;

  for (i = 0; i < this->left_choplength; i++) {
    FPRINTF(fp,"*");
  }
  return;
}


void
Shortread_print_right_chop_symbols (Filestring_T fp, T this) {
  int i;

  for (i = 0; i < this->right_choplength; i++) {
    FPRINTF(fp,"*");
  }
  return;
}


void
Shortread_print_quality (Filestring_T fp, T this, int hardclip_low, int hardclip_high,
			int shift, bool show_chopped_p) {
  int i;
  int c;

  if (this->quality == NULL) {
    FPRINTF(fp,"*");

  } else if (shift == 0) {
    FPRINTF(fp,"%.*s",this->fulllength - hardclip_high - hardclip_low,&(this->quality[hardclip_low]));

  } else {
    for (i = hardclip_low; i < this->fulllength - hardclip_high; i++) {
      if ((c = this->quality[i] + shift) <= 32) {
	fprintf(stderr,"Warning: With a quality-print-shift of %d, QC score %c becomes non-printable.  May need to specify --quality-protocol or --quality-print-shift\n",
		shift,this->quality[i]);
	abort();
      } else {
	FPRINTF(fp,"%c",c);
      }
    }

    if (show_chopped_p == true) {
      assert(hardclip_high == 0);
      if (shift == 0) {
	FPRINTF(fp,"%.*s",this->right_choplength,this->right_chop_quality);

      } else {
	for (i = 0; i < this->right_choplength; i++) {
	  if ((c = this->right_chop_quality[i] + shift) <= 32) {
	    fprintf(stderr,"Warning: With a quality-print-shift of %d, QC score %c becomes non-printable.  May need to specify --quality-protocol or --quality-print-shift\n",
		    shift,this->right_chop_quality[i]);
	    abort();
	  } else {
	    FPRINTF(fp,"%c",c);
	  }
	}
      }
    }

  }

  return;
}

void
Shortread_print_quality_revcomp (Filestring_T fp, T this, int hardclip_low, int hardclip_high,
				int shift, bool show_chopped_p) {
  int i;
  int c;

  if (this->quality == NULL) {
    FPRINTF(fp,"*");

  } else if (shift == 0) {
    FPRINTF(fp,"%.*r",this->fulllength - hardclip_low - hardclip_high,&(this->quality[hardclip_high]));

  } else {
    for (i = this->fulllength - 1 - hardclip_low; i >= hardclip_high; --i) {
      if ((c = this->quality[i] + shift) <= 32) {
	fprintf(stderr,"Warning: With a quality-print-shift of %d, QC score %c becomes non-printable.  May need to specify --quality-protocol or --quality-print-shift\n",
		shift,this->quality[i]);
	abort();
      } else {
	FPRINTF(fp,"%c",c);
      }
    }

    if (show_chopped_p == true) {
      /* assert(hardclip_low == 0); */
      if (shift == 0) {
	FPRINTF(fp,"%.*r",this->right_choplength,this->right_chop_quality);

      } else {
	for (i = this->right_choplength - 1; i >= 0; i--) {
	  if ((c = this->right_chop_quality[i] + shift) <= 32) {
	    fprintf(stderr,"Warning: With a quality-print-shift of %d, QC score %c becomes non-printable.  May need to specify --quality-protocol or --quality-print-shift\n",
		    shift,this->right_chop_quality[i]);
	    abort();
	  } else {
	    FPRINTF(fp,"%c",c);
	  }
	}
      }
    }
  }

  return;
}

/* Called by print_genomic in substring.c for standard output */
void
Shortread_print_subseq_uc (Filestring_T fp, T this, int querystart, int queryend) {
  int i;

  for (i = 0; i < querystart; i++) {
    FPRINTF(fp,"-");
  }

#ifdef PRINT_INDIVIDUAL_CHARS
  for ( ; i < queryend; i++) {
    FPRINTF(fp,"%c",this->contents_uc[i]);
  }
#else
  FPRINTF(fp,"%.*s",queryend - querystart,&(this->contents_uc[querystart]));
#endif

  for (i = queryend; i < this->fulllength; i++) {
    FPRINTF(fp,"-");
  }

  return;
}

/* Called by print_genomic in substring.c for standard output */
void
Shortread_print_subseq_revcomp_uc (Filestring_T fp, T this, int querystart, int queryend) {
  int i;

  for (i = this->fulllength-1; i >= queryend; --i) {
    FPRINTF(fp,"-");
  }

#ifdef PRINT_INDIVIDUAL_CHARS
  for (i = queryend-1; i >= querystart; --i) {
    FPRINTF(fp,"%c",complCode[(int) this->contents_uc[i]]);
  }
#else
  FPRINTF(fp,"%.*R",queryend - querystart,&(this->contents_uc[querystart]));
#endif

  for (i = querystart-1; i >= 0; --i) {
    FPRINTF(fp,"-");
  }

  return;
}

