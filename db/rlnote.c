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
#include "pscsys.h"
#include "bkpub.h"
#include "dbcode.h"
#include "dbcontext.h"
#include "dbpub.h"  /* for DB_EXBAD */
#include "latpub.h"
#include "rlmsg.h"
#include "rldoundo.h"
#include "rlprv.h"
#include "rlpub.h"
#include "rltlpub.h"
#include "tmmgr.h"
#include "tmprv.h"
#include "tmpub.h"
#include "tmtrntab.h"  /* needed to get trnmask */
#include "usrctl.h"
#include "utspub.h"
#include "uttpub.h"

#if OPSYS==WIN32API
#include <time.h>
#endif

/* Initialize the do/undo routine dispatch table */

#define RLDEF(n, d, u, disp)	{n, d, u, disp, 0}

LOCAL char  rl_NULL[] = "RL_NULL";
LOCAL char  rl_IXMSTR[] = "RL_IXMSTR";
LOCAL char  rl_IXINS[] = "RL_IXINS";
LOCAL char  rl_IXREM[] = "RL_IXREM";
LOCAL char  rl_IXSP1[] = "RL_IXSP1";
LOCAL char  rl_IXSP2[] = "RL_IXSP2";
LOCAL char  rl_IXDBLK[] = "RL_IXDBLK";
LOCAL char  rl_IXIBLK[] = "RL_IXIBLK";
LOCAL char  rl_BKREPL[] = "RL_BKREPL";
LOCAL char  rl_BKFRM[] = "RL_BKFRM";
LOCAL char  rl_BKFRB[] = "RL_BKFRB";
LOCAL char  rl_BKFAM[] = "RL_BKFAM";
LOCAL char  rl_BKFAB[] = "RL_BKFAB";
LOCAL char  rl_INMEM[] = "RL_INMEM";
LOCAL char  rl_TBGN[] = "RL_TBGN";
LOCAL char  rl_TEND[] = "RL_TEND";
LOCAL char  rl_PUNDO[] = "RL_PUNDO";
LOCAL char  rl_PBKOUT[] = "RL_PBKOUT";
LOCAL char  rl_PBKEND[] = "RL_PBKEND";
LOCAL char  rl_LRA[] = "RL_LRA";
LOCAL char  rl_IXGEN[] = "RL_IXGEN";
LOCAL char  rl_IXDEL[] = "RL_IXDEL";
LOCAL char  rl_IXFILC[] = "RL_IXFILC";
LOCAL char  rl_IXKIL[] = "RL_IXKIL";
LOCAL char  rl_IXUKL[] = "RL_IXUKL";
LOCAL char  rl_RMCR[] = "RL_RMCR";
LOCAL char  rl_RMDEL[] = "RL_RMDEL";
LOCAL char  rl_JMP[] = "RL_JMP";
LOCAL char  rl_XTDB[] = "RL_XTDB";
LOCAL char  rl_FILC[] = "RL_FILC";
LOCAL char  rl_LSTMOD[] = "RL_LSTMOD";
LOCAL char  rl_RMNXTF[] = "RL_RMNXTF";
LOCAL char  rl_BK2EM[] = "RL_BK2EM";
LOCAL char  rl_BK2EB[] = "RL_BK2EB";
LOCAL char  rl_BKMBOT[] = "RL_BKMBOT";
LOCAL char  rl_BKBBOT[] = "RL_BKBBOT";
LOCAL char  rl_RMCHG[] = "RL_RMCHG";
LOCAL char  rl_RCOMM[] = "RL_RCOMM";
LOCAL char  rl_TABRT[] = "RL_TABRT";
LOCAL char  rl_CTEND[] = "RL_CTEND";
LOCAL char  rl_RTEND[] = "RL_RTEND";
LOCAL char  rl_CXINS[] = "RL_CXINS";
LOCAL char  rl_CXREM[] = "RL_CXREM";
LOCAL char  rl_CXSP1[] = "RL_CXSP1";
LOCAL char  rl_CXSP2[] = "RL_CXSP2";
LOCAL char  rl_CXROOT[] = "RL_CXROOT";
LOCAL char  rl_AIEXT[] = "RL_AIEXT";
LOCAL char  rl_SEASN[] = "RL_SEASN";
LOCAL char  rl_SEINC[] = "RL_SEINC";
LOCAL char  rl_SESET[] = "RL_SESET";
LOCAL char  rl_SEADD[] = "RL_SEADD";
LOCAL char  rl_SEDEL[] = "RL_SEDEL";
LOCAL char  rl_SEUPD[] = "RL_SEUPD";
LOCAL char  rl_RCTAB[] = "RL_RCTAB";
LOCAL char  rl_BKFORMAT[] = "RL_BKFORMAT";
LOCAL char  rl_BKMAKE[] = "RL_BKMAKE";
LOCAL char  rl_BKHWM[] = "RL_BKHWM";
LOCAL char  rl_BKAREAADD[] = "RL_BKAREA_ADD";
LOCAL char  rl_EXTENTCREATE[] = "RL_BKEXTENT_CREATE";
LOCAL char  rl_WRITE_AREA_BLOCK[] = "RL_BKWRITE_AREA_BLOCK";
LOCAL char  rl_WRITE_OBJECT_BLOCK[] = "RL_BKWRITE_OBJECT_BLOCK";
LOCAL char  rl_TMSAVE[] = "RL_TMSAVE";
LOCAL char  rl_OMCREATE[] = "RL_OMCREATE";
LOCAL char  rl_OMUPDATE[] = "RL_OMUPDATE";
LOCAL char  rl_BKAREADELETE[] = "RL_BKAREA_DELETE";
LOCAL char  rl_EXTENTDELETE[] = "RL_BKEXTENT_DELETE";
LOCAL char  rl_IDXBLOCKCOPY[] = "RL_IDXBLOCK_COPY";
LOCAL char  rl_IDXBLOCKUNCOPY[] = "RL_IDXBLOCK_UNCOPY";
LOCAL char  rl_CXCOMPACT_LEFT[] = "RL_CXCOMPACT_LEFT";
LOCAL char  rl_CXCOMPACT_RIGHT[] = "RL_CXCOMPACT_RIGHT";
LOCAL char  rl_CXTEMPROOT[] = "RL_CXTEMPROOT";
LOCAL char  rl_TL_ON[] = "RL_TL_ON";
LOCAL char  rl_ROW_COUNT_UPDATE[] = "RL_ROW_COUNT_UPDATE";
LOCAL char  rl_AUTO_INCREMENT[] = "RL_AUTO_INCREMENT";
LOCAL char  rl_AUTO_INCREMENT_SET[] = "RL_AUTO_INCREMENT_SET";

