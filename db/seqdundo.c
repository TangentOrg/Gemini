
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
*	C Source:		seqdundo.c
*	Subsystem:		1
*	Description:	
*	%created_by:	richt %
*	%date_created:	Tue Jan 10 17:53:58 1995 %
*
**********************************************************************/

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include "dbconfig.h"

typedef struct bkbuf void_seq_bkbuf_t;

#include "bmpub.h"  /* for bkrlsbuf funct call */
#include "dbcontext.h"  /* for p_dbctl */
#include "latpub.h" /* for MT_LOCK_SEQ */

#include "seqpub.h"
#include "seqprv.h"
#include "sedoundo.h"
#include "semsg.h"

#include "rlprv.h"  /* BUM for prlctl->rlwarmstrt */
#include "utspub.h"

#ifdef SEQTRACE
#define MSGD MSGD_CALLBACK
#endif

/*** LOCAL FUNCTION PROTOTYPES ***/

LOCALF DSMVOID     seqUDRemove	(dsmContext_t *pcontext,
				 seqCtl_t *pseqctl, seqTbl_t *pseqtbl);

/*** END LOCAL FUNCTION PROTOTYPES ***/

/* *********************************************************************
   *********************************************************************

 	The functions below are the do and undo functions for the bi
	notes

   *********************************************************************
   ********************************************************************* */


/* PROGRAM: seqDoAdd - add sequence to cache, set initial value
 *			_Sequence record has been created
 *			called from rlphynt or from roll forward
 *
 * RETURNS: DSMVOID
 */
DSMVOID
seqDoAdd (
	dsmContext_t	*pcontext _UNUSED_,
	RLNOTE		*pSeqNote,	/* pointer to rl note		*/
	void_seq_bkbuf_t *pblock,	/* pointer to sequence block	*/
	COUNT		 reclen _UNUSED_,	/* not used */
	TEXT		*precord _UNUSED_)	/* not used */
{
    seqAddNote_t *pnote = (seqAddNote_t *)pSeqNote;
    seqBlock_t	 *pbk = (seqBlock_t *)pblock;

    /* store the initial value in the block */

    pbk->seq_values[xct ((TEXT*)&pnote->seqno)] = xlng ((TEXT*)&pnote->newval);

}  /* end seqDoAdd */


/* PROGRAM: seqDoAssign - assign a sequence number
 *		  	  called from rlphynt or from roll forward
 *
 * RETURNS: DSMVOID
 */
DSMVOID
seqDoAssign (
	dsmContext_t	*pcontext,
	RLNOTE		*pSeqNote, /* pointer to rl note		*/
	void_seq_bkbuf_t *pblock _UNUSED_,  /* pointer to sequence block */
	COUNT		 reclen _UNUSED_,   /* not used */
	TEXT		*precord _UNUSED_)  /* not used */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    seqAsnNote_t *pnote = (seqAsnNote_t *)pSeqNote;
#if 0
    seqBlock_t	*pbk = (seqBlock_t *)pblock;
#endif

    seqCtl_t	*pseqctl = pdbcontext->pseqctl;
    seqTbl_t	*pseqtbl;
    COUNT	sno;

    if (pdbcontext->prlctl->rlwarmstrt)
    {
	/* We don't need to do anything during warm start because the
	   sequence cache will be loaded from the _sequence records
	   after warm start is finished */

	return;
    }
    sno = xct ((TEXT*)&pnote->seqno);
    pseqtbl = pseqctl->seq_setbl + sno;

#ifdef SEQTRACE
MSGD(pcontext, "%LDoAssign seq %i", sno);
#endif

    MT_LOCK_SEQ ();

    /* mark the slot reserved. There can be no record with dbkey 1 */

    pseqtbl->seq_recid = (DBKEY)1;
    MT_UNLK_SEQ ();

}  /* end seqDoAssign */


/* PROGRAM: seqDoDelete - delete sequence from cache
 *			_Sequence record has been deleted
 *			called from rlphynt or from roll forward
 *
 * RETURNS: DSMVOID
 */
