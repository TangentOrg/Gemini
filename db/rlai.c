
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

#if 0
#include "argctl.h"
#endif
#include "bkpub.h"
#include "bmpub.h"
#include "dbcontext.h"
#include "dbpub.h"
#include "latpub.h"
#include "lkpub.h"
#include "mstrblk.h"
#include "rlpub.h"
#include "rlprv.h"
#include "rlai.h"
#include "rlmsg.h"
#include "shmpub.h"
#include "tmprv.h"
#include "usrctl.h"
#include "utcpub.h"
#include "utspub.h"
#include "uttpub.h"

#if OPSYS==UNIX
#include <unistd.h>
#endif

#include "time.h"

#if OPSYS==WIN32API
#include <io.h>
#include <process.h>
#endif

#ifdef AITRACE
#define MSGD MSGD_CALLBACK
#endif

LOCALF DSMVOID rlaiwrite (dsmContext_t *pcontext, AIBUF *paibuf);

LOCALF DSMVOID rlaiwwt (dsmContext_t *pcontext);
LOCALF AIBUF *rlainext (dsmContext_t *pcontext);
LOCALF DSMVOID rlaiwrcur(dsmContext_t *pcontext);
#if 0
LOCALF DSMVOID rlaiwtloc (dsmContext_t *pcontext, LONG aictr);
#endif

#if 0
int
rlaiDump(dsmContext_t *pcontext)
{
    AICTL      *pai = pcontext->pdbcontext->paictl;
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm = pdbcontext->pdbpub;
    AIBUF      *paibuf;
    BKTBL      *pbktbl;
    int         i;
 
    MT_LOCK_AIB();
    MSGD_CALLBACK(pcontext,(TEXT *)
                  "%LSYSTEM ERROR: BEGIN AI Control Structure Dump");
    MSGD_CALLBACK(pcontext,(TEXT *)
                  "%L aisize %l ailoc %l aiofst %l aiupdctr %l aiwritten %l",
         pai->aisize, pai->ailoc, (LONG)pai->aiofst, pai->aiupdctr,
         pai->aiwritten);
    MSGD_CALLBACK(pcontext,(TEXT *)
                  "%L Ring has %l buffers of %l size  aiqcurb: %l aiqwrtb: %l",
         pdbshm->argaibufs, pai->aiblksize, pai->aiqcurb, pai->aiqwrtb);
    MSGD_CALLBACK(pcontext,(TEXT *)
                  "%L Dumping the ring starting with aiqbufs %l",pai->aiqbufs);
 
    paibuf = XAIBUF(pdbcontext,pai->aiqbufs);
    for ( i = 0; i < pdbshm->argaibufs; i++ )
    {
      MSGD_CALLBACK(pcontext,(TEXT *)
                    "%Lqself: %l aiqnxtb: %l aisctr: %l ailctr %l",
           paibuf->qself, paibuf->aiqnxtb, paibuf->aisctr, paibuf->ailctr);
      pbktbl = &paibuf->aibk;
      if (pbktbl)
      {
          MSGD_CALLBACK(pcontext,(TEXT *)
                        "%L bt_dbkey: %l  bt_busy: %l bt_flags.changed: %l",
               pbktbl->bt_dbkey, (LONG)pbktbl->bt_busy,
               (LONG)pbktbl->bt_flags.changed);
      }
      if( paibuf->aiqnxtb )
      {
        paibuf = XAIBUF(pdbcontext,paibuf->aiqnxtb);
      }
      else
        break;
    }
 
    MT_UNLK_AIB();
    MSGD_CALLBACK(pcontext,(TEXT *)
                  "%LSYSTEM ERROR: END AI Control Structure Dump");
}
#endif

/* RLAI.C - normal progress ai entrypints */

#if SHMEMORY
/* PROGRAM: rlwakaiw -- wake up the ai writer so he can write some ai blocks
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlwakaiw (dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
FAST	AICTL	*pai = pdbcontext->paictl;
        usrctl_t  *pusr;
 
    if (pai->aiwpid == 0) return;
    if (pai->qaiwq == 0) return;
 
    pusr = XUSRCTL(pdbcontext, pai->qaiwq);
    pai->qaiwq = pusr->uc_qwtnxt;
    pusr->uc_qwtnxt = 0;
    latUsrWake (pcontext, (usrctl_t *)pusr, (int)(pusr->uc_wait));
}


/* PROGRAM: rlaiwwt - ai writer wait for something to do
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
rlaiwwt(dsmContext_t *pcontext)
{
    usrctl_t *pusr = pcontext->pusrctl;
    AICTL    *pai = pcontext->pdbcontext->paictl;
    COUNT     depthSave;
 
    pusr->uc_qwtnxt = pai->qaiwq;
    pai->qaiwq = QSELF(pusr);  
    pusr->uc_wait = AIWWAIT;
    pusr->uc_wait1 = pai->aiwritten;
    MT_FREE_AIB(&depthSave);
    latUsrWait (pcontext);
    MT_RELOCK_AIB(depthSave);
}


/* PROGRAM: rlaiwait - wait until the ia buffer is released
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlaiwait(
	dsmContext_t	*pcontext,
	DBKEY		 aidbkey)
{
    usrctl_t *pusr = pcontext->pusrctl;
    AICTL    *pai = pcontext->pdbcontext->paictl;
    COUNT     depthSave;

    /* the block is in the proccess of being written */
    /* put myself in chain of waiting users */
    /* mark that I am waiting, so they will know to wake me up*/

    STATINC (aiwtCt);
    pusr->uc_qwtnxt =  pai->qwrhead;
    pai->qwrhead = QSELF(pusr);
    pusr->uc_wait = AIWRITEWAIT;
    pusr->uc_wait1 = aidbkey;
    MT_FREE_AIB(&depthSave);
    latUsrWait (pcontext);
    MT_RELOCK_AIB(depthSave);
}
#endif /* SHMEMORY */


