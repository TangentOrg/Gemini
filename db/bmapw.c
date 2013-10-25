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
/**********************************************************************
*
*	C Source:		bmapw.c
*	Subsystem:		1
*	Description:	
*	%created_by:	marshall %
*	%date_created:	Mon Nov 28 11:57:54 1994 %
*
**********************************************************************/

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include "dbconfig.h"

#include "bkpub.h"
#include "bmprv.h"
#include "dbcontext.h"
#include "dbpub.h"  /* for DB_EXBAD */
#include "dsmpub.h"
#include "latpub.h"
#include "rlpub.h"
#include "usrctl.h"

/* LOCAL functions: */
LOCALF int	bkApwCleanBuf (dsmContext_t *pcontext, struct bkctl *pbkctl,
                               bktbl_t *pbktbl, LONG64 *pbkApwLastBi);
LOCALF LONG	bkApwScheduleCkp (dsmContext_t *pcontext, ULONG curTime);
LOCALF int	bkApwWriteCkp (dsmContext_t *pcontext, ULONG curTime,
                               LONG64 *pbkApwLastBi);
LOCALF DSMVOID	bkApwScan (dsmContext_t *pcontext, LONG *pcurHashNum,
                           LONG *pextraScans, LONG *pnumSkipped,
                           LONG64 *pbkApwLastBi);
LOCALF DSMVOID	bkApwWritePwq (dsmContext_t *pcontext, LONG64 *pbkApwLastBi);

#if BK_HAS_APWS	/* whole program devoted to BK_HAS_APWS */

/* PROGRAM: bkapwbegin - apw starting up, increment number of active 
 *                       page writers
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkapwbegin (dsmContext_t *pcontext)
{
        struct  bkctl   *pbkctl = pcontext->pdbcontext->pbkctl;

    BK_LOCK_POOL ();

    /* Increment number of apw's */

    pbkctl->bk_numapw++;

    BK_UNLK_POOL ();
}

/* PROGRAM: bkapwend - apw going away, decrement number of active page writers
 *
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkapwend (dsmContext_t *pcontext)
{
        struct  bkctl   *pbkctl = pcontext->pdbcontext->pbkctl;
 
    if (pcontext->pdbcontext->pdbpub->shutdn == DB_EXBAD) return;
 
    BK_LOCK_POOL ();
 
    /* Decrement number of apw's */
 
    if (pbkctl->bk_numapw >= 1) pbkctl->bk_numapw--;
 
    BK_UNLK_POOL ();
}

/* PROGRAM: bkApwCleanBuf - Clean a single database buffer
 *
 *      Clean a single database buffer if it can be share locked
 *      and has not been written already.
 *
 *     	Note that we assume that the bktbl entry is locked by the
 *     	caller.
 *
 * RETURNS: 0: Buffer was not written
 *          1: Buffer was written
 */
LOCALF int
bkApwCleanBuf (
	dsmContext_t	*pcontext,
	struct bkctl	*pbkctl,
	bktbl_t		*pbktbl,
        LONG64          *pbkApwLastBi)
{
    BKANCHOR	*pCkpChain;
    int		didIwriteIt;
    int         pctFill = 0;
        
    pCkpChain = &(pbkctl->bk_Anchor[BK_CHAIN_CKP]);

    /* No need if it has already been done */

    if (pbktbl->bt_flags.changed == 0) return (0);

    /* No need if someone else is doing it */

    if (pbktbl->bt_flags.writing) return (0);

    /* Can't write without getting a lock on it */

    if ((COUNT)pbktbl->bt_busy >= BKSTEXCL) return (0);

    if ((COUNT)pbktbl->bt_busy == BKSTFREE)
    {
        /* put a share lock on the buffer */

        pbktbl->bt_busy = BKSTSHARE;         
    }
    pbktbl->bt_usecnt++;
    BK_PUSH_LOCK (pcontext->pusrctl, pbktbl);

    /* Mark that we are going to write this buffer. That way no one
       else will try to clean it at the same time */

    pbktbl->bt_flags.writing = 1;
    if (*pbkApwLastBi < pbktbl->bt_bidpn)
    {
	/* Find out what the last bi note that has
	   been written out is.  We do this only when needed to
	   reduce contention on BK and RL latches. */

        BK_UNLK_BKTBL (pbktbl);
        *pbkApwLastBi = rlgetwritten (pcontext);
        pctFill = rlGetClFill(pcontext);
        BK_LOCK_BKTBL (pbktbl);
    }
    didIwriteIt = 0;
    if (pbktbl->bt_flags.changed &&
        ( pctFill >= 95 || *pbkApwLastBi >= pbktbl->bt_bidpn ))
    {
        /* block is modified */

        /* The log records for this block have been written */
        /* or the bi cluster is more than 95% full          */
        
        pbktbl->bt_bidpn = 0;

        if (pbktbl->bt_flags.chkpt)
        {
            /* scheduled for checkpoint */

            STATINC (bfpcCt); /* Buffers checkpointed by page writers */

            if (pbktbl->bt_Links[BK_LINK_CKP].bt_qNext)
            {
                /* On the checkpoint queue, take it off */

	        BK_LOCK_CHAIN (pCkpChain);

		bmRemoveQ (pcontext, pCkpChain, pbktbl);

		BK_UNLK_CHAIN (pCkpChain);
            }
        }
        /* write out the block */

        bmFlush (pcontext, pbktbl, BUFFERED);

        didIwriteIt = 1;
	STATINC (bfclCt); /* buffers cleaned by page writers */
    }
    pbktbl->bt_flags.writing = 0;

    /* release the share lock */

    bmdecrement(pcontext, pbktbl);

    return (didIwriteIt);
}

