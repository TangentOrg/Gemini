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
*	C Source:		bkrec.c
*	Subsystem:		1
*	Description:	
*	%created_by:	britt %
*	%date_created:	Mon Dec 12 16:18:11 1994 %
*
**********************************************************************/

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include "dbconfig.h"

#include "bkdoundo.h"
#include "bkpub.h"
#include "bkprv.h"
#include "bmpub.h"
#include "mstrblk.h"
#include "dbcode.h"
#include "dsmpub.h" 
#include "latpub.h"
#include "lkpub.h"
#include "dbcontext.h"
#include "objblk.h"
#include "rlpub.h"
#include "usrctl.h"
#include "ompub.h"
#include "rmpub.h"  /* for rmFormat call */
#include "tmtrntab.h"
#include "scasa.h"
#include "utmpub.h"   /* for utmalloc call */
#include "utspub.h"

#include <time.h>

/**** LOCAL FUNCTION PROTOTYPES ****/

LOCALF bmBufHandle_t
bkMakeBlock (dsmContext_t *pcontext, ULONG area, DBKEY dbkey, 
	     	bmBufHandle_t objHandle, int newBlockType);


/**** END LOCAL FUNCTION PROTOTYPES ****/

/* PROGRAM: bkRowCountUpdate - Update the row count in the object block
*/
DSMVOID
bkRowCountUpdate(dsmContext_t *pcontext, dsmArea_t areaNumber, 
		 dsmTable_t table, int addIt)
{
    bkRowCountUpdate_t  note;
    bmBufHandle_t       objHandle;


    INITNOTE(note,RL_ROW_COUNT_UPDATE,RL_LOGBI | RL_PHY);
  
    note.addIt = (TEXT)addIt;
    note.table = table;
    objHandle = bmLocateBlock(pcontext, areaNumber,
                          BLK2DBKEY(BKGET_AREARECBITS(areaNumber)), TOMODIFY);
    rlLogAndDo(pcontext,(RLNOTE *)&note,objHandle,0,PNULL);

    bmReleaseBuffer(pcontext, objHandle);

    return;
}

/* PROGRAM - bkRowCountUpdateDo  */
DSMVOID
bkRowCountUpdateDo(dsmContext_t *pcontext,
                   RLNOTE *pnote,
                   BKBUF *pblk,
                   COUNT  dlen _UNUSED_,
                   TEXT *pdata _UNUSED_)
{
  dsmObject_t  table = ((bkRowCountUpdate_t *)pnote)->table;

  if(SCTBL_IS_TEMP_OBJECT(table))
  {
    omTempObject_t *ptemp;

    ptemp = omFetchTempPtr(pcontext,DSMOBJECT_MIXTABLE,
			   table,table);
    if(((bkRowCountUpdate_t *)pnote)->addIt)
      ptemp->numRows++;
    else
      ptemp->numRows--;
    return;
  }
  if(((bkRowCountUpdate_t *)pnote)->addIt)
    ((objectBlock_t *)pblk)->totalRows++;
  else
    ((objectBlock_t *)pblk)->totalRows--;
}

/* PROGRAM - bkRowCountUpdateUndo */
DSMVOID
bkRowCountUpdateUndo(dsmContext_t *pcontext,
                     RLNOTE *pnote,
                     BKBUF *pblk, 
                     COUNT  dlen _UNUSED_,
                     TEXT *pdata _UNUSED_)
{
  dsmObject_t  table = ((bkRowCountUpdate_t *)pnote)->table;

  if(SCTBL_IS_TEMP_OBJECT(table))
  {
    omTempObject_t *ptemp;

    ptemp = omFetchTempPtr(pcontext,DSMOBJECT_MIXTABLE,
			   table,table);
    if(((bkRowCountUpdate_t *)pnote)->addIt)
      ptemp->numRows--;
    else
      ptemp->numRows++;
    return;
  }

  if(((bkRowCountUpdate_t *)pnote)->addIt)
    ((objectBlock_t *)pblk)->totalRows--;
  else
    ((objectBlock_t *)pblk)->totalRows++;
}

    
/* PROGRAM: bkfradd - add a disk block to the indicated chain.
 *		Refer to bkfr.h for more information on the chains
 *		managed by bkfradd and bkfrrem.
 *
 * RETURNS: DSMVOID
 */

DSMVOID
bkfradd (
	dsmContext_t	*pcontext,
	bmBufHandle_t	bufHandle,	/* buffer containing block */
	int	 chain)		/* indicates which chain to add
				   block to. Value is either
				   FREECHN, RMCHN, or LOCKCHN. */
{
    objectBlock_t *pobjectBlock;
/* need to change name of note?? */
    BKFRMST mstnote;
    BKFRBLK blknote;
    BKBUF   *pbkbuf;
    bmBufHandle_t objHandle;  /* Buffer handle to the object block */
    dsmArea_t areaNumber;

    
    pbkbuf = bmGetBlockPointer(pcontext,bufHandle);
    areaNumber = bmGetArea(pcontext, bufHandle);
    if(areaNumber == DSMAREA_TEMP)
      /* Whole objects get put on free chain in the temp area
	 not individual blocks                                  */
      return;

#if SHMEMORY
    if (chain == FREECHN) STATINC (fradCt);
    else if (chain == RMCHN) STATINC (rmadCt);
#endif

    INITNOTE( mstnote, RL_BKFAM, RL_PHY );
    INIT_S_BKFRMST_FILLERS( mstnote );	/* bug 20000114-034 by dmeyer in v9.1b */

    INITNOTE( blknote, RL_BKFAB, RL_PHY );
    INIT_S_BKFRBLK_FILLERS( blknote );	/* bug 20000114-034 by dmeyer in v9.1b */

    /* FATAL error if the block is already on any free chain */

    if (pbkbuf->BKFRCHN != NOCHN)
	FATAL_MSGN_CALLBACK(pcontext, bkFTL005, pbkbuf->BKFRCHN);
    /* Fill in the blk note and DO it */
    /* must do the data block before the master block, 'cause the DO */
    /* routing gets some "old" data out of the master block	     */


    objHandle = bmLocateBlock(pcontext, areaNumber,
                          BLK2DBKEY(BKGET_AREARECBITS(areaNumber)), TOMODIFY);
    pobjectBlock = (objectBlock_t *)bmGetBlockPointer(pcontext,objHandle);
    blknote.flag = chain;
    blknote.oldtype = pbkbuf->BKTYPE;
    blknote.newtype = (chain == FREECHN) ? FREEBLK : blknote.oldtype;
    blknote.next = pobjectBlock->chainFirst[chain];

    /* If the micro-transaction lock is not an update then get
       one now.                                                 */
    if(pcontext->pusrctl->uc_txelk == RL_TXE_SHARE)
        rlTXElock(pcontext,RL_TXE_UPDATE,RL_MISC_OP);
    
    rlLogAndDo (pcontext, (RLNOTE *)&blknote, bufHandle, 0, PNULL);

    /* Fill in the mst note and DO it */

    mstnote.chain = chain;
    sct((TEXT*)&mstnote.lockidx, (UCOUNT)0);
    mstnote.newfirst = pbkbuf->BKDBKEY;
    mstnote.oldfirst = pobjectBlock->chainFirst[chain];

    rlLogAndDo (pcontext, (RLNOTE *)&mstnote, objHandle, 0, PNULL);

    /* release the object block */
    bmReleaseBuffer(pcontext, objHandle);

}  /* end bkfradd */