/* PROGRAM: rlaiwrite - write an ai buffer to disk
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
rlaiwrite(
	dsmContext_t	*pcontext,
	AIBUF		*paibuf)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm = pdbcontext->pdbpub;
	AICTL	*paictl = pdbcontext->paictl;
	bktbl_t	*pbktbl;

    if (QSELF (paibuf) != paictl->aiqwrtb)
    {
        paictl->aiwpid = 0;
	MT_UNLK_AIB ();
	FATAL_MSGN_CALLBACK(pcontext, rlFTL008);
    }


    pbktbl = &(paibuf->aibk);

    /* lock the buffer */

    pbktbl->bt_busy = 1;

    /* See if we need to flush any of the .bi file before doing this */

    if (pdbshm->fullinteg && pbktbl->bt_bidpn > 0)
    {
	MT_UNLK_AIB ();
	rlbiflsh (pcontext, pbktbl->bt_bidpn);
	MT_LOCK_AIB ();
	pbktbl->bt_bidpn = 0;
    }

#if 0
    /* remove for bug 92-02-07-006 */
    paiblk = XAIBLK(pdbcontext, pbktbl->bt_qbuf);
    paiblk->aihdr.aiused = paictl->aiofst;
#endif

    /* count partially full writes */
    if (paictl->aiofst < paictl->aiblksize) STATINC (aipwCt);

    MT_UNLK_AIB ();

#ifdef AITRACE
    MSGD(pcontext, "%Lrlaiwrite: writing AI block %D.", pbktbl->bt_dbkey);
#endif

    /* write the block out */
    bkWrite(pcontext, pbktbl, UNBUFFERED);

    MT_LOCK_AIB ();

    /* bug 92-06-24-018 */
    if (pcontext->pusrctl->uc_usrtyp & AIW) STATINC (aibcCt);

    pbktbl->bt_flags.changed = 0;
    paictl->aiwritten = paibuf->ailctr;
    paibuf->ailctr = 0;
    paibuf->aisctr = 0;

    /* mark the buffer no longer busy */

    pbktbl->bt_busy = 0;
    if (paictl->qwrhead != 0)
    {
	/* wake anyone who is waiting for write completion */
	lkwakeup (pcontext, &paictl->qwrhead, pbktbl->bt_dbkey, AIWRITEWAIT);
    }
}


/* PROGRAM: rlainext - get the next available ai buffer, writing
 *	it to disk if necessary.
 *
 * RETURNS:
 */
LOCALF AIBUF *
rlainext (dsmContext_t *pcontext)
{
        dbcontext_t *pdbcontext = pcontext->pdbcontext;
	AICTL	*paictl = pdbcontext->paictl;
	bktbl_t	*pbktbl;
	AIBUF	*paibuf;
	AIBLK	*paiblk;

#if SHMEMORY
    if (paictl->aiwpid)
    {
        /* get the ai writer to write the buffer we just filled */
        rlwakaiw (pcontext);
    }
#endif /* SHMEMORY */

retry:
    /* get pointers to the current .ai buffer */
    paibuf = XAIBUF(pdbcontext, paictl->aiqcurb);
    pbktbl = &(paibuf->aibk);

    /* if it is dirty, skip on to the next, otherwise */
    /* just reuse the same buffer                     */
    if (pbktbl->bt_flags.changed)
    {
	/* look at the next buffer in the ring */
	paibuf = XAIBUF(pdbcontext, paibuf->aiqnxtb);
	pbktbl = &(paibuf->aibk);
    }

    /* if the buffer is locked, wait for it */
    if (pbktbl->bt_busy)
    {
	rlaiwait (pcontext, pbktbl->bt_dbkey);
        goto retry;
    }

    if (pbktbl->bt_flags.changed)
    {
	/* The next buffer is dirty. Either there is no ai writer or the ai
	   writer has not written the buffer out yet. We have to write it
	   out before we can use it */

	rlaiwrite (pcontext, paibuf);

        STATINC (ainaCt); /* bug 92-06-24-018 */

	/* advance the next buffer to write pointer */
	paictl->aiqwrtb = paibuf->aiqnxtb;

#ifdef AITRACE
MSGD (pcontext, "%Lrlainext: move aiqwrtb %D",
      XAIBUF(pdbcontext, paictl->aiqwrtb)->aibk.bt_dbkey);
#endif

    }

    /* advance the current buffer to next buffer in ring */
    paictl->aiqcurb = QSELF (paibuf);
    paibuf->aisctr = paictl->aiupdctr;
    paibuf->ailctr = paictl->aiupdctr;

	
#if SHMEMORY
    /* mark buffer as busy */
    pbktbl->bt_busy = 1;
#endif

    /* advance the ai file position by one block */
    paictl->ailoc += paictl->aiblksize;
    pbktbl->bt_dbkey = paictl->ailoc;

    if (paictl->ailoc == paictl->aisize)
    {
	/* extent is full extend file or switch */
	rlaixtn (pcontext);
    }

    /* Start at beginning of the new block: */
    paiblk = XAIBLK(pdbcontext, paibuf->aibk.bt_qbuf);

    if (pdbcontext->prlctl->rlwarmstrt)
    {
	/* have to read it during warm start because we are going
	   to look at what got written and not write again if the
	   ai note is already there - see below */

        bkRead(pcontext, pbktbl);
        if (paiblk->aihdr.aihdrlen == 0 ||
	    paiblk->aihdr.ainew != pdbcontext->pmstrblk->mb_ai.ainew)
        {
	    /* this is a virgin block or its
	       or its an old block from a previous time as the
	       busy extent. */

	    stnclr ((TEXT *)paiblk, (int)paictl->aiblksize);
 	    paiblk->aihdr.aihdrlen = sizeof(AIHDR);
	    paiblk->aihdr.ainew = pdbcontext->pmstrblk->mb_ai.ainew;
        }
    }
    else
    {
	/* don't do the read to gain some speed */
        bkaddr (pcontext, pbktbl,1);

	stnclr ((TEXT *)paiblk, (int)paictl->aiblksize);
        paiblk->aihdr.aihdrlen = sizeof (AIHDR);
	paiblk->aihdr.ainew = pdbcontext->pmstrblk->mb_ai.ainew;
    }

    /* offset into the new block */
    paictl->aiofst = paiblk->aihdr.aihdrlen;
    paiblk->aihdr.aiused = paictl->aiofst;

#ifdef AITRACE
MSGD (pcontext, "%Lrlainext: move aiqcurb to %D", pbktbl->bt_dbkey);
#endif

#if SHMEMORY
    pbktbl->bt_busy = 0;
    if (paictl->qwrhead != 0)
    {
        /* wake anyone who is waiting for read completion */
	
        lkwakeup (pcontext, &paictl->qwrhead, pbktbl->bt_dbkey, AIWRITEWAIT);
    }
#endif /* SHMEMORY */
    return (paibuf);
}

