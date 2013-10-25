
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
#include "bmpub.h"   /* needed for bmReleaseBuffer & bmLocateBlock calls */
#include "dbcontext.h"   /* needed for  rm[*]Ct & DBSERVER */
#include "dsmpub.h"
#include "cxpub.h"
#include "dbpub.h"
#include "lkpub.h"   /* needed for LKEXCL... */
#include "ompub.h"
#include "recpub.h"
#include "rmpub.h"
#include "rmdoundo.h"
#include "rmprv.h"
#include "rmmsg.h"
#include "objblk.h"
#include "stmpub.h"  /* storage management subsystem */
#include "stmprv.h"  
#include "tmmgr.h"   /* needed for tmdtask call */
#include "usrctl.h"  /* needed for pusrctl */
#include "utspub.h"
#include "uttpub.h"
#include "scasa.h"

#if OPSYS==WIN32API
#include <time.h>
#endif

/*** LOCAL FUNCTION PROTOTYPES ***/

LOCALF int	rmClearDbkeys	(dsmContext_t *pcontext, ULONG area,
				 bmBufHandle_t bufHandle, 
                                 rmBlock_t *pbk, COUNT *pdelRecSize);
LOCALF DBKEY	rmCreateRec	(dsmContext_t *pcontext, ULONG area, LONG table,
                                 TEXT *prec, COUNT size, int cont, 
                                 COUNT *pdelRecSize, UCOUNT lockRequest);
LOCALF int	rmDeleteRecFrags (dsmContext_t *pcontext, dsmTable_t table, ULONG area,
				 DBKEY dbkey, COUNT *pdelRecSize);
LOCALF DSMVOID	rmDeleteRecSpace (dsmContext_t *pcontext,dsmTable_t table,
                                  rmCtl_t *prmctl,
                                  COUNT *pdelRecSize);
LOCALF bmBufHandle_t rmFindSpace (dsmContext_t *pcontext, ULONG area,
				 COUNT size, COUNT *pdelRecSize);
LOCALF int	rmGenDif	(dsmContext_t *pcontext, COUNT oldLen,
				 TEXT *pOldRec, COUNT newLen,
		   	         TEXT *pNewRec, rmDiffList_t *pDifL);
LOCALF int	rmUpdateRec	(dsmContext_t *pcontext, dsmTable_t table, 
				 ULONG area,
				 DBKEY dbkey, TEXT *prec, COUNT size);

LOCALF DBKEY	rmUpdateRecFrags (dsmContext_t *pcontext, dsmTable_t, 
				  ULONG area,
				 DBKEY dbkey, TEXT *prec, COUNT size,
                                 COUNT *pdelRecSize);

LOCALF DSMVOID	rmUpdateRest	(dsmContext_t *pcontext, dsmTable_t table, 
				 rmCtl_t *prmctl,
				 TEXT *prec, COUNT size, int cont,
                                 COUNT *pdelRecSize);

LOCALF bmBufHandle_t rmFindTempSpace(dsmContext_t *pcontext, 
				     dsmObject_t table);


/*** END LOCAL FUNCTION PROTOTYPES ***/

#if RMTRACE
#define MSGD     MSGD_CALLBACK
#endif

#define CREATE_RETRY_LIMIT  3


/* PROGRAM: rmClearDbkeys - local routine to clear defunct dbkeys
 *
 * RETURNS: 0 if directory full and none of the entries
 *	    are placeholders for deleted records
 */
LOCALF int
rmClearDbkeys (
	dsmContext_t	*pcontext,
	ULONG		 area,
	bmBufHandle_t	 bufHandle,
	rmBlock_t	*pbk,
        COUNT           *pdelRecSize)
{
    ULONG   *pdir;
    COUNT    recnum;
    int	     hold = 0;
    rmCtl_t  rmctl;
    COUNT    reclen;    /* length of the record */
    LONG     trannum;   /* transaction number */

    for (recnum = pbk->numdir-1, pdir = pbk->dir+recnum;
	 recnum>=0;
	 pdir--, recnum--)
    {
	if (*pdir & HOLD)
	{
            /* get the length of the record and the transaction number */

            reclen = xct((TEXT *)pbk +(*pdir & RMOFFSET));
	    trannum = (LONG)xlng64((TEXT *)pbk +(*pdir & RMOFFSET) + 
				   sizeof(COUNT));
	    
            /* if the record length doesn't match a hold note length
               or if the transcation number is < 0, this isn't a hold
               note */

            if (reclen != sizeof(DBKEY))
            {
#if RMTRACE
                 MSGD(pcontext, "%LHold note %d has an invalid length %d",
                       pbk->rm_bkhdr.bk_dbkey+recnum,reclen);
#endif
                ;
            }
            else if (trannum < 0)
            {
#if RMTRACE
                 MSGD(pcontext,
                       "%LHold note %d has an invalid transaction number %d",
                       pbk->rm_bkhdr.bk_dbkey+recnum,trannum);
#endif
                ;
            }
            else
            {
	        /* This entry is a placeholder for a deleted record */

	        if (cxIxrprGet() || tmdtask(pcontext, trannum))
	        {
		    /* transaction finished, it can be deleted */
	            rmRCtl(pcontext, pbk, recnum, &rmctl);
		    rmctl.area = area;
                    rmctl.bufHandle = bufHandle;
	            rmDeleteRecSpace(pcontext, 0, &rmctl, pdelRecSize);
                    STATINC (rmhnCt);
	        }
	        else
		    hold = 1;
	    }
	}
    }

    /* check for some free directories and no placeholders left */
    if ((pbk->freedir > 0) || hold)
    {
       /* block has at a potential directory slot */
       return 1;
    }

    /* block is packed full of records */
    return 0;

}  /* end rmClearDbkeys */


/* PROGRAM: rmDelete - deletes a record
 *
 * RETURNS: status from rmDeleteRec:
 *		RMOK if successful
 *	        DSM_S_RMNOTFND if the record is not found
 *          status from lklock if all the locks are free
 *          error status from lkrglock if can't record-get lock fails
 *	    DSM_S_CTRLC if re-sync'ing or lkservcon
 */
int
rmDelete (
	dsmContext_t	*pcontext,
	LONG		 table,
	DBKEY		 dbkey,
	TEXT		*pname) /* refname for Rec in use message,
                                 * child stub only */
{
    int	ret;
    ULONG       area;   /* storage area the record is in     */
    lockId_t    lockId; /* identifier for locking the record */
	
    
    TRACE2_CB(pcontext, "rmDelete: area=%d dbkey=%D", area, dbkey)

    /* determine area for table  */
    ret = omIdToArea( pcontext, DSMOBJECT_MIXTABLE, 
			    table, table, &area );
    if( ret )
    {
	goto retrn1;
    }
    
    lockId.table = table;
    lockId.dbkey = dbkey;

    if (pcontext->pdbcontext->resyncing)
        return DSM_S_CTRLC; /*selfserve only, resync in progress*/
    if (lkservcon(pcontext)) return DSM_S_CTRLC;
	
    /* lock the record (LKEXCL) FIXFIX: we should not have to do this. the
       record should be locked already */
	
    ret = lklock(pcontext, &lockId, LKEXCL, pname);
    if (ret) return ret; /* all the locks are free */
	
    /* install a record-get lock to prevent anyone else from fiddling with
       the record while we delete it */
    /* record get locks lock area + recid   */
    lockId.table = area;
    ret = lkrglock (pcontext, &lockId,LKEXCL);
    if (ret != 0)
    {
	goto retrn1;
    }


    STATINC (rmdlCt);
    ret = rmDeleteRec (pcontext, table, area, dbkey);
	

    /* release the record get lock */
    lkrgunlk (pcontext, &lockId);

    lockId.table = table;
    lkpurge(pcontext, &lockId);
    lkrels(pcontext, &lockId);


retrn1:


    return ret;
}  /* end rmDelete */


