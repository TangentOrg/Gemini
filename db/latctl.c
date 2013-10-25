
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

#include "bkpub.h"
#include "dbcontext.h"
#include "dbmsg.h"
#include "dbutret.h"
#include "ditem.h"
#include "latpub.h"
#include "latprv.h"
#include "lkpub.h"
#include "lkschma.h"
#include "mtmsg.h"
#include "sempub.h"
#include "shmprv.h"
#include "stmpub.h"
#include "stmprv.h"
#include "umlkwait.h"
#include "usrctl.h"
#include "utcpub.h"
#include "utepub.h"        /* utgetpid() */
#include "utmpub.h"
#include "utmsg.h"
#include "uttpub.h"

#if OPSYS==UNIX 
#include <sys/time.h>
#include <sys/poll.h>      /* needed for poll() */
#endif /* UNIX */

#include <errno.h>

#if OPSYS==WIN32API
#ifndef _MAX_PATH
#define _MAX_PATH 260
#endif
#endif /* WIN32API */

/* Amount to inc/dec semaphores when waiting/waking for latches */

#define MTSEM_LAT_WAIT	-3
#define MTSEM_LAT_WAKE	3

/* Amount to inc/dec semaphores when waiting/waking for resource queues */

#define MTSEM_RSRC_WAIT	-1
#define MTSEM_RSRC_WAKE	1

/* Amounts to inc/dec semaphores when waiting/waking for record, schema,
   or task wait locks */

#define MTSEM_LOCK_WAIT	-1
#define MTSEM_LOCK_WAKE	1

/* return codes from latWaitOnSem */

#define MT_WAIT_OK		0
#define MT_WAIT_PENDING		1
#define MT_WAIT_CANCELLED	CTRLC

#define MT_LOCK_CHECK_TIME	5000 /* (ms) Lock check timeout */
#define MT_MAX_NAP_TIME		5000 /* (ms) Upper bound for spinlock sleeps */
#define MT_CPR_TIME		5   /* (secs) semaphore wait timeout */


/* data passed to lkBusy callback */

typedef struct
{
    dsmContext_t *pcontext;
    int		checkTime;	/* ms timeout for lkBusy callback */
    int		waitStatus;	/* current status of lock request */
} mtLockWaitInfo_t;

#if SEQUENT || SEQUENTPTX
/* for microsecond timer */
#include <usclkc.h>
#endif /* SEQUENT || SEQUENTPTX */

#if NAP_TYPE==NAP_POLL
#include <sys/poll.h>
#endif /* NAP_POLL */

/* LOCAL Function Protypes */
#if 0
LOCALF int  latChkLockWait(mtLockWaitInfo_t *pinfo,
                           TEXT **ppuname, 
                           TEXT **pptty, int mode); 
LOCALF int  latGetHolder(dsmContext_t *pcontext, int latchId);
#endif

LOCALF DSMVOID latPingSem		(dsmContext_t *pcontext, usrctl_t *pu,
				 int semcnt);
LOCALF DSMVOID latSleep		(dsmContext_t *pcontext, MTCTL *pmt,
				 int *pNapTime);
LOCALF DSMVOID latLockQSLatch	(dsmContext_t *pcontext, MTCTL *pmt,
				 usrctl_t *pu, latch_st *lp);
LOCALF DSMVOID latLockSpinLatch	(dsmContext_t *pcontext, MTCTL *pmt,
				 usrctl_t *pu, latch_st *lp);
LOCALF LONG latQXlock		(dsmContext_t *pcontext, MTCTL *pmt,
				 usrctl_t *pu, latch_st *lp);
LOCALF int  latWaitOnSem	(dsmContext_t *pcontext, int semcnt,
				 usrctl_t *pu, int waitCode);
LOCALF DSMVOID latWait		(dsmContext_t *pcontext, MTCTL *pmt,
				 usrctl_t *pu, latch_st *latchPtr);
LOCALF DSMVOID latWake		(dsmContext_t *pcontext, MTCTL *pmt,
				 latch_st *latchPtr,     usrctl_t *pu);
LOCALF DSMVOID latXlock		(dsmContext_t *pcontext, latch_st *lp);
LOCALF DSMVOID latXfree		(dsmContext_t *pcontext, latch_st *lp);
/*****************************************/

int latChkLockWaitOnIt(dsmContext_t *pcontext, int mode);
/*****************************************/

/* LOCAL Function Prototypes end */

/* PROGRAM: latalloc
 * Allocate/acquire the section of shared/private memory 
 * that will represent the latch structure.
 *
 * RETURNS:   None
 */
DSMVOID
latalloc(dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm = pdbcontext->pdbpub;

    if (pdbcontext->argnoshm)
    {
        pdbcontext->pmtctl = (MTCTL *)stGet(pcontext,
                             XSTPOOL(pdbcontext, pdbshm->qdbpool), sizeof(MTCTL));
    }
    else
    {
        /* Have to make sure this is aligned properly */

        pdbcontext->pmtctl =
                (MTCTL *)stGet(pcontext, XSTPOOL(pdbcontext,
                  pdbshm->qdbpool), sizeof(MTCTL) +
                (sizeof (muxlatch_t) * MTM_TOTAL) + MT_SPIN_ALIGN);

        pdbcontext->pmtctl =
            (MTCTL *)(((unsigned_long)pdbcontext->pmtctl + MT_SPIN_ALIGN-1)&
                            ~((unsigned_long)MT_SPIN_ALIGN-1));
    }
} /* end latalloc */

/* The rest of the program is SHARED MEMORY ONLY */

/* PROGRAM: latcanwait - cancel long wait for current user
 *
 * RETURNS:    None 
 */
DSMVOID
latcanwait (dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    usrctl_t *pusr;

    if (pdbcontext->argnoshm) return;

    if (pdbcontext->shmgone)
        dbExit(pcontext, 0, DB_EXGOOD); /* shm, broker are gone */

    if (pdbcontext->pdbpub->shutdn == DB_EXBAD)
    {
        if ((pdbcontext->usertype & (BROKER | WDOG)) == 0)
        {
            /* Fatal error. Time to bail out */

            dbExit(pcontext, 0, DB_EXBAD);
        }
    }

    pusr  = pcontext->pusrctl;
    if (pusr->uc_wait == 0)
        return;

    if (pusr->uc_wait <= (TEXT)TSKWAIT)
    {
	/* remove the long wait (record or schema lock, or
	   other user's transaction commit/rollback) */

        lkrmwaits (pcontext);
    }
    else
    {
	/* have to do the wait */

	latUsrWait (pcontext);
    }
} /* end latcanwait */


#if 0
/* PROGRAM: latChkLockWait - check status of a lockwait
 *
 * RETURNS:     int - LW_END_WAIT
 *                    LW_CONTINUE_WAIT
 *                    LW_STOP_WAIT
 */
LOCALF int
latChkLockWait (
	mtLockWaitInfo_t *pinfo,
        TEXT		**ppuname,
        TEXT		**pptty,
        int		 mode)
{
	int	ret;

    dsmContext_t *pcontext = pinfo->pcontext;

    ret = latChkLockWaitOnIt(pcontext, mode);
    if (ret == LW_STOP_WAIT)
        pinfo->waitStatus = MT_WAIT_CANCELLED;

    return (ret);

} /* end latChkLockWait */
#endif


/* PROGRAM: latfrlatch - free a latch, but save its depth counter so it
 *			can be reacquired by a leter call to latrelatch.
 *
 * RETURNS:	None
 */
DSMVOID
latfrlatch (
	dsmContext_t	*pcontext,
	int		 latchId,
        COUNT           *platchDepth)
{
	dbcontext_t	*pdbcontext = pcontext->pdbcontext;
	usrctl_t	*pusr;
	latch_st	*lp;

    if (pdbcontext->argnoshm)
    {
        *platchDepth = 0;
        return;
    }

    lp = &(pdbcontext->pmtctl->latches[latchId].l);

    /* we need to push the latch stack with one extra latch.  latXfree
       will end up poping off the latch, with will then pu the
       stack pointer in the wrong place 
    */

    pusr = pcontext->pusrctl;
    MT_PUSH_LOCK(pusr, latchId);

    pusr->uc_latfree++;

    /* release the latch. A subsequent call to latrelatch will restore the
       saved value of the depth counter. */

    *platchDepth = lp->latchDepth;
    lp->latchDepth = 0;

#if 0
    MSGD_CALLBACK(pcontext, (TEXT *)"%Llatfrlatch: latchid: %d savedDepth: %d ",
                       latchId, *platchDepth);
#endif

    latXfree (pcontext, lp);

} /* end latfrlatch */


/* PROGRAM: latfrmux - free a multiplexed latch 
 *
 * RETURNS:	None
 */
DSMVOID
latfrmux (
	dsmContext_t	*pcontext,
	int		 muxId,		/* id of muxctl */
	int		 muxNo)		/* number of mux element */
{
	dbcontext_t	*pdbcontext = pcontext->pdbcontext;
	usrctl_t	*pu;
	MTCTL		*pmt;
	latch_st	*lp;
	muxctl_t	*mcp;
	muxlatch_t	*mp;

    if (pdbcontext->argnoshm) return;
    pmt = pdbcontext->pmtctl;

    mcp = &(pmt->mt_muxctl[muxId]);
    lp = &(pmt->latches[mcp->latchId].l);
    if (mcp->nmuxes)
    {
        /* release the mux element */
        mp = &(pmt->mt_muxes[muxNo + mcp->base]);

        pu  = pcontext->pusrctl;
#if MTSANITYCHECK
        if (mp->holder != pu->uc_usrnum)
        {
	    pdbcontext->pdbpub->shutdn = DB_EXBAD;
	    FATAL_MSGN_CALLBACK(pcontext, mtFTL014, mcp->latchId);
	}
#endif
	mp->lock = 0;
   	MT_POP_LOCK (pu);
	return;
    }

    /* Not using muxes */
    latXfree (pcontext, lp);

    return;

} /* end latfrmux */


/* PROGRAM: latGetUsrCount - Update usrcnt in latch structure.
 * This function will retrieve, increment, or decrement the
 * value of usrcnt depending on the action parameter passed.
 *
 * RETURNS: COUNT - updated pmtctl->usrcnt value.
 */
COUNT
latGetUsrCount(
	dsmContext_t	*pcontext,
	int		 action)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;

    if (action == LATINCR)
        pdbcontext->pmtctl->usrcnt++;

    if (action == LATDECR)
        pdbcontext->pmtctl->usrcnt--;

    return(pdbcontext->pmtctl->usrcnt);

}


#if 0
/* PROGRAM: latGetHolder
 * Get the user number of the process holding a latch.
 * For use by monitor utility
 * 
 * Parameters:
 * latch number
 * 
 * RETURNS: int - 
 * 	-1:	no one holding latch
 * 	>= 0:	user number of latch holder
 */