/* PROGRAM: bkfrrem - remove the next database block from the
 *		indicated chain, assigning it to a specific new
 *		owner, or use. Refer to bkfr.h for more information
 *		on the chains managed by bkfradd and bkfrrem.
 *
 *		Use bkrmrem to remove top block from rm chain
 *
 *		If space is requested from the free chain and the
 *		free chain is empty, bkxtn is called to extend the
 *		database. If this extend fails, we FATAL, since
 *		bkfrrem callers can't handle NULL returns for free
 *		chain requests.
 *
 *		The demo version of bkxtn always FATAL's.
 *
 * RETURNS:	a pointer to the assigned block in a buffer,
 *		else NULL if there are no more blocks in the indicated
 *		chain.
 */
bmBufHandle_t
bkfrrem (
	dsmContext_t	*pcontext,
	ULONG   area,          /* area id for free chain              */
	int	 newowner,	/* indicates new use of block.
				   Taken from RMBLK, IDXBLK, or ITBLBK. */
	int	 chain)		/* indicates which chain to take
				   block from. Value is either
				   FREECHN, RMCHN, or LOCKCHN. */
{
    BKBUF	     *pbkbuf;
    objectBlock_t    *pobjectBlock;
    DBKEY	      freedbk;
    BKFRMST	      mstnote;
    BKFRBLK	      blknote;
    bmBufHandle_t     objHandle;/* Handle to the object block */
    bmBufHandle_t     bHandle;  /* Handle to the free block   */
    LONG              wait = 1;
    LONG              numBlocksOnChain;
    bkAreaDesc_t     *pbkAreaDesc;

    INITNOTE( mstnote, RL_BKFRM, RL_PHY );
    INIT_S_BKFRMST_FILLERS( mstnote );	/* bug 20000114-034 by dmeyer in v9.1b */

    INITNOTE( blknote, RL_BKFRB, RL_PHY );
    INIT_S_BKFRBLK_FILLERS( blknote );	/* bug 20000114-034 by dmeyer in v9.1b */

    pbkAreaDesc = BK_GET_AREA_DESC(pcontext->pdbcontext, area);

    if( chain == LOCKCHN )
    {
        /* Blocks on the index delete chain are also in an index.
           The access off the chain violates the index buffer locking
           protocol which is all blocks must be locked from the index
           root down.  Therefore if there is a buffer lock confict we must
           NOT que for the buffer lest deadlock occur.                    */
        wait = 0;
    }
    
retry:

    if (chain == RMCHN)
        STATINC (rmrmCt);
    else if (chain == FREECHN)
        STATINC (frrmCt);

    /* If requesting a block from the index delete chain and it
     * is empty; return 0
     */
    if( chain == LOCKCHN && pbkAreaDesc->bkChainFirst[LOCKCHN] == (DBKEY)0 )
    {
        return ((bmBufHandle_t) 0 );
    }
    
    objHandle = bmLockBuffer(pcontext, area, 
                             BLK2DBKEY(BKGET_AREARECBITS(area)), TOMODIFY, 1);
    
    pobjectBlock = (objectBlock_t *)bmGetBlockPointer(pcontext,objHandle);

    /* If requesting a free block and none are left allocate a
       block from above the database hi-water mark. */

    if (chain == FREECHN && pobjectBlock->chainFirst[FREECHN] == (DBKEY)0)
    {
        /* Free chain is empty */
	if (pobjectBlock->hiWaterBlock == pobjectBlock->totalBlocks)
	{
	    /* There are no blocks above the high water mark so extend 
	       the database to get some. */

	    bkExtend(pcontext, area, objHandle);
	}
	/* Get a free block by advancing high water mark */
        /* If the micro-transaction lock is not an update then get
           one now.                                                 */
        if(pcontext->pusrctl->uc_txelk == RL_TXE_SHARE)
            rlTXElock(pcontext,RL_TXE_UPDATE,RL_MISC_OP);
 
	bHandle = 
        bkMakeBlock (pcontext, area,
		    (DBKEY)(pobjectBlock->hiWaterBlock + 1) <<
                     BKGET_AREARECBITS(area), 
                     objHandle, newowner);
	
        /* Release the lock on the object block */
	bmReleaseBuffer(pcontext, objHandle);
	return (bHandle);
    }
    /* Get a firm grip on the block we're going to return */
    /* get the first free block */

    freedbk = pobjectBlock->chainFirst[chain];
    numBlocksOnChain = pobjectBlock->numBlocksOnChain[chain];
    
    /* release the object block to avoid deadlocks. Note that someone
       else could possibly grab our block then */
    bmReleaseBuffer(pcontext, objHandle);

    if ( freedbk == 0 && chain == LOCKCHN && numBlocksOnChain == 0 )
    {
        return 0;
    }
    
    if (freedbk == 0 )
    {
	FATAL_MSGN_CALLBACK(pcontext, bkFTL003, chain);
    }

    bHandle = bmLockBuffer(pcontext, area, freedbk, TOMODIFY, wait);
    if( bHandle == 0 )
    {
        return bHandle;
    }
    
    pbkbuf = bmGetBlockPointer (pcontext,bHandle);

    /* now lock the object block again */

    objHandle = bmLocateBlock(pcontext, area, 
                              BLK2DBKEY(BKGET_AREARECBITS(area)), TOMODIFY);    
    pobjectBlock = (objectBlock_t *)bmGetBlockPointer(pcontext,objHandle);

    /* check the chain */

    if (freedbk != pobjectBlock->chainFirst[chain])
    {
	/* someone else got the one we wanted. try again */

        bmReleaseBuffer(pcontext, objHandle);
	bmReleaseBuffer(pcontext, bHandle);
	goto retry;
    }
    /* NOTE : the usage of "old" and "new" for block type and first
	      block is reversed from the sense in which it is used
	      in bkfradd.  This looks funny but allows us to use the same
	      do and undo routines for adding and removing blocks.
    */
    /* set up the master block note and DO it				*/
    /* must do the master block before the data block, 'cause the	*/
    /* data block UNDO must happen before the master block UNDO during	*/
    /* physical undo							*/

    /* If the micro-transaction lock is not an update then get
       one now.                                                 */
    if(pcontext->pusrctl->uc_txelk == RL_TXE_SHARE)
        rlTXElock(pcontext,RL_TXE_UPDATE,RL_MISC_OP);
 
    mstnote.chain = chain ;
    sct((TEXT *)&mstnote.lockidx, (UCOUNT)0);
    mstnote.oldfirst = pbkbuf->BKNEXTF;
    mstnote.newfirst = freedbk;
    rlLogAndDo (pcontext, (RLNOTE *)&mstnote, objHandle, 0, PNULL);

    /* set up and DO the database block note */

    blknote.next = pbkbuf->BKNEXTF;
    blknote.flag = pbkbuf->BKFRCHN;
    blknote.oldtype = newowner;
    if(area == DSMAREA_TEMP)
      blknote.newtype = FREEBLK;
    else
      blknote.newtype = pbkbuf->BKTYPE;
    rlLogAndDo (pcontext, (RLNOTE *)&blknote, bHandle, 0, PNULL);

    bmReleaseBuffer(pcontext, objHandle);

    return bHandle;

}  /* end bkfrrem */


