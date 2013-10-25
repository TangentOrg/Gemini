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
*	C Source:		rmdoundo.c
*	Subsystem:		1
*	Description:	
*	%created_by:	richb %
*	%date_created:	Fri Jan  6 13:49:25 1995 %
*
**********************************************************************/

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include "dbconfig.h"

#include "dbcontext.h"     /* needed for rm[*]uCt */
#include "lkpub.h"     /* needed for LKEXCL & LCK */
#include "recpub.h"
#include "rmpub.h"
#include "rmdoundo.h"
#include "rmprv.h"
#include "rmmsg.h"
#include "stmpub.h"
#include "stmprv.h"        /* needed for GLOBAL STPOOL *pdsmStoragePool */
#include "tmmgr.h"
#include "tmtrntab.h"  /* needed for MAXTRN */
#include "utmpub.h"
#include "utspub.h"
#include "rlprv.h"


/*** LOCAL FUNCTION PROTOTYPES ***/

LOCALF int      rmDoDif         (dsmContext_t *pcontext, COUNT whichWay,
				 COUNT oldLen, TEXT *pOldRec,
                                 rmDiffList_t *pDifList, COUNT newLen,
                                 TEXT *pNewRec);
LOCALF DSMVOID     rmSetLogicalUndo (dsmContext_t *pcontext, COUNT txSlot);

/*** END LOCAL FUNCTION PROTOTYPES ***/



/* PROGRAM: rmDoChange - changes a record
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rmDoChange(
	dsmContext_t	*pcontext,
	RLNOTE		*pnote,     /* pointer to rl note	  */
	struct bkbuf	*pblock,    /* pointer to database block  */
	COUNT		 difLen _UNUSED_,    /* length of difference list  */
	TEXT		*pdata)     /* pointer to difference list */
{
    rmChangeNote_t *pchgnote = (rmChangeNote_t *)pnote;  /* ptr to rl note  */
    rmBlock_t	   *pbk = (rmBlock_t *)pblock;      /* ptr to database block */
    rmDiffList_t   *pDifL = (rmDiffList_t *)pdata;  /* ptr to difference list */

    TEXT     *pNewRec;      /* pointer to new record */
    COUNT     newSize;          /* size of new record */ 
    rmCtl_t   rmctl;

     rmRCtl (pcontext, pbk, pchgnote->recnum, &rmctl);

     /* build the new record */

     pNewRec = (TEXT *)stRentd (pcontext, pdsmStoragePool, pchgnote->newsz);
     newSize = rmDoDif (pcontext, 1, rmctl.recsize, rmctl.prec, pDifL,
                        pchgnote->newsz, pNewRec);

     if (newSize != pchgnote->newsz)
          FATAL_MSGN_CALLBACK(pcontext, rmFTL001, newSize, pchgnote->newsz);

     if (rmctl.recsize == newSize)
     {
         STATINC (rmfuCt);
         STATADD (rmbuCt, (ULONG)newSize);

         bufcop (rmctl.prec, pNewRec, (int)newSize);
     }
     else
     {
         rmDoDelete (pcontext, pnote, pblock, rmctl.recsize, (TEXT *)NULL);

         /* put in the new record */

         rmDoInsert (pcontext, pnote, pblock, newSize, pNewRec);
     }
     stVacate (pcontext, pdsmStoragePool, pNewRec);

}  /* end rmDoChange */


/* PROGRAM: rmDoDif - Process diff list making a new record from an old one.
 *
 *      Process a difference list to make a new record from an old one.
 *      The difference list consists of one or more entries which contain
 *      record offset, length, byte stream from the original record and
 *      record offset, length, byte stream from the new record.
 *      Either can be empty. Any part of the record which is not mentioned
 *      by the difference entries is the same in the old and new records.
 *
 * RETURNS: The length of the new record is returned
 */