#if SHMEMORY
/* PROGRAM: rlaiwrspare -  write out the spare ai buffer.
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
rlaiwrspare(dsmContext_t *pcontext)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm = pdbcontext->pdbpub;
    AICTL	*paictl = pdbcontext->paictl;
    bktbl_t	*pbktbl;
    AIBLK	*paiblk;
    ULONG        aiNoteCounter;
    
    pbktbl = &paictl->aiwbktbl;

    /* count partially full writes */

    paiblk = XAIBLK(pdbcontext, pbktbl->bt_qbuf);
#if 0
    /* remove for bug 92-02-07-006 */
    paiblk->aihdr.aiused = paictl->aiofst;
#endif
    if (paictl->aiofst < paictl->aiblksize) STATINC (aipwCt);

    pbktbl->bt_busy = 1;

    /* Record the current note counter before releasing the AIB latch. */
    aiNoteCounter = paictl->aiupdctr;

    /* See if we need to flush any of the .bi file before doing this */

    if (pdbshm->fullinteg && pbktbl->bt_bidpn > 0)
    {
        MT_UNLK_AIB ();
        rlbiflsh (pcontext, pbktbl->bt_bidpn);
        MT_LOCK_AIB ();
        pbktbl->bt_bidpn = 0;
    }

    MT_UNLK_AIB ();

#ifdef AITRACE
MSGD (pcontext, "%Lrlaiwrspare: writing AI block %D", pbktbl->bt_dbkey);
#endif

    /* write the block out */
    bkWrite(pcontext, pbktbl, UNBUFFERED);

    MT_LOCK_AIB ();

    pbktbl->bt_flags.changed = 0;
    paictl->aiwritten = aiNoteCounter;

    /* mark the buffer no longer busy */

    pbktbl->bt_busy = 0;
    if (paictl->qwrhead != 0)
    {
	/* wake anyone who is waiting for write completion */
	
	lkwakeup (pcontext, &paictl->qwrhead, pbktbl->bt_dbkey, AIWRITEWAIT);
    }
}
#endif /* SHMEMORY */

/* PROGRAM: rlaiwrcur - write the current ai buffer
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
rlaiwrcur(dsmContext_t *pcontext)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    AICTL	*paictl = pdbcontext->paictl;
    AIBUF	*paibuf;
    bktbl_t	*pbktbl;
    bktbl_t	*ptmptbl;
    TEXT	*p1;
    TEXT	*p2;

retry:
    paibuf = XAIBUF(pdbcontext, paictl->aiqcurb);
    pbktbl = &(paibuf->aibk);
    if (pbktbl->bt_flags.changed == 0) return;

    ptmptbl = &(paictl->aiwbktbl);

#if SHMEMORY
    if (pdbcontext->argnoshm)
    {
	/* no point in copying the current buffer */

	rlaiwrite (pcontext, paibuf);
	return;
    }
    if (pbktbl->bt_flags.changed == 0) return;

    if (ptmptbl->bt_busy)
    {
	/* someone has locked the copy buffer */

	rlaiwait (pcontext, ptmptbl->bt_dbkey);
	goto retry;
    }
    if (pbktbl->bt_busy)
    {
	/* someone has locked the current buffer */

	rlaiwait (pcontext, pbktbl->bt_dbkey);
	goto retry;
    }
    /* flag output buffer as write pending */

    pbktbl->bt_busy = 2;

    /* copy the current output buffer so we can release the other one. */

    ptmptbl->bt_dbkey = pbktbl->bt_dbkey;
    ptmptbl->bt_raddr = pbktbl->bt_raddr; 
    ptmptbl->bt_qftbl = pbktbl->bt_qftbl;
    ptmptbl->bt_offst = pbktbl->bt_offst; 
    ptmptbl->bt_bidpn = pbktbl->bt_bidpn;
    ptmptbl->bt_flags.changed = 1;
    p1 = (TEXT *)(XAIBLK(pdbcontext, pbktbl->bt_qbuf));
    p2 = (TEXT *)(XAIBLK(pdbcontext, ptmptbl->bt_qbuf));
    bufcop (p2, p1, paictl->aiblksize);

    /* write it out */

    rlaiwrspare (pcontext);

    pbktbl->bt_busy = 0;
    if (paictl->qwrhead != 0)
    {
	/* wake anyone who is waiting for write completion */
	
	lkwakeup (pcontext, &paictl->qwrhead, pbktbl->bt_dbkey, AIWRITEWAIT);
    }
#else
    rlaiwrite (pcontext, paibuf);

#endif /* SHMEMORY */
    tmdidflsh (pcontext, BKAI);	/* tell tm manager that all ai notes flushed */
}


/* PROGRAM: rlaiflsx -- Write out unwritten ai buffers already locked 
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlaiflsx(dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    AICTL	*paictl = pdbcontext->paictl;
    AIBUF	*paibuf;
    bktbl_t	*pbktbl;
    
    if (paictl == NULL) return;

retry1:

    
    for (;;)
    {
        /* examine next buffer to write */

        paibuf = XAIBUF(pdbcontext, paictl->aiqwrtb);
        
	if (paibuf->aibk.bt_flags.changed == 0) break;

	/* This buffer is dirty so we have to write it */

	pbktbl = &(paibuf->aibk);
#if SHMEMORY
	if (pbktbl->bt_busy)
	{
	    /* someone has buffer locked */
	    rlaiwait (pcontext, pbktbl->bt_dbkey);
	    goto retry1;
	}
#endif /* SHMEMORY */
	if (paictl->aiqwrtb == paictl->aiqcurb)
	{
	    /* this is the current output buffer */

            rlaiwrcur (pcontext);
	    break;
	}
	/* write the buffer */

        rlaiwrite (pcontext, paibuf);

	/* advance the write pointer to the next buffer in the ring */

	paictl->aiqwrtb = paibuf->aiqnxtb;
#ifdef AITRACE
MSGD(pcontext, "%Lrlaiflsx: move aiqwrtb to %D",
     XAIBUF(pdbcontext, paictl->aiqwrtb)->aibk.bt_dbkey);
#endif

    }