/* PROGRAM: bkrmrem - remove the specified database block from the
 *		RM chain IF it is the first one.
 *		The block has to be locked exclusive by caller
 *		Use bkfrrem for free chain and index lock chain
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkrmrem (
	dsmContext_t	*pcontext,
	bmBufHandle_t	 bufHandle)
{
    BKBUF       *pbkbuf;
    bmBufHandle_t objHandle;
    objectBlock_t *pobjectBlock;
    DBKEY	topdbk;
    BKFRMST	mstnote;
    BKFRBLK	blknote;
    ULONG       area;

    pbkbuf = bmGetBlockPointer(pcontext,bufHandle);

    if (pbkbuf->BKFRCHN != RMCHN) return;

    INITNOTE( mstnote, RL_BKFRM, RL_PHY );
    INIT_S_BKFRMST_FILLERS( mstnote );	/* bug 20000114-034 by dmeyer in v9.1b */

    INITNOTE( blknote, RL_BKFRB, RL_PHY );
    INIT_S_BKFRBLK_FILLERS( blknote );	/* bug 20000114-034 by dmeyer in v9.1b */

    area = bmGetArea(pcontext, bufHandle);

    objHandle = bmLocateBlock(pcontext, area, 
                              BLK2DBKEY(BKGET_AREARECBITS(area)), TOMODIFY );
    pobjectBlock = (objectBlock_t *)bmGetBlockPointer (pcontext,objHandle);

    topdbk = pobjectBlock->chainFirst[RMCHN];
    if (topdbk != pbkbuf->BKDBKEY)
    {
	/* our block is not on front so we cannot remove it now */

        bmReleaseBuffer(pcontext, objHandle);
	return;
    }
    STATINC (rmrmCt);

    /* NOTE : the usage of "old" and "new" for block type and first
	      block is reversed from the sense in which it is used
	      in bkfradd.  This looks funny but allows us to use the same
	      do and undo routines for adding and removing blocks.
    */
    /* set up the master block note and DO it				*/
    /* must do the master block before the data block, 'cause the	*/
    /* data block UNDO must happen before the master block UNDO during	*/
    /* physical undo							*/

    /* If the micro-transaction lock is not an update then get
       one now.                                                 */
    if(pcontext->pusrctl->uc_txelk == RL_TXE_SHARE)
        rlTXElock(pcontext,RL_TXE_UPDATE,RL_MISC_OP);
 
    mstnote.chain = RMCHN;
    sct((TEXT *)&mstnote.lockidx, (UCOUNT)0);
    mstnote.oldfirst = pbkbuf->BKNEXTF;
    mstnote.newfirst = topdbk;
    rlLogAndDo (pcontext, (RLNOTE *)&mstnote, objHandle, 0, PNULL);

    /* set up and DO the database block note */

    blknote.next = pbkbuf->BKNEXTF;
    blknote.flag = pbkbuf->BKFRCHN;
    blknote.oldtype = RMBLK;
    blknote.newtype = RMBLK;
    rlLogAndDo(pcontext, (RLNOTE *)&blknote, bufHandle, 0, PNULL );

    /* release the object block */

    bmReleaseBuffer(pcontext, objHandle);

}  /* end bkrmrem */


/* PROGRAM: bkfrmlnk - object block half of adding a block to a free chain
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkfrmlnk (
	dsmContext_t	*pcontext,
	RLNOTE		*pnote,	  /* Pointer to note */
	BKBUF		*pblk,	  /* Pointer to buffer or 0 if in mem. tab. */
	COUNT		 dlen _UNUSED_,	  /* Not used - should be 0	*/
	TEXT		*pdata _UNUSED_)  /* Not used - should be PNULL */
{
    int              chain = ((BKFRMST *)pnote)->chain;
    objectBlock_t   *pobjectBlock;
    bkAreaDesc_t    *pbkAreaDesc;

    pbkAreaDesc = BK_GET_AREA_DESC(pcontext->pdbcontext, 
                                   xlng((TEXT *)&pnote->rlArea));
 
  /* Decide whether this is really an object block update */
    pobjectBlock = (objectBlock_t *)pblk;

    pobjectBlock->chainFirst[chain] = ((BKFRMST *)pnote)->newfirst;
    pbkAreaDesc->bkChainFirst[chain] = pobjectBlock->chainFirst[chain];

    pobjectBlock->numBlocksOnChain[chain]++;
    pbkAreaDesc->bkNumBlocksOnChain[chain] =
                                 pobjectBlock->numBlocksOnChain[chain];

    if (((BKFRMST *)pnote)->oldfirst == 0)
    {
        /* chain was empty */

      pobjectBlock->chainLast[chain] =
	     ((BKFRMST *)pnote)->newfirst; /* 1st is also last */
    }


}  /* end bkfrmlnk */


/* PROGRAM: bkfrbl1 - database block half of adding a block to a free chain 
 * Used during original bkfradd or during roll forward of bkfradd
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkfrbl1 (
	dsmContext_t	*pcontext,
	RLNOTE		*pnote,              /* Pointer to note */
	BKBUF		*pblk,               /* Pointer to buffer */
	COUNT		 dlen _UNUSED_,      /* Not used - should be 0     */
	TEXT		*pdata _UNUSED_)     /* Not used - should be PNULL */
{

    pblk->BKNEXTF = ((BKFRBLK *)pnote)->next;
    pblk->BKFRCHN = ((BKFRBLK *)pnote)->flag;
    pblk->BKTYPE = ((BKFRBLK *)pnote)->newtype;
    if ((((BKFRBLK *)pnote)->oldtype == RMBLK) &&
        (((BKFRBLK *)pnote)->newtype == FREEBLK))
    {
	bkclear (pcontext, pblk);
    }

}  /* end bkfrbl1 */


