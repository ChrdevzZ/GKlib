/*
 * Copyright 2004, Regents of the University of Minnesota
 *
 * This file contains routines for manipulating a direct-access hash table
 *
 * Started 3/22/04
 * George
 *
 */

#include <GKlib.h>
#include "memory_internal.h"


/******************************************************************************
* This function checks the internal state of a hash table
*******************************************************************************/
static int HTable_IsValid(gk_HTable_t *htable)
{
  return htable != NULL && htable->harray != NULL &&
      htable->nelements > 0 && htable->htsize >= 0 &&
      htable->htsize <= htable->nelements;
}


/******************************************************************************
* This function inserts into a table whose capacity is already sufficient
*******************************************************************************/
static int HTable_InsertInto(gk_HTable_t *htable, int key, int val)
{
  int i, first;

  first = (int)((unsigned int)key % (unsigned int)htable->nelements);

  for (i=first; i<htable->nelements; i++) {
    if (htable->harray[i].key == HTABLE_EMPTY ||
        htable->harray[i].key == HTABLE_DELETED) {
      htable->harray[i].key = key;
      htable->harray[i].val = val;
      htable->htsize++;
      return 1;
    }
  }

  for (i=0; i<first; i++) {
    if (htable->harray[i].key == HTABLE_EMPTY ||
        htable->harray[i].key == HTABLE_DELETED) {
      htable->harray[i].key = key;
      htable->harray[i].val = val;
      htable->htsize++;
      return 1;
    }
  }

  return 0;
}


/******************************************************************************
* This function prepares a replacement table before publishing it
*******************************************************************************/
static int HTable_ResizeNoSignal(gk_HTable_t *htable, int nelements)
{
  gk_HTable_t next;
  gk_ikv_t *new_harray;
  gk_ikv_t *old_harray;
  int i, saved_errno;
  size_t harray_bytes;

  if (!HTable_IsValid(htable) ||
      nelements <= 0 || nelements < htable->htsize) {
    errno = EINVAL;
    return 0;
  }
  if (!gk_size_mul((size_t)nelements, sizeof(gk_ikv_t), &harray_bytes))
    return 0;

  new_harray = (gk_ikv_t *)gk_malloc_nosignal(harray_bytes);
  if (new_harray == NULL) {
    if (errno == 0)
      errno = ENOMEM;
    return 0;
  }

  next.nelements = nelements;
  next.htsize = 0;
  next.harray = new_harray;
  for (i=0; i<nelements; i++)
    next.harray[i].key = HTABLE_EMPTY;

  for (i=0; i<htable->nelements; i++) {
    if (htable->harray[i].key != HTABLE_EMPTY &&
        htable->harray[i].key != HTABLE_DELETED &&
        !HTable_InsertInto(&next, htable->harray[i].key,
                           (int)htable->harray[i].val)) {
      (void)gk_free_nosignal((void **)&new_harray);
      errno = EINVAL;
      return 0;
    }
  }
  if (next.htsize != htable->htsize) {
    (void)gk_free_nosignal((void **)&new_harray);
    errno = EINVAL;
    return 0;
  }

  old_harray = htable->harray;
  if (!gk_free_nosignal((void **)&old_harray)) {
    saved_errno = errno != 0 ? errno : EINVAL;
    (void)gk_free_nosignal((void **)&new_harray);
    errno = saved_errno;
    return 0;
  }

  htable->nelements = next.nelements;
  htable->htsize = next.htsize;
  htable->harray = next.harray;
  return 1;
}


/******************************************************************************
* This function creates the hash-table
*******************************************************************************/
gk_HTable_t *HTable_Create(int nelements)
{
  int saved_errno;
  gk_HTable_t *htable;
  size_t harray_bytes;

  if (nelements <= 0) {
    errno = EINVAL;
    gk_errexit(SIGERR, "HTable_Create: capacity must be positive");
    errno = EINVAL;
    return NULL;
  }
  if (!gk_size_mul((size_t)nelements, sizeof(gk_ikv_t), &harray_bytes)) {
    gk_errexit(SIGMEM, "HTable_Create: capacity size overflow");
    errno = EOVERFLOW;
    return NULL;
  }

  htable = (gk_HTable_t *)gk_malloc_nosignal(sizeof(gk_HTable_t));
  if (htable == NULL) {
    saved_errno = errno != 0 ? errno : ENOMEM;
    errno = saved_errno;
    gk_errexit(SIGMEM, "HTable_Create: htable allocation failed");
    errno = saved_errno;
    return NULL;
  }
  htable->harray = (gk_ikv_t *)gk_malloc_nosignal(harray_bytes);
  if (htable->harray == NULL) {
    saved_errno = errno != 0 ? errno : ENOMEM;
    gk_free((void **)&htable, LTERM);
    errno = saved_errno;
    gk_errexit(SIGMEM, "HTable_Create: harray allocation failed");
    errno = saved_errno;
    return NULL;
  }
  htable->nelements = nelements;
  htable->htsize = 0;

  HTable_Reset(htable);

  return htable;
}