/* PROGRAM: bkApwScheduleCkp
 *
 *
 *      Compute the number of database blocks that must be written out
 *      now in order for the ratio of blocks checkpointed to blocks
 *      marked for checkpoint to be equal to or greater than the
 *      percentage of the current bi cluster that has been used.
 *
 *      This algorithm attempts to ensure that the page cleaners write
 *      blocks at a rate such that the last block scheduled for checkpoint
 *      will be written when the current cluster becomes 95% full.
 *
 *      Any blocks that have not been written by the time the cluster
 *      is 100% full will have to be written before the next cluster
 *      can be opened. No new database updates can between the time the
 *      cluster is full and the time the next cluster is opened. If
 *      database blocks must be written during this time, closing the
 *      current cluster will take an unnecessarily long time.
 *
 *      A subtle quirk: If the current cluster is 100% full, then we
 *      are probably in the process of doing the checkpoint before we
 *      open the next cluster. At this time, blocks are being added to
 *      the checkpoint queue as well as being removed. But we can't tell
 *      which were just added and which are going to be removed. It
 *      looks as if we are supposed to have zero blocks on the queue
 *      since the cluster is full. So we do nothing if the cluster is
 *      more than 98% full, waiting until the next cluster has been
 *      opened. This should be ok most of the time. If the page writers
 *      have not succeeded in writing everything, then one of the following
 *      is probably true:
 *
 *      	1. The cluster size is too small and the page writers can't
 *      	keep up. Increase the cluster size. 1024 kb looks like
 *      	a good size for many installations. Bigger may be needed
 *      	if there are many hundreds of clients and a lot of updates.
 *
 *      	2. There are not enough page writers and they can't keep
 *      	up. Add another page writer.
 *
 *      	3. The page writers can't get enough cpu time to do their
 *      	work. The system must be *very* heavily overloaded since
 *      	page writers do not need much cpu time.
 *
 *      promon's "Checkpoints" display can be used to see how many buffers
 *      are being written "Flushed" at checkpoint time. This number should
 *      be zero or close to zero for each checkpoint.
 *
 *      Preliminary testing indicates that this algorithm is relatively
 *      stable and successful. But we won't know for sure until we know
 *      more.
 *
 *
 * RETURNS: The number of blocks that need to be written, divided
 *          by the number of active page writers.
 */
