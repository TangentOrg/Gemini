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
#include "shmpub.h"
#include "dbcontext.h"
#include "bkpub.h"
#include "latpub.h"
#include "mstrblk.h"
#include "rlpub.h"
#include "rlprv.h"
#include "rlai.h"
#include "rltlprv.h"
#include "rldoundo.h"
#include "rlmsg.h"
#include "stmpub.h"
#include "stmprv.h"
#include "tmmgr.h"
#include "tmprv.h"
#include "tmtrntab.h"
#include "usrctl.h"
#include "utbpub.h"
#include "utspub.h"

/* The maximum number of tx table entries to write in a single memnote */
#ifdef MULTITRAN
#define MAX_ENTRIES_PER_NOTE   5      /* for testing purposes only */
#else
#define MAX_ENTRIES_PER_NOTE   512      
#endif

LOCALF DSMVOID
rlrctwt (dsmContext_t *pcontext);

LOCALF DSMVOID
unPackTxTable(dsmContext_t *pcontext, TEXT *pdata, COUNT dlen, COUNT tx_count );

/* PROGRAM: rlmemwt - write the transaction table and phy. backout flag
 *		      into an rlnote
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlmemwt(dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbpub = pdbcontext->pdbpub;
    MEMNOTE      note;
    TEXT        *ptx;
    TX          *ptrn;
    TEXT        *pdata;
    COUNT        dlen;
    TRANTAB     *ptran = pdbcontext->ptrantab;
    int          i,j,numToGo;
	
TRACE_CB(pcontext, "rlmemwt")

    INITNOTE(note, RL_INMEM, RL_PHY );

    note.rlpbkout = pdbcontext->prlctl->rlpbkout;
    note.lasttask = pdbcontext->pmstrblk->mb_lasttask;
    note.numLive  = pdbcontext->ptrantab->numlive;
    note.numMemnotes = (note.numLive / (UCOUNT)MAX_ENTRIES_PER_NOTE) + 1; 
    note.tmsize = sizeof (TX) - sizeof(txPlus_t);

    /* This is a funny note.  It describes physical state but since no action
    is taken and no database blocks depend on it so we'll write it with... */

    note.rlnote.rlArea   = 0;	/* bug 20000114-034 by dmeyer in v9.1b */
    note.rlnote.rldbk	 = 0;
    note.rlnote.rlupdctr = 0;
    
    note.spare[0] = 0;
    note.spare[1] = 0;
    
    /* put 0 in the trn id part of the note */
    note.rlnote.rlTrid = 0;

    if ( ptran->numlive > MAX_ENTRIES_PER_NOTE )
        note.numThisNote = MAX_ENTRIES_PER_NOTE;
    else
        note.numThisNote = ptran->numlive;
	
    dlen = note.numThisNote * note.tmsize;
    /* bug 93-01-21-035: use stRent instead of stkpush */

    pdata = (TEXT *)stRentd (pcontext, pdsmStoragePool, (unsigned)dlen);
    
    numToGo = ptran->numlive;
    ptrn = ptran->trn;
    for ( j = 0; j < (int)note.numMemnotes; j++)
    {
       note.noteSequence = j + 1;

       MT_LOCK_AIB();
       note.aiupdctr = pdbcontext->pmstrblk->mb_aictr;
       note.ainew    = pdbcontext->pmstrblk->mb_ai.ainew;

       if(pdbcontext->ptlctl)
       {
           note.tlWriteBlockNumber = pdbcontext->ptlctl->writeLocation;
           note.tlWriteOffset = pdbcontext->ptlctl->writeOffset;

       }
       else
       {
           note.tlWriteBlockNumber = 0;
           note.tlWriteOffset = 0;
       }
       
       if (pdbpub->qaictl)
           note.aiwrtloc =
               pdbcontext->paictl->ailoc + pdbcontext->paictl->aiofst;

       else note.aiwrtloc = 0; /* bug 20000114-034 by dmeyer in v9.1b */

       MT_UNLK_AIB();
       
       for(i=note.numThisNote, ptx = pdata;
	i > 0; ptrn++)
       {
          /* Only copy the transaction if it is in an active state */
	  if (ptrn->txstate && (ptrn->txstate != TM_TS_ALLOCATED) )
	  {
	     bufcop (ptx, (TEXT *)ptrn, note.tmsize);
	     ptx += note.tmsize;
	     i--;
	  }
       }

       /* Why not just use rlputnote here? */
       rlwrite(pcontext, (RLNOTE *)&note, dlen, pdata);
       if (rlainote (pcontext, (RLNOTE *)&note))
       {
	  /* write note to ai also. Notice that the ai information which is
	     recorded in the note will change when we do this. */
	  
	  MT_LOCK_AIB ();
	  
	  rlaiwrt (pcontext, (RLNOTE *)&note, dlen, pdata,
                   ++pdbcontext->pmstrblk->mb_aictr,
		   pcontext->pusrctl->uc_lstdpend);
	  
	  MT_UNLK_AIB ();
       }
       numToGo -= note.numThisNote;
       if( numToGo > MAX_ENTRIES_PER_NOTE )
	  note.numThisNote = MAX_ENTRIES_PER_NOTE;
       else
       {
	  note.numThisNote = numToGo;
	  dlen = numToGo * note.tmsize;
       }
       
    }
    /* bug 93-01-21-035: use stVacate instead of stkpop */
    stVacate (pcontext, pdsmStoragePool, pdata);
    
    /* Write out the ready to commit tx table if there any */
    if( pdbcontext->ptrcmtab != NULL )
	rlrctwt(pcontext);
	
}  /* end rlmemwt */