/* PROGRAM: rmDeleteRec - to deletes a record
 *
 * RETURNS: RMOK if successful
 *	    DSM_S_RMNOTFND if the record is not found
 */
int
rmDeleteRec (
	dsmContext_t	*pcontext,
        dsmTable_t       table,
	ULONG		 area,
	DBKEY		 dbkey)
{
    TEXT	 ctask[sizeof(DBKEY)];	/* this is the hold record */
    rmCtl_t	 rmctl;
    COUNT    delRecSize;

    TRACE1_CB(pcontext, "rmDeleteRec: dbkey=%D", dbkey)

    rlTXElock (pcontext, RL_TXE_UPDATE, RL_RECORD_DELETE_OP);

    /* find the record to be deleted */
    if (rmLocate (pcontext, area, dbkey, &rmctl, TOMODIFY))
    {
	bmReleaseBuffer(pcontext, rmctl.bufHandle);

        rlTXEunlock (pcontext);

	MSGN_CALLBACK(pcontext, rmMSG001, dbkey);
	return DSM_S_RMNOTFND;
    }



    delRecSize = 0; /* initialize the cumulative length of deleted fragments */


    /* build a dbkey place holder record containing the xaction number */
    slng64 (ctask, (LONG64)tmctask(pcontext));

    /* delete the first fragment of the record          */
    rmDeleteRecSpace (pcontext, table, &rmctl, &delRecSize);
    /* put in a placeholder and delete the rest */
    rmUpdateRest (pcontext, table, &rmctl, ctask, (COUNT)sizeof(ctask),
                  HOLD, &delRecSize);

    rlTXEunlock (pcontext);

    return(RMOK);

}  /* end rmDeleteRec */


/* PROGRAM: rmDeleteRecFrags - delete all the fragments of a record
 *
 * RETURNS: RMOK if successful
 *	    DSM_S_RMNOTFND if can't find the record fragment
 */
LOCALF int
rmDeleteRecFrags (
	dsmContext_t	*pcontext,
        dsmTable_t       table,
	ULONG		 area,
	DBKEY		 dbkey,
        COUNT           *pdelRecSize)
{
    rmCtl_t	 rmctl;

    TRACE1_CB(pcontext, "rmDeleteRecFrags: dbkey=%D", dbkey)

    while (dbkey)
    {
	/* get and lock the block containing the record fragment */

	if (rmLocate(pcontext, area, dbkey, &rmctl, TOMODIFY))
	{
	    /* cant find the fragment */

	    MSGN_CALLBACK(pcontext, rmMSG001, dbkey);
            bmReleaseBuffer(pcontext, rmctl.bufHandle);
	    return (DSM_S_RMNOTFND);
	}
	/* delete it */

	rmDeleteRecSpace (pcontext, table, &rmctl, pdelRecSize);

	/* put block on rm chain if necessary */

	rmFree (pcontext, rmctl.area, rmctl.bufHandle, rmctl.pbk, pdelRecSize);

	/* unlock the block */

        bmReleaseBuffer(pcontext, rmctl.bufHandle);

	/* the next fragment */

	dbkey = rmctl.nxtfrag;
    }
    return (RMOK);

}  /* end rmDeleteRecFrags */


/* PROGRAM: rmFitCheck - determine how much of a record to put in the chosen blk
 *
 * RETURNS: how much of the record fits (in bytes)
 *	    0 if none of it fits
 */

int
rmFitCheck (
	dsmContext_t	*pcontext _UNUSED_,
	rmBlock_t	*pbk,	 /* the record will be put in this block */
	COUNT		 size,	 /* the length of the record	       */
	COUNT		 recnum) /* the record will have this record # */
{
    LONG	 overhead;

    overhead = sizeof(COUNT);	/* reserve 2 bytes for the length prefix */

    /* reserve room if a new directory entry will be made */

    if (recnum >= pbk->numdir)
	overhead += (recnum-pbk->numdir+1) * sizeof(pbk->dir[0]);

    /* if it fits, then put it all in the block */

    if (overhead+size <= pbk->free) return size;

    /* if it will be split, there is additional overhead */

    overhead += sizeof(DBKEY) + SPLITSAVE;

    if (overhead >= pbk->free)
	 return 0;		/* none of the record fits */
    else return pbk->free - overhead;

}  /* end rmFitCheck */


/* PROGRAM: rmFormat - formats blk for rm. 
 *	    Called by bkfrrem when getting a new block for rm
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rmFormat (
	dsmContext_t	*pcontext,
	BKBUF		*pbk,
        dsmArea_t	 areaNumber)
{
    dbshm_t *pdbpub = pcontext->pdbcontext->pdbpub;

    ((rmBlock_t *) pbk) -> numdir = 0;
    ((rmBlock_t *) pbk) -> freedir = RECSPERBLK(BKGET_AREARECBITS(areaNumber));
    ((rmBlock_t *) pbk) -> free = pdbpub->dbblksize - SZBLOCK;
}  /* end rmFormat */


/* PROGRAM: rmFindSpace - find space in a block for a record
 *
 * This function finds free space in the database so that it can be allocated
 * to store records.  It will fragment a record if it will not fit in a
 * single block, and if less than a complete block is needed, it will try to
 * find it on the RM chain.   As the RM chain is examined, cleanup work is
 * done on directories and blocks with no space popped off the chain.
 *
 * The method used is summarized as follows:
 *
 *     look at the RM chain in a loop until a limit is reached or space found
 *           if block is not RM block or on RM chain ignore it (limit 20 trys)
 *           if a block with enough space is found use it
 *           pop blocks with no useful space off chain (limit 100 trys)
 *           push block with too little space to bottom of chain (limit 3 trys)
 *     if no RM block is found get a free block (extend db if necessary).
 *
 * RETURNS: pointer to block with free space.
 */
