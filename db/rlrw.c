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
#include "bmpub.h"
#include "dbcontext.h"
#include "dbpub.h"  /* for DB_EXBAD */
#include "latpub.h"
#include "lkpub.h"
#include "mstrblk.h"
#include "rlprv.h"
#include "rlpub.h"
#include "rlmsg.h"
#include "rlai.h"
#include "shmpub.h"
#include "tmmgr.h"
#include "tmprv.h"
#include "tmtrntab.h"
#include "usrctl.h"
#include "utcpub.h"
#include "utspub.h"
#include "uttpub.h"
 
#if OPSYS==WIN32API
#include <process.h>
#endif


#if DOBCOPY
#define bufcop(t,s,n) bcopy((s), (t), (unsigned int)(n))
#define bufmov(ps,pe,amt) bcopy ((ps), (ps + amt), (unsigned int)(pe - ps))
#endif


/* rlrw.c - read and write entrypoints for the .bi file */

/* Function proto-types for functions private to this file. */

LOCALF RLBLK *prevbiblk	(dsmContext_t *pcontext, RLBLK *prlblk, 
                         LONG * prightBlock);

LOCALF LONG rdbibkwd	(dsmContext_t *pcontext, TEXT *pbuf, int dlen,
			 LONG   rightBlock, LONG   rightOffset,
                         LONG   *pblockNumber, LONG   *pblockOffset, 
                         RLCURS *prlcurs);

LOCALF DSMVOID rlbimv	(dsmContext_t *pcontext, TEXT *pdata, COUNT dlen);

LOCALF DSMVOID rlbinext	(dsmContext_t *pcontext);
LOCALF DSMVOID rlbisched	(dsmContext_t *pcontext);

LOCALF QRLBF rlgetfree	(dsmContext_t *pcontext);

LOCALF RLBF *rlgetmod	(dsmContext_t *pcontext, QRLBF q);

LOCALF DSMVOID rlputfree	(dsmContext_t *pcontext, QRLBF q);

LOCALF DSMVOID rlputmod	(dsmContext_t *pcontext, QRLBF q);

LOCALF DSMVOID rlsetout	(dsmContext_t *pcontext, QRLBF q);

LOCALF DSMVOID rlwrtcur	(dsmContext_t *pcontext, QRLBF q);

LOCALF DSMVOID rlbiwwt	(dsmContext_t *pcontext);

LOCALF DSMVOID rldumpchain (dsmContext_t *pcontext);


/* PROGRAM: rldumpchain  dump the modified buffer chain to the log file
 *
 * DEBUG: dump the modified buffer chain to the log file
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID 
rldumpchain (dsmContext_t *pcontext)
{
        dbcontext_t *pdbcontext = pcontext->pdbcontext;
FAST	RLCTL	*prl = pdbcontext->prlctl;
	QRLBF	q;
	RLBF	*p;

    q = prl->qmodhead;
    while (q)
    {
	p = XRLBF(pdbcontext, q);
        MSGN_CALLBACK(pcontext, rlMSG087,
	      p->dpend, p->sdpend, p->rlbktbl.bt_dbkey);
        q = XRLBF(pdbcontext, q)->qnext;
    }
    p = XRLBF(pdbcontext, prl->qoutbf);
    MSGN_CALLBACK(pcontext, rlMSG088,
	  (ULONG)p->dpend, (ULONG)p->sdpend, p->rlbktbl.bt_dbkey);
    MSGN_CALLBACK(pcontext, rlMSG089, (ULONG)prl->rlwritten,
                  (ULONG)prl->rldepend);
}

/* PROGRAM: rlGetBlockNum -- Returns the block number for the input bi block
 *
 * RETURNS:
 */
LONG
rlGetBlockNum (
	dsmContext_t	*pcontext _UNUSED_,
	BKBUF		*pblk)
{
#ifdef BITRACERW
    MSGD_CALLBACK(pcontext, (TEXT *)"%LrlGetBlockNum: rlblk=%l", 
                 ((RLBLKHDR *)pblk)->rlblknum);
#endif /* bitracerw */
    return (((RLBLKHDR *)pblk)->rlblknum);
}

/* PROGRAM: rlPutBlockNum -
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlPutBlockNum(
	dsmContext_t	*pcontext _UNUSED_,
	BKBUF		*pblk,
	LONG		 blknum)
{
#ifdef BITRACERW
    MSGD_CALLBACK(pcontext, (TEXT *)"%LrlPutBlockNum: rlblk=%l", 
                 blknum);
#endif /* bitracerw */
    ((RLBLKHDR *)pblk)->rlblknum = blknum;
} /* rlPutBlockNum */

/* PROGRAM: rlwakbiw -- wake up the bi writer so he can write some bi blocks
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlwakbiw (dsmContext_t *pcontext )
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    FAST RLCTL	*prl = pdbcontext->prlctl;
    usrctl_t	*pusr;

    if (prl->abiwpid == 0) return;
    if (prl->qbiwq == 0) return;
    
    pusr = XUSRCTL(pdbcontext, prl->qbiwq);

    if (!pusr->uc_qwtnxt)
      /* Biw is not waiting  */
      return;

    prl->qbiwq = pusr->uc_qwtnxt;
    pusr->uc_qwtnxt = 0;
    latUsrWake (pcontext, pusr, (int)(pusr->uc_wait));
}

/* PROGRAM: rlbiwait - wait until the rlbiflsh is done by another user
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlbiwait (
	dsmContext_t	*pcontext,
	DBKEY		 bidbkey,/* wait until block with this dbkey is flushed */
	int		 dontWait)  /* 1 if we just want to be queued */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    usrctl_t *pusr = pcontext->pusrctl;
    COUNT       depthSave;

    /* the block is in the proccess of being written */
    /* put myself in chain of waiting users */
    /* mark that I am waiting, so they will know to wake me up*/

    STATINC (biwtCt);
    pusr->uc_qwtnxt =  pdbcontext->prlctl->qwrhead;
    pdbcontext->prlctl->qwrhead = QSELF(pusr);
    pusr->uc_wait = BIWRITEWAIT;
    pusr->uc_wait1 = bidbkey;
    if (dontWait)
    {
        /* just mark that we are waiting */

        pusr->uc_2phase |= TPWAITNT;
        return;
    }
    MT_FREE_BIB(&depthSave);
    latUsrWait (pcontext);
    MT_RELOCK_BIB(depthSave);
}

/* PROGRAM: rlbiwwt - bi writer wait for something to do
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
rlbiwwt (dsmContext_t *pcontext)
{
    usrctl_t    *pusr = pcontext->pusrctl;
    FAST RLCTL	*prl = pcontext->pdbcontext->prlctl;
    COUNT        depthSave;

    if (prl->numWaiters) return;

    pusr->uc_qwtnxt =  prl->qbiwq;
    prl->qbiwq = QSELF(pusr);
    pusr->uc_wait = BIWWAIT;
    pusr->uc_wait1 = (LONG)prl->rlwritten;
    MT_FREE_BIB(&depthSave);
    latUsrWait (pcontext);
    MT_RELOCK_BIB(depthSave);
}

/* PROGRAM: rlwrite -- write a note to the .bi file	(rlprv.h)	*/
/* RETURNS: status - DSM_S_SUCCESS - successful                         */
/*                   DSM_S_FAILURE -  probably fatals                   */ 
/* Side Effect: lseek address of the last byte of the 2 byte length	*/
/* suffix for the note.  This addresss is placed in rlctl structure     */
/* members rllastBlock and rllastOffset. This is used by the caller to  */
/* keep track of the address of the last note written for each          */
/* transaction.	                                                        */
LONG
rlwrite(
	dsmContext_t	*pcontext,
	RLNOTE *prlnote,   /* pts note header                       */
	COUNT  dlen,       /* length of variable data, or 0 if none */
	TEXT   *pdata)     /* variable data, or PNULL if none       */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    dbshm_t         *pdbshm = pdbcontext->pdbpub;
    COUNT	    totlen;
    TEXT	    lenpfx[sizeof(COUNT)];
    RLCTL	    *prl = pdbcontext->prlctl;
    AICTL           *pai = pdbcontext->paictl;
    TEXT	    *pbuf;
    COUNT           adjustlength;

