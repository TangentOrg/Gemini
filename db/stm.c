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

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include "dbconfig.h"
 
#include "dbcontext.h"
#include "dbpub.h"
#include "dsmpub.h"
#include "latpub.h"
#include "sempub.h"
#include "shmprv.h"
#include "stmpub.h"
#include "stmprv.h"
#include "stmsg.h"
#include "smmsg.h"
#include "usrctl.h"
#include "utcpub.h"
#include "utmpub.h"
#include "utspub.h"

#if OPSYS==WIN32API
#include "dos.h"
#endif

#if OPSYS==UNIX
#include <unistd.h>
#endif

#ifdef ST_PURIFY
#include <malloc.h>
#endif

#define MSGD      MSGD_CALLBACK

/*#define ST_MEMCHK 1	*/
#ifdef ST_MEMCHK

#include "stmchk.h"
 

/* The poolList is protected by PL_LOCK_STPOOL */
LOCAL POOLPTRS *poolList = (POOLPTRS*)NULL;

/* The following statics are NOT used by rocket */
LOCAL int poolChecking = 0;
LOCAL int poolsAllocated = 0;
LOCAL int last_slot = 0;

#endif /* ST_MEMCHK */

/* LOCAL Function Protypes */
LOCALF DSMVOID stmVacInBlock   (dsmContext_t *pcontext, stpool_t *pstpool,
                             abk_t *pabk, rentprfx_t *phdr);
LOCALF DSMVOID stmInitDummyHdr (dsmContext_t *pcontext, stpool_t *pstpool, 
                             abk_t *pabk);
LOCALF abk_t* stmGetbk(dsmContext_t *pcontext, stpool_t *pstpool, unsigned len);

#ifdef ST_MEMCHK
LOCALF int stmEnableChecking(dsmContext_t *pcontext, int flag);

LOCALF POOLPTRS *stmAddPoolPtr(dsmContext_t *pcontext, stpool_t *pstpool);

LOCALF DSMVOID stmRemovePoolPtr(dsmContext_t *pcontext, stpool_t *pstpool);
#endif

#ifdef ST_PURIFY
LOCALF POOLPTRS *stmFindPoolPtr(dsmContext_t *pcontext, stpool_t *pstpoolr,
                                 TEXT *parea);

LOCALF stpool_t * stmGetPoolPtr(dsmContext_t *pcontext, TEXT *parea);

LOCALF DSMVOID stmFreeAllPUR (dsmContext_t *pcontext, stpool_t *pstpool);

LOCALF TEXT * stmMallocPUR(dsmContext_t *pcontext, stpool_t *pstpool,
                           unsigned len, GBOOL st_fatal);
LOCALF DSMVOID stmFreePUR(dsmContext_t *pcontext, stpool_t *pstpool,
                       TEXT *parea);
#endif

/* set it to 0 in case anyone #if's */
/* default is off, turn it on in $RDL/local/rh/machine.h for     */
/* central dev environment and during the EARLY stages of a port */
#ifdef STERILIZE
#undef STERILIZE
#define STERILIZE 0
#endif

/* PROGRAM: stGet
 * 
 * 
 * get storage from general storage or system heap Usage stget is used
 * throughout PROGRESS to allocate additional storage
 * 
 * NOTE: stget clears the allocated storage, this is cpu time
 * expensive, if possible use the faster "stgetd" that allocates
 * "dirty" storage.
 * 
 *       If no storage is available, stget FATALs
 * 
 * Returns: pointer to the beginning of new storage 
 *
  RETURNS:
 */
TEXT *
stGet(dsmContext_t *pcontext, 
      stpool_t	   *pstpool,	/* pointer to storage pool */
      unsigned     len)		/* size of requested storage */
{
 	TEXT	 *pret;

    pret = stGetDirty(pcontext, pstpool, len, 1); /* allocate it "dirty" */
    if (!(pstpool->poolflgs & CLEARPOOL)) /* if it's not a clear pool, clear */
        stnclr(pret, len);		 /* clear the allocated stg */
    return pret;

}  /* end stGet */


/* PROGRAM: stGetIt - same as stGet but handles fatals conditionally 
 *
 * RETURNS: see stGet
 */
TEXT *
stGetIt(
        dsmContext_t *pcontext, 
        stpool_t     *pstpool, /* pointer to storage pool */
        unsigned      len,     /* size of requested storage */
        GBOOL          st_fatal)
{
 	TEXT	 *pret;

    /* allocate it "dirty" */
    pret = stGetDirty(pcontext, pstpool, len, st_fatal);
    if (!(pstpool->poolflgs & CLEARPOOL)) /* if it's not a clear pool, clear */
        stnclr(pret, len);		 /* clear the allocated stg */
    return pret;
}

/* PROGRAM: stGetd
 * 
 * 
 * get "dirty" storage from general storage or system heap Usage
 * stGetd is used to allocate additional storage as with stGet, but
 * when the allocated area does not have to be cleared. This is a
 * faster choice than stGet.
 * 
 *       If no storage is available, stGetd FATALs
 * 
 * Returns: pointer to the beginning of new storage 
 *
 * RETURNS:
 */
TEXT *
stGetd(dsmContext_t *pcontext, 
       stpool_t	    *pstpool,	/* pointer to storage pool */
       unsigned     len)        /* size of requested storage */
{
    return (stGetDirty(pcontext, pstpool, len, 1));

}  /* end stGetd */


/* PROGRAM: stGetDirty - same as stGetd but handles conditional error processing
 *
 * RETURNS: see stGetd
 */
