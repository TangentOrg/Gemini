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

#include "bmprv.h"
#include "bmpub.h"
#include "dbcontext.h"
#include "dbpub.h"  /* for DB_EXBAD */
#include "latpub.h"
#include "lkpub.h"
#include "objblk.h"
#include "rlpub.h"
#include "rltlpub.h"
#include "stmpub.h"
#include "stmprv.h"
#include "usrctl.h"
#include "utspub.h"
#include "uttpub.h"

#if SEQUENT | SEQUENTPTX
#if TIMING
#include <usclkc.h>
#endif
#endif

#include <time.h>

#ifdef BM_DEBUG
LOCALF DSMVOID bmDumpLRU	(dsmContext_t *pcontext, bkchain_t* pprivReadOnlyLRU,
			 QBKCHAIN qprivReadOnlyLRU);
#endif

#ifdef BKTRACE
#define MSGD MSGD_CALLBACK
#endif

/* Local function prototypes */
LOCALF DSMVOID bmDeqLru 	(dsmContext_t *pcontext, bkchain_t *pLruChain,
			 bktbl_t *pbktbl);
LOCALF DSMVOID bmblkwait	(dsmContext_t *pcontext, struct  bktbl *pbktbl,
			 int action);
LOCALF DSMVOID bmdolru 	(dsmContext_t *pcontext, bkchain_t *pLruChain,
			 bktbl_t *pbktbl);
LOCALF DSMVOID bmrdnew 	(dsmContext_t *pcontext, struct bkctl  *pbkctl,
			 bktbl_t *pbktbl, int action);
LOCALF DSMVOID bmwake 	(dsmContext_t *pcontext, bktbl_t *pbktbl,
			 int wakeType);
LOCALF DSMVOID bmwrold 	(dsmContext_t *pcontext, bktbl_t *pbktbl);
LOCALF int bmrehash 	(dsmContext_t *pcontext, bktbl_t *pbktbl,
			 ULONG newArea, DBKEY newdbkey, LONG newhash);
LOCALF int bmsteal 	(dsmContext_t *pcontext, LONG hash, ULONG blkArea,
			 DBKEY blkdbkey, int action, bktbl_t **ppbktbl);
LOCALF bktbl_t *bmgetlru(dsmContext_t *pcontext, bkchain_t *pLruChain,
                         COUNT privROBufferRequest);
LOCALF DSMVOID bmChooseAndDoLRU(dsmContext_t *pcontext, bktbl_t *pbktbl);


#define TOUPGRADE	-1

/* Buffer manager return codes - used internally for now */

#define RC_OK		0	/* success */
#define RC_BK_DUP	1	/* block already in memory */
#define RC_BK_NOBUF	2	/* no more buffers available */

/* Last bi dependency counter that we know has been written.
   This is used by the page cleaners to avoid unnecessary calls
   to rlflush() */

/*****************************************************************/
/**								**/
/**	bkbuf.c - buffer pool management routines. These	**/
/**	routines manage the PROGRESS disk block buffer pool	**/
/**	in a fashion designed to minimize actual disk I/O.	**/
/**								**/
/**	The following entry points are defined for external	**/
/**	use:							**/
/**								**/
/**	bmbufo - initialize the buffer pool.			**/
/**								**/
/**	bkloc - locate the indicated block in the buffer	**/
/**		pool, reading it in only if necessary.		**/
/**								**/
/**								**/
/**	bmLockDownMasterBlock - read the first 2 blcks into     **/
/**             storage & mark their buffers to avoid flushing. **/
/**								**/
/**	bmMarkModified - indicate that a particular buffer has  **/
/**		been modified.					**/
/**								**/
/**	bmFlushx - flush all modified blocks from the buffer	**/
/**		pool to their disk locations, ignoring the	**/
/**		master block.					**/
/**								**/
/**	bmFlushMasterblock - flush the master block to disk.	**/
/**								**/
/**	The other routines in this module are for internal	**/
/**	use only and should not be used.			**/
/**								**/
/*****************************************************************/

#if BK_HAS_BSTACK
/* PROGRAM: bmPopBufferStack - Pops the top of the buffer stack.
 *                    Normally the buffer
 *                    being released is at the top of the stack.  But for
 *		      some cases, it is not, so we must search for the
 *		      block being released.
 *	      
 *		      bktbl must be locked by caller
 * RETURNS: DSMVOID
 */
DSMVOID
bmPopBufferStack (
	dsmContext_t	*pcontext,
	bktbl_t		*pbktbl)
{
    usrctl_t	*pusr = pcontext->pusrctl;
    QBKTBL	qrel;
    QBKTBL	*ptop;
    QBKTBL	*psearch;

    /* decrement the stack pointer. It now points at the entry we want to
       remove */

    pusr->uc_bufsp--;
    if (pusr->uc_bufsp >= 0)
    {
        /* look at the top entry. In most cases, it will be the correct one.
           The main problem is with the index manager, which has a very complex
           algorithm for choosing which block to release */

        ptop = pusr->uc_bufstack + pusr->uc_bufsp;
        qrel = QSELF (pbktbl);
        if (qrel == *ptop) return;

        /* The entry we are popping is not on top. See if we can find it */

        for (psearch = ptop - 1; psearch >= pusr->uc_bufstack; psearch--)
        {
	    if (*psearch == qrel)
            {
                /* one that was on top goes where the one we want was */

                *psearch = *ptop;
	        return;
	    }
        }
        /* could not find it */
	
        BK_UNLK_BKTBL (pbktbl);

        pcontext->pdbcontext->pdbpub->shutdn = DB_EXBAD;
        FATAL_MSGN_CALLBACK(pcontext, bkFTL027, pbktbl->bt_dbkey);
    }
    /* oops! stack underflow */

    pusr->uc_bufsp = 0;

    BK_UNLK_BKTBL (pbktbl);

    pcontext->pdbcontext->pdbpub->shutdn = DB_EXBAD;
    FATAL_MSGN_CALLBACK(pcontext, bkFTL021, pbktbl->bt_dbkey);

}  /* end bmPopBufferStack */

#endif /* BK_HAS_BSTACK */


/* PROGRAM: bmwake - Wake processes queued for a block.
 *
 *      bktbl must be locked by caller
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
bmwake (
	dsmContext_t	*pcontext,
	bktbl_t		*pbktbl,
	int		 wakeType) /* EXCLWAIT to wake share or excl locks or
			     SHAREWAIT to wake only share locks */
{
	usrctl_t	*pusr;
	int		waitType;

    while (pbktbl->bt_qfstuser)
    {
	/* take the first guy off the queue */

        pusr = XUSRCTL(pdbcontext, pbktbl->bt_qfstuser);
        waitType = pusr->uc_wait;
	if (wakeType == SHAREWAIT)
	{
	    /* only wake those waiting for a share lock */

	    if (waitType != SHAREWAIT) break;
	}
	pbktbl->bt_qfstuser = pusr->uc_qwtnxt;
	if (pbktbl->bt_qfstuser == (QUSRCTL)NULL)
	{
	    /* queue is empty now */

	    pbktbl->bt_qlstuser = (QUSRCTL)NULL;
	}
	pusr->uc_qwtnxt = (QUSRCTL)NULL;
	pusr->uc_blklock = (QBKTBL)NULL;
#ifdef BKTRACE
MSGD ("%Lwake %i for %D flags %X state %i count %i",
      pusr->uc_usrnum, pbktbl->bt_dbkey, bkf2l (pbktbl), pbktbl->bt_busy,
      pbktbl->bt_usecnt);
#endif
	latUsrWake(pcontext, pusr, waitType); 

	/* only wake one exclusive */

	if (waitType == EXCLWAIT) break;
    }
}  /* end bmwake */


/* PROGRAM: bmdecrement - decrement a buffer's use count, waking
 *
 *      anyone who is waiting on it if needed.
 *      bktbl must be locked by caller
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bmdecrement (
	dsmContext_t	*pcontext,
	bktbl_t		*pbktbl)
{

	usrctl_t	*pusr;
	
    if (pbktbl->bt_usecnt > 0)
    {
        BK_POP_LOCK (pcontext, pbktbl);
        pbktbl->bt_usecnt--;
		
        if (pbktbl->bt_usecnt == 0)
        {
           /* nobody is using the block anymore, so it becomes free */
            pbktbl->bt_busy = BKSTFREE;
            pbktbl->bt_qcuruser = 0;
			
            /* wake anyone queued for it */
            if (pbktbl->bt_qfstuser)
                bmwake (pcontext, pbktbl, EXCLWAIT);
			
            return;
        }
        /* Still some locks left. See if we can grant an upgrade */
		
        if (pbktbl->bt_busy != BKSTINTENT) return;
		
        /* someone has an intend to write lock */
		
        if (pbktbl->bt_usecnt != 1) return;
		
        /* he is the only one with a lock left */
		
        if (pbktbl->bt_qfstuser != pbktbl->bt_qcuruser) return;
		
        /* The first one waiting for the block is the intender */
		
        pusr = XUSRCTL(pdbcontext, pbktbl->bt_qfstuser);
        if (pusr->uc_wait != (TEXT)EXCLWAIT) return;
		
        /* he is waiting for exclusive so we wake him up */
		
        pbktbl->bt_qfstuser = pusr->uc_qwtnxt;
        if (pbktbl->bt_qfstuser == (QUSRCTL)NULL)
        {
			/* queue is empty now */
			
			pbktbl->bt_qlstuser = (QUSRCTL)NULL;
        }
        pusr->uc_qwtnxt = (QUSRCTL)NULL;
        pusr->uc_blklock = (QBKTBL)NULL;
		
#ifdef BKTRACE
        MSGD ("%Lwake %i for upgrade %D flags %X state %i count %i",
               pusr->uc_usrnum, pbktbl->bt_dbkey, bkf2l (pbktbl),
               pbktbl->bt_busy, pbktbl->bt_usecnt);
#endif

        latUsrWake (pcontext, pusr, (int)pusr->uc_wait); 
        return;
		
    }
    /* Use count is less than 1 */
	
    pcontext->pdbcontext->pdbpub->shutdn = DB_EXBAD;
    FATAL_MSGN_CALLBACK(pcontext, bkFTL030,
                        pbktbl->bt_dbkey, pbktbl->bt_usecnt);

}  /* end bmdecrement */


/* PROGRAM: bmbufo - initialize the buffer pool manager.
 *
 *  This involves allocating a buffer control block array, allocating a
 *  buffer pool, and initializing the buffer control blocks.
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bmbufo(dsmContext_t *pcontext)
{
        dbcontext_t     *pdbcontext = pcontext->pdbcontext;
	struct	bkctl	*pbkctl = pdbcontext->pbkctl;
        dbshm_t         *pdbpub = pdbcontext->pdbpub;
	bktbl_t		*pbktbl;
	struct	bkbuf	*pbkbuf;
	bktbl_t   	*prevbk = (bktbl_t *)NULL;
	bktbl_t   	*p1stbk = (bktbl_t *)NULL;
	int		bklatch;
	LONG		bufperlatch;
	LONG		bufnum;
	int		bkmux;
	int		muxmax;
	int		j;
	LONG		i;
	int		blockSize;
	
    /* determine total number of entries in the bktbl. The extra ones
		are for the master block and first index anchor block */
	
    /* general purpose buffers */
	
    pbkctl->bk_numgbuf = pdbpub->argbkbuf + XTRABUFS;
	
    /* number of secondary buffers */
    pbkctl->bk_numsbuf = 0;
	
    /* number of index block buffers */
	
    pbkctl->bk_numibuf = 0;
    i = 0;
    if (i)
    {
		if (i > 100) i = 100;
		pbkctl->bk_numibuf = (pbkctl->bk_numgbuf * i) / 100;
		pbkctl->bk_numgbuf = pbkctl->bk_numgbuf - pbkctl->bk_numibuf;
    }
	
    pbkctl->bk_numbuf = pbkctl->bk_numgbuf + pbkctl->bk_numibuf +
		pbkctl->bk_numsbuf; 
	
    /* Set hash table size */
	
    pbkctl->bk_hash = pdbpub->arghash;
	
    /* Index block lru recycle count (defaults to 0) */
	
    pbkctl->bk_recycle = pdbpub->argixlru;
	
    /* Since a block can be on only one lru chain, we use the same link
		field regardless of which lru chain the block is on */
	
    for (i = 0; i < BK_MAXLRU; i++)
    {
        pbkctl->bk_Anchor[i].bk_linkNumber = BK_LINK_LRU;
    }
    /* link numbers for other chains */
	
    pbkctl->bk_Anchor[BK_CHAIN_APW].bk_linkNumber = BK_LINK_APW;
    pbkctl->bk_Anchor[BK_CHAIN_CKP].bk_linkNumber = BK_LINK_CKP;
    pbkctl->bk_Anchor[BK_CHAIN_BKU].bk_linkNumber = BK_LINK_BKU;
	
    j = MTL_LRU;
    for (i = 0; i < BK_MAXLRU; i++)
    {
        /* set latch number for each lru chain */
		
        pbkctl->bk_lru[i].bk_lrulock = j;
        pbkctl->bk_Anchor[i].bk_lockNumber = j;
		
        j++;
        if (j > MTL_LRLAST)
           j = MTL_LRU;
    }
    /* latch numbers for other chains */
	
    pbkctl->bk_Anchor[BK_CHAIN_APW].bk_lockNumber = MTL_PWQ;
    pbkctl->bk_Anchor[BK_CHAIN_CKP].bk_lockNumber = MTL_CPQ;
    pbkctl->bk_Anchor[BK_CHAIN_BKU].bk_lockNumber = MTL_BFP;
	
#if BK_HAS_APWS
	
    /* set page writer parameters */
    /* apw delay between buffer scans (seconds) */
	
    if (pdbpub->pwsdelay <= 0) pdbpub->pwsdelay = 1;
	
    /* apw max buffers to write in 1 buffer scan */
	
    if (pdbpub->pwwmax <= 0) pdbpub->pwwmax = 25;
	
    /* apw delay between queue scans */
	
    if (pdbpub->pwqdelay <= 0) pdbpub->pwqdelay = 100;
	
    /* apw minimum queue length before writing */
	
    if (pdbpub->pwqmin <= 0) pdbpub->pwqmin = 1;
	
    i = pdbpub->pwscan;
    if (i <= 0)
    {
        /* scan all buffers every 10 minutes by default */
        i = (pbkctl->bk_numbuf * (LONG)pdbpub->pwsdelay) / (LONG)600;
    }
    if (i < 1) i = 1;
    pdbpub->pwscan = i;
	