/* PROGRAM: rlGetMaxActiveTxs - Return the maximum number of active
 *                              txs that will fit in a checkpoint note.
 *                              This number is the maximum number of
 *                              simultaneous transactions that the
 *                              Progress DBMS can support
 *
 * RETURNS:
 */
ULONG
rlGetMaxActiveTxs(dsmContext_t *pcontext)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    ULONG   maxActiveTxs;

    /* We won't let the size of the checkpoint note(s) exceed 50% of the
       bi cluster size.                                            */
    if( pdbcontext->prlctl )
    {
        maxActiveTxs = (pdbcontext->prlctl->rlcap / 2) / sizeof(TX) ;
    }
    else
    {
       /* No rl subsystem for temp-table db so return 1  */
       maxActiveTxs = 1;
    }
    return maxActiveTxs;

}  /* end rlGetMaxActiveTxs */


/* PROGRAM: rlmemrd - reconstruct the in memory stuff from the note
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlmemrd (
	dsmContext_t	*pcontext,
	RLNOTE		*pInNote, 
	BKBUF		*pblk _UNUSED_, /* not used, no data block involved */
	COUNT		 dlen, 
	TEXT		*pdata )
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    MEMNOTE     *pnote = (MEMNOTE *)pInNote;
    TRANTAB	*ptran = pdbcontext->ptrantab;
    AICTL	*paictl = pdbcontext->paictl;
    LONG	aipos;

TRACE_CB(pcontext, "rlmemrd")

    /* reload the misc values */
    pdbcontext->prlctl->rlpbkout = pnote->rlpbkout;
    pdbcontext->pmstrblk->mb_lasttask = pnote->lasttask;
    if (paictl)
    {
	/* position the ai file to where we were before this note was
	   written to it */

	pdbcontext->pmstrblk->mb_aictr = pnote->aiupdctr;

        aipos = pnote->aiwrtloc;

	/* The ai update counter in the ai control structure was set */
	/* to the update counter in the header block of the busy     */
	/* extent by rlaiopn .                                       */
        /*  During the redo phase of warm start we don't want to seek */
	/* into an extent which is not the busy extent.              */
	if ( pnote->ainew != pdbcontext->pmstrblk->mb_ai.ainew )
	{
	    /* redo phase starts just before an extent switch       */
	    paictl->aiupdctr = (ULONG)0xFFFFFFFF;
	}

	if( pnote->aiupdctr >= paictl->aiupdctr )
	{
	    if ((aipos & ~(paictl->aiblkmask)) == 0)
	    {
		/* this is a kludge. when rlmemwt was called,     */
		/* if paictl->aiofst==paictl->aiblksize then rlmemrd */
		/* reconstructs it as 0.  That will be corrected by this */

		aipos = aipos - paictl->aiblksize;
		rlaiseek (pcontext, aipos);

                /* BUM - Assuming LONG aiblksize < 2^15 */
		paictl->aiofst = (COUNT)paictl->aiblksize;
	    }
	    else rlaiseek(pcontext, aipos);
	}
    }
    if(pdbcontext->ptlctl)
    {
        rltlSeek(pcontext,pnote->tlWriteBlockNumber,pnote->tlWriteOffset);
    }        
  
    /* Deduce the number of transactions live at the time of the note */
    if( pnote->noteSequence == 1 )
        ptran->numlive = pnote->numLive;	
    else if( (UCOUNT)ptran->numlive != pnote->numLive )
    {
       FATAL_MSGN_CALLBACK(pcontext, rlFTL043,
	    pnote->noteSequence, pnote->numLive, ptran->numlive);
    }
        
    /* Copy the tx table from the note into the transaction table     */
    unPackTxTable(pcontext, pdata, dlen, pnote->numThisNote );

}  /* end rlmemrd */