TEXT *
stGetDirty(
        dsmContext_t *pcontext,
        stpool_t     *pstpool,   /* pointer to storage pool */
        unsigned      len,        /* size of requested storage */
        GBOOL          st_fatal)
{
    TEXT	 *pret;
    abk_t	 *ptmpabk;

    PL_LOCK_STPOOL(&pstpool->poolLatch)

#ifdef ST_PURIFY
    if (pstpool && !pstpool->pshmdsc)
    {
        pret = (stmMallocPUR(pcontext, pstpool, len, st_fatal));
        goto done;
    }
#endif /* ST_PURIFY */

    len = STROUND8(len); /* round to allignment # */

    if (pstpool->pshmdsc && pcontext->pusrctl)
        MT_LOCK_GST ();

    /* if general pool and the length > ABKSIZE/2 */
    if (pstpool == pdsmStoragePool && len > ABKSIZE/2)
    {
	if ((pret = utmalloc(len)) == PNULL)
            goto badExit;

        if (pstpool->pshmdsc && pcontext->pusrctl)
            MT_UNLK_GST ();
	goto done;
    }

    /*look for a block with enough space*/
    ptmpabk = stmGetbk(pcontext, pstpool, len);
    if (ptmpabk == NULL)
        goto badExit;

    if(pstpool->pshmdsc)
    {
	pret = XTEXT(pcontext->pdbcontext, ptmpabk->ua.qtr);
	ptmpabk->ua.qtr = P_TO_QP(pcontext, pret + len);
    }
    else
    {
	pret = ptmpabk->ua.ptr;
	ptmpabk->ua.ptr += len;
    }
    ptmpabk->rmleft -= len;

    if (pstpool->pshmdsc && pcontext->pusrctl)
        MT_UNLK_GST ();

#if STERILIZE
    if (!(pstpool->poolflgs & CLEARPOOL))
        do_sterilize(pret, 0xFF, len);
#endif

done:
    PL_UNLK_STPOOL(&pstpool->poolLatch)
    return pret;

badExit:
    PL_UNLK_STPOOL(&pstpool->poolLatch)
    if (pstpool->pshmdsc && pcontext->pusrctl)
        MT_UNLK_GST ();
    if (st_fatal)
        stFatal(pcontext);
    return NULL;   /* we actually never return from stFatal */

}  /* end stGetDirty */


/* PROGRAM: stFree
 * 
 * 
 *  to free entire storage pool stFree should not be used to free
 *  the general pool
 *
 * RETURNS:
 */
DSMVOID
stFree(
        dsmContext_t *pcontext,
        stpool_t    *pstpool)   /* pointer to storage pool */
{
    abk_t    *ptmpabk;
    abk_t     *pnxabk;

    /* dont free pools in shared memory */
    if (pstpool->pshmdsc)
        return;

#ifdef ST_PURIFY
    stmFreeAllPUR(pcontext, pstpool);
    return;
#endif /* ST_PURIFY */

    PL_LOCK_STPOOL(&pstpool->poolLatch)

    for (ptmpabk = pstpool->uhabk.pabk; ptmpabk != NULL; ptmpabk = pnxabk)
    {
	pnxabk = ptmpabk->unabk.pabk;
	utfree((TEXT *) ptmpabk);
    }

    for (ptmpabk = pstpool->uxabk.pabk; ptmpabk != NULL; ptmpabk = pnxabk)
    {
	pnxabk = ptmpabk->unabk.pabk;
	utfree((TEXT *) ptmpabk);
    }

    pstpool->uhabk.pabk = NULL;
    pstpool->utabk.pabk = NULL;
    pstpool->uxabk.pabk = NULL;
    pstpool->free.next.pren = NULL;
    pstpool->free.next.qren = 0;

#ifdef ST_MEMCHK
    stmRemovePoolPtr(pcontext, pstpool);
#endif

    PL_UNLK_STPOOL(&pstpool->poolLatch)

}  /* end stFree */



/* PROGRAM: stmInitPool
 * 
 * 
 * routine to intialize a pool and optionally set the bits for the
 * pool 
 *
 * RETURNS:
 */
DSMVOID
stmInitPool(
        dsmContext_t *pcontext,
        stpool_t *pstpool,
        TTINY     bits,
        BYTES   abksize _UNUSED_)
{
    stnclr((TEXT *)pstpool, sizeof(stpool_t));

    PL_LOCK_STPOOL(&pstpool->poolLatch)

#ifdef ST_MEMCHK
    stmAddPoolPtr(pcontext, pstpool);
#endif

    pstpool->poolflgs |= bits;

#ifdef ST_PURIFY
    /* if using purify, don't use optimal way of getting clear storage */
    pstpool->poolflgs &= ~CLEARPOOL;
#endif
    PL_UNLK_STPOOL(&pstpool->poolLatch)

}  /* end stmInitPool */


/* PROGRAM: stRent
 * 
 *  to allocate (cleared) temporary storage and later free it with
 *  stVacate
 * 
 * Usage	like stRentd, but with clear storage - see stRentd for details.
 * 
 * NOTE: the clear storage is cpu time expensive,
 *       if possible use the faster "stRentd" that allocates "dirty" storage.
 * 
 *       If no storage is available, stRent FATALs
 * 
 * Returns: pointer to the beginning of new storage 
 *
 * RETURNS:
 */
TEXT *
stRent(dsmContext_t *pcontext, 
       stpool_t	    *pstpool,	/* pointer to storage pool */
       unsigned     len)        /* size of requested storage */
{
 	TEXT	 *pret;

    pret = stRentd(pcontext, pstpool, len); /* allocate it "dirty" */
    if (!(pstpool->poolflgs & CLEARPOOL)) /* if it's not a clear pool, clear */
        stnclr(pret, len);
    return pret;
}

/* PROGRAM: stRentIt - same as stRent by has fatal processing flag
 *
 * RETURNS: see stRent
 */
TEXT *
stRentIt(dsmContext_t *pcontext, 
       stpool_t	    *pstpool,	/* pointer to storage pool */
       unsigned     len,        /* size of requested storage */
       GBOOL         st_fatal)
{
 	TEXT	 *pret;

    /* allocate it "dirty" */
    pret = stRentDirty(pcontext, pstpool, len, st_fatal);

    if (!(pstpool->poolflgs & CLEARPOOL)) /* if it's not a clear pool, clear */
        stnclr(pret, len);
    return pret;

}  /* end stRentIt */