LOCALF int
latGetHolder (
	dsmContext_t	*pcontext,
	int		 latchId)
{
        latch_st        *lp;

    if ((latchId < 0) || (latchId > MTL_MAX)) return (-1);

    lp = &(pcontext->pdbcontext->pmtctl->latches[latchId].l);
    switch (lp->ltype)
    {
    case MT_LT_SPIN:
    case MT_LT_SSPIN:
         if (! MT_IS_FREE (&(lp->fastLock))) return (lp->holder);
         break;

    case MT_LT_QUEUE:
    case MT_LT_SQUEUE:
        if (lp->slowLock) return (lp->holder);
        break;
    }
    return (-1);

} /* end latGetHolder */
#endif

/* PROGRAM: latGetStatMode - return current statistics mode setting
 * 
 *
 * RETURNS: int - pcontext->pdbcontext->pmtctl->mt_stats - stat setting   
 */
int
latGetStatMode (dsmContext_t *pcontext)
{

    return (pcontext->pdbcontext->pmtctl->mt_stats);

} /* end latGetStatMode */


/* PROGRAM: latlatch - request a nested latch
 * NOTE:
 * It is important for this function and its partner
 * latfrlatch to be as fast as possible. When using
 * spinlocks, we often spin several times before getting
 * the latch.
 * 
 * Latches are one of
 * 1) spinlocks implemented with a test-and-set loop, or
 * 2) queueing locks implemented with a lock bit and a
 * system semaphore. An auxiliary lock bit protects the
 * latch queue.
 * 3) multiplexed 2 level hierarchical spinlocks (see latlkmux)
 *
 * RETURNS:	None
 */
DSMVOID 
latlatch (
	dsmContext_t	*pcontext,
	int		 latchId) /* Id number of latch to get - see latpub.h */
{
	dbcontext_t	*pdbcontext = pcontext->pdbcontext;
	usrctl_t	*pu;
	MTCTL		*pmt;
	latch_st	*lp;

    if (pdbcontext->argnoshm) 
    	return;

    pmt = pdbcontext->pmtctl;
    lp = &(pmt->latches[latchId].l);

    pu  = pcontext->pusrctl;
    if ((pu->uc_latches & (1 << latchId)) &&
        lp->latchDepth )
    {
        /* we are already holding it so we just bump the counter */
        lp->latchDepth++;

#if 0
    MSGD_CALLBACK(pcontext,
              (TEXT *)"%Llatlatch: already holding latchid: %d latchDepth: %d ",
                       latchId, lp->latchDepth);
#endif
 
        return;
    }


#if MTSANITYCHECK
    MT_CHECK_STACK (pu, 0)
    if (pu->uc_latches & lp->mask)
    {
        /* Not allowed to go for this latch while holding these others,
           because deadlock will surely come of it */

	pdbcontext->pdbpub->shutdn = DB_EXBAD;
        FATAL_MSGN_CALLBACK(pcontext,  mtFTL007, latchId,
                            lp->mask & pu->uc_latches);
    }
#endif

    latXlock (pcontext, lp);
    lp->latchDepth = 1;

#if 0
    MSGD_CALLBACK(pcontext,
              (TEXT *)"%Llatlatch: latchid: %d latchDepth: %d ",
                       latchId, lp->latchDepth);
#endif

} /* end latlatch */


/* PROGRAM: latPrivLatch - request a process private latch
 * NOTE:
 * It is important for this function and its partner
 * latFreePrivLatch to be as fast as possible. When using
 * spinlocks, we often spin several times before getting
 * the latch.
 *
 * NOTE: a process will block if requesting a latch already aquired 
 *       by the process from this or another thread - we DO NOT
 *       simply bump up the latch count
 * 
 * Latches are one of
 * 1) spinlocks implemented with a test-and-set loop, or
 * 2) queueing locks implemented with a lock bit and a
 * system semaphore. An auxiliary lock bit protects the
 * latch queue.
 * 3) multiplexed 2 level hierarchical spinlocks (see latlkmux)
 *
 * RETURNS:	None
 */
DSMVOID 
latPrivLatch(
        dsmContext_t *pcontext,
        latch_t      *pprivLatch) /* latch structure of latch to get */
{
    dbcontext_t *pdbcontext;
    shmemctl_t  *pmt;
    latch_st    *ptheLatch  = (latch_st *)pprivLatch;
    spinlock_t  *lockPtr;
    int          napTime;
    UCOUNT       spins;
    UCOUNT       spinValue;
    ULONG        napCnt = 0;

    if (pcontext && pcontext->pdbcontext && !pcontext->pdbcontext->shmgone)
    {
        pdbcontext = pcontext->pdbcontext;
        if (pdbcontext->argnoshm)
            return;
        pmt = pdbcontext->pmtctl;
    }
    else
    {
        pdbcontext = 0;
        pmt = 0;
    }

    if (pmt)
        spinValue = pmt->mt_spins;
    else
        spinValue = 0;

    /* spinlock without stats -- do it here inline for speed. */
    spins = spinValue + 1;
    napTime = 0;
    napCnt = 0;
    lockPtr = &(ptheLatch->fastLock);
    for (;;)
    {
        if (MT_IS_FREE (lockPtr) && (! MT_TEST_AND_SET (lockPtr)) )
        {
            /* now locked by us */
            ptheLatch->lockCnt++;
            ptheLatch->waitCnt += napCnt;
            return;
        }

        /* busy wait */
        if (spins--) continue;

        napCnt++;
        if (pmt)
            latSleep (pcontext, pmt, &napTime);
        else
        {
            latnap (NULL, napTime);
            napTime+=10;
        }
        spins = spinValue + 1;
    }
} /* end latPrivLatch */


/* PROGRAM: latFreePrivLatch - free a private latch
 *
 * RETURNS:	None
 */
DSMVOID
latFreePrivLatch(
        dsmContext_t  *pcontext,
        latch_t       *pprivLatch) /* latch structure of latch to free */
{
    dbcontext_t *pdbcontext;
    latch_st    *ptheLatch  = (latch_st *)pprivLatch;

    if (pcontext && pcontext->pdbcontext && !pcontext->pdbcontext->shmgone)
    {
        pdbcontext = pcontext->pdbcontext;
        if (pdbcontext->argnoshm)
            return;
    }
    else
    {
        pdbcontext = 0;
    }

    /* release the latch. */

    if (ptheLatch->ltype == MT_LT_SPIN)
    {
	/* spinlock */
        MT_CLEAR_LOCK (&(ptheLatch->fastLock));
        return;
    }

    FATAL_MSGN_CALLBACK(pcontext, mtFTL011);

}  /* end latFreePrivLatch */


/* PROGRAM: latPrivFastLock - request a process private latch
 * NOTE:
 * It is important for this function and its partner
 * latFreePrivMutes to be as fast as possible. When using
 * spinlocks, we often spin several times before getting
 * the latch.
 *
 * NOTE: a process will block if requesting a latch already aquired 
 *       by the process from this or another thread - we DO NOT
 *       simply bump up the latch count
 * 
 * RETURNS:	1 lock obtained
 *              0 lock busy
 */
int 
latPrivFastLock(
        dsmContext_t *pcontext,
        spinlock_t   *pfastLock, /* latch structure of latch to get */
        int           wait)      /* if 1, wait for fast lock */
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    shmemctl_t  *pmt        = pdbcontext->pmtctl;
    int          napTime;
    UCOUNT       spins;
    UCOUNT       spinValue;

    if (pdbcontext->argnoshm || pdbcontext->shmgone) 
    	return 0;

    if (pmt)
        spinValue = pmt->mt_spins;
    else
        spinValue = 0;

    /* spinlock without stats -- do it here inline for speed. */
    spins = spinValue + 1;
    napTime = 0;

    for (;;)
    {

#if 0
#ifdef PRO_REENTRANT /* FOR TESTING PURPOSES */
    napTime = 0;
#endif
#endif

        if (MT_IS_FREE (pfastLock) && (! MT_TEST_AND_SET (pfastLock)) )
        {
            /* now locked by us */
            return 1;
        }

        if (!wait)
            break;

        /* busy wait */
        if (spins--) continue;

        latSleep (pcontext, pmt, &napTime);
        spins = spinValue + 1;
    }

    return 0;

}  /* end latPrivFastLock */


/* PROGRAM: latFreePrivFastLock - free a private latch
 *
 * RETURNS:	None
 */
DSMVOID
latFreePrivFastLock(
        dsmContext_t  *pcontext,
        spinlock_t    *pfastLock) /* latch structure of latch to free */
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;

    if (pdbcontext->argnoshm || pdbcontext->shmgone)
        return;

    /* release the latch. */
    MT_CLEAR_LOCK(pfastLock);

    return;

}  /* end latFreePrivFastLock */


#if 0
/* PROGRAM: latPrivUserMutex - request a process private latch
 * NOTE:
 * It is important for this function and its partner
 * latFreePrivLatch to be as fast as possible. When using
 * spinlocks, we often spin several times before getting
 * the latch.
 *
 * NOTE: a process will block if requesting a latch already aquired 
 *       by the process from this or another thread - we DO NOT
 *       simply bump up the latch count
 * 
 * Latches are one of
 * 1) spinlocks implemented with a test-and-set loop, or
 * 2) queueing locks implemented with a lock bit and a
 * system semaphore. An auxiliary lock bit protects the
 * latch queue.
 * 3) multiplexed 2 level hierarchical spinlocks (see latlkmux)
 *
 * RETURNS:	None
 */
DSMVOID 
latPrivUserMutex(
        dsmContext_t *pcontext,
        mutex_t      *ptheLatch) /* latch structure of latch to get */
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;

    if (mutex_lock(ptheLatch))
        FATAL_MSGN_CALLBACK(pcontext, mtFTL008, 0);

} /* end latPrivUserMutex */


/* PROGRAM: latFreePrivUserMutex - free a private latch
 *
 * RETURNS:	None
 */
DSMVOID
latFreePrivUserMutex(
        dsmContext_t  *pcontext,
        mutex_t       *ptheLatch) /* latch structure of latch to free */
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;

    if (mutex_unlock(ptheLatch))
        FATAL_MSGN_CALLBACK(pcontext, mtFTL011);

}  /* end latFreePrivUserMutex */

#endif


/* PROGRAM: latlkmux - lock a multiplexed latch 
 *
 * RETURNS:	None
 */