#endif /* BK_HAS_APWS */
	
    pbkctl->bk_Cp[0].bk_cpBegTime = pbkctl->bk_otime;
	
    /* how deep we scan lru chain while looking for a buffer to steal.
       In single-user, single-server, and small buffer pools, we 
       scan 1 buffer, otherwise we scan 10  */
	
    if ((pbkctl->bk_numbuf <= 100) || (pcontext->pdbcontext->argnoshm))
    {
        pbkctl->bk_lrumax = 1;
    }
    else 
    {
        pbkctl->bk_lrumax = 10;
    }
	
    /* allocate pbktbl's */
    /* Note that we dont get buffers here. This is because the control blocks
       are much smaller than the buffers and we will get fewer page faults
       if they are as contiguous as possible */
	
    pbktbl = (bktbl_t *)NULL;
	
    bklatch = MTL_BF1;
    bkmux = 0;
    muxmax = MTM_MAXBPB;
    bufperlatch = (pbkctl->bk_numbuf / 4);
    bufnum = bufperlatch;
			
    pbktbl = (struct bktbl *)stGet(pcontext, (STPOOL *)QP_TO_P(pdbcontext,
                  pdbpub->qdbpool), 
                  (unsigned) ((sizeof(struct bktbl)) * pbkctl->bk_numbuf) );
    for (i = pbkctl->bk_numbuf; i > 0; i--, pbktbl++)
    {
        QSELF (pbktbl) = P_TO_QP(pcontext, pbktbl); 
        pbktbl->bt_raddr = -1;
        pbktbl->bt_area = 0;
	pbktbl->bt_ftype = BKDATA;
		
        /* set the latch which controls the bktbl */
		
        if (pdbpub->argspin && pdbpub->argmux)
        {
            /* use set of multiplexed latches */
			
            if (bkmux >= MTM_MAXBPB) bkmux = 0;
            pbktbl->bt_muxlatch = bkmux;
            bkmux++;
        }
        else
        {
            /* use 4 buffer control block latches */
			
            pbktbl->bt_muxlatch = bklatch;
            bufnum = bufnum - 1;
            if (bufnum < 0)
            {
                /* next group of buffers goes to the next latch */
                bufnum = bufperlatch;
                bklatch = bklatch + 1;
                if (bklatch > MTL_BF4)
                    bklatch = MTL_BF1;
            }
        }
        /* put it on its lru chain */
		
        if (i <= pbkctl->bk_numsbuf)
        {
            /* on secondary lru chain */
			
            pbktbl->bt_whichlru = (TTINY)BK_SELRU;
        }
        else if (i <= (pbkctl->bk_numsbuf + pbkctl->bk_numibuf))
        {
            /* on index lru chain */
			
            pbktbl->bt_whichlru = (TTINY)BK_IXLRU;
        }
        else
        {
            /* on primary lru chain */
			
            pbktbl->bt_whichlru = (TTINY)BK_GNLRU;
        }

        bmEnqLru (pcontext, pbkctl->bk_lru + (int)pbktbl->bt_whichlru, pbktbl);
		
	if (prevbk == (bktbl_t *)NULL)
	{
	    /* set the head of the chains */
			
            p1stbk = pbktbl;
	    pbkctl->bk_qbktbl = QSELF (pbktbl);
	}
	else
	{
	    /* connect previous buffer to current */ 
			
	    prevbk->bt_qnextb = QSELF (pbktbl);
	}
	prevbk = pbktbl;
    }
    /* now link the last one */
	
    prevbk->bt_qnextb = QSELF (pbktbl);
	
    /* allocate buffers and link them in */
	
    blockSize = pcontext->pdbcontext->pdbpub->dbblksize;
    pbktbl = p1stbk;
    for (i = pbkctl->bk_numbuf; i > 0; )
    {
        /* allocate a bunch at once to reduce waste in stGet/stRent */

        j = 32768 / BKBUFSIZE(blockSize);

        if (j > i) j = i;
		
        bmGetBuffers (pcontext, pbktbl,
                      XSTPOOL(pdbcontext, pdbpub->qdbpool), j);
		
        pbkbuf = XBKBUF(pdbcontext, pbktbl->bt_qbuf);
		
        while (j)
        {
            pbktbl->bt_qbuf = P_TO_QP(pcontext, pbkbuf);
            
            /* next entry */	    
            pbktbl = XBKTBL(pdbcontext, pbktbl->bt_qnextb);
			
            pbkbuf = (bkbuf_t *) ((TEXT *)pbkbuf + BKBUFSIZE(blockSize));
            j--;
            i--;
        }
    }

}  /* end bmbufo */


/* PROGRAM: usrbufo - initialize the user's private buffer pool. 
 *
 * RETURNS: DSMVOID
 */
DSMVOID
usrbufo(dsmContext_t *pcontext)
{
    if (pcontext->pdbcontext->pbkctl->bk_numsbuf < 1)
        pcontext->pdbcontext->argprbuf = 0;
}


/* PROGRAM: bmGetPrivROBuffers - get a users private read only buffers 
 *
 *
 * RETURNS: 0
 */
int
bmGetPrivROBuffers(
        dsmContext_t *pcontext,
        UCOUNT        numBuffersRequested)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    usrctl_t    *pusr = pcontext->pusrctl;

    UCOUNT      numBuffers;
    int         ret;
    bkchain_t  *pprivReadOnlyLRU;

    /* If requested value is less than current maximum.
     * Return everything to the shared general pool and start anew.
     */
    if (!numBuffersRequested || (numBuffersRequested < pusr->uc_ROBuffersMax) )
    {
        bmfpbuf(pcontext, pusr);
        if (!numBuffersRequested)
        {
            ret = 0;
            goto done;
        }
    }
 
    /* If user needs an LRU chain head structure, allocate one */
    if (pusr->uc_qprivReadOnlyLRU)
        pprivReadOnlyLRU = XBKCHAIN(pdbcontext, pusr->uc_qprivReadOnlyLRU);
    else
    {
        pprivReadOnlyLRU = (bkchain_t *)stRent(pcontext,
                       XSTPOOL(pdbcontext, pdbcontext->pdbpub->qdbpool),
                       (unsigned)sizeof(bkchain_t) );
        pusr->uc_qprivReadOnlyLRU = P_TO_QP(pcontext, pprivReadOnlyLRU);
        pprivReadOnlyLRU->bk_lrulock = MTL_LRU;
    }
 
    BK_LOCK_LRU(pprivReadOnlyLRU);
 
    numBuffers = numBuffersRequested - pusr->uc_ROBuffersMax;
 
    ret = BK_BAD_SEQ_BUFFERS(pdbcontext, numBuffers);
 
    if (ret)
        BK_GET_SEQ_BUFFERS(pdbcontext, numBuffers);
 
    if (numBuffers)
    {
        pdbcontext->pdbpub->numROBuffers += numBuffers;
        pusr->uc_ROBuffersMax += numBuffers;
    }
    BK_UNLK_LRU(pprivReadOnlyLRU);
 
    if (ret)
    {
        if (numBuffers)
        {
            /* Print error messagse and ignore assignment. */
            MSGN_CALLBACK(pcontext, bkMSG158,
                          numBuffersRequested, pusr->uc_ROBuffersMax);
        }
        else
        {
            MSGN_CALLBACK(pcontext, bkMSG159, numBuffersRequested);
        }
        ret = 0;
    }

done:
    return ret;


}  /* end bmGetPrivROBuffers */


/* PROGRAM: bmfpbuf - free a users private read only buffers 
 *
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bmfpbuf(
        dsmContext_t *pcontext,
        usrctl_t     *pusr)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    bkchain_t   *pLruChain;
    bkchain_t   *pprivReadOnlyLRU;
    bktbl_t     *pbktbl;

    ULONG        numROBuffers;

    /* private buffers have been depreciated.
     * This entry point will now be used to free
     * up a user's private read only buffers 
     * by putting them back on the general LRU chain.
     */

    if (!pusr->uc_qprivReadOnlyLRU)
        return;

    numROBuffers = pusr->uc_ROBuffersMax;

    pprivReadOnlyLRU = XBKCHAIN(pdbcontext, pusr->uc_qprivReadOnlyLRU);

    /* private read only buffers always stolen from BK_GNLRU */
    pLruChain = &(pdbcontext->pbkctl->bk_lru[BK_GNLRU]);

    /* loop until no more private buffers chained */
    while(pprivReadOnlyLRU->bk_nent)
    {
        BK_LOCK_LRU (pLruChain);
        if (!pprivReadOnlyLRU->bk_nent)
        {
            /* someone else stole this before we latched it */
            BK_UNLK_LRU (pLruChain);
            break;
        }

        pbktbl = XBKTBL(pdbcontext, pprivReadOnlyLRU->bk_qhead);
        if (!pbktbl)
        {
            /* It should never happen that we have entries
             * on our private LRU but our bk_qhead is zero because
             * the LRU is latched.  Test it just in case to avoid
             * any possible race condition.
             * latched it. (Maybe we should fatal instead to avoid
             * potential infinite loop?)
             */
            BK_UNLK_LRU (pLruChain);
            continue;
        }

        /* remove from private read only lru and put back
         * on general shared lru
         */
        bmDeqLru (pcontext, pprivReadOnlyLRU, pbktbl);

        bmEnqLru (pcontext, pLruChain, pbktbl);
        BK_UNLK_LRU (pLruChain);
    }

    pusr->uc_qprivReadOnlyLRU = 0;
    pusr->uc_ROBuffersMax = 0;

    /* remove the chain anchor structure */
    stVacate(pcontext, XSTPOOL(pdbcontext, pdbcontext->pdbpub->qdbpool),
             (TEXT *)pprivReadOnlyLRU);

    BK_LOCK_LRU (pLruChain);
        /* update number of currently allocated/reserved private R/O buffers */
        pdbcontext->pdbpub->numROBuffers -= numROBuffers;
    BK_UNLK_LRU (pLruChain);

    return;

}  /* end bmfpbuf */


/* PROGRAM: bmEnqLru
 *      Add an entry to the GENERAL or INDEX lru chain on the HEAD 
 *      The buffer's whichlru field is used to determine which chain
 *      to put the entry back onto.
 *      The entry is put on the LEAST RECENTLY USED (OLDEST) end.
 *      
 *      lru chain and bktbl must be locked by caller.
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bmEnqLru (
	dsmContext_t	*pcontext,
	bkchain_t       *pLruChain,
	bktbl_t		*pbktbl)
{

	bktbl_t   	*pOldest;
	bktbl_t		*pNewest;
	QBKTBL		q;

    if (pbktbl->bt_flags.fixed) return;

    /* skip if already on the chain */

    if (pbktbl->bt_qlrunxt != (QBKTBL)NULL) return;

    q = QSELF (pbktbl);

    if (pLruChain->bk_qhead)
    {
        /* chain is not empty, add between newest and oldest */

        pOldest = XBKTBL(pdbcontext, pLruChain->bk_qhead);
	pNewest = XBKTBL(pdbcontext, pOldest->bt_qlruprv);

        pbktbl->bt_qlrunxt = QSELF (pOldest);
        pbktbl->bt_qlruprv = QSELF (pNewest);

	pNewest->bt_qlrunxt = q;
        pOldest->bt_qlruprv = q;

        /* make the one we added the oldest */

        pLruChain->bk_qhead = q;
        pLruChain->bk_nent++;
	return;
    }
    /* New entry is the only one */

    pLruChain->bk_qhead = q;
    pbktbl->bt_qlrunxt = q;
    pbktbl->bt_qlruprv = q;
    pLruChain->bk_nent = 1;

}  /* end bmEnqLru */


/* PROGRAM: bmDeqLru
 *      Remove an entry from the lru chain
 *      The buffer's whichlru field is used to determine which chain
 *      it is currently on. This field is used later to restore the
 *      entry to the same chain.
 *
 *      lru chain and bktbl must be locked by caller.
 */
LOCALF DSMVOID
bmDeqLru (
	dsmContext_t	*pcontext,
	bkchain_t	*pLruChain,
	bktbl_t		*pbktbl)
{

	bktbl_t		*pNext;
	bktbl_t		*pPrev;

    /* skip if not on the chain */

    if (pbktbl->bt_qlrunxt == (QBKTBL)NULL) return;

    /* If this was a private read only buffer, clear the ptr to the owner */
    if (pbktbl->bt_qseqUser)
        pbktbl->bt_qseqUser = (QUSRCTL)NULL;

    if (pLruChain->bk_qhead == QSELF (pbktbl))
    {
	/* we are removing the head of the chain */

	pLruChain->bk_qhead = pbktbl->bt_qlrunxt;
	if (pbktbl->bt_qlrunxt == pbktbl->qself)
	{
	    /* This is the one and only entry */

	    pLruChain->bk_qhead = (QBKTBL)NULL;
	    pLruChain->bk_nent = 0;
	    pbktbl->bt_qlrunxt = (QBKTBL)NULL;
	    pbktbl->bt_qlruprv = (QBKTBL)NULL;
	    return;
	}
    }
    /* disconnect from chain */
 
    pNext = XBKTBL(pdbcontext, pbktbl->bt_qlrunxt);
    pPrev = XBKTBL(pdbcontext, pbktbl->bt_qlruprv);

    pNext->bt_qlruprv = pbktbl->bt_qlruprv;
    pPrev->bt_qlrunxt = pbktbl->bt_qlrunxt;

    pLruChain->bk_nent--;

    /* null pointers here mean the block is not on the lru chain */

    pbktbl->bt_qlrunxt = (QBKTBL)NULL;
    pbktbl->bt_qlruprv = (QBKTBL)NULL;

}  /* end bmDeqLru */