GLOBAL struct rldisp rldisptab[] =
{
  RLDEF(rl_NULL, rlnull, rlnull, rlnull ),
  RLDEF(rl_IXMSTR, cxDoMasterInit, cxClearBlock, rlnull),
  RLDEF(rl_IXINS, cxInvalid, cxInvalid, rlnull),
  RLDEF(rl_IXREM, cxInvalid, cxInvalid, rlnull),
  RLDEF(rl_IXSP1, cxInvalid, cxInvalid, rlnull),
  RLDEF(rl_IXSP2, cxInvalid, cxInvalid, rlnull),
  RLDEF(rl_IXDBLK, cxClearBlock, cxUndoClearBlock, rlnull),
  RLDEF(rl_IXIBLK, cxUndoClearBlock, cxClearBlock, rlnull),
  RLDEF(rl_BKREPL, bkrpldo, bkrplundo, rlnull),
  RLDEF(rl_BKFRM, bkfrmulk, bkfrmlnk, rlnull),
  RLDEF(rl_BKFRB, bkfrbulk, bkfrbl2, rlnull),
  RLDEF(rl_BKFAM, bkfrmlnk, bkfrmulk, rlnull),
  RLDEF(rl_BKFAB, bkfrbl1, bkfrbulk, rlnull),
  RLDEF(rl_INMEM, rlmemchk, rlnull, rlnull),
  RLDEF(rl_TBGN, tmadd, tmrem, tmdisp),
  RLDEF(rl_TEND, tmrem, tmadd, tmdisp),

/* backout notes */
  RLDEF(rl_PUNDO, rlnull, rlnull , rlnull),
  RLDEF(rl_PBKOUT, rlnull, rlnull , rlnull),
  RLDEF(rl_PBKEND, rlnull, rlnull, rlnull),
  RLDEF(rl_LRA, rlnull, rlnull , rlnull),

/* #define the purely logical note codes */
  RLDEF(rl_IXGEN, rlnull, rlnull , rlnull),
  RLDEF(rl_IXDEL, rlnull, rlnull , rlnull),
  RLDEF(rl_IXFILC, rlnull, rlnull , rlnull),
  RLDEF(rl_IXKIL, rlnull, rlnull , rlnull),
  RLDEF(rl_IXUKL, rlnull,rlnull , rlnull),

/* notes for rm */
  RLDEF(rl_RMCR, rmDoInsert, rmDoDelete , rlnull),
  RLDEF(rl_RMDEL, rmDoDelete, rmDoInsert , rlnull),

/* jump note, is purly logical */
  RLDEF(rl_JMP,    rlnull,	rlnull , rlnull),

  RLDEF(rl_XTDB,  bkdoxtn,	rlnull , rlnull),
  RLDEF(rl_FILC,   rlnull,	rlnull , rlnull),
  RLDEF(rl_LSTMOD, dolstmod,	rlnull, rlnull),
  RLDEF((char *)"",      rlnull,	rlnull, rlnull),  /* unused entry, avail */

  RLDEF(rl_RMNXTF, rmDoNextFrag, rmUndoNextFrag, rlnull),

  /* RM free chain, top block to bottom of chain */
  RLDEF(rl_BK2EM, bk2emdo, bk2emundo, rlnull),
  RLDEF(rl_BK2EB, bk2ebdo, bk2ebundo, rlnull),

  /* RM free chain, add block to bottom of chain */
  RLDEF(rl_BKMBOT, bkbotmdo, bkbotmundo, rlnull),
  RLDEF(rl_BKBBOT, bkbotdo, bkbotundo, rlnull),

  /* change a record by diff note */
  RLDEF(rl_RMCHG, rmDoChange, rmUndoChange, rlnull),

  /* Two-phase-commit notes */
  RLDEF(rl_RCOMM, trcmadd, rlnull, rlnull),
  RLDEF(rl_TABRT, tmrmrcm, rlnull, rlnull),
  RLDEF(rl_CTEND, tmrem, tmadd, tmdisp),
  RLDEF(rl_RTEND, tmrem, tmadd, tmdisp),

  /* Compressed index notes */
  RLDEF(rl_CXINS, cxDoInsert, cxDoDelete, rlnull),
  RLDEF(rl_CXREM, cxDoDelete, cxDoInsert, rlnull),
  RLDEF(rl_CXSP1, cxDoStartSplit, cxUndoStartSplit, rlnull),
  RLDEF(rl_CXSP2, cxDoEndSplit, cxClearBlock, rlnull),
  RLDEF(rl_CXROOT, cxInvalid, cxInvalid, rlnull),

  /* Ai extent switch note    */
  RLDEF(rl_AIEXT, rlnull, rlnull , rlnull),

  /* Sequences */
  RLDEF(rl_SEASN, seqDoAssign, seqUndoAssign, rlnull),
  RLDEF(rl_SEINC, seqDoIncrement, rlnull, rlnull),
  RLDEF(rl_SESET, seqDoSetValue, seqUndoSetValue, rlnull),
  RLDEF(rl_SEADD, seqDoAdd, seqUndoAdd, rlnull),
  RLDEF(rl_SEDEL, seqDoDelete, seqUndoDelete, rlnull),
  RLDEF(rl_SEUPD, seqDoUpdate, seqUndoUpdate, rlnull),

  /* Note for ready to commit table          */
  RLDEF(rl_RCTAB, rlrctrd, rlnull, rlnull),

  /* Notes to allocate a block above the hiwater mark */
  RLDEF(rl_BKFORMAT,bkFormatBlockDo,bkFormatBlockUndo, rlnull),
  RLDEF(rl_BKMAKE,bkMakeBlockDo,rlnull, rlnull),
  RLDEF(rl_BKHWM,bkHwmDo,rlnull, rlnull),

/* Notes for storage areas and extents                */
  RLDEF(rl_BKAREAADD,bkAreaAddDo,bkAreaRemoveDo, rlnull),
  RLDEF(rl_EXTENTCREATE, bkExtentCreateDo, rlnull, rlnull),
  RLDEF(rl_WRITE_AREA_BLOCK, bkWriteAreaBlockDo, rlnull, rlnull),
  RLDEF(rl_WRITE_OBJECT_BLOCK, bkWriteObjectBlockDo, rlnull, rlnull),

  /* Note for Marking and Unsaving savepoints */
  RLDEF(rl_TMSAVE, rlnull, rlnull , rlnull),

  /* Notes for creating and updating entries in the object cache */
  RLDEF(rl_OMCREATE, rlnull, rlnull , rlnull),
  RLDEF(rl_OMUPDATE, rlnull, rlnull , rlnull),

/* More notes for storage areas and extents                */
  RLDEF(rl_BKAREADELETE,bkAreaRemoveDo,bkAreaAddDo, rlnull),
  RLDEF(rl_EXTENTDELETE, bkExtentRemoveDo, rlnull, rlnull),

/* notes for copy and uncopy of index blocks */
  RLDEF(rl_IDXBLOCKCOPY, cxUndoClearBlock, cxClearBlock, rlnull),
  RLDEF(rl_IDXBLOCKUNCOPY, cxClearBlock, cxUndoClearBlock, rlnull),

  /* Notes for compacting index entries in index blocks */
  RLDEF(rl_CXCOMPACT_LEFT, cxDoCompactLeft, cxUndoCompactLeft, rlnull),
  RLDEF(rl_CXCOMPACT_RIGHT, cxDoCompactRight, cxUndoCompactRight, rlnull),

   /* temporary indices created by dbmgr */
  RLDEF(rl_CXTEMPROOT, rlnull, rlnull , rlnull),

  /* turn on 2pc                      */
  RLDEF(rl_TL_ON, rltlOnDo, rlnull, rlnull),
  
  RLDEF(rl_ROW_COUNT_UPDATE,bkRowCountUpdateDo,bkRowCountUpdateUndo,rlnull),
  RLDEF(rl_AUTO_INCREMENT,dbTableAutoIncrementDo,dbTableAutoIncrementUndo,rlnull),
  RLDEF(rl_AUTO_INCREMENT_SET,dbTableAutoIncrementSetDo,rlnull,rlnull),
};