/* PROGRAM: unPackTxTable - Local routine to copy the transaction table
 *                          from a Memory or extent switch note 
 *			    into the transaction table.
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
unPackTxTable(
	dsmContext_t	*pcontext,
	TEXT		*pdata, 
	COUNT		 dlen, 
	COUNT		 tx_count)
{
    TX	*pTranEntry;
    TX	logTxEntry;
    COUNT txSize = sizeof(TX) - sizeof(txPlus_t);
    
    /* Unpack the mother */
    if( (COUNT)(dlen / txSize) != tx_count )
    {
       	FATAL_MSGN_CALLBACK(pcontext,rlFTL069,tx_count, dlen / txSize);
    }
    
    for(; tx_count; tx_count--, pdata += txSize)
    {
	/* copy to aligned storage so we can look at it to get the
	   transaction table entry */
	bufcop ((TEXT *)&logTxEntry, pdata, txSize);
	pTranEntry = tmEntry(pcontext, logTxEntry.transnum);

	bufcop((TEXT *)pTranEntry, (TEXT *)&logTxEntry, txSize );
    }

}  /* end unPackTxTable */


/* PROGRAM: rlmemchk - bi roll forward check INMEM contents
 *		       against in memory table
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlmemchk (
	dsmContext_t	*pcontext,
	RLNOTE		*pInNote, 
	BKBUF		*pblk, 
	COUNT		 dlen, 
	TEXT		*pdata )
{    
    rlVerifyTxTable(pcontext, pInNote, pblk, dlen, pdata, 1 );

}  /* end rlmemchk */


/* PROGRAM: rlVerifyTxTable - check in memory state primarily tx table
 *                            against contents of the checkpoint note
 *			      during redo phase of warm start or during
 *			      roll forward recovery.
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlVerifyTxTable (
	dsmContext_t	*pcontext,
	RLNOTE		*pInNote, 
	BKBUF		*pblk _UNUSED_,
	COUNT		 dlen, 
	TEXT		*pdata,
	int		 checkRlCounters   /* 1 - for database warm start 
                                       0 - for roll forward utility. */
	  )
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    MEMNOTE     *pnote = (MEMNOTE *)pInNote;
    COUNT	tx_count = 0;
    TRANTAB     *ptran = pdbcontext->ptrantab;
    MSTRBLK	*pmstr = pdbcontext->pmstrblk;
    TX	        logTxEntry;
    TX  	*pTranEntry;

TRACE_CB(pcontext, "rlmemchk")

    /* check the misc values */

    if (pdbcontext->prlctl->rlpbkout != pnote->rlpbkout)
    {
	FATAL_MSGN_CALLBACK(pcontext, rlFTL022,
                      pnote->rlpbkout, pdbcontext->prlctl->rlpbkout);
    }

    /* There exists the case in roll forward recovery where the mb_lasttask
     * is 1 less than the pnote->lasttask.  That is when as part of writting a
     * transaction begin note an inmem note must get written.  This will
     * cause the value in the inmem note to be 1 greater than the current
     * mb_lasttask value because the mb_lasttask value has not been updated
     * as part of the transaction begin do routine yet.
     */
    if (pmstr->mb_lasttask > pnote->lasttask)
    {
	FATAL_MSGN_CALLBACK(pcontext, rlFTL023,
                            pnote->lasttask, pmstr->mb_lasttask);
    }