DSMVOID
seqDoDelete (	
	dsmContext_t	*pcontext,
	RLNOTE		*pnote,	/* pointer to rl note		*/
	void_seq_bkbuf_t *pbk _UNUSED_,	/* pointer to sequence block	*/
	COUNT		 reclen _UNUSED_,	/* not used */
	TEXT		*precord _UNUSED_)	/* not used */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
	seqCtl_t	*pseqctl = pdbcontext->pseqctl;
	seqTbl_t	*pseqtbl;
	COUNT		 sno;

    if (pdbcontext->prlctl->rlwarmstrt)
    {
	/* We don't need to do anything during warm start because the
	   sequence cache will be loaded from the _sequence records
	   after warm start is finished */

	return;
    }
    sno = xct ((TEXT*)&((seqDelNote_t *)pnote)->seqno);
    pseqtbl = pseqctl->seq_setbl + sno;

#ifdef SEQTRACE
MSGD(pcontext, "%LDoDelete %i recid %D", (int)sno, pseqtbl->seq_recid);
#endif

    MT_LOCK_SEQ ();

    /* just free the slot */

    seqUDRemove (pcontext, pseqctl, pseqtbl);

    MT_UNLK_SEQ ();

}  /* end seqDoDelete */


/* PROGRAM: seqDoIncrement - get the next value of a seqence number
 *		 	     _Sequence record has not been changed
 *			     called from rlphynt or from roll forward
 *
 * RETURNS: DSMVOID
 */
DSMVOID
seqDoIncrement (
	dsmContext_t	*pcontext _UNUSED_,
	RLNOTE		*pnote,   /* pointer to rl note	*/
	void_seq_bkbuf_t *pbk,	   /* pointer to sequence block	*/
	COUNT		 reclen _UNUSED_,  /* not used */
	TEXT		*precord _UNUSED_) /* not used */
{

    /* store the new sequence number value in the block */

   ((seqBlock_t *)pbk)->seq_values[xct ((TEXT*)&((seqIncNote_t *)pnote)->seqno)]
                  = xlng ((TEXT*)&((seqIncNote_t *)pnote)->newval);

}  /* end seqDoIncrement */


/* PROGRAM: seqDoSetValue - set the current value of a seqence
 *			     _Sequence record has not been changed
 *			     called from rlphynt or from roll forward
 *
 * RETURNS: DSMVOID
 */
DSMVOID
seqDoSetValue (
	dsmContext_t	*pcontext _UNUSED_,
	RLNOTE	        *pnote,	  /* pointer to rl note		*/
	void_seq_bkbuf_t *pbk,	  /* pointer to sequence block	*/
	COUNT	         reclen _UNUSED_,  /* not used */
	TEXT	        *precord _UNUSED_) /* not used */
{
    /* store the new current value in the block */

   ((seqBlock_t *)pbk)->seq_values[xct ((TEXT*)&((seqSetNote_t *)pnote)->seqno)]
                 = xlng ((TEXT*)&((seqSetNote_t *)pnote)->newval);

}  /* end seqDoSetValue */


/* PROGRAM: seqDoUpdate - update cache when seq defn changed
 *			_Sequence record has been updated
 *			called from rlphynt or from roll forward
 *
 * RETURNS: DSMVOID
 */
DSMVOID
seqDoUpdate (
	dsmContext_t	*pcontext,
	RLNOTE		*prlnote,  /* pointer to rl note		*/
	void_seq_bkbuf_t *pblock _UNUSED_,  /* pointer to sequence block */
	COUNT		 reclen _UNUSED_,   /* not used */
	TEXT		*precord _UNUSED_)  /* not used */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    seqUpdNote_t *pnote = (seqUpdNote_t *)prlnote;
#if 0
    seqBlock_t	 *pbk = (seqBlock_t *)pblock;
#endif

    seqCtl_t	*pseqctl = pdbcontext->pseqctl;
    seqTbl_t	*pseqtbl;
    seqTbl_t	 tmpseqtbl;
    DBKEY	 rid;
    COUNT	 sno;

    if (pdbcontext->prlctl->rlwarmstrt)
    {
	/* We don't need to do anything during warm start because the
	   sequence cache will be loaded from the _sequence records
	   after warm start is finished */

	return;
    }
    rid = xlng ((TEXT*)&pnote->recid);
    sno = xct ((TEXT*)&pnote->seqno);
    pseqtbl = pseqctl->seq_setbl + sno;

#ifdef SEQTRACE
MSGD(pcontext, "%LDoUpdate %i recid %D %s",
     (int)sno, pseqtbl->seq_recid, pnote->name);