DSMVOID
latlkmux (
	dsmContext_t	*pcontext,
	int		 muxId,  /* Id no of mux set */
	int		 muxNo)  /* number of mux element */
{
	dbcontext_t	*pdbcontext = pcontext->pdbcontext;
	usrctl_t	*pu;
	MTCTL		*pmt;

	muxctl_t	*mcp;
	muxlatch_t	*mp;

	latch_st	*lp;
	spinlock_t	*lockPtr;
	unsigned	spins;
	int		napTime;
	ULONG		napCnt;
	psc_user_number_t unum;

    if (pdbcontext->argnoshm) return;
    pmt = pdbcontext->pmtctl;

    mcp = &(pmt->mt_muxctl[muxId]);
    lp = &(pmt->latches[mcp->latchId].l);

    if (mcp->nmuxes == 0)
    {
	latXlock(pcontext, lp);
	return;
    }

    pu  = pcontext->pusrctl;

    mp = &(pmt->mt_muxes[muxNo + mcp->base]);
    unum = pu->uc_usrnum;

#if MTSANITYCHECK
    if (pu->uc_latches & lp->mask)
    {
        /* Not allowed to go for this latch while holding these others,
           because deadlock will surely come of it */

	pdbcontext->pdbpub->shutdn = DB_EXBAD;
        FATAL_MSGN_CALLBACK(pcontext, mtFTL013,
                      lp->latchId, lp->mask & pu->uc_latches);
    }
#endif

    napTime = 0;
    napCnt = 0;
    spins = (unsigned)(pmt->mt_spins);
    if (!pmt->mt_stats)
    {
        /* do it inline for speed */

        lockPtr = &(lp->fastLock);
        for (;;)
        {
            if ((mp->lock == 0) && (MT_IS_FREE (lockPtr)) &&
	        (! MT_TEST_AND_SET (lockPtr)))
            {
	        /* got the latch */

	        if (mp->lock == 0)
	        {
                    /* we have it */

                    mp->lock = 1;
                    mp->holder = unum;
                    mp->lockCnt++;
                    lp->waitCnt += napCnt;

                    /* release the controlling latch */

                    MT_CLEAR_LOCK (lockPtr);
                    MT_PUSH_LOCK (pu, (ULONG)muxId + ((ULONG)(muxNo + 1) << 8));
		    return;
	        }
	        /* someone else has it - release the controlling
                   latch and try again */

	        MT_CLEAR_LOCK (lockPtr);
            }
	    if (spins--) continue;

            napCnt++;
            latSleep (pcontext, pmt, &napTime);

	    /* reset the spin counter after the nap */

            spins = (unsigned)(pmt->mt_spins);
        }
    }

    /* do it the slow way */

    lockPtr = &(lp->fastLock);
    for (;;)
    {
	/* spin on the mux latch until it is free */

        if ((mp->lock == 0) && (MT_IS_FREE (lockPtr)))
	{
	    /* lock the controlling latch */

            latXlock(pcontext, lp);

	    if (mp->lock == 0)
	    {
	        /* muxlatch is free, so lock it */

	        mp->lock = 1;
	        mp->holder = unum;
	        mp->lockCnt++;
                lp->waitCnt += napCnt;
	        pmt->lockcnt[LATWAIT]++;
	        pu->lockcnt[LATWAIT]++;

	        /* free the latch */

                latXfree (pcontext, lp);
                MT_PUSH_LOCK (pu, (ULONG)muxId + ((ULONG)(muxNo + 1) << 8));
	        return;
	    }
	    /* mux is locked by someone free the latch and try again */

            latXfree (pcontext, lp);
	    pmt->waitcnt[LATWAIT]++;
	    pu->waitcnt[LATWAIT]++;
        }
	if (spins--) continue;

	if (napTime == 0) napCnt = 0;
        napCnt++;
        latSleep (pcontext, pmt, &napTime);

	/* reset the spin counter after the nap */

        spins = (unsigned)(pmt->mt_spins);
    }
} /* end latlkmux */


/* PROGRAM latLockQSLatch - Lock a latch that has a queue and a semaphore
 *
 *
 *
 * RETURNS:	None
 */
LOCALF DSMVOID
latLockQSLatch (
	dsmContext_t	*pcontext,
	MTCTL		*pmt,
	usrctl_t	*pu,
	latch_st        *lp)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
	ULONG		lockStartTime = 0;

    if (pmt->mt_timing) lockStartTime = US_MARK ();

    for (;;)
    {
        /* Lock the queue and check the latch state */

        if (latQXlock (pcontext, pmt, pu, lp) == 0)
        {
            /* The latch is free. Grab it and release the queue */

            lp->slowLock = 1;
            lp->holder = pu->uc_usrnum;

            MT_CLEAR_LOCK (&(lp->fastLock));

            /* mark that we have it */

            MT_MARK_LOCKED (pu, lp->latchId);
            MT_PUSH_LOCK (pu, lp->latchId);
            lp->lockCnt++;

            if (! pmt->mt_timing) return;

            /* When we got the lock */

            lp->lockedTime = US_MARK ();

            /* how long it took to get it */

            lp->waitTime += (lp->lockedTime - lockStartTime);
	    return;
        }

#if MTSANITYCHECK
	if (lp->holder == pu->uc_usrnum)
	{
	    pdbcontext->pdbpub->shutdn = DB_EXBAD;
	    FATAL_MSGN_CALLBACK(pcontext, mtFTL006, lp->latchId);
	}
#endif

        /* Busy. Put ourselves in the wait queue for the latch */

	lp->busyCnt++;

        pu->uc_wlatchid = lp->latchId;
        pu->uc_qwnxtl = 0;
        if (lp->qtail) XUSRCTL(pdbcontext, lp->qtail)->uc_qwnxtl = QSELF (pu);
        lp->qtail = QSELF (pu);
        if (lp->qhead == 0) lp->qhead = lp->qtail;

        /* release the queue */

        MT_CLEAR_LOCK (&(lp->fastLock));

        /* Wait for latch owner to release it and give it to us. When
           we wake up, we will try again */

        latWait (pcontext, pmt, pu, lp);

    } /* for loop */
} /* end latLockQSLatch */


/* PROGRAM: latLockSpinLatch -
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
latLockSpinLatch (
	dsmContext_t	*pcontext,
	MTCTL		*pmt,
	usrctl_t	*pu,
	latch_st	*lp)      /* pointer to latch */
{
	unsigned	spins;
	spinlock_t	*lockPtr;
	int		napTime = 0;
	ULONG		busyCnt = 0;
	ULONG		spinCnt = 0;
	ULONG		napCnt = 0;
	ULONG		lockStartTime = 0;

    /* There is a faster version in latlatch, so if you change it you have
       to change it in both places */

    if (pmt->mt_timing) lockStartTime = US_MARK ();

    spins = (unsigned)(pmt->mt_spins) + 1;

    lockPtr = &(lp->fastLock);
    for (;;)
    {
	/* test the lock first without a test-and-set to minimize cpu cache
	   activity */

        if (MT_IS_FREE (lockPtr))
        {
	    if (! MT_TEST_AND_SET (lockPtr))
	    {
		/* now we have it */

		lp->holder = pu->uc_usrnum;
		MT_MARK_LOCKED (pu, lp->latchId);
		MT_PUSH_LOCK (pu, lp->latchId);
		lp->lockCnt++;
		lp->waitCnt += napCnt;

		if (pmt->mt_stats)
		{
		    /* update stats */

		    lp->busyCnt += busyCnt;
		    lp->spinCnt += spinCnt;
		    pmt->waitcnt[LATWAIT] += busyCnt;
		    pu->waitcnt[LATWAIT] += busyCnt;
		    pmt->lockcnt[LATWAIT]++;
		    pu->lockcnt[LATWAIT]++;
		}
		if (pmt->mt_timing)
		{	
		    /* when we got the lock */

		    lp->lockedTime = US_MARK ();

		    /* How long it took to get the lock */

		    lp->waitTime += (lp->lockedTime - lockStartTime);
		}
		return;
            }
            /* Count number of times we found it locked by someone else */

            busyCnt++;
	}
        /* Count number of spins */

	spinCnt++;
        if (spins--) continue;

	/* We have tried to get the latch and not gotten it after
	   trying several times. Now we sleep for a short while so
	   we stop burning cpu and give the latch holder a chance to
	   use the cpu. Note that latSleep will gradually increase the
           nap time if we keep failing */

	napCnt++;
        latSleep (pcontext, pmt, &napTime);
        spins = (unsigned)(pmt->mt_spins) + 1;
    } /* end of spin loop */

} /* latLockSpinLatch */


/* PROGRAM: latLockWait - wait for a lock
 * A record lock, schema lock, or task end
 *
 * RETURNS: int - info.waitStatus
 *                MT_WAIT_PENDING
 *                MT_WAIT_CANCELLED
 *                MT_WAIT_OK
 */
int
latLockWait (
	dsmContext_t	*pcontext,
	int		 waitCode,
	int		 msgNum,	/* which message number to use */
	TEXT		*pfile _UNUSED_, /* pointer to file name */
	TEXT		*puname,	/* pointer to name of holder */
	TEXT		*ptty)	/* pointer to tty name of holder */
{
	MTCTL			*pmt = pcontext->pdbcontext->pmtctl;
	usrctl_t		*pusr = pcontext->pusrctl;
	mtLockWaitInfo_t	info;
	int			ret;
	ULONG			waitStartTime = 0;
        LONG                    t;

    if (waitCode == 0)
    {
	pcontext->pdbcontext->pdbpub->shutdn = DB_EXBAD;
	FATAL_MSGN_CALLBACK(pcontext, mtFTL015);
    }
    /* collect timing data for waits */

    if (pmt->mt_timing) waitStartTime = US_MARK ();

    /* collect stats on waits */

    pmt->waitcnt[waitCode]++;
    pusr->waitcnt[waitCode]++;

#if OPSYS==WIN32API
    if (pusr->uc_rsrccnt)
    {
        /* we have already received a wakeup for this wait. This
           can happen on VMS because we hibernate and if we are waiting
           for a lock and a latch at the same time, the lock holder
           can wake us while we are in the latch wait code */
 
        pusr->uc_rsrccnt = 0;
        pusr->usrwake = 0;
        pusr->uc_wait = 0;
        return (0);
    }
#endif /* WIN32API */


    info.waitStatus = MT_WAIT_PENDING;
    info.pcontext   = pcontext;
    info.checkTime  = MT_LOCK_CHECK_TIME;

    time (&t);
 
    pusr->uc_timeLeft = pusr->uc_lockTimeout * 1000;
    pusr->uc_endTime  = pusr->uc_lockTimeout + t;
 
    for (;;)
    {
        /* Check if the lock has been granted or cancelled */
        ret = latChkLockWaitOnIt(pcontext, LW_WAIT_RETURN);

	switch (ret)
	{
	case LW_END_WAIT:	/* lock was granted */
	    info.waitStatus = MT_WAIT_OK;
	    break;

	case LW_STOP_WAIT:	/* wait was cancelled */
	    info.waitStatus = MT_WAIT_CANCELLED;
	    break;

	case LW_CANCEL_WAIT:	/* cancel the wait */
	    lkrmwaits (pcontext);
            if(pcontext->pdbcontext->accessEnv == DSM_SQL_ENGINE)
                info.waitStatus = DSM_S_RQSTREJ;
            else
            {   
                utssoftctlc ();     /* turn on soft stop */
                info.waitStatus = MT_WAIT_CANCELLED;
            }
	    break;

	case LW_CONTINUE_WAIT:	/* keep waiting */
	    break;

	default:
            /* If we get here the assumption is that there is a lock
               but we don't want to wait, so there is no lock wait.
               The current message, mtMSG008, is not very informative
               so a new message has been added to provide additional
               information about the lock. */
            if (msgNum == LW_SCHLK_MSG)
            {
                MSGN_CALLBACK(pcontext, mtMSG019, puname, ptty);
            }
            else
            {
                MSGN_CALLBACK(pcontext, mtMSG008, ret);
            }
	    lkrmwaits (pcontext);
	    utssoftctlc ();	/* turn on soft stop */
	    info.waitStatus = MT_WAIT_CANCELLED;
	    break;
	}
	if (info.waitStatus != MT_WAIT_PENDING) break;
    }
    /* No longer waiting */

    pusr->uc_wait = 0;
    pusr->usrwake = 0;

    if (pmt->mt_timing)
    {
        /* collect timing data for waits */

        pcontext->pdbcontext->pdbpub->waitTimes[waitCode] +=
                                         (US_MARK () - waitStartTime);
    }
    return (info.waitStatus);

} /* end latLockWait */