/* PROGRAM: bmdolru -  update the lru chain when referencing a block.
 *
 *      LRU and bktbl_t must be locked by caller
 *      We make the specified block the NEWEST one.
 *      The head of the circular chain is the OLDEST one.
 *      The one before that (the oldest one's previous) is the newest.
 *      Starting with the head (oldest), following the next links gets
 *      you progressively newer buffers. The newest buffer's next link
 *      points to the OLDEST buffer (the head of the chain).
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
bmdolru (
	dsmContext_t	*pcontext,
	bkchain_t	*pLruChain,
	bktbl_t		*pbktbl)
{

	bktbl_t		*pOldest;
	bktbl_t		*pNewest;
	QBKTBL		q;
	QBKTBL		qNext;
	QBKTBL		qPrev;
	bktbl_t		*pNext;
	bktbl_t		*pPrev;

    if (pbktbl->bt_flags.fixed) return;

    /* Make the buffer the most recently used.
       The master block and the index anchor block are not on the lru
       chain because we want them fixed in memory and they cant move */

    if (pbktbl->bt_qlrunxt)
    {
	/* block is on lru chain. */ 

        qNext = pbktbl->bt_qlrunxt;
        if (pLruChain->bk_qhead == qNext)
        {
	    /* Current buffer's next link points to the same buffer as
               the head does, so it is already the newest - do nothing */

	    return;
        }
        q = QSELF (pbktbl);
        if (pLruChain->bk_qhead == q)
        {
	    /* it is now the oldest - make it the newest by rotating one.
               The newest is always the head's previous and the head is 
               always the oldest */

            pLruChain->bk_qhead = qNext;
	    return;
        }
        /* disconnect from current spot. There must be at least three
	   entries on the chain because the one we want is not the most
	   recently used and it is not the least recently used. So we do
	   not have to check for the cases where we removed the first
	   or last entry */

        qPrev = pbktbl->bt_qlruprv;
        pNext = XBKTBL(pdbcontext, qNext);
        pPrev = XBKTBL(pdbcontext, qPrev);

        pNext->bt_qlruprv = qPrev;
        pPrev->bt_qlrunxt = qNext;

        /* Insert between newest and oldest. New entry becomes newest */

        pOldest = XBKTBL(pdbcontext, pLruChain->bk_qhead);
	pNewest = XBKTBL(pdbcontext, pOldest->bt_qlruprv);

        pbktbl->bt_qlrunxt = QSELF (pOldest);
        pbktbl->bt_qlruprv = QSELF (pNewest);;

	pNewest->bt_qlrunxt = q;
        pOldest->bt_qlruprv = q;

	return;
    }
    /* Block not on chain so we add it between the newest and oldest */

    q = QSELF (pbktbl);
    if (pLruChain->bk_qhead)
    {
        /* chain is not empty, insert between newest and oldest.
           New entry becomes newest. */

        pOldest = XBKTBL(pdbcontext, pLruChain->bk_qhead);
	pNewest = XBKTBL(pdbcontext, pOldest->bt_qlruprv);

        pbktbl->bt_qlrunxt = QSELF (pOldest);
        pbktbl->bt_qlruprv = QSELF (pNewest);

	pNewest->bt_qlrunxt = q;
        pOldest->bt_qlruprv = q;

        pLruChain->bk_nent++;
	return;
    }
    /* New entry is the only one */

    pLruChain->bk_nent = 1;
    pLruChain->bk_qhead = q;
    pbktbl->bt_qlrunxt = q;
    pbktbl->bt_qlruprv = q;

}  /* end bmdolru */


/* PROGRAM: bmChooseAndDoLRU - pick the appropriate lru to use then bmdolru it
 *
 * The rules here are:
 * 1. If the buffer is in the shared general LRU,
 *    just make it MRU.
 * 2. If the buffer is this user's private read only LRU and TOMODIFY,
 *    move it the the shared general LRU and make it MRU.
 * 3. If the buffer is this user's private read only LRU and TOREAD,
 *    just make it MRU in its current private read only LRU.
 * 4. If the buffer is in a private read only LRU, but not mine,
 *    and I am a private read only buffer user 
 *    just make it MRU in its current private read only LRU.
 * 5. If the buffer is in a private read only LRU an I am not a private R/O user
 *    move it the the shared general LRU and make it MRU.
 *    
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
bmChooseAndDoLRU(
        dsmContext_t *pcontext,
        bktbl_t      *pbktbl)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    bkchain_t   *pLruChain;
    bkchain_t   *pprivReadOnlyLRU;

    pLruChain = pdbcontext->pbkctl->bk_lru + pbktbl->bt_whichlru;
    BK_LOCK_LRU (pLruChain);

    if (!pbktbl->bt_qseqUser)
    {
        /* Not a private read only buffer - use lru specified in bktbl 
         * in marking the buffer as most recently used
         */
        bmdolru(pcontext, pLruChain, pbktbl);
    }
    else if (pbktbl->bt_qseqUser == pcontext->pusrctl->qself)
    {
        /* The buffer belong's this users's private read only lru */
        pprivReadOnlyLRU =
                  XBKCHAIN(pdbcontext, pcontext->pusrctl->uc_qprivReadOnlyLRU);
        if (pbktbl->bt_busy == BKSTEXCL)
        {
            /* We found a buffer in our private read only lru chain and we
             * have marked it for update.  It must therefore be moved 
             * out to its shared lru.
             */
            bmDeqLru (pcontext, pprivReadOnlyLRU, pbktbl);

            bmEnqLru (pcontext, pLruChain, pbktbl);
            bmdolru(pcontext, pLruChain, pbktbl);
        }
        else
        {
            /* just move it to the most recently used end of the lru chain */
            bmdolru(pcontext, pprivReadOnlyLRU, pbktbl);
        }
    }
        /* The buffer belong's to someone else's private read only lru */
    else if (pcontext->pusrctl->uc_qprivReadOnlyLRU)
    {
        /* This user is a private read only buffer user too, so don't put buffer
         * on the shared lru - just make it MRU in its current LRU
         */
        pprivReadOnlyLRU = XBKCHAIN(pdbcontext,
               XUSRCTL(pdbcontext, pbktbl->bt_qseqUser)->uc_qprivReadOnlyLRU);
        bmdolru(pcontext, pprivReadOnlyLRU, pbktbl);
    }
    else
    {
        /* This is not a private read only buffer user so move buffer from the
         * private read only LRU chain to the the general shared LRU chain
         */
        pprivReadOnlyLRU = XBKCHAIN(pdbcontext,
              XUSRCTL(pdbcontext, pbktbl->bt_qseqUser)->uc_qprivReadOnlyLRU);
        bmDeqLru (pcontext, pprivReadOnlyLRU, pbktbl);

        bmEnqLru (pcontext, pLruChain, pbktbl);
        bmdolru(pcontext, pLruChain, pbktbl);
    }
    BK_UNLK_LRU (pLruChain);

}  /* end bmChooseAndDoLRU */


/* PROGRAM: bmEnqueue
 *      Add an entry to the tail of a circular doubly linked queue.
 *      The newest entries go on the "tail".
 *      The oldest entry is the "head".
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bmEnqueue (
	BKANCHOR	*pAnchor,
	bktbl_t		*pbktbl)
{

	COUNT		nLink;
	QBKTBL		qNewest;
	QBKTBL		qOldest;
	bktbl_t		*pNewest;
	bktbl_t		*pOldest;
	QBKTBL		q;

    nLink = pAnchor->bk_linkNumber;
    if (pbktbl->bt_Links[nLink].bt_qNext) return;

    q = QSELF (pbktbl);
    if (pAnchor->bk_numEntries == 0)
    {
        /* New entry becomes the only one */

        pAnchor->bk_qHead = q;
        pNewest = pbktbl;
        pOldest = pbktbl;
    }
    else
    {
        /* chain is not empty, add between newest and oldest.
           newest <---> addition <---> oldest */

        qOldest = pAnchor->bk_qHead;
        pOldest = XBKTBL(pdbcontext, qOldest);

        qNewest = pOldest->bt_Links[nLink].bt_qPrev;
        pNewest = XBKTBL(pdbcontext, qNewest);

	pbktbl->bt_Links[nLink].bt_qNext = qOldest;
	pbktbl->bt_Links[nLink].bt_qPrev = qNewest;
    }
    pNewest->bt_Links[nLink].bt_qNext = q;
    pOldest->bt_Links[nLink].bt_qPrev = q;
    pAnchor->bk_numEntries++;

}  /* end bmEnqueue */


/* PROGRAM: bmRemoveQ
 *      Remove specified entry from the queue, regardless of its
 *      position in the queue
 * 
 * RETURNS: DSMVOID
 */
DSMVOID
bmRemoveQ (
	dsmContext_t	*pcontext,
	BKANCHOR	*pAnchor,
	bktbl_t		*pbktbl)
{

	COUNT		nLink;
	QBKTBL		qNext;
	QBKTBL		qPrev;
	bktbl_t		*pPrev;
	bktbl_t		*pNext;

    nLink = pAnchor->bk_linkNumber;
    if (pbktbl->bt_Links[nLink].bt_qNext == (QBKTBL)NULL) return;

    if (pAnchor->bk_numEntries == 1)
    {
	/* It is the one and only entry */

	pAnchor->bk_qHead = (QBKTBL)NULL;
    }
    else
    {
        /* There are multiple entries */

	qNext = pbktbl->bt_Links[nLink].bt_qNext;
	qPrev = pbktbl->bt_Links[nLink].bt_qPrev;

	if (pAnchor->bk_qHead == QSELF (pbktbl))
	{
	    /* We are taking the one at the head, so advance the head */

	    pAnchor->bk_qHead = qNext;
	}
	/* Remove the entry between previous and next.
	   previous <---> deletion <---> next */

	pNext = XBKTBL(pdbcontext, qNext);
	pPrev = XBKTBL(pdbcontext, qPrev);

	pNext->bt_Links[nLink].bt_qPrev = qPrev;
	pPrev->bt_Links[nLink].bt_qNext = qNext;
    }
    pAnchor->bk_numEntries--;

    /* Clear the links */

    pbktbl->bt_Links[nLink].bt_qNext = (QBKTBL)NULL;
    pbktbl->bt_Links[nLink].bt_qPrev = (QBKTBL)NULL;
    return;

}  /* end bmRemoveQ */


/* PROGRAM: bmBackupOnline - Check if online backup is active for this
 *                           database.
 *
 * RETURNS: 0 = no online backup has been started
 *          1 = online backup is already active
 *         -1 = online backup is not supported
 */
int
bmBackupOnline (
        dsmContext_t    *pcontext)
{
#ifdef HAVE_OLBACKUP
    struct      bkctl   *pbkctl = pcontext->pdbcontext->pbkctl;
    int         ret = 1;

    if (pbkctl->bk_olbackup == 0)
    {
        ret = 0;
        /* Indicate that an online backup has started */
        pbkctl->bk_olbackup = 1;
    }
    return (ret);
#else /* HAVE_OLBACKUP */
    return (-1);
#endif /* HAVE_OLBACKUP */
}  /* end bmBackupOnline */


/* PROGRAM: bmBackupClear
 *      Clear bkctl of online backup state. If the backup queue has
 *      any blocks on it, they are removed and any users waiting on
 *      them are wakened.
 *
 * RETURNS: 0 - success
 *         -1 - online backup is not supported
 */
int
bmBackupClear (dsmContext_t     *pcontext)
{
#ifdef HAVE_OLBACKUP
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    struct      bkctl   *pbkctl = pdbcontext->pbkctl;
    bktbl_t             *pbktbl;

    /* backup no longer in progress */
    pbkctl->bk_olbackup = 0;

    /* If the online backup is aborted while backing up the
       bi file, then the MTX and TXE latches are being held
       by the online backup - so release them.
    */
    if ( MTHOLDING(MTL_MTX) )
    {
        MT_UNLK_BIB();
        MT_UNLK_MTX();
    }

    if( pcontext->pusrctl->uc_txelk )
        rlTXEunlock(pcontext);

    return (0);
#else /* HAVE_OLBACKUP */
    return (-1);
#endif /* HAVE_OLBACKUP */
}  /* end bmBackupClear */


/* PROGRAM: bmFindBuffer
 *      Find the buffer which contains a block with the specified dbkey.
 *      It is tacitly assummed that the caller (except for bkloc) has the
 *      block locked so it wont disappear after it has been found
 *
 * RETURNS: NULL if buffer could not be found.
 */
bmBufHandle_t
bmFindBuffer (
	dsmContext_t	*pcontext,
	ULONG		 area,
	DBKEY		 blkdbkey)
{
#if BK_HAS_BSTACK

    usrctl_t *pusr = pcontext->pusrctl;
    /* Do it this way since we have buffer stack. It is faster.
       All the buffers on the buffer lock stack are locked by us so they
       can't move. So we can check the dbkeys without any other locks */
	
    QBKTBL	*psearch;
    bktbl_t	*pbktbl;
	
    if (pusr->uc_bufsp > 0)
    {
        /* Go down the buffer stack from top to bottom to see if we can
           find it */
		
        for (psearch = pusr->uc_bufstack + pusr->uc_bufsp - 1;
             psearch >= pusr->uc_bufstack; psearch--)
        {
            pbktbl = XBKTBL(pdbcontext, *psearch);
            if (pbktbl->bt_dbkey == blkdbkey && pbktbl->bt_area == area)
            {
                return (QSELF(pbktbl));
            }
        }
    }
    /* could not find it */
	
    pcontext->pdbcontext->pdbpub->shutdn = DB_EXBAD;
    FATAL_MSGN_CALLBACK(pcontext, bkFTL047, blkdbkey);
	
#else /* no buffer stack */
	struct  bkctl	*pbkctl = pcontext->pdbcontext->pbkctl;
	bktbl_t		*pbktbl;
	LONG	 hash;
	QBKTBL	qhash;
	
    /* find the right hash chain */
	
    hash = BKHASHDBKEY (blkdbkey, area, BKGET_AREARECBITS(area));
	
    BK_LOCK_HASH ();
	
    /* scan the chain of same hash to find the requested dbkey */
	
    qhash = pbkctl->bk_qhash[hash];
    while (qhash)
    {
        pbktbl = XBKTBL(pdbcontext, qhash);
        if (pbktbl->bt_dbkey == blkdbkey)
        {
            /* Found it - return the address of the buffer */

            BK_UNLK_HASH ();
            return (QSELF(pbktbl));
        }
        if (pbktbl->bt_dbkey > blkdbkey) break;
		
        qhash = pbktbl->bt_qhash;
    }
    BK_UNLK_HASH ();
	
    FATAL_MSGN_CALLBACK(pcontext, bkFTL047, blkdbkey);
	
#endif /* no buffer stack */
    return (bmBufHandle_t) 0;

}  /* end bmFindBuffer */