/* PROGRAM: stRentd
 * 
 * 
 * - to allocate temporary storage and later free it with stVacate
 * 
 * Usage:
 * stRent is used to allocate temporary storage in an existing storage pool.
 * It should be used when the storage may be freed before the whole
 * pool is removed, so it may be used and reused many times.
 * Free it with stVacate.
 * 
 * A chain of free areas is maintained, its anchor is freest in the respective
 * pool structure stpool_t. The chain is in order of locations.
 * stRent tries first
 * to find best fit area in the free chain. Best fit is area < 112.5% of needed
 * space, or if not found - the smallest to be big enough.
 * 
 * If found in the chain, only the requested length is allocated,
 * the rest remains as a smaller piece in the free chain.
 * 
 * Only if non is found in the free chain, a new area is allocated in one of
 * the existing blocks of the respective pool. A new block may be added to
 * the pool when needed.
 * 
 * The allocated space is taken from the bottom of the free space in the
 * pool block, unlike "stGet" that allocates from the top. This is to
 * prevent rented area to mix with permanently allocated areas.
 * 
 * RETURNS: pointer to the beginning of new storage 
 */
TEXT *
stRentd(dsmContext_t *pcontext, 
	stpool_t     *pstpool,	    /* pointer to storage pool */
 	unsigned     len)           /* size of requested storage */
{
    return stRentDirty(pcontext, pstpool, len, 1);

}  /* end stRentd */


/* PROGRAM: stRentDirty - stRentd with the st_fatal flag 
 *
 * RETURNS: see stRentd
 */
TEXT *
stRentDirty(
        dsmContext_t *pcontext, 
        stpool_t     *pstpool,	    /* pointer to storage pool */
        unsigned     len,           /* size of requested storage */
        GBOOL         st_fatal)      /* determines error processing */
{

 	RENTST	 *parea;	/* pointer to allocated area */
 	RENTST	 *pbest;	/* pointer to best fit area */
 	RENTST	 *prev;		/* pointer to previous area */
 	RENTPRFX *phdr;		/* pointer to rented area prefix */
 	unsigned  alen;		/* area length */
 	unsigned  bestlen=0;	/* length of best fit area */
	unsigned  oklen;	/* 112.5% of needed length */
	RENTST	 *prevbest=0;	/* pointer to previous of best area */
 	abk_t	 *ptmpabk;
        RENTPRFX *phdr1;	/* pointer to rented area prefix */

    /* The 32K limit has been removed.  The new limit is
     * 0x7fffffff - prefix or about gig.  The signed value
     * is preserved because of stnclr functions requires a
     * signed int.
     */
    if (len > 0x7fffffff - STROUND(PRFXSIZE+1) - STROUND(1))
        FATAL_MSGN_CALLBACK(pcontext, smFTL006);

    /* allocated space must not be < sizeof(RENTST) */
    if (len < sizeof(RENTST))
        len = sizeof(RENTST);

    PL_LOCK_STPOOL(&pstpool->poolLatch)

#ifdef ST_PURIFY
    if (pstpool && !pstpool->pshmdsc)
    {
        parea = (RENTST *)stmMallocPUR(pcontext, pstpool, len, st_fatal);
        goto done;
    }
#elif defined(ST_MEMCHK)
    stmAddPoolPtr(pcontext, pstpool);
#endif /* ST_PURIFY */

    /* additional length for the RENTPRFX  and ENDMAGIC*/
    len = len + PRFXSIZE + 1;
#if 1  /* we would like to make each allocation a multiple of double 
        * so the addresses come out properly aligned.
        */
    len = STROUND8(len); /* round to allignment #*/
#else
    len = STROUND(len); /* round to allignment #*/
#endif

    /* if general pool and the length > ABKSIZE/2, use malloc directly */
    if (pstpool == pdsmStoragePool && len > ABKSIZE/2)
    {
	if ((phdr = (RENTPRFX *)utmalloc(len)) == NULL)
            goto badExit;
	phdr->rentlen = len;
	phdr->abkoffst = 0;  /* this stg is not in an abk_t*/
	((TEXT *)phdr)[len-1] = ENDMAG1;
	/* point past the header */
	parea = (RENTST *)((TEXT*)phdr + PRFXSIZE);
        pbest = parea;
	goto out;
    }

     oklen = STROUND(len + (len >> 3)); /* OK length, has 12.5% more than needed */

    if (pstpool->pshmdsc)
        MT_LOCK_GST ();

    /* scan the free chain to find best fit */
    /* the chain is ordered by location of entries */
     pbest = NULL;

     for(prev = (RENTST *)&(pstpool->free),
	 parea = (pstpool->pshmdsc) ? XRENTST(pcontext->pdbcontext, pstpool->free.next.qren)
				    : pstpool->free.next.pren;
	 parea;
	 prev = parea,
	 parea = (pstpool->pshmdsc) ? XRENTST(pcontext->pdbcontext, parea->next.qren)
				    : parea->next.pren)
     {
	 phdr = (RENTPRFX*)((TEXT*)parea - PRFXSIZE);
	 alen = phdr->rentlen; /*length of the area */
	 /* alen==1 is dummy free cell in abk_theader */
	 FASSERT_CB(pcontext, alen==1 || ((TEXT *)phdr)[alen-1]==VACMAGIC,
		 "stRent: bad VACMAGIC in chain");
	 if (len > alen) continue; /* too small */
	 if (pbest==NULL || bestlen > alen)
	 {
	     bestlen = alen;
	     pbest = parea;
	     prevbest = prev;
	     if (alen <= oklen) break;/* not best but good enough < 112.5%*/
	 }
     }

     if (pbest)
     {
	/* found best fit (or good enough fit) in the chain */
	if (bestlen < len + PRFXSIZE + sizeof(RENTST) + 1)
	{   /* the area is not big enough to be split
	    just remove it from the chain */
	    prevbest->next = pbest->next;
	    phdr = (RENTPRFX*)((TEXT*)pbest - PRFXSIZE);
	    len = phdr->rentlen;
	}
	else
	{   /* split the best fit area,
	    allocate the requested size, leave the rest in the chain */
	    phdr = (RENTPRFX*)((TEXT*)pbest - PRFXSIZE);
	    phdr->rentlen = len; /* new length of the area */

	    /* point to the remaining area */
	    parea =(RENTST *)((TEXT*)pbest + len);
	    phdr1 = (RENTPRFX*)((TEXT*)parea - PRFXSIZE);
	    phdr1->rentlen = bestlen - len; /*remaining len*/
	    if (pstpool->pshmdsc)
		 phdr1->abkoffst = 0;
	    else phdr1->abkoffst = phdr->abkoffst + len;

	    /* chain the remaining area */
	    parea->next = pbest->next;
	    if(pstpool->pshmdsc)
		 prevbest->next.qren = P_TO_QP(pcontext, parea);
	    else prevbest->next.pren = parea;
	}
	parea = pbest;
     }
     else
     {	/* get new area from the pool */

	/*look for a block with enough space*/
	ptmpabk = stmGetbk(pcontext, pstpool, len);
	if (ptmpabk == NULL)
	{
	    if (pstpool->pshmdsc)
                MT_UNLK_GST ();
            goto badExit;
	}

	/* the first time any stg is rented from an abk_tblock, */
	/* put the abk_tdummy free header on the free chain     */
	if (ptmpabk->dummyhdr.rentlen == 0)
	    stmInitDummyHdr(pcontext, pstpool, ptmpabk);

	/* allocate the area at the end of the free space
	unlike stGet that allocates at the beginning of the free space*/
        ptmpabk->rmleft -= len;

	if(pstpool->pshmdsc)
	    phdr = (RENTPRFX *)(XTEXT(pcontext->pdbcontext, ptmpabk->ua.qtr) + ptmpabk->rmleft);
	else
	    phdr = (RENTPRFX *)((TEXT*)(ptmpabk->ua.ptr) + ptmpabk->rmleft);

	phdr->rentlen = len;
	if (pstpool->pshmdsc)
	     phdr->abkoffst = 0;
	else phdr->abkoffst = (TEXT *)phdr - (TEXT *)ptmpabk;

	/* point past the header */
	parea = (RENTST *)((TEXT*)phdr + PRFXSIZE);
     }
    ((TEXT *)phdr)[phdr->rentlen-1] = pstpool->pshmdsc ? ENDMAG2 : ENDMAG3 ;

out:

    if (pstpool->pshmdsc)
        MT_UNLK_GST ();

    if (pstpool->poolflgs & CLEARPOOL) /* supposed to be a clean pool */
    {
        if (pbest) /* re-allocated or newly malloc'd chunk */
            stnclr(parea, len - PRFXSIZE - 1);
        /* else, it's clean */
    }
#if STERILIZE
    else
        do_sterilize((TEXT*)parea, 0xFF, len - PRFXSIZE - 1);
#endif

#ifdef ST_PURIFY
done:
#endif
    PL_UNLK_STPOOL(&pstpool->poolLatch)
    return (TEXT *)parea;

badExit:
    PL_UNLK_STPOOL(&pstpool->poolLatch)
    if (st_fatal)
        stFatal(pcontext);
    return PNULL;

}  /* end stRentDirty */