LOCALF LONG
bkApwScheduleCkp (
	dsmContext_t	*pcontext,
	ULONG		 curTime)
{
	struct  bkctl   *pbkctl = pcontext->pdbcontext->pbkctl;
	BKCPDATA	*pCkpData;
	BKANCHOR	*pCkpChain;
	LONG		num2write = 1;
	LONG		numWanted;
	LONG		numMarked;
	LONG		numQed;
	LONG		numDone;
	int		pctFull;
	int		n;

    pCkpChain = &(pbkctl->bk_Anchor[BK_CHAIN_CKP]);

    BK_LOCK_CHAIN (pCkpChain);

    pCkpData = &(pbkctl->bk_Cp[(pbkctl->bk_chkpt & BK_CPSLOT_MASK)]);

    numMarked = pCkpData->bk_cpMarked;
    numDone = pCkpData->bk_cpNumDone;
    numQed = pCkpChain->bk_numEntries;

    if ((numQed == 0) && (pCkpData->bk_cpEndTime == 0))
    {
	/* Note the time we discovered this for the monitor */

	pCkpData->bk_cpEndTime = curTime;
    }
    BK_UNLK_CHAIN (pCkpChain);

    if (numQed < 1) return (0);

    /* Get amount of space used in current cluster */

    pctFull = rlGetClFill (pcontext);
    if ((pctFull < 0) || (pctFull > 98)) return (0);

    if (pctFull > 95) return (numQed);

    /* We want to have written this many blocks now - the
       same percentage of the dirty blocks as the space used in the
       current cluster. The idea is to be done writing then just
       before the cluster fills (95 percent) */

    numWanted = (numMarked * (LONG)pctFull) / (LONG)95;

    if ((numWanted > 0) && (numWanted > numDone))
    {
        /* We are behind - we have to write this many to catch up */

        num2write = numWanted - numDone;
    }
    /* Factor in the number of page writers */

    n = pbkctl->bk_numapw;
    if (n > 1) num2write = (num2write + (n - 1)) / n;

    return (num2write);
}

/* PROGRAM: bkApwWriteCkp - Write the some dirty blocks from the 
 *                          checkpoint queue.
 *
 * RETURNS: The number of buffers left on the checkpoint queue
 */
LOCALF int
bkApwWriteCkp (
	dsmContext_t	*pcontext,
	ULONG		 curTime,
        LONG64            *pbkApwLastBi)
{
        dbcontext_t *pdbcontext = pcontext->pdbcontext;
	struct	bkctl   *pbkctl = pdbcontext->pbkctl;
	struct  bktbl   *pbktbl;
	LONG		num2write;
	LONG		numQueued;
	LONG		numDeQueued = 0;
	int		wroteIt;
	BKANCHOR	*pCkpChain;

    pCkpChain = &(pbkctl->bk_Anchor[BK_CHAIN_CKP]);

    /* Figure out how many to write this time */

    num2write = bkApwScheduleCkp (pcontext, curTime);
    numQueued = num2write;

    /* go down the checkpoint list */
     
    while (num2write > 0)
    {
        if (numDeQueued > numQueued)
        {
            /* Stop and come back later if we have looked at all the
               queued entries and could not write any of them because
               there are lock conflicts. */

            break;
        }
        /* remove oldest entry */

        BK_LOCK_CHAIN (pCkpChain);

        numQueued = pCkpChain->bk_numEntries;

        pbktbl = XBKTBL(pdbcontext, pCkpChain->bk_qHead);
        if (pbktbl) bmRemoveQ (pcontext, pCkpChain, pbktbl);

	BK_UNLK_CHAIN (pCkpChain);

        if (pbktbl == (BKTBL *)NULL)
        {
            /* Queue is empty */

            break;
        }
        numDeQueued++;
        wroteIt = 0;

	BK_LOCK_BKTBL (pbktbl);

	if (pbktbl->bt_flags.chkpt)
	{
	    /* Try to write this one */

	    if (bkApwCleanBuf(pcontext, pbkctl, pbktbl, pbkApwLastBi))
	    {
		/* We did write it */

	        num2write--;
                wroteIt = 1;
	        numDeQueued = 0;
                STATINC (pwcwCt);
	      pbkctl->bk_Cp[(pbkctl->bk_chkpt & BK_CPSLOT_MASK)].bk_cpcWrites++;
	    }
	}
        if (wroteIt == 0)
        {
            /* Did not write it */

	    BK_LOCK_CHAIN (pCkpChain);
	    if (pbktbl->bt_flags.chkpt)
            {
                /* The block still needs checkpointing, but we couldn't get a
                   lock on it, so put it back on the checkpoint chain */

                bmEnqueue (pCkpChain, pbktbl);
            }
            BK_UNLK_CHAIN (pCkpChain);
        }
	/* release the buffer control block */

	BK_UNLK_BKTBL (pbktbl);
    }
    return (numQueued);
}