/* PROGRAM: latnap - take a short sleep
 * Go to sleep for a short (less than 1 second) period of time.
 * On most systems which have select or poll, the timeout period is
 * specified in in microseconds, but is rounded up to a multiple
 * of the clock interval, which is 10 milliseconds on a lot of machines.
 * 20 millisec is also common. Specifying zero for the timeout for
 * select causes it to return immediately.
 * Do not use the unix setitimer() timer here. Multiple unix kernel
 * calls plus a signal is too much overhead.
 *
 * RETURNS:     None	
 */
DSMVOID
latnap (
	dsmContext_t	*pcontext,
	int		 napTime)
{
#if NAP_TYPE==NAP_POLL || NAP_TYPE==NAP_SELECT 
	int	ret;
#endif

#if NAP_TYPE==NAP_SELECT && BSDSOCKETS
	struct timeval waitTime;
#endif

#if NAP_TYPE==NAP_POLL
	struct	pollfd	fds;
#endif

/*************************/

/* Use one of the methods below to sleep for a short time **************** */

#if NAP_TYPE==NAP_POLL
#define NAP_DEFINED 1
    /* Use poll when available. It is the preferred over select because
       some systems do not have select in the kernel unless the networking
       option is installed and that adds a lot of other stuff, so people
       who don't need networking don't always install it.
       Poll is available on newer System V's and some Berzerkely */

    fds.fd = -1;
    ret = poll (&fds, 1L, (int)napTime);
    if (ret != 0 && errno != EINTR)
    {
        MSGN_CALLBACK(pcontext, utMSG099); /* Poll retn -1, errno%E*/
    }
#endif /* NAP_POLL */

/* **************** */

#if NAP_TYPE==NAP_SELECT 
#define NAP_DEFINED 1
    /* If poll is not available then use select when the select is
       native to the OS. See note above on poll.
       BEWARE: select is broken on some systems. Notably AIX. */

#if BSDSOCKETS
    waitTime.tv_usec = (LONG)(napTime % 1000) * 1000;
    waitTime.tv_sec = napTime / 1000;
    ret = select (0L, (DSMVOID *)NULL, (DSMVOID *)NULL, (DSMVOID *)NULL, &waitTime);
#endif /* BSDSOCKETS */

#if MCPSOCKETS
    ret = select (0, PNULL, PNULL, (LONG)napTime);
#endif

#if DOSSOCKETS
    ret = soselect (0, PNULL, PNULL, (LONG)napTime);
#endif

    if (ret != 0 && errno != EINTR)
    {
        MSGD_CALLBACK(pcontext,"Select returned -1 , errno=%E");
    }
#endif /* NAP_SELECT */

/* **************** */

#if NAP_TYPE==NAP_NAPMS
#define NAP_DEFINED 1
    /* This is the least preferred method because napms is unreliable,
       undocumented and unsupported. Sometimes it is the only method
       available so we have no choice. It can usually be found in the
       "curses" library when available. It does not always work */

    napms ((int)napTime);
#endif /* NAP_NAPMS */

/* **************** */

#if NAP_TYPE==NAP_OS && OPSYS==WIN32API
#define NAP_DEFINED 1
    Sleep(( LONG)napTime);
#endif

/* *** One of the above must have been chosen *** */
#ifndef NAP_DEFINED
gronk: How do we nap??
#endif

} /* end latNap */


/* PROGRAM: latrelatch - reacquire a latch that was previously freed by
 * a call to latfrlatch
 *
 * RETURNS:	None
 */
DSMVOID
latrelatch (
	dsmContext_t	*pcontext,
	int		 latchId,
        COUNT            latchDepth)
{
	dbcontext_t	*pdbcontext = pcontext->pdbcontext;
	latch_st	*lp;

    if (pdbcontext->argnoshm)
        return;

    lp = &(pdbcontext->pmtctl->latches[latchId].l);

#if MTSANITYCHECK
    if (latchDepth == 0) 
    { 
	/* oops - nothing there */

	pdbcontext->pdbpub->shutdn = DB_EXBAD;
	FATAL_MSGN_CALLBACK(pcontext, mtFTL012, latchId);
    }
#endif

    /* lock the latch */
    latXlock(pcontext, lp);

#if 0
    MSGD_CALLBACK(pcontext,
                  (TEXT *)"%Llatrelatch: latchid: %d restoring depth: %d ",
                   latchId, latchDepth);
#endif
 

    /* restore the previous latch depth counter value */
    lp->latchDepth = latchDepth;

    /* we need to pop the latch stack. latXlock will end up pushing
       an latch on the stck which will inturn put the stack pointer 
       in the wrong place 
    */

    MT_POP_LOCK(pcontext->pusrctl);

    pcontext->pusrctl->uc_latfree--;

} /* end latrelatch */


/* PROGRAM: latPingSem - Wake user who is waiting on a semaphore
 *
 *
 * RETURNS:     None
 */
LOCALF DSMVOID
latPingSem (
	dsmContext_t	*pcontext,
	usrctl_t	*pu,
	int		 semcnt)
{
	dbcontext_t *pdbcontext = pcontext->pdbcontext;
        int     ret;
#if OPSYS==WIN32API
        ULONG   semid;
        HANDLE  semnum;
#else
        int     semid;
        int     semnum;
#endif

#if 0
        MTCTL   *pmt = pdbcontext->pmtctl;
#endif

    semid = pu->uc_semid;

#if OPSYS==UNIX

    semnum = pu->uc_semnum;

    /* wake the user by incrementing his semaphore */

    ret = semAdd (pcontext, semid, semnum, semcnt, SEM_NOUNDO, SEM_NORETRY);
    if (ret < 0)

    {
        pdbcontext->shmgone = 1;
        FATAL_MSGN_CALLBACK(pcontext, dbFTL003,ret);
        dbExit(pcontext, 0, DB_EXBAD);              /* statement not reached*/
    }
#endif /* UNIX */

#if OPSYS==WIN32API

    semnum = *(pdbcontext->phsem + (pu->uc_usrnum));

    /* wake the user by incrementing his semaphore */

    ret = semAdd (pcontext, semid, semnum, 1, SEM_NOUNDO, SEM_NORETRY);
    if (ret < 0)
    {
        pdbcontext->shmgone = 1;
        FATAL_MSGN_CALLBACK(pcontext, dbFTL003,ret);
        dbExit(pcontext, 0, DB_EXBAD);          /* statement not reached*/
    }
#endif

} /* end latPingSem */

/* PROGRAM: latpoplocks - release all the latches on the users' latch
 * stack. 
 *
 * RETURNS:     int - number of latches released	
 */
int
latpoplocks(dsmContext_t *pcontext, usrctl_t *pu)
{
	dbcontext_t	*pdbcontext = pcontext->pdbcontext;
        ULONG		i;
        int		j = 50;
        int		n = 0;
        usrctl_t	*p;
	latch_st	*lp;

    if (pdbcontext->argnoshm)
        return (0);

    p = pcontext->pusrctl;

    /* if there are no latches on the stack */
    if (pu->uc_latsp == 0)
        return (0);

    /* if the uc_latsp pointer is not 0, this might be caused
       by being in the middle of a free/relock pair.  To check this
       we need to see if the BIB or the AIB latch is being held.  If
       either of these latches are in the middle of this free / relock
       operation, then we need to pop them off the stack */

    while (pu->uc_latfree > 0)
    {
        MT_POP_LOCK(pu);
        pu->uc_latfree--;
    }

    /* if there are no latches on the stack */
    if (pu->uc_latsp == 0) return (0);

    pcontext->pusrctl = pu;

    /* mark database for death */
               
    pdbcontext->pdbpub->shutdn = DB_EXBAD;
               
    /* free the latches */
               
    while ((pu->uc_latsp) && (j > 0))
    {          
        n++;   
        i = pu->uc_latstack[pu->uc_latsp - 1];
        if (i < 255)
        {      
            /* a regular latch */
               
            MSGN_CALLBACK(pcontext, mtMSG005, i); /* Releasing regular latch. */
            lp = &(pdbcontext->pmtctl->latches[i].l);

	    lp->latchDepth = 0;

            latXfree (pcontext, lp);
        }
        else
        {   
            /* a multiplexed latch, so stack has muxid and mux no. */

            /* Releasing multiplexed latch. */
            MSGN_CALLBACK(pcontext, mtMSG013, i);
            latfrmux(pcontext, (int)(i & 0xff), (int)(i >> 8) - 1);
        }
        j--;
    }
    pcontext->pusrctl = p;

    return (n);

} /* end latpoplocks */


/* PROGRAM: latQXlock - lock the queue for a queued latch
 *
 *
 *
 * RETURNS:	lp->slowLock
 */
LOCALF LONG
latQXlock (
	dsmContext_t	*pcontext,
	MTCTL		*pmt,
	usrctl_t	*pu,
	latch_st	*lp)      /* pointer to latch */
{
        spinlock_t	*lockPtr;
	unsigned	spins;
	int		napTime;
	ULONG		napCnt;

    spins = (unsigned)(pmt->mt_spins) + 1;
    napTime = 0;
    napCnt = 0;
    lockPtr = &(lp->fastLock);
    for (;;)
    {
        if (MT_IS_FREE (lockPtr) && (! MT_TEST_AND_SET (lockPtr)) )
	{
            /* now we have locked it */

            lp->qholder = pu->uc_usrnum;
            lp->waitCnt += napCnt;
	    break;
        }
        if (spins--) continue;

        napCnt++;
        latSleep (pcontext, pmt, &napTime);
        spins = (unsigned)(pmt->mt_spins) + 1;
    }

    return (lp->slowLock);

} /* end latQXlock */

/* PROGRAM: latSemClear - Clear (reinitialize) a user's semaphore
 * Used when a wait is being cancelled.
 *
 * RETURNS:	int
 */
int
latSemClear(dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    usrctl_t    *pu = pcontext->pusrctl;

#if OPSYS==WIN32API
    HANDLE  semnum;
#else
    int     semnum;
#endif
    int		ret = 0;

    if ((pdbcontext) && (pdbcontext->psemdsc))
    {
#if OPSYS==WIN32API
        semnum = *(pdbcontext->phsem + (pu->uc_usrnum));
#else
        semnum = pu->uc_semnum;
#endif
	/* clear the wait semaphore */
	ret = semSet (pcontext, pu->uc_semid, semnum, 0);
    }

    pu->uc_rsrccnt = 0;
    return (ret);

} /* end latSemClear */