TRACE2_CB(pcontext, "rlwrite code=%t dlen=%d", prlnote->rlcode, dlen)

    /* busy value of 2 means that thew current buffer has been copied and
       the copy is being written out. So the current buffer can be modified
       but not written until the write completes */

    if (XRLBF(pdbcontext, prl->qoutbf)->rlbktbl.bt_busy != 2)
    {
	/* current buffer may be busy */

        while (XRLBF(pdbcontext, prl->qoutbf)->rlbktbl.bt_busy)
        {
            /* current output buffer is being written, wait for it */

            rlbiwait (pcontext, 
                      XRLBF(pdbcontext, prl->qoutbf)->rlbktbl.bt_dbkey,
                      (int)0);
	}
    }

    /* total length of the note: COUNT pfx, note, data, COUNT suffix */
    totlen = prlnote->rllen + dlen + 2*sizeof(COUNT);


    if (totlen > MAXRLNOTE)
    {
	pdbshm->shutdn = DB_EXBAD;
	FATAL_MSGN_CALLBACK(pcontext, rlFTL051, 
	      prlnote->rlcode & 0xff, (int)totlen);
    }

    STATADD (bibwCt, totlen);
    STATINC (binwCt);

    /* 95-04-20-012 - Enforce the rule that AIEXT notes must always */
    /* be the first note in an after image extent.  So we must save */
    /* room for the note.                                           */

    /* Check if ai extents are in use */
    if ((pdbcontext->pmstrblk->mb_aibusy_extent == 0) &&
        (prl->rlclused + totlen > prl->rlcap))
    {
        /* Ai extents are not in use - so               */
        /* the note wont fit in the current cluster     */
        /* close the current cluster and open a new one */
        /* parm 0 means close it even if it is open now */
        rlclcls(pcontext, 0);  
        rlclnxt(pcontext, 0);
    }
    else if ((pdbcontext->pmstrblk->mb_aibusy_extent) &&
             (prl->rlclused + totlen > 
             (prl->rlcap - (LONG)sizeof(AIEXTNOTE))))
    {
       /* If this is not an ai extent switch note close cluster */
       if (prlnote->rlcode != RL_AIEXT)
       {
           /* parm 0 means close it even if it is open now */
           rlclcls(pcontext, 0);  
           rlclnxt(pcontext, 0);
       }
       /* else this is a RL_AIEXT note, we will write it and the    */
       /* next call to rlwrite will close the cluster.              */
       /* We know we will not handle multiple RL_AIEXT notes        */
       /* properly, BUT the probability of encountering this is     */
       /* statistically insignificant.  95-04-20-012                */
       else if (prl->rlclused + totlen > prl->rlcap)
       {
           /* We hit the rare circumstance of multiple AIEXT notes */
           /* parm 0 means close it even if it is open now */
           rlclcls(pcontext, 0);  
           rlclnxt(pcontext, 0);
       }
    }

    adjustlength = 0;

    /* Is after imaging turned on ?                               */
    if( pai )
    {
	/* Are there after image extents ?                        */
	if( pdbcontext->pmstrblk->mb_aibusy_extent > (TEXT)0 )
        {
            /* Will this note fit into the current block or will  */
            /* it span blocks? #96-06-20-002                      */
            if ( totlen > ( pai->aiblksize - (LONG)sizeof(AIHDR)) )
            {
                adjustlength = ((totlen / (pai->aiblksize - sizeof(AIHDR)))
                                 * sizeof(AIHDR));
            }

            /* Will this note fit in the current ai extent ?      */
            if((pai->ailoc + pai->aiofst + totlen
                + adjustlength) >= pai->aisize )
            {
		/* No - so call rlaixtn - which extends variable  */
		/* length ai extents or switches to a new extent  */
		/* if this is a fixed length extent.              */
		MT_UNLK_BIB();
		MT_LOCK_AIB();
		rlaixtn(pcontext);
		MT_UNLK_AIB();
		MT_LOCK_BIB();
	    }
        }
    }

    /* keep track of how much data is in the cluster */
    prl->rlclused += totlen;

    /* build the note in the output buffer			*/
    /* 2 byte len prefix, rlnote, variable data, 2 byte trailer	*/
#ifdef BITRACE
    MSGD_CALLBACK(pcontext, 
                 (TEXT *)"%Lrlwrite: rlNumber=%l rlblOff=%l totlen %l",
                 prl->rlblNumber, prl->rlblOffset, totlen);
#endif

    if (prl->rlblOffset + totlen <= prl->rlbiblksize)
    {	
        /* it fits in the current block, use fast inline code */
	pbuf = (TEXT *)XRLBLK(pdbcontext, prl->qoutblk) + prl->rlblOffset;
	sct(pbuf, totlen);	/* insert length prefix */
	pbuf += sizeof(COUNT);
	bufcop(pbuf, (TEXT *)prlnote, (int)prlnote->rllen);
	pbuf += prlnote->rllen;
	if (dlen > 0) 
        {
            bufcop(pbuf, pdata, (int)dlen);
            pbuf += dlen;
        }
	sct(pbuf, totlen);	/* insert length suffix */
	prl->rlblOffset += totlen;

	if (XRLBF(pdbcontext, prl->qoutbf)->rlbktbl.bt_flags.changed == 0)
	{
	    XRLBF(pdbcontext, prl->qoutbf)->rlbktbl.bt_flags.changed = 1;
	    prl->numDirty++;
	}
        XRLBLK(pdbcontext, 
               prl->qoutblk)->rlblkhdr.rlused = prl->rlblOffset;
    }
    else
    {	
     	/* spans blocks, use slower code */
	sct(lenpfx, totlen);
	rlbimv(pcontext, lenpfx, sizeof(totlen));
	rlbimv(pcontext, (TEXT *)prlnote, (COUNT)prlnote->rllen);
	if (dlen > 0) rlbimv(pcontext, pdata, dlen);
	rlbimv(pcontext, lenpfx, sizeof(totlen));
    }

    /* Increment the dependency counter. Whenever an rl block is written 
       the current value of rlctl.rldepend is copied to rlctl.rlwritten 
       to indicate that database and ai blocks which depend on that 
       rl block may now be written safely.  */

    prl->rldepend++;
    XRLBF(pdbcontext, prl->qoutbf)->dpend = prl->rldepend;

    /* return address of last note written */
    prl->rlnxtBlock = prl->rlblNumber;
    prl->rlnxtOffset = prl->rlblOffset;
    
    /* set the last bi block and offset for the bi file */
    prl->rllastOffset = prl->rlnxtOffset - 1;
    prl->rllastBlock = prl->rlnxtBlock;

    /* set the last bi block and offset for the user */
    pcontext->pusrctl->uc_lastBlock = prl->rllastBlock;
    pcontext->pusrctl->uc_lastOffset = prl->rllastOffset;

    return (DSM_S_SUCCESS);
}