/* PROGRAM:	bmUpgrade - convert a share lock with intent to modify into
 *      	an exclusive lock. If there are no other processes which
 *      	have share locks, the request is granted immediately.
 *      	If there are share locks, the request is queued and granted
 *      	after the current share locks have been released. No new
 *      	share locks or exclusive locks will be granted before
 *      	the upgrade. bkrlsbuf must be called as usual to
 *      	release the exclusive lock eventually.
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bmUpgrade (
	dsmContext_t	*pcontext,
	bmBufHandle_t	 bufHandle )
{

	bktbl_t	   *pbktbl;
	ULONG           retvalue;    /* return value from latSetCount() */
	int             counttype,   /* 0=waitcnt, 1=lockcnt */
	codetype;    /* type of wait requested */
	
    pbktbl = XBKTBL(pdbcontext, bufHandle);
	
    BK_LOCK_BKTBL (pbktbl);
	
    if (pbktbl->bt_busy == BKSTINTENT  &&
        (pbktbl->bt_qcuruser == QSELF(pcontext->pusrctl)))
    {
        while (1)
        {
            if (pbktbl->bt_usecnt == 1)
            {
                /* grant the upgrade */
				
                pbktbl->bt_busy = BKSTEXCL;
                BK_UNLK_BKTBL (pbktbl);
                return;
            }
            counttype = 0;
            codetype = INTENTWAIT;
            /* update the waitcnt in the latch structure */
            retvalue = latSetCount(pcontext, counttype, codetype, LATINCR);
            pcontext->pusrctl->waitcnt[INTENTWAIT]++;

            /* wait for the locks to be released */
            bmblkwait (pcontext, pbktbl, TOUPGRADE);
                       BK_LOCK_BKTBL (pbktbl);
        }
    }
    /* oops - we dont have an intent to write lock */
	
    BK_UNLK_BKTBL (pbktbl);
    pcontext->pdbcontext->pdbpub->shutdn = DB_EXBAD;
    FATAL_MSGN_CALLBACK(pcontext, bkFTL048, pbktbl->bt_dbkey);
    MSGN_CALLBACK(pcontext, bkMSG160, pcontext->pusrctl->uc_usrnum);
}  /* end bmUpgrade */


/* PROGRAM: bmDownGrade - convert an exclusive lock to an intent lock.
 *                        called by the index manager after clearing 
 *                        index delete locks from a block to prevent
 *                        a block from splitting because of delete locks
 *                        for committed tx's
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bmDownGrade (
	dsmContext_t	*pcontext,
	bmBufHandle_t	 bufHandle )
{
    bktbl_t    *pbktbl;

    
    pbktbl = XBKTBL(pdbcontext, bufHandle);

    BK_LOCK_BKTBL (pbktbl);

    if (pbktbl->bt_busy == BKSTEXCL && 
        pbktbl->bt_qcuruser == QSELF(pcontext->pusrctl)) 
    {
       /* Go ahead and downgrade   */
       pbktbl->bt_busy = BKSTINTENT;
       if( pbktbl->bt_qfstuser ) 
       {
           /* Wake up those waiting for a share lock on the buffer.
              If we don't wake up share lockers this could cause dead
              lock should the index manager need to subsequently upgrade
              this lock to an exclusive lock.                             */
           bmwake(pcontext, pbktbl, SHAREWAIT);
       }
       BK_UNLK_BKTBL(pbktbl);
       return;
    }
    BK_UNLK_BKTBL(pbktbl);
    pcontext->pdbcontext->pdbpub->shutdn = DB_EXBAD;
    MSGN_CALLBACK(pcontext, bkMSG161, pbktbl->bt_dbkey, (LONG)pbktbl->bt_busy);
    return;

}  /* end bmDownGrade */


/* PROGRAM: bmgetlru - Get least recently used buffer which is not busy
 *      and preferably not modified
 *      The lru chain must be locked
 *
 *      Skip buffers which are wired down so they cant be replaced.
 *
 *      We start searching at the LEAST RECENTLY USED end (the head),
 *      following the "next" pointer toward the MRU end (the tail).
 *
 *      returns with bktbl locked if buffer was found
 *
 * RETURNS: NULL if no buffer available
 */
LOCALF struct bktbl *
bmgetlru (
	dsmContext_t	*pcontext,
	bkchain_t       *pLruChain,
        COUNT            privROBufferRequest)
{
        dbcontext_t     *pdbcontext = pcontext->pdbcontext;
	bkctl_t         *pbkctl = pdbcontext->pbkctl;
	BKANCHOR	*pApwChain;
	bktbl_t		*pbktbl;
	bktbl_t		*pfirstmod;
	QBKTBL		qnext;
	LONG		num2Check;
	LONG		Max2Check;

    /* dont search too much of the chain - otherwise we defeat the
       lru mechanism entirely by getting too close to the mru end */

    num2Check = pbkctl->bk_lrumax;
    pfirstmod = (bktbl_t *)NULL;
    pApwChain = &(pbkctl->bk_Anchor[BK_CHAIN_APW]);
    Max2Check = pLruChain->bk_nent;

    BK_LOCK_LRU (pLruChain);

    /* While we have the lru chain locked, no buffers can be added,
       deleted, or moved unless we do it ourselves.
       NOTE: The HEAD of the LRU chain is the OLDEST buffer. The oldest
       buffer's previous link points to the NEWEST buffer and the oldest
       buffer's next link points to a slightly newer one than the oldest.
       Following the next links gets you progressively newer buffers. */ 

    qnext = pLruChain->bk_qhead;
    if (qnext == (QBKTBL)NULL)
       goto nobuffer;
    while (1)
    {
	num2Check = num2Check - 1;
	Max2Check = Max2Check - 1;

	pbktbl = XBKTBL(pdbcontext, qnext);
        qnext = pbktbl->bt_qlrunxt;

        if ((pbktbl->bt_busy == BKSTFREE) && (pbktbl->bt_flags.fixed == 0)
            && (pbktbl->bt_qfstuser == 0))
        {
            /* This buffer is a candidate for replacement  - see if it
	       is still free and clean */

	    BK_LOCK_BKTBL (pbktbl);

            if ((pLruChain->bk_nent > 100) && (pbktbl->bt_cycle > 0))
	    {
                /* Decrement the recycle counter so when we
                   get to 0 we'll stop recycling it */

                pbktbl->bt_cycle--;

		bmdolru(pcontext, pLruChain, pbktbl);
	    }
	    else if (pbktbl->bt_busy == BKSTFREE)
            { 
	        /* Still free */

                if ((pbktbl->bt_flags.changed == 0) && (pbktbl->bt_qfstuser == 0) )
                    goto takeit;

                /* This buffer has been modified.
		   FIXFIX: Will we come out ahead if we take it if it
		   is marked for checkpoint? We will have to write it
		   out now, but then it won't need to be written at the next
		   checkpoint. */

                if (pfirstmod == (bktbl_t *)NULL)
		{
		    /* use it if we already looked at a bunch */

		    if (num2Check <= 0)
                        goto takeit;

		    /* remember the first one we find */

		    pfirstmod = pbktbl;
		}
		/* Skip it */

#if BK_HAS_APWS
		if ((pbkctl->bk_numapw > 0) &&
                    (pLruChain->bk_nent > 100) && /* don't take all of them */
		    (pbktbl->bt_Links[BK_LINK_APW].bt_qNext == (QBKTBL)NULL))
	        {
	            /* take it off the lru chain. page writer will put it
	               back when it has been written. This reduces the number
	               of buffers that have to be examined to find one to
	               replace next time. */

	            bmDeqLru (pcontext, pLruChain, pbktbl);

	            /* Put it on apw chain. This will cause page writer
	               to write it */

                    BK_LOCK_CHAIN (pApwChain);
	            bmEnqueue (pApwChain, pbktbl);
	            BK_UNLK_CHAIN (pApwChain);

                    STATINC (pwqaCt); /* Count buffers added to apw queue */
	        }
#endif /* BK_HAS_APWS */
	    }
	    BK_UNLK_BKTBL (pbktbl);
	}
        STATINC (bffsCt); /* LRU buffers skipped */

        /* advance to next buffer */
         
        if (num2Check >= 0) continue;

        /* We have checked as many as we should to avoid keeping the
	   lru chain locked too long */

	if (pfirstmod == (bktbl_t *)NULL)
	{
	    /* Have not seen a candidate yet */

            if (Max2Check < 0)
            {
                /* We have looked at all of them and none are free */
                goto nobuffer;
            }
	}
	else
	{
	    /* Take the first modified candidate if still free */

            pbktbl = pfirstmod;
	    pfirstmod = (bktbl_t *)NULL;

	    BK_LOCK_BKTBL (pbktbl);

            if ((pbktbl->bt_busy == BKSTFREE) && (pbktbl->bt_qfstuser == 0) )
                goto takeit;

	    /* Cant use it. Start over. Eventually we will find one or we will
	       have examined them all. It is supposed to be very rarely that
	       this happens */

	    BK_UNLK_BKTBL (pbktbl);

            qnext = pLruChain->bk_qhead;
	}
    } /* while loop */

takeit:

    if (privROBufferRequest)
    {
        bkchain_t *pprivReadOnlyLRU = XBKCHAIN(pcontext->pdbcontext,
                                      pcontext->pusrctl->uc_qprivReadOnlyLRU);
        /* The request is to remove a buffer from the general storage pool
         * and to put it into a private read only buffer.
         * We will therefore deque the  buffer from the general storage pool
         * and enqueue it to the users buffer.
         * At this point both chains are locked.
         */
      
        bmDeqLru (pcontext, pLruChain, pbktbl);

        /* Identify owning private read only buffer user */
        pbktbl->bt_qseqUser = pcontext->pusrctl->qself;
        bmEnqLru (pcontext, pprivReadOnlyLRU, pbktbl);
        bmdolru(pcontext, pprivReadOnlyLRU, pbktbl);
    }
    else
    {
        /* we have selected a buffer. move it to the front of the lru chain
           now since we already have the lru chain locked. This will also
           shorten the searches others have to do */

        bmdolru(pcontext, pLruChain, pbktbl);
    }

    /* since we locked the bktbl after the lru, we have to do a latch
       exchange to get the lock stack right before we unlock the lru */

    latswaplk(pcontext);

    BK_UNLK_LRU (pLruChain);

    return (pbktbl);

nobuffer:
    /* no buffer available */
    BK_UNLK_LRU (pLruChain);

    return ((bktbl_t *)NULL);

}  /* end bmgetlru */


/* PROGRAM: bmrehash - move block to a new hash chain
 *      bktbl_t must be locked.
 *      if the block's dbkey is zero, it is a buffer that
 *      has not been used yet and has nothing in it.
 *      In that case, it is not on a hash chain.
 *
 * RETURNS: 0 if ok.
 *          1 if the new dbkey is already in the new hash chain
 */
LOCALF int
bmrehash (
	dsmContext_t	*pcontext,
	bktbl_t		*pbktbl,
	ULONG		 newArea,
	DBKEY		 newdbkey,
	LONG		 newhash)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    struct  bkctl	*pbkctl = pdbcontext->pbkctl;
    QBKTBL		qhash;
    bktbl_t		*phash;
    bktbl_t		*prvOldHash;
    bktbl_t		*prvNewHash;
    LONG		oldhash;
    DBKEY		olddbkey;
	
    olddbkey = pbktbl->bt_dbkey;
    oldhash = pbktbl->bt_curhsh;
    
    phash = (bktbl_t *)NULL;
    prvNewHash = (bktbl_t *)NULL;
    prvOldHash = (bktbl_t *)NULL;
	
    BK_LOCK_HASH ();
    
    /* Find the spot where we insert the new entry. Note that we want each
       chain ordered by dbkey. This makes inserts a little slower, but
       lookups faster. Also, we could do writes in order if we wanted to. */
    
    qhash = pbkctl->bk_qhash[newhash];
    while (qhash)
    {
        phash = XBKTBL(pdbcontext, qhash);
        if (phash->bt_dbkey == newdbkey && phash->bt_area == newArea )
        {
            /* Block is there already. Someone must have put it into a
               different buffer while we were writing out the dirty block
               in this buffer before we can read the new one */
			
            BK_UNLK_HASH ();
            return (1);
        }
        if (phash->bt_dbkey > newdbkey)
        {
            /* we want to insert here */
            break;
        }
        prvNewHash = phash;
        qhash = phash->bt_qhash;
    }
    if (prvNewHash != pbktbl)
    {
        /* Block does not go in the same spot. Have to move it */
		
        if (olddbkey != (DBKEY)0)
        {
            /* Find the block in the old chain */
    		
            qhash = pbkctl->bk_qhash[oldhash];
            while (1)
            {
                if (qhash)
                {
                    if (qhash == QSELF (pbktbl))
                    {
                        /* Found it - Remove from old hash chain */
                        if (prvOldHash)
                            prvOldHash->bt_qhash = pbktbl->bt_qhash;
                        else pbkctl->bk_qhash[oldhash] = pbktbl->bt_qhash;
    					
                        /* Insert in new chain */
                        break;
                    }
                    prvOldHash = XBKTBL(pdbcontext, qhash);
                    qhash = prvOldHash->bt_qhash;
                    continue;
                }
                /* Missing block. It is not in the hash chain */
    			
                pcontext->pdbcontext->pdbpub->shutdn = DB_EXBAD;
                BK_UNLK_HASH ();
                BK_UNLK_BKTBL (pbktbl);
                FATAL_MSGN_CALLBACK(pcontext, bkFTL054, olddbkey);
            }
        }
        /* Insert in new chain */
		
        if (prvNewHash)
        {
            /* connect new block after prvNewHash */
    		
            pbktbl->bt_qhash = prvNewHash->bt_qhash;
            prvNewHash->bt_qhash = QSELF (pbktbl);
        }
        else
        {
            /* First one - head of chain points to new block */
    		
            pbktbl->bt_qhash = pbkctl->bk_qhash[newhash];
            pbkctl->bk_qhash[newhash] = QSELF(pbktbl);
        }
    }
    /* Set the new block's dbkey and hash value */
	
    pbktbl->bt_area = newArea;
    pbktbl->bt_dbkey = newdbkey;
    pbktbl->bt_curhsh = newhash;
	
    if (olddbkey == (DBKEY)0) pbkctl->bk_inuse++;
	
    BK_UNLK_HASH ();
	
    return (0);

}  /* end bmrehash */


/* PROGRAM: bmwrold - writes out a modified buffer
 *
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
bmwrold (
	dsmContext_t	*pcontext,
	bktbl_t	*pbktbl)
{
#if TIMING
    ULONG   t1;
#endif
	
    /* Write out a modified buffer */
	
    STATINC (bfwrCt); /* buffers written */
	