LOCALF bmBufHandle_t 
rmFindSpace(
	dsmContext_t	*pcontext,
	ULONG		 area,    /* Area to find space in         */
	COUNT		 size,	 /* number of bytes needed */
        COUNT           *pdelRecSize)
{
    dbshm_t      *pdbshm = pcontext->pdbcontext->pdbpub;
    rmBlock_t    *pbk;	  	/* ptr to block	*/
    DBKEY         dbkey;	/* dbkey of this block */
    int	          mod;          /* modulo of full blocks */
    LONG          free;         /* free space working value */
    LONG          rmcnt;        /* number of rm blocks on chain */
    LONG          trycnt;       /* remaining retry attempts */
    LONG          pushcnt;      /* remaining push attempts */
    LONG          popcnt;       /* remaining pop attempts */
    bmBufHandle_t bufHandle;    /* Handle to block in the buffer pool */
    COUNT         areaRecbits;
    LONG          recsperblk;
    bkAreaDesc_t *pbkAreaDesc;

    pbkAreaDesc = BK_GET_AREA_DESC(pcontext->pdbcontext, area);

    areaRecbits = pbkAreaDesc->bkAreaRecbits;
    recsperblk  = RECSPERBLK(areaRecbits);

    /* count number and size of space allocation requests */
    STATINC(rmalCt);
    STATADD(rmbaCt, (ULONG)size);

    stnclr(&bufHandle, sizeof(bmBufHandle_t));

    /* set limits for searching RM chain */
    trycnt  = 20;
    pushcnt = 3;
    popcnt  = 100;

    /* initialize block pointer */
    pbk = (rmBlock_t *)NULL;

    /* find modulo of full blocks, to use first */
    for (mod = size + sizeof (COUNT);
	 mod > (int)(pdbshm->dbblksize - (SZBLOCK + sizeof(DBKEY) + SPLITSAVE));
	 mod -= (pdbshm->dbblksize - (SZBLOCK + sizeof(DBKEY) + SPLITSAVE)));

    while (trycnt > 0 && pushcnt > 0 && popcnt > 0)
    {
	/* get the front of the RM chain, and count */
        dbkey = pbkAreaDesc->bkChainFirst[RMCHN];
	rmcnt = pbkAreaDesc->bkNumBlocksOnChain[RMCHN];

	/* determine if RM chain is empty */
	if (dbkey == 0 || rmcnt == 0)
	{
            /* RM chain is empty, give up and look for free block */
	    break;
	}

	/* exclusive lock the block on front of RM chain */
	bufHandle = bmLocateBlock(pcontext, area, dbkey, TOMODIFY);	
	pbk = (rmBlock_t *)bmGetBlockPointer(pcontext,bufHandle);
	/* count blocks examined */
        STATINC(rmbsCt);

	/* make sure this is RM block and still on RM chain */
	if ((pbk->rm_bkhdr.bk_type!=RMBLK) || (pbk->rm_bkhdr.bk_frchn!=RMCHN))
	{
	    /* decrement retry limit and try again */
            trycnt--;
	    goto retry;
	}

        /* clear directory entries, check for available slots */
	if (rmClearDbkeys(pcontext, area, bufHandle, pbk, pdelRecSize))
	{
	    /* directory not full, check if any on HOLD */
	    if (pbk->freedir == 0)
	    {
		/* directory full w/ HOLDs, push block to end of RM chain */
		goto push;
	    }
	}
	else
	{
	    /* directory is completely full, pop this block off chain  */
	    goto pop;
	}

	/* candidate block with free directories - get free space in block */
	free = pbk->free;

	/* check if block is completely empty */
	if (free == (int)(pdbshm->dbblksize - SZBLOCK))
	{
	    /* this block is completely empty RM block so take it */
	    STATINC (rmusCt);
	    break;
	}

	/* check if enough free space to allow creates */
	if (free < pdbshm->dbCreateLim)
	{
	    /* less than minimum allowed space, pop this block off chain */
	    goto pop;
	}

	/*  does the record (or mod) fits in block making sure
         to leave at least dbCreateLim free bytes in the block after
         we have put the record in it. */
	if (mod <= (free - pdbshm->dbCreateLim))
	{
	    /* enough to use, take this block */
	    STATINC (rmusCt);
	    break;
	}
	else
	{
	    /* not enough to use, check if worth keeping on chain */
	    if (free <= pdbshm->dbTossLim) 
	    {
		/* not worth keeping unclog chain, pop block off RM chain */
		goto pop;
	    }
	    else
	    {
		/* worth keeping, push block to end of RM chain */
		goto push;
	    }
	}

      push:
	/* push block to end of RM chain */
	bktoend(pcontext, bufHandle);
	
	/* check count in RM chain and adjust limit if needed */
        if (rmcnt < pushcnt)
	{
	    /* set push limit to number of blocks on chain */
	    pushcnt = rmcnt;
	}

	/* decrement the push limit and retry */
	pushcnt--;
	goto retry;

      pop:
	/* pop this block off chain  */
	bkrmrem (pcontext, bufHandle);

	/* decrement the pop limit and retry */
	popcnt--;

      retry:
	/* release the current block, clear pointer */
	bmReleaseBuffer(pcontext, bufHandle);
	pbk = (rmBlock_t *)NULL;
	
    } /* while limits ok */

    /* determine if a useful block was found on chain */
    if (pbk == (rmBlock_t *)NULL)
    {
	/* no block found on RM chain, get a free block */
	bufHandle = bkfrrem(pcontext, area, RMBLK, FREECHN);
    }

    /* return the block found */
    return(bufHandle);

}  /* end rmFindSpace */


/* PROGRAM: rmFree - manages free list and release block
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rmFree (
	dsmContext_t	*pcontext,
	ULONG		 area,             /* area that block is in           */
        bmBufHandle_t	 bufHandle, /* buffer mgr handle to buffer containing
				    the rm block                   */
	rmBlock_t	*pbk,		/* pointer to rm block of interest */
        COUNT           *pdelRecSize)
{
    dbshm_t *pdbshm = pcontext->pdbcontext->pdbpub;

    TRACE_CB(pcontext, "rmFree")

    /* clear directory entries and place holders to recover space. */

    if (!rmClearDbkeys(pcontext, area, bufHandle, pbk, pdelRecSize))
    {
        /* dir is still full and there were no placeholders */

	return;
    }
    if(area == DSMAREA_TEMP)
      return;

    if (pbk->rm_bkhdr.bk_frchn != NOCHN)
    {
        /* block is already on the rm free chain */

	return;
    }
    if (pbk->free <= pdbshm->dbTossLim)
    {
	/* not enough free space */

	return;
    }
    
    if (pbk->freedir > 4)
    {
        /* put it on the front of rm free chain */
 
        bkfradd(pcontext, bufHandle, RMCHN);
    }
    else
    {
        /* put it on the back of rm free chain */
 
        bkrmbot(pcontext, bufHandle);
    }

}  /* end rmFree */


/* PROGRAM: rmGenDif - Generate a difference list between two byte strings.
 *
 *	Generate a difference list between two byte strings.
 *	Returned value is the length of the difference record
 *
 *	FIXFIX: As a first cut, we scan the records from both ends
 *	to find a part which is changed. This is returned as the
 *	sole difference entry. If the last field of a record is changed,
 *	the entire record may be considered different because the
 *	field skip table may change.
 *
 *	A better way to do this might be to do the difference list on a
 *	field by field basis.
 *
 * RETURNS: the length of the difference record
 */