/* PROGRAM: rlgetmod - take a buffer off the modified queue.
 *	If NULL specified for buffer, the buffer at the head of the queue
 *	is returned. NULL is returned if queue is empty
 *
 * RETURNS:
 */
LOCALF RLBF *
rlgetmod (
	dsmContext_t	*pcontext,
	QRLBF		 q)
{
        dbcontext_t *pdbcontext = pcontext->pdbcontext;
FAST	RLCTL	*prl = pdbcontext->prlctl;
	RLBF	*prlbf;

    if (q == 0)
    {
	/* buffer not specified */

	q = prl->qmodhead;
        if (q == 0) return (NULL);
    }
    prlbf = XRLBF(pdbcontext, q);

    /* connect prev and next to each other */

    if (prlbf->qnext) XRLBF(pdbcontext, prlbf->qnext)->qprev = prlbf->qprev;

    if (prlbf->qprev) XRLBF(pdbcontext, prlbf->qprev)->qnext = prlbf->qnext;

    /* the one we took could be first or last */

    if (q == prl->qmodhead) prl->qmodhead = prlbf->qnext;

    if (q == prl->qmodtail) prl->qmodtail = prlbf->qprev;

    /* the one we took off is now loose */

    prlbf->bufstate = RLBSLOOSE;
    prlbf->qnext = 0;
    prlbf->qprev = 0;
    return (prlbf);
}

/* PROGRAM: rlputmod: add a buffer to the tail of the modified queue
 * 
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
rlputmod (
	dsmContext_t	*pcontext,
	QRLBF		 q)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm = pdbcontext->pdbpub;
FAST	RLCTL	*prl = pdbcontext->prlctl;
	RLBF	*prlbf;

    prlbf = XRLBF(pdbcontext, q);
    if (prlbf->rlbktbl.bt_busy)
    {
        /* current buffer may be busy */

        while (prlbf->rlbktbl.bt_busy)
        {
            /* current output buffer is being written, wait for it */

            rlbiwait (pcontext, prlbf->rlbktbl.bt_dbkey, (int)0);
        }
    }

    if (prl->qmodtail)
    {
	/* sanity check */

	if ((XRLBF(pdbcontext, prl->qmodtail)->dpend >= prlbf->sdpend) ||
	    (XRLBF(pdbcontext, prl->qmodtail)->dpend >= prlbf->dpend))
	{
	    /* dependency counter should be higher for buffer we are adding */

	    pdbshm->shutdn = DB_EXBAD;
	    rldumpchain (pcontext);
	    FATAL_MSGN_CALLBACK(pcontext, rlFTL052, prlbf->dpend);
	}
    }
    if (prl->rlwritten >= prlbf->dpend)
    {
        if( prl->rlwritten == prlbf->dpend &&
            prlbf->rlbktbl.bt_flags.changed == 0 )
        {
            /* Someone else flushed the current output buffer
               like a buffer evicter or an apw while we were asleep
               waiting on a busy bi buffer in rlgetfree.
               So put the current output buffer directly on the empty
               que.  */
            rlputfree(pcontext, q );
            return;
        }
        pdbshm->shutdn = DB_EXBAD;
        rldumpchain (pcontext);
	FATAL_MSGN_CALLBACK(pcontext, rlFTL053, prlbf->dpend);
    }
    /* make new one last */

    prlbf->qnext = 0;
    prlbf->qprev = prl->qmodtail;
    prl->qmodtail = q;

    /* connect old last to new one */

    if (prlbf->qprev) XRLBF(pdbcontext, prlbf->qprev)->qnext = q;

    /* could be first too */

    if (prl->qmodhead == 0) prl->qmodhead = q;

    /* mark it dirty */

    prlbf->bufstate = RLBSDIRTY;

    if (prl->rlwarmstrt) return;

    /* wake up the bi writer if there is one */

    if (prl->abiwpid) rlwakbiw (pcontext);
}

/* PROGRAM: rlgetfree: get an unused buffer from the free queue
 *	If no free buffer available, one is written from dirty queue.
 *
 * RETURNS:
 */
LOCALF QRLBF
rlgetfree (dsmContext_t *pcontext)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm = pdbcontext->pdbpub;
FAST	RLCTL	*prl = pdbcontext->prlctl;
	QRLBF	q;
	RLBF	*p;

	COUNT	n = 1000; /* FIXFIX debug */

retry:
    q = prl->qfree;
    if (q == 0)
    {
	if (prl->qmodhead == 0)
	{
            pdbshm->shutdn = DB_EXBAD;
	    FATAL_MSGN_CALLBACK(pcontext, rlFTL054);
	}
        /* no buffers available, flush one
	   note: we have assumed that there must be buffers on the
	   modified queue if there are no free ones. This means that
	   there must be at least two buffers, since the current one
	   got put on the modified list above and there must be an
	   input and output buffer available */

        STATINC (binaCt);
        rlbiflsh (pcontext, XRLBF(pdbcontext, prl->qmodhead)->dpend);
        n--;
        if (n == 0) /* FIXFIX - debug */
        {
            pdbshm->shutdn = DB_EXBAD;
            rldumpchain (pcontext);
            FATAL_MSGN_CALLBACK(pcontext, rlFTL055);
        }
        goto retry;
    }
    prl->qfree = XRLBF(pdbcontext, q)->qnext;
    p = XRLBF(pdbcontext, q);
    p->bufstate = RLBSLOOSE;
    p->dpend = 0;
    p->sdpend = 0;
    return (q);
}

/* PROGRAM: rlputfree: add a buffer to the free queue
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
rlputfree (
	dsmContext_t	*pcontext,
	QRLBF		 q)
{
        dbcontext_t *pdbcontext = pcontext->pdbcontext;
FAST	RLCTL	*prl = pdbcontext->prlctl;
	RLBF	*prlbf;

    /* link to front of free chain */

    prlbf = XRLBF(pdbcontext, q);
    prlbf->qnext = prl->qfree;
    prl->qfree = q;
    prlbf->qprev = 0;
    prlbf->bufstate = RLBSFREE;
    prlbf->dpend = 0;
    prlbf->sdpend = 0;
    prlbf->rlbktbl.bt_dbkey = -1;
}