/* PROGRAM: bkfrbl2 - database block half of adding a block to a free chain
 * Used during UNDO of a bkfrrem
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkfrbl2 (
	dsmContext_t	*pcontext,
	RLNOTE		*pnote,              /* Pointer to note */
	BKBUF		*pblk,               /* Pointer to buffer */
	COUNT		 dlen _UNUSED_,      /* Not used - should be 0     */
	TEXT		*pdata _UNUSED_)     /* Not used - should be PNULL */
{
    pblk->BKNEXTF = ((BKFRBLK *)pnote)->next;
    pblk->BKFRCHN = ((BKFRBLK *)pnote)->flag;
    pblk->BKTYPE = ((BKFRBLK *)pnote)->newtype;
    if ((((BKFRBLK *)pnote)->oldtype == RMBLK) && 
        (((BKFRBLK *)pnote)->newtype == FREEBLK))
    {
	bkclear (pcontext, pblk);
    }

}  /* end bkfrbl2*/


/* PROGRAM: bkfrmulk - object block half of removing a block from free chain
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkfrmulk (
	dsmContext_t	*pcontext,
	RLNOTE		*pnote,	  /* Pointer to note */
	BKBUF		*pblk,	  /* Pointer to buffer or 0 if in mem. tab. */
	COUNT		 dlen _UNUSED_,	  /* Not used - should be 0	*/
	TEXT		*pdata _UNUSED_)  /* Not used - should be PNULL */
{
    int               chain = ((BKFRMST *)pnote)->chain;
    objectBlock_t    *pobjectBlock;
    bkAreaDesc_t     *pbkAreaDesc;

    pbkAreaDesc = BK_GET_AREA_DESC(pcontext->pdbcontext, 
                                   xlng((TEXT *)&pnote->rlArea));
    pobjectBlock = (objectBlock_t *)pblk;

    pobjectBlock->chainFirst[chain] = ((BKFRMST *)pnote)->oldfirst;
    pbkAreaDesc->bkChainFirst[chain] = pobjectBlock->chainFirst[chain];
	
    pobjectBlock->numBlocksOnChain[chain]--;
    pbkAreaDesc->bkNumBlocksOnChain[chain] =
                                 pobjectBlock->numBlocksOnChain[chain];

    if (((BKFRMST *)pnote)->oldfirst == 0)
    {
       /* chain empty */
       
       pobjectBlock->chainLast[chain] = 0; /* no last */
    }


}  /* end bkfrmulk */


/* PROGRAM: bkfrbulk - database block half of removing block
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkfrbulk (
	dsmContext_t	*pcontext,
	RLNOTE		*pnote,		     /* Pointer to note */
	BKBUF		*pblk,		     /* Pointer to buffer */
	COUNT		 dlen _UNUSED_,	     /* Not used - should be 0	   */
	TEXT		*pdata _UNUSED_)     /* Not used - should be PNULL */
{

    pblk->BKNEXTF = (DBKEY)0;
    pblk->BKFRCHN = NOCHN;
    pblk->BKTYPE = ((BKFRBLK *)pnote)->oldtype;
    if ((((BKFRBLK *)pnote)->oldtype == RMBLK) &&
        (((BKFRBLK *)pnote)->newtype == FREEBLK))
    {
	rmFormat (pcontext, pblk, (ULONG)xlng((TEXT *)&pnote->rlArea));
    }

}  /* end bkfrbulk */


/* PROGRAM: bkExtend - Determine by how much to extend the db and write
 *                     the db extend note.
 *
 * RETURNS: DSMVOID
 */ 
DSMVOID
bkExtend(
	dsmContext_t	*pcontext,
	ULONG		 area,
	bmBufHandle_t	 objHandle)
{
       XTDBNOTE   xtnnote;
       LONG       xtndamt;
       objectBlock_t   *pobjectBlock;
       TEXT            *pareaName = NULL;
       dsmStatus_t    	returnCode;
       ULONG        	recordSize = 0;
       DBKEY        	areaRecid;
       mstrblk_t        *pcontrolBlock;
       bmBufHandle_t  	controlBufHandle;
       UCOUNT           controlRecbits;


    INITNOTE(xtnnote, RL_XTDB, RL_PHY);
    
    /* We do an odd thing here.                              */
    /* We extend the database then we write the extend note. */
    /* We do this so that if the extend fails because the    */
    /* disk is full we won't try and extend the database     */
    /* during the redo phase of crash recovery and just crash*/
    /* again.  This way the database can be recovered if a   */
    /* crash occurred because we could not extend the variable*/
    /* length data extent.                                   */

    xtndamt = bkxtn(pcontext, area, 0L);

    pobjectBlock = (objectBlock_t *)bmGetBlockPointer(pcontext,objHandle);

    if (xtndamt == 0)
    {
	bmReleaseBuffer(pcontext, objHandle);
        controlRecbits = BKGET_AREARECBITS(DSMAREA_CONTROL);
        controlBufHandle = bmLocateBlock(pcontext, DSMAREA_CONTROL,
                                         BLK1DBKEY(controlRecbits), TOREAD);
        pcontrolBlock = (mstrblk_t *)bmGetBlockPointer(pcontext,
                                                            controlBufHandle);
        pareaName = (TEXT *)utmalloc(MAXPATHN);
        /* given the area number, get the name */
        returnCode = bkFindAreaName(pcontext, pcontrolBlock, pareaName, &area,
                                    &areaRecid, &recordSize);
 
        FATAL_MSGN_CALLBACK(pcontext, bkFTL084, pareaName);
 
        if (pareaName)
        {
            utfree(pareaName);
        }
    }
    STATADD (dbxtCt, xtndamt);

    xtnnote.newtot	 = pobjectBlock->totalBlocks + xtndamt;
    rlLogAndDo(pcontext, (RLNOTE *)&xtnnote.rlnote, objHandle, (COUNT)0, PNULL);

}  /* end bkExtend */


/* PROGRAM: bkdoxtn - extend the .db file.
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkdoxtn(
	dsmContext_t	*pcontext,
	RLNOTE		*prlnote, /* pointer to the note */
	BKBUF		*pobjectBlock,  /* this note always affects the objblk*/
	COUNT		 dlen _UNUSED_,	/* not used	*/
	TEXT		*pdata _UNUSED_)	/* not used	*/
{
        dbcontext_t *pdbcontext = pcontext->pdbcontext;
	LONG	needxtn;
	LONG	xtndamt = 0;
	ULONG   area;
	bkAreaDesc_t *pbkAreaDesc;

        TEXT         *pareaName = NULL;
        dsmStatus_t   returnCode;
        ULONG         recordSize = 0;
        DBKEY         areaRecid;
        mstrblk_t    *pcontrolBlock;
        bmBufHandle_t    controlBufHandle;
        UCOUNT           controlRecbits;

    /* see if the database has already been extended enough */
    area = xlng((TEXT *)&prlnote->rlArea);
    needxtn = ((XTDBNOTE *)prlnote)->newtot - bkflen(pcontext, area);
    if (needxtn > 0)
    {
	/* now try to extend the database */

	xtndamt = bkxtn(pcontext, area, needxtn);
	if (xtndamt < needxtn)
        {
	    controlRecbits = BKGET_AREARECBITS(DSMAREA_CONTROL);
            controlBufHandle = bmLocateBlock(pcontext, DSMAREA_CONTROL,
                                             BLK1DBKEY(controlRecbits), TOREAD);
            pcontrolBlock = (mstrblk_t *)bmGetBlockPointer(pcontext,
                                                            controlBufHandle);
            pareaName = (TEXT *)utmalloc(MAXPATHN);
            /* given the area number, get the name */
            returnCode = bkFindAreaName(pcontext, pcontrolBlock, pareaName,
                                        &area, &areaRecid, &recordSize);
 
            FATAL_MSGN_CALLBACK(pcontext, bkFTL084, pareaName);
 
            if (pareaName)
            {
                utfree(pareaName);
            }
        }
        STATADD (dbxtCt, xtndamt);
    }
    ((objectBlock_t *)pobjectBlock)->totalBlocks =
		 ((XTDBNOTE *)prlnote)->newtot + (xtndamt - needxtn);

    pbkAreaDesc    = BK_GET_AREA_DESC(pdbcontext, area);
    pbkAreaDesc->bkMaxDbkey =
        ((DBKEY)((objectBlock_t *)pobjectBlock)->totalBlocks+1) <<
                          pbkAreaDesc->bkAreaRecbits;

}  /* end bkdoxtn */


