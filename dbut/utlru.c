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

#include "gem_global.h"
#include "dbconfig.h"
#include "utlru.h"

/***********************************************************************/
/* A generic Least Recently Used (LRU) mechanism.                      */
/*                                                                     */
/* To use the LRU mechanism you need to allocate an Anchor for the     */
/* LRU chain and call utLruInit() providing the total number of        */
/* entries allowed on the chain.                                       */
/* Once you have sone this you can then use the utDoLru() call to      */
/* managed the LRU chain.  The utDoLru() call expects to get passed    */
/* a structure that has a next and prev pointer as it's first 2 members*/ 
/* For Example:                                                        */
/* typedef struct mystruct                                             */
/* {                                                                   */
/*     struct mystruct   *next;                                        */
/*     struct mystruct   *prev;                                        */
/*             o                                                       */
/*             o                                                       */
/*             o                                                       */
/* } mystruct_t                                                        */
/* As you can see the LRU pointers are embedded in the user structure. */
/***********************************************************************/

/* PROGRAM: utLruInit - Allocate all memory for the LRU chain
 *
 *  utLruInit is passed the number of entries allowed in the LRU
 *  chain.  It initializes the anchor with the nunber of entries
 *  and initializes the mutex
 *  This structure contains:
 *            mutex to lock on
 *            pointer to the head of the LRU chaain
 *
 * RETURNS: DSMVOID
 */
DSMVOID
utLruInit(lruAnchor_t *pAnchor, ULONG numEntries)
{
#ifdef PRO_REENTRANT
    pthread_mutexattr_t attr;
#endif

#ifdef PRO_REENTRANT
    pthread_mutexattr_init(&attr);
    pthread_mutex_init(&pAnchor->lru_lock, &attr);
    pthread_mutexattr_destroy(&attr);       
#endif

    /* initialize the total number allowed */
    pAnchor->numEntries = pAnchor->entryCnt = numEntries;

    /* terminte the LRU chain */
    pAnchor->phead = (lruEntry_t *)0;
}

/* PROGRAM: utDoLru - moves the passed pointer to the front of the LRU chain
 *
 *  If the passed pointer is found on the chain it will be moved
 *  to the front of the chain
 *
 *  If the pointer is not on the chain and the counter is non-zero
 *  it will add the entry to the head of the chain
 *
 *  If the pointer is not on the chain and there are no free entries,
 *  the Least Recently Used entry is evicted, it's pointer member is 
 *  returned to the caller, and the entry is replaced with the passed
 *  pointer and moved to the head of the chain.
 *
 * RETURNS: NULL - if entry is on the chain already
 *          DSMVOID * pointing to the evicted entry
 */