LOCALF int
rmGenDif (
	dsmContext_t	*pcontext _UNUSED_,
	COUNT		oldLen,		/* length of old record */
	TEXT		*pOldRec,	/* pointer to old record */
	COUNT		newLen,		/* length of new record */
	TEXT		*pNewRec,	/* pointer to new record */
	rmDiffList_t	*pDifL)		/* where to put difference list */
{
	TEXT		*pBegO;
	TEXT		*pEndO;
	TEXT		*pBegN;
	TEXT		*pEndN;
	TEXT		*p;
	COUNT		 len;
	COUNT		 minlen;
	COUNT		 oldDifLen;
	COUNT		 newDifLen;
	COUNT		 listLen;
	rmDiffEntry_t	*pDifE;

    pBegO = pOldRec;
    pEndO = pOldRec + oldLen;
    pBegN = pNewRec;
    pEndN = pNewRec + newLen;

    pDifL->numDifs = 0;
    listLen = sizeof (rmDiffList_t);
    pDifE = (rmDiffEntry_t *)((TEXT *)pDifL + sizeof (rmDiffList_t));

    if (oldLen < newLen) minlen = oldLen;
    else minlen = newLen;

    if (minlen == 0)
    {
	if ((oldLen == 0) && (newLen == 0)) return (0);
    }
    /* compare forward from beginning of both records */

    len = minlen;
    while (--len >= 0)
    {
	if (*pBegO++ == *pBegN++) continue;

	/* records differ starting here */

        pBegO--;
        pBegN--;
	len++;
	break;
    }
    /* pBegO and pBegN point to the first byte which is different.
       Note that this may be one byte past the end of either or both
       records if they are identical or the shorter matches the first
       part of the longer. */

    if ((pBegO < pEndO) && (pBegN < pEndN))
    {
	/* Find how much of the records match from the end backward */

        while (--len >= 0)
        {
            if (*(--pEndO) == *(--pEndN)) continue;

	    /* records match from here to the end */

            pEndO++;
            pEndN++;
	    break;
        }
    }
    /* pEndO and pEndN point to the last byte which is the same. If the
       final byte of the records differ, then they point one beyond the
       last byte of each record. */

    oldDifLen = pEndO - pBegO;
    newDifLen = pEndN - pBegN;

    /* fill in the difference entry */

    pDifL->numDifs++;
    listLen = listLen + newDifLen + oldDifLen + sizeof (rmDiffEntry_t);

    pDifE->oldLen = oldDifLen;
    pDifE->newLen = newDifLen;
    pDifE->oldOffset = pBegO - pOldRec;
    pDifE->newOffset = pBegN - pNewRec;

    p = (TEXT *)pDifE + sizeof (rmDiffEntry_t);

    if (oldDifLen > 0)
    {
	/* copy the data from the OLD record to the difference entry */

	bufcop (p, pBegO, (int)oldDifLen);
	p = p + oldDifLen;
    }
    if (newDifLen > 0)
    {
	/* copy the data from the NEW record to the difference entry */

	bufcop (p, pBegN, (int)newDifLen);
	p = p + newDifLen;
    }
    /* update the pointer to the next difference entry */

    pDifE = (rmDiffEntry_t *)((TEXT *)pDifE + oldDifLen + newDifLen +
			     sizeof (rmDiffEntry_t));
    
    return (listLen);

}  /* end rmGenDif */


/* PROGRAM: rmFetchDir - local routine to get directory entry
 *
 * RETURNS: pointer to directory entry
 */
int
rmFetchDir (
	dsmContext_t	*pcontext,
	rmBlock_t	*pbk,
	dsmArea_t	 areaNumber)
{
    ULONG	*pdir;

    TRACE_CB(pcontext, "rmFetchDir")

    /* if all the allocated directories are busy, then use the	*/
    /* next unallocated directory				*/

    if (pbk->numdir + pbk->freedir == RECSPERBLK(BKGET_AREARECBITS(areaNumber)))
	return pbk->numdir;

    /* there is an embedded free directory, find it and return it */

    for(pdir = pbk->dir; *pdir; pdir++);
    return (int) (pdir - pbk->dir);

}  /* end rmFetchDir */


/* PROGRAM: rmFetchRecord - retrieve a record
 *
 *	recursive function to get the record.  Recurses until DSM_S_RMNOTFND
 *
 * RETURNS: recsize if successful
 *	    DSM_S_RMNOTFND if record not found
 */
int
rmFetchRecord (
	dsmContext_t	*pcontext,
	ULONG		 area,          /* area the record is in    */
	DBKEY		 dbkey,		/* dbkey of record to retrieve */
	TEXT		*pto,		/* target area */
	COUNT		 maxsize,	/* space allocated */
	int		 cont)		/* looking for continuation */
{
    rmCtl_t	 rmctl;
    COUNT	 ret;

    TRACE1_CB(pcontext, "rmFetchRecord: dbkey=%D", dbkey)

#if SHMEMORY
    /**********TEMPORARY
    sltrace("rmFetchRecord",0 );
    *****/
#endif

    if (rmLocate(pcontext, area, dbkey, &rmctl, TOREAD) || 
	rmctl.holdtask || ((ULONG)cont != rmctl.cont))
    {
	/* Release the buffer for others to use */
    	bmReleaseBuffer(pcontext, rmctl.bufHandle);
	return DSM_S_RMNOTFND;
    }
    if (rmctl.recsize < 0) /* condition arises with bad recid */
    {
	/* Release the buffer for others to use */
        bmReleaseBuffer(pcontext, rmctl.bufHandle);
        return DSM_S_RMNOTFND;
    }
    if ((ret = maxsize - rmctl.recsize) >= 0)
    {
	if( !cont )
	{
	    STATINC (rmgtCt);
	}
	/* copy the record fragment */

        STATADD (rmbrCt, (ULONG)(rmctl.recsize));
        STATINC (rmfrCt);

	bufcop (pto, rmctl.prec, (int) rmctl.recsize);
    }
    /* Release the buffer for others to use */
    bmReleaseBuffer(pcontext, rmctl.bufHandle);

    if (rmctl.nxtfrag == (DBKEY)0) return rmctl.recsize;

    /* get the remaining fragment(s) */

    ret = rmFetchRecord (pcontext, rmctl.area, rmctl.nxtfrag, 
		      pto + rmctl.recsize, ret, CONT);
    if (ret == DSM_S_RMNOTFND)
    {
	MSGN_CALLBACK(pcontext, rmMSG001, rmctl.nxtfrag);
        if( !cont )
        {
            /* Report the recid of the beginning of the record -
               this is what "proutil <db> -C dbrpr" needs to get
               rid of the record.                              */
	    /*%BInvalid record with recid %D.*/
	    MSGN_CALLBACK(pcontext, rmMSG004,dbkey);
        }
 	return DSM_S_RMNOTFND;
    }
    return rmctl.recsize + ret;

}  /* end rmFetchRecord */


/* PROGRAM: rmLocate - locates an existing record
 *
 * RETURNS: RMOK if successful
 *	    DSM_S_RMNOTFND if no block is located
 */