#endif

    stnclr ((TEXT *)&tmpseqtbl, (int)(sizeof (tmpseqtbl)));

    /* get new */

    tmpseqtbl.seq_recid	= xlng ((TEXT*)&pnote->recid);
    tmpseqtbl.seq_initial = xlng ((TEXT*)&pnote->new_initial);
    tmpseqtbl.seq_min	= xlng ((TEXT*)&pnote->new_min);
    tmpseqtbl.seq_max	= xlng ((TEXT*)&pnote->new_max);
    tmpseqtbl.seq_increment = xlng ((TEXT*)&pnote->new_increment);
    tmpseqtbl.seq_cycle	= (GBOOL)(pnote->new_cycle);
    tmpseqtbl.seq_num	= sno;
    bufcop (tmpseqtbl.seq_name, pnote->new_name, sizeof (tmpseqtbl.seq_name));

    MT_LOCK_SEQ ();

    /* remove the old data */

    seqUDRemove (pcontext, pseqctl, pseqtbl);

    /* copy in the new values */

    bufcop (pseqtbl, &tmpseqtbl, sizeof (tmpseqtbl));

    /* put name in hash table */

    seqInsert (pcontext, pseqctl, pseqtbl);

    MT_UNLK_SEQ ();

}  /* end seqDoUpdate */


/* PROGRAM: seqUDRemove - remove a setbl entry from the name lookup table
 *                      and free the setbl entry
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
seqUDRemove (
	dsmContext_t	*pcontext,
	seqCtl_t	*pseqctl,
        seqTbl_t	*pseqtbl)
{
        seqTbl_t  *pprev;
        seqTbl_t  *pcur;
        COUNT     n;

    if (pseqtbl->seq_recid == (DBKEY)0)
        return;

    if (pseqtbl->seq_recid == (DBKEY)1)
    {
        /* entry has been reserved but was never loaded with data */

        pseqtbl->seq_nxhash = SEQINVALID;
        pseqtbl->seq_vhash = SEQINVALID;
        pseqtbl->seq_recid = (DBKEY)0;
        return;
    }
    /* entry has data in it */

#ifdef SEQTRACE
MSGD(pcontext, "%LRemove: seq %i %s", pseqtbl->seq_num, pseqtbl->seq_name);
#endif

    /* walk down the hash chain to find which entry points to the one we want
       to delete */

    pcur = (seqTbl_t *)PNULL;
    for (n = pseqctl->seq_hash[pseqtbl->seq_vhash];
         n != SEQINVALID;
         n = pcur->seq_nxhash) /* bug #95-08-03-075 */
    {
        /* get entry address */

        pprev = pcur;
        pcur = pseqctl->seq_setbl + n;

        if (pcur != pseqtbl)
            continue;

        /* now have the previous entry. disconnect current */

        if (pprev)
        {
            pprev->seq_nxhash = pseqtbl->seq_nxhash;
        }
        else
        {
            /* bug #95-08-03-075 */
            pseqctl->seq_hash[pseqtbl->seq_vhash] = pseqtbl->seq_nxhash;
        }

        /* free the entry */

        pseqtbl->seq_recid = (DBKEY)0;
        pseqtbl->seq_nxhash = SEQINVALID;
        return;
    }
    MSGN_CALLBACK(pcontext, seMSG001, pseqtbl->seq_name);

}  /* end seqUDRemove */


#if 0
LOCALF DSMVOID
seqPrintChain(
	dsmContext_t	*pcontext,
	seqCtl_t	*pseqctl,
        COUNT            vhash)
{
    COUNT n;
    seqTbl_t *pcur = (seqTbl_t *)PNULL;

    for (n = pseqctl->seq_hash[vhash];
         n != SEQINVALID;
         n = pcur->seq_nxhash) /* bug #95-08-03-075 */
    {
        /* get entry address */
        pcur = pseqctl->seq_setbl + n;
        MSGD_CALLBACK(pcontext,
                     (TEXT *)("0l, %l, %s"), pcur->seq_vhash,
                    pcur->seq_nxhash, pcur->seq_name);
    }

}  /* end seqPrintChain */
#endif


/* PROGRAM: seqUndoAdd - undo add sequence to cache
 *
 * RETURNS: DSMVOID
 */
DSMVOID
seqUndoAdd (
	dsmContext_t	*pcontext _UNUSED_,
	RLNOTE		*pnote,	/* pointer to rl note		*/
	void_seq_bkbuf_t  *pbk,	/* pointer to sequence block	*/
	COUNT		 reclen _UNUSED_,	/* not used */
	TEXT		*precord _UNUSED_)	/* not used */
{
    /* restore the old current value in the block */

   ((seqBlock_t *)pbk)->seq_values[xct ((TEXT*)&((seqAddNote_t *)pnote)->seqno)]
          = xlng ((TEXT*)&((seqAddNote_t *)pnote)->oldval);

}  /* end seqUndoAdd */