DSMVOID *
utDoLru(lruAnchor_t * pAnchor, DSMVOID *pEntry)
{
    lruEntry_t  *ptr = (lruEntry_t *)pEntry;
    lruEntry_t  *pEvict;

#ifdef PRO_REENTRANT
   /* lock the entire chain */
   pthread_mutex_lock((pthread_mutex_t *)&pAnchor->lru_lock);
#endif

    /* if the LRU list is empty, just put the entry on the front */
    if (pAnchor->phead == (lruEntry_t *)0)
    {
#ifdef UTLRU_DEBUG
    utWalkLru(pAnchor, "BEGIN:Head is empty");
#endif
        /* make this entry the head */
        pAnchor->phead = (lruEntry_t *)ptr;

        /* set the next and prev pointers */
        ptr->next = ptr;
        ptr->prev = ptr;

        /* use up one of the entries */
        pAnchor->entryCnt--;
#ifdef UTLRU_DEBUG
        utWalkLru(pAnchor, "END:Head is empty");
#endif
        goto found;
    }

    /* if the entry is on the chain, move it to the head */
    if (ptr->next)
    {
        /* if it is already the head, just return */
        if (pAnchor->phead == (lruEntry_t *)ptr)
            goto found;

#ifdef UTLRU_DEBUG
    utWalkLru(pAnchor, "BEGIN:Entry exists moving");
#endif
        /* remove it from it's current location */
        ptr->prev->next = ptr->next;
        ptr->next->prev = ptr->prev;
    
        /* put it on the head */
        ptr->next = pAnchor->phead;
        ptr->prev = pAnchor->phead->prev;
        ptr->prev->next = ptr;
        ptr->next->prev = ptr;
        pAnchor->phead = ptr;
#ifdef UTLRU_DEBUG
        utWalkLru(pAnchor, "END:Entry exists moving");
#endif
        goto found;
    }

    /* entry is not on a chain, if there is still room on the 
       chain just put this one at the head */
    if (pAnchor->entryCnt)
    {
#ifdef UTLRU_DEBUG
        utWalkLru(pAnchor, "BEGIN: Adding an entry");
#endif
        /* use up an entry */
        pAnchor->entryCnt--;

        /* put it on the head */
        ptr->next = pAnchor->phead;
        ptr->prev = pAnchor->phead->prev;
        ptr->prev->next = ptr;
        ptr->next->prev = ptr;
        pAnchor->phead = ptr;
#ifdef UTLRU_DEBUG
        utWalkLru(pAnchor, "END: Adding an entry");
#endif
        goto found;
    }

    /* entry is not on the LRU chain and all entries on the chain
       are in-use.  Evict the oldest entry and return the pointer
       that was evicted. */

#ifdef UTLRU_DEBUG
    utWalkLru(pAnchor, "BEGIN: Evicting to add");
#endif

    /* remove the evicted block */
    pEvict = pAnchor->phead->prev;
    pEvict->prev->next = pEvict->next;
    pEvict->next->prev = pEvict->prev;
    pEvict->next = pEvict->prev = (lruEntry_t *)0;

    /* put the passed entry at the head */
    ptr->next = pAnchor->phead;
    ptr->prev = pAnchor->phead->prev;
    ptr->prev->next = ptr;
    ptr->next->prev = ptr;
    pAnchor->phead = ptr;
#ifdef UTLRU_DEBUG
    utWalkLru(pAnchor, "END: Evicting to add");
#endif
    goto notfound;

found:
#ifdef PRO_REENTRANT
    pthread_mutex_unlock((pthread_mutex_t *)&pAnchor->lru_lock);
#endif
    return (DSMVOID *)0;

notfound:
#ifdef PRO_REENTRANT
    pthread_mutex_unlock((pthread_mutex_t *)&pAnchor->lru_lock);
#endif
    return pEvict;
}

/* PROGRAM: utRemoveLru - Remove an entry from the LRU chain
 *
 * RETURNS: DSMVOID
 */
DSMVOID
utRemoveLru(lruAnchor_t * pAnchor, DSMVOID *pEntry)
{
    lruEntry_t  *ptr = (lruEntry_t *)pEntry;

#ifdef PRO_REENTRANT
   /* lock the entire chain */
   pthread_mutex_lock((pthread_mutex_t *)&pAnchor->lru_lock);
#endif

#ifdef UTLRU_DEBUG
    utWalkLru(pAnchor, "BEGIN: removing an entry");
#endif
    /* remove the evicted block */
    ptr->prev->next = ptr->next;
    ptr->next->prev = ptr->prev;

    if (pAnchor->phead = ptr)
    {
        pAnchor->phead = ptr->next;
    }
    ptr->next = ptr->prev = (lruEntry_t *)0;

    if (pAnchor->phead->next == (lruEntry_t *)0)
    {
        /* removed the last entry, so clear the head */
        pAnchor->phead = (lruEntry_t *)0;
    }

    /* free up an entry */
    pAnchor->entryCnt++;

#ifdef UTLRU_DEBUG
    utWalkLru(pAnchor, "END: removing an entry");
#endif
#ifdef PRO_REENTRANT
    pthread_mutex_unlock((pthread_mutex_t *)&pAnchor->lru_lock);
#endif
}

/* PROGRAM: utWalkLru - Walk the chain and dump out the values
 *
 * RETURNS: 0
 */
int
utWalkLru(lruAnchor_t * pAnchor, TEXT *p)
{
    lruEntry_t  *ptr;
    ULONG        i;

    printf("%s\n",p);
    if (!pAnchor)
    {
        printf("Anchor is empty\n");
        return 0;
    }

    printf("There are %ld entries left on the chain\n",pAnchor->entryCnt); 
    if (!pAnchor->phead)
    {
        printf("Chain is empty\n");
        return 0;
    }

    printf("Head: %x, next: %x, prev: %x\n",pAnchor->phead,
                             pAnchor->phead->next,pAnchor->phead->prev);
    i = 1;
    ptr = pAnchor->phead->next;
    while (ptr != pAnchor->phead)
    {
        i++;
        printf("[%ld]: %x, next: %x, prev: %x\n", i, ptr, ptr->next, ptr->prev);
        ptr = ptr->next;
        if ((ptr->next == ptr) && (ptr != pAnchor->phead))
        {
            printf("Chain is broken\n");
            break;
        }
    }
    printf("COMPLETE walk of chain\n");
    
    return 0;
}