/* PROGRAM: rlwrtcur - copy and write current bi buffer
 * copy the contents of the current bi buffer to a scratch area and write
 * the copy. This allows notes to be added to the current buffer and it
 * may be queued while the copy is being written. But the modified output
 * buffer or any subsequent buffers may not be written until after the
 * copy has been written
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
rlwrtcur (
	dsmContext_t	*pcontext,
	QRLBF		 q)
{
        dbcontext_t *pdbcontext = pcontext->pdbcontext;
FAST	RLCTL	*prl = pdbcontext->prlctl;
	RLBF	*prlbf;
	BKTBL	*pbktbl;
	RLBF	*pcurrlbf;
	BKTBL	*pcurbktbl;
	RLBLK	*pcurbuf;
	RLBLK	*pscrbuf;
	LONG64	depend;
	QRLBF	qf;
	int	scratch;
        COUNT   depthSave;

    prlbf = XRLBF(pdbcontext, q);
    pbktbl = &(prlbf->rlbktbl);
    pcurrlbf = prlbf;
    pcurbktbl = pbktbl;

    depend = pcurrlbf->dpend;

    /* mark the buffer as in transit */

    pcurbktbl->bt_busy = 2;

    pcurbuf = XRLBLK(pdbcontext, prlbf->rlbktbl.bt_qbuf);

    /* count partially filled buffers */

    if (pcurbuf->rlblkhdr.rlused < prl->rlbiblksize) STATINC (bipwCt);

    qf = prl->qfree;
    if ((MTHOLDING (MTL_MTX) == 0) && (qf != 0))
    {
	/* take a free buffer off the free list to use to write from.
	   No point in wasting time if we are holding the MT lock though */

        prl->qfree = XRLBF(pdbcontext, qf)->qnext;
        prlbf = XRLBF(pdbcontext, qf);

        pbktbl = &(prlbf->rlbktbl);
        pscrbuf = XRLBLK(pdbcontext, prlbf->rlbktbl.bt_qbuf);

        /* copy the data and buffer info */

        prlbf->dpend = pcurrlbf->dpend;
        prlbf->sdpend = pcurrlbf->sdpend;
	pbktbl->bt_dbkey = pcurbktbl->bt_dbkey;
	pbktbl->bt_raddr = pcurbktbl->bt_raddr;
	pbktbl->bt_qftbl = pcurbktbl->bt_qftbl;
	pbktbl->bt_offst = pcurbktbl->bt_offst;

        bufcop ((TEXT *)(pscrbuf), (TEXT *)pcurbuf, prl->rlbiblksize);
	scratch = 1;
    }
    else
    {
	/* no empty buffer is available, so we have to lock the current one
	   and write it out - lock the buffer first */

        pbktbl->bt_busy = 1;
	scratch = 0;
    }
    /* write out the block */

    MT_FREE_BIB(&depthSave);
    bkWrite(pcontext, &(prlbf->rlbktbl), UNBUFFERED);
    MT_RELOCK_BIB(depthSave);

    /* update .db block dependency */

    prl->rlwritten = depend;
    if (pcurrlbf->dpend == depend)
    {
	/* the current buffer did not get modified while we were writing
	   the copy */

        pcurbktbl->bt_flags.changed = 0;

        /* less dirty buffers */

        prl->numDirty--;
    }
    if (scratch)
    {
	/* put scratch buffer back on free list */

        pbktbl->bt_busy = 0;
        pbktbl->bt_flags.changed = 0;
	rlputfree (pcontext, qf);
    }
    /* release the current bi buffer */

    pcurbktbl->bt_busy = 0;
    /* wake up all waiting on BI writing, if any */

    lkwakeup (pcontext, &prl->qwrhead, pcurbktbl->bt_dbkey, BIWRITEWAIT);
}

/* PROGRAM: rlbiflsh - flush out the dirty bi buffers */ 
DSMVOID
rlbiflsh(
	dsmContext_t	*pcontext,
	LONG64 depend)   /* flush if this depend ctr not yet flshed*/
		     /* RL_FLUSH_ALL_BI_BUFFERS means flush unconditionally */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm = pdbcontext->pdbpub;
FAST	RLCTL	*prl = pdbcontext->prlctl;
	QRLBF	q;
	RLBF	*prlbf;
	BKTBL	*pbktbl;
	DBKEY	 wakeKey;
        COUNT    depthSave;

    TRACE1_CB(pcontext, "rlbiflsh depend=%l", depend)
    /* bug 90-12-13-03: Used to be if (ixrpr) return; */
    if (prl==NULL) return;  /* .bi file is suppressed during index repair util*/

    MT_LOCK_BIB ();

    if (prl->rlwritten >= depend)
    {
        /* this depend was already wrtn*/

        MT_UNLK_BIB ();
        return;
    }
    /* pull blocks off the dirty list till we get as far as the dependency
       counter requires or until we have the number of buffers requested */

    for (;;)
    {
        q = prl->qmodhead;
	if (q == 0)
	{
	    /* done with all buffers which are queued. Now do current
	       output buffer */

	    q = prl->qoutbf;
            prlbf = XRLBF(pdbcontext, q);
	    if (prlbf->sdpend > depend) break;
	    if (!(prlbf->rlbktbl.bt_flags.changed)) break;
	    if (prlbf->rlbktbl.bt_busy)
	    {
	        rlbiwait(pcontext, prlbf->rlbktbl.bt_dbkey, (int)0);
	        continue;
	    }
	    rlwrtcur (pcontext, q);
	    break;
	}
        prlbf = XRLBF(pdbcontext, q);
	if (prlbf->sdpend > depend) break;
	pbktbl = &(prlbf->rlbktbl);

        if (prl->rlwritten >= prlbf->dpend)
        {
            pdbshm->shutdn = DB_EXBAD;
            rldumpchain (pcontext);
            FATAL_MSGN_CALLBACK(pcontext, rlFTL056, prlbf->dpend);
        }
	if (pbktbl->bt_busy)
	{
	    rlbiwait(pcontext, pbktbl->bt_dbkey, (int)0);
	    continue;
	}
        /* prevent another user from simultaneously flushing
	   the same blk*/

        pbktbl->bt_busy = 1;
        wakeKey = pbktbl->bt_dbkey;

        /* write out the block */

	MT_FREE_BIB(&depthSave);

        bkWrite(pcontext, pbktbl, UNBUFFERED);
	MT_RELOCK_BIB(depthSave);

        pbktbl->bt_flags.changed = 0;
	prl->numDirty--;

	/* update .db block dependency */

        prl->rlwritten = prlbf->dpend;

	/* count partial writes of bi blocks */
	if (XRLBLK(pdbcontext, 
            prlbf->rlbktbl.bt_qbuf)->rlblkhdr.rlused < prl->rlbiblksize)
	     STATINC (bipwCt);

	/* move to free pool */

        rlgetmod (pcontext, q);
	rlputfree (pcontext, q);

        /* wake up all waiting on BI writing, if any */

        pbktbl->bt_busy = 0;
        lkwakeup (pcontext, &prl->qwrhead, wakeKey, BIWRITEWAIT);
    }
    /* tell the tm manager if all blocks written so the -Mf timer will be
       reset */

    if (prl->numDirty == 0) tmdidflsh (pcontext, BKBI);

    if ((depend < RL_FLUSH_ALL_BI_BUFFERS) && (prl->rlwritten < depend))
    {
	/* something is wrong. The last flush did not work */

	pdbshm->shutdn = DB_EXBAD;
	FATAL_MSGN_CALLBACK(pcontext, rlFTL057,
                            (LONG)depend, (LONG)prl->rlwritten);
    }
    MT_UNLK_BIB ();
}


/* PROGRAM: rlgetwritten -  get dpend counter of last bi buffer written
 *
 * RETURNS:
 */
LONG64
rlgetwritten (dsmContext_t *pcontext)
{
    LONG64	ret;

    MT_LOCK_BIB ();
    ret = pcontext->pdbcontext->prlctl->rlwritten;
    MT_UNLK_BIB ();
    return (ret);
}

/* PROGRAM: rlGetClFill - get current cluster fill percentage
 *
 * RETURNS:
 */