LOCALF int
rmDoDif (
        dsmContext_t *pcontext,
        COUNT          whichWay,
        COUNT          oldLen,
        TEXT          *pOldRec,
        rmDiffList_t  *pDifList,
        COUNT          newLen,
        TEXT          *pNewRec)
{
        int             i;
        rmDiffEntry_t  *pE;
        COUNT           srcLen;
        COUNT           dstLen;
        COUNT           tsz;
        COUNT           dsz;
        TEXT           *pSrcRec;
        TEXT           *pDstRec;
        TEXT           *pSrcDif;
        TEXT           *pDstDif;
        COUNT           copyCnt;
        COUNT           insCnt;
        COUNT           delCnt;

    pE = (rmDiffEntry_t *)((TEXT *)pDifList + sizeof (rmDiffList_t));
    pSrcRec = pOldRec;
    srcLen = oldLen;
    pDstRec = pNewRec;
    dstLen = 0;
    dsz = newLen;
    for (i = pDifList->numDifs; i > 0; i--)
    {
        if (whichWay == 1)
        {
            /* This is a do */

            pSrcDif = (TEXT *)pE + sizeof (rmDiffEntry_t);
            pDstDif = pSrcDif + pE->oldLen;
            delCnt = pE->oldLen;
            insCnt = pE->newLen;
            copyCnt = pE->oldOffset - (pSrcRec - pOldRec);
        }
        else
        {
            /* This is an undo */

            pDstDif = (TEXT *)pE + sizeof (rmDiffEntry_t);
            pSrcDif = pDstDif + pE->oldLen;
            insCnt = pE->oldLen;
            delCnt = pE->newLen;
            copyCnt = pE->newOffset - (pSrcRec - pOldRec);
        }
        tsz = dstLen + copyCnt - delCnt + insCnt;
        if (tsz > dsz)
        {
            MSGN_CALLBACK(pcontext, rmMSG002, tsz, dsz);
            return (tsz);
        }
        if (copyCnt > 0)
        {
            /* copy from source to destination */

            bufcop (pDstRec, pSrcRec, copyCnt);
            pSrcRec = pSrcRec + copyCnt;
            pDstRec = pDstRec + copyCnt;
            srcLen = srcLen - copyCnt;
            dstLen = dstLen + copyCnt;
        }
        if (delCnt > 0)
        {
            /* delete some old material */

            pSrcRec = pSrcRec + delCnt;
            srcLen = srcLen - delCnt;
        }
        if (insCnt > 0)
        {
            /* insert some new material */

            bufcop (pDstRec, pDstDif, insCnt);
            pDstRec = pDstRec + insCnt;
            dstLen = dstLen + insCnt;
        }
        /* advance to next diff entry */

        pE = (rmDiffEntry_t *)( (TEXT *)pE 
                              + delCnt
                              + insCnt
                              + sizeof (rmDiffEntry_t));
    }
    if (srcLen > 0)
    {
        tsz = srcLen + dstLen;
        if (tsz > dsz)
        {
            MSGN_CALLBACK(pcontext, rmMSG002, tsz, dsz);
            return (tsz);
        }
        /* copy remainder of old record to new */

        bufcop (pDstRec, pSrcRec, srcLen);
        dstLen = dstLen + srcLen;
    }
    return (dstLen);

}  /* end rmDoDif */