/* PROGRAM: bkApwScan - Scan a few buffers and write some dirty ones 
 *                      if there are any.
 *
 *      We do this (slowly) so that when the system is idle or not
 *      very busy, dirty buffers are written out so they don't
 *      accumulate in large quantities. This has several benefits:
 *
 *      1. When activity picks up again, dirty buffers won't have to
 *      be written out in order to bring in new blocks that are not
 *      in memory.
 *
 *      2. When the system is shut down, we won't have to write a
 *      large number of blocks before the shutdown.
 *
 *      The default scanning parameters are arranged so that all buffers
 *      get looked at once every 10 minutes. With 50,000 buffers (a
 *      not overly large number for a big system), that means that
 *      83 buffers per second must be checked. This is probably not
 *      very much overhead.
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
bkApwScan (
        dsmContext_t *pcontext,
        LONG         *pcurHashNum,
        LONG         *pextraScans,
        LONG         *pnumSkipped,
        LONG64       *pbkApwLastBi)
{
        dbcontext_t *pdbcontext = pcontext->pdbcontext;
	bktbl_t		*pbktbl;
	QBKTBL		q;
	struct  bkctl   *pbkctl = pdbcontext->pbkctl;
	BKCPDATA	*pCkpData;

	LONG		max2write;
	LONG            num2scan;
	LONG		n;

    if (pbkctl->bk_numdirty == 0)
    {
	/* No need to scan if no dirty buffers */

        return;
    }
    /* Each apw does his share of the scanning */

    num2scan  = pcontext->pdbcontext->pdbpub->pwscan;
    max2write = pcontext->pdbcontext->pdbpub->pwwmax;

    n = pbkctl->bk_numapw;
    if (n > 1) num2scan = (num2scan + (n - 1)) / n;
 
    if (*pextraScans > 0)
    {
	/* Adjust the number to scan. We scanned too many before because
	   we only scan complete hash chains. We want the number we look
	   at to come out ok *on the average* even if we do too many now
	   and then. */

        if (*pextraScans > num2scan)
	{
	    /* Many extra so do none this time but keep track of how many
	       extra are left for next time */

	    *pextraScans = *pextraScans - num2scan;
	    return;
	}
	/* Just a few extra so do some this time and all next time */

	num2scan = num2scan - *pextraScans;
	*pextraScans = 0;
    }
    /* Find out how far we got in the bi log */

    *pbkApwLastBi = rlgetwritten (pcontext);

    /* Scan over all the hash chains */

    BK_LOCK_HASH ();
    while ((num2scan >= 0) && (max2write >= 0))
    {
	if (*pcurHashNum >= pbkctl->bk_hash)
	{
            STATINC (bfscCt); /* Scans of entire pool completed */
            if ((*pnumSkipped > 0) && (pbkctl->bk_numdirty))
	    {
		/* There are dirty buffers and we did a full cycle
		   but skipped over some because they were locked or
		   because their bi notes had not been written yet.
		   Flush bi buffers now. */

		BK_UNLK_HASH ();

                rlbiflsh (pcontext, RL_FLUSH_ALL_BI_BUFFERS);

		BK_LOCK_HASH ();
	    }
	    /* Start over again at the beginning of the hash table */

	    *pcurHashNum = 0;
	    *pnumSkipped = 0;
	}
	q = pbkctl->bk_qhash[*pcurHashNum];
	(*pcurHashNum)++;

	while (q)
	{
            /* Scan the current hash chain. Note that we go all the way to
	       the end so we don't end up skipping some buffers if the
	       hash chain is long and the number to scan is low. */

	    num2scan--;
	    if (num2scan < 0)
                (*pextraScans)++;
	    if (max2write < 0)
                break;

	    pbktbl = XBKTBL(pdbcontext, q);
	    q = pbktbl->bt_qhash;

            STATINC (bfckCt); /* Buffers checked */

            if ((pbktbl->bt_flags.changed) &&
                (*pbkApwLastBi >= pbktbl->bt_bidpn))
	    {
	    /* This one has been modified and its bi notes written */

		BK_UNLK_HASH ();
		BK_LOCK_BKTBL (pbktbl);

		/* Try to to write it */

		if (bkApwCleanBuf (pcontext, pbkctl, pbktbl, pbkApwLastBi))
		{
		    /* We did write it */

		    max2write--;
		    *pnumSkipped = 0;
		    STATINC (pwswCt);
		    pCkpData =
                         &(pbkctl->bk_Cp[(pbkctl->bk_chkpt & BK_CPSLOT_MASK)]);

		    pCkpData->bk_cpsWrites++;
		}
		else
                    (*pnumSkipped)++;

		BK_UNLK_BKTBL (pbktbl);
		BK_LOCK_HASH ();
            }
	} /* end of one hash chain */
    }
    BK_UNLK_HASH ();
}