/* bug 92-02-07-005 & 92-02-07-007 - ai checking code was moved to rlset.c */
/* and bug 92-03-05-003 also */

    /* verify the number of live transactions at the time of the note */
    if( pnote->numLive != (UCOUNT)ptran->numlive )
    {
       FATAL_MSGN_CALLBACK(pcontext, rlFTL026, pnote->numLive, ptran->numlive);
    }
    if ( dlen / pnote->tmsize != pnote->numThisNote )
    {
	FATAL_MSGN_CALLBACK(pcontext, rlFTL069,
	     pnote->numThisNote, dlen / pnote->tmsize);
    }

    if(pdbcontext->ptlctl)
    {
        /* The write locations may not match when rolling forward.
           The tl area could be larger or smaller -- but everything
           else should still match.
           checkRlCounters is zero when roll forward calls this
           function -- so I'm overloading the meaning of this
           parameter so roll forward won't abort with a
           tl write location mismatch */
        if (checkRlCounters &&
            (pdbcontext->ptlctl->writeLocation !=
             pnote->tlWriteBlockNumber) )
        {
            FATAL_MSGN_CALLBACK(pcontext, rlFTL070,
                          pdbcontext->ptlctl->writeLocation,
                          pnote->tlWriteBlockNumber);
        }
        
        if (pdbcontext->ptlctl->writeOffset !=
            pnote->tlWriteOffset)
        {
            FATAL_MSGN_CALLBACK(pcontext, rlFTL071,
                          pdbcontext->ptlctl->writeOffset,
                          pnote->tlWriteOffset);
        }
    }        
 
    /* Now verify the transaction tableunpack */
    for(tx_count = pnote->numThisNote;
        tx_count; tx_count--, pdata += pnote->tmsize)
    {
	/* copy to aligned storage */

	bufcop ((TEXT *)&logTxEntry, pdata, pnote->tmsize);

	pTranEntry = tmEntry(pcontext, logTxEntry.transnum);

        /* This stuff should match. If not, then something is wrong */

        if ((logTxEntry.transnum != pTranEntry->transnum) ||
            ((logTxEntry.rlcounter != pTranEntry->rlcounter) && 
	     checkRlCounters ))
	{
	    /* What we read in does not match what is currently
	       in the table */
#if SHMEMORY
            /* These diagnostics are conditionally compiled for
               shared memory to save static space on DOS/WINDOWS */
            MSGD_CALLBACK(pcontext, (TEXT *)"%Lnumlive %l tx_count %l",
                 (LONG)ptran->numlive, (LONG)tx_count);
            MSGD_CALLBACK(pcontext, 
                 (TEXT *)"%LlogTxEntry transnum %l rlcounter %l",
                 (LONG)logTxEntry.transnum,
                 (LONG)logTxEntry.rlcounter);
	    MSGD_CALLBACK(pcontext, 
                 (TEXT *)"%LpTranEntry transnum %l rlcounter %l",
                 (LONG)pTranEntry->transnum,
                 (LONG)pTranEntry->rlcounter);
#endif
	    FATAL_MSGN_CALLBACK(pcontext, rlFTL027);
	}
    }

}  /* end rlVerifyTxTable */


/* PROGRAM: rlaiextwt - write extent switch bi note
 *                      and write transaction table to ai file in addition
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlaiextwt (
	dsmContext_t	*pcontext,
	int		 redo) /* !0 when called during redo phase of warm start */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    AIEXTNOTE  note;
    TEXT       *pdata = NULL;
    COUNT      dlen = 0;
    TRANTAB    *ptran = pdbcontext->ptrantab;
       