GLOBAL struct rldisp *prldisp = rldisptab;
GLOBAL int rldispsz = sizeof(rldisptab)/sizeof(struct rldisp);


LOCALF  DSMVOID  wrtjmp(dsmContext_t *pcontext);

LOCALF LONG64
rlputnote (dsmContext_t *pcontext,
	   RLNOTE *pnote,       /* rlnote describes db change		*/ 
	   ULONG  area,         /* Storage area containing the block    */
	   BKBUF  *pblk,        /* ptr to affected block		*/
	   COUNT  dlen,         /* length of optional data or 0		*/
	   TEXT   *pdata);      /* optional variable len data or PNULL	*/

LOCALF
int
rlTXEgetLock(dsmContext_t *pcontext,
             RLCTL *prlctl, usrctl_t *pself, LONG lockMode);

LOCALF
int
grantUpgradeRequests(dsmContext_t *pcontext, RLCTL *prlctl);

LOCALF
DSMVOID
queForTXE(dsmContext_t *pcontext, RLCTL *prlctl,
          usrctl_t *pself, int lockType);
LOCALF
DSMVOID
rlTXEgrantLocks(dsmContext_t *pcontext, RLCTL *prlctl);

LOCALF DSMVOID rlLogAndDoIt(dsmContext_t *pcontext, RLNOTE *pnote,
                          bmBufHandle_t bufHandle, COUNT dlen, TEXT *pdata);