#ifdef AITRACE
MSGD (pcontext, "%Lrlaiflsx: flushed up to %D", pbktbl ? pbktbl->bt_dbkey : 0L);
#endif

}


/* PROGRAM: rlaifls - flush unwritten ai buffers
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlaifls (dsmContext_t *pcontext)
{
    MT_LOCK_AIB ();
    rlaiflsx (pcontext);
    MT_UNLK_AIB ();
}

#if SHMEMORY
#if 0
/* PROGRAM: rlaiwtloc - wait until specified ai file location has been
 *			written to disk
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
rlaiwtloc (
	dsmContext_t	*pcontext,
	LONG		 aictr)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm = pdbcontext->pdbpub;
FAST	AICTL	*pai = pdbcontext->paictl;
	AIBUF	*paibuf;
	bktbl_t	*pbktbl;

    if (pdbcontext->prlctl->rlwarmstrt) return;

    MT_LOCK_AIB ();
    if (aictr <= pai->aiwritten)
    {
	/* has been written */

        MT_UNLK_AIB ();
	return;
    }
    /* the ai note has not yet been written */

    if ((pai->aiwpid == 0) || (pdbshm->shutdn))
    {
        /* have to write it ourself */

        rlaiflsx (pcontext);
        MT_UNLK_AIB ();
        return;
    }
    /* find the buffer which contins the one we want */

    paibuf = XAIBUF(pdbcontext, pai->aiqwrtb);
    for (;;)
    {
	if (paibuf->aibk.bt_flags.changed == 0) break;

	if ((aictr >= paibuf->aisctr) &&
	    (aictr <= paibuf->ailctr))
	{
	    /* This one */

	    pbktbl = &(paibuf->aibk);

            /* kick the ai writer and wait */

            STATINC (aidwCt);
            pai->nWaiters++;

            rlwakaiw (pcontext);

	    rlaiwait (pcontext, pbktbl->bt_dbkey);
            pai->nWaiters--;

            MT_UNLK_AIB ();
	    return;
	}
	if (QSELF (paibuf) == pai->aiqcurb) break;

	/* advance to the next one */

	paibuf = XAIBUF(pdbcontext, paibuf->aiqnxtb);
    }
    /* oops */

    MT_UNLK_AIB ();
    FATAL_MSGN_CALLBACK(pcontext, rlFTL018, aictr);
}
#endif


/* PROGRAM: rlaiclean -
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlaiclean (dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm = pdbcontext->pdbpub;
FAST	AICTL	*pai = pdbcontext->paictl;
FAST	bktbl_t	*pbktbl;
	AIBUF	*paibuf;

    if (pai == NULL) return;
    MT_LOCK_AIB ();
    for ( ;; )
    {
        /* wait for someone to wake us up */

        if( pdbshm->bwdelay == 0 )
            rlaiwwt (pcontext);
        
	if (pcontext->pusrctl->usrtodie) break;
        if (pdbshm->shutdn) break;

	/* start at the next buffer to write and go forward till we get to the
	   current buffer */

	for (;;)
	{
	    paibuf = XAIBUF(pdbcontext, pai->aiqwrtb);
	    pbktbl = &(paibuf->aibk);
	    if (QSELF (paibuf) == pai->aiqcurb)
	    {
		/* This is the current buffer. Only need to write it
		   if someone is waiting for it */
		
		break;
	    }
	    /* not the current buffer */

	    if (pbktbl->bt_flags.changed == 0)
	    {
                pai->aiwpid = 0;
                MT_UNLK_AIB ();
		FATAL_MSGN_CALLBACK(pcontext, rlFTL020);
	    }
            if (pbktbl->bt_busy)
	    {
		/* someone has it locked so we have to wait for it */

		rlaiwait (pcontext, pbktbl->bt_dbkey);
		continue;
	    }

	    rlaiwrite (pcontext, paibuf);

	    /* advance the write pointer to the next buffer */

	    pai->aiqwrtb = paibuf->aiqnxtb;
#ifdef AITRACE
MSGD (pcontext, "%Lrlaickean: move aiqwrtb to %D",
      XAIBUF(pdbcontext, pai->aiqwrtb)->aibk.bt_dbkey);
#endif
	}
        if( pdbshm->bwdelay > 0)
        /* Caller will nap and then call us again.   */
            break;
    }
    pai->aiwpid = 0;
    MT_UNLK_AIB ();
}
#endif /* SHMEMORY */

/* PROGRAM: rlaiwrt - write a note to the after-image file
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlaiwrt(
	dsmContext_t	*pcontext,
	RLNOTE *pnote,   /* variable length note structure		*/
	COUNT  len,      /* length of optional variable len data, or 0  */
	TEXT   *pdata,   /* optional variable len data, or PNULL	*/
	LONG   updctr,   /* ai block update counter used to determine   */
	LONG64   bidpend)  /* bi note counter for flushing bi */
{
	AICTL	*pai = pcontext->pdbcontext->paictl;
	TEXT	mi_len[sizeof(COUNT)];	 /* Machine independent format */
	COUNT	tlen;

#ifdef AITRACE
MSGD (pcontext, "%Lrlaiwrt: ailoc %i aictr %i note %i",
       pai->ailoc + pai->aiofst, updctr, pnote->rlcode);
#endif

    STATINC (ainwCt);
    tlen = (COUNT)(pnote->rllen + len + (2 * sizeof (COUNT)));

    /* Set up total note length */
    sct(mi_len, (COUNT)tlen);

    /* And write it out */
    rlaimv(pcontext, mi_len, (COUNT)sizeof(mi_len), updctr, bidpend);

    /* write the fixed part of the note */
    rlaimv(pcontext, (TEXT *)pnote, (COUNT)pnote->rllen, updctr, bidpend);

    /* write the variable part - if any */
    if (len)
	rlaimv(pcontext, pdata, len, updctr, bidpend);

    /* finish it off with the length */
    /* the final parm: 1 tells rlaimv this is the end of a note */
    rlaimv(pcontext, mi_len, (COUNT)sizeof(mi_len), updctr, bidpend);

    pcontext->pusrctl->uc_ailoc = pai->ailoc;
    pcontext->pusrctl->uc_aictr = updctr;
    pai->aiupdctr = updctr;
}