/* PROGRAM: bkApwWritePwq
 *
 *      Write all buffers from apw queue to disk except any that are
 *      exclusive locked (for update). After they have been written,
 *      put them one the old end of the appropriate lru chain, which
 *      is where they came from.
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
bkApwWritePwq (
        dsmContext_t *pcontext,
        LONG64         *pbkApwLastBi)
{
        dbcontext_t *pdbcontext = pcontext->pdbcontext;
	struct	bkctl   *pbkctl = pdbcontext->pbkctl;
	struct	bkchain	*pLruChain;
	struct  bktbl   *pbktbl;
	LONG		numQueued;
	BKANCHOR	*pApwChain;
	BKCPDATA	*pCkpData;
    
    pApwChain = &(pbkctl->bk_Anchor[BK_CHAIN_APW]);
 
    numQueued = pApwChain->bk_numEntries;
    if (numQueued < 1) return;
    if (numQueued < pcontext->pdbcontext->pdbpub->pwqmin) return;

    /* go down the work queue */
     
    for (;;)
    {
        /* remove oldest entry */

	BK_LOCK_CHAIN (pApwChain);
     
        pbktbl = XBKTBL(pdbcontext, pApwChain->bk_qHead);
        if (pbktbl) bmRemoveQ (pcontext, pApwChain, pbktbl);

	BK_UNLK_CHAIN (pApwChain);

        if (pbktbl == (BKTBL *)NULL)
        {
            /* Queue is empty */

            break;
        }
	BK_LOCK_BKTBL (pbktbl);

	/* Try to write this one */

	if (bkApwCleanBuf (pcontext, pbkctl, pbktbl, pbkApwLastBi))
	{
	    /* We did write it */

	    STATINC (pwqwCt); /* Writes from page writer queue */

            pCkpData = &(pbkctl->bk_Cp[(pbkctl->bk_chkpt & BK_CPSLOT_MASK)]);
	    pCkpData->bk_cpaWrites++;
	}
	/* release the buffer control block */

	BK_UNLK_BKTBL (pbktbl);

	if (pbktbl->bt_qlrunxt == (QBKTBL)NULL)
	{
	    /* We wrote the block (or someone else did). This block
               is not on an lru chain, so we must put it back
	       on the chain it was on before */

	    pLruChain = pbkctl->bk_lru + pbktbl->bt_whichlru;
	    BK_LOCK_LRU (pLruChain);
	    bmEnqLru (pcontext, pLruChain, pbktbl);
	    BK_UNLK_LRU (pLruChain);
	}
    }
    return;
}

/* PROGRAM: bkApw - Check to see if there is apw work to do.
 *
 *          Program should be called once every pwqdelay milliseconds.
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkApw (
	dsmContext_t	*pcontext,
	ULONG 		 curTime,
        ULONG           *plastScanTime,
        LONG            *pcurHashNum,
        LONG            *pextraScans,
        LONG            *pnumSkipped,
        LONG64          *pbkApwLastBi)
{
    /* Try to completely flush the APW queue */

    bkApwWritePwq(pcontext, pbkApwLastBi);

    if (curTime >= (*plastScanTime + 
                     (ULONG)(pcontext->pdbcontext->pdbpub->pwsdelay)))
    {
	/* We have passed or equalled the time to write from the
	   checkpoint queue */

	*plastScanTime = curTime;

	/* Write any blocks scheduled for checkpointing at the current
	   time */

        if (bkApwWriteCkp(pcontext, curTime, pbkApwLastBi) < 1)
        {
            /* No more buffers are scheduled for checkpointing at the
	       current time or during the current bi cluster, so we
               just scan a few buffers and write some of the dirty ones */

	    bkApwScan(pcontext, pcurHashNum, pextraScans, pnumSkipped,
                      pbkApwLastBi);
        }
    }
}

#endif /* BK_HAS_APWS */