/* PROGRAM: stVacate
 * 
 * 
 * to vacate (free) temporary storage that was allocated by stRent.
 * 
 * Usage:
 *     stVacate is used to free storage that was allocated with stRent.
 *     This will make it available for reuse by stRent.
 * !!!! Only area that was allocated with stRent may be freed with stVacate.!!!!
 * 
 * Parameters:
 *     pstpool - If the storage was stRent ed from a shared memory pool, or
 *     from pdsmStoragePool, then the caller must pass the same pool pointer
 *     If the storage was allocated from any other pool, the caller
 *     may pass PNULL, or the orignial storage pool pointer.
 * 
 *     parea   - the pointer to the area returned by stRent()
 * 
 * Algorithm:
 *     A chain of free areas freed by stVacate is maintained, its anchor is in
 *     the respective pool structure stpool_t. The free areas in each
 *     abk_tare chained together in address order, but the order of
 *     the abk_tstructures is arbitrary.
 * 
 *     Each abk_tstructure contributes a dummy free header which is chained 
 *     as the first free area in that ABK.  This helps speed up adding new
 *     areas to the free chain.
 * 
 *     neighboring areas are combined together if possible to become
 *     larger areas.  
 *
 * RETURNS:
 */
DSMVOID
stVacate(dsmContext_t *pcontext, 
	 stpool_t     *pstpool,	    /* pointer to storage pool */
         TEXT	      *parea)	    /* pointer to the allocated area */

{
	RENTPRFX	*phdr;	/* points to header prefix on allocated area */
	int		 magic; /* magic byte at end of storage area         */
	abk_t		*pabk;


#ifdef ST_PURIFY
    if (pstpool == NULL  || (pstpool && !pstpool->pshmdsc))
        stmFreePUR (pcontext, pstpool, parea);
    return;
#endif /* ST_PURIFY */

    PL_LOCK_STPOOL(&pstpool->poolLatch)

    phdr = (RENTPRFX *)(parea - PRFXSIZE);
    magic = ((TEXT *)phdr)[phdr->rentlen-1];

    /* FIRST case: check for large areas allocated from pdsmStoragePool. They */
    /* were allocated with malloc and don't have an abk_tblock around them */
    if (   pstpool == pdsmStoragePool
	&& magic == ENDMAG1)
    {
	utfree( (TEXT *)phdr);
	goto done;
    }

    /* SECOND case: check for all other non-shared memory areas  */
    /* in this case, we can immediately get a pointer to the abk_t*/
    if (phdr->abkoffst)
    {
	/* sanity checks to be sure all is OK */
	pabk = (abk_t*)((TEXT *)phdr - phdr->abkoffst);
#if ASSERT_ON
	if (   magic != ENDMAG3
	    || pabk->abkmagic != ABKMAGIC
	    || (pstpool && pstpool != pabk->pstpool) )
	    goto stVacate_sanity_err;
#endif

	/* now that we have the pool pointer and the ABK, do the work */
	stmVacInBlock(pcontext, pabk->pstpool, pabk, phdr);
	goto done;
    }

    /* Must be a stVacate from the shared memory pool */
#if ASSERT_ON
    /* sanity checks */
    if (   pstpool == NULL
	|| pstpool->pshmdsc==NULL
	|| magic != ENDMAG2)
	goto stVacate_sanity_err;
#endif

    MT_LOCK_GST();

    /* search the abk_tchain for this pool to find the right abk_t*/
    for(pabk = pstpool->uhabk.qabk ? XABK(pcontext->pdbcontext,pstpool->uhabk.qabk)
				   : XABK(pcontext->pdbcontext,pstpool->uxabk.qabk);
	pabk;
	pabk = (pabk==XABK(pcontext->pdbcontext, pstpool->utabk.qabk))
                                 ? XABK(pcontext->pdbcontext, pstpool->uxabk.qabk)
                                 : XABK(pcontext->pdbcontext, pabk->unabk.qabk))
    {
	if ((TEXT *)phdr >= XTEXT(pcontext->pdbcontext, pabk->ua.qtr) + pabk->rmleft
	    && (TEXT *)phdr < pabk->area + (pabk->abksize-1))
	    break;
    }

#if ASSERT_ON
    /* if the storage isn't in any ABK, then someone is trying to free  */
    /* storage which was never allocated				*/
    if (pabk==NULL)
    {	MT_UNLK_GST();
        PL_UNLK_STPOOL(&pstpool->poolLatch)
	FATAL_MSGN_CALLBACK(pcontext,  smFTL009, pstpool, phdr);
    }
#endif

    /* now that we know which abk_tthe memory is in, vacate it */
    stmVacInBlock(pcontext, pstpool, pabk, phdr);
    MT_UNLK_GST();
    goto done;

#if ASSERT_ON
stVacate_sanity_err:
    MSGN_CALLBACK(pcontext, smMSG655);
    PL_UNLK_STPOOL(&pstpool->poolLatch)

    FATAL_MSGN_CALLBACK(pcontext, smFTL010,
                  pstpool, pdsmStoragePool, phdr, phdr->abkoffst, magic);
#endif

done:
    PL_UNLK_STPOOL(&pstpool->poolLatch);
    return;

}  /* end stVacate */