/* PROGRAM: bkclear -- clear out the data area of a data block.
 *		Leaves the header information alone.
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkclear (
	dsmContext_t	*pcontext,
	bkbuf_t		*pbkbuf)	/* points to buffer containing
				   block to be cleared. */
{
    stnclr (pbkbuf->bkdata, BLKSPACE(pcontext->pdbcontext->pdbpub->dbblksize));

}  /* end bkclear */


/* PROGRAM: bktoend - remove the top database block from the
 *		top of RM free chain and push to the bottom of the chain.
 *
 *		This is done to avoid chain clogging, when a block
 *		may not be used it is pushed here to give all the blocks
 *		in the chain a chance.
 *
 *		The block must be locked exclusive by the caller
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bktoend (
	dsmContext_t	*pcontext,
	bmBufHandle_t	 bufHandle)
{
        bmBufHandle_t objHandle;
	objectBlock_t	*pobjectBlock;
	BKBUF   *pbkbuf;
	DBKEY	 topdbk;
	DBKEY	 botdbk;
	DBKEY	 blkdbk;
	DBKEY	 topnext;
        bmBufHandle_t botHandle;
	BKBUF	*pbkbot;
	ULONG    area;

	BK2ENDMST	mstnote;
	BK2ENDBLK	blknote;

    STATINC (rmenCt);

    INITNOTE( mstnote, RL_BK2EM, RL_PHY );
    INITNOTE( blknote, RL_BK2EB, RL_PHY );

    pbkbuf = bmGetBlockPointer(pcontext,bufHandle);
    area = bmGetArea(pcontext, bufHandle);
    blkdbk = pbkbuf->BKDBKEY;
retry:

    objHandle = bmLocateBlock(pcontext, area, 
                              BLK2DBKEY(BKGET_AREARECBITS(area)), TOMODIFY);
    pobjectBlock = (objectBlock_t *)bmGetBlockPointer(pcontext,objHandle);
    topdbk = pobjectBlock->chainFirst[RMCHN];
    botdbk = pobjectBlock->chainLast[RMCHN];
    if ((topdbk == botdbk) || (topdbk != blkdbk))
    {
	/* last and first the same, or block passed in not first */

        bmReleaseBuffer(pcontext, objHandle);
	return; /* the last is also the first */
    }
    /* to avoid deadlock, we have to lock the bottom block before the object
       block, so we now must release the object block */

    bmReleaseBuffer(pcontext, objHandle);

    /* then we lock the bottom block and the object block again */

    botHandle =  bmLocateBlock(pcontext, area, botdbk, TOMODIFY);
    pbkbot = bmGetBlockPointer(pcontext,botHandle);

    objHandle = bmLocateBlock(pcontext, area, 
                              BLK2DBKEY(BKGET_AREARECBITS(area)), TOMODIFY);
    pobjectBlock = (objectBlock_t *)bmGetBlockPointer(pcontext,objHandle);

    topdbk = pobjectBlock->chainFirst[RMCHN];
    botdbk = pobjectBlock->chainLast[RMCHN];
    if (topdbk != blkdbk)
    {
	/* While we were mucking about with buffer locks, someone else
	   put a different block on the front of the chain. So we just
	   leave things as they are */

        bmReleaseBuffer(pcontext, objHandle);

        bmReleaseBuffer(pcontext, botHandle);

	return;
    }
    if (pbkbot->BKDBKEY != botdbk)
    {
	/* The bottom block has changed while we were messing about with
	   the buffer locks. Try again */

	bmReleaseBuffer(pcontext, objHandle);

	bmReleaseBuffer(pcontext, botHandle);

	goto retry;
    }
    /* If the micro-transaction lock is not an update then get
       one now.                                                 */
    if(pcontext->pusrctl->uc_txelk == RL_TXE_SHARE)
        rlTXElock(pcontext,RL_TXE_UPDATE,RL_MISC_OP);
 
    /* set up the object block note and DO it */

    topnext = pbkbuf->BKNEXTF;

    mstnote.top = topdbk;
    mstnote.second = topnext;
    mstnote.bot = botdbk;
    rlLogAndDo (pcontext, (RLNOTE *)&mstnote, objHandle, 0, PNULL);

    /* set up and DO the top block note */

    blknote.oldnext = topnext;
    blknote.newnext = 0;
    rlLogAndDo (pcontext, (RLNOTE *)&blknote, bufHandle, 0, PNULL);

    /* set up and DO the bottom block note */

    blknote.oldnext = 0;
    blknote.newnext = topdbk;
    rlLogAndDo (pcontext, (RLNOTE *)&blknote, botHandle, 0, PNULL);

    bmReleaseBuffer(pcontext, objHandle);

    bmReleaseBuffer(pcontext, botHandle);

}  /* end bktoend */


/* PROGRAM: bk2emdo - object block half of moving a block to end of chain
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bk2emdo (
	dsmContext_t	*pcontext,
	RLNOTE		*pnote,		  /* Pointer to note */
	BKBUF		*pblk,		  /* Pointer to buffer */
 	COUNT		 dlen _UNUSED_,	  /* Not used - should be 0	*/
	TEXT		*pdata _UNUSED_)  /* Not used - should be PNULL */
{
    bkAreaDesc_t     *pbkAreaDesc;
 
    pbkAreaDesc = BK_GET_AREA_DESC(pcontext->pdbcontext, 
                                   xlng((TEXT *)&pnote->rlArea));

    ((objectBlock_t *)pblk)->chainFirst[RMCHN] = ((BK2ENDMST *)pnote)->second;
    pbkAreaDesc->bkChainFirst[RMCHN]           = ((BK2ENDMST *)pnote)->second;
    ((objectBlock_t *)pblk)->chainLast[RMCHN]  = ((BK2ENDMST *)pnote)->top;

}  /* end bk2emdo */


