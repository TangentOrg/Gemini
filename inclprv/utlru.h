/* 
Copyright (C) 2001 NuSphere Corporation, All Rights Reserved.

This program is open source software.  You may not copy or use this 
file, in either source code or executable form, except in compliance 
with the NuSphere Public License.  You can redistribute it and/or 
modify it under the terms of the NuSphere Public License as published 
by the NuSphere Corporation; either version 2 of the License, or 
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
NuSphere Public License for more details.

You should have received a copy of the NuSphere Public License
along with this program; if not, write to NuSphere Corporation
14 Oak Park, Bedford, MA 01730.
*/

#ifndef UTLRU_H
#define UTLRU_H

#ifdef PRO_REENTRANT
#include <prothread.h>
#endif

typedef struct lruEntry
{
     struct lruEntry    *next;      /* ptr to the next in the chain */
     struct lruEntry    *prev;      /* ptr to the prev in the chain */
} lruEntry_t;

typedef struct lruAnchor
{
#if defined(PRO_REENTRANT) && OPSYS==UNIX
    pthread_mutex_t     lru_lock;    /* Used to lock the LRU chain */
#endif
    lruEntry_t          *phead;     /* ptr to the head of the chain */
    ULONG               entryCnt;   /* # of available entries 0 = full */
    ULONG               numEntries; /* initial # of entries */
} lruAnchor_t;

/* API's */

DSMVOID utLruInit(lruAnchor_t *pAnchor, ULONG numEntries);

DSMVOID * utDoLru(lruAnchor_t * pAnchor, DSMVOID *pEntry);

DSMVOID utRemoveLru(lruAnchor_t * pAnchor, DSMVOID *pEntry);

#endif  /* UTLRU_H */