/* PROGRAM: stmVacInBlock
 * 
 * 
 * INTERNAL entry to vacate storage when the * stpool and pabk are
 * known
 *
 * RETURNS:
 */
LOCALF DSMVOID 
stmVacInBlock(dsmContext_t *pcontext, 
              stpool_t     *pstpool,
              abk_t        *pabk, 
              rentprfx_t   *phdr) 
{
	TEXT		*plast;	   /* points at last byte in abk_t*/
	RENTPRFX	*pnexthdr; /* points to next free past phdr */
	RENTPRFX	*pprevhdr; /* points to last free before phdr */
	int		 magic;

    pprevhdr = &pabk->dummyhdr;	/* first free area in blk */

    /* set plast to the last byte in the abk_tstructure */
    plast = pabk->area + (pabk->abksize-1);

    /* this loop looks for the last free area before phdr */
    /* and the first free area after phdr                 */
    /* 11-18-92 DC - The next header is the pointer to the "available space"
    ** + the "room left".  Set it here.
    */
    pnexthdr = (RENTPRFX *)
	    (  (pstpool->pshmdsc ? XTEXT(pcontext->pdbcontext, pabk->ua.qtr) 
                                 : pabk->ua.ptr)
	       + pabk->rmleft);
    for(;
	(TEXT *)pnexthdr-1 < plast;
	pnexthdr = (RENTPRFX *)((TEXT *)pnexthdr + pnexthdr->rentlen))
    {
#if ASSERT_ON
        if (pnexthdr->rentlen == 0)
        {
            PL_UNLK_STPOOL(&pstpool->poolLatch)
            FATAL_MSGN_CALLBACK(pcontext, smFTL011);
        }
	/* sanity check */
	if (   pnexthdr < phdr
	    && (TEXT *)pnexthdr + (pnexthdr->rentlen-1) >= (TEXT *)phdr)
	{
	    /* if you get here, you may consider compiling this with
	       -DSTDEBUG2 to use dbgvacate() to detect when an abk pool's
	       rented storage went south */
            PL_UNLK_STPOOL(&pstpool->poolLatch)
	    if (MTHOLDING(MTL_GST))
                MT_UNLK_GST();
	    FATAL_MSGN_CALLBACK(pcontext, smFTL012,
		pabk, pnexthdr, phdr);
	}
#endif
	/* VACMAGIC means that it is a free block */
	magic = ((TEXT *)pnexthdr)[pnexthdr->rentlen-1];
	if (magic == VACMAGIC)
	{
	    /* is this one the last one before phdr so far */
	    if (pnexthdr < phdr) pprevhdr = pnexthdr;
	    else break;  /* must be first free one past phdr */
	}
#if ASSERT_ON
	else
	if (magic<ENDMAG1 || magic>ENDMAG3)
	{
	    /* if you get here, you may consider compiling this with
	       -DSTDEBUG2 to use dbgvacate() to detect when an abk pool's
	       rented storage went south */
            PL_UNLK_STPOOL(&pstpool->poolLatch)
	    if (MTHOLDING(MTL_GST))
                MT_UNLK_GST();
	    FATAL_MSGN_CALLBACK(pcontext, smFTL013,
		pabk, pnexthdr, phdr);
	}
#endif
    }

    /* at this point, pprevhdr points at the last one before phdr	    */
    /* pnexthdr either points at the first one after, or off the end of abk_t*/

#if STERILIZE
    do_sterilize(((TEXT *)phdr)+PRFXSIZE, 0xFF, (int)phdr->rentlen - PRFXSIZE);
#endif

    /* make the parameter storage block look like vacated storage */
    ((TEXT *)phdr)[phdr->rentlen-1] = VACMAGIC;

    /* try to join together with the next neighbor */
    if (   (TEXT *)pnexthdr-1 != plast
	&& (TEXT *)phdr + phdr->rentlen == (TEXT *)pnexthdr)
    {
	phdr->rentlen += pnexthdr->rentlen;
	((RENTST *)(phdr+1))->next = ((RENTST *)(pnexthdr+1))->next;
    }
    else
	((RENTST *)(phdr+1))->next = ((RENTST *)(pprevhdr+1))->next;

    /* try to join together with the prev neighbor */
    if (   pprevhdr != &pabk->dummyhdr
	&& (TEXT *)pprevhdr + pprevhdr->rentlen == (TEXT *)phdr)
    {
	pprevhdr->rentlen += phdr->rentlen;
	((RENTST *)(pprevhdr+1))->next = ((RENTST *)(phdr+1))->next;
    }
    else
    {
	if (pstpool->pshmdsc)
	     ((RENTST *)(pprevhdr+1))->next.qren = 
                                   (QRENTST)P_TO_QP(pcontext, (TEXT *)(phdr+1));
	else ((RENTST *)(pprevhdr+1))->next.pren = (RENTST *)(phdr+1);
    }

    return;

}  /* end stmVacInBlock */