/* PROGRAM: latSetCount - update and/or retrieve 
 * either the waitcnt or the lockcnt.
 *
 *
 * RETURNS: ULONG - value of the lockcnt or waitcnt.
 */
ULONG
latSetCount(
	dsmContext_t	*pcontext,
	int		 cnttype,
	int		 code,
	int		 action)
{

    dbcontext_t *pdbcontext = pcontext->pdbcontext;

    /* Retrieve the counts */
    if (action == LATGET)
    {
        if (cnttype)
            return(pdbcontext->pmtctl->lockcnt[code]);
        else 
            return(pdbcontext->pmtctl->waitcnt[code]);
    }

    /* Increment the counts */
    if (action == LATINCR)
    {
        if (cnttype)
            return(pdbcontext->pmtctl->lockcnt[code]++);
        else 
            return(pdbcontext->pmtctl->waitcnt[code]++);
    }
/* BUM BUM
 *  SHOULD RETURN ERROR if action != (LATINCR || LATGET)
 */
 return(0);

} /* end latSetCount */


/* PROGRAM: latseto
 * Initialize various things in the latch array structure, in particular,
 * the latches.
 *
 * RETURNS:   None
 */
DSMVOID
latseto (dsmContext_t *pcontext)
{
	dbcontext_t	*pdbcontext = pcontext->pdbcontext;
        dbshm_t         *pdbshm = pdbcontext->pdbpub;
	MTCTL		*p;
	COUNT		i;
	latch_t		*lp;
	muxctl_t	*mcp;
	muxlatch_t	*mp;
	int		muxbase = 0;


    if (pdbcontext->argnoshm)
    {
        /* No shared memory (single user), so no latches either */

	return;
    }
    p = pdbcontext->pmtctl;
    p->mt_spins = pdbshm->argspin;

#if MT_HAS_QUEUE_SEMAPHORE
    /* Init latch QUEUE semaphore */

    mtQSemInit ();
#endif

    lp = p->latches;
    for (i = 0; i <= MTL_MAX; i++, lp++)
    {
	if (p->mt_spins > 0) lp->l.ltype = MT_LT_SPIN;
        else lp->l.ltype = MT_LT_QUEUE;

        lp->l.mask = (ULONG)0xffffffff;
        lp->l.latchId = i;
        lp->l.holder = -1;
        lp->l.qholder = -1;
        lp->l.slowLock = 0;

#if MT_HAS_LATCH_SEMAPHORES
        if (lp->l.ltype == MT_LT_SEM)
	{
            /* Init latch semaphore */

	    mtLSemInit (&(lp.l));
	}
#endif
        /* This has to be done with the macro because the value of a free
           latch is machine dependent */

        MT_CLEAR_LOCK (&(lp->l.fastLock));
    }
/* USR - protect usrctl, etc ****************** */

    lp = &(p->latches[MTL_USR]);

    /* USR can be locked holding MTX */ 

    lp->l.ltype = MT_LT_QUEUE;

/* OM - protect _Object cache (OM) ****************** */

    lp = &(p->latches[MTL_OM]);

/* MTX - makes bi notes, ai notes, bi allocation atomic ******* */

    lp = &(p->latches[MTL_MTX]);

/* BFP - protects bkctl and backup queue ****************** */

    lp = &(p->latches[MTL_BFP]);

    /* BFP can be locked holding MTX or bktbl */ 

    lp->l.mask ^= (1 << MTL_MTX) |
                (1 << MTL_BF1) | (1 << MTL_BF2) | (1 << MTL_BF3) |
		(1 << MTL_BF4) | (1 << MTL_BF5) | (1 << MTL_BF6) |
		(1 << MTL_BF7) | (1uL << MTL_BF8);

/* CPQ - protects checkpoint queue ****************** */

    lp = &(p->latches[MTL_CPQ]);

    /* CPQ can be locked holding MTX or bktbl */ 

    lp->l.mask ^= (1 << MTL_MTX) |
                (1 << MTL_BF1) | (1 << MTL_BF2) | (1 << MTL_BF3) |
		(1 << MTL_BF4) | (1 << MTL_BF5) | (1 << MTL_BF6) |
		(1 << MTL_BF7) | (1uL << MTL_BF8);

/* PWQ - protects apw write queue  ******* */

    lp = &(p->latches[MTL_PWQ]);

    /* PWQ can be locked holding MTX or bktbl or lru */ 

    lp->l.mask ^= (1 << MTL_MTX) | (1 << MTL_BFP) |
                (1 << MTL_LRU) | (1 << MTL_LR2) |
                (1 << MTL_BF1) | (1 << MTL_BF2) | (1 << MTL_BF3) |
		(1 << MTL_BF4) | (1 << MTL_BF5) | (1 << MTL_BF6) |
		(1 << MTL_BF7) | (1uL << MTL_BF8);

/* LRU - protects buffer pool lru chain  ******* */

    lp = &(p->latches[MTL_LRU]);

    /* LRU can be locked holding MTX  or LRU2 */ 

    lp->l.mask ^= (1 << MTL_MTX) | (1 << MTL_LR2);

/* BF1 - protects buffer bktbls  ******* */

    lp = &(p->latches[MTL_BF1]);

    /* BF1 can be locked holding MTX or lru */ 

    lp->l.mask ^= (1 << MTL_MTX) | (1 << MTL_LRU) | (1 << MTL_LR2);

/* BF2 - protects buffer bktbls  ******* */

    lp = &(p->latches[MTL_BF2]);

    /* BF2 can be locked holding MTX or lru */ 

    lp->l.mask ^= (1 << MTL_MTX) | (1 << MTL_LRU) | (1 << MTL_LR2);

/* BF3 - protects buffer bktbls  ******* */

    lp = &(p->latches[MTL_BF3]);

    /* BF3 can be locked holding MTX or lru */ 

    lp->l.mask ^= (1 << MTL_MTX) | (1 << MTL_LRU) | (1 << MTL_LR2);

/* BF4 - protects buffer bktbls  ******* */

    lp = &(p->latches[MTL_BF4]);

    /* BF4 can be locked holding MTX or lru */ 

    lp->l.mask ^= (1 << MTL_MTX) | (1 << MTL_LRU) | (1 << MTL_LR2);

/* BF5 - protects buffer bktbls  ******* */

    lp = &(p->latches[MTL_BF5]);

    /* BF5 can be locked holding MTX or lru */ 

    lp->l.mask ^= (1 << MTL_MTX) | (1 << MTL_LRU) | (1 << MTL_LR2);

/* BF6 - protects buffer bktbls  ******* */

    lp = &(p->latches[MTL_BF6]);

    /* BF6 can be locked holding MTX or lru */ 

    lp->l.mask ^= (1 << MTL_MTX) | (1 << MTL_LRU) | (1 << MTL_LR2);

/* BF7 - protects buffer bktbls  ******* */

    lp = &(p->latches[MTL_BF7]);

    /* BF7 can be locked holding MTX or lru */ 

    lp->l.mask ^= (1 << MTL_MTX) | (1 << MTL_LRU) | (1 << MTL_LR2);

/* BHT - protects buffer hash chain  ******* */

    lp = &(p->latches[MTL_BHT]);

    /* BHT can be locked holding MTX or bktbl */ 

    lp->l.mask ^= (1 << MTL_MTX) |
                (1 << MTL_BF1) | (1 << MTL_BF2) | (1 << MTL_BF3) |
		(1 << MTL_BF4) | (1 << MTL_BF5) | (1 << MTL_BF6) |
		(1 << MTL_BF7) | (1uL << MTL_BF8);

/* LKF - lock table free chain  ******* */

    lp = &(p->latches[MTL_LKF]);

    /* LKF can be locked holding MTX or schema cache or lock table chain */ 

    lp->l.mask ^= (1 << MTL_MTX) | (1 << MTL_SCC) | (1 << MTL_LHT) |
		(1 << MTL_LKP);

/* LKP - lock table purge chains  ******* */

    lp = &(p->latches[MTL_LKP]);

    /* LKP can be locked holding MTX or lock table chain */

    lp->l.mask ^= (1 << MTL_MTX) | (1 << MTL_LHT);

/* SCC - protects schema cache ****************** */

    lp = &(p->latches[MTL_SCC]);

    /* LK can be locked holding MTX */

    lp->l.mask ^= (1 << MTL_MTX);
    lp->l.ltype = MT_LT_QUEUE;

/* LHT - protects lock hash table and chains ****************** */

    lp = &(p->latches[MTL_LHT]);

    /* LHT can be locked holding MTX, SCC */

    lp->l.mask ^= (1 << MTL_MTX) | (1 << MTL_SCC);

/* BIB - protects rlctl and bi buffers ****************** */

    lp = &(p->latches[MTL_BIB]);

    /* BIB can be locked holding MTX */

    lp->l.mask ^= (1 << MTL_MTX);

/* AIB - protects aictl and ai buffers ****************** */

    lp = &(p->latches[MTL_AIB]);

    /* AIB can be locked holding MTX, TXT, BIB */

    lp->l.mask ^= (1 << MTL_MTX) | (1 << MTL_BIB) | (1 << MTL_TXT);

/* GST - protects shared memory storage pool ****************** */

    lp = &(p->latches[MTL_GST]);

    /* ST can be locked holding most things because we have to allocate
       storage for the other structures */

    lp->l.mask ^= (1 << MTL_MTX) | (1 << MTL_USR) | (1 << MTL_BFP) |
		(1 << MTL_BIB) | (1 << MTL_SCC) | (1 << MTL_TXT) |
		(1 << MTL_AIB) | (1 << MTL_LHT) |
		(1 << MTL_GST) | (1 << MTL_LKF);

/* TXT - protects transaction table ****************** */

    lp = &(p->latches[MTL_TXT]);

    /* TT can be locked holding MTX, BIB */

    lp->l.mask ^= (1 << MTL_MTX) | (1 << MTL_BIB);

/* SEQ - protects sequence generator control ***************************** */

    lp = &(p->latches[MTL_SEQ]);

/* TXQ - protects transaction end lock queue ****************** */

    lp = &(p->latches[MTL_TXQ]);
    lp->l.mask ^= (1 << MTL_MTX);

/* BIW - protects bi empty/dirty buffer list ****************** */

    lp = &(p->latches[MTL_BIW]);
    lp->l.mask ^= (1 << MTL_MTX) | (1 << MTL_BIB);

/* AIW - protects next write/last write ai buffers ****************** */

    lp = &(p->latches[MTL_AIW]);
    lp->l.mask ^= (1 << MTL_MTX) | (1 << MTL_AIB);

/* ****************** */

    mcp = &(p->mt_muxctl[0]);
    for (i = 0; i <= MTM_MAX; i++)
    {
        mcp->nmuxes = 0;
	mcp++;
    }
    /* buffer pool buffers */

    mcp = &(p->mt_muxctl[MTM_BPB]);
    if (pdbshm->argspin && pdbshm->argmux) 
    {
        mcp->nmuxes = MTM_MAXBPB;
    }
    mcp->latchId = MTL_BF1;
    mcp->base = muxbase;
    muxbase = muxbase + mcp->nmuxes;

    /* lock table chains */

    mcp = &(p->mt_muxctl[MTM_LKH]);
    if (pdbshm->argspin && pdbshm->argmux)
    {
        mcp->nmuxes = MTM_MAXLKH;
    }
    mcp->latchId = MTL_LHT;
    mcp->base = muxbase;
    muxbase = muxbase + mcp->nmuxes;

    mp = &(p->mt_muxes[0]);
    for (i = 0; i < muxbase; i++)
    {
	mp->lock = 0;
	mp++;
    }
    p->mt_inited = 1;

    /* initialize the process id */
    pdbshm->brokerpid = utgetpid();

} /* end latseto */