/* RLNOTE - recovery log routines */


/* PROGRAM rlputnote - write note  to all active logs
 *
 * NOTE: The block must have an exclusive lock on it
 *
 * RETURNS: bi dependency counter (sequence number) for the note
 */
LOCALF LONG64
rlputnote (
	dsmContext_t *pcontext,
	RLNOTE *pnote,       /* rlnote describes db change		*/ 
	   ULONG  area,         /* Storage area containing the block    */
	   BKBUF  *pblk,        /* ptr to affected block		*/
	   COUNT  dlen,         /* length of optional data or 0		*/
	   TEXT   *pdata)       /* optional variable len data or PNULL	*/
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    dbshm_t         *pdbpub = pdbcontext->pdbpub;
    usrctl_t        *pusr = pcontext->pusrctl;
    RLCTL           *prl = pdbcontext->prlctl;
    LONG64          bidepend = 0;
    LONG            retCode;

TRACE2_CB(pcontext, "rlphynt code=%t dlen=%d", pnote->rlcode, dlen)


    if (pblk == NULL)
    {
	/* no database block for this note */
    
	pnote->rlupdctr = 0;
/*----------------------------------------------------------------------*/
/*	    This line was originally released but backed out as		*/
/*	    there is a path on some platforms (through cxRemove) that	*/
/*	    will initialize rlArea itself to a used value.		*/
/*----------------------------------------------------------------------*/
/*	pnote->rlArea	= 0; */ /* bug 20000114-034 by dmeyer in v9.1b	*/
/*----------------------------------------------------------------------*/
	pnote->rldbk    = 0;
    }
    else
    {
	/* Increment the block's update counter */
         
	slng64 ((TEXT *)&pnote->rlupdctr, pblk->BKUPDCTR + 1);
	slng ((TEXT *)&pnote->rlArea, area);
	sdbkey ((TEXT *)&pnote->rldbk,	pblk->BKDBKEY);
     }
    /* Logging is not on so just return.            */
    if (!prl) return (bidepend);
    /* Logging is not turned for this user as in a table repair
       operation.   */
    if(pcontext->noLogging) return(bidepend);

    /* Lock mtx so notes are written to bi and ai in the same order */
    
    MT_LOCK_MTX ();

    if (pdbpub->fullinteg || (xct((TEXT *)&pnote->rlflgtrn) & RL_LOGBI))
    {
    
	if (!((pnote->rlcode == RL_RTEND) || (pnote->rlcode == RL_TABRT)))
	{
	    /* set the transaction table slot number in the note */
	    slng((TEXT *)&pnote->rlTrid, pusr->uc_task);
	}
    
	MT_LOCK_BIB ();
    
        sct((TEXT *)&pnote->rlflgtrn,
            (COUNT)(xct((TEXT *)&pnote->rlflgtrn) | prl->rlmtflag));

	prl->rlmtflag = 0;
    
	/* Write the note out to the recovery log */
    
        /* if the prev note is in a differnt blk, write a jump note*/
        if ( (pusr->uc_lastBlock != 0 && pusr->uc_lastOffset != 0)
            &&  (pusr->uc_lastBlock != prl->rlnxtBlock) )
        {
	    wrtjmp(pcontext); 
        }
    
	retCode = rlwrite (pcontext, pnote, dlen, pdata);
	bidepend = prl->rldepend;
    
	MT_UNLK_BIB ();
    
	pcontext->pusrctl->uc_lstdpend = bidepend;
    }

    if (rlainote(pcontext, pnote))
    {
	/* This note needs to go into the ai log */

        MT_LOCK_AIB ();

        rlaiwrt (pcontext, pnote, dlen, pdata,
                 ++pdbcontext->pmstrblk->mb_aictr, bidepend);

        MT_UNLK_AIB ();

    }
    if ((pnote->rlcode == RL_CTEND) &&
        ((TMNOTE *)pnote)->accrej == TMACC)
    {
        /* 2-PC coordinator's commit note - just need the transaction id */
        rltlSpool(pcontext,((TMNOTE *)pnote)->xaction);
    }
    
    MT_UNLK_MTX ();

    return (bidepend);

}  /* end rlputnote */