/* PROGRAM: stmInitDummyHdr
 * 
 * 
 * INTERNAL procedure to initialize the   
 *	    dummy free entry in the header of an abk_t the first time 
 *	    any storage is rented in it 			    
 *
 * RETURNS:
 */
LOCALF DSMVOID
stmInitDummyHdr(dsmContext_t *pcontext, 
	        stpool_t     *pstpool,   /* the stpool_t the abk_tbelongs to*/
	        abk_t	     *pabk)	 /* the abk_tto be initialized    */
{
    pabk->dummyhdr.rentlen = 1;	/* flag that this init was done */
    pabk->free = pstpool->free;
    if (pstpool->pshmdsc)
	 pstpool->free.next.qren = P_TO_QP(pcontext, &pabk->free);
    else pstpool->free.next.pren = &pabk->free;
}


/* PROGRAM: stmGetbk
 * 
 * 
 * get a new block for the respective storage pool.
 * Usage	stmGetbk is used by stGet and stRent to get a new block
 * 	for the pool, when there is not enough space for the allocation.
 * 
 * Returns: pointer to the beginning of new block
 */
LOCALF abk_t*
stmGetbk(dsmContext_t *pcontext, 
        stpool_t     *pstpool,    /* pointer to storage pool */
        unsigned     len)	  /* size of requested storage */

{
 	abk_t	 *ptmpabk;
 	unsigned  alen;

    /*look for a block with enough space*/
    if(pstpool->pshmdsc)
    {
    	for(ptmpabk=XABK(pcontext->pdbcontext, pstpool->uhabk.qabk);
            ptmpabk != NULL;
	    ptmpabk=XABK(pcontext->pdbcontext, ptmpabk->unabk.qabk))
	{
	    FASSERT_CB(pcontext, ptmpabk->abkmagic == ABKMAGIC,
		    "stmGetbk: abk_tfound with bad abkmagic");
	    if (ptmpabk->rmleft >= len) return ptmpabk;
	}
    }
    else
    {
	for(ptmpabk=pstpool->uhabk.pabk; ptmpabk != NULL;
	    ptmpabk=ptmpabk->unabk.pabk)
	{
	    FASSERT_CB(pcontext, ptmpabk->abkmagic == ABKMAGIC,
		    "stmGetbk: abk_tfound with bad abkmagic");
	    if (ptmpabk->rmleft >= len)
		return ptmpabk;
	}
    }

    /* no available blocks, create a new one */
    if (pstpool->pshmdsc) /* if shared memory pool */
    {
	if (pcontext->pdbcontext->psemdsc && 
           (semValue(pcontext, pcontext->pdbcontext->psemdsc->semid, RDYSEM) != 1))
        {
            PL_UNLK_STPOOL(&pstpool->poolLatch)
	    /* shared memory requested not during init */
	    FATAL_MSGN_CALLBACK(pcontext, smFTL007);
        }

#if ALPHAOSF
        /* The old value in pstpool->pshmdsc->segsize may be huge (1Gb),
                so, knock it down to 16Mb if it is to save some memory.  */
        if (pstpool->pshmdsc->segsize > 16*1024*1024 &&
            len < 16*1024*1024)
                pstpool->pshmdsc->segsize = 16*1024*1024;
#endif
 
	ptmpabk = (abk_t*)shmFormatSegment(pcontext, pstpool->pshmdsc);
	if (ptmpabk==NULL)
	{
            PL_UNLK_STPOOL(&pstpool->poolLatch)
	    /* Unable to allocate enough shared memory */
	    MSGN_CALLBACK(pcontext, smMSG652);
	    dbExit(pcontext, 0, DB_EXBAD); /*do not dump core, nor give SYSERR */
	}

	if (len > ptmpabk->rmleft)
        {
            PL_UNLK_STPOOL(&pstpool->poolLatch)
	    /* stGet length greater than max shm segment length */
	    FATAL_MSGN_CALLBACK(pcontext, smFTL008);
        }
    }
    else
    {
	/* malloc and fatal if failed */
	alen = sizeof(abk_t);
	if (len > ABKSIZE) alen += len - ABKSIZE;
	if ((ptmpabk = (abk_t*) utmalloc(alen)) == NULL)
	    return (abk_t*)NULL;
	alen = MAX(len, ABKSIZE);
	stIntabk(pcontext, ptmpabk,pstpool,alen); /* initialize the new block */

	if (len==ptmpabk->rmleft)  /* the block wont have any free room */
	     stLnkx(pcontext, pstpool, ptmpabk); /* so put it on dead chain */
	else stLnk(pcontext, pstpool, ptmpabk); /* else put it on live chain */
    }


    return ptmpabk;
}

/* PROGRAM: stIntabk
 * 
 * 
 * initialize a new ABK
 *
 * RETURNS:
 */
DSMVOID
stIntabk(dsmContext_t *pcontext, 
         abk_t	      *pabk,
         stpool_t     *pstpool,	    /* pointer to storage pool */
	 unsigned     size)
{
    pabk->rmleft =
    pabk->abksize = size;
    pabk->free.next.qren = 0;
    pabk->free.next.pren = NULL;
    pabk->dummyhdr.rentlen = 0;
    pabk->abkmagic = ABKMAGIC;

    if(pstpool->pshmdsc)
    {
	pabk->ua.qtr = P_TO_QP(pcontext, pabk->area);
	pabk->unabk.qabk = 0;
	pabk->pstpool = NULL;  /* not used in shared memory */
    }
    else
    {
	pabk->ua.ptr = pabk->area;
	pabk->unabk.pabk = NULL;
	pabk->pstpool = pstpool;
    }
    if (pstpool->poolflgs & CLEARPOOL)
        stnclr((TEXT *)pabk->area, (int)size);
#if STERILIZE
    else
        do_sterilize((TEXT *)pabk->area, 0xFF, (int)size);
#endif
}