/* PROGRAM: bk2emundo - undo object block half of moving a block to end chain
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bk2emundo (
	dsmContext_t	*pcontext,
	RLNOTE		*pnote,		  /* Pointer to note */
	BKBUF		*pblk,		  /* Pointer to buffer */
	COUNT		 dlen _UNUSED_,	  /* Not used - should be 0	*/
	TEXT		*pdata _UNUSED_)  /* Not used - should be PNULL */
{
    bkAreaDesc_t     *pbkAreaDesc;
 
    pbkAreaDesc = BK_GET_AREA_DESC(pcontext->pdbcontext,
                                   xlng((TEXT *)&pnote->rlArea));
 
    ((objectBlock_t *)pblk)->chainFirst[RMCHN] = ((BK2ENDMST *)pnote)->top;
    pbkAreaDesc->bkChainFirst[RMCHN]           = ((BK2ENDMST *)pnote)->top;
    ((objectBlock_t *)pblk)->chainLast[RMCHN]  = ((BK2ENDMST *)pnote)->bot;

}  /* end bk2emundo */


/* PROGRAM: bk2ebdo - do block half of moving the top  block to end
 *		of RM chain
 * Used during original bktoend or during roll forward of bkfradd
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bk2ebdo (
	dsmContext_t	*pcontext _UNUSED_,
	RLNOTE		*pnote,    /* Pointer to note */
	BKBUF		*pblk,     /* Pointer to buffer */
	COUNT		 dlen _UNUSED_,     /* Not used - should be 0     */
	TEXT		*pdata _UNUSED_)    /* Not used - should be PNULL */
{
    pblk->BKNEXTF = ((BK2ENDBLK *)pnote)->newnext;

}  /* bk2ebdo */


/* PROGRAM: bk2ebundo - undo block half of moving the top block to 
 *			the end of RM free chain
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bk2ebundo (
	dsmContext_t	*pcontext _UNUSED_,
	RLNOTE		*pnote,              /* Pointer to note */
	BKBUF		*pblk,               /* Pointer to buffer */
	COUNT		 dlen _UNUSED_,      /* Not used - should be 0     */
	TEXT		*pdata _UNUSED_)     /* Not used - should be PNULL */
{
    pblk->BKNEXTF = ((BK2ENDBLK *)pnote)->oldnext;

}  /* end bk2ebundo */


/* PROGRAM: bkrmbot - add a disk block to the bottom of RM free chain or
 *                     the bottom of the index delete chain (LOCKCHN).
 *		the block was not on any chain before this
 *
 * RETURNS: DSMVOID
 */

DSMVOID
bkrmbot (
	dsmContext_t	*pcontext,
	bmBufHandle_t	 bufHandle)	/* buffer containing block */
{
    BKBUF       *pbkbuf;
    bmBufHandle_t objHandle;
    objectBlock_t *pobjectBlock;
    DBKEY	 botdbk;
    DBKEY	 dbkey;
    bmBufHandle_t botHandle;
    BKBUF	*pbkbot;

    BKFRMBOT    mstnote;
    BKFRBBOT    blknote;
    BK2ENDBLK   blk2note;
    
    ULONG       area;
    int         chain;
    
    INITNOTE( mstnote, RL_BKMBOT, RL_PHY );
    INIT_S_BKFRMBOT_FILLERS( mstnote );	/* bug 20000114-034 by dmeyer in v9.1b */

    INITNOTE( blknote, RL_BKBBOT, RL_PHY );
    INIT_S_BKFRBBOT_FILLERS( blknote );	/* bug 20000114-034 by dmeyer in v9.1b */

    INITNOTE( blk2note, RL_BK2EB, RL_PHY );

    stnclr(&botHandle, sizeof(bmBufHandle_t));

    /* FATAL error if the block is already on any free chain */
    pbkbuf = bmGetBlockPointer(pcontext,bufHandle);
    if (pbkbuf->BKFRCHN != NOCHN)
    {
	FATAL_MSGN_CALLBACK(pcontext, bkFTL005, pbkbuf->BKFRCHN);
    }
    area = bmGetArea(pcontext, bufHandle);
    if( pbkbuf->BKTYPE == IDXBLK )
       	chain = LOCKCHN;
    else
        chain = RMCHN;

    if ( chain == RMCHN ) STATINC (rmboCt);
    
retry:
    pbkbot = (BKBUF *)NULL;

    objHandle = bmLocateBlock(pcontext, area, 
                              BLK2DBKEY(BKGET_AREARECBITS(area)), TOMODIFY);
    pobjectBlock = (objectBlock_t *)bmGetBlockPointer(pcontext,objHandle);

    botdbk = pobjectBlock->chainLast[chain]; /* old last blk */
    dbkey = pbkbuf->BKDBKEY ; /* new last blk */

    if (botdbk)
    {
        bmReleaseBuffer(pcontext, objHandle);

        /* get the old bottom block */

        botHandle = bmLocateBlock(pcontext, area, botdbk, TOMODIFY);
        pbkbot = bmGetBlockPointer(pcontext,botHandle);
        /* lock the object block again */

	objHandle = bmLocateBlock(pcontext, area, 
                              BLK2DBKEY(BKGET_AREARECBITS(area)), TOMODIFY);
	pobjectBlock = (objectBlock_t *)bmGetBlockPointer(pcontext,objHandle);

        if (pobjectBlock->chainLast[chain] != botdbk)
        {
	    /* bottom changed while we were acquiring locks. start over */

            bmReleaseBuffer(pcontext, objHandle);

	    bmReleaseBuffer(pcontext, botHandle);

	    goto retry;
	}
    }

    /* If the micro-transaction lock is not an update then get
       one now.                                                 */
    if(pcontext->pusrctl->uc_txelk == RL_TXE_SHARE)
        rlTXElock(pcontext,RL_TXE_UPDATE,RL_MISC_OP);
 
    /* Fill in the blk note and DO it */
    /* must do the data block before the object block, 'cause the DO */
    /* routing gets some "old" data out of the object block	     */

    blknote.oldtype = pbkbuf->BKTYPE;
    rlLogAndDo (pcontext, (RLNOTE *)&blknote, bufHandle, 0, PNULL);

    /* Fill in the mst note and DO it */
    mstnote.chain = (TEXT)chain;
    mstnote.newlast = dbkey ;
    mstnote.oldlast = botdbk;
    rlLogAndDo (pcontext, (RLNOTE *)&mstnote, objHandle, 0, PNULL );

    if (!botdbk)
    {
        bmReleaseBuffer(pcontext, objHandle);
	return; /* the chain was empty */
    }
    /* set up and DO the old bottom block note */

    blk2note.oldnext = 0;
    blk2note.newnext = dbkey;
    rlLogAndDo(pcontext, (RLNOTE *)&blk2note, botHandle, 0, PNULL );

    bmReleaseBuffer(pcontext, objHandle);

    bmReleaseBuffer(pcontext, botHandle);

}  /* end bkrmbot */