#if TIMING
    if (pbkctl->bk_timing)
    {
        /* timing has overhead on many machines so it is optional */

        t1 = GETUSCLK ();
		
        bmFlush(pcontext, pbktbl, BUFFERED);
		
        t1 = GETUSCLK () - t1;
        STATADD (dbwrTm, t1);
        return;
    }
#endif
    /* Write the modified block */
	
    bmFlush (pcontext, pbktbl, BUFFERED);

}  /* end bmwrold */


/* PROGRAM: bmrdnew - read a database block and put it into the buffer pool
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
bmrdnew (
	dsmContext_t	*pcontext,
	struct	bkctl	*pbkctl,
	bktbl_t		*pbktbl,
	int		 action)
{

	struct	bkbuf	*pbkbuf;
#if 0 /* include if bmLocateBlock on object block OK to call here */
        bmBufHandle_t    bufHandle;
        objectBlock_t   *pobjectBlock;
#endif
#if TIMING
	ULONG		t1;
#endif

#ifdef BKTRACE
MSGD ("%Lbmrdnew: read %D flags %X state %i count %i",
      pbktbl->bt_dbkey, bkf2l (pbktbl), pbktbl->bt_busy, pbktbl->bt_usecnt);
#endif

/**** TODO: This is an isssue!! the hiwater mark is now in the 
 *          object block.  If we attempt to locate the the
 *          object block here could cause deadlock?  Could the fact
 *          that we are not locking the master block here be the cause
 *          of unexplainable bksteal attempt to read above the high water
 *          mark errors?  (richb)
 */
#if 0  /* TODO: will the following cause an infinite loop? */
    bufHandle = bmLocateBlock (pcontext, pbktbl->bt_area, 
                         BLK2DBKEY(BKGET_AREARECBITS(pbktbl->bt_area)), TOREAD);
    pobjectBlock = (objectBlock_t *)bmGetBlockPointer(pcontext,bufHandle);
    if (pbktbl->bt_dbkey >
            ((DBKEY)(pobjectBlock->hiWaterBlock + 1) << 
              BKGET_AREARECBITS(pbktbl->bt_area)) )
    {
        pcontext->pdbcontext->pdbpub->shutdn = DB_EXBAD;
        FATAL_MSGN_CALLBACK(pcontext, bkFTL055, pbktbl->bt_dbkey,
              (DBKEY)pobjectBlock->hiWaterBlock << BKGET_AREARECBITS(pbktbl->bt_area));
    }
    bmReleaseBuffer (pcontext, bufHandle);
#endif

    /* Zero the activity counters */

    pbktbl->bt_AccCnt = 0;
    pbktbl->bt_UpdCnt = 0;
    pbktbl->bt_WrtCnt = 0;
    pbktbl->bt_ConCnt = 0;

    /* default is no recycling */

    pbktbl->bt_cycle = 0;

    pbkbuf = XBKBUF(pdbcontext, pbktbl->bt_qbuf);
    if (action == TOCREATE)
    {
	/* Just manufacture th block in memory. No need to read it */

	stnclr ((TEXT *)&pbkbuf->bk_hdr, sizeof(struct bkhdr));
	pbkbuf->BKDBKEY = pbktbl->bt_dbkey;
	bkaddr (pcontext, pbktbl,1);
	return;
    }
    BK_UNLK_BKTBL (pbktbl);

    STATINC (bfrdCt); /* Buffers read */

#if TIMING
    if (pbkctl->bk_timing)
    {
        /* timing has overhead on many machines so it is optional */

        t1 = GETUSCLK ();

        /* read the requested block */

        bkRead(pcontext, pbktbl);

        t1 = GETUSCLK () - t1;
        STATADD (dbrdTm, t1);
    }
    else
#endif
    {
        /* read the requested block */

        bkRead(pcontext, pbktbl);
    }
    BK_LOCK_BKTBL (pbktbl);

    if ((pbkbuf->BKTYPE == IDXBLK) || (pbkbuf->BKTYPE == IXANCHORBLK))
    {
        /* Allow this block to recycle on the lru chain a few times */

        pbktbl->bt_cycle = pbkctl->bk_recycle;
    }

    /* while we were doing i/o, someone could have queued up for it */

    if (pbktbl->bt_qfstuser)
        bmwake (pcontext, pbktbl, EXCLWAIT);

}  /* end bmrdnew */


/* PROGRAM: bmsteal - Take a buffer and read the desired block into it.
 *      If the block is modified, it is written out before the new
 *      block is read.
 *      The usecount of the block is incremented. Its state will be
 *      set to BKSTSHARE. The caller must set it to the correct value.
 *
 * RETURNS: RC_OK	    block read successfully
 *          RC_BK_NOBUF	    no buffers available
 *          RC_BK_DUP	    block read by someone else
 */
LOCALF int
bmsteal (
        dsmContext_t	*pcontext,
	LONG	hash,
	ULONG  blkArea,
	DBKEY	blkdbkey,
	int	action,
	struct	bktbl	**ppbktbl)
{
    usrctl_t    *pusr = pcontext->pusrctl;
    dbcontext_t  *pdbcontext = pcontext->pdbcontext;
    struct	bkctl	*pbkctl = pdbcontext->pbkctl;
    struct	bktbl	*pbktbl;

	
    /* get the least recently used buffer which is free and not modified */

    pbktbl = (bktbl_t *)NULL;

    if ( pusr->uc_ROBuffersMax &&
         (action == TOREAD) )
    {
        /* A user marked for private read read only buffers is attempting to
         * steal a buffer.
         * If the max buffers is reached, then steal one out of its own
         * LRU.  Otherwise, take a buffer out of the general LRU and
         * put it on the private read only LRU.
         */

        /* lock private read only buffer (by locking the LRU chain)
         * to protect a different user stealing
         * from our buffer while we are updating it.
         * Here what we are really protecting is the uc_seqBuffers value
         * from getting updated.  The chain itself is protected in bmgetlru.
         */
        BK_LOCK_LRU(XBKCHAIN(pdbcontext, pusr->uc_qprivReadOnlyLRU));

        if (XBKCHAIN(pdbcontext, pusr->uc_qprivReadOnlyLRU)->bk_nent >= 
            pusr->uc_ROBuffersMax)
        {
            /* max private read only buffers reached.  Steal one of our own. */
            pbktbl = bmgetlru(pcontext,
                        XBKCHAIN(pdbcontext, pusr->uc_qprivReadOnlyLRU), 0);
        }
        else
        {
            /* steal a buffer out of the general buffer pool and put it 
             * on our private read only buffer LRU chain
             */
            pbktbl = bmgetlru(pcontext, &(pbkctl->bk_lru[BK_GNLRU]), 1);
        }

        BK_UNLK_LRU(XBKCHAIN(pdbcontext, pusr->uc_qprivReadOnlyLRU));
    }
    else
    if ((pdbcontext->argprbuf > 0) &&
        (pbkctl->bk_numsbuf)       &&
        (action == TOREAD))
    {
        /* First we try the secondary pool to reduce activity on the main
           pool */
 
        pbktbl = bmgetlru (pcontext, &(pbkctl->bk_lru[BK_SELRU]), 0);
        if (pbktbl == (bktbl_t *)NULL)
        {
            /* none available, try main pool */
 
            pbktbl = bmgetlru (pcontext, &(pbkctl->bk_lru[BK_GNLRU]), 0);
        }
    }
    else
    {
        /* use main pool */
        pbktbl = bmgetlru (pcontext, &(pbkctl->bk_lru[BK_GNLRU]), 0);
    }

    if (pbktbl == (bktbl_t *)NULL)
    {
        /* no buffer available. */
		
        return (RC_BK_NOBUF);
    }
#ifdef BKTRACE
MSGD ("%Lbmsteal: take %D flags %X state %i count %i",
      pbktbl->bt_dbkey, bkf2l (pbktbl), pbktbl->bt_busy, pbktbl->bt_usecnt);
#endif

    pbktbl->bt_usecnt++;
    pbktbl->bt_busy = BKSTIO;
    BK_PUSH_LOCK (pcontext->pusrctl, pbktbl);

    /* Write out old contents if it is dirty */

    if (pbktbl->bt_flags.changed)
    {
        bmwrold(pcontext, pbktbl);
        if(pbktbl->bt_qfstuser)
        {
            /* While writing it out we got more users queued for
               the block so give up on it.  If we replace the contents
               of the buffer while others are queued for it, it can
               result in dead lock with users queued for this buffer
               who will never be woken up but are holding the txe lock. */
            bmdecrement(pcontext, pbktbl);
            BK_UNLK_BKTBL(pbktbl);
            return (RC_BK_NOBUF);
        }
    }

    /* Now we have to remove the block from its current hash chain
       and insert it in the new one before we read the block. This is
       so that if someone else comes looking for the same block while
       we are reading it, they will find it and wait for the read to
       complete */

    if (bmrehash (pcontext, pbktbl, blkArea, blkdbkey, hash))
    {
        /* By the time we got the hash chain locked, someone else had
           already added the same block in a different buffer. So we
           have to release this buffer and start over. The old block
           is still in the buffer we had planned to use. We have to
           release the buffer lock we got for i/o above */
		
        bmdecrement(pcontext, pbktbl); 
		
        BK_UNLK_BKTBL (pbktbl);
        return (RC_BK_DUP);
    }
    /* read the block */
    
    bmrdnew(pcontext, pbkctl, pbktbl, action);

    /* Block is now in memory. State is still BKSTIO. The bktbl is locked
       so no one else can access it.  Later we will set the correct state
       in bkloc. */

    *ppbktbl = pbktbl;

    return (RC_OK);

}  /* end bmsteal */


/* PROGRAM: bmLocateBlock - given the dbkey for a disk block, locate the
 *      	    indicated block in one of the buffer pools.
 *                  See bmLockBuffer for details of arguments.
 */
bmBufHandle_t
bmLocateBlock (
	dsmContext_t	*pcontext,
	ULONG		 area,          /* Area the block is in.         */
	DBKEY		 dbkey,		/* dbkey of requested block, low-
                                           order RECBITS bits are zero */
        int	action)                 /* buffer lock mode            */
{
    return(bmLockBuffer(pcontext,area,dbkey,action,1 /* wait */));
}
    

                                           
/* PROGRAM: bmLockBuffer - given the dbkey for a disk block, locate the
 *      	    indicated block in one of the buffer pools.
 *
 *      The action field can be one of TOREAD, TOMODIFY, TOREADINTENT,
 *      or TOBACKUP.
 *
 *      The requested block can be in one of the following states:
 *
 *      not in memory		All requests are granted, but the block
 *      			has to be read into memory first. This may
 *      			require stealing some other in memory block
 *      			to find a place to read the block into.
 *      			If the block to be stolen is dirty, it
 *      			is written first.
 *
 *      free			In memory but no process is using it.
 *      			All requests granted immediately.
 *
 *      share locked		In memory and one or more processes are
 *      			reading it.
 *      			Share lock requests are granted if the
 *      			queue is empty.
 *      			Exclusive lock requests are queued until all
 *      			current share locks are released.
 *      			Intent lock requests are granted if the
 *      			queue is empty and queued otherwise
 *
 *      exclusive locked	One process is reading and modifying it.
 *      			Share lock requests are queued.
 *      			Exclusive lock requests are queued
 *      			Intent lock requests are queued.
 *
 *      intent locked		One or more processes have share locks
 *      			and one process is going to upgrade
 *      			his share lock to exclusive.
 *      			Share lock requests are queued.
 *      			Exclusive lock requests are queued.
 *
 *      queued for backup	Some process has requested an exclusive or
 *      			intent lock on the block, but it has to be
 *      			backed up before the lock can be granted.
 *      			All requests are queued.
 *
 *      disk read		The block is currently being read from disk
 *      			All requests are queued.
 *
 *      disk write		The block is currently being written to
 *      			disk, most likely so it can be replaced.
 *      			All requests are queued.
 *
 *
 * RETURNS: Pointer to buffer containing requested record, else FATAL's.
 */
bmBufHandle_t
bmLockBuffer (
	dsmContext_t	*pcontext,
	ULONG		 area,          /* Area the block is in.              */
	DBKEY		 dbkey,		/* dbkey of requested block, low-
                                           order RECBITS bits are zero */
        int	         action,        /* TOMODIFY - if intend to modify
                                           TOREAD - if intend for read only
                                           TOBACKUP - if intend to back up */
        LONG             wait)          /* 1 - wait if buffer mode lock
                                           conflict.                       */
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    struct	bkctl	*pbkctl = pdbcontext->pbkctl;
    struct	bktbl	*pbktbl;
    struct	bkbuf	*pbkbuf;
    QBKTBL		qhash;
    DBKEY		blkdbkey;
    LONG		hash;
    int	                updatelru;
    COUNT		nobufCount = 0;
    int                 ret;
    ULONG               retvalue;    /* return value from latSetCount() */
    int                 counttype;   /* 0=waitcnt, 1=lockcnt */
    int                 codetype;    /* type of wait requested */

    bkAreaDescPtr_t    *pbkAreaDescPtr;
    bkAreaDesc_t       *pbkAreaDesc;

#if 1  /* bmLocateBlock is called by bkLoadAreas BEFORE pbkAreaDesc is setup */
    /* check for obviously illegal dbkeys */
    pbkAreaDescPtr = XBKAREADP(pdbcontext, pdbcontext->pbkctl->bk_qbkAreaDescPtr);
    pbkAreaDesc    = XBKAREAD(pdbcontext, pbkAreaDescPtr->bk_qAreaDesc[area]);
    if ( (dbkey < BLK1DBKEY(pbkAreaDesc->bkAreaRecbits)) ||
         (dbkey >= pbkAreaDesc->bkMaxDbkey) )
	    FATAL_MSGN_CALLBACK(pcontext, bkFTL001, dbkey);
#else
    if (dbkey < BLK1DBKEY(pbkAreaDesc->bkAreaRecbits))
        FATAL_MSGN_CALLBACK(pcontext, bkFTL001, dbkey);
#endif
	
    /* get dbkey of requested block */
	
    blkdbkey = ((DBKEY) (dbkey & (~ RECMASK(pbkAreaDesc->bkAreaRecbits))) );
	
    /* use hash table to check if block in memory */
#if 0	
    hash = BKHASHDBKEY (blkdbkey);
#endif
    hash = BKHASHDBKEY (blkdbkey, area, pbkAreaDesc->bkAreaRecbits);
	
#ifdef BKTRACE
	MSGD ("%Lbkloc: %D action %i hash %i", blkdbkey, action, hash);