int
rmLocate (
	dsmContext_t	*pcontext,
	ULONG		 area,		/* area the record is in    */
	DBKEY		 dbkey,		/* dbkey to be found */
	rmCtl_t		*prmctl,	/* structure to be filled in */
	int		 action)	/* TOMODIFY - if intend to modify
			   	 	   TOREAD - if intend for read only */
{
    bmBufHandle_t bufHandle;
    rmBlock_t	 *pbk;    /* ptr to block */
    COUNT	  recnum; /* record number within the block */
    dsmRecbits_t  areaRecbits;
    int		  recmask;
    bkAreaDesc_t *pbkAreaDesc;

    pbkAreaDesc = BK_GET_AREA_DESC(pcontext->pdbcontext, area);
    areaRecbits = pbkAreaDesc->bkAreaRecbits;
    recmask     = RECMASK(areaRecbits);

    TRACE1_CB(pcontext, "rmLocate: dbkey=%D", dbkey)

    if ((dbkey < BLK1DBKEY(areaRecbits)) ||
      (dbkey >= ((DBKEY)(pbkAreaDesc->bkHiWaterBlock + 1) << areaRecbits)) )
    {
        /* No block is located, for bkrlsbuf */
        prmctl->bufHandle = 0;
        prmctl->pbk = NULL;
        return DSM_S_RMNOTFND;
    }

    recnum = countl((LONG) (dbkey & recmask));
    bufHandle = bmLocateBlock(pcontext, area, dbkey & (~ recmask), action);
    pbk = (rmBlock_t *)bmGetBlockPointer(pcontext,bufHandle);
    if (pbk->rm_bkhdr.bk_dbkey != (dbkey & (~ recmask)))
    {
        MSGD_CALLBACK(pcontext, 
             (TEXT *)"%G Wrong db-Block %l vs. %l (DbKey: %l)(rmFTL014)",
             (LONG)pbk->rm_bkhdr.bk_dbkey, (LONG)(dbkey & (~ recmask)),
             (LONG)dbkey);
/*        FATAL_MSGN_CALLBACK(pcontext, rmFTL014); / * bad dbkey */
    }
   
    prmctl->bufHandle = bufHandle;
    prmctl->area = area;
    prmctl->pbk = pbk;

    if (pbk->rm_bkhdr.bk_type != RMBLK) return DSM_S_RMNOTFND;

    if ((recnum >= pbk->numdir) || (pbk->dir[recnum] == 0) )
	return DSM_S_RMNOTFND;

    /* we've got the block, fill in rmctl */
    rmRCtl(pcontext, pbk, recnum, prmctl);
    return(RMOK);

}  /* end rmLocate */


/* PROGRAM: rmCreate - creates a new record
 *
 * RETURNS: status code received from rmCreateRec:
 *		DBKEY of new record created.
 */
DBKEY
rmCreate (
        dsmContext_t    *pcontext,
        ULONG            area,          /* Storate area table is in */
        LONG             table,         /* table record is in.       */
        TEXT            *prec,          /* ptr to record template */
        COUNT            size,          /* size of record template */
        UCOUNT           lockRequest)   /* IN/request record lock */
{

    DBKEY      ret;
    int        retries;
    COUNT      delRecSize;

    TRACE_CB(pcontext, "rmCreate")

    if (size > MAXRECSZ) return DSM_S_RECTOOBIG;

#if SHMEMORY
    if (lkservcon(pcontext)) return DSM_S_CTRLC;
    if (pcontext->pdbcontext->resyncing) /* resyncing in progress (self-serve only) */
        FATAL_MSGN_CALLBACK(pcontext, rmFTL015); /* rmCreate during resyncing */
#endif


    STATINC (rmmkCt);
    
    delRecSize = 0;

    for( retries = 0; ; retries++)
    {
        rlTXElock (pcontext,RL_TXE_SHARE,RL_RECORD_CREATE_OP);
	    
	/* create the record */
	    
        if ( lockRequest )
            ret = rmCreateRec (pcontext, area, table, prec,
                               size, 0, &delRecSize, (UCOUNT)RM_LOCK_NEEDED);
        else
            /* We never lock blob records */
            ret = rmCreateRec (pcontext, area, table, prec,
                               size, 0, &delRecSize,
                              (UCOUNT)RM_LOCK_NOT_NEEDED);

	if( ret > 0 )
	{
	    /* Got a record cause we got a Recid   */
            /* Update the row count    */
            /* Don't increase row count if creating a blob */
            if (lockRequest)
                bkRowCountUpdate(pcontext,area,table, 1);
	    rlTXEunlock(pcontext);
	    break;
	}
	    
	rlTXEunlock (pcontext);
	    
	if ( ret == DSM_S_RQSTREJ )
	{
            if( retries >= CREATE_RETRY_LIMIT )
            {
                /* Couldn't create record                   */
                return ret;
            }       
	    /* A find by recid can have a lock on non-existent
	       record -- only long enough to find out that
	       record doesn't exist.  So we'll sleep for second
	       and hope by then that the other guy has released
	       his lock after finding out the record doesn't exist */
	    utsleep( 1 );
	}
	else if ( ret == DSM_S_LKTBFULL) 
	{
	    return ret;
	}
	else if( ret == DSM_S_CTRLC )
	{
	    return ret;
	}

	else
	{
	    /* Shut down this user - unexpected return from rmCreateRec */
	    /* rmCreate: unexpected return code from rmCreateRec %l */
	    FATAL_MSGN_CALLBACK(pcontext, rmFTL004,ret);
	}
    }
    
    return ret;

}  /* end rmCreate */


/* PROGRAM: rmRCtl - Given the block containing a record, fill in rmctl
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rmRCtl (
	dsmContext_t	*pcontext _UNUSED_,
	rmBlock_t	*pbk,		/* ptr to block */
	COUNT		 recnum,	/* record number within the block */
	rmCtl_t		*prmctl)	/* structure to be filled in */
{
    ULONG	 ofst;	/* directory entry			*/
    TEXT	*prec;	/* points to record in the block	*/

    ofst = pbk->dir[recnum];
    prmctl->offset = ofst & RMOFFSET;
    prmctl->flags  = ofst & ~RMOFFSET;
    prmctl->pbk = pbk;
    prmctl->recnum = recnum;
    prmctl->cont = ofst & CONT;
    prec = (TEXT *)pbk + prmctl->offset;

    /* if the record is split, get the dbkey of the next fragment */
    if (ofst & SPLITREC)
    {	prmctl->nxtfrag = xlng64(prec);
	prec += sizeof(DBKEY);		/* skip past the next pointer */
    }
    else prmctl->nxtfrag = 0;

    prmctl->recsize = xct(prec);   /* first 2 bytes of record are length	*/
    prec += sizeof(COUNT);	  /* skip past the length prefix	*/
    prmctl->prec = prec;

    prmctl->holdtask = ofst & HOLD ? xlng64(prec) : 0;

    prmctl->space = prmctl->recsize + sizeof(COUNT);
    if (ofst & SPLITREC) prmctl->space += sizeof(DBKEY);
    else
    if ((ULONG)prmctl->space < MINRSIZE+sizeof(pbk->dir[0]))
	    prmctl->space = MINRSIZE+sizeof(pbk->dir[0]);

}  /* end rmRCtl */