/* PROGRAM: latSetStats - change the latch stats setting
 *
 *
 * RETURNS:   None
 *
 */
DSMVOID
latSetStats (
	dsmContext_t	*pcontext,
	int		 statMode)
{
	MTCTL		*p;
	COUNT		i;
	latch_t		*lp;

    p = pcontext->pdbcontext->pmtctl;
    if (statMode == p->mt_stats)
    {
        /* Current mode matches desired mode */

        return;
    }
    /* Current mode does not match, so invert current mode */

    lp = p->latches;
    for (i = 0; i <= MTL_MAX; i++, lp++)
    {
        /* For each latch type, change it to its opposite */

        switch (lp->l.ltype)
        {
        case MT_LT_SPIN:
            lp->l.ltype = MT_LT_SSPIN;
            break;
        case MT_LT_SSPIN:
            lp->l.ltype = MT_LT_SPIN;
            break;
        case MT_LT_QUEUE:
            lp->l.ltype = MT_LT_SQUEUE;
            break;
        case MT_LT_SQUEUE:
            lp->l.ltype = MT_LT_QUEUE;
            break;
        }
    }
    /* Flip the stats enabled bit */

    p->mt_stats = (p->mt_stats ^ 1) & 1;

} /* end latSetStats */

/* PROGRAM: latSizeStructure - function to calculate the sizeof MTCTL
 * plus muxlatch_t multipled by MTM_TOTAL.  The calculation is
 * done in this function, so that the structures involved can
 * remain part of the private interface.
 *
 * RETURNS: LONG - overall size of the structure.
 */
LONG latGetSizeofStruct()
{
    LONG latchsize;

    latchsize = ((LONG)sizeof(MTCTL) +
    (LONG)(sizeof (muxlatch_t) * MTM_TOTAL));
    
    return(latchsize);

} /* end latSizeStructure */

/* PROGRAM: latSleep - nap for a minimum amount of time
 * Go to sleep for a short period of time. The intent here is to give
 * the process which is holding a latch enough time to do its thing and
 * release it. Generally, we want to sleep for the minimum time possible
 * and if we still can't get the lock, increase the time up to some
 * reasonable maximum. The maximum is contained in pdbshm->argnapmax
 * Since latnap only works for times less than 1 second on most systems,
 * we loop in 500 ms increments if the nap time becomes larger than that.
 *
 * RETURNS:	None
 */
LOCALF DSMVOID
latSleep (
	dsmContext_t	*pcontext,
	MTCTL		 *pmt _UNUSED_,
	int		 *pNapTime)
{
	dbshm_t *pdbshm = pcontext->pdbcontext->pdbpub;
	int	timeLeft;
	int	napTime;


    if (!pdbshm->argnapmax) 
    {
        pdbshm->argnapmax = MT_MAX_NAP_TIME;
    }

    if (*pNapTime == 0)
    {
        /* Initialize time and counts */

        *pNapTime   = (int)(pdbshm->argnap);
    }
    timeLeft = *pNapTime;
    while (timeLeft > 0)
    {
        if (timeLeft < pdbshm->argnapmax) napTime = timeLeft;
        else napTime = pdbshm->argnapmax;

        if (pdbshm->shutdn == DB_EXBAD)
        {
	    if ((pcontext->pdbcontext->usertype & (BROKER | WDOG)) == 0)
	    {
	        /* Fatal error. Time to bail out */

	        dbExit(pcontext, 0, DB_EXBAD);
	    }
	}
	latnap (pcontext, napTime);
	timeLeft = timeLeft - napTime;
    }
    if (*pNapTime < pdbshm->argnapmax)
    {
	/* double the nap time for the next try */

	*pNapTime = *pNapTime << 1;
    }
} /* end latSleep */

/* PROGRAM: latswaplk - swap the top two entires on the latch stack 
 *
 * RETURNS:	None
 */
DSMVOID
latswaplk (dsmContext_t *pcontext)
{
	dbcontext_t	*pdbcontext = pcontext->pdbcontext;
	usrctl_t	*pu;
	ULONG		*sp;
	ULONG		l;

    if (pdbcontext->argnoshm) return;

    pu = pcontext->pusrctl;
    MT_CHECK_STACK (pu, 2)

    /* exchange top two entries on lock stack */

    sp = &(pu->uc_latstack[pu->uc_latsp - 2]);

    l = *sp;
    *sp = *(sp + 1);
    *(sp + 1) = l;

} /* end latswaplk */


/* PROGRAM: latunlatch - release a latch
 *
 * RETURNS:	None
 */
DSMVOID
latunlatch (
	dsmContext_t	*pcontext,
	int		 latchId) /* id of latch */
{
	dbcontext_t	*pdbcontext = pcontext->pdbcontext;
	MTCTL		*pmt;
	latch_st	*lp;

    if (pdbcontext->argnoshm)
        return;

    pmt = pdbcontext->pmtctl;
    lp = &(pmt->latches[latchId].l);

    lp->latchDepth--;

#if 0
    MSGD_CALLBACK(pcontext, (TEXT *)"%Llatunlatch: latchid: %d latchDepth: %d ",
                       latchId, lp->latchDepth);
#endif

    if (lp->latchDepth > 0)
        return;

#if MTSANITYCHECK
    MT_CHECK_STACK (pcontext->pusrctl, 1)

    /* make sure that the top of the lock stack has this latch on it.
       This ensures that locks are released in the same order they are
       acquired, which is not strictly necessary but indicates poor
       coding practice and prevents the lock stack from being accurate */

    /* BUM - Assume latchId is always positive */
    MT_CHECK_TOP (pcontext->pusrctl, (ULONG)latchId)
    if (lp->latchDepth < 0)
    {
        pdbcontext->pdbpub->shutdn = DB_EXBAD;
	FATAL_MSGN_CALLBACK(pcontext, mtFTL010, latchId, lp->latchDepth);
    }
#endif
 
    latXfree (pcontext, lp);

} /* end latunlatch */


/* PROGRAM: latUsrWait - user must wait for a shared resource.
 * The thing we are waiting for is a medium term thing like
 * a buffer lock, NOT a record lock
 *
 * RETURNS:   NONE
 */
DSMVOID
latUsrWait(dsmContext_t	*pcontext)
{
	MTCTL	*pmt = pcontext->pdbcontext->pmtctl;
	usrctl_t *pusr = pcontext->pusrctl;
	int	waitCode;
    	int	ret;
	ULONG	waitStartTime = 0;

    waitCode = (int)(pusr->uc_wait);
    if (waitCode <= TSKWAIT)
    {
	/* lock wait */

	ret = latLockWait (pcontext, waitCode, LW_RECLK_MSG,
                           (TEXT *)NULL, (TEXT *)NULL, (TEXT *)NULL);
	return;
    }
    /* collect stats on waits */

    pmt->waitcnt[waitCode]++;
    pusr->waitcnt[waitCode]++;

    /* collect timing data for waits */

    if (pmt->mt_timing) waitStartTime = US_MARK ();

    /* Wait until someone wakes us up */

    for (;;)
    {
	ret = latWaitOnSem (pcontext, MTSEM_RSRC_WAIT,
                            pusr, (int)(pusr->uc_wait));
	if (ret == MT_WAIT_OK)
            break;

	/* Must have been awakened by a signal (UNIX) or an AST (VMS).
	   Continue waiting */
    }
    /* The wait is over. We were awakened */

    pusr->uc_wait = 0;
    pusr->usrwake = 0;

    if (pmt->mt_timing)
    {
        /* collect timing data for waits */

        pcontext->pdbcontext->pdbpub->waitTimes[waitCode] +=
                                         (US_MARK () - waitStartTime);
    }

} /* end latUsrWait */


/* PROGRAM: latUsrWake - a waiting user is waked up.
 * the user's semaphore in the U semaphore set is incremented.
 * Usage:
 * - server-less user wake up
 *
 * RETURNS:   None
 */
DSMVOID
latUsrWake(
	dsmContext_t	*pcontext,
	usrctl_t	*pusr,
	int		 whichQueue)  /* which queue are waking them from */
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm = pdbcontext->pdbpub;

    if (pusr == pcontext->pusrctl)
        MSGN_CALLBACK(pcontext, mtMSG009);

#if MTSANITYCHECK
    if ((int)(pusr->uc_wait) != whichQueue)
    {
        /* not waiting for this queue */

        if (pdbshm->shutdn == DB_EXBAD) return;
        pdbshm->shutdn = DB_EXBAD;
        MSGN_CALLBACK(pcontext, mtMSG010,
                      (int)pusr->uc_usrnum, whichQueue, (int)pusr->uc_wait);
	return;
    }
    if (pusr->usrwake)
    {
        /* already waked up by someone */

        if (pdbshm->shutdn == DB_EXBAD) return;
        pdbshm->shutdn = DB_EXBAD;
        MSGN_CALLBACK(pcontext, mtMSG011,
                      (int)pusr->uc_usrnum, whichQueue, (int)pusr->usrwake);
	return;
    }

    /* Ignore the following sanity check for expired schema locks since a
     * user who times out awaiting a schema lock is not necessarily at
     * the end of the chain.
     */
    if (pusr->uc_qwtnxt &&
        !(pusr->uc_wait == SCHWAIT && pusr->uc_schlk & SLEXPIRED) &&
        (pusr->uc_wait != TSKWAIT))
    {
        pdbshm->shutdn = DB_EXBAD;
        MSGN_CALLBACK(pcontext, mtMSG012, (int)pusr->uc_usrnum, whichQueue);
    }
#endif
    /* wake him */

    pusr->usrwake = pusr->uc_wait;
    latPingSem (pcontext, pusr, MTSEM_LOCK_WAKE);

} /* latUsrWake */


/* PROGRAM: latWaitOnSem - Wait on a semaphore
 *
 * RETURNS:	
 * 
 *  MT_WAIT_OK		wait completed
 *  MT_WAIT_PENDING	wait interrupted
 */