/* PROGRAM: rmDoInsert - inserts a record or fragment in a block
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rmDoInsert(
	dsmContext_t	*pcontext,
	RLNOTE		*pnote,        	/* pointer to rl note		*/
	struct bkbuf	*pblock,	/* pointer to database block	*/
	COUNT		 reclen,       	/* length of record or fragment	*/
	TEXT		*precord)      	/* record or fragment to insert	*/
{
    rmRecNote_t	*prmrecnt = (rmRecNote_t *)pnote;
    rmBlock_t	*pbk = (rmBlock_t *)pblock;
    TEXT	*pt;		/* points into block where record will go*/
    COUNT	 ofst;		/* offset of record in the block	 */
    COUNT	 space;		/* the amount of space the rec will use  */


    /* calculate how much space the record will use */
    space = reclen + sizeof(COUNT);	/* record plus length prefix */
    if (prmrecnt->flags & SPLITREC) /* if there is a next fragment	 */
	space += sizeof(DBKEY);   /* allow room for ptr to next fragment */
    else			  /* else reserve at least 6 bytes	 */
    if (space < (COUNT)(MINRSIZE+sizeof(pbk->dir[0])) )
	space = MINRSIZE+sizeof(pbk->dir[0]);

    if ((prmrecnt->flags & HOLD) == 0)
    {
        STATADD (rmbcCt, (ULONG)space);
        STATINC (rmfcCt);
    }

    /* bug 92-06-25-022: add dbkey to message */
    if (space > pbk->free )
        FATAL_MSGN_CALLBACK(pcontext, rmFTL012, pbk->rm_bkhdr.bk_dbkey);

    /* reduce the free space appropriately */
    pbk->free -= space;

    pt = (TEXT *)(pbk->dir + pbk->numdir) + pbk->free;
    ofst = pt - (TEXT *)pbk;

    /* move in the next fragment pointer */
    if (prmrecnt->flags & SPLITREC)
    {	sdbkey(pt, prmrecnt->nxtfrag);
	pt += sizeof(DBKEY);
    }

    /* move in the record length prefix and the record or fragment */
    sct(pt, reclen);
    bufcop(pt + sizeof(COUNT), precord, reclen);

    /* extend the directory area if necessary */
    while(prmrecnt->recnum >= pbk->numdir)
    {
	pbk->free -= sizeof(pbk->dir[0]);
	pbk->dir[pbk->numdir++] = 0;
    }

    /* fill in the new directory entry */
    pbk->freedir--;
    pbk->dir[prmrecnt->recnum] = ofst | (prmrecnt->flags & ~RMOFFSET);

}  /* end rmDoInsert */


/* PROGRAM: rmDoNextFrag - insert a next fragment pointer into a record
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rmDoNextFrag(
	dsmContext_t	*pcontext _UNUSED_,
	RLNOTE		*pnote, 	/* the physical note		*/
	struct bkbuf	*pblock,	/* pointer to database block	*/
	COUNT		 len _UNUSED_,	/* unused	     */
	TEXT		*pdata _UNUSED_) /* unused	     */
{
    rmNextFragNote_t *prmnxtfnt = (rmNextFragNote_t *)pnote;
    rmBlock_t    *pbk = (rmBlock_t *)pblock;

    sdbkey((TEXT *)pbk + 
	 (pbk->dir[prmnxtfnt->recnum] & 
	  RMOFFSET),
	 prmnxtfnt->nxtfrag);

}  /* end rmDoNextFrag */


/* PROGRAM: rmDoDelete - deletes a record or fragment from a block
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rmDoDelete(
	dsmContext_t	*pcontext,
	RLNOTE		*pnote,	   /* pointer to rl note               */
	struct bkbuf	*pblock,   /* pointer to database block	       */
	COUNT		 reclen,   /* size of the record to be deleted */
	TEXT		*precord _UNUSED_)  /* copy of record (ignored) */
{
    rmRecNote_t	*prmrecnt = (rmRecNote_t *)pnote;
    rmBlock_t	*pbk = (rmBlock_t *)pblock;
    ULONG	*pdir;		/* points to the directory entry for rec */
    ULONG	 ofst;		/* offset in block to the record to remov*/
    ULONG	 space;		/* amount of space occupied by the record*/
    TEXT	*ps;
    TEXT	*pe;

    /* get location of record out of the directory */
    pdir = pbk->dir + prmrecnt->recnum;
    ofst = *pdir & RMOFFSET;

    /* determine how much space the record occupies */
    space = reclen+sizeof(COUNT);
    if (*pdir & SPLITREC) space += sizeof(DBKEY);
    else
    if (space < MINRSIZE+sizeof(pbk->dir[0])) 
        space = MINRSIZE+sizeof(pbk->dir[0]);

    if ((*pdir & HOLD) == 0)
    {
        STATADD (rmbdCt, (ULONG)space);
        STATINC (rmfdCt);
    }
    /* close up the hole where we removed the record */

    ps = (TEXT *)(pbk->dir + pbk->numdir) + pbk->free;
    pe = (TEXT *)pbk + ofst;

    bufcop (ps + space, ps, pe - ps);

    pbk->free += space;

    *pdir = 0;
    pbk->freedir++;

    /* delete trailing zero directory entries */
    for (pdir = pbk->dir + pbk->numdir - 1;
         (pdir >= pbk->dir) && (*pdir==0);
         pdir-- )
    {	pbk->numdir--;
	pbk->free += sizeof(pbk->dir[0]);
    }

    /* adjust directory entries for records which moved */
    for (;pdir >= pbk->dir; --pdir)
	if (*pdir && (*pdir & RMOFFSET) < ofst)
	    *pdir += space;

}  /* end rmDoDelete */