/* PROGRAM: stLnk
 * 
 * 
 * link an abk_t to an stpool_t 
 *
 * RETURNS:
 */
DSMVOID
stLnk(dsmContext_t *pcontext, 
      stpool_t	   *pstpool,
      abk_t	   *pabk)
{
	QABK	 qabk;

    /*chain new block into stpool*/
    if(pstpool->pshmdsc)
    {
	qabk = P_TO_QP(pcontext, pabk);
	if (pstpool->uhabk.qabk == 0)
	{   pstpool->uhabk.qabk = qabk;
	    pstpool->utabk.qabk = qabk;
	}
	else
	{   XABK(pcontext->pdbcontext, pstpool->utabk.qabk)->unabk.qabk = qabk;
	    pstpool->utabk.qabk = qabk;
	}
    }
    else
    {
	if (pstpool->uhabk.pabk == NULL)
	{   pstpool->uhabk.pabk = pabk;
	    pstpool->utabk.pabk = pabk;
	}
	else
	{   pstpool->utabk.pabk->unabk.pabk = pabk;
	    pstpool->utabk.pabk = pabk;
	}
    }
}

/* PROGRAM: stLnkx
 * 
 * 
 * link a full abk_t to the stpool_t dead chain 
 *
 * RETURNS:
 */
DSMVOID
stLnkx(dsmContext_t *pcontext, 
       stpool_t	    *pstpool,
       abk_t	    *pabk)
{
    pabk->unabk = pstpool->uxabk;
    if(pstpool->pshmdsc)
	 pstpool->uxabk.qabk = P_TO_QP(pcontext, pabk);
    else pstpool->uxabk.pabk = pabk;
}

/* PROGRAM: stFatal
 * 
 * 
 * generate a fatal error message for insufficient stg 
 *
 * RETURNS:
 */
DSMVOID
stFatal(dsmContext_t *pcontext)
{
    /* if -hs parameter is enabled and self-serving client */
    if(pcontext->pdbcontext->argheapsize)
	/* stGet: out of storage: the current -hs parmeter is %d */
	MSGN_CALLBACK(pcontext, smMSG653, pcontext->pdbcontext->argheapsize);
    else
	MSGN_CALLBACK(pcontext, smMSG654); /* out of storage */

    dbExit(pcontext, 0, DB_EXBAD); /*do not dump core, nor give SYSERR */
}


/**** THE FOLLOWING FUNCTIONS ARE EXCLUSIVELY FOR PURIFY AND MEMCHECK ****/

#ifdef ST_MEMCHK
LOCALF int
stmEnableChecking(
        dsmContext_t *pcontext,
        int           flag)
{
    poolChecking = flag;
    return poolChecking;

}  /* end stmEnableChecking */

#ifdef ST_PURIFY
/* PROGRAM: stmFindPoolPtr
 *
 * DESCRIPTION: Finds a storage pool pointer
 *
 * HISTORY: Create 12-27-92 DC
 *
 *
 * RETURNS:
 */
LOCALF POOLPTRS *
stmFindPoolPtr(
        dsmContext_t *pcontext,
        stpool_t     *pstpool,
        TEXT         *parea)
{
    int i;

    if (pstpool == NULL)
        pstpool  = stmGetPoolPtr(pcontext, parea);
    if (pstpool != NULL)
    {
        for (i = 0; (i < poolsAllocated); i ++)
            if ((unsigned_long)poolList[i].pstpool == (unsigned_long)pstpool)
                return(&poolList[i]);
    }
    return (NULL);

}  /* end stmFindPoolPtr */


/* PROGRAM: stmFreeAllPUR
 *
 *
 * RETURN: DSMVOID
 */
LOCALF DSMVOID
stmFreeAllPUR(
        dsmContext_t *pcontext,
        stpool_t     *pstpool)   /* pointer to storage pool */
{
    POOLPTRS        *ppools;        /* pointer to list of pools*/
    int             i = 0;
 
    ppools = stmFindPoolPtr(pcontext, pstpool, PNULL);
    if (ppools == NULL) return;
 
    for (i = 0; (i < ppools->numAllocated); i++)
    if ((unsigned_long)(ppools->pchain[i]) != (unsigned_long)NULL)
    {
        free(ppools->pchain[i]);
        ppools->pchain[i] = PNULL;
    }
 
    stmRemovePoolPtr(pcontext, pstpool);

}  /* end stmFreeAllPUR */


/* PROGRAM: stmMallocPUR 
 *
 *
 * RETURN:
 */
LOCALF TEXT *
stmMallocPUR(
        dsmContext_t *pcontext, 
        stpool_t     *pstpool,	  /* pointer to storage pool */
        unsigned      len,	  /* size of requested storage */
        GBOOL          st_fatal)
{
    TEXT            *parea;         /* pointer to rented area*/
    POOLPTRS        *ppools;        /* pointer to list of pools*/
    int             i = 0;
 
    ppools = stmAddPoolPtr(pcontext, pstpool);
    if (len == 0) len = 1;
 
    if ((parea = (TEXT *)malloc(len)) == NULL)
    {
        MSGD_CALLBACK(pcontext, "stmMallocPUR ERROR: malloc failed");
        return PNULL;
    }
 
#if STERILIZE
    do_sterilize((TEXT*)parea, 0xFF, len);
#endif
 
    if (ppools == NULL) return parea;
 
    for (i = 0; (i < ST_CHAIN_SIZE); i++)
    if ((unsigned_long)(ppools->pchain[i]) == (unsigned_long)NULL)
    {
        if (i >= ppools->numAllocated)
                ppools->numAllocated = i + 1;
        ppools->pchain[i] = parea;
        return (parea);
    }
 
    MSGD_CALLBACK(pcontext, "stmMallocPUR ERROR: full chain");
    return (parea);

}  /* end stmMallocPUR */


/* PROGRAM: stmFreePUR
 *
 *
 * RETURN:
 */
LOCALF DSMVOID
stmFreePUR(
        dsmContext_t *pcontext,
        stpool_t     *pstpool,	/* pointer to storage pool */
        TEXT	     *parea)	/* pointer to the allocated area */