/* PROGRAM: rlaimv - Internal procedure to move data into the ai output	*
 *		     buffer, update appropriate control variables and	
 *		     detect when the block is full.
 *
 * RETURNS:
 */
int
rlaimv(
	dsmContext_t	*pcontext,
	TEXT  *pdata,  /* data to put in ai output buffer	  */
	COUNT len,     /* length of data				  */
	LONG  updctr,  /* ai block update counter used to determine*/
                      /* if the note was already removed	  */ 
	LONG64 bidpend) /* bi note counter for flushing bi          */
{
        dbcontext_t *pdbcontext = pcontext->pdbcontext;
	AICTL	*paictl = pdbcontext->paictl;
	AIBUF	*paibuf;
	AIBLK	*paiblk;
	int	 this_move;
	int	 retcode = 0;	/* return non zero if we wrote	*/

    STATADD (aibwCt, (ULONG)len);

    paibuf = XAIBUF(pdbcontext, paictl->aiqcurb);

    paiblk = XAIBLK(pdbcontext, paibuf->aibk.bt_qbuf);

#ifdef AITRACE
MSGD (pcontext, "%Lrlaimv: moving %d bytes with updctr %l", len, updctr);
#endif

    while(len > 0)
    {
#if SHMEMORY
        if (paibuf->aibk.bt_busy != 2)
	{
            while (paibuf->aibk.bt_busy)
            {
		/* wait for the buffer to become free */

                rlaiwait (pcontext, paibuf->aibk.bt_dbkey);
            }
	}
#endif /* SHMEMORY */
	if (paictl->aiofst == paictl->aiblksize)
	{
	    /* buffer is full, get a new one */

	    paibuf = rlainext (pcontext);
            paiblk = XAIBLK(pdbcontext, paibuf->aibk.bt_qbuf);
	}
	/* Determine how much will fit in this block */

	this_move = paictl->aiblksize - paictl->aiofst;
	if (this_move > len) this_move = len;

	/* Conditionally move it in to the output buffer */

	if (updctr >= (LONG)paiblk->aihdr.aiupdctr)
	{
	    /* kludge: the return code is used during		*/
	    /* warm restart.  The caller wants to know if the	*/
	    /* block was updated.  if updctr==paiblk->aiupdctr	*/
	    /* then the block wont be modified.  We enter this	*/
	    /* block of code because during normal execution,	*/
	    /* there are several sequential calls to rlaimv with*/
	    /* the same updctr					*/

	    if (updctr > (LONG)paiblk->aihdr.aiupdctr)
	    {
		retcode = 1;
		paibuf->aibk.bt_flags.changed = 1;
		paibuf->aibk.bt_bidpn = bidpend;
	    }
	    paiblk->aihdr.aiupdctr = updctr;
	    bufcop((TEXT *)paiblk+paictl->aiofst, pdata, this_move );
#ifdef AITRACE
MSGD (pcontext, "%Lrlaimv: bufcop %d bytes to offset %l",
      this_move, paictl->aiofst);
#endif

	    /* bug 92-02-07-006 */
	    paiblk->aihdr.aiused = paictl->aiofst + this_move;

	}
	paictl->aiofst += this_move;
	pdata += this_move;
	len -= this_move;
	paibuf->ailctr = updctr;
    }
    if (paictl->aiofst == paictl->aiblksize)
    {
        /* BUG 97-10-10-027
           This move exactly filled the buffer -- set a new one */
        paibuf = rlainext (pcontext);
    }

    return retcode;
}

/* PROGRAM: rlaiqon - query if the after-image facility is enabled this db *
 *
 * RETURNS:
 *          1 - aimage is enabled.                                         *
 *          0 - No it isnt                                                 *
 */
int
rlaiqon(dsmContext_t *pcontext)
{
    mstrblk_t  *pmstr = pcontext->pdbcontext->pmstrblk; 

    if (pmstr->mb_ai.ainew)
    {
        return 1;
    }
    else
        return 0;
}


/* 
 * PROGRAM: rlaixtn - extend the after-image file
 *
 * If the note won't fit in the ai file and the ai file can't
 * be extended, then this routine generates a FATAL error		
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlaixtn(dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm = pdbcontext->pdbpub;
    int          switchMsg, rc, switch_command;
    COUNT        depthSave;
	
    /* check if we are using ai extents */
    if( pdbcontext->pmstrblk->mb_aibusy_extent == 0 )
    {
	MT_FREE_AIB(&depthSave);
	/* Bugfix 92-10-16-022 Test for < 64L instead of != 64L */
	if ( bkxtn(pcontext, pdbcontext->paictl->aiArea, 64L)
	    < 64L )    /* extend the after-image file.*/
	{
	    pdbshm->shutdn = DB_EXBAD;
	    FATAL_MSGN_CALLBACK(pcontext, rlFTL001);  /* insufficient disk space */
	}

	/* update size information in ai control structure */
	pdbcontext->paictl->aisize = 
	    (gem_off_t)bkflen(pcontext, pdbcontext->paictl->aiArea) << pdbcontext->paictl->aiblklog;

#ifdef AITRACE
MSGD (pcontext, "%Lrlaixtn: extended AI file to ", pdbcontext->paictl->aisize);
#endif

	MT_RELOCK_AIB(depthSave);
    }
    else
    {
	/* using ai extents, try extending/switching first */
	switch_command = RLAI_SWITCH;
        switchMsg = 1;
	while(( rc = rlaiswitch(pcontext, switch_command,
                switchMsg )) == RLAI_CANT_SWITCH
                || rc == RLAI_CANT_EXTEND )
	{
	    /* could not extend or switch, determine what the problem is */
	    if( rc == RLAI_CANT_EXTEND )
	    {
		/* can't extend variable extent try switch to next extent. */
		switch_command = RLAI_TRY_NEXT;
	    }
	    else
	    {
                switchMsg = 0;
		/* can't switch for some reason, wait a few seconds, retry */
		MT_FREE_AIB(&depthSave);
		utsleep( 3 );
		MT_RELOCK_AIB(depthSave);
	    }
	}
    }
}

/*
 * PROGRAM: rlaiGetNextArea - returns the next ai slot found in the 
 *                            the area table in shared memory.
 *
 * RETURNS:  >0  slot number of next ai area.
 *            0  no next area found.
 */