/* PROGRAM: seqUndoAssign - undo an assign. restore previous initial value
 *			    called during backout or transaction undo
 *
 * RETURNS: DSMVOID
 */
DSMVOID
seqUndoAssign (
	dsmContext_t	*pcontext,
	RLNOTE	        *prlnote, /* pointer to rl note		*/
	void_seq_bkbuf_t *pblock,  /* pointer to sequence block	*/
	COUNT	         reclen _UNUSED_,  /* not used */
	TEXT	        *precord _UNUSED_) /* not used */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    seqAsnNote_t *pnote = (seqAsnNote_t *)prlnote;
    seqBlock_t	 *pbk = (seqBlock_t *)pblock;

    seqCtl_t	 *pseqctl = pdbcontext->pseqctl;
    seqTbl_t	 *pseqtbl;
    COUNT	  sno;

    sno = xct ((TEXT*)&pnote->seqno);
    pseqtbl = pseqctl->seq_setbl + sno;

#ifdef SEQTRACE
MSGD(pcontext, "%LUndoAssign seq %i", (int)sno);
#endif

    if (pdbcontext->prlctl->rlwarmstrt == 0)
    {
	/* This part does not need to be undone during warm start, only
	   during transaction backout, because the entire cache gets
	   reloaded after warm start is done */

        MT_LOCK_SEQ ();

        /* just free the slot */

        seqUDRemove (pcontext, pseqctl, pseqtbl);

        MT_UNLK_SEQ ();
    }
    /* restore the old current value in the block */

    pbk->seq_values[sno] = xlng ((TEXT*)&pnote->oldval);

}  /* end seqUndoAssign */


/* PROGRAM: seqUndoDelete - undo a delete
 *		            _Sequence record will be restored after
 *			    called during backout or transaction undo
 *
 * RETURNS: DSMVOID
 */
DSMVOID
seqUndoDelete (
	dsmContext_t	*pcontext,
	RLNOTE	        *prlnote, /* pointer to rl note		*/
	void_seq_bkbuf_t *pbk _UNUSED_,	  /* pointer to sequence block	*/
	COUNT	         reclen _UNUSED_,  /* not used */
	TEXT	        *precord _UNUSED_) /* not used */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    seqDelNote_t *pnote = (seqDelNote_t *)prlnote;

    seqCtl_t	 *pseqctl = pdbcontext->pseqctl;
    seqTbl_t	 *pseqtbl;
    COUNT	  sno;

    if (pdbcontext->prlctl->rlwarmstrt)
    {
	/* We don't need to do anything during warm start because the
	   sequence cache will be loaded from the _sequence records
	   after warm start is finished */

	return;
    }
    sno = xct ((TEXT*)&pnote->seqno);
    pseqtbl = pseqctl->seq_setbl + sno;

#ifdef SEQTRACE
MSGD(pcontext, "%LUndoDelete %i recid %D %s",
     (int)sno, xlng ((TEXT*)&pnote->recid), pnote->name);
#endif

    MT_LOCK_SEQ ();

    /* restore the other values from the note */

    pseqtbl->seq_recid = xlng ((TEXT*)&pnote->recid);
    pseqtbl->seq_initial = xlng ((TEXT*)&pnote->initial);
    pseqtbl->seq_min = xlng ((TEXT*)&pnote->min);
    pseqtbl->seq_max = xlng ((TEXT*)&pnote->max);
    pseqtbl->seq_increment = xlng ((TEXT*)&pnote->increment);
    pseqtbl->seq_cycle = (GBOOL)(pnote->cycle);
    bufcop (pseqtbl->seq_name, pnote->name, sizeof (pseqtbl->seq_name));

    /* put the name back in the name lookup table */

    seqInsert (pcontext, pseqctl, pseqtbl);

    MT_UNLK_SEQ ();

}  /* end seqUndoDelete */


/* PROGRAM: seqUndo - logically undo a sequence operation
 *            called from transaction backout
 *
 * RETURNS: DSMVOID
 */