{
    POOLPTRS        *ppools;        /* pointer to list of pools*/
    int             i = 0;
 
    ppools = stmFindPoolPtr(pcontext, pstpool, parea);
    if (ppools == NULL) return;
 
    for (i = 0; (i < ppools->numAllocated); i++)
    if ((unsigned_long)(ppools->pchain[i]) == (unsigned_long)parea)
    {
        free(ppools->pchain[i]);
        ppools->pchain[i] = PNULL;
        return;
    }
 
    MSGD_CALLBACK(pcontext, "stmFreePUR ERROR: parea not found");
    free(parea);

}  /* end stmFreePUR */


/* PROGRAM: stmGetPoolPtr
 * 
 * 
 * given a pointer to stRent storage, return its pstpool
 * Requires: st.h
 *    Returns:  pointer to the stpool the storage was rented in
 *    Restrictions (these could be removed with some effort if needed):
 *     the storage must not be in the general storage pool (pdsmStoragePool)
 *     the storage must not be in a shared memory pool
 *    The reasons for the restrictions are described in the header comment for
 *     the entrypoint stscanp()
 *
 * RETURNS:
 */
LOCALF stpool_t *
stmGetPoolPtr(
        dsmContext_t *pcontext,
        TEXT *parea)
{
    RENTPRFX        *phdr;
    ABK             *pabk;
    int              magic;
 
    int   i = 0;
    int   j = 0;
 
    if (parea == NULL)
    {
        MSGD_CALLBACK(pcontext, "stmGetPoolPtr ERROR: NULL area");
        return (NULL);
    }
 
    for (i = 0; (i < poolsAllocated); i ++)
    {
       if (poolList[i].pstpool == NULL) continue;
          for (j = 0; (j < poolList[i].numAllocated); j++)
             if ((unsigned_long)(poolList[i].pchain[j]) == (unsigned_long)parea)
                 return (poolList[i].pstpool);
    }
 
    MSGD_CALLBACK(pcontext, "stmGetPoolPtr ERROR: area not found");
    return (NULL);
 
}  /* end stmGetPoolPtr */


#endif /* ST_PURIFY */

/*
** stmAddPoolPtr()
**
** DESCRIPTION: builds a list of storage pool pointers
**
** HISTORY: Create 12-27-92 DC
**
*/
LOCALF POOLPTRS *
stmAddPoolPtr(
        dsmContext_t *pcontext,
        STPOOL       *pstpool)
{
    int i = 0;
#if ST_SHOW_MALLOC
    int msglen;
    char tempbuf[200];
#endif /* ST_SHOW_MALLOC */
 
#ifdef ST_PURIFY
    if (pstpool == NULL)
    {
        MSGD_CALLBACK(pcontext, "stmAddPoolPtr ERROR: NULL pool");
        return (NULL);
    }
#endif /* ST_PURIFY */
 
    if (poolList == NULL) /* first time */
    {
        int size = MAXPOOLPTR * sizeof(POOLPTRS);
 
#if ST_SHOW_MALLOC
        MSGD_CALLBACK(pcontext, "Initializing Pool List: size = %i", size);
#endif /* ST_SHOW_MALLOC */
 
        poolList = (POOLPTRS *) malloc(size);
        stnclr((TEXT *)poolList, size); /* initialize array */
    }
 
    for (i = 0; (i < poolsAllocated); i ++)
       if ((unsigned_long)poolList[i].pstpool == (unsigned_long)pstpool)
          return(&poolList[i]);
 
    /* we got through the above loop and we didn't find the pool in the
    ** chain, so add it.  Loop through again in order to reuse abandoned
    ** slots
    */
    for (i = 0; (i < MAXPOOLPTR); i ++)
    {
        if ((unsigned_long)poolList[i].pstpool == (unsigned_long)NULL)
        {
#ifdef ST_PURIFY
            int size = ST_CHAIN_SIZE*sizeof(TEXT*);
#endif /* ST_PURIFY */
 
            if (i >= poolsAllocated)
                poolsAllocated = i + 1;
            poolList[i].pstpool = pstpool;
 
#ifdef ST_PURIFY
            poolList[i].numAllocated = 0;
            poolList[i].pchain = (TEXT**) malloc(size);
#if ST_SHOW_MALLOC
            MSGD_CALLBACK(pcontext, "stAddpoolPtr: Allocated pool No. %i at %X",
                  poolsAllocated, poolList[i].pchain); /* pieter */
#endif /* ST_SHOW_MALLOC */
            stnclr((TEXT*)poolList[i].pchain, size);
#endif /* ST_PURIFY */
            return(&poolList[i]);
        }
    }
 
#ifdef ST_PURIFY
    MSGD_CALLBACK(pcontext, "stmAddPoolPtr ERROR: full pool list");
#endif /* ST_PURIFY */
    return (NULL);

}  /* end stmAddPoolPtr */


/*
** stmRemovePoolPtr()
**
** DESCRIPTION: removes a pool pointer from a list of storage pool pointers
**
** HISTORY: Create 12-27-92 DC
**
*/
LOCALF DSMVOID
stmRemovePoolPtr(
        dsmContext_t *pcontext,
        STPOOL       *pstpool)
{
   int i = 0;
 
#ifdef ST_PURIFY
    if (pstpool == NULL)
    {
        MSGD_CALLBACK(pcontext, "stmRemovePoolPtr ERROR: NULL pool");
        return;
    }
#endif /* ST_PURIFY */
 
    if (poolList == NULL) return;

    for (i = 0; (i < poolsAllocated); i ++)
    {
        if ((unsigned_long)poolList[i].pstpool == (unsigned_long)pstpool)
        {
            poolList[i].pstpool = NULL;
#ifdef ST_PURIFY
            free((TEXT*)poolList[i].pchain);
            poolList[i].pchain = NULL;
#endif /* ST_PURIFY */
            return;
        }
    }
 
#ifdef ST_PURIFY
    MSGD_CALLBACK(pcontext, "stmRemovePoolPtr ERROR: pool not found");
#endif /* ST_PURIFY */
 
}  /* end stmRemovePoolPtr */

#endif /* ST_MEMCHK */