ULONG
rlaiGetNextArea(
	dsmContext_t	*pcontext,
	ULONG		 areaSlot )
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    bkAreaDesc_t  *pbkAreaDesc;
    ULONG          i;
	ULONG          numAreaSlots;
	
	numAreaSlots = XBKAREADP
		(pdbcontext, pdbcontext->pbkctl->bk_qbkAreaDescPtr)->bkNumAreaSlots;
	for ( i = areaSlot + 1; i < numAreaSlots; i++ )
	{
	    pbkAreaDesc = BK_GET_AREA_DESC(pdbcontext, i);
	    if ( pbkAreaDesc && 
		pbkAreaDesc->bkAreaType == DSMAREA_TYPE_AI && 
		pbkAreaDesc->bkNumFtbl > 0)
	    {
	       return( i );
	    }
	}
	for ( i = 1 ; i < areaSlot; i++ )
	{
	   pbkAreaDesc = BK_GET_AREA_DESC(pdbcontext, i);
	   if ( pbkAreaDesc &&
	       pbkAreaDesc->bkAreaType == DSMAREA_TYPE_AI && 
	       pbkAreaDesc->bkNumFtbl > 0)
	   {
	      return( i );
	   }
	}
	return ( 0 );
}


/* PROGRAM: rlaiGetLastArea - returns the last ai slot found in the 
 *                            the area table in shared memory in pareaSlot.
 *                            pareaSlot wil always be >= DSMAREA_BASE
 *
 * RETURNS:   0  success - last ai area found
 *           -1  no ai area found.
 */
LONG
rlaiGetLastArea(
	dsmContext_t	*pcontext,
	ULONG		*pareaSlot )
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    bkAreaDesc_t  *pbkAreaDesc;
    ULONG          numAreaSlots;
	
    numAreaSlots = XBKAREADP
                    (pdbcontext, pdbcontext->pbkctl->bk_qbkAreaDescPtr)->bkNumAreaSlots;

    for ( *pareaSlot = numAreaSlots - 1;
          *pareaSlot >= DSMAREA_BASE;
          (*pareaSlot)-- )
    {
        pbkAreaDesc = BK_GET_AREA_DESC(pdbcontext, *pareaSlot);
        if ( pbkAreaDesc && 
             pbkAreaDesc->bkAreaType == DSMAREA_TYPE_AI)
        {
           return(0);
        }
    }

    *pareaSlot = DSMAREA_BASE;

    return ( -1 );

}  /* end rlaiGetLastArea */

/*
 * PROGRAM: rlaiNumAreas - Returns the number of ai areas found in the
 *                         shared memory area table.
 * 
 *
 * RETURNS: >=0 if success, -1 on failure
 */
LONG
rlaiNumAreas(dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
	bkAreaDesc_t *pbkAreaDesc;
	ULONG        numAreaSlots, areaNumber;
	LONG         aiAreas = 0;
	
	numAreaSlots = XBKAREADP
		(pdbcontext, pdbcontext->pbkctl->bk_qbkAreaDescPtr)->bkNumAreaSlots;
	
	for( areaNumber = 1; areaNumber < numAreaSlots; areaNumber++ )
	{
	    pbkAreaDesc = BK_GET_AREA_DESC(pdbcontext, areaNumber);
	    if( pbkAreaDesc && 
	       pbkAreaDesc->bkAreaType == DSMAREA_TYPE_AI)
	    {
		aiAreas++;
	    }
	}
	return aiAreas;
}


/*
 * PROGRAM: rlaiswitch - Switch logging of after images to next extent.
 *
 * This procedure supports on-line switching and extending of ai files.
 * A command is passed in as an argument to tell what operation to perform:
 *
 * RLAI_SWITCH   - runtime extent switch of fixed extent or extend variable
 * RLAI_NEW      - runtime extent switch always for aimage new/online backup 
 * RLAI_TRY_NEXT - if can't extend variable length, try switching to next one
 *
 * A request fo RLAI_SWITCH will try to switch fixed length ai extents, but
 * will attempt to extend a variable length extent.  RLAI_NEW always attempts
 * to switch regardless of the current extent type.  RLAI_TRY_NEXT allows the
 * procedure to switch to the next extent when the current variable length
 * extent cannot be extended (for example if the device is full).
 *
 * If an extent swich is done, a complex set of operations are done to:
 * 1) write bi note for extent switch
 * 2) write old extent marking it as full and set extent eof
 * 3) write new extent as open with initialized header info
 *
 * RETURNS:  0 for success
 *           RLAI_CANT_SWITCH if next extent not empty
 *           RLAI_CANT_EXTEND if can't extend variable length extent. 
 */