/* PROGRAM: rmUndoChange - physically undo a "changed" record
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rmUndoChange(
	dsmContext_t	*pcontext,
	RLNOTE		*pnote,   /* pointer to rl note	   */
	struct bkbuf	*pblock,  /* pointer to database block  */
	COUNT		 difLen _UNUSED_,  /* length of difference list  */
        TEXT		*pdata)   /* pointer to difference list */
{
    rmChangeNote_t *pchgnote = (rmChangeNote_t *)pnote;
    rmBlock_t	   *pbk = (rmBlock_t *)pblock;
    rmDiffList_t   *pDifL = (rmDiffList_t *)pdata;

    TEXT     *pOldRec;      /* pointer to old record */
    COUNT     oldSize;      /* size of old record */
    rmCtl_t   rmctl;

    rmRCtl (pcontext, pbk, pchgnote->recnum, &rmctl);

    /* reconstruct the old record */

    pOldRec = (TEXT *)stRentd (pcontext, pdsmStoragePool, pchgnote->recsz);
    oldSize = rmDoDif (pcontext, 0, rmctl.recsize, rmctl.prec, pDifL,
                       pchgnote->recsz, pOldRec);

    if (oldSize != pchgnote->recsz)
        FATAL_MSGN_CALLBACK(pcontext, rmFTL018, oldSize, pchgnote->recsz);

    if (rmctl.recsize == oldSize)
    {
         STATINC (rmfuCt);
         STATADD (rmbuCt, (ULONG)oldSize);
 
        bufcop (rmctl.prec, pOldRec, (int)oldSize);
    }
    else
    {
        /* take out the new record */
        rmDoDelete (pcontext, pnote, pblock, rmctl.recsize, (TEXT *)NULL);

        /* put back the old record */

        rmDoInsert (pcontext, pnote, pblock, oldSize, pOldRec);
    }
    stVacate(pcontext, pdsmStoragePool, pOldRec);

}  /* end rmUndoChange */


/* PROGRAM: rmUndoNextFrag - undo rmDoNextFrag
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rmUndoNextFrag(
	dsmContext_t	*pcontext _UNUSED_,
	RLNOTE		*pnote,	/* the physical note */
	struct bkbuf	*pblock,	/* pointer to database block */
	COUNT		 len _UNUSED_,	/* unused	     */
	TEXT		*pdata _UNUSED_) /* unused	     */
{
    rmNextFragNote_t *prmnxtfnt = (rmNextFragNote_t *)pnote;
    rmBlock_t    *pbk = (rmBlock_t *)pblock;

    sdbkey((TEXT *)pbk + (pbk->dir[prmnxtfnt->recnum] & RMOFFSET),
	 prmnxtfnt->prevval);

}  /* end rmUndoNextFrag */




/* data structure to control logical undo and logical roll forward */

#define RMUNDEL 1
#define RMRFCR	2
struct rmundo {
	TEXT	*pbuf;		/* points to start of buffer		*/
	TEXT	*pcur;		/* points location to put next fragment	*/
	DBKEY	 dbkey;		/* the dbkey of the first rec fragment	*/
	COUNT	 reclen;	/* total reclen accumulated so far	*/
	TEXT	 opstate;	/* RMUNDEL = undoing logical delete	*/
				/* RMRFCR  = roll forward logical create*/
	};

/* The following is protected by the process private latch PL_LOCK_RMUNDO */
LOCAL struct rmundo **	rmundotab = NULL; /* array of undo str pointers */