TRACE_CB(pcontext, "rlaiextwt")

    INITNOTE(note, RL_AIEXT, RL_PHY );

    MT_LOCK_AIB ();
    note.rlpbkout = pdbcontext->prlctl->rlpbkout;
    note.lasttask = pdbcontext->pmstrblk->mb_lasttask;
    note.aiupdctr = pdbcontext->pmstrblk->mb_aictr;
    /* BUM - Assumong UCOUNT mb_aibusy_extent < 256 */
    note.extent = (TEXT)pdbcontext->pmstrblk->mb_aibusy_extent;
    note.ainew = pdbcontext->pmstrblk->mb_ai.ainew;

    MT_UNLK_AIB ();
    note.numlive  = ptran->numlive;
    if ( pdbcontext->ptrcmtab )
	note.numrc = pdbcontext->ptrcmtab->numrc;
    else
	note.numrc = 0;
     
    /* This is a funny note.  It describes physical state but since no action
    is taken and no database blocks depend on it so we'll write it with... */

    note.rlnote.rlArea = 0;	/* bug 20000114-034 by dmeyer in v9.1b */
    note.rlnote.rldbk	 = 0;
    note.rlnote.rlupdctr = 0;

    /* put 0 in the trn id part of the note */
    note.rlnote.rlTrid = 0;

    /* Only the bi file gets the transaction table               */
    if( !redo )
	rlwrite (pcontext, (RLNOTE *)&note, (COUNT) 0, (TEXT *)NULL);
    if (rlainote (pcontext, (RLNOTE *)&note))
    {
	/* write note to ai also. Notice that the ai information which is
	   recorded in the note will change when we do this. */

        MT_LOCK_AIB ();
 
        rlaiwrt (pcontext, (RLNOTE *)&note, dlen, pdata,
                 ++pdbcontext->pmstrblk->mb_aictr,
                pcontext->pusrctl->uc_lstdpend);
 
        MT_UNLK_AIB ();
    }

}  /* end rlaiextwt */


/* PROGRAM: rlaiextrd - reconstruct the in memory stuff from the note
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlaiextrd (
	dsmContext_t	*pcontext,
	RLNOTE		*pInNote, 
	BKBUF		*pblk _UNUSED_,
	COUNT		 dlen _UNUSED_,
	TEXT		*pdata _UNUSED_)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    AIEXTNOTE *pnote = (AIEXTNOTE *)pInNote;
    TRANTAB   *ptran = pdbcontext->ptrantab;
    
    pdbcontext->pmstrblk->mb_chgd = (TEXT)1;
    if ( ptran->numlive != pnote->numlive )
    {
	/* rlaiextrd: Transaction table mismatch ptran: %i note %i */
        FATAL_MSGN_CALLBACK(pcontext, rlFTL065, ptran->numlive, pnote->numlive);
    }

    return;	    

}  /* end rlaiextrd */


/* PROGRAM: rlrctwt - Write the ready to commit table into the bi file
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
rlrctwt (dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    RCTABNOTE   note;
    TEXT        *ptx;
    TXRC        *ptrnrc;
    TEXT        *pdata;
    COUNT        dlen;
    TRANTABRC   *ptranrc = (TRANTABRC *)pdbcontext->ptrcmtab;
    int          i, j, numToGo;
       
    TRACE_CB(pcontext, "rlrctwt")

    INITNOTE(note, RL_RCTAB, RL_PHY );
    
    if (ptranrc->numrc == 0)
        return;

    note.numrc      = ptranrc->numrc;
    note.numRcNotes = (note.numrc / MAX_ENTRIES_PER_NOTE) + 1;

    /* This is a funny note.  It describes physical state but since no action
    is taken and no database blocks depend on it so we'll write it with... */

    note.rlnote.rlArea = 0;	/* bug 20000114-034 by dmeyer in v9.1b */
    note.rlnote.rldbk    = 0;
    note.rlnote.rlupdctr = 0;

    /* put 0 in the trn Id part of the note */
    note.rlnote.rlTrid = 0;

    if ( ptranrc->numrc > MAX_ENTRIES_PER_NOTE )
        note.numThisNote = MAX_ENTRIES_PER_NOTE;
    else
        note.numThisNote = ptranrc->numrc;

    dlen = ptranrc->numrc * sizeof(TXRC);

    /* bug 93-01-21-035: use stRent instead of stkpush */
    pdata = (TEXT *)stRentd (pcontext, pdsmStoragePool, (unsigned)dlen);

    numToGo = ptranrc->numrc;

    for ( j = 0; j < (int)note.numRcNotes; j++)
    {
        note.noteSequence = j + 1;

        for(i=note.numThisNote, ptx = pdata, ptrnrc = ptranrc->trnrc;
            i > 0; ptrnrc++)
        {
            if (ptrnrc->trnum != 0L)
            {
                bufcop( ptx, (TEXT *)ptrnrc, sizeof(TXRC));
                ptx += sizeof(TXRC);
                i--;
            }
        }

        rlwrite (pcontext, (RLNOTE *)&note, dlen, pdata);

        if (rlainote (pcontext, (RLNOTE *)&note))
        {
            /* write note to ai also. Notice that the ai information which is
               recorded in the note will change when we do this. */

            MT_LOCK_AIB ();
 
            rlaiwrt (pcontext, (RLNOTE *)&note, dlen, pdata,
                     ++pdbcontext->pmstrblk->mb_aictr,
                     pcontext->pusrctl->uc_lstdpend);
 
            MT_UNLK_AIB ();
        }
        numToGo -= note.numThisNote;
 
        if( numToGo > MAX_ENTRIES_PER_NOTE )
           note.numThisNote = MAX_ENTRIES_PER_NOTE;
        else
        {
           note.numThisNote = numToGo;
           dlen = numToGo * sizeof(TXRC);
        }
    }

    /* bug 93-01-21-035: use stVacate instead of stkpop */
    stVacate (pcontext, pdsmStoragePool, pdata);

    return;

}  /* end rlrctwt */