int
rlaiswitch(
	dsmContext_t	*pcontext,
	int		 command,
        int              switchMsg)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm = pdbcontext->pdbpub;
    AICTL	*paictl = pdbcontext->paictl;  /* ai control */
    mstrblk_t   *pmstr = pdbcontext->pmstrblk; /* master block */
	
    bkftbl_t  *pbkftbl = (bkftbl_t *)NULL;  /* file table entry */
	
    AIBUF  *paibuf;              /* pointer to current ai buffer control */
    AIBLK  *paiblk;              /* pointer to current ai output buffer */
    AIBLK  *ptmpblk;             /* pointer to spare ai output buffer */
	
    bktbl_t *pbktbl;              /* bktbl for current ai buffer */
    bktbl_t *ptmpbktbl;           /* bktbl for spare ai buffer */
	
    ULONG   fullArea;            /* extent number for old extent */
    ULONG   busyArea;         /* extent number for new extent */
    LONG    ret;
    int     i;
    COUNT   depthSave;
	
    /* get file table entry for current extent */
    ret = bkGetAreaFtbl(pcontext, paictl->aiArea,&pbkftbl);
    
    /* check to see switching is required - fixed extent or command */
    if( (pbkftbl->ft_type & BKFIXED) || command )
    {
		/* Switch to next extent if possible */

#if TRACE_AIBUGS
                ptmpbktbl = &paictl->aiwbktbl;
                MSGD_CALLBACK(pcontext,
                              "%LBefore flush:- bt_busy=%d",
                               ptmpbktbl->bt_busy);
#endif
                
		/* flush everything currently in ai buffers */
		rlaiflsx(pcontext);
		
#if TRACE_AIBUGS
                MSGD_CALLBACK(pcontext,
                              "%LAfter flush:- bt_busy=%d",
                               ptmpbktbl->bt_busy);
#endif

		/* Get the current ai buffer control block and output buffer */
		paibuf = XAIBUF(pdbcontext, paictl->aiqcurb);
		pbktbl = &(paibuf->aibk);
		paiblk = XAIBLK(pdbcontext, pbktbl->bt_qbuf);
		
		/* save the existing ai area context for later */
		fullArea = paictl->aiArea;
		
		/* find the next ai area.                        */
		busyArea = rlaiGetNextArea(pcontext, paictl->aiArea );                  
		if ( busyArea == 0 )
		{
		    pdbshm->shutdn = DB_EXBAD;
		    FATAL_MSGN_CALLBACK(pcontext, rlFTL042);
		}
		
		
		/* use the spare ai buffer as a working buffer */
		ptmpbktbl = &paictl->aiwbktbl;
		ptmpblk = XAIBLK(pdbcontext, ptmpbktbl->bt_qbuf);
		
#if SHMEMORY
		/* if the buffer is locked, wait for it */
		while (ptmpbktbl->bt_busy)
		{
			rlaiwait (pcontext, ptmpbktbl->bt_dbkey);
		}
		
		/* lock the spare ai buffer since we may unlock AIB latch */
		ptmpbktbl->bt_busy = 1;
#endif /* SHMEMORY */
		
		/* setup to read initial ai block in file */
		ptmpbktbl->bt_dbkey = 0;
		ptmpbktbl->bt_area = busyArea;
		/* read in the new extent header */
		bkRead(pcontext, ptmpbktbl);
		
		ret = bkGetAreaFtbl(pcontext, busyArea, &pbkftbl);
		
		/* check to see if we can switch extents */	
		if( ptmpblk->aifirst.aiextstate != AIEXTEMPTY )
		{
			/* no - set ai extent back as before attempted extent switch */
                        /* BUM - Assuming ULONG fullArea < 2^16 */
			pmstr->mb_aibusy_extent = (UCOUNT)fullArea;
			
			/* unlock spare ai buffer */
			ptmpbktbl->bt_busy = 0;
			ptmpbktbl->bt_area = fullArea;
			/* 
			* Next extent is full - this is always a fatal error in single 
			* user mode, during crash recovery, or if db shutdown flag has
			* been set.  It's also fatal as the default unless the broker
			* has been started with the -aistall parameter.
			*
			* On the other hand, we never want to fatal if it is an aimage new
			* command or online backup which is causing the extent switch.
			*/
			if ((pdbcontext->arg1usr || 
				 pdbcontext->prlctl->rlwarmstrt ||
				 pdbshm->shutdn || 
				 !pdbshm->aistall)
				&& (command != RLAI_NEW) )
			{
				/* set downdown flag */
				pdbshm->shutdn = DB_EXBAD;
				
				/* shutdown because we cannot switch extents to new extent */
				MSGN_CALLBACK(pcontext, rlMSG052,
                                             XTEXT(pdbcontext, pbkftbl->ft_qname));
				
				MT_UNLK_AIB();
                               /* Server shutting down, no -aistall set */
                                MSGN_CALLBACK(pcontext, rlMSG121); 
				FATAL_MSGN_CALLBACK(pcontext, rlFTL021);
			}
			else
			{
				/* User specified -aistall arg starting broker */
				if ( switchMsg )
				{
					MSGN_CALLBACK(pcontext, rlMSG053,
                                      XTEXT(pdbcontext, pbkftbl->ft_qname));
					MSGN_CALLBACK(pcontext, rlMSG054);
				}
				return(RLAI_CANT_SWITCH);
			}
		}
		
		/* clear the cantswitch flag */
		
		/* quickly switch back to old extent to get ai header */
		
		ptmpbktbl->bt_area = fullArea;
		/* read the ai header block using the spare ai buffer */
		bkRead(pcontext, ptmpbktbl);
		
		/* mark the extent as full and set eof to aictl.loc + ofst */
		ptmpblk->aifirst.aiextstate = AIEXTFULL;
		ptmpblk->aifirst.aieof = paictl->ailoc + paictl->aiofst;
		
		/* use the current ai buffer to initialize new ai extent */
		pbktbl->bt_dbkey = 0;
		pbktbl->bt_area = busyArea;
		
		/* clear the current output buffer for new ai header */
		stnclr((TEXT *)paiblk, (int)paictl->aiblksize );
		
		/* initialize the header information */
		paiblk->aihdr.aihdrlen = sizeof(AIHDR) + sizeof(AIFIRST);
		paiblk->aihdr.aiused = sizeof(AIHDR) + sizeof(AIFIRST);
		paiblk->aihdr.aiupdctr = 0;
		paiblk->aihdr.aiblknum = 0;
		
		/* initialize extended header in first block */
		paiblk->aifirst.aidates.aibegin = pmstr->mb_ai.aibegin;
		time((time_t *)&paiblk->aifirst.aidates.ainew);
		paiblk->aihdr.ainew = paiblk->aifirst.aidates.ainew;
		paiblk->aifirst.aidates.aigennbr = ++pmstr->mb_ai.aigennbr;
		paiblk->aifirst.aidates.aiopen = paiblk->aifirst.aidates.ainew;
		paiblk->aifirst.lstmod = pmstr->mb_lstmod;
		paiblk->aifirst.aiversion = AIVERSION;
		paiblk->aifirst.aiblksize = pmstr->mb_aiblksize;
		paiblk->aifirst.aiextstate = AIEXTBUSY;
		paiblk->aifirst.aifirstctr = 0;
		paiblk->aifirst.aieof = 0;
		
		/* initialize master block with new ai dates */
		pmstr->mb_ai.ainew  = paiblk->aifirst.aidates.ainew;
		pmstr->mb_ai.aiopen = paiblk->aifirst.aidates.ainew;
		
		/* initialize ai control for new extent */
		paictl->ailoc = 0;
		paictl->aiofst = paiblk->aihdr.aihdrlen;
		paictl->aisize = rlaielen(pcontext, busyArea );
		
		/* Reset the ai counter                                */
		pdbcontext->pmstrblk->mb_aictr = 0;
		pdbcontext->pmstrblk->mb_aibusy_extent = (TEXT)busyArea;
		
#if TRACE_AIBUGS
                MSGD_CALLBACK(pcontext, 
                (TEXT*)"%LBefore UNLK_AIB: oldextstate %d, newextstate %d", 
                ptmpblk->aifirst.aiextstate,
                paiblk->aifirst.aiextstate);
#endif

		/* generate the extent switch note to offically switch */
		MT_UNLK_AIB();
		if( pdbcontext->prlctl->rlwarmstrt != 1 )
		{
			MT_LOCK_BIB();
			rlaiextwt(pcontext, 0 );
			MT_UNLK_BIB();

#if TRACE_AIBUGS
                        MSGD_CALLBACK(pcontext,
                              "%LBefore rlbiflsh:- bt_busy=%d",
                               ptmpbktbl->bt_busy);
#endif

			rlbiflsh(pcontext, RL_FLUSH_ALL_BI_BUFFERS);

#if TRACE_AIBUGS
                        MSGD_CALLBACK(pcontext,
                              "%LAfter rlbiflsh:- bt_busy=%d",
                               ptmpbktbl->bt_busy);
#endif

		}
		else
		{
			rlaiextwt(pcontext, 1 );
		}
		MT_LOCK_AIB();
		
		
#if TRACE_AIBUGS
                MSGD_CALLBACK(pcontext, 
                (TEXT*)"%LAfter UNLK_AIB: oldextstate %d, newextstate %d", 
                ptmpblk->aifirst.aiextstate,
                paiblk->aifirst.aiextstate);
#endif

		/* write the old full extent header */
		bkaddr(pcontext, ptmpbktbl,1);
		bkWrite(pcontext, ptmpbktbl,UNBUFFERED);
		
		/* unlock spare ai buffer since we are done with it */
		ptmpbktbl->bt_busy = 0;
		ptmpbktbl->bt_area = busyArea;
		
		/* write the new busy ai extent header */
		bkaddr(pcontext, pbktbl,1);
		bkWrite(pcontext, pbktbl,UNBUFFERED);
		
		/* clear first note flag */
		pdbshm->didchange = (TEXT)0;
		
		/* Change all of the ai buffer control structures to reference
			the new busy area                                           */
		paictl->aiArea = busyArea;
		paibuf = XAIBUF(pdbcontext, paictl->aiqbufs);
		for( i = 0 ; i < pdbshm->argaibufs; i++)
		{
		    pbktbl = &(paibuf->aibk);
			pbktbl->bt_area = busyArea;
			paibuf = XAIBUF(pdbcontext, paibuf->aiqnxtb);
		}
		/* flush out the master block to top it all off */
		MT_UNLK_AIB();
		bmFlushMasterblock(pcontext);
		MSGN_CALLBACK(pcontext, rlMSG055,
                              XTEXT(pdbcontext, pbkftbl->ft_qname));
		MSGN_CALLBACK(pcontext, rlMSG056, pmstr->mb_ai.aigennbr);
		MT_LOCK_AIB();
		
#ifdef AITRACE
		MSGD(pcontext, "%Lrlaiswitch: switched to AI extent %d",
                     (int)busy_extent);
#endif
		
    }
    else
    {
		/* try and extend the current variable length extent */
		MT_FREE_AIB(&depthSave);
		
		/* extend the variable length ai extent */
		if ( bkxtn(pcontext, paictl->aiArea, (LONG)64) != (LONG)64 )
		{
			/* check if this is one and only ai extent */
			if( rlaiGetNextArea(pcontext, paictl->aiArea) == 0 )
			{
				/* no more extents, we could not try to switch */
				pdbshm->shutdn = DB_EXBAD;
				/* insufficient disk space */
				FATAL_MSGN_CALLBACK(pcontext, rlFTL001);
				dbExit(pcontext, DB_NICE_EXIT, DB_EXBAD);
			}
			
			/* yes we have more than one extent, complain to the caller */
			MT_RELOCK_AIB(depthSave);
			MSGN_CALLBACK(pcontext, rlMSG057,
                                      XTEXT(pdbcontext, pbkftbl->ft_qname));
			return(RLAI_CANT_EXTEND);
		}
		
		MT_RELOCK_AIB(depthSave);
		
		/* extended the file, update size information in ai control */
		paictl->aisize = rlaielen(pcontext, paictl->aiArea);
		
#ifdef AITRACE
		MSGD(pcontext, "%Lrlaiswitch: extended AI extent to ",
                     paictl->aisize);
#endif
		
    }
	
    /* success */
    return 0;
	
} /* rlaiswitch */