/* PROGRAM: rmBeginLogicalUndo - begin logical undo phase of crash recovery
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rmBeginLogicalUndo(dsmContext_t *pcontext)
{
    int rmundoTabSize;

    if (rmundotab == (struct rmundo **)NULL)
    {
        /* we need pointers to the undo structures */
        /* We must utmalloc this because it could become > 32k
         * if the -n of the last database session was > 8k (sizeof(*) * 8k)
         */
        rmundoTabSize = sizeof(struct rmundo *) *
                                  pcontext->pdbcontext->ptrantab->maxtrn;
        rmundotab = (struct rmundo **)utmalloc(rmundoTabSize);
        stnclr((TEXT *)rmundotab, rmundoTabSize);
    }
    else
    {
	/* %B rmBeginLogicalUndo: rmundotab already allocated */
	MSGN_CALLBACK(pcontext, rmMSG003); 
    }

}  /* end rmBeginLogicalUndo */


/* PROGRAM: rmEndLogicalUndo - end logical undo phase of crash recovery
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rmEndLogicalUndo(dsmContext_t *pcontext)
{
	struct rmundo	*p;
	int		i;

TRACE_CB(pcontext, "rmEndLogicalUndo")

    if (rmundotab != (struct rmundo **)NULL)
    {
        for (i = 0; i < pcontext->pdbcontext->ptrantab->maxtrn; i++)
	{
	    p = rmundotab[i];
            if (p != (struct rmundo *)NULL)
	    {
                if (p->pbuf != NULL)
                {
                    /* discard partially built up record */

                    stVacate (pcontext, pdsmStoragePool, p->pbuf);
		}
		stVacate (pcontext, pdsmStoragePool, (TEXT *)p);
            }
	}
        utfree((TEXT *)rmundotab);
	rmundotab = NULL;
	pcontext->prmundo = NULL;
    }

}  /* end rmEndLogicalUndo */


/* PROGRAM: rmUndoLogicalCreate - undo a create during logical rollback
 *		This can only occur for the 1st
 *		fragment created by rmCreate
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rmUndoLogicalCreate (
	dsmContext_t	*pcontext,
	rmRecNote_t	*prmrecnt, /* points to logical create note */
	COUNT		 reclen _UNUSED_,  /* length of 1st record fragment */
	TEXT		*precord)  /* points to 1st record fragment */
{
    lockId_t xDbkey;
    LONG     actualTable;
    int      blobSegment;
    
#if SHMEMORY
    int   ret;
#endif

    xDbkey.table = xlng((TEXT *)&prmrecnt->rlnote.rlArea);
    xDbkey.dbkey = xdbkey((TEXT*)&prmrecnt->rlnote.rldbk) + prmrecnt->recnum;

    TRACE2_CB(pcontext, "rmUndoLogicalCreate: dbkey=%D, size=%d",
             xDbkey.dbkey, reclen);


    /* delete the record without locking */
    /* lock rec-get to protect NOLOCK rec get */
    /* record get locks use area plus dbkey rather than table plus dbkey */
    ret =lkrglock(pcontext, &xDbkey,LKEXCL);
    if (ret != 0)
    {
        FATAL_MSGN_CALLBACK(pcontext, rmFTL006, xDbkey, ret );
        return;
    }

    blobSegment = 0; /* init to no segment present */
    /* Check for Blobs and don't extract table number if a blob */
    if ( ( *(recFieldFirst(precord))  == BLOB_RES_1 ) ||
         ( *(recFieldFirst(precord))  == BLOB_RES_2 ) )
        blobSegment = 1;
    else
        actualTable = prmrecnt->table;

    if ( rmDeleteRec(pcontext, actualTable, xDbkey.table, xDbkey.dbkey) != RMOK)
    {
        /* Could not delete record with dbkey %d<DBKEY> in undo. */
        FATAL_MSGN_CALLBACK(pcontext, rmFTL008, xDbkey.dbkey );
    }


    lkrgunlk(pcontext, &xDbkey);
    /* Check for blobs and don't purge locks */
    if ( ! blobSegment )
    {
        xDbkey.table = actualTable;
        if(!(pcontext->pdbcontext->accessEnv & DSM_SQL_ENGINE))
            lkpurge(pcontext, &xDbkey);
    }

}  /* end rmUndoLogicalCreate */