#endif
	
  retry:
		
    updatelru = 1;
    pbktbl = (bktbl_t *)NULL;
	
    BK_LOCK_HASH ();
	
    /* scan the hash chain to find the requested dbkey */
    /* if the value of bk_hash is large enough, the chains will be short */
	
    qhash = pbkctl->bk_qhash[hash];
    while (qhash)
    {
        pbktbl = XBKTBL(pdbcontext, qhash);
        if (pbktbl->bt_dbkey == blkdbkey && pbktbl->bt_area == area )
        {
            BK_UNLK_HASH ();
            goto foundit;
        }
        if (pbktbl->bt_dbkey > blkdbkey)
        {
            qhash = (QBKTBL)NULL;
            break;
        }
        qhash = pbktbl->bt_qhash;
    }
    BK_UNLK_HASH ();
	
    if (qhash)
    {
foundit:	/* FOUND - the block is in memory */
        nobufCount = 0;
		
        BK_LOCK_BKTBL (pbktbl);
        if (pbktbl->bt_dbkey != blkdbkey || pbktbl->bt_area != area)
        {
            /* by the time we got the lock for the block's bktbl, it moved
               so we have to try again */
			
            BK_UNLK_BKTBL (pbktbl);
            goto retry;
        }
#ifdef BKTRACE
        MSGD ("%Lbkloc: found %D state %i count %i flags %X",
              pbktbl->bt_dbkey, pbktbl->bt_busy, pbktbl->bt_usecnt, bkf2l (pbktbl));
#endif
        if ((pbktbl->bt_busy == BKSTFREE) && (action == TOREAD))
        {
            /* most common path - do it as fast as possible */
			
            pbktbl->bt_busy = BKSTSHARE;
            pbktbl->bt_usecnt++;
            BK_PUSH_LOCK (pcontext->pusrctl, pbktbl);
			
            /* Count accesses */
			
            pbktbl->bt_AccCnt++;
			
            BK_UNLK_BKTBL (pbktbl);

            bmChooseAndDoLRU(pcontext, pbktbl);
			
            counttype = 1;
            codetype = SHAREWAIT;
            /* update the lockcnt in the latch structure */
            retvalue = latSetCount(pcontext, counttype, codetype, LATINCR);
            pcontext->pusrctl->lockcnt[SHAREWAIT]++;
			
            return (QSELF(pbktbl));
        }
        switch (pbktbl->bt_busy)
        {
            case BKSTFREE:	/* not locked - we can probably use it */
                FASSERT_CB(pcontext, pbktbl->bt_usecnt == 0, "free in use");
                break;
				
            case BKSTSHARE:    /* locked for shared access */
                if ((action == TOREAD) &&
                    (pbktbl->bt_qfstuser == (QUSRCTL)NULL))
                {       
                    /* Locked for shared access and no one waiting
                       so we can give it to him */
					
                    break; 
                }
                else if (action == TOBACKUP)
                {
                    /* Block is going to be backed up, so we give the backup
                       a share lock ahead of everyone waiting */
					
                    break;
                }
                /* have to wait for the block to become free so the guy
                   who is waiting will get a chance */
                if ( wait == 0 )
                {
                    /* Caller does not want to wait */
                    BK_UNLK_BKTBL (pbktbl);
                    return (bmBufHandle_t)0;
                }
 
				
                bmblkwait (pcontext, pbktbl, action);
                goto retry;
				
            case BKSTINTENT:    /* locked for shared access intend to mod */
                if (action == TOREAD)
                {
                    if (pbktbl->bt_qfstuser == (QUSRCTL)NULL ) 
                    {
                        /* Locked for shared access and no one waiting
                           so we can give it to him      */
                        break;
                    }
				   
                }
                else if (action == TOBACKUP)
                {
                    /* Block is going to be backed up, so we give the backup
                       a share lock ahead of everyone waiting */
					
                    break;
                }
                /* have to wait for the block to become free so the guy
                   who is waiting will get a chance */
                if ( wait == 0 )
                {
                    /* Caller does not want to wait */
                    BK_UNLK_BKTBL (pbktbl);
                    return (bmBufHandle_t)0;
                }
                
                bmblkwait (pcontext, pbktbl, action);
                goto retry;
				
            case BKSTEXCL:	/* locked - for exclusive access */  
                if (pbktbl->bt_qcuruser == QSELF(pcontext->pusrctl))
                {
                    /* Locked for exclusive access, but holder has requested
                       the same block again. Give it to him. But change
                       the action to modify so that the lock wont change
                       from exclusive to shared down below */
					
                    action = TOMODIFY;
                    break;
                }
                if (action == TOBACKUP)
                {
                    /* The online backup does not wait for buffer locks
                       To do so would risk buffer locking deadlocks    */
					
                    BK_UNLK_BKTBL(pbktbl);
                    return (0);
                }
				
                /* have to wait for the block to become free */
                if ( wait == 0 )
                {
                    /* Caller does not want to wait */
                    BK_UNLK_BKTBL (pbktbl);
                    return (bmBufHandle_t)0;
                }
  		
                bmblkwait (pcontext, pbktbl, action);
                goto retry;
				
            case BKSTIO:	/* locked - read from or write to disk */
                if ( wait == 0 )
                {
                    /* Caller does not want to wait */
                    BK_UNLK_BKTBL (pbktbl);
                    return (bmBufHandle_t)0;
                }
  				
                bmblkwait (pcontext, pbktbl, action);
                goto retry;
        }

        /* use the buffer which we found in memory */
		
        pbktbl->bt_usecnt++;
        BK_PUSH_LOCK (pcontext->pusrctl, pbktbl);
    }
    else
    {
        /* NOT FOUND - the block is not in memory so
           we try to steal the least recently used buffer and read the
           requested block into it */
		
        ret = bmsteal (pcontext, hash, area, blkdbkey, action, &pbktbl);
        if (ret == RC_BK_NOBUF)
        {
            /* no buffer is available */
			
            if ((pcontext->pdbcontext->argnoshm) || (nobufCount > 5))
            {
                /* fatal, bkloc, not enough buffers in pool */
				
                pcontext->pdbcontext->pdbpub->shutdn = DB_EXBAD;
                FATAL_MSGN_CALLBACK(pcontext, bkFTL033);
            }
            /* we can wait for someone else to free a buffer. This is
               an extremely rare event, so we can just sleep a little
               and try again. */
			
            nobufCount++;
            counttype = 0;
            codetype = NOBUFWAIT;
            /* update the waitcnt in the latch structure */
            retvalue = latSetCount(pcontext, counttype, codetype, LATINCR);
            /* pcontext->pdbcontext->pmtctl->waitcnt[NOBUFWAIT]++; */
            pcontext->pusrctl->waitcnt[NOBUFWAIT]++;
			
            utsleep (1);
            goto retry; /* love those goto's */
			
        }
        if (ret == RC_BK_DUP) goto retry;
		
        /* got a free buffer which now has the block in it, the
           use count has been bumped, and the state is BKSTIO. The bktbl
           is locked and the lru chain was updated */
		
           pbktbl->bt_busy = BKSTFREE;
           updatelru = 0;
    }
    nobufCount = 0;
	
    /* At this point most access conflicts have been resolved, except for
       blocks that need to be queued for backup before they are modified */
	
    pbkbuf = XBKBUF(pdbcontext, pbktbl->bt_qbuf);
	
    /* Make sure that the buffer contains the correct dbkey. It has
       failed more than once due to bugs, hardware and device driver
       problems! */
	
    if (pbkbuf->BKDBKEY != blkdbkey)
    {
        bmdecrement(pcontext, pbktbl);
		
        BK_UNLK_BKTBL (pbktbl);
        pcontext->pdbcontext->pdbpub->shutdn = DB_EXBAD;
        FATAL_MSGN_CALLBACK(pcontext, bkFTL004,
               blkdbkey >> pbkAreaDesc->bkAreaRecbits, pbkbuf->BKDBKEY);
    }
	
    switch (action)
    {
        case TOREAD:
            /* block is being locked for shared access */
			
            if (pbktbl->bt_busy != BKSTINTENT)
                pbktbl->bt_busy = BKSTSHARE;
            counttype = 1;
            codetype = SHAREWAIT;
            /* update the lockcnt in the latch structure */
            retvalue = latSetCount(pcontext, counttype, codetype, LATINCR);
            pcontext->pusrctl->lockcnt[SHAREWAIT]++;
            break;
			
        case TOMODIFY:
        case TOCREATE: 
            if (pbktbl->bt_busy == BKSTEXCL)
            {
                FASSERT_CB(pcontext, 
                    pbktbl->bt_qcuruser == QSELF(pcontext->pusrctl), "ex usr"); 
				
                /* we already have an exlcusive lock on this block */
				
                counttype = 1;
                codetype = EXCLWAIT;
                /* update the lockcnt in the latch structure */
                retvalue = latSetCount(pcontext, counttype, codetype, LATINCR);
                pcontext->pusrctl->lockcnt[EXCLWAIT]++;
                break;
            }
            /* block is now locked for exclusive access */
			
            pbktbl->bt_qcuruser = QSELF(pcontext->pusrctl); 
            pbktbl->bt_busy = BKSTEXCL;
            counttype = 1;
            codetype = EXCLWAIT;
            /* update the lockcnt in the latch structure */
            retvalue = latSetCount(pcontext, counttype, codetype, LATINCR);
            pcontext->pusrctl->lockcnt[EXCLWAIT]++;
            break;
			
        case TOBACKUP:
            /* block is going to be backed up now so we lock it */
			
            updatelru = 0;
            if ((pbktbl->bt_busy == BKSTSHARE) ||
                (pbktbl->bt_busy == BKSTFREE))
            {
                /* Block was free or shared, so we can make a share lock */
				
                pbktbl->bt_busy = BKSTSHARE;
                counttype = 1;
                codetype = SHAREWAIT;
                /* update the lockcnt in the latch structure */
                retvalue = latSetCount(pcontext, counttype, codetype, LATINCR);
                pcontext->pusrctl->lockcnt[SHAREWAIT]++;
                break;
            }
            else if (pbktbl->bt_busy == BKSTINTENT)
            {
                /* someone has intend to write lock so we share it */
				
                counttype = 1;
                codetype = SHAREWAIT;
                /* update the lockcnt in the latch structure */
                retvalue = latSetCount(pcontext, counttype, codetype, LATINCR);
                pcontext->pusrctl->lockcnt[SHAREWAIT]++;
                break;
            }
            bmdecrement(pcontext, pbktbl);
            BK_UNLK_BKTBL (pbktbl);
			
            pcontext->pdbcontext->pdbpub->shutdn = DB_EXBAD;
            FATAL_MSGN_CALLBACK(pcontext, bkFTL059, action, pbktbl->bt_busy);
            break;
			
        case TOREADINTENT:
            /* read with intent to modify */
			
            if ((pbktbl->bt_busy == BKSTSHARE) || 
                (pbktbl->bt_busy == BKSTFREE))
            {
                /* block was free or shared so we can make an intent lock */
                pbktbl->bt_busy = BKSTINTENT;
                pbktbl->bt_qcuruser = QSELF(pcontext->pusrctl); 
                counttype = 1;
                codetype = INTENTWAIT;
                /* update the lockcnt in the latch structure */
                retvalue = latSetCount(pcontext, counttype, codetype, LATINCR);
                pcontext->pusrctl->lockcnt[INTENTWAIT]++;
                break;
            }
			
            /* fatal error - attempt to illegal intent lock */
            bmdecrement(pcontext, pbktbl);
            BK_UNLK_BKTBL (pbktbl);
			
            pcontext->pdbcontext->pdbpub->shutdn = DB_EXBAD;
            FATAL_MSGN_CALLBACK(pcontext, bkFTL059, action, pbktbl->bt_busy);
            break;
			
        default:
            bmdecrement(pcontext, pbktbl);
            BK_UNLK_BKTBL (pbktbl);
			
            pcontext->pdbcontext->pdbpub->shutdn = DB_EXBAD;
            FATAL_MSGN_CALLBACK(pcontext, bkFTL059, action, pbktbl->bt_busy);
            break;
    }
    /* Count accesses */
    pbktbl->bt_AccCnt++;

    /* update the lru chain - but not for blocks being backed up, because
       we don't want to trash the buffer pool and backup will access every
       block once */
	
    BK_UNLK_BKTBL (pbktbl);
    if (updatelru)
    {
        bmChooseAndDoLRU(pcontext, pbktbl);
    }
	
#ifdef BKTRACE
	MSGD ("%Lbkloc: got %D state %i count %i",
         pbktbl->bt_dbkey, pbktbl->bt_busy, pbktbl->bt_usecnt, bkf2l (pbktbl));
#endif
	
    /* accumulate stats on accessed blocks */
	
    return (QSELF(pbktbl));

}  /* end bmLockBuffer */


/* PROGRAM: bmGetBlockPointer - Given the handle to a buffer return a real
 *                          pointer to the block in the buffer.
 */
struct bkbuf *
bmGetBlockPointer(
    dsmContext_t   *pcontext,
    bmBufHandle_t   bufHandle)
{

    bktbl_t    *pbktbl;
    
    pbktbl = XBKTBL(pdbcontext,bufHandle);
    
    return(XBKBUF(pdbcontext,pbktbl->bt_qbuf));

}  /* end bmGetBlockPointer */


/* PROGRAM: bmGetArea -     Given the handle to a buffer return 
 *                          the area number for the buffer.
 */
ULONG 
bmGetArea(
 	dsmContext_t	*pcontext,
	bmBufHandle_t	 bufHandle)
{
    bktbl_t   *pbktbl;

    pbktbl = XBKTBL(pcontext->pdbcontext, bufHandle);
    return(pbktbl->bt_area);

}  /* end bmGetArea */