/* PROGRAM: rlWriteTxBegin - Write out the delayed transaction begin note.
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlWriteTxBegin(
	dsmContext_t	*pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    usrctl_t    *pusr       = pcontext->pusrctl;
    COUNT       lenUsrName;
    TEXT        *pusrName;
    TMNOTE       note;

      /*----------------------*/
      /* Initialize the note. */
      /*----------------------*/

    INITNOTE( note, RL_TBGN, RL_PHY | RL_LOGBI );

      /*------------------------------------------------*/
      /* Bug 20000114-034:				*/
      /*						*/
      /*    Initialize the fillers in the TMNOTE.	*/
      /*------------------------------------------------*/

    INIT_S_TMNOTE_FILLERS( note );
    note.accrej = 0;

      /* who we are */

    note.usrnum = pusr->uc_usrnum;
    
    if (pdbcontext->dbcode == PROCODET)
    {
        /* no need to get the time for temp-tables.  Infact we have seen
         * that the time() call is extremely slow on WIN95 when using the
         * WIN32API calls.
         */
        note.timestmp = 0;
    }
    else
    {
        /* the time the transaction began */
        time( &note.timestmp );
    }

    pusrName = XTEXT( pdbcontext, pusr->uc_qusrnam );

    if (pusrName)
        lenUsrName = (COUNT)stlen( pusrName );
    else
        lenUsrName = 0;

    /* fill in the transaction id and bi sequence number */
    note.xaction = pusr->uc_task;

    MT_LOCK_MTX();

    if (pdbcontext->prlctl)
    {
        /* We do not always have a log. It may be turned off, or this
         * may be a Progress temp table db.
         * Mark where we are in the bi file so later we can tell what
         * clusters can be reused. Have to keep mtx lock until the note
         * is spooled so we have the correct value
         */
        note.rlcounter = rlclcurr(pcontext);
    }

    else
        note.rlcounter = 0;

    note.lasttask = pdbcontext->pmstrblk->mb_lasttask;

    rlLogAndDoIt( pcontext, &note.rlnote, 0, lenUsrName, pusrName );

    if (pcontext->pdbcontext->accessEnv == DSM_SQL_ENGINE)
    {
        dsmTxnSave_t savePoint = 1;
        tmMarkSavePoint(pcontext, &savePoint);
    }

    MT_UNLK_MTX();

}  /* end rlWriteTxBegin */


/* PROGRAM: rlLogAndDo - log and perform action for requested note.
 *
 * If the requestor's transaction is note active, create a TBGN note
 * and make it active.  Then
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlLogAndDo(
	dsmContext_t	*pcontext,
	RLNOTE		*pnote,	/* rlnote describes db change		*/
	bmBufHandle_t	 bufHandle,/* handle to buffer containing the 
				   database block to be changed.        */
	COUNT		 dlen,	/* length of optional data or 0		*/
	TEXT		*pdata)	/* optional variable len data or PNULL	*/
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    usrctl_t    *pusr       = pcontext->pusrctl;
    TX          *pTranEntry;

TRACE2_CB(pcontext, "rlLogAndDoIt code=%t dlen=%d", pnote->rlcode, dlen)
 
    if (pusr->uc_task && pdbcontext->ptrantab)
    {
        pTranEntry = tmEntry(pcontext, pusr->uc_task);
 
        if ( pTranEntry->txstate == TM_TS_ALLOCATED )
            rlWriteTxBegin(pcontext);
    }

    /* Log and perform the requested action */
    rlLogAndDoIt (pcontext, pnote, bufHandle, dlen, pdata);

}  /* end rlLogAndDo */


/* PROGRAM rlLogAndDoIt - write physical note and perform its action
 *
 * Should only be called from rlLogAndDo!
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
rlLogAndDoIt ( 
	dsmContext_t	*pcontext,
	RLNOTE		*pnote,	/* rlnote describes db change		*/
	bmBufHandle_t	 bufHandle,/* handle to buffer containing the 
				   database block to be changed.        */
	COUNT		 dlen,	/* length of optional data or 0		*/
	TEXT		*pdata)	/* optional variable len data or PNULL	*/
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbpub = pdbcontext->pdbpub;
    LONG64	bidepend;
    LONG        timeStamp;
    ULONG       area = 0;
    BKBUF       *pblk;          /* ptr to affected block		*/
    int         mtxlocked;

TRACE2_CB(pcontext, "rlLogAndDoIt code=%t dlen=%d", pnote->rlcode, dlen)

#ifdef BITRACERW
MSGD_CALLBACK(pcontext, 
    (TEXT*)"%LrlLogAndDoIt: user=%d code=%t dlen=%d", 
     pcontext->pusrctl->uc_usrnum, pnote->rlcode, dlen);
#endif
    /* NOTE: The block must have an exclusive lock on it */
    if ( bufHandle )
    {
	pblk = bmGetBlockPointer(pcontext,bufHandle);
	area = bmGetArea(pcontext, bufHandle);

        if (pblk->bk_hdr.bk_dbkey != BLK2DBKEY(BKGET_AREARECBITS(area)))
        {
            /* check to see if the area has been marked as opened yet */
            bkModifyArea(pcontext, area);
        }
    }
    else
	pblk = NULL;

    /* Write the note to whatever logs are active */


    bidepend = rlputnote (pcontext, pnote, area, pblk, dlen, pdata);

    /* Do the database action */

    RLDO (pcontext, pnote, pblk, dlen, pdata);
 
    if (pblk != NULL)
    {
        /* flag the database block as modified */
 
        pblk->BKUPDCTR++;
        bmMarkModified(pcontext, bufHandle, bidepend);
    }
    /* See if the database changed flag is clear -- set it if it is.
       This flag forces backups before enabling after imaging but
       we don't want changes to the control area to set this flag. */
    if ( !pdbpub->didchange && (area != DSMAREA_CONTROL) )
    {
        /* if we are holding the MTX latch free it, then relock after */
        /* the tmlstmod */
 
        mtxlocked = 0;
        if ( MTHOLDING(MTL_MTX) )
        {
            mtxlocked = 1;
            MT_UNLK_MTX();
        }

	time (&timeStamp);
	tmlstmod(pcontext, timeStamp);

        if (mtxlocked)
        {
            MT_LOCK_MTX();
        }

    }

}  /* end rlLogAndDoIt */