/******************************************************************************
* This function resets the data-structures associated with the hash-table
*******************************************************************************/
void HTable_Reset(gk_HTable_t *htable)
{
  int i;

  if (!HTable_IsValid(htable)) {
    errno = EINVAL;
    gk_errexit(SIGERR, "HTable_Reset: invalid hash table");
    errno = EINVAL;
    return;
  }

  for (i=0; i<htable->nelements; i++)
    htable->harray[i].key = HTABLE_EMPTY;
  htable->htsize = 0;

}

/******************************************************************************
* This function resizes the hash-table
*******************************************************************************/
void HTable_Resize(gk_HTable_t *htable, int nelements)
{
  int saved_errno;

  if (!HTable_ResizeNoSignal(htable, nelements)) {
    saved_errno = errno != 0 ? errno : ENOMEM;
    errno = saved_errno;
    gk_errexit(saved_errno == EINVAL ? SIGERR : SIGMEM,
        "HTable_Resize: resize failed");
    errno = saved_errno;
  }
}


/******************************************************************************
* This function inserts a key-value pair in the array
*******************************************************************************/
void HTable_Insert(gk_HTable_t *htable, int key, int val)
{
  int saved_errno;

  if (!HTable_IsValid(htable)) {
    errno = EINVAL;
    gk_errexit(SIGERR, "HTable_Insert: invalid hash table");
    errno = EINVAL;
    return;
  }
  if (key == HTABLE_EMPTY || key == HTABLE_DELETED) {
    errno = EINVAL;
    gk_errexit(SIGERR, "HTable_Insert: reserved key");
    errno = EINVAL;
    return;
  }

  if (htable->htsize > htable->nelements/2) {
    if (htable->nelements > INT_MAX/2) {
      errno = EOVERFLOW;
      gk_errexit(SIGMEM, "HTable_Insert: capacity size overflow");
      errno = EOVERFLOW;
      return;
    }
    if (!HTable_ResizeNoSignal(htable, 2*htable->nelements)) {
      saved_errno = errno != 0 ? errno : ENOMEM;
      errno = saved_errno;
      gk_errexit(saved_errno == EINVAL ? SIGERR : SIGMEM,
          "HTable_Insert: resize failed");
      errno = saved_errno;
      return;
    }
  }

  if (!HTable_InsertInto(htable, key, val)) {
    errno = ENOSPC;
    gk_errexit(SIGERR, "HTable_Insert: hash table is full");
    errno = ENOSPC;
  }
}


/******************************************************************************
* This function deletes key from the htable
*******************************************************************************/
void HTable_Delete(gk_HTable_t *htable, int key)
{
  int i, first;

  if (!HTable_IsValid(htable)) {
    errno = EINVAL;
    gk_errexit(SIGERR, "HTable_Delete: invalid hash table");
    errno = EINVAL;
    return;
  }
  if (key == HTABLE_EMPTY || key == HTABLE_DELETED) {
    errno = EINVAL;
    gk_errexit(SIGERR, "HTable_Delete: reserved key");
    errno = EINVAL;
    return;
  }

  first = HTable_HFunction(htable->nelements, key);

  for (i=first; i<htable->nelements; i++) {
    if (htable->harray[i].key == key) {
      htable->harray[i].key = HTABLE_DELETED;
      htable->htsize--;
      return;
    }
  }

  for (i=0; i<first; i++) {
    if (htable->harray[i].key == key) {
      htable->harray[i].key = HTABLE_DELETED;
      htable->htsize--;
      return;
    }
  }

}