/* PROGRAM: rmCreateRec - creates a record in a block
 *
 * RETURNS: dbkey + recnum if successful
 *	    error status from lklocky if lock not available
 */

LOCALF DBKEY
rmCreateRec (
	dsmContext_t	*pcontext,
	ULONG		 area,           /* Area table is in            */
        LONG             table,          /* table record is in          */
	TEXT		*prec,		/* prt to record (fragment)	*/
	COUNT		 size,		/* size of the record		*/
	int		 cont,		/* this is continuation		*/
        COUNT           *pdelRecSize,
        UCOUNT           lockRequest)   /* lock request on create       */
{
    rmRecNote_t  rmrecnt;	/* rl log file note for this rec frgmnt	*/
    rmBlock_t    *pbk;  	/* ptr to block				*/
    DBKEY	 dbkey; 	/* dbkey of this block			*/
    COUNT	 recnum;	/* record number within block		*/
    COUNT	 fraglen;	/* amount of rec to put in this block	*/
    rmNextFragNote_t   rmnxtfnt;
    int          rc = 0;
    bmBufHandle_t bmBufHandle;
    
    lockId_t     lockId;
    dbshm_t *pdbshm = pcontext->pdbcontext->pdbpub;

    TRACE2_CB(pcontext, "rmCreateRec: prec=%p, size=%d", prec, size);

    /* pick a block with the right amount of free space in it. The block
       chosen will be exclusive locked */

    if(SCTBL_IS_TEMP_OBJECT(table))
      bmBufHandle = rmFindTempSpace(pcontext,table);
    else
      bmBufHandle = rmFindSpace (pcontext, area, 
				 (COUNT)(size + sizeof (COUNT)),
				 pdelRecSize);
    pbk = (rmBlock_t *)bmGetBlockPointer(pcontext,bmBufHandle);
    dbkey  = pbk->rm_bkhdr.bk_dbkey;


    /* pick a directory entry for the record */

    recnum = rmFetchDir (pcontext, pbk, area);
    lockId.table = table;
    lockId.dbkey = dbkey + recnum;

    /* determine how much of the record fits */

    fraglen = rmFitCheck (pcontext, pbk, size, recnum);

    /* If its the first fragment of a record create
       then lock the record                         */
    if( ( !cont ) && ( lockRequest ) )
    {
	rc = lklocky(pcontext, &lockId, (LKEXCL | LKNOWAIT) ,LK_RECLK,
		     (TEXT *)NULL, (LKSTAT *)NULL );
	if ( rc )
	{
	    bmReleaseBuffer(pcontext, bmBufHandle );
	    return (DBKEY)rc;
	}
        if (size > fraglen)
        {
            /* not all of created record will fit in one block
               so upgrade the txe lock if space chain manipulation
               did not already cause it to happen.                */
            if(pcontext->pusrctl->uc_txelk == RL_TXE_SHARE)
                rlTXElock(pcontext,RL_TXE_UPDATE,RL_RECORD_CREATE_OP);
        }
    }

    /* build the recovery log file note */

    rmrecnt.flags = cont;
    rmrecnt.table = table;
    if (fraglen < size) rmrecnt.flags |= SPLITREC;
    if (!cont && !lockRequest) rmrecnt.flags |= RMBLOB;
    rmrecnt.recsz = size;

    rmrecnt.recnum = recnum;
    rmrecnt.nxtfrag = 0; /* even if split rec, we dont know next dbk yet */
    rmrecnt.rlnote.rllen  = sizeof(rmrecnt);
    rmrecnt.rlnote.rlcode = RL_RMCR;

    /* RL_LOGBI only for 1st fragment in rmCreate */

    sct((TEXT *)&rmrecnt.rlnote.rlflgtrn,  (COUNT)(cont ? RL_PHY
				  : RL_PHY + RL_LOGBI));

    /* write the note and do the operation */

    rlLogAndDo (pcontext, (RLNOTE *)&rmrecnt, bmBufHandle, fraglen, prec);

    if ((dbkey == pcontext->pusrctl->uc_rmdbkey) && (pbk->rm_bkhdr.bk_frchn == NOCHN))
    {
        /* forget this block if there is no more room */

	if (pbk->free < pdbshm->dbCreateLim) pcontext->pusrctl->uc_rmdbkey = 0;
    }
    rmFree (pcontext, area, bmBufHandle, pbk, pdelRecSize);

    /* release the exclusive lock on the block */

    bmReleaseBuffer(pcontext, bmBufHandle );

    if (fraglen != size)
    {
        /* Do a continuation since the record did not fit in the block
	   we put it in. */

        /* create the next fragment */

        rmnxtfnt.nxtfrag = 
	    rmCreateRec (pcontext, area, table, prec+fraglen,
			 (COUNT)(size-fraglen), CONT, pdelRecSize,
                         (UCOUNT)RM_LOCK_NOT_NEEDED);

        /* put the forward pointer in the previous piece */

        rmnxtfnt.prevval = 0;
        rmnxtfnt.recnum = recnum;
        rmnxtfnt.rlnote.rllen  = sizeof(rmnxtfnt);
        rmnxtfnt.rlnote.rlcode = RL_RMNXTF;
        sct((TEXT *)&rmnxtfnt.rlnote.rlflgtrn,  RL_PHY);

        /* lock the previous piece again */

	bmBufHandle = bmLocateBlock(pcontext, area, dbkey, TOMODIFY);
        pbk = (rmBlock_t *)bmGetBlockPointer (pcontext,bmBufHandle);

        /* stick in the next fragment pointer */

        rlLogAndDo (pcontext, (RLNOTE *)&rmnxtfnt, bmBufHandle,
                    (COUNT)0, (TEXT *)PNULL);

        bmReleaseBuffer(pcontext, bmBufHandle);
    }
    return lockId.dbkey;

}  /* end rmCreateRec */


/* PROGRAM: rmDeleteRecSpace - local routine to delete record space within block
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
rmDeleteRecSpace (
	dsmContext_t	*pcontext,
        dsmTable_t       table,
	rmCtl_t		*prmctl,
        COUNT           *pdelRecSize)
{
	rmRecNote_t  rmrecnt;

    TRACE3_CB(pcontext, "rmDeleteRecSpace: offset=%d, recsize=%d, space=%d",
	    prmctl->offset, prmctl->recsize, prmctl->space)

    /* build a rl file note */

    rmrecnt.rlnote.rlcode  = RL_RMDEL;
    rmrecnt.rlnote.rllen   = sizeof(rmRecNote_t);
    sct((TEXT *)&rmrecnt.rlnote.rlflgtrn,
                    (COUNT)((prmctl->flags & HOLD)
			    ? RL_PHY
			    : RL_PHY + RL_LOGBI));
    *pdelRecSize += prmctl->recsize;  /* cumulative length of deleted frags */
    rmrecnt.recsz = *pdelRecSize;
    rmrecnt.recnum = prmctl->recnum;
    rmrecnt.table = table;
    rmrecnt.nxtfrag = prmctl->nxtfrag;
    rmrecnt.flags  = prmctl->flags;

    /* and do the delete */

    rlLogAndDo(pcontext, (RLNOTE *)&rmrecnt, prmctl->bufHandle, 
               prmctl->recsize, prmctl->prec);

}  /* end rmDeleteRecSpace */