int
rlGetClFill (dsmContext_t *pcontext)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
        LONG    fillPercentage;
        LONG    bytesUsed;
        LONG    bytesThatFit;

    if (!pdbcontext->prlctl) return (0);

    MT_LOCK_BIB ();

    bytesUsed = pdbcontext->prlctl->rlclused;

    MT_UNLK_BIB ();
 
    bytesThatFit = pdbcontext->prlctl->rlcap;
    if (bytesThatFit >= (LONG)1048576)
    {
        /* Big (more than a megabyte) numbers. Use computation that works
           for big numbers. If we don't, then we get overflows, and that
           causes the page writers (which use this function to figure out
           how many database buffers to write to disk) to become insane
           and very lazy. */

        fillPercentage = bytesUsed / (bytesThatFit / (LONG)100);
    }
    else
    {
        /* Small numbers. Use computation that works for small numbers.
           Note that integer division causes the expression

                bytesUsed/bytesThatFit to be zero always
        */

        fillPercentage = (bytesUsed * (LONG)100) / bytesThatFit;
    }
    if (fillPercentage < 1) fillPercentage = 0;
    else if (fillPercentage > 100) fillPercentage = 100;

    return ((int)fillPercentage);
}

/* PROGRAM: rlwtdpend - self-server wait until bi note with this dependency
 *             has been written. Server queue the client and go on.
 *	       ONLY IF -Mf 0 has been specified
 *
 * RETURNS:
 */
int
rlwtdpend(
	dsmContext_t	*pcontext,
	LONG64		 dpend,
	ULONG		 aiNoteCounter)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm = pdbcontext->pdbpub;
    RLCTL	*prl = pdbcontext->prlctl;
    COUNT        groupDelay = 0;
    
    if (prl->rlwarmstrt) return 0;

    if( pdbcontext->arg1usr || pdbshm->shutdn )
        groupDelay = 0;
    else
        groupDelay = pdbshm->groupDelay;
    
    if(pcontext->pusrctl->uc_latches)
        FATAL_MSGN_CALLBACK(pcontext, rlFTL076);

    if( groupDelay )
        utnap(groupDelay);
#if 0
    MT_LOCK_MTX ();
#endif
    MT_LOCK_BIB ();
    if (dpend > prl->rlwritten)
    {
	/* The specified bi note has now been written to disk */
        rlbiflsh(pcontext, dpend);
    }
    MT_UNLK_BIB();
    
    if( rlaiqon(pcontext) == 1 )
    {
        MT_LOCK_AIB();
        if( aiNoteCounter > pdbcontext->paictl->aiwritten )
        {
            rlaiflsx(pcontext);
        }
        MT_UNLK_AIB();
    }
#if 0    
    MT_UNLK_MTX();
#endif
    
    return 0;

}

/* PROGRAM: rlbiclean - flush dirty bi buffers except for current
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlbiclean (dsmContext_t *pcontext)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm = pdbcontext->pdbpub;
    RLCTL	*prl = pdbcontext->prlctl;
    LONG64	 dpend;

    MT_LOCK_BIB ();

    /* do the work */

    for (;;)
    {
	if (prl->qmodhead == 0)
        {
            if (pdbshm->bwdelay == 0 )
                rlbiwwt(pcontext);
            else        
                break;  
        }
        
        for ( ; prl->qmodhead; )
        {
            dpend = XRLBF(pdbcontext, prl->qmodhead)->dpend;

            /* flush this buffer */

            rlbiflsh (pcontext, dpend);
            STATINC (bibcCt);
        }
	if (prl->qwrhead)
	{
	    rlbiflsh (pcontext, RL_FLUSH_ALL_BI_BUFFERS);
            STATINC (bibcCt);
	}
	/* none left */

        if (pdbshm->shutdn || pdbcontext->pusrctl->usrtodie ) break;
    }

    MT_UNLK_BIB ();
}

/* PROGRAM: rlsetout - make the current output buffer be the one specified
 *
 * RETURNS:
 */
LOCALF DSMVOID
rlsetout (
	dsmContext_t	*pcontext,
	QRLBF		 q)
{
        dbcontext_t *pdbcontext = pcontext->pdbcontext;
FAST	RLCTL	*prl = pdbcontext->prlctl;
	RLBF	*p;

    p = XRLBF(pdbcontext, q);
    prl->qoutbf = q;
    prl->qoutblk = (QRLBLK)p->rlbktbl.bt_qbuf;

    p->bufstate = RLBSOUT;
    p->dpend = prl->rldepend;
    p->sdpend = prl->rldepend;
    p->rlbktbl.bt_flags.changed = 0;
}

/* PROGRAM: rlbisched - schedule the current output buffer for writing
 *                      and get a new one
 *
 * RETURNS:
 */
LOCALF DSMVOID
rlbisched(dsmContext_t *pcontext)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    RLCTL	*prl = pdbcontext->prlctl;
    QRLBF	q;

    /* Output buffer might be full (or almost full) and have been forced
       out already. In this case, it may not be dirty and we don't want
       to schedule it for writing */

    if (XRLBF(pdbcontext, prl->qoutbf)->rlbktbl.bt_flags.changed)
    {
        /* Current output buffer is dirty and full. */
	/* Get a free buffer */
	q = rlgetfree (pcontext);

        /* queue current buffer for write */
        rlputmod (pcontext, prl->qoutbf);

        /* make new one the output buffer */
        rlsetout (pcontext, q);
   }
}

/* PROGRAM: rlbinext - schedule current buffer and get the next one
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
rlbinext (dsmContext_t *pcontext)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm = pdbcontext->pdbpub;
FAST	RLCTL	*prl = pdbcontext->prlctl;
	RLBLK	*prlblk;
	RLBF	*poutbf;
        LONG    clusterEndBlock; /* calculated end block of cluster */

    prlblk = XRLBLK(pdbcontext, prl->qoutblk);
    prlblk->rlblkhdr.rlused = prl->rlblOffset;
    prl->rldepend++;
    XRLBF(pdbcontext, prl->qoutbf)->dpend = prl->rldepend;
    prl->rldepend++;

    /* put the current buffer in the queue and get a free one */

    rlbisched (pcontext);

    /* compare current block position with the cluster end block */
    clusterEndBlock = prl->rlcurr + (prl->rlclbytes >> prl->rlbiblklog);
    if ((prl->rlblNumber + 1) >= clusterEndBlock)
    {
        /* oops. off the end */
 
        pdbshm->shutdn = DB_EXBAD;
	FATAL_MSGN_CALLBACK(pcontext, rlFTL060, prl->rlblNumber + 1, clusterEndBlock);
    }
   /* prepare the new one for use */
    prl->rlblNumber += 1;
 
    poutbf = XRLBF(pdbcontext, prl->qoutbf);
    poutbf->rlbktbl.bt_dbkey = prl->rlblNumber;

    bkaddr (pcontext, &(poutbf->rlbktbl), 1);

    prlblk = XRLBLK(pdbcontext, prl->qoutblk);
    prl->rlblOffset = 
    prlblk->rlblkhdr.rlused   =
    prlblk->rlblkhdr.rlhdrlen = sizeof(RLBLKHDR);
    prlblk->rlblkhdr.rlcounter = prl->rlcounter;

    /* block number to rlblkhdr for sanity checks */
    prlblk->rlblkhdr.rlblknum = prl->rlblNumber;