LOCALF int
latWaitOnSem (
	dsmContext_t	*pcontext,
	int		 semcnt,
	usrctl_t	*pu,
	int		 waitCode)
{
	int		ret;
	int		semid;
	TEXT	*pWakeFlag;
#if OPSYS==WIN32API
        HANDLE          semnum;
#else
        int             semnum;
#endif

    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm = pdbcontext->pdbpub;


    if (semcnt == MTSEM_LAT_WAIT)
    {
        pWakeFlag = &(pu->uc_latchwaked);
    }
    else
    {
        pWakeFlag = &(pu->usrwake);
    }
    semid = pu->uc_semid;

    if (pdbshm->shutdn == DB_EXBAD)
    {
        if ((pdbcontext->usertype & (BROKER | WDOG)) == 0)
        {
            /* Fatal error. Time to bail out */
            dbExit(pcontext, 0, DB_EXBAD);
        }
    }
    
    STATINC (semwCt);

#if OPSYS==UNIX

    semnum = pu->uc_semnum;

    /* wait until the semaphore has been incremented */
    ret = semAdd (pcontext, semid, semnum, semcnt, SEM_NOUNDO, SEM_NORETRY);

    if (ret >= 0)
    {
        if (*pWakeFlag) 

            return (MT_WAIT_OK);

        /* nobody admits to waking us so %Gunexplained wakeup */
        FATAL_MSGN_CALLBACK(pcontext, dbFTL002, waitCode, pu->uc_usrnum);
    }
    
    if (errno == EINTR) 
    	return (MT_WAIT_PENDING);
    
    /* an error occurred. semAdd should have generated an error message */
    pdbcontext->shmgone = 1;
    dbExit(pcontext, 0, DB_EXBAD);
    return(MT_WAIT_PENDING);   /* dr exit wont return, but the compiler 
                                  doesn't know that. */

#endif /* UNIX  */

#if OPSYS==WIN32API

    semnum = *(pdbcontext->phsem + pu->uc_usrnum);

    /* wait until the semaphore has been incremented */
    ret = semAdd(pcontext, semid, semnum, -1, SEM_NOUNDO, SEM_NORETRY);

    if (ret >= 0)
    {
        if (*pWakeFlag) 
        {
            return (MT_WAIT_OK);
        }
        if( pu->usrwake )
        {
            /* We're waiting on a latch and a record lock and we're being
               pinged for the record lock.  e.g. cxFind gets a lock before
               releasing the buffer lock on the index leaf block -- to release
               the buffer lock we need to get a latch for which we can que.
               if in the mean time the record lock holder grants us the record
               lock he'll ping our semaphore.                         */
            return (MT_WAIT_PENDING);
        }
        
        /* nobody admits to waking us so %Gunexplained wakeup */
        FATAL_MSGN_CALLBACK(pcontext, dbFTL002, waitCode, pu->uc_usrnum);
    }

    if (ret == -1)
    {
        return(MT_WAIT_PENDING);
    }
    
    /* an error occurred. semAdd should have generated an error message */
    pdbcontext->shmgone = 1;
    dbExit(pcontext, 0, DB_EXBAD);
    return(MT_WAIT_PENDING);   /* dr exit wont return, but the compiler 
                                  doesn't know that. */
#endif /* WIN32API */

} /* end latWaitOnSem */

/* PROGRAM: latWait - Wait for latch holder to wake us up
 * This is used for latches with queues when we have an
 * atomic test-and-set. The test and set is used to lock
 * the queue while acquiring the latch. See latLockQSLatch().
 *
 * Companion to latWake().
 *
 * RETURNS:	
 */
LOCALF DSMVOID
latWait (
	dsmContext_t	*pcontext,
	MTCTL		*pmt _UNUSED_,
	usrctl_t	*pu,
	latch_st	*latchPtr)
{
    int   ret;

    /* Loop until someone wakes us. We loop because we may be awakened
       by a signal or an AST before the latch holder wakes us. The latch
       holder will put the latch id usrctl.latchwaked when he wakes us
       so that we know he did it */

    for (;;)
    {
	ret = latWaitOnSem(pcontext, MTSEM_LAT_WAIT, pu,
                           (int)(-latchPtr->latchId));
#if OPSYS==WIN32API
	if (pu->usrwake)
	{
	    /* We must have been awakened because we were in
	       a medium or long term queue and the resource holder
	       has incremented it before we have done the wait
	       so we increment the counter. Then we continue waiting
	       for the latch */

	    pu->uc_rsrccnt++;
	}
#endif /* WIN32API */

        if (ret == MT_WAIT_OK) break;

	/* We must have been awakend by a signal (UNIX) or an AST (VMS).
	   Continue waiting */

    } /* wait loop */

    /* latch holder has awakened us */

    pu->uc_wlatchid = 0;
    pu->uc_latchwaked = 0;

} /* end latWait */

/* PROGRAM: latWake - Wake someone who is waiting for a latch
 * This is used for latches with queues when we have an
 * atomic test-and-set.
 * 
 * Companion to latWait().
 *
 * RETURNS:	
 */
LOCALF DSMVOID
latWake (
	dsmContext_t	*pcontext,
	MTCTL		*pmt _UNUSED_,
	latch_st	*latchPtr,
	usrctl_t	*pu)
{

#if MTSANITYCHECK
    if (pu == pcontext->pusrctl)
        MSGN_CALLBACK(pcontext, mtMSG007);
    if (pu->uc_wlatchid != latchPtr->latchId)
    {
	/* user not waiting for this latch */

        if (pcontext->pdbcontext->pdbpub->shutdn == DB_EXBAD) return;

	pcontext->pdbcontext->pdbpub->shutdn = DB_EXBAD;
	FATAL_MSGN_CALLBACK(pcontext, mtFTL005,
                            (int)pu->uc_usrnum, latchPtr->latchId,
	                    (int)pu->uc_wlatchid);
    }
#endif
    /* Wake him */

    pu->uc_latchwaked = (TEXT)(latchPtr->latchId);
    latPingSem (pcontext, pu, MTSEM_LAT_WAKE);

} /* end latWake */


/* PROGRAM: latXfree - release an exclusive latch
 * Nested calls not allowed.
 *
 * RETURNS:	None
 */
LOCALF DSMVOID 
latXfree (
	dsmContext_t	*pcontext,
	latch_st	*lp)
{
	dbcontext_t	*pdbcontext = pcontext->pdbcontext;
	usrctl_t	*pu;
	MTCTL		*pmt;
	usrctl_t	*nextGuy;
	LONG		dummy;

    if (pdbcontext->argnoshm)
    	return;

    pu  = pcontext->pusrctl;
    pmt = pdbcontext->pmtctl;

#if MTSANITYCHECK
    if (!MTHOLDING (lp->latchId))
    {
        pdbcontext->pdbpub->shutdn = DB_EXBAD;
        FATAL_MSGN_CALLBACK(pcontext, mtFTL009, lp->latchId);
    }
#endif

    if (lp->ltype == MT_LT_SPIN)
    {
	/* spinlock */
        MT_CLEAR_LOCK (&(lp->fastLock));
	MT_MARK_UNLOCKED (pu, lp->latchId);
	MT_POP_LOCK (pu);

        return;
    }

    if ((lp->ltype == MT_LT_QUEUE) || (lp->ltype == MT_LT_SQUEUE))
    {

        /* This latch has a queue, so we lock the queue and see
	   if anyone is waiting for it. If they are we wake them */

        dummy = latQXlock (pcontext, pmt, pu, lp);

        /* free the latch. The latch value is protected by its q bit */

        lp->slowLock = 0;
        MT_MARK_UNLOCKED (pu, lp->latchId);
        MT_POP_LOCK (pu);

        nextGuy = (usrctl_t *)NULL;
        while (lp->qhead)
        {
            /* Take first user off the latch wait queue and wake him up */

            nextGuy = XUSRCTL(pdbcontext, lp->qhead);
            lp->qhead = nextGuy->uc_qwnxtl;
            if (lp->qhead == 0) lp->qtail = 0;
            nextGuy->uc_qwnxtl = 0;

	    /* wake everybody if we are shutting down */

            if (pdbcontext->pdbpub->shutdn != DB_EXBAD) break;

            /* wake him up */

            latWake (pcontext, pmt, lp, nextGuy);
        }

        if (pmt->mt_timing)
	{
	    /* How long did we keep it locked ? */

	    lp->lockTime += (US_MARK () - lp->lockedTime);
	}

        /* release the queue */

        MT_CLEAR_LOCK (&(lp->fastLock));

        if (nextGuy) latWake (pcontext, pmt, lp, nextGuy);
	return;
    }

    if (lp->ltype == MT_LT_SSPIN)
    {
	/* spinlock with stats */
        if (pmt->mt_timing)
	{
	    /* How long did we keep it locked ? */
	    lp->lockTime += (US_MARK() - lp->lockedTime);
	}
        MT_CLEAR_LOCK (&(lp->fastLock));
	MT_MARK_UNLOCKED (pu, lp->latchId);
	MT_POP_LOCK (pu);

        return;
    }

    FATAL_MSGN_CALLBACK(pcontext, mtFTL011);

} /* end latXfree */


/* PROGRAM: latXlock - get an exclusive lock an a latch.
 * Nested calls to this function are NOT allowed. Use latlatch instead.
 *
 *
 * RETURNS:	None
 */
LOCALF DSMVOID 
latXlock (
	dsmContext_t	*pcontext,
	latch_st	*lp)
{
	dbcontext_t	*pdbcontext = pcontext->pdbcontext;
	usrctl_t	*pu;
	MTCTL		*pmt;
	spinlock_t	*lockPtr;
	int		napTime;
	ULONG		napCnt;
	unsigned	spins;

    if (pdbcontext->argnoshm) 
    	return;

    pu  = pcontext->pusrctl;
    pmt = pdbcontext->pmtctl;

    if (lp->ltype == MT_LT_SPIN)
    {
	/* spinlock without stats -- do it here inline for speed. */

	spins = (unsigned)(pmt->mt_spins) + 1;
	napTime = 0;
	napCnt = 0;
        lockPtr = &(lp->fastLock);
        for (;;)
	{
            if (MT_IS_FREE (lockPtr) && (! MT_TEST_AND_SET (lockPtr)) )
            {
		/* now locked by us */

                lp->holder = pu->uc_usrnum;
                MT_MARK_LOCKED (pu, lp->latchId);
                MT_PUSH_LOCK (pu, lp->latchId);
                lp->lockCnt++;
                lp->waitCnt += napCnt;
                return;
	    }
	    /* busy */

            if (spins--) continue;

            napCnt++;
            latSleep (pcontext, pmt, &napTime);
	    spins = (unsigned)(pmt->mt_spins) + 1;
        }
    }
    if (lp->ltype == MT_LT_QUEUE)
    {
	/* queue and semaphore */

        for (;;)
        {
            /* Lock the queue and check the latch */

            if (latQXlock (pcontext, pmt, pu, lp) == 0)
            {
                /* The latch is free. Grab it and release the queue */

                lp->slowLock = 1;
                lp->holder = pu->uc_usrnum;

                MT_CLEAR_LOCK (&(lp->fastLock));

                /* mark that we have it */

                MT_MARK_LOCKED (pu, lp->latchId);
                MT_PUSH_LOCK (pu, lp->latchId);
                lp->lockCnt++;

	        return;
            }
            /* Busy. Put ourselves in the wait queue for the latch */

            pu->uc_wlatchid = lp->latchId;
            pu->uc_qwnxtl = 0;
            if (lp->qtail) XUSRCTL(pdbcontext, lp->qtail)->uc_qwnxtl = QSELF (pu);
            lp->qtail = QSELF (pu);
            if (lp->qhead == 0) lp->qhead = lp->qtail;

            /* release the queue */

            MT_CLEAR_LOCK (&(lp->fastLock));

            /* Wait for latch owner to release it and give it to us. When
               we wake up, we will try again */

            latWait (pcontext, pmt, pu, lp);

        } /* for loop */
    }
    if (lp->ltype == MT_LT_SSPIN)
    {
	/* spinlock with statistics enabled */

        latLockSpinLatch (pcontext, pmt, pu, lp);
        return;
    }
    if (lp->ltype == MT_LT_SQUEUE)
    {
	/* queue and semaphore with statistics enabled */

        latLockQSLatch (pcontext, pmt, pu, lp);
	return;
    }
    FATAL_MSGN_CALLBACK(pcontext, mtFTL008, lp->latchId);

} /*  end latXlock */