/* PROGRAM: bmReleaseBuffer - decrement the use count in the usecnt field
 *      		   if the count is 0 then
 *      		   release buffer from the fixing in memory 
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bmReleaseBuffer (
	dsmContext_t	*pcontext,
	bmBufHandle_t	 bufHandle)    /* identifies modified buffer */
{

	struct  bktbl   *pbktbl;
    	struct  bkbuf   *pbkbuf;

    if (bufHandle) /* called sometimes with no buffer, see rmloc */
    {
        pbktbl = XBKTBL(pdbcontext, bufHandle);
	pbkbuf = XBKBUF(pdbcontext, pbktbl->bt_qbuf);

#ifdef BKTRACE
MSGD ("%LbmReleaseBuffer: %D state %i count %i flags %X",
      pbktbl->bt_dbkey, pbktbl->bt_busy, pbktbl->bt_usecnt, bkf2l (pbktbl));
#endif

        BK_LOCK_BKTBL (pbktbl);

        if (pbktbl->bt_dbkey != pbkbuf->BKDBKEY)
        {
            /* Corrupt block while releasing a buffer - #95-02-24-024 */
            MSGN_CALLBACK(pcontext, bkMSG110);
            /* wrong dbkey in block. Found <dbkey>%D, should be <dbkey>%D */
            pcontext->pdbcontext->pdbpub->shutdn = DB_EXBAD;
            FATAL_MSGN_CALLBACK(pcontext, bkFTL056,
                          pbkbuf->BKDBKEY, pbktbl->bt_dbkey);

        }

        if (pbktbl->bt_usecnt > 0)
        {
            /* release the buffer */

            bmdecrement(pcontext, pbktbl);

            BK_UNLK_BKTBL (pbktbl);
	    return;
	}
        BK_UNLK_BKTBL (pbktbl);

	pcontext->pdbcontext->pdbpub->shutdn = DB_EXBAD;
        /* bkrlsbuf - buffer not in use */ 
        FATAL_MSGN_CALLBACK(pcontext, bkFTL044, pbktbl->bt_usecnt);
    }

}  /* end bmReleaseBuffer */


/* PROGRAM: bmLockDownMasterBlock - read the frst 2 database blocks
 *             into reserved buffers and return a pointer to first one. 
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bmLockDownMasterBlock (
	dsmContext_t	*pcontext)

{

    struct  bkctl       *pbkctl = pcontext->pdbcontext->pbkctl;
    bmBufHandle_t        bmHandle1;
    struct bktbl	*pbktbl1;
    struct mstrblk	*pmstrblk;

    /* get the master block, set shared pointers,
       lock it into memory so it cant move, take it off the lru chain */

    bmHandle1 = bmLocateBlock (pcontext, DSMAREA_CONTROL, 
                          BLK1DBKEY(BKGET_AREARECBITS(DSMAREA_CONTROL)), TOREAD);
    pmstrblk = (struct mstrblk *)bmGetBlockPointer(pcontext,bmHandle1);
    pbktbl1 =  XBKTBL(pdbcontext, bmHandle1);
    pcontext->pdbcontext->pdbpub->qmstrblk = QSELF(pbktbl1);
    pcontext->pdbcontext->pmstrblk = pmstrblk;

    pbktbl1->bt_flags.fixed = 1;
    bmDeqLru (pcontext, pbkctl->bk_lru + (int)pbktbl1->bt_whichlru, pbktbl1);

    /* release the blocks - since they are not on the LRU chain, they
       wont get replaced */
 
    bmReleaseBuffer (pcontext, bmHandle1);

}  /* end bmLockDownMasterBlock */


/* PROGRAM: bmMarkModified - indicate that the contents of a buffer have
 *      	been modified.
 *
 *      	If the contents of this buffer should not be written
 *      	to the disk until a particular bi note is written
 *      	out to the bi file, then the bi file offset of the
 *      	byte past the end of the note is passed as a parameter.
 *      	This allows the lower level I/O routines to preserve
 *      	this ordering.
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bmMarkModified (
	dsmContext_t	*pcontext,
	bmBufHandle_t	 bufHandle,	/* identifies modified buffer */
	LONG64		 bireq)		/* this block cannot be written
					   until at least this much bi
					   has been reliably written */

{

	struct	bktbl	*pbktbl;
	struct bkctl    *pbkctl = pcontext->pdbcontext->pbkctl;
	struct bkbuf    *pbkbuf;
        struct mstrblk	*pmstrblk = pcontext->pdbcontext->pmstrblk;
	
    pbktbl = XBKTBL(pdbcontext, bufHandle);
    pbkbuf = XBKBUF(pdbcontext, pbktbl->bt_qbuf);
#ifdef BKTRACE
MSGD ("%LbmMarkModified: %D state %i count %i flags %X",
      pbktbl->bt_dbkey, pbktbl->bt_busy, pbktbl->bt_usecnt, bkf2l (pbktbl));
#endif
#if BKDEBUG
    /* To mark a block dirty, you must have it locked for exclusive access
       except after image buffer, master block, index anchor block */

    if ((pbktbl->bt_busy != BKSTEXCL) && (pbktbl->bt_dbkey > 
                        BLK2DBKEY(BKGET_AREARECBITS(pbktbl->bt_area))))
    {
	pcontext->pdbcontext->pdbpub->shutdn = DB_EXBAD;
        FATAL_MSGN_CALLBACK(pcontext, bkFTL061, pbktbl->bt_dbkey);
    }

#endif

    if (pbkbuf->BKDBKEY != pbktbl->bt_dbkey)
    {
	pcontext->pdbcontext->pdbpub->shutdn = DB_EXBAD;

        /* Corrupt block while modifing - #95-02-24-024 */
        MSGN_CALLBACK(pcontext, bkMSG109);
        /* wrong dbkey in block. Found <dbkey>%D, should be <dbkey>%D */
        pcontext->pdbcontext->pdbpub->shutdn = DB_EXBAD;
	FATAL_MSGN_CALLBACK(pcontext, bkFTL056, pbkbuf->BKDBKEY, pbktbl->bt_dbkey);
    }

    STATINC (bfmdCt); /* Buffers modified */

    pbktbl->bt_UpdCnt++;

    /* This block can not be written to disk until this much bi has been
       reliably written first */

    pbktbl->bt_bidpn = bireq;
    if (pbktbl->bt_flags.changed == 0)
    {
        BK_LOCK_BKTBL (pbktbl);

        /* This block just became dirty */

        pbktbl->bt_flags.changed = 1;

	/* set the write counter for incremental backup */

	pbkbuf->BKINCR = pmstrblk->BKINCR;

	BK_UNLK_BKTBL (pbktbl);

	/* this counter is incremented without locking, so may be slightly
	   inaccurate. This is ok since it is only for promon */

	pbkctl->bk_numdirty++;
    }
    else STATINC (bfmmCt); /* modified buffers modified */

}  /* end bmMarkModified */


/* PROGRAM: bmFlushx - flush modified blocks in the buffer pool
 *      	to their corresponding locations on the disk.
 *
 *      	Note that blocks are scheduled for checkpointing by a
 *      	page writer if they were first dirtied in the current
 *      	checkpoint interval. If any blocks remain from the previous
 *      	checkpoint interval, they are forced out now.
 *
 *      	If synchronous is 1, then all blocks are forced out now.
 *
 *      	Note that the masterblock may be flushed
 *      	by this routine. 
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bmFlushx (dsmContext_t	*pcontext,
          ULONG          area,
          int            synchronous)	/* 1 if doing synchronous */
{
        dbcontext_t *pdbcontext = pcontext->pdbcontext;
	struct	bktbl	*pbktbl;
	struct  bkctl   *pbkctl = pdbcontext->pbkctl;
	BKANCHOR	*pCkpChain;
	BKCPDATA	*pCkpData;
	BKCPDATA	*pOldData;
	time_t		curTime;

    pCkpChain = &(pbkctl->bk_Anchor[BK_CHAIN_CKP]);

    if (pcontext->pdbcontext->pdbpub->fullinteg)
    {
        /* make sure all .bi blocks are written first */

        rlbiflsh(pcontext, RL_FLUSH_ALL_BI_BUFFERS);
    }

    /* Update data for previous checkpoint */

    time (&curTime);
    pOldData = &(pbkctl->bk_Cp[(pbkctl->bk_chkpt & BK_CPSLOT_MASK)]);

    BK_LOCK_CHAIN (pCkpChain);

    if (pOldData->bk_cpEndTime == 0) pOldData->bk_cpEndTime = curTime;
    pOldData->bk_cpFlushed = pbkctl->bk_mkcpcnt;

    /* next checkpoint */

    pbkctl->bk_chkpt++;

    pCkpData = &(pbkctl->bk_Cp[(pbkctl->bk_chkpt & BK_CPSLOT_MASK)]);

    stnclr ((TEXT *)pCkpData, sizeof(struct bkcpdata));

    pCkpData->bk_cpBegTime = curTime;
    pCkpData->bk_cpDirty = pbkctl->bk_numdirty;

    BK_UNLK_CHAIN (pCkpChain);

    /* loop over all the buffers */

    pbktbl = XBKTBL(pdbcontext, pbkctl->bk_qbktbl);
    while (pbktbl != (bktbl_t *)NULL)
    {
	BK_LOCK_BKTBL (pbktbl);
	pbktbl->bt_bidpn = 0; /* all .bi blocks were already flshed above */

        /* Only worry about blocks which are dirty and not being written
           already (by page writer, most likely) */

        if ((area == FLUSH_ALL_AREAS || pbktbl->bt_area == area) &&
            (pbktbl->bt_flags.changed) && (pbktbl->bt_flags.writing == 0))
        {
	    if ((pbktbl->bt_flags.chkpt == 0) && (!synchronous))
	    {
		/* Schedule this buffer for async checkpoint by page writers */

		STATINC (bfncCt); /* Buffers tagged for checkpoint */

		BK_LOCK_CHAIN (pCkpChain);

		/* Put buffer on checkpoint chain for page writer */

	        if (pbkctl->bk_numapw)
                    bmEnqueue (pCkpChain, pbktbl);

		pbktbl->bt_flags.chkpt = 1;
	        pbkctl->bk_mkcpcnt++;

		BK_UNLK_CHAIN (pCkpChain);
	    }
            else
            {
                /* We must write this buffer now. */

		STATINC (bfflCt); /* Buffers flushed (write for checkpoint) */

		if (pbktbl->bt_busy == BKSTFREE)
		{
		    /* put a share lock on the buffer */

		    pbktbl->bt_busy = BKSTSHARE;         
		}
		pbktbl->bt_usecnt++;
		BK_PUSH_LOCK (pcontext->pusrctl, pbktbl);

		/* write out the block */            
 
		bmFlush (pcontext, pbktbl, BUFFERED);

		/* release the buffer */

		bmdecrement(pcontext, pbktbl);
	    }
        }
        /* Release the buffer control block */

        BK_UNLK_BKTBL (pbktbl);

        /* Advance to the next buffer */

	pbktbl = XBKTBL(pdbcontext, pbktbl->bt_qnextb);
    }

    BK_LOCK_CHAIN (pCkpChain);

    pCkpData->bk_cpMarked = pbkctl->bk_mkcpcnt;

    BK_UNLK_CHAIN (pCkpChain);

}  /* end bmFlushx */


/* PROGRAM: bmFlushMasterblock - write the database master block using RAW I/O 
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bmFlushMasterblock (dsmContext_t	*pcontext)
{
                dbcontext_t *pdbcontext = pcontext->pdbcontext;
		bktbl_t	*pbktbl;
	struct  bkctl   *pbkctl = pdbcontext->pbkctl;
		LONG64	 dpend;
	BKANCHOR	*pCkpChain;

    pCkpChain = &(pbkctl->bk_Anchor[BK_CHAIN_CKP]);
    pbktbl = XBKTBL(pdbcontext, pdbcontext->pdbpub->qmstrblk);

    STATINC(mbflCt); /* Master block flushes */

    BK_LOCK_BKTBL (pbktbl);

    if (pbktbl->bt_busy == BKSTFREE)
    {
        /* put a share lock on the buffer */

        pbktbl->bt_busy = BKSTSHARE;         
    }
    pbktbl->bt_usecnt++;
    BK_PUSH_LOCK (pcontext->pusrctl, pbktbl);

    pbktbl->bt_flags.writing = 1;

    /* make sure any necessary .bi blocks are flushed first */

    dpend = pbktbl->bt_bidpn;

    BK_UNLK_BKTBL (pbktbl);
    if (pcontext->pdbcontext->pdbpub->fullinteg && dpend > 0)
    {
	/* have to force bi dependency */

	rlbiflsh (pcontext, dpend);
    }
    /* write the block */

    bkWrite(pcontext, pbktbl, UNBUFFERED);

    BK_LOCK_BKTBL (pbktbl);
    if (pbktbl->bt_flags.chkpt)
    {
        /* This buffer was marked as needing checkpointing. Now it no
           longer does */

        BK_LOCK_CHAIN (pCkpChain);

        pbktbl->bt_flags.chkpt = 0;
	pbkctl->bk_mkcpcnt--;
	pbkctl->bk_Cp[(pbkctl->bk_chkpt & BK_CPSLOT_MASK)].bk_cpNumDone++;

	/* Take the buffer off the checkpoint list */

	bmRemoveQ (pcontext, pCkpChain, pbktbl);

        BK_UNLK_CHAIN (pCkpChain);
    }
    pbktbl->bt_WrtCnt++;
    pbktbl->bt_bidpn = 0;
    pbktbl->bt_flags.writing = 0;

    if (pbktbl->bt_flags.changed)
    {
        pbktbl->bt_flags.changed = 0;
        pbkctl->bk_numdirty--;
        if (pbkctl->bk_numdirty < 0) pbkctl->bk_numdirty = 0;
    }
    bmdecrement(pcontext, pbktbl);

    BK_UNLK_BKTBL (pbktbl);

}  /* end bmFlushMasterblock */