#ifdef BITRACE
    MSGD_CALLBACK(pcontext, (TEXT *)"%Lrlbinext: rlblk=%l", 
                  prlblk->rlblkhdr.rlblknum);
#endif
}

/* PROGRAM: rlbimv - move a string of data into 1 or more	*
 *	             .bi blocks all within one cluster		*
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
rlbimv(
	dsmContext_t	*pcontext,
	TEXT  *pdata, /* data to put in the .bi block */
	COUNT dlen)   /* length of the data		*/
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm = pdbcontext->pdbpub;
    int 	 movelen; /* amount to put in current block		*/
    RLCTL	*prl = pdbcontext->prlctl;
    RLBLK	*prlblk;

TRACE1_CB(pcontext, "rlbimv, dlen=%d", dlen)
    while (XRLBF(pdbcontext, prl->qoutbf)->rlbktbl.bt_busy)
    {
        /* current output buffer is being written, wait for it */

        rlbiwait (pcontext, 
                  XRLBF(pdbcontext, prl->qoutbf)->rlbktbl.bt_dbkey, (int)0);
    }
    /* 1 iteration of this loop for each block used to hold the data */
    while (dlen > 0)
    {
	if (prl->rlblOffset >= prl->rlbiblksize)
	{
            /* current block is full,  get a new one */

#ifdef BITRACERW
            MSGD_CALLBACK(pcontext, 
                          (TEXT *)"%Lrlbimv1: rlblNum= %l rlblOff=%l",
                          prl->rlblNumber, prl->rlblOffset);
#endif

            rlbinext (pcontext);
	}
	/* move as much as will fit in this block */

	prlblk = XRLBLK(pdbcontext, prl->qoutblk);
#ifdef BITRACERW
        MSGD_CALLBACK(pcontext, 
                      (TEXT *)"%Lrlbimv2: rlblNum=%l rlblOff=%l",
                      prl->rlblNumber, prl->rlblOffset);
#endif
	movelen = prl->rlbiblksize - prl->rlblOffset;
	if (movelen > dlen) movelen = dlen;
	if (movelen < 1)
	{
	    pdbshm->shutdn = DB_EXBAD;
	    FATAL_MSGN_CALLBACK(pcontext, rlFTL061, movelen, 
                               (int)prl->rlblOffset);
	}
	bufcop((TEXT *)(prlblk) + prl->rlblOffset, pdata, movelen);
	pdata += movelen;
	dlen -= movelen;
        prl->rlblOffset += movelen;

        prlblk->rlblkhdr.rlused = prl->rlblOffset;

	if (XRLBF(pdbcontext, prl->qoutbf)->rlbktbl.bt_flags.changed == 0)
	{
	    /* mark the buffer as dirty and count it */

	    XRLBF(pdbcontext, prl->qoutbf)->rlbktbl.bt_flags.changed = 1;
	    prl->numDirty++;
	}
    }
}

/* PROGRAM: rlrdprv - read the prev note in the .bi file    (rlprv.h)	*/
/* RETURNS:  a pointer to the previous note, warning, the		*/
/*		       note may be in the IN, OUT, or ASSY buffer and	*/
/*		       may not necessarily be properly aligned		*/

/*  In addition to returning a pointer, rlcurs will contain:	*/
/*	buftype - which buffer the pointer is in.		*/
/*	notelen - the length of the note (not including the 2	*/
/*	     byte length prefix and suffix).			*/

/*  In certain cases, if the returned note is not in the ASSY buffer,	*/
/*  then the caller should copy the note to the ASSY buffer.  These	*/
/*  cases include, but are not limited to:				*/
/*	- the caller will do any logical operation (cxixgen, rmdel, etc)*/
/*	  which might clobber the current IN or OUT buffer.		*/
/*	- the caller will do any operation which requires the note to be*/
/*	  in correctly aligned storage.					*/

RLNOTE *
rlrdprv(
	dsmContext_t	*pcontext,
	RLCURS		*prlcurs)   /* pointer to an active rl cursor */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    dbshm_t         *pdbshm = pdbcontext->pdbpub;
    TEXT	    lenbuf[sizeof(COUNT)];
    COUNT	    totlen;
    LONG            blockNumber;
    LONG            blockOffset;       
    LONG            retCode;

TRACE2_CB(pcontext, "rlrdprv nxtblk=%l nxtoff=%l", prlcurs->nextBlock,
                     prlcurs->nextOffset)
#ifdef BITRACERW
    MSGD_CALLBACK(pcontext, 
                  (TEXT *)"%Lrlrdprv1: nxtblock=%l nextoff=%l",
                   prlcurs->nextBlock, prlcurs->nextOffset);
#endif

    /* Get the trailing length field  and Update values in prlcurs */
    retCode = rdbibkwd(pcontext, lenbuf, sizeof(COUNT), prlcurs->nextBlock, 
                       prlcurs->nextOffset, &blockNumber, &blockOffset, 
                       prlcurs);
    if (retCode)
        FATAL_MSGN_CALLBACK(pcontext, rlFTL028);

    totlen = xct(lenbuf) - sizeof(COUNT); /* msg len less length suffix */
    if (totlen > MAXRLNOTE)
    {
	pdbshm->shutdn = DB_EXBAD;
	FATAL_MSGN_CALLBACK(pcontext, rlFTL062, (int)totlen);
    }
    prlcurs->notelen = totlen - sizeof(COUNT); /* deduct len pfix */
TRACE2_CB(pcontext, "	notelen=%d noteOffset=%l",
         prlcurs->notelen,prlcurs->noteOffset) 

    /* the note and its prev note are not all in the same block, must */
    /* use subroutine calls to do the work			      */
    retCode = rdbibkwd(pcontext, prlcurs->passybuf+sizeof(COUNT),
                       (int)totlen, blockNumber, blockOffset,
                       &blockNumber, &blockOffset, (RLCURS *)NULL);
    if (retCode)
        FATAL_MSGN_CALLBACK(pcontext, rlFTL029);

    prlcurs->nextOffset = blockOffset;
    prlcurs->nextBlock = blockNumber;
#ifdef BITRACERW
    MSGD_CALLBACK(pcontext, 
                 (TEXT *)"%Lrlrdprv2: notelen=%d block=%l offset=%l",
                 prlcurs->notelen,
                 prlcurs->nextBlock,
                 prlcurs->nextOffset);
#endif
    prlcurs->buftype = RLASSYBUF;
    STATINC (binrCt);
    return (RLNOTE *)(prlcurs->passybuf + 2*sizeof(COUNT)); /* skip pfx*/
}

/* PROGRAM: rdbibkwd - read data backwards out of the .bi file
 *
 * Side Effects: pblockNumber - set to preceding bi block number
 *               pblockOffset - set to offset within above block number
 * RETURNS:  0 - successful operation
 *           1 - failure - read yielded beginning of bi file (not expected) 
 */