/* PROGRAM: rmUpdateRecord - replaces a record
 *
 * RETURNS: RMOK if successful
 *          DSM_S_RMNOTFND if can't find the record.
 *          DSM_S_RECTOOBIG if input size > MAXRECSZ
 *	    or error status received from lkrglock
 */
int
rmUpdateRecord(
	dsmContext_t	*pcontext,
        dsmTable_t       table,
	ULONG		 area,  /* area containing the record */
	DBKEY		 dbkey,	/* dbkey of record */
	TEXT		*prec,	/* ptr to record */
	COUNT		 size)	/* size of record */
{
    dsmStatus_t	 returnCode;
    lockId_t xDbkey;

    TRACE2_CB(pcontext, "rmUpdateRecord: dbkey=%D, size=%d", dbkey, size)

    xDbkey.table = area;
    xDbkey.dbkey = dbkey;

    STATINC (rmrpCt);

    /* record get lock to protect NOLOCK reads */
    returnCode = (dsmStatus_t)lkrglock (pcontext, &xDbkey,LKEXCL);
    if (returnCode != 0) return returnCode;

    returnCode = rmUpdateRec (pcontext, table, area, xDbkey.dbkey, prec, size);

    /* unlock record get lock */
    lkrgunlk(pcontext, &xDbkey);


    return returnCode;

}  /* end rmUpdateRecord */


/* PROGRAM: rmUpdateRec - replaces a record
 *
 * RETURNS: RMOK if successful
 *	    DSM_S_RMNOTFND if can't find the record.
 *	    DSM_S_RECTOOBIG if input size > MAXRECSZ
 */
LOCALF int
rmUpdateRec (
	dsmContext_t	*pcontext,
        dsmTable_t       table,
	ULONG		 area,  /* area of record  */
	DBKEY		 dbkey,	/* dbkey of record */
	TEXT		*prec,	/* ptr to record */
	COUNT		size)	/* size of record */
{
	rmCtl_t	 	 rmctl;
	rmChangeNote_t	*pnote; /* points to the logical change note	*/
	rmDiffList_t	*pDifL; /* points to difference list */
	COUNT		 difLen;/* length of diff list */
        COUNT            delRecSize;

    TRACE2_CB(pcontext, "rmUpdateRec: dbkey=%D, size=%d", dbkey, size)


    if (size > MAXRECSZ) return DSM_S_RECTOOBIG;

    rlTXElock (pcontext,RL_TXE_SHARE,RL_RECORD_UPDATE_OP);
    
    /* rlMTXstart (pcontext,RL_RECORD_UPDATE_OP);   */

    if (rmLocate(pcontext, area, dbkey, &rmctl, TOMODIFY))
    {
	bmReleaseBuffer(pcontext, rmctl.bufHandle);

        rlTXEunlock (pcontext);

	MSGN_CALLBACK(pcontext, rmMSG001, dbkey);
	return DSM_S_RMNOTFND;
    }
    delRecSize = 0; /* initialize cumulative length of deleted fragments */

    if ((rmctl.flags & SPLITREC) || (rmctl.holdtask) ||
	((size + (COUNT)sizeof(COUNT)) > (rmctl.space + rmctl.pbk->free)))
    {
	/* the record is fragmented, or the new one is too big to fit in
	   the same block as the existing record. Do the change the old way,
	   a piece at a time, logging entire old record and new record */
        rlTXElock(pcontext,RL_TXE_UPDATE,RL_RECORD_UPDATE_OP );
        
        rmDeleteRecSpace (pcontext, table, &rmctl, &delRecSize);
        rmUpdateRest (pcontext, table, &rmctl, prec, size, 0, &delRecSize);

        rlTXEunlock (pcontext);

        return (RMOK);
    }
    /* generate a difference note which describes the changed parts of the
       new record */

    /* stkpsh replaced with strentd to insure no stack overflow */
    pnote = (rmChangeNote_t *)stRentd(pcontext, pdsmStoragePool, rmctl.recsize 
                                     + size
                                     + sizeof (rmChangeNote_t) 
                                     + sizeof (rmDiffList_t)
                                     + sizeof (rmDiffEntry_t)
                                     + 10);

    pnote->flags = rmctl.flags;
    pnote->recsz = rmctl.recsize;
    pnote->newsz = size;
    pnote->table = table;
    pnote->recnum = rmctl.recnum; 
    pnote->rlnote.rlcode = RL_RMCHG;
    pnote->rlnote.rllen  = sizeof (rmChangeNote_t);
    sct((TEXT *)&pnote->rlnote.rlflgtrn, RL_PHY + RL_LOGBI);

    pDifL = (rmDiffList_t *)((TEXT *)pnote + sizeof (rmChangeNote_t));

    /* Build difference list between old and new records */

    difLen = rmGenDif (pcontext, rmctl.recsize, rmctl.prec, size, prec, pDifL);

    rlLogAndDo(pcontext, (RLNOTE *)pnote, rmctl.bufHandle,
               difLen, (TEXT *)pDifL);

    /* See block should go on the rm chain             */
    rmFree(pcontext, rmctl.area, rmctl.bufHandle, rmctl.pbk, &delRecSize);
	
    bmReleaseBuffer(pcontext, rmctl.bufHandle);

    rlTXEunlock (pcontext);

    stVacate(pcontext, pdsmStoragePool, (TEXT *)pnote);
    return (RMOK);

}  /* end rmUpdateRec */


/* PROGRAM: rmUpdateRecFrags - local routine to replace record fragments
 *
 * RETURNS: RMOK if successful
 *          DBKEY of new record if one was needed
 */