/* PROGRAM: bmFlush - Write the block from a buffer back out to
 *      	its disk location. Note that a block must be read
 *      	before it can be written. 
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bmFlush (
	dsmContext_t	*pcontext,
	bktbl_t		*pbktbl,	/* block table entry */
	int		 mode)		/* BUFFERED or UNBUFFERED */
{
        dbcontext_t *pdbcontext = pcontext->pdbcontext;
	struct  bkbuf   *pbkbuf;
        struct  bkctl   *pbkctl = pdbcontext->pbkctl;
	bkchain_t	*pLruChain;

	LONG64	         dpend;
	BKANCHOR	*pCkpChain;

    pCkpChain = &(pbkctl->bk_Anchor[BK_CHAIN_CKP]);

    FASSERT_CB(pcontext, pbktbl->bt_usecnt, "usecount 0");
    FASSERT_CB(pcontext, pbktbl->bt_busy != BKSTFREE, "not locked");
    FASSERT_CB(pcontext, pbktbl->bt_flags.changed == 1, "not changed");

    pbkbuf = XBKBUF(pdbcontext, pbktbl->bt_qbuf);


    /* flush all the bi's this block depends upon */

#ifdef BKTRACE
MSGD ("%Lbmflsh: write %D flags %X state %i count %i",
      pbktbl->bt_dbkey, bkf2l (pbktbl), pbktbl->bt_busy, pbktbl->bt_usecnt);
#endif
    dpend = pbktbl->bt_bidpn;

    pbktbl->bt_flags.writing = 1;

    BK_UNLK_BKTBL (pbktbl);
    if (pcontext->pdbcontext->pdbpub->fullinteg && (dpend > 0))
    {
	/* have to force bi dependency */

	rlbiflsh (pcontext, dpend);
    }
    /* just write out the block */

    bkWrite(pcontext, pbktbl, mode);

    if ((pbktbl->bt_qlrunxt == (QBKTBL)NULL) && (pbktbl->bt_flags.fixed == 0))
    {
        /* This block is not on an lru chain. It was probably removed
           to put it on the modified queue so page writer could write it.
	   Put it back on the chain it was on before */
	
        pLruChain = pbkctl->bk_lru + pbktbl->bt_whichlru;

        /* Lock the LRU chain */

	BK_LOCK_LRU (pLruChain);

        /* Put it on the chain */

        bmEnqLru (pcontext, pLruChain, pbktbl);

        /* Unlock the LRU chain */

	BK_UNLK_LRU (pLruChain);
    }
    BK_LOCK_BKTBL (pbktbl);
    if (pbktbl->bt_flags.chkpt)
    {
        /* This buffer was marked as needing checkpointing. Now it no
           longer does */

        BK_LOCK_CHAIN (pCkpChain);

        pbktbl->bt_flags.chkpt = 0;
	pbkctl->bk_mkcpcnt--;
	pbkctl->bk_Cp[(pbkctl->bk_chkpt & BK_CPSLOT_MASK)].bk_cpNumDone++;

	/* Take the buffer off the checkpoint list */

	bmRemoveQ (pcontext, pCkpChain, pbktbl);

        BK_UNLK_CHAIN (pCkpChain);
    }
    pbktbl->bt_WrtCnt++;
    pbktbl->bt_bidpn = 0;
    pbktbl->bt_flags.writing = 0;
    pbktbl->bt_flags.changed = 0;

    pbkctl->bk_numdirty--;
    if (pbkctl->bk_numdirty < 0) pbkctl->bk_numdirty = 0;

}  /* end bmFlush */


/* PROGRAM: bmGetBufferPool - Allocate buffer pool buffers that are raw i/o able. 
 *		     Given a bktbl pointer allocate the buffer and hook 
 *		     then together.					
 *		     For safety sakes, all buffers are long word aligned
 * RETURNS: DSMVOID
 */
DSMVOID
bmGetBufferPool(
	dsmContext_t	*pcontext,
	struct bktbl	*pbktbl)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    bmGetBuffers(pcontext, pbktbl,
                 XSTPOOL(pdbcontext, pdbcontext->pdbpub->qdbpool), 1);
}


/* PROGRAM: bkGetBuffers - allocate a number of aligned buffers of given type
 *
 * This procedure allocates buffers in contigious memory that can be used for
 * any type of i/o.  It allocates space based on the buffer type requested.
 *
 * BKDATA  buffers are contigious in memory using bkbuf structures.
 *
 * BKBI    buffers are contigious in memory and are intended to be
 *         written as a unit to disk. mb_biblksize bytes are allocated.
 *
 * BKAI    buffers are contigious in memory and are intended to be
 *         written as a unit to disk. mb_aiblksize bytes are allocated.
 *
 * BKTL    one 16k buffer is allocated.
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bmGetBuffers(
	dsmContext_t	*pcontext,
	struct	bktbl	*pbktbl,
	STPOOL		*ppool, /* the storage pool where to allocate memory */
	int		 nbufs)  /* number to allocate */
{
	TEXT  *pbuf;		/* aligned buffer pointer */
	int   bytesNeeded;	/* number of bytes of memory needed */
   
	
    /* calculate memory needed based on type of buffer requested */
	
    if (pbktbl->bt_ftype == BKBI)
    {
        /* bi allocates variable length rlblk structures */
		
        bytesNeeded = (pcontext->pdbcontext->pmstrblk->mb_biblksize * nbufs) +
                            BK_BFALIGN;
    }
    else if (pbktbl->bt_ftype == BKTL )
    {
        bytesNeeded = (RLTL_BUFSIZE * nbufs) + BK_BFALIGN;
    }
    else if (pbktbl->bt_ftype == BKAI)
    {
	/* ai allocates variable length aiblk structures */
	bytesNeeded = (pcontext->pdbcontext->pmstrblk->mb_aiblksize * nbufs)
			 + BK_BFALIGN;
    }
    else 
    {
        /* all others allocate bkbuf structures */

        bytesNeeded = (BKBUFSIZE(pcontext->pdbcontext->pdbpub->dbblksize) *
                                 nbufs) + BK_BFALIGN;
    }
    /* allocate the buffers */
	
    pbuf = (TEXT *)stGet (pcontext, ppool, (unsigned)bytesNeeded);
	
    if (pbuf)
    {
        /* align buffer */
		
        bmAlign (pcontext, &pbuf);
    }
    else
    {
        /* no memory */
		
        FATAL_MSGN_CALLBACK(pcontext, bkFTL025);
    }
    /* connect buffer and pbktbl to each other */
	
    pbktbl->bt_qbuf = XBKBUF(pcontext, pbuf);
	
}  /* end bmGetBuffers */


/* PROGRAM bmAlign - align buffer on full word boundary  
 *
 * Note !! it assumes that enough memory was allocated for the adjustment 
 *
 * RETURNS: 0
 */
DSMVOID
bmAlign(
	dsmContext_t	*pcontext _UNUSED_,
	TEXT		**ppbuf)
{
	TEXT	*p;

    p = (TEXT *)( ((unsigned_long)*ppbuf + (BK_BFALIGN - 1)) & (~(BK_BFALIGN - 1)) );
    *ppbuf = p;
    return;
}


/* PROGRAM: bmblkwait - wait for either exclusive/share lock on buffer 
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
bmblkwait(
	dsmContext_t	*pcontext,
	struct  bktbl   *pbktbl,	/* pointer to block ctl structure */
	int		 action)	/* action which is intended */
{

	int		waitType;

    /* Need exclusive lock to modify a block */

#ifdef BKTRACE
MSGD ("%Lbmblkwait: %D state %i count %i flags %X",
      pbktbl->bt_dbkey, pbktbl->bt_busy, pbktbl->bt_usecnt, bkf2l (pbktbl));
#endif
    if (action == TOMODIFY) waitType = EXCLWAIT;
    else if (action == TOUPGRADE) waitType = EXCLWAIT;
    else waitType = SHAREWAIT;

    pcontext->pusrctl->uc_qwtnxt = (QUSRCTL)NULL;
    if ((action == TOBACKUP) || (action == TOUPGRADE))
    {
	/* put request for upgrade from read with intent to modify to
	   exclusive lock on the fron of the queue. As soon as all the
	   processes with share locks have released them, the upgrade will be
	   honored.

	   put online backup at the front of the wait queue. We do this because
	   it is possible for a process which has an exclusive lock on a block
	   to request an exclusive lock again. When he does so, the block will
	   get put on the backup queue until it has been backed up. If the
	   backup process is in the queue waiting for the block, it will never
	   be released and voila! you are hung. */

	pcontext->pusrctl->uc_qwtnxt = pbktbl->bt_qfstuser;

	/* we are the first one in the queue */

	pbktbl->bt_qfstuser = QSELF (pcontext->pusrctl);
	if (pbktbl->bt_qlstuser == (QUSRCTL)NULL)
	{
	    /* we are the only one in the queue */

	    pbktbl->bt_qlstuser = QSELF (pcontext->pusrctl);
	}
    }
    else
    {
        /* add ourselves to the end of the wait queue */

        if (pbktbl->bt_qlstuser)
	{
	    /* last one on queue points to us */

	    XUSRCTL (pdbcontext, pbktbl->bt_qlstuser)->uc_qwtnxt =
                                                      QSELF(pcontext->pusrctl);
	}
	/* we are last in the queue */

        pbktbl->bt_qlstuser = QSELF(pcontext->pusrctl);
        if (pbktbl->bt_qfstuser == (QUSRCTL)NULL)
	{
	    /* We are the only one on the queue so we are first too */

	    pbktbl->bt_qfstuser = pbktbl->bt_qlstuser;
	}
    }
    /* wait for block holder to release it */

    pcontext->pusrctl->uc_wait = (TEXT)waitType;
    pcontext->pusrctl->uc_wait1 = pbktbl->bt_dbkey;
    pcontext->pusrctl->uc_blklock = (QBKTBL)(QSELF (pbktbl));

    /* Count number of waits */

    pbktbl->bt_ConCnt++;

    /* release the buffer pool */

    BK_UNLK_BKTBL (pbktbl);

    /* wait for the block to be released */

    latUsrWait (pcontext);
    pcontext->pusrctl->uc_blklock = (QBKTBL)(NULL);

}  /* end bmblkwait */


/* PROGRAM: bmCancelWait - Cancel a buffer wait for specified user 
 *                      take him out of the queue and forget all 
 *                      about the request.
 * RETUNRS: DSMVOID
 */
DSMVOID 
bmCancelWait (dsmContext_t	*pcontext)
{

	usrctl_t     *pusr = pcontext->pusrctl;
	struct  bktbl   *pbktbl;
	int		waitCode;

    waitCode = (int)(pusr->uc_wait);
 
    switch (waitCode)
    {
	case SHAREWAIT: /* share lock on a buffer */
            pbktbl = XBKTBL(pdbcontext, pusr->uc_blklock);
            BK_LOCK_BKTBL (pbktbl);
            lkunchain (pcontext, 0, pusr, &(pbktbl->bt_qfstuser),
		       &(pbktbl->bt_qlstuser));
            BK_UNLK_BKTBL (pbktbl);
	    break;

	case EXCLWAIT:  /* exclusive lock on a buffer */
            pbktbl = XBKTBL(pdbcontext, pusr->uc_blklock);
            BK_LOCK_BKTBL (pbktbl);
            lkunchain (pcontext, 0, pusr, &(pbktbl->bt_qfstuser),
		       &(pbktbl->bt_qlstuser));
            BK_UNLK_BKTBL (pbktbl);
	    break;
    }

}  /* end bmCancelWait */


/* PROGRAM: bmPopLocks - release all blocks on the user's block lock stack
 *
 * RETURNS: number of blocks unlocked
 */
int
bmPopLocks (dsmContext_t *pcontext, usrctl_t *pusr)
{

	struct  bktbl   *pbktbl;
        struct  bkbuf   *pbkbuf;        /* identifies modified buffer */
	usrctl_t	*psaveu;
	int		n = 0;

#if BK_HAS_BSTACK

    psaveu = pcontext->pusrctl;
    while (pusr->uc_bufsp > 0)
    {
        pbktbl = XBKTBL(pdbcontext, pusr->uc_bufstack[pusr->uc_bufsp - 1]);
        pbkbuf = XBKBUF(pdbcontext, pbktbl->bt_qbuf);
        if (pbktbl->bt_busy >= BKSTEXCL)
        {
            /* we had an exclusive lock on the buffer. So it may be
               corrupted since we may have died while updating it.
               So we have to mark the database for death */
 
            /* the caller of this in all cases will print out a message
               stating why we are shutting down */
            pcontext->pdbcontext->pdbpub->shutdn = DB_EXBAD;
        }
	n++;

	pcontext->pusrctl = pusr;
	bmReleaseBuffer (pcontext, pusr->uc_bufstack[pusr->uc_bufsp - 1]);
	pcontext->pusrctl = psaveu;

	/* prevent infinite loops */

	if (n > 1000) break;
    }
    return (n);
#else
    return (0);
#endif /* BK_HAS_BSTACK */

}  /* end bmPopLocks */

#ifdef BM_DEBUG
LOCALF DSMVOID
bmDumpLRU(
        dsmContext_t *pcontext,
        bkchain_t    *pprivReadOnlyLRU,
        QBKCHAIN      qprivReadOnlyLRU)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    bktbl_t *pbktbl;
    COUNT i;

    if (qprivReadOnlyLRU)
        pprivReadOnlyLRU = XBKCHAIN(pdbcontext, qprivReadOnlyLRU);

    if (!pprivReadOnlyLRU)
        return;

    /* Traverse from head of the chain to the tail -
     * Least recently used to most recently used.
     */

    MSGD_CALLBACK(pcontext, (TEXT *)"%LLEAST RECENTLY USED...  ");

    pbktbl = XBKTBL(pdbcontext, pprivReadOnlyLRU->bk_qhead);
    for (i = pprivReadOnlyLRU->bk_nent; (i && pbktbl); i--)
    {
        MSGD_CALLBACK(pcontext, (TEXT *)"%L    Prev: %i", pbktbl->bt_qlruprv);
        MSGD_CALLBACK(pcontext, (TEXT *)"%L    Self: %i", pbktbl->qself);
        MSGD_CALLBACK(pcontext, (TEXT *)"%L    Next: %i", pbktbl->bt_qlrunxt);
        MSGD_CALLBACK(pcontext, (TEXT *)"%L   ");

        pbktbl = XBKTBL(pdbcontext, pbktbl->bt_qlrunxt);
    }
    MSGD_CALLBACK(pcontext, (TEXT *)"%L...MOST RECENTLY USED");

}
#endif

/* PROGRAM: bmAreaFreeBuffers - go through the buffer pool and mark all blocks
 *          associated with deleted areas as clean and invalidate the area
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bmAreaFreeBuffers(struct dsmContext *pcontext, ULONG area)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    bktbl_t     *pbktbl;

    for (pbktbl = XBKTBL(pdbcontext, pdbcontext->pbkctl->bk_qbktbl);
         pbktbl != (bktbl_t *)0;
         pbktbl = XBKTBL(pdbcontext, pbktbl->bt_qnextb))
    {
        if (pbktbl->bt_area == area )
        {
            BK_LOCK_BKTBL (pbktbl);
            /* need to check again to make sure it didn't change */
            if (pbktbl->bt_area == area )
            {
                pbktbl->bt_area = DSMAREA_INVALID;
                pbktbl->bt_busy = BKSTFREE;
                pbktbl->bt_flags.changed = 0;
            }

            BK_UNLK_BKTBL (pbktbl);
        }
/*      bkaddr(pcontext, pbktbl, 0); */
    }
}