/* PROGRAM: rlaicls - close the after-image file.  Flush the current */
/*			output block if it is dirty		     */
DSMVOID
rlaicls(dsmContext_t *pcontext)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
	AICTL	*paictl = pdbcontext->paictl;
        bkftbl_t  *pbkftbl = (bkftbl_t *)NULL;
	LONG	 ret;

    if (paictl == NULL || paictl->aimode == AICLOSED) return;
    
    ret = bkGetAreaFtbl(pcontext, paictl->aiArea, &pbkftbl);
    
    if (paictl->aimode == AIOUTPUT)
    {
	paictl->aimode = AICLOSED;	 /* Prevent infinite loops */
	rlaifls (pcontext);
	rlaieof(pcontext);
	bmFlushMasterblock(pcontext);
    }
    else paictl->aimode = AICLOSED;	/* In case we're open for input */

    for( ; pbkftbl->ft_type ; pbkftbl++)
    {
	bkioClose(pcontext,
                  pdbcontext->pbkfdtbl->pbkiotbl[pbkftbl->ft_fdtblIndex],0);
    }
}

/* PROGRAM: rlaieof - save the current logical eof in mstrblock
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlaieof(dsmContext_t *pcontext)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    pdbcontext->pmstrblk->mb_aiwrtloc =
	pdbcontext->paictl->ailoc + pdbcontext->paictl->aiofst;
}

/* PROGRAM: rlainote - determine if a note belongs in the .ai file
 *
 * RETURNS  1 if yes, 0 if no
 */
int
rlainote(
	dsmContext_t	*pcontext,
	RLNOTE		*prlnote _UNUSED_)
{
    AICTL	*paictl = pcontext->pdbcontext->paictl;

    if (paictl==NULL || paictl->aimode != AIOUTPUT)
    {
	return 0;  /* AI is turned off or we are doing roll forward */
    }
    return (1);
}