/* PROGRAM: bkbotdo - do of adding a block to bottom of RM or index LOCK chain
 * Used during original bkfrbot or during roll forward of bkfradd
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkbotdo (
	dsmContext_t	*pcontext _UNUSED_,
	RLNOTE		*pnote _UNUSED_,     /* Pointer to note */
	BKBUF		*pblk,               /* Pointer to buffer */
	COUNT		 dlen _UNUSED_,      /* Not used - should be 0     */
	TEXT		*pdata _UNUSED_)     /* Not used - should be PNULL */
{
    pblk->BKNEXTF = 0;
    if( pblk->BKTYPE == RMBLK )
    	pblk->BKFRCHN = RMCHN;	
    else
        pblk->BKFRCHN = LOCKCHN;
    
}  /* end bkbotdo */


/* PROGRAM: bkbotundo - undo of adding a block to bottom of  RM chain
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkbotundo (
	dsmContext_t	*pcontext _UNUSED_,
	RLNOTE		*pnote,              /* Pointer to note */
	BKBUF		*pblk,               /* Pointer to buffer */
	COUNT		 dlen _UNUSED_,      /* Not used - should be 0 */
	TEXT		*pdata _UNUSED_)     /* Not used - should be PNULL */
{
    pblk->BKNEXTF = 0;
    pblk->BKFRCHN = NOCHN;
    pblk->BKTYPE = ((BKFRBBOT *)pnote)->oldtype;

}  /* end bkbotundo */


/* PROGRAM: bkbotmdo - object block half of adding a block to bottom of chain
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkbotmdo (
	dsmContext_t	*pcontext,
	RLNOTE		*pnote,          /* Pointer to note */
	BKBUF		*pblk,           /* Pointer to buffer */
	COUNT		 dlen _UNUSED_,  /* Not used - should be 0 */
	TEXT		*pdata _UNUSED_) /* Not used - should be PNULL */
{
    int           chain;
    bkAreaDesc_t *pbkAreaDesc;
    
    chain = ((BKFRMBOT *)pnote)->chain;
    pbkAreaDesc = BK_GET_AREA_DESC(pcontext->pdbcontext,
                                   xlng((TEXT *)&pnote->rlArea));

    if (((BKFRMBOT *)pnote)->oldlast == 0)
    {
        /* chain was empty */

	((objectBlock_t *)pblk)->chainFirst[chain] =
                                   ((BKFRMBOT *)pnote)->newlast;
        pbkAreaDesc->bkChainFirst[chain] = 
                                   ((BKFRMBOT *)pnote)->newlast;
    }

    ((objectBlock_t *)pblk)->chainLast[chain] = ((BKFRMBOT *)pnote)->newlast;

    ((objectBlock_t *)pblk)->numBlocksOnChain[chain]++;
    pbkAreaDesc->bkNumBlocksOnChain[chain] =
                             ((objectBlock_t *)pblk)->numBlocksOnChain[chain];

}  /* end bkbotmdo */


/* PROGRAM: bkbotmundo - undo object block half of adding a block to bottom
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkbotmundo (
	dsmContext_t	*pcontext,
	RLNOTE		*pnote,          /* Pointer to note */
     	BKBUF   	*pblk,           /* Pointer to buffer */
     	COUNT    	 dlen _UNUSED_,  /* Not used - should be 0 */
     	TEXT    	*pdata _UNUSED_) /* Not used - should be PNULL */
{
    int           chain;
    bkAreaDesc_t *pbkAreaDesc;
    
    chain = ((BKFRMBOT *)pnote)->chain;
    pbkAreaDesc = BK_GET_AREA_DESC(pcontext->pdbcontext,
                                   xlng((TEXT *)&pnote->rlArea));
 
    if (((BKFRMBOT *)pnote)->oldlast == 0)
    {
        /* chain was empty */
	((objectBlock_t *)pblk)->chainFirst[chain] = 0;
        pbkAreaDesc->bkChainFirst[chain]           = 0;
    }

    ((objectBlock_t *)pblk)->chainLast[chain] = ((BKFRMBOT *)pnote)->oldlast;

    ((objectBlock_t *)pblk)->numBlocksOnChain[chain]--;
    pbkAreaDesc->bkNumBlocksOnChain[chain] =
                 ((objectBlock_t *)pblk)->numBlocksOnChain[chain];

}  /* end bkbotmundo  */


/* PROGRAM: bkMakeBlock - Format a note which causes a block with the
 *                        given dbkey to be materialized in the buffer
 *			  pool.
 *
 * RETURNS:
 */
LOCALF
bmBufHandle_t
bkMakeBlock(
	dsmContext_t	*pcontext,
	ULONG		 area,
	DBKEY		 dbkey,
	bmBufHandle_t	 objHandle,
	int		 newBlockType)
{
    objectBlock_t      *pobjectBlock;
    BKMAKENOTE    note;
    BKBUF        *pbkbuf;
    BKHWMNOTE     bkHwmNote;
    BKFORMATNOTE  formatNote;
    bmBufHandle_t bHandle;

    /* Get pointer to object block   */
    pobjectBlock = (objectBlock_t *)bmGetBlockPointer(pcontext,objHandle);

    /* Read next block above the high water mark                  */
    bHandle = bmLocateBlock(pcontext, area,
			    (DBKEY)(pobjectBlock->hiWaterBlock + 1)
                         << BKGET_AREARECBITS(area), TOCREATE);
    pbkbuf = bmGetBlockPointer(pcontext,bHandle);

    INITNOTE (note, RL_BKMAKE, RL_PHY);    
    note.area = area;
    note.dbkey = dbkey;
    rlLogAndDo (pcontext, (RLNOTE *)&note, 0, 0, PNULL);

    /* Format the block                                           */

    INITNOTE( formatNote, RL_BKFORMAT, RL_PHY );
    INIT_S_BKFORMATNOTE_FILLERS( formatNote );	/* bug 20000114-034 by dmeyer in v9.1b */

    formatNote.newtype = newBlockType;
    rlLogAndDo (pcontext, (RLNOTE *)&formatNote, bHandle, 0, PNULL);

    /* Advance the hiwater mark                                   */

    INITNOTE(bkHwmNote,RL_BKHWM,RL_PHY);
    bkHwmNote.newhwm = pobjectBlock->hiWaterBlock + 1;
    rlLogAndDo(pcontext, (RLNOTE *)&bkHwmNote, objHandle,0,PNULL);

    return (bHandle);

}  /* end bkMakeBlock */


