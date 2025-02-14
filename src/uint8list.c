static char rcsid[] = "$Id: 3c2109637c4d0b05cd6392bd07ee08f1beb2c66d $";
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "uint8list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mem.h"

#define T Uint8list_T

#if !defined(HAVE_INLINE)
T
Uint8list_push (T list, UINT8 x) {
  T new = (T) MALLOC(sizeof(*new));
  
  new->first = x;
  new->rest = list;
  return new;
}

T
Uint8list_pop (T list, UINT8 *x) {
  T head;

  if (list) {
    head = list->rest;
    *x = list->first;
    FREE(list);
    return head;
  } else {
    return list;
  }
}
  
UINT8
Uint8list_head (T list) {
  return list->first;
}

T
Uint8list_next (T list) {
  if (list) {
    return list->rest;
  } else {
    return NULL;
  }
}

void
Uint8list_free (T *list) {
  T prev;

  while ((prev = *list) != NULL) {
    *list = prev->rest;
    FREE(prev);
  }
}

T
Uint8list_reverse (T list) {
  T head = NULL, next;

  for ( ; list; list = next) {
    next = list->rest;
    list->rest = head;
    head = list;
  }
  return head;
}

int
Uint8list_length (T list) {
  int n;
  
  for (n = 0; list; list = list->rest) {
    n++;
  }
  return n;
}
#endif


void
Uint8list_head_set (T this, UINT8 x) {
  this->first = x;
  return;
}

T
Uint8list_keep_one (T list, int i) {
  T head;

  while (--i >= 0) {
    /* Pop */
    head = list->rest;
    FREE(list);
    list = head;
  }

  Uint8list_free(&list->rest);
  return list;
}

UINT8
Uint8list_max (T list) {
  UINT8 m = 0;

  while (list) {
    if (list->first > m) {
      m = list->first;
    }
    list = list->rest;
  }

  return m;
}

UINT8
Uint8list_min (T list) {
  UINT8 m;

  if (list == NULL) {
    return 0;

  } else {
    m = list->first;
    list = list->rest;
    while (list) {
      if (list->first < m) {
	m = list->first;
      }
      list = list->rest;
    }

    return m;
  }
}

UINT8 *
Uint8list_to_array (T list, UINT8 end) {
  UINT8 *array;
  int n, i;

  n = Uint8list_length(list);
  array = (UINT8 *) MALLOC((n+1)*sizeof(UINT8));
  for (i = 0; i < n; i++) {
    array[i] = list->first;
    list = list->rest;
  }
  array[i] = end;
  return array;
}

UINT8 *
Uint8list_to_array_n (int *n, T list) {
  UINT8 *array;
  int i;

  *n = Uint8list_length(list);
  if (*n == 0) {
    return (UINT8 *) NULL;
  } else {
    array = (UINT8 *) CALLOC(*n,sizeof(UINT8));
    for (i = 0; i < *n; i++) {
      array[i] = list->first;
      list = list->rest;
    }
    return array;
  }
}

UINT8 *
Uint8list_to_array_out (int *n, T list) {
  UINT8 *array;
  int i;

  *n = Uint8list_length(list);
  if (*n == 0) {
    return NULL;
  } else {
    array = (UINT8 *) CALLOC_OUT(*n,sizeof(UINT8));
    for (i = 0; i < *n; i++) {
      array[i] = list->first;
      list = list->rest;
    }
    return array;
  }
}

void
Uint8list_fill_array (UINT8 *array, T list) {
  int i = 0;

  while (list) {
    array[i++] = list->first;
    list = list->rest;
  }

  return;
}

void
Uint8list_fill_array_and_free (UINT8 *array, T *list) {
  T prev;
  int i = 0;

  while ((prev = *list) != NULL) {
    array[i++] = prev->first;
    *list = prev->rest;
    FREE(prev);
  }

  return;
}


T
Uint8list_from_array (UINT8 *array, int n) {
  T list = NULL, p;

  while (--n >= 0) {
    p = (T) MALLOC(sizeof(*p));
    p->first = array[n];
    p->rest = list;
    list = p;
  }

  return list;
}

T
Uint8list_copy (T list) {
  T head, *p = &head;

  for ( ; list; list = list->rest) {
    *p = (T) MALLOC(sizeof(**p));
    (*p)->first = list->first;
    p = &(*p)->rest;
  }
  *p = NULL;
  return head;
}

T
Uint8list_append (T list, T tail) {
  T *p = &list;

  while (*p) {
    p = &(*p)->rest;
  }
  *p = tail;
  return list;
}

UINT8
Uint8list_last_value (T this) {
  T last = NULL, r;

  for (r = this; r != NULL; r = r->rest) {
    last = r;
  }
  return last->first;
}

UINT8
Uint8list_index (T this, int index) {
  while (index-- > 0) {
    this = this->rest;
  }
  return this->first;
}


bool
Uint8list_find (T this, UINT8 value) {
  T r;

  for (r = this; r != NULL; r = r->rest) {
    if (r->first == value) {
      return true;
    }
  }
  return false;
}

bool
Uint8list_equal (T x, T y) {

  while (x && y && x->first == y->first) {
    x = x->rest;
    y = y->rest;
  }
  if (x || y) {
    return false;
  } else {
    return true;
  }
}

char *
Uint8list_to_string (T this) {
  char *string, Buffer[256];
  T p;
  int n, i, strlength;

  if ((n = Uint8list_length(this)) == 0) {
    string = (char *) CALLOC(1,sizeof(char));
    string[0] = '\0';
  } else {
    strlength = 0;
    for (i = 0, p = this; i < n-1; i++, p = Uint8list_next(p)) {
      sprintf(Buffer,"%llu,",(unsigned long long) Uint8list_head(p));
      strlength += strlen(Buffer);
    }
    sprintf(Buffer,"%llu",(unsigned long long) Uint8list_head(p));
    strlength += strlen(Buffer);

    string = (char *) CALLOC(strlength + 1,sizeof(char));
    string[0] = '\0';
    for (i = 0, p = this; i < n-1; i++, p = Uint8list_next(p)) {
      sprintf(Buffer,"%llu,",(unsigned long long) Uint8list_head(p));
      strcat(string,Buffer);
    }
    sprintf(Buffer,"%llu",(unsigned long long) Uint8list_head(p));
    strcat(string,Buffer);
  }

  return string;
}


char *
Uint8list_to_string_offset (T this, UINT8 chroffset) {
  char *string, Buffer[256];
  T p;
  int n, i, strlength;

  if ((n = Uint8list_length(this)) == 0) {
    string = (char *) CALLOC(1,sizeof(char));
    string[0] = '\0';
  } else {
    strlength = 0;
    for (i = 0, p = this; i < n-1; i++, p = Uint8list_next(p)) {
      sprintf(Buffer,"%llu,",(unsigned long long) (Uint8list_head(p) - chroffset));
      strlength += strlen(Buffer);
    }
    sprintf(Buffer,"%llu",(unsigned long long) (Uint8list_head(p) - chroffset));
    strlength += strlen(Buffer);

    string = (char *) CALLOC(strlength + 1,sizeof(char));
    string[0] = '\0';
    for (i = 0, p = this; i < n-1; i++, p = Uint8list_next(p)) {
      sprintf(Buffer,"%llu,",(unsigned long long) (Uint8list_head(p) - chroffset));
      strcat(string,Buffer);
    }
    sprintf(Buffer,"%llu",(unsigned long long) Uint8list_head(p) - chroffset);
    strcat(string,Buffer);
  }

  return string;
}