/* PROGRAM: rmUndoLogicalDelete - undo a delete during logical rollback
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rmUndoLogicalDelete (
	dsmContext_t	*pcontext,
	rmRecNote_t	*prmrecnt, /* points to logical delete note */
	COUNT		 reclen,   /* length of record fragment     */
	TEXT		*precord)  /* points to record fragment     */
{
    lockId_t	 lockId;
    ULONG        area;
    unsigned     bufsize;
    TEXT	*plck;
    COUNT        txSlot;
    int          ret;
    struct rmundo *prmundo = pcontext->prmundo;

    txSlot = tmSlot(pcontext, xlng((TEXT*)&prmrecnt->rlnote.rlTrid));

    if( (rmundotab != (struct rmundo **)NULL ) && 
              (pcontext->pdbcontext->prlctl->rlwarmstrt))
    {
	/* Logical undo during crash recovery        */
	rmSetLogicalUndo(pcontext, txSlot );
	prmundo = pcontext->prmundo;
    }
    else
    {
	/* Logical undo during on-line processing    */
	if ( prmundo == (struct rmundo *)NULL )
	{
	     /* Haven't allocated state structure for an undo yet */

	    prmundo = pcontext->prmundo = (struct rmundo *)
		stRent (pcontext, pdsmStoragePool, sizeof (struct rmundo) );
	}
    }


    switch(prmundo->opstate) {
    case 0:
	/* first call, setup to gather fragments backwards */

	if (prmrecnt->flags & SPLITREC)
        {
            FATAL_MSGN_CALLBACK(pcontext, rmFTL009);
        }
#if 0
	if (prmrecnt->rlnote.rllen < sizeof(rmRecNote_t))
	     bufsize = 2048;  /* pre version 5 style notes */
	else 
#endif
        bufsize = prmrecnt->recsz;
	prmundo->pbuf = stRentd(pcontext, pdsmStoragePool, bufsize);
	prmundo->pcur = prmundo->pbuf + bufsize;
	prmundo->opstate = RMUNDEL;
	prmundo->reclen = 0;

	break;
    case RMUNDEL:
	/* subsequent calls, gather more fragments */
	if (!(prmrecnt->flags & SPLITREC))
        {
            FATAL_MSGN_CALLBACK(pcontext, rmFTL005);
        }
	break;
    case RMRFCR:
	FATAL_MSGN_CALLBACK(pcontext, rmFTL010);
    }

    /* copy this fragment into the buffer */
    prmundo->pcur   -= reclen;
    prmundo->reclen += reclen;
    bufcop(prmundo->pcur, precord, reclen);

    /* if this is not the last call, then return and   */
    /* wait for more fragments to be gathered together */
    if (prmrecnt->flags & CONT) return;

    /* if this is the last call, then recreate the record */
    lockId.table = (LONG)prmrecnt->table;
    lockId.dbkey = xdbkey((TEXT*)&prmrecnt->rlnote.rldbk) + prmrecnt->recnum;
    area = xlng((TEXT*)&prmrecnt->rlnote.rlArea);

    lkcheck(pcontext, &lockId, (LCK **)(&plck));    /* fix bug 89-04-12-03 */
    if (!plck)			 /* fix bug 89-04-12-03 */
    {
	lklock(pcontext, &lockId, LKEXCL, PNULL);	/* reacquire the lock */
	lkrels(pcontext, &lockId);
    }
    ret = rmUpdateRecord(pcontext, prmrecnt->table, area,lockId.dbkey,
                         prmundo->pcur, prmundo->reclen);
    if ( ret != 0 )
    {
        /* Undo failed to reproduce the record with dbkey %D<DBKEY> 
                and return code %i<Return Code>. */
        FATAL_MSGN_CALLBACK(pcontext, rmFTL011, lockId.dbkey, ret );
    }

    
    /* return the buffer, and re-init for the next logical undo */
    stVacate(pcontext, pdsmStoragePool, prmundo->pbuf);
    prmundo->pbuf = NULL;
    prmundo->opstate = 0; 
    
}  /* end rmUndoLogicalDelete */