/* PROGRAM: rlrctrd - Loads the ready to commit tx table
 *                    from the note
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlrctrd (
        dsmContext_t *pcontext,
        RLNOTE       *pInNote, 
        BKBUF        *pblk _UNUSED_, /* not used, no data block is involved */
        COUNT         dlen _UNUSED_,
        TEXT         *pdata )
{
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    TRANTABRC     *ptranrc = pdbcontext->ptrcmtab;
    COUNT          tx_count;
    LONG           tx_num;
    RCTABNOTE     *pnote = (RCTABNOTE *)pInNote;

LOCAL GTBOOL   rcTableLoaded = 0;  /* table fully loaded when set
                                   * It is assumed that reading of the ready
                                   * to commit note is single threaded.
                                   */
 
    if ( ptranrc == NULL )
    {
        /* Allocate a ready to commit table                           */
        /* We may need to do it now because is possible to turn off
           2phase commit but have RL_RCOMM notes processed during the
           redo phase of warm start and also when rolling forward an
           after image file                                           */
        tmrcmalc(pcontext);
        ptranrc = pdbcontext->ptrcmtab;
    }
    
    if ( rcTableLoaded )
    {
        /* Since the table has been fully loaded once, any additional
         * rc entries would be added by forward processing.  We will there-
         * fore verify that the information in the current rc note matches
         * what was added to the rc table by forward procesing.
         */

        /* Then check that we agree with whats in this note */
        if ( pnote->numrc != ptranrc->numrc )
            FATAL_MSGN_CALLBACK(pcontext, rlFTL032,
                                pnote->numrc, ptranrc->numrc);

        for( tx_count = pnote->numThisNote;
             tx_count;
             tx_count--, pdata += sizeof(TXRC))
        {
            bufcop((TEXT *)&tx_num, pdata, sizeof(LONG) );
            if ( bufcmp ( (TEXT*)&ptranrc->trnrc[tmSlot(pcontext, tx_num)],
                 (TEXT*)pdata, sizeof(TXRC) ) )
            {
                FATAL_MSGN_CALLBACK(pcontext, rlFTL033);
            }
        }
    }
    else
    {
        ptranrc->numrc = pnote->numrc;
        tx_count     = pnote->numThisNote;

        /* Now unpack the mother */
        for(; tx_count; tx_count--, pdata += sizeof(TXRC))
        {
            bufcop((TEXT *)&tx_num, pdata, sizeof(LONG) );
            bufcop((TEXT *)&ptranrc->trnrc[tmSlot(pcontext, tx_num)],
                   pdata, sizeof(TXRC) );
        }
        if (pnote->numRcNotes == pnote->noteSequence)
            rcTableLoaded = 1;  /* table load complete */
    }

    return;

}  /* end rlrctrd */