/******************************************************************************
* This function returns the data associated with the key in the hastable
*******************************************************************************/
int HTable_Search(gk_HTable_t *htable, int key)
{
  int i, first;

  if (!HTable_IsValid(htable)) {
    errno = EINVAL;
    gk_errexit(SIGERR, "HTable_Search: invalid hash table");
    errno = EINVAL;
    return -1;
  }
  if (key == HTABLE_EMPTY || key == HTABLE_DELETED) {
    errno = EINVAL;
    gk_errexit(SIGERR, "HTable_Search: reserved key");
    errno = EINVAL;
    return -1;
  }

  first = HTable_HFunction(htable->nelements, key);

  for (i=first; i<htable->nelements; i++) {
    if (htable->harray[i].key == key)
      return (int)htable->harray[i].val;
    else if (htable->harray[i].key == HTABLE_EMPTY)
      return -1;
  }

  for (i=0; i<first; i++) {
    if (htable->harray[i].key == key)
      return (int)htable->harray[i].val;
    else if (htable->harray[i].key == HTABLE_EMPTY)
      return -1;
  }

  return -1;
}


/******************************************************************************
* This function returns the next key/val
*******************************************************************************/
int HTable_GetNext(gk_HTable_t *htable, int key, int *r_val, int type)
{
  int i;
  static int first, last;

  if (!HTable_IsValid(htable) || r_val == NULL ||
      (type != HTABLE_FIRST && type != HTABLE_NEXT)) {
    errno = EINVAL;
    gk_errexit(SIGERR, "HTable_GetNext: invalid argument");
    errno = EINVAL;
    return -1;
  }
  if (key == HTABLE_EMPTY || key == HTABLE_DELETED) {
    errno = EINVAL;
    gk_errexit(SIGERR, "HTable_GetNext: reserved key");
    errno = EINVAL;
    return -1;
  }

  if (type == HTABLE_FIRST)
    first = last = HTable_HFunction(htable->nelements, key);

  if (first > last) {
    for (i=first; i<htable->nelements; i++) {
      if (htable->harray[i].key == key) {
        *r_val = (int)htable->harray[i].val;
        first = i+1;
        return 1;
      }
      else if (htable->harray[i].key == HTABLE_EMPTY)
        return -1;
    }
    first = 0;
  }

  for (i=first; i<last; i++) {
    if (htable->harray[i].key == key) {
      *r_val = (int)htable->harray[i].val;
      first = i+1;
      return 1;
    }
    else if (htable->harray[i].key == HTABLE_EMPTY)
      return -1;
  }

  return -1;
}


/******************************************************************************
* This function returns the data associated with the key in the hastable
*******************************************************************************/
int HTable_SearchAndDelete(gk_HTable_t *htable, int key)
{
  int i, first;

  if (!HTable_IsValid(htable)) {
    errno = EINVAL;
    gk_errexit(SIGERR, "HTable_SearchAndDelete: invalid hash table");
    errno = EINVAL;
    return -1;
  }
  if (key == HTABLE_EMPTY || key == HTABLE_DELETED) {
    errno = EINVAL;
    gk_errexit(SIGERR, "HTable_SearchAndDelete: reserved key");
    errno = EINVAL;
    return -1;
  }

  first = HTable_HFunction(htable->nelements, key);

  for (i=first; i<htable->nelements; i++) {
    if (htable->harray[i].key == key) {
      htable->harray[i].key = HTABLE_DELETED;
      htable->htsize--;
      return (int)htable->harray[i].val;
    }
    else if (htable->harray[i].key == HTABLE_EMPTY)
      gk_errexit(SIGERR, "HTable_SearchAndDelete: Failed to find the key!\n");
  }

  for (i=0; i<first; i++) {
    if (htable->harray[i].key == key) {
      htable->harray[i].key = HTABLE_DELETED;
      htable->htsize--;
      return (int)htable->harray[i].val;
    }
    else if (htable->harray[i].key == HTABLE_EMPTY)
      gk_errexit(SIGERR, "HTable_SearchAndDelete: Failed to find the key!\n");
  }

  return -1;

}



/******************************************************************************
* This function destroys the data structures associated with the hash-table
*******************************************************************************/
void HTable_Destroy(gk_HTable_t *htable)
{
  if (htable == NULL)
    return;

  gk_free((void **)&htable->harray, &htable, LTERM);
}


/******************************************************************************
* This is the hash-function. Based on multiplication
*******************************************************************************/
int HTable_HFunction(int nelements, int key)
{
  if (nelements <= 0) {
    errno = EINVAL;
    gk_errexit(SIGERR, "HTable_HFunction: capacity must be positive");
    errno = EINVAL;
    return 0;
  }

  return (int)((unsigned int)key % (unsigned int)nelements);
}