/* PROGRAM: rmUndoLogicalChange -- undo a change during logical rollback
 *		this happens when we undo an update that was done
 *		by a difference note
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rmUndoLogicalChange (
	dsmContext_t	*pcontext,
	rmChangeNote_t	*pchgnote) /* points to logical chg note */
{
    rmDiffList_t *pDifL;         /* points to difference list */
    TEXT         *pOldRec;       /* pointer to record we build from diffs */
    TEXT         *pCurRec;       /* pointer to current record in db */
    COUNT         curSize;       /* size of current record in db */
    COUNT         oldSize;       /* expected size of built record */
    COUNT         retSize;       /* actual size of built record */
    ULONG         area;          /* area of "changed" record  */
    DBKEY         dbkey;         /* dbkey of "changed" record */
    int           ret;
   
    pDifL = (rmDiffList_t  *)((TEXT *)pchgnote + sizeof (rmChangeNote_t));
    area = xlng((TEXT*)&pchgnote->rlnote.rlArea);
    dbkey = xdbkey((TEXT*)&pchgnote->rlnote.rldbk) + pchgnote->recnum;

    /* size of the record we are going to put back */

    oldSize = pchgnote->recsz;
    pOldRec = (TEXT *)stRent (pcontext, pdsmStoragePool, (unsigned)oldSize);

    /* size of the record currently in the database */

    curSize = pchgnote->newsz;
    pCurRec = (TEXT *)stRent (pcontext, pdsmStoragePool, (unsigned)curSize);

    /* get the current record and all its fragments. We do it this way
       because even though it was not a fragmented record when the
       update was made, it might have become fragmented later by undoing
       a subsequent update */

    ret = rmFetchRecord(pcontext, area,dbkey, pCurRec,
                       curSize, 0 /* not continuation */);
    if (ret != (int)curSize)
    {
        FATAL_MSGN_CALLBACK(pcontext, rmFTL016, dbkey, ret, curSize);
    }
    /* reconstruct the orignal record from the diff list */

    retSize = rmDoDif(pcontext, 0, curSize, pCurRec, pDifL, oldSize, pOldRec);
    if (retSize != oldSize)
    {
        FATAL_MSGN_CALLBACK(pcontext, rmFTL017, dbkey, retSize, oldSize);
    }
    /* replace the current record with the original record */

    ret = rmUpdateRecord(pcontext, pchgnote->table, area, dbkey, pOldRec, oldSize);
    if ( ret != 0 )
    {
        /* Undo failed to reproduce the record with dbkey %D<DBKEY> and return code %i<Return Code>. */
	FATAL_MSGN_CALLBACK(pcontext, rmFTL011, dbkey, ret );
    }

    /* free the space we allocated */

    stVacate (pcontext, pdsmStoragePool, pCurRec);
    stVacate (pcontext, pdsmStoragePool, pOldRec);

}  /* end rmUndoLogicalChange */


/* PROGRAM: rmSetLogicalUndo - init for multiple logical undos
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
rmSetLogicalUndo (
	dsmContext_t	*pcontext,
	COUNT		 txSlot)         /* Tx slot */
{

    if (rmundotab == (struct rmundo **)NULL)
    {
	/* rmSetLogicalUndo: rmundotab not allocated    */
	FATAL_MSGN_CALLBACK(pcontext, rmFTL002); 
    }

    /* Get the undo entry address for this transaction. Could be null if
       this is the first one */

    pcontext->prmundo = rmundotab[txSlot];
    if (pcontext->prmundo == (struct rmundo *)NULL)
    {
        /* Haven't allocated state structure for an undo yet */

        pcontext->prmundo = (struct rmundo *)stRent (pcontext, pdsmStoragePool, 
                   sizeof (struct rmundo) );
	rmundotab[txSlot] = pcontext->prmundo;
    }

}  /* end rmSetLogicalUndo */