/* PROGRAM: wrtjmp - If this note is not in the same block as the prev
 *		     one for the same xaction, write a jump note
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
wrtjmp(dsmContext_t *pcontext)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    RLMISC	    jmpnote;
    usrctl_t	    *pusr = pcontext->pusrctl;

#ifdef BITRACE
MSGD_CALLBACK(pcontext, 
  (TEXT*)"%Lwrtjmp1: uc_lstblk=%l uc_lstoff %l, rlnxtblk=%l rlnxtoff %l",
	 pusr->uc_lastBlock, pusr->uc_lastOffset, pdbcontext->prlctl->rlnxtBlock, 
         pdbcontext->prlctl->rlnxtOffset);
#endif

TRACE2_CB(pcontext, 
         "writing jump note uc_lstblk=%l, rlnxtblk=%l",
	 pusr->uc_lastBlock, pdbcontext->prlctl->rlnxtBlock)

    jmpnote.rlnote.rllen	= sizeof(jmpnote);
    jmpnote.rlnote.rlcode	= RL_JMP;
    sct((TEXT *)&jmpnote.rlnote.rlflgtrn,RL_LOGBI);
    slng((TEXT *)&jmpnote.rlnote.rlTrid, pusr->uc_task);
    jmpnote.rlnote.rldbk	= 0;
    jmpnote.rlnote.rlArea	= 0;	/* bug 20000114-034 by dmeyer in v9.1b */
    jmpnote.rlnote.rlupdctr	= 0;
    slng((TEXT *)&jmpnote.rlvalueBlock, pusr->uc_lastBlock);
    slng((TEXT *)&jmpnote.rlvalueOffset, pusr->uc_lastOffset);

    rlwrite(pcontext, &jmpnote.rlnote, (COUNT)0, PNULL);
    if (rlainote(pcontext, (RLNOTE *)(&jmpnote)))
    {
        MT_UNLK_BIB ();
        MT_LOCK_AIB ();

        rlaiwrt(pcontext, (RLNOTE *)&jmpnote, (COUNT)0, PNULL, 
                ++pdbcontext->pmstrblk->mb_aictr,
                pusr->uc_lstdpend);

	    MT_UNLK_AIB ();
            MT_LOCK_BIB ();
    }
    
}  /* end wrtjmp */


/* PROGRAM: rlTXElock - lock the transaction end lock
 *
 * RETURNS: DSMVOID
 *
 */
DSMVOID
rlTXElock(dsmContext_t *pcontext, LONG lockMode, LONG operationType)
{
    RLCTL      *prlctl = pcontext->pdbcontext->prlctl;
    usrctl_t   *pself;
    int         conflict;

    
    if (pcontext->pdbcontext->argnoshm) return;
    if (prlctl == NULL)
    {
        /* Progress temp db has transaction table but
           no bi file - so just return in this case */

	return;
    }
    /* If this is not a lock upgrade request then make sure we have
       no buffer locks at this point.                               */
    pself = pcontext->pusrctl;
    if( pself->uc_txelk == 0 && pself->uc_bufsp != 0 )
    {
        /* Good manners requires getting the transaction end lock before you
           get any buffer locks -- and we enforce these manners ruthlessly.
           Deadlocks between buffer locks and txe locks will occur if we do
           not. */
	FATAL_MSGN_CALLBACK(pcontext,rlFTL045);
    }
    /* Only allow uprades from RL_TXE_SHARE to RL_TXE_UPDATE
       and no nested locks allowed */
    if( pself->uc_txelk != 0 )
    {
        if( !(pself->uc_txelk == RL_TXE_SHARE && lockMode == RL_TXE_UPDATE ))
        {
            /* Oops - nested locks of different types are not allowed
               because it may cause deadlocks due to the fact that some
               other resource was almost certainly locked in between
               the calls. */
            pcontext->pdbcontext->pdbpub->shutdn = DB_EXBAD;
            FATAL_MSGN_CALLBACK (pcontext, rlFTL073,
                                 lockMode, (int)pself->uc_txelk);
            return;            
        }
    }
    
    /* Determine if the requested lock conflicts with any that might
       be granted.                                               */
    MT_LOCK_TXQ();

    if(lockMode != RL_TXE_COMMIT)
        prlctl->txeRequestsByOp[operationType]++;
    
    conflict = rlTXEgetLock(pcontext,prlctl, pself, lockMode);

    if (conflict)
    {
        /* Que for the lock                        */
        queForTXE(pcontext,prlctl,pself,lockMode);
        prlctl->txeLocks[lockMode].currentInQ++;
        if( lockMode != RL_TXE_COMMIT )
            prlctl->txeQuedByOp[operationType]++;
        MT_UNLK_TXQ ();

        /* We are now queued. Wait for the lock to be granted */
        latUsrWait (pcontext);

        /* SANITY: When we are awakened, we should have had the lock granted
           to us by the previous holder. */

        if ((pself->uc_txelk != (TEXT)lockMode) ||
            (prlctl->txeLocks[lockMode].theLock == 0))
        {
            pcontext->pdbcontext->pdbpub->shutdn = DB_EXBAD;
            FATAL_MSGN_CALLBACK (pcontext, rlFTL074);
        }
    }
    else
        MT_UNLK_TXQ();

    return;
}