LOCALF DBKEY
rmUpdateRecFrags (
	dsmContext_t	*pcontext,
        dsmTable_t       table,
	ULONG		 area,          /* area containing record    */
	DBKEY		 dbkey,		/* dbkey of fragment to replace */
	TEXT		*prec,		/* ptr to replacement record */
	COUNT		 size,		/* size of replacement */
        COUNT           *pdelRecSize)
{
    rmCtl_t	 rmctl;
    rmBlock_t	*pbk;
    dbshm_t *pdbshm = pcontext->pdbcontext->pdbpub;

    TRACE2_CB(pcontext, "rmUpdateRecFrags: dbkey=%D, size=%d", dbkey, size)

    for (; dbkey; )
    {
	/* lock the block with the record fragment */

	if (rmLocate(pcontext, area, dbkey, &rmctl, TOMODIFY))
	{
	    /* the old record is not there */

	    bmReleaseBuffer(pcontext, rmctl.bufHandle);
	    MSGN_CALLBACK(pcontext, rmMSG001, dbkey);
	    dbkey =0;
	    break;
	}
	pbk = rmctl.pbk;

	/* delete it */

	rmDeleteRecSpace (pcontext, table, &rmctl, pdelRecSize);
	if (pbk->free > pdbshm->dbCreateLim)
	{
	    /* there is enough room to put in the new fragment */

	    rmctl.recnum = rmFetchDir(pcontext, pbk, area);
	    rmUpdateRest (pcontext, table, &rmctl, prec, size, CONT, pdelRecSize);
	    return (dbkey & ~RECMASK(BKGET_AREARECBITS(area))) + rmctl.recnum;
	}
	/* see if we need to put block on rm chain */

	rmFree (pcontext, rmctl.area, rmctl.bufHandle, pbk, pdelRecSize);

	/* release the block */

	bmReleaseBuffer(pcontext, rmctl.bufHandle);

	/* next piece */

	dbkey = rmctl.nxtfrag;
    }
    if (dbkey == (DBKEY)0)
    {
	/* Either we couldn't find a piece, or we deleted all of them
	   and did not find enough space to create the new pieces. So
	   we create a new record */

	return rmCreateRec (pcontext, area, 0, prec, size, CONT, 
                            pdelRecSize, (UCOUNT)RM_LOCK_NOT_NEEDED);
    }
    return(RMOK);

}  /* end rmUpdateRecFrags */


/* PROGRAM: rmUpdateRest - replaces rest of record
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
rmUpdateRest (
	dsmContext_t	*pcontext,
        dsmTable_t       table,
	rmCtl_t		*prmctl,
	TEXT		*prec,
	COUNT		 size,
	int		 cont,	/* may be 0, CONT, or HOLD */
        COUNT           *pdelRecSize)
{
    rmBlock_t	    *pbk;
    DBKEY	     dbkey;
    COUNT	     fraglen;
    rmRecNote_t	     rmrecnt;
    rmNextFragNote_t rmnxtfnt;
    bmBufHandle_t    bufHandle;

    TRACE1_CB(pcontext, "rmUpdateRest: size=%d", size)

    bufHandle = prmctl->bufHandle;
    pbk = prmctl->pbk;

    /* decide how much of the new record will fit in this block, */
    /* make a recovery log note and insert it in the block	 */

    fraglen = rmFitCheck (pcontext, pbk, size, prmctl->recnum);
    rmrecnt.flags = (prmctl->flags & RMBLOB) | cont;
    if (fraglen < size)
    {
	rmrecnt.flags |= SPLITREC;
	rmrecnt.nxtfrag = prmctl->nxtfrag; /* might be the right one */
    }
    else rmrecnt.nxtfrag = 0;

    rmrecnt.recsz = size;
    rmrecnt.table = table;
    rmrecnt.recnum = (TEXT)prmctl->recnum;
    rmrecnt.rlnote.rllen  = sizeof(rmrecnt);
    sct((TEXT *)&rmrecnt.rlnote.rlflgtrn, RL_PHY);
    rmrecnt.rlnote.rlcode = RL_RMCR;
    rlLogAndDo (pcontext, (RLNOTE *)&rmrecnt, bufHandle,
                (COUNT)fraglen, (TEXT *)prec);

    /* if there is more left of the new record, then recursively  */
    /* use it to replace whatever might be left of the old record */
    /* and insert a pointer to the continuation back in this blck */

    if (fraglen != size)
    {
	dbkey = pbk->rm_bkhdr.bk_dbkey;

	/* unlock the current block */

        bmReleaseBuffer(pcontext, bufHandle);

	rmnxtfnt.nxtfrag = rmUpdateRecFrags (
					pcontext,
                                        table, 
					prmctl->area,
					prmctl->nxtfrag, prec+fraglen,
					(COUNT)(size-fraglen),
					pdelRecSize);

        /* get the block again and insert the next fragment pointer */

	bufHandle = bmLocateBlock(pcontext, prmctl->area, dbkey, TOMODIFY);
	pbk = (rmBlock_t *)bmGetBlockPointer (pcontext,bufHandle);
	if (rmnxtfnt.nxtfrag != prmctl->nxtfrag)
	{
	    rmnxtfnt.prevval = prmctl->nxtfrag;
	    rmnxtfnt.recnum = prmctl->recnum;
	    rmnxtfnt.rlnote.rllen  = sizeof(rmnxtfnt);
	    rmnxtfnt.rlnote.rlcode = RL_RMNXTF;
	    sct((TEXT *)&rmnxtfnt.rlnote.rlflgtrn,  RL_PHY);
	    rlLogAndDo (pcontext, (RLNOTE *)&rmnxtfnt, bufHandle,
			 (COUNT)0, (TEXT *)PNULL);
	}
    }
    /* see if we need to update the rm chain */

    rmFree (pcontext, prmctl->area, bufHandle, pbk, pdelRecSize);

    /* unlock the block */

    bmReleaseBuffer(pcontext, bufHandle);

     /* if the new record is all finished, but there is */
     /* something left of the old record, delete it	*/

    if ((fraglen == size) && prmctl->nxtfrag) 
	rmDeleteRecFrags (pcontext, table, prmctl->area, prmctl->nxtfrag, pdelRecSize);

}  /* end rmUpdateRest */

/* PROGRAM: rmFindTempSpace -
 */
LOCALF
bmBufHandle_t rmFindTempSpace(dsmContext_t *pcontext, dsmObject_t table)
{
  omTempObject_t   *ptemp;
  bmBufHandle_t    bmHandle=0,newBlock;
  rmBlock_t        *pbk=NULL,*pnewbk;

  ptemp = omFetchTempPtr(pcontext, DSMOBJECT_MIXTABLE,table,table);
  if(ptemp->lastBlock)
  {
    bmHandle = bmLockBuffer(pcontext,DSMAREA_TEMP, ptemp->lastBlock,
			    TOMODIFY,1);
    pbk = (rmBlock_t *)bmGetBlockPointer(pcontext,bmHandle);
  
    if(pbk->free > 100 && pbk->freedir)
      return bmHandle;
  }
  /* No more space in the last block of the object so get
     a free block and link it in to the object.           */
  newBlock = bkfrrem(pcontext,DSMAREA_TEMP,RMBLK,FREECHN);
  pnewbk = (rmBlock_t *)bmGetBlockPointer(pcontext,newBlock);
  
  if(pbk)
  {
    pbk->rm_bkhdr.bk_nextf = pnewbk->rm_bkhdr.bk_dbkey;
    bmMarkModified(pcontext,bmHandle,0);  
    bmReleaseBuffer(pcontext,bmHandle);  
  }
  else
    ptemp->firstBlock = pnewbk->rm_bkhdr.bk_dbkey;

  bmMarkModified(pcontext,newBlock,0);

  /* Update the last block pointer    */
  ptemp->lastBlock = pnewbk->rm_bkhdr.bk_dbkey;
  ptemp->numBlocks++;

  return newBlock;
}
  