LOCALF LONG
rdbibkwd(
	dsmContext_t	*pcontext,
	TEXT   *pbuf,         /* put the data into this buffer		*/
	int    dlen,          /* amount of data to be read		*/
	LONG   rightBlock,    /* rightmost block of data to read        */
	LONG   rightOffset,   /* rightmost byte of data to read         */
        LONG   *pblockNumber, /* OUT - block number of preceding block  */
        LONG   *pblockOffset, /* OUT - offset within preceding block    */
	RLCURS *prlcurs)      /* if non-NULL, put note address in it	*/
{
    RLCTL   *prl = pcontext->pdbcontext->prlctl;
    RLBLK	*prlblk;
    COUNT	 movelen;

TRACE3_CB(pcontext, "rdbibkwd, dlen=%i blk=%l offset=%l", dlen, 
                     rightBlock, rightOffset)

    pbuf += dlen;	/* skip to right end of the input buffer */
    while(dlen > 0)
    {	
        /* get a pointer to the .bi block containing the data */
	prlblk = rlbird(pcontext, rightBlock);

	/* is there any more data in this block ?*/
	while( rightOffset + 1 <= prlblk->rlblkhdr.rlhdrlen )
	{
	    /* no, must read the previous block */
	    /* get address of first byte of prev block */

	    prlblk = prevbiblk(pcontext, prlblk, &rightBlock);
	    if (prlblk == NULL) 
            {
                pblockNumber = NULL;
                pblockOffset = NULL;
                return 1;
            }

	    /* find the last used byte in this block.  When reading the  */
	    /* last byte of a note (prlcurs!=NULL), rlused tells where */
	    /* that byte is in the block.				 */

	    if (prlcurs != NULL) 
            {
                rightOffset = prlblk->rlblkhdr.rlused - 1;
            }
	    else 
            {
                rightOffset = prl->rlbiblksize - 1;
            }
	}

	/* if this is the rightmost byte of data of this note	*/
	/* then put its address in the callers cursor		*/
	if (prlcurs != NULL)
	{   
            prlcurs->noteOffset = rightOffset;
            prlcurs->noteBlock = rightBlock;
	    prlcurs->rlcounter = prlblk->rlblkhdr.rlcounter;
	    prlcurs = NULL;	/* dont change noteBlock and offset again */
	}

	/* copy as much data as the block contains */
	movelen = rightOffset + 1 - prlblk->rlblkhdr.rlhdrlen;
	if (movelen > dlen) movelen = dlen;
        rightOffset -= movelen;
	pbuf -= movelen;
	dlen -= movelen;
	bufcop(pbuf, (TEXT *)prlblk + rightOffset + 1, movelen);
	STATADD (bibrCt, (ULONG)(movelen));
    }

    /* Return block and offset of the note preceding the current */
    *pblockNumber = rightBlock;
    *pblockOffset = rightOffset;

    return(0);
}

/* PROGRAM: prevbiblk - given a pointer to a bi block and its lseek address,
 *	                find the previous block in the bi file and return
 *	                a pointer to it and its lseek address
 *
 * Side Effects: prightBlock has it value changed by this routine.
 * 
 * RETURNS NULL if beginning of the .bi file was reached
 *         (RLBLK *) pblock - pointer to current block 
 */
LOCALF RLBLK *
prevbiblk(
	dsmContext_t	*pcontext,
	RLBLK		*prlblk, /* points to current block	*/
        LONG            *prightBlock) /* block number of current block */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    LONG            currentBlock;
    LONG	    savectr;
    LONG	    newctr;
    RLCTL	    *prl = pdbcontext->prlctl;

TRACE1_CB(pcontext, "prevbiblk *pblk=%l", *prightBlock)

    currentBlock = *prightBlock;	

    /* Check to see if reading backwards causes a read of the 
       previous cluster                                       */
    if ( (currentBlock % (prl->rlclbytes >> prl->rlbiblklog)) != 0 ) 
    {
        /* the current block is NOT the first block in a cluster */
        /* simply return the previous block in the cluster	 */

        currentBlock -= (LONG)1;
        *prightBlock = currentBlock;
	return rlbird(pcontext, currentBlock);
    }

    /* must read the previous cluster and find its last valid block */

    /* Check if the beginning of bi file has been reached */
    if (currentBlock == pdbcontext->prlctl->rltype1)
        return (RLBLK *)NULL;  

    /* Set the start of previous cluster */
    currentBlock = prlblk->rlclhdr.rlprevBlock;
    savectr = prlblk->rlblkhdr.rlcounter;

    /* read the prev cluster header and make sure it is OK */

    prlblk = rlbird(pcontext, currentBlock);
    newctr = prlblk->rlblkhdr.rlcounter;
    if (newctr == 0 || newctr >= savectr)
        FATAL_MSGN_CALLBACK(pcontext, rlFTL030);

    /* scan the cluster backwards to find the last valid block */
    currentBlock += (prl->rlclbytes >> prl->rlbiblklog);
    do {
        currentBlock -= (LONG)1;
	prlblk = rlbird(pcontext, currentBlock);
        } 
    while(prlblk->rlblkhdr.rlcounter != newctr);

    *prightBlock = currentBlock;
    return prlblk;
}

/* PROGRAM: rlbird -- return a pointer to a .bi block
 *
 * RETURNS:
 *	returns current output or input buffer or a buffer
 *	in the modified list
 */
RLBLK *
rlbird(
	dsmContext_t	*pcontext,
        LONG             rlBlock) /* bi block number to read */
{
        dbcontext_t *pdbcontext = pcontext->pdbcontext;
	RLCTL  *prl = pdbcontext->prlctl;
	BKTBL  *pbktbl;
	RLBF   *prlbf;
	DBKEY	blkdbkey;
        COUNT   depthSave;

TRACE1_CB(pcontext, "rlbird rlBlock=%l", rlBlock)

    blkdbkey = rlBlock;

#ifdef BITRACERW
    /* This is temporary - will be removed at end of 2gig proj */
    MSGD_CALLBACK(pcontext, 
                 (TEXT *)"%Lrlbird: blknum=%l", rlBlock);
#endif /* bitracerw */

    if (blkdbkey < 0)
        FATAL_MSGN_CALLBACK(pcontext, rlFTL031, rlBlock);

    if (blkdbkey == XRLBF(pdbcontext, prl->qinbf)->rlbktbl.bt_dbkey)
    {
	/* block is in current input buffer */

	return XRLBLK(pdbcontext, prl->qinblk);
    }
    if (blkdbkey == XRLBF(pdbcontext, prl->qoutbf)->rlbktbl.bt_dbkey)
    {
	/* block is in current output buffer */

	return XRLBLK(pdbcontext, prl->qoutblk);
    }
    /* look through modified list */

retry:
    prlbf = NULL;
    if (prl->qmodhead)
    {
        prlbf = XRLBF(pdbcontext, prl->qmodhead);
        for (;;)
        {
            pbktbl = &(prlbf->rlbktbl);
            if (blkdbkey == pbktbl->bt_dbkey) 
            {
	        /* found it */

                if (pbktbl->bt_busy)
	        {
		    /* someone is doing i/o on this block */

		    rlbiwait (pcontext, pbktbl->bt_dbkey, (int)0);
		    goto retry;
	        }
                return XRLBLK(pdbcontext, prlbf->rlbktbl.bt_qbuf);
	    }
            if (prlbf->qnext == 0) break;
	    prlbf = XRLBF(pdbcontext, prlbf->qnext);
	}
    }
    /* block not in memory. read it into current input buffer. This can
       be done because the MT lock is being held so no one else can be
       reading at the same time. */

    pbktbl = &(XRLBF(pdbcontext, prl->qinbf)->rlbktbl);
    pbktbl->bt_dbkey = blkdbkey;

    MT_FREE_BIB(&depthSave);
    bkRead(pcontext, pbktbl);
    MT_RELOCK_BIB(depthSave);

    return XRLBLK(pdbcontext, pbktbl->bt_qbuf);
}