/* PROGRAM: rlTXEunlock -    Release my lock on the TXE lock
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlTXEunlock(dsmContext_t *pcontext)
{
    RLCTL    *prlctl = pcontext->pdbcontext->prlctl;
    usrctl_t *pself;
    
    if (pcontext->pdbcontext->argnoshm) return;
    if (prlctl == NULL)
    {
        /* Progress temp db has transaction table but
           no bi file - so just return in this case */

	return;
    }
    
    pself = pcontext->pusrctl;

    if (!pself->uc_txelk)
    {
        FATAL_MSGN_CALLBACK(pcontext,rlFTL067); /* rlultxe: txe not locked */
    }
    MT_LOCK_TXQ();

    prlctl->txeLocks[pself->uc_txelk].theLock--;

    rlTXEgrantLocks(pcontext,prlctl);
    
    MT_UNLK_TXQ();
    pself->uc_txelk = (TEXT)0;
    
}

    
/* PROGRAM: rlTXEgetLock -          Determines if the requested lock is
 *                                  compatible with all existing locks on
 *                                  the TXE and if so aquires the lock.
 * RETURNS: 0 - got lock
 *          1 - lock conflict
 */
LOCALF
int
rlTXEgetLock(dsmContext_t *pcontext,
             RLCTL *prlctl, usrctl_t *pself, LONG lockMode)
{
    int conflict = 0;
    int i;

    if ( !pcontext->pdbcontext->pdbpub->fullinteg )
    {
        /* If running in no integrity mode (-i) then no need to get
           the txe lock -- so just grant it to keep things simple  */
        prlctl->txeLocks[lockMode].theLock++;
        pcontext->pdbcontext->pmtctl->lockcnt[TXSWAIT + lockMode - 1]++;     
    }
    else if ( lockMode == RL_TXE_SHARE )
    {
        if ((prlctl->txeLocks[RL_TXE_EXCL].theLock == 0 &&
             /* When granting queued locks all that matters
                is that there is no current exclusive lock */
            prlctl->qtxehead && QSELF(pself) == prlctl->qtxehead)
            ||
            (prlctl->txeLocks[RL_TXE_EXCL].theLock == 0 &&
            prlctl->txeLocks[RL_TXE_EXCL].currentInQ == 0 &&
            /* If we have qued commit we must que because of
               the requirement to grant update lock upgrade requests */
            prlctl->txeLocks[RL_TXE_COMMIT].currentInQ == 0))
        {
            prlctl->txeLocks[RL_TXE_SHARE].theLock++;
            pcontext->pdbcontext->pmtctl->lockcnt[TXSWAIT]++;
        }
        else
            conflict = 1;
    }
    else if (lockMode == RL_TXE_UPDATE)
    {
        /* If any commit or exclusive locks are currently in effect
           then que this lock.                             */
        if(prlctl->txeLocks[RL_TXE_COMMIT].theLock ||
           prlctl->txeLocks[RL_TXE_EXCL].theLock)
        {
            conflict = 1;
        }
        else
        {
            prlctl->txeLocks[RL_TXE_UPDATE].theLock++;
            pcontext->pdbcontext->pmtctl->lockcnt[TXBWAIT]++;   
        }
    }
    else if (lockMode == RL_TXE_COMMIT)
    {
        /* If any update or exclusive locks are in effect then que
           this request                                    */
        if (prlctl->txeLocks[RL_TXE_UPDATE].theLock ||
            prlctl->txeLocks[RL_TXE_EXCL].theLock)
        {
            conflict = 1;
        }
        else if(prlctl->qtxehead && prlctl->qtxehead != QSELF(pself))
            /* There are queued requests so que this one as well
               Unless the qhead is me then someone is wanting to grant
               me the lock and shortly I'll be taken off the queue */
            conflict = 1;
        else
        {
            prlctl->txeLocks[RL_TXE_COMMIT].theLock++;
            pcontext->pdbcontext->pmtctl->lockcnt[TXEWAIT]++;   
        }
    }
    else if (lockMode == RL_TXE_EXCL)
    {
        for ( i = 1; i <= RL_TXE_MAX_LOCK_TYPES; i++)
            conflict += prlctl->txeLocks[i].theLock;
        if(conflict == 0 )
        {
            int upgrades;
            /* But grant any queued upgrade requests before
               acquiring the exclusive lock.                 */
            upgrades = grantUpgradeRequests(pcontext,prlctl);
            if( upgrades )
                conflict = 1;
            else
            {
                pcontext->pdbcontext->pmtctl->lockcnt[TXXWAIT]++;
                prlctl->txeLocks[RL_TXE_EXCL].theLock++;
            }
        }
        else
        {
            conflict = 1;
        }
    }
    else
    {
        MSGD_CALLBACK(pcontext,"%GSYSTEM ERROR: Invalid txe lock request %l",
                      lockMode);
    }
    
    if (!conflict)
    {
        if(pself->uc_txelk == RL_TXE_SHARE)
            /* Its an upgrade remove the share lock  */
            prlctl->txeLocks[RL_TXE_SHARE].theLock--;
        
        pself->uc_txelk = lockMode;
    }
    
    return conflict;
}