#if !WINNT_ALPHA
/*
  tstset and tstclr for Windows NT Alpha and Digital Unix are located in file
  $RLDC/tstset.cc.  This code has been written is assembly to enchance
  spinlock performance. The routines below will not be compiled for
  Windows NT Alpha and Digital Unix.
*/

#if OPSYS == WIN32API

/* PROGRAM: tstset - test and set the lock pointer
 * The lock pointed to by pLock is atomically tested
 * and locked.
 *
 * RETURNS: 0 if the lock was free before the call
 *          1 if the lock was locked before the call
 */
int
tstset(spinlock_t    *pLock)
{

    LONG  lockOn   = 1;
    LONG  lockOff  = 0;

    _asm
       {
           mov     eax, dword ptr [pLock];
           mov     edx,     lockOn;
           xchg    dword ptr [eax],   edx;
                                     /* atomically swap constant with       */
                                     /* current value of what was in pLock. */
           cmp     lockOff, edx;     /* compare unlocked value with swapped */
                                     /* pLock value.                        */

           /* If what was in pLock was zero, it wasnt locked  */
           je     Not_Locked;
           xor     edx,     edx;
           jmp     Return_Is_Locked;

           Not_Locked:
       }

          /* the lock was free before the call  */
     return (int)lockOff;

          /* the lock was not free before the call */
     Return_Is_Locked:
     return (int)lockOn;
}

/* PROGRAM: tstclr - clear the lock pointer
   The lock pointed to by pLock is atomically cleared

   Returns: Nothing
*/

int tstclr(spinlock_t    *pLock)
{

    LONG lockOff = 0;

    _asm
       {
           mov    eax, dword ptr [pLock];
           mov    edx, lockOff;
           xchg   DWORD PTR [eax], edx;    /* atomically put zero in pLock  */
       }
          /* the lock pointer has been cleared   */

    return 0;
}
#endif  /* WIN32API */
#endif  /* !WINNT_ALPHA */
/*
****************************************************************************
****************************************************************************
****************************************************************************
	OBSOLETE FUNCTIONS - TO BE DELETED, but still called by nss.c
****************************************************************************
****************************************************************************
****************************************************************************
*/
DSMVOID mtlockdb (dsmContext_t *pcontext);
DSMVOID
mtlockdb (dsmContext_t *pcontext)
{
    latlatch (pcontext, MTL_USR);
}

DSMVOID mtunlkdb (dsmContext_t *pcontext);
DSMVOID
mtunlkdb (dsmContext_t *pcontext)
{
    latunlatch (pcontext, MTL_USR);
}


/****************************************************************************/
/****************************************************************************/


/* PROGRAM: latLockTimeout - See if a latch timeout has expired.
 *          If so, act on it.
 *
 * REUTRNS: LW_CANCEL_WAIT if time out expired.
 *          LW_CONTINUE_WAIT if should continue waiting.
 */
DSMVOID
latLockTimeout(
        dsmContext_t     *pcontext,
        usrctl_t         *pusr,
        LONG             currTime)
{
    lockId_t    lockId;
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    lkctl_t     *plkctl     = pdbcontext->plkctl;

    lck_t       *plck;
    lockId_t    *plockIdCurr;

    int    chain;
    int    hashGroup;
 
#if 0  /* we will now always use the current time passed in */
    int    waitStatus = LW_CONTINUE_WAIT;
    LONG   delayTime = MT_LOCK_CHECK_TIME > 60000 ? MT_LOCK_CHECK_TIME
                                                  : 60000;

    /* decrease the time left */
    pusr->uc_timeLeft = pusr->uc_timeLeft - delayTime;
 
    /* count number of checks */
    (*pcheckCount)++;
    if ((*pcheckCount % 10) == 5)
    {
        /* check the time occasionally because waits are not accurate */
        time (&t);
        if (t > pusr->uc_endTime)
        {
            waitStatus = LW_CANCEL_WAIT;
        }
        else
        { 
            /* adjust time left because sleeps are not accurate */
            pusr->uc_timeLeft = (pusr->uc_endTime - t) * 1000;
        }
    }
    /* have we exceeded the maximum elapsed waiting time? */
    if (pusr->uc_timeLeft < 1)
        waitStatus = LW_CANCEL_WAIT;
 
    if (waitStatus == LW_CONTINUE_WAIT)
        return;
#else
    /* NOTE: uc_timeLeft no longer used! */
    if ( (currTime < pusr->uc_endTime) && !pusr->usrtodie )
        return;
#endif

    if (pusr->uc_wait == SCHWAIT)
    {
        MT_LOCK_SCC ();
        /* Am I still waiting on the schema lock? */
        if (pusr->uc_wait != SCHWAIT)
        {
            MT_UNLK_SCC();
            return;
        }
 
        /* Ensure that resource not already obtained */
        pusr->uc_timeLeft = 0;  /* This signifies lock timeout to user. */
        pusr->uc_endTime = 0;   /* This signifies lock timeout to user. */
 
        pusr->uc_schlk |= SLEXPIRED; /* mark the schema lock as expired */
 
        /* Wake the user up. */
        latUsrWake(pcontext, pusr, pusr->uc_wait);
 
        MT_UNLK_SCC();
        return;
    }
    if(pusr->uc_wait == TSKWAIT)
    {
        MT_LOCK_TXT();
        /* Am I still waiting */
        if(pusr->uc_wait == TSKWAIT)
        {
            
            pusr->uc_timeLeft = 0;
            pusr->uc_endTime = 0;
            /* Wake the user up. */
            latUsrWake(pcontext, pusr, pusr->uc_wait);
        }
        MT_UNLK_TXT();
        return;
    }
    

    /* A user whose timeout has expired has been encountered.
     * (or a user who is marked to die is waiting on a lock so we'll
     * process as if a lock timeout occurred)
     * Process the lock timeout - Wake them up so they can exit.
     */
     /* Get recid of the lock we are waiting for */
     lockId.table = pusr->waitArea;
     lockId.dbkey = pusr->uc_wait1;
 
     /* determine correct chain for this dbkey */
     if( lockId.dbkey == 0 )
         hashGroup = LK_TABLELK;
     else
         hashGroup = LK_RECLK; /* Could lockGroup be LK_RGLK? */
 
     chain = MU_HASH_CHN((lockId.table + lockId.dbkey), hashGroup);
 
     /* locate the entry to release, set pointers */
 
     MU_LOCK_LOCK_CHAIN (chain);
 
    /* we need to make sure that between checking if the timeout
     * expired and now the resource was not yet obtained.
     * If it was, then we will ignore the timeout.
     * If it wasn't, then we wake the user up so it can
     * perform its lock timeout processing.
     */
 
     if (!pusr->uc_qwait2)
     {
         /* We are not queued for any lock request */
         goto done;
     }
 
     /* Ensure we are still queued for the same lock. */
     plck = XLCK(pdbcontext, pusr->uc_qwait2);
 
     if (!plck ||
         !(plck->lk_curf & LKQUEUED)  ||
         (plck->lk_qusr != QSELF(pusr)) )
     {
         /* assume resource already obtained and possible released. */
         goto done;
     }
 
     plockIdCurr = &plck->lk_id;
     if ((plockIdCurr->dbkey != lockId.dbkey) ||
         (plockIdCurr->table != lockId.table))
     {
         /* assume waiting on a different resource already. */
         goto done;
     }
 
     plck->lk_curf |= LKEXPIRED;
     /* Ensure that resource not already obtained */
     pusr->uc_timeLeft = 0;  /* This signifies lock timeout to user. */
     pusr->uc_endTime = 0;  /* This signifies lock timeout to user. */
 
     /* Wake the user up. */
     latUsrWake(pcontext, pusr, pusr->uc_wait);
 
done:
     MU_UNLK_LOCK_CHAIN (chain);

}  /* end latLockTimeout */


/* PROGRAM: latChkLockWaitOnIt -
 * 
 * RETURNS: LW_END_WAIT    - lock obtained
 *          LW_STOP_WAIT   - user pressed cntlc
 *          LW_CANCEL_WAIT - lock timeout occurred
 */
int
latChkLockWaitOnIt(
        dsmContext_t    *pcontext,
        int		 mode)
{
    usrctl_t     *pusr = pcontext->pusrctl;
    int           ret;

    if (pusr->usrwake == 0)
    {
        /* The wait is still pending */
        for (;;)
        {
            /* Is this for the 4gl server??? */
            /* The wait is still pending */
            if (mode == LW_IMMEDIATE_RETURN)
            {
                /* We are supposed to tell the caller that the lock has not been
                 * granted yet */
                return (LW_CONTINUE_WAIT);
            }

            ret = latWaitOnSem(pcontext, MTSEM_LOCK_WAIT, pusr,
                               (int)(pusr->uc_wait));

            if (pcontext->pdbcontext->pdbpub->shutdn)
            {
                /* shutdown in progress. Cancel waits and return error
                 * regardless if we obtained the lock or not.
                 * 
                 * TODO: Should we do this for user-to-die as well?
                 */
                lkrmwaits(pcontext);
                return (LW_STOP_WAIT);
            }

            if (ret == MT_WAIT_OK && pusr->uc_endTime)
            {
                /* Awakened before timeout occurred */
                pusr->uc_endTime = 0;
                return (LW_END_WAIT);
            }

#ifndef LIBDBMGR
            if (utpctlc())
            {
                /* user hit stop key, cancel the wait */
                pusr->uc_endTime = 0;
                lkrmwaits(pcontext);
                return (LW_STOP_WAIT);
            }
#endif
            if (ret == MT_WAIT_OK && !pusr->uc_endTime)
            {
                /* I was awakened due to lock timeout.
                 * Caller will clean up lock table.
                 */
                return (LW_CANCEL_WAIT);
            } 
        }
    }


/* TODO: This case needs to be tested!! */

    /* The lock seems to have already been granted. But a wait on semaphore
       is necessary so it will be decremented. Otherwise, the next time we
       wait on it, we will be awakened too soon since someone has
       incremented it now */
    do
    {
        ret = latWaitOnSem(pcontext, MTSEM_LOCK_WAIT, pusr,
                           (int)(pusr->uc_wait));
 
    } while (ret == MT_WAIT_PENDING);
 
    return (LW_END_WAIT);

}  /* end latChkLockWaitOnIt */