/* PROGRAM: bkFormatBlockDo - Format a database block as given block type
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkFormatBlockDo (
	dsmContext_t	*pcontext,
	RLNOTE		*pnote,
	BKBUF		*pblk,           /* Ptr to block to be formatted */
	COUNT		 dlen _UNUSED_,      /* Not used - should be 0     */
	TEXT		*pdata _UNUSED_)     /* Not used - should be PNULL */
{

    bkclear(pcontext, pblk);
    
    pblk->BKTYPE = ((BKFORMATNOTE *)pnote)->newtype;
    pblk->BKFRCHN = NOCHN;
    
    if (((BKFORMATNOTE *)pnote)->newtype == RMBLK)
    {
	rmFormat (pcontext, pblk, (ULONG)xlng((TEXT *)&pnote->rlArea) );
    }

}  /* end bkFormatBlockDo */


/* PROGRAM: bkFormatBlockUndo - Undo of a block format always results in a 
 *                              free block.
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkFormatBlockUndo(
	dsmContext_t	*pcontext,
	RLNOTE		*pnote _UNUSED_,
	BKBUF		*pblk, /* Ptr to block */
	COUNT		 dlen _UNUSED_,      /* Not used - should be 0     */
	TEXT		*pdata _UNUSED_)     /* Not used - should be PNULL */
{
    bkclear(pcontext, pblk);
    pblk->BKTYPE = EMPTYBLK;
    pblk->BKFRCHN = NOCHN;
    pblk->BKNEXTF = 0;

}  /* end bkFormatBlockUndo */


/* PROGRAM: bkMakeBlockDo - Do routine to create a block in buffer pool
 *                          without reading it from disk.
 *
 * RETURNS: DSMVOID
 */
DSMVOID 
bkMakeBlockDo(
	dsmContext_t	*pcontext,
	RLNOTE		*pnote,
	BKBUF		*pblk,      /* Null */
	COUNT		 dlen _UNUSED_,       /* zero */
	TEXT		*pdata _UNUSED_)     /* Null */
{

    bmBufHandle_t bufHandle;

    if(((BKMAKENOTE *)pnote)->area == DSMAREA_TEMP)
      return;

    bufHandle = bmLocateBlock(pcontext,
                              ((BKMAKENOTE *)pnote)->area,
                              ((BKMAKENOTE *)pnote)->dbkey, TOCREATE );
    pblk = bmGetBlockPointer(pcontext,bufHandle);
    /* Mark block as modified in case this is last note written to an
       ai extent.                                                       */

    /* Support Roll Forward Retry */
    stnclr ((TEXT *)&pblk->bk_hdr, sizeof(struct bkhdr));
    pblk->BKDBKEY = ((BKMAKENOTE *)pnote)->dbkey;
    pblk->BKTYPE = EMPTYBLK;
    pblk->BKFRCHN = NOCHN;
    pblk->BKUPDCTR = 0;

    bmMarkModified(pcontext, bufHandle,0);
    
    /* Since bkloc returns an exclusively locked block
       we must release the lock here so that it gets released
       during the redo phase of warm start.                  */

    bmReleaseBuffer(pcontext, bufHandle);

}  /* end bkMakeBlockDo */


/* PROGRAM: bkHwmDo - Do routine to advance the database hi-water marK
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkHwmDo (
	dsmContext_t	*pcontext,
	RLNOTE		*pnote,
	BKBUF		*pobjectBlock,
	COUNT		 dlen _UNUSED_,
	TEXT		*pdata _UNUSED_)
{
    bkAreaDesc_t       *pbkAreaDesc;

    if (((BKHWMNOTE *)pnote)->newhwm > 
        ((objectBlock_t *)pobjectBlock)->hiWaterBlock)
    {
        pbkAreaDesc = BK_GET_AREA_DESC(pcontext->pdbcontext,
                                       xlng((TEXT *)&pnote->rlArea));

	((objectBlock_t *)pobjectBlock)->hiWaterBlock =
                                         ((BKHWMNOTE *)pnote)->newhwm;
        pbkAreaDesc->bkHiWaterBlock    = ((BKHWMNOTE *)pnote)->newhwm;
 
    }

}  /* end bkHwmDo */

/* PROGRAM: bkGetTotBlks - Count total number of blocks in the database by
 *                         adding the totalBlocks value for each area.
 *
 * RETURNS: Total number of blocks in database
 */
LONG
bkGetTotBlks(dsmContext_t *pcontext)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    ULONG             numAreaSlots = 0;
    bkAreaDesc_t    *pbkAreaDesc = NULL;
    ULONG            area = 0;
    bmBufHandle_t    bmHandle = 0;
    struct  bkbuf   *pbk = NULL;
    LONG            totblocks = 0;
 
    numAreaSlots = XBKAREADP
          (pdbcontext, pdbcontext->pbkctl->bk_qbkAreaDescPtr)->bkNumAreaSlots;
 
    for ( area = 1; area < numAreaSlots; area++ )
    {
        pbkAreaDesc =  BK_GET_AREA_DESC(pdbcontext, area);
        if( pbkAreaDesc == NULL )
            continue;
 
        if ( pbkAreaDesc->bkAreaType != DSMAREA_TYPE_DATA )
            continue;
 
        bmHandle = bmLocateBlock(pcontext, area,
                                 BLK2DBKEY(pbkAreaDesc->bkAreaRecbits), TOREAD);
        pbk = bmGetBlockPointer(pcontext,bmHandle);
        totblocks += ((objectBlock_t *)pbk)->totalBlocks;
        bmReleaseBuffer(pcontext,bmHandle);
    }
    return totblocks;
}

/* PROGRAM: bkDeleteTempObject - Splice object into the free chain
   for the temporary area              */
DSMVOID
bkDeleteTempObject(dsmContext_t *pcontext, DBKEY firstBlock, DBKEY lastBlock,
		   LONG numBlocks)
{
  bmBufHandle_t  objHandle,lastBlockHandle;
  objectBlock_t *pobjectBlock;
  struct  bkbuf   *pbk;  

  if(firstBlock == 0)
    return;

  objHandle = bmLockBuffer(pcontext,DSMAREA_TEMP,
		     BLK2DBKEY(BKGET_AREARECBITS(DSMAREA_TEMP)) ,
			   TOMODIFY,1);
  pobjectBlock = (objectBlock_t *)bmGetBlockPointer(pcontext,objHandle);

  lastBlockHandle = bmLockBuffer(pcontext,DSMAREA_TEMP,lastBlock,
				 TOMODIFY,1);
  pbk = bmGetBlockPointer(pcontext, lastBlockHandle);

  pbk->bk_hdr.bk_nextf = pobjectBlock->chainFirst[FREECHN];
  pobjectBlock->chainFirst[FREECHN] = firstBlock;
  pobjectBlock->numBlocksOnChain[FREECHN] += numBlocks;
  bmMarkModified(pcontext, objHandle,0);
  bmMarkModified(pcontext, lastBlockHandle,0);
  bmReleaseBuffer(pcontext,lastBlockHandle);
  bmReleaseBuffer(pcontext,objHandle);
  
  return;
}

  