/* PROGRAM: grantUpgradeRequests - Before granting an exclusive lock
 *                                 grant all of the queued upgrade
 *                                 requests.  The only allowable upgrade
 *                                 request being from share to update.
 *                                 We do this because the exclusive lock
 *                                 not only means we are the only ones with
 *                                 a lock on the txe but that no one
 *                                 has a buffer
 *                                 locked for modify or intent to update.
 * RETURNS: number of locks granted.
 */
LOCALF
int
grantUpgradeRequests(dsmContext_t *pcontext, RLCTL *prlctl)
{
    usrctl_t   *pwaiter;
    usrctl_t   *pprevWaiter = 0;
    int         upgrades;

    pwaiter = XUSRCTL(pcontext->pdbcontext,prlctl->qtxehead);
    for(upgrades = 0; prlctl->txeLocks[RL_TXE_UPDATE].currentInQ &&
            pwaiter != NULL;)
    {
        if(pwaiter->uc_txelk == RL_TXE_SHARE)
        {
            /* This user is queued for upgrade
             give 'em the lock */
            prlctl->txeLocks[RL_TXE_UPDATE].currentInQ--;
            prlctl->txeLocks[RL_TXE_UPDATE].theLock++;
            /* Count the lock grant   */
            pcontext->pdbcontext->pmtctl->lockcnt[TXBWAIT]++;
            prlctl->txeLocks[RL_TXE_SHARE].theLock--;

            if(QSELF(pwaiter) == prlctl->qtxetail)
                prlctl->qtxetail = QSELF(pprevWaiter);
       
            pprevWaiter->uc_qwtnxt = pwaiter->uc_qwtnxt;
            pwaiter->uc_qwtnxt = 0;
            pwaiter->uc_txelk = RL_TXE_UPDATE;
            latUsrWake(pcontext,pwaiter,pwaiter->uc_wait);
            pwaiter = XUSRCTL(pcontext->pdbcontext,
                              pprevWaiter->uc_qwtnxt);
            upgrades++;
        }
        else
        {
            pprevWaiter = pwaiter;
            pwaiter = XUSRCTL(pcontext->pdbcontext,pwaiter->uc_qwtnxt);
        }
    }
    return upgrades;
}

        
/* PROGRAM: queForTXE - queue for the TXE lock
 *
 * RETURNS: void
 *
 */
LOCALF
DSMVOID
queForTXE(dsmContext_t *pcontext, RLCTL *prlctl,
          usrctl_t *pself, int lockType)
{
    usrctl_t  *pLastUser;
    
    pself->uc_wait = TXEWAIT;
    pself->uc_wait1 = lockType;
    pself->uc_qwtnxt = 0;
    
    
    if (prlctl->qtxehead == (QUSRCTL)NULL)
    {
        /* The queue is empty so we become the head */
        prlctl->qtxehead = QSELF (pself);
    }
    else
    {
        /* Tail points to us */
        pLastUser = XUSRCTL (pcontext->pdbcontext,prlctl->qtxetail);
        pLastUser->uc_qwtnxt = QSELF (pself);
    }
    /* We are the last entry in the queue */
    prlctl->qtxetail = QSELF (pself);
    if ( lockType == RL_TXE_SHARE )
        pself->uc_wait = TXSWAIT;
    else if( lockType == RL_TXE_UPDATE )
        pself->uc_wait = TXBWAIT;
    else if ( lockType == RL_TXE_COMMIT )
        pself->uc_wait = TXEWAIT;
    else
        pself->uc_wait = TXXWAIT;
}

/* PROGRAM: rlTXEgrantLocks - Wake up any users waiting for locks
 *
 * RETURNS: DSMVOID
 */
LOCALF
DSMVOID
rlTXEgrantLocks(dsmContext_t *pcontext, RLCTL *prlctl)
{
    usrctl_t    *pgrantee;
    int conflict = 0;
    
    while (prlctl->qtxehead != 0 && !conflict)
    {
        /* Grant locks until there is a conflict  */
        pgrantee = XUSRCTL(pcontext->pdbcontext,prlctl->qtxehead);
        conflict = rlTXEgetLock(pcontext,prlctl,pgrantee,
                                (LONG)pgrantee->uc_wait1);
        if(!conflict)
        {
            prlctl->txeLocks[pgrantee->uc_wait1].currentInQ--;
            prlctl->qtxehead = pgrantee->uc_qwtnxt;
            pgrantee->uc_qwtnxt = 0;
            latUsrWake(pcontext,pgrantee,pgrantee->uc_wait);
        }
    }
    if(prlctl->qtxehead == 0)
        prlctl->qtxetail = 0;
    else
    {
        /* If any update locks have been granted; then grant all
           that maybe queued to prevent deadlocks. Where an update lock
           is not released because it is blocked waiting to get a buffer
           lock that won't be released because the user holding it has
           a queued update txe lock request.                          */
        if (!prlctl->txeLocks[RL_TXE_COMMIT].theLock &&
            !prlctl->txeLocks[RL_TXE_EXCL].theLock)
            conflict = grantUpgradeRequests(pcontext, prlctl);

    }
}


/* PROGRAM: rlnull - NO-OP action routine
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlnull(
	dsmContext_t	*pcontext _UNUSED_,
	RLNOTE		*prlnote _UNUSED_,	/* not used */
	BKBUF		*pblk _UNUSED_,		/* not used */
	COUNT		 dlen _UNUSED_,		/* not used */
	TEXT		*pdata _UNUSED_)	/* not used */
{

}  /* end rlnull */