DSMVOID
seqUndo(
	dsmContext_t	*pcontext,
	RLNOTE		*prlnote)		/* note to undo	*/
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    void_seq_bkbuf_t	*pseqblk;
    bmBufHandle_t        seqBufHandle;

    rlTXElock(pcontext,RL_TXE_UPDATE,RL_MISC_OP);

    seqBufHandle = bmLocateBlock(pcontext, BK_AREA1,
                                 pdbcontext->pmstrblk->mb_seqblk, TOMODIFY);
    pseqblk = (void_seq_bkbuf_t *)bmGetBlockPointer(pcontext,seqBufHandle);
    switch(prlnote->rlcode)
    {
      case RL_SEASN:
           seqUndoAssign(pcontext, prlnote, pseqblk,
		        (COUNT)0, (TEXT *)PNULL);
           break;
      case RL_SEADD:
           seqUndoAdd(pcontext, prlnote, pseqblk,
		     (COUNT)0, (TEXT *)PNULL);
           break;
      case RL_SEUPD:
           seqUndoUpdate(pcontext, prlnote, pseqblk,
		    (COUNT)0, (TEXT *)PNULL);
           break;
      case RL_SEDEL:
           seqUndoDelete(pcontext, prlnote, pseqblk,
		        (COUNT)0, (TEXT *)PNULL);
           break;
      default:		/* fatal error */
           FATAL_MSGN_CALLBACK(pcontext, seFTL003, prlnote->rlcode);
    }
    
    bmReleaseBuffer(pcontext, seqBufHandle);
    
    rlTXEunlock(pcontext);

}  /* end seqUndo */


/* PROGRAM: seqUndoSetValue - restore the old current value of a seqence
 *			      _Sequence record has not been changed
 *			      called during backout or transaction undo
 *
 * RETURNS: DSMVOID
 */
DSMVOID
seqUndoSetValue (
	dsmContext_t	*pcontext _UNUSED_,
	RLNOTE		*pnote,   /* pointer to rl note	*/
	void_seq_bkbuf_t *pbk,	    /* pointer to sequence block */
	COUNT		 reclen _UNUSED_,  /* not used */
	TEXT		*precord _UNUSED_) /* not used */
{
    /* store the old current value in the block */

   ((seqBlock_t *)pbk)->seq_values[xct ((TEXT*)&((seqSetNote_t *)pnote)->seqno)]
           = xlng ((TEXT*)&((seqSetNote_t *)pnote)->oldval);

}  /* end seqUndoSet */


/* PROGRAM: seqUndoUpdate - undo an update
 *			    _Sequence record will be undone later
 *			    called during backout or transaction undo
 *
 * RETURNS: DSMVOID
 */
DSMVOID
seqUndoUpdate (
	dsmContext_t	*pcontext,
	RLNOTE	        *prlnote, /* pointer to rl note	*/
	void_seq_bkbuf_t *pbk _UNUSED_,	  /* pointer to sequence block */
	COUNT	         reclen _UNUSED_,  /* not used */
	TEXT	        *precord _UNUSED_) /* not used */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    seqUpdNote_t *pnote = (seqUpdNote_t *)prlnote;
    seqCtl_t	 *pseqctl = pdbcontext->pseqctl;
    seqTbl_t	 *pseqtbl;
    COUNT	  sno;

    if (pdbcontext->prlctl->rlwarmstrt)
    {
	/* We don't need to do anything during warm start because the
	   sequence cache will be loaded from the _sequence records
	   after warm start is finished */

	return;
    }
    sno = xct ((TEXT*)&pnote->seqno);
    pseqtbl = pseqctl->seq_setbl + sno;

#ifdef SEQTRACE
MSGD(pcontext, "%LUndoUpdate %i recid %D", (int)sno, pseqtbl->seq_recid);
#endif

    MT_LOCK_SEQ ();

    /* remove name from table */

    seqUDRemove (pcontext, pseqctl, pseqtbl);

    /* restore the other values from the note */

    pseqtbl->seq_recid = xlng ((TEXT*)&pnote->recid);
    pseqtbl->seq_initial = xlng ((TEXT*)&pnote->initial);
    pseqtbl->seq_min = xlng ((TEXT*)&pnote->min);
    pseqtbl->seq_max = xlng ((TEXT*)&pnote->max);
    pseqtbl->seq_increment = xlng ((TEXT*)&pnote->increment);
    pseqtbl->seq_cycle = (GBOOL)(pnote->cycle);
    bufcop (pseqtbl->seq_name, pnote->name, sizeof (pseqtbl->seq_name));

    /* put the name back in the name lookup table */

    seqInsert (pcontext, pseqctl, pseqtbl);

    MT_UNLK_SEQ ();

}  /* end seqUndoUpdate */