/* PROGRAM: rlbiwrt - write a .bi block from current buffer
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlbiwrt(
	dsmContext_t	*pcontext,
	RLBLK		*prlblk)  /* points to the block to write */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm = pdbcontext->pdbpub;
    RLCTL   *prl = pdbcontext->prlctl;     /* bi buffer control */
    RLBF    *prlbf = XRLBF(pdbcontext, prl->qpool); /* bi buffer list head */
    BKTBL   *pbktbl;                    /* buffer to write */
    COUNT    depthSave;

    /* search for the buffer or die trying */
    for (;;)
    {
	/* if the buffer pointer matches the block we have it */
	if (XRLBLK (pdbcontext, prlbf->rlbktbl.bt_qbuf) == prlblk) break;

	/* no buffer, is there another buffer in the list */
	if (XRLBF(pdbcontext, prlbf->qlink) == NULL)
	{
	    /* buffer not found, ouch */
	    pdbshm->shutdn = DB_EXBAD;
	    /* rlbiwrt: Invalid rl block pointer */
            FATAL_MSGN_CALLBACK(pcontext, rlFTL068);
	}
	
	/* look at the next buffer in the list */
	prlbf = XRLBF(pdbcontext, prlbf->qlink);
    }
    
    /* set pointer to bktbl structure */
    pbktbl = &(prlbf->rlbktbl);
    
    TRACE1_CB(pcontext, "rlbiwrt addr=%l", pbktbl->bt_dbkey)

    MT_FREE_BIB(&depthSave);
    bkWrite(pcontext, pbktbl, UNBUFFERED);
    MT_RELOCK_BIB(depthSave);
#ifdef BITRACERW
    MSGD_CALLBACK(pcontext, (TEXT *)"%Lrlbiwrt: bt_dbkey=%l", 
                 pbktbl->bt_dbkey);
#endif /* bitracerw */

    /* look at space used in last block in buffer */
   if ((XRLBLK(pdbcontext, pbktbl->bt_qbuf))->rlblkhdr.rlused < prl->rlbiblksize)
   {
       /* count partially filled buffers */
       STATINC (bipwCt);
   }
}


/* PROGRAM: rlbiout - designate a .bi buffer as the current output buffer
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlbiout(
	dsmContext_t	*pcontext,
	RLBLK		*prlblk)     /* points to the (new) output buffer */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm = pdbcontext->pdbpub;
	RLCTL	*prl = pdbcontext->prlctl;
	RLBF	*prlbf;
	RLBF	*pin;
	RLBF	*pout;
	QRLBF	q;
        COUNT   depthSave;

TRACE1_CB(pcontext, "rlbiout addr=%l", 
                    XRLBF(pdbcontext, prl->qoutbf)->rlbktbl.bt_dbkey)

retry:
    if (prlblk == XRLBLK (pdbcontext, prl->qoutblk))
        return; /* already is the output buf */

    if (XRLBF(pdbcontext, prl->qoutbf)->rlbktbl.bt_busy)
    {
        /* current output buffer is being written, wait for it */

        rlbiwait(pcontext, XRLBF(pdbcontext, prl->qoutbf)->rlbktbl.bt_dbkey,
                 (int)0);
	goto retry;
    }
    if (prlblk == XRLBLK (pdbcontext, prl->qinblk))
    {
	/* it is the current input buffer */

        pout = XRLBF(pdbcontext, prl->qoutbf);
	if (pout->rlbktbl.bt_flags.changed)
	{
	    /* output buffer dirty. Queue for writing */

            rlbisched (pcontext);
	}
	/* swap input and output buffers */

	q = prl->qinbf;
	prl->qinbf = prl->qoutbf;
	prl->qinblk = prl->qoutblk;
	pin = XRLBF(pdbcontext, prl->qinbf);
	pin->bufstate = RLBSIN;
	rlsetout (pcontext, q);
	return;
    }

    /* search for the buffer */

    prlbf = XRLBF(pdbcontext, prl->qpool);
    for (;;)
    {
	if (XRLBLK(pdbcontext, prlbf->rlbktbl.bt_qbuf) == prlblk) break;
	if (prlbf->qlink == 0)
	{ 
	    pdbshm->shutdn = DB_EXBAD;
	    FATAL_MSGN_CALLBACK(pcontext, rlFTL063);
	}
	prlbf = XRLBF(pdbcontext, prlbf->qlink);
    }
    if (prlbf->rlbktbl.bt_busy)
    {
        /* buffer is being written, wait for it */

        rlbiwait (pcontext, prlbf->rlbktbl.bt_dbkey, (int)0);
	goto retry;
    }
    /* make it the output buffer */

    if (prlbf->bufstate != RLBSDIRTY)
    {
	pdbshm->shutdn = DB_EXBAD;
        FATAL_MSGN_CALLBACK(pcontext, rlFTL064, prlbf->bufstate);
    }

    /* this is a modfied buffer which has to be written */

    q = QSELF (prlbf);
    rlgetmod (pcontext, q);

    prlbf->rlbktbl.bt_busy = 1;

    /* write all earlier buffers */

    rlbiflsh (pcontext, prlbf->dpend);

    /* write this one */

    MT_FREE_BIB(&depthSave);
    bkWrite(pcontext, &(prlbf->rlbktbl), UNBUFFERED);
    MT_RELOCK_BIB(depthSave);

    if (XRLBLK(pdbcontext, 
               prlbf->rlbktbl.bt_qbuf)->rlblkhdr.rlused < prl->rlbiblksize)
        STATINC (bipwCt);
    prlbf->rlbktbl.bt_busy = 0;
    if (prlbf->rlbktbl.bt_flags.changed)
    {
        prlbf->rlbktbl.bt_flags.changed = 0;
	prl->numDirty--;
	if (prl->numDirty == 0) tmdidflsh (pcontext, BKBI);
    }
    prl->rlwritten = prlbf->dpend;

    pout = XRLBF(pdbcontext, prl->qoutbf);
    if (pout->rlbktbl.bt_flags.changed)
    {
        /* old output buffer dirty - queue it for writing */

	rlputmod (pcontext, prl->qoutbf);
    }
    else
    {
        /* free old output buffer */

	rlputfree (pcontext, prl->qoutbf);
    }
    /* make the one we found be the new output buffer */

    rlsetout (pcontext, q);

    /* wake anybody waiting on buffer */

    lkwakeup (pcontext, &prl->qwrhead, prlbf->rlbktbl.bt_dbkey, BIWRITEWAIT);
}

/* PROGRAM: rlinitp -- initialize an rlcurs before using rlrdprv
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlinitp(
	dsmContext_t	*pcontext _UNUSED_,
	RLCURS		*prlcurs,     /* rl cursor to be initialized	    */
	LONG		 noteBlock,   /* block of last byte of a rl note */
	LONG		 noteOffset,  /* offset of last byte of a rl note */
	TEXT		*passybuf)  /* points to the assembly buffer, NULL if
				     rlinitp is being used to process a jump note*/
{
TRACE1_CB(pcontext, "rlinitp addr=%l", noteaddr)

    prlcurs->nextOffset = noteOffset;
    prlcurs->nextBlock = noteBlock;

    prlcurs->status   = 0;
    if (passybuf) prlcurs->passybuf = passybuf;
}
