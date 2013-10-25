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
#include "cxpub.h"
#include "cxprv.h"
#include "dbcontext.h"
#include "dsmpub.h"
#include "ompub.h"

#include "usrctl.h"  /* temp for DSMSETCONTEXT */
#include "objblk.h"


/*
 *	PROGRAM: cxRemoveFromDeleteChain - Takes a block off the index
 *			 delete chain clears the delete locks and if all are gone
 *			 puts it on the area free chain.  
 *
 *	RETURNS: 0
 */
int
cxRemoveFromDeleteChain(
	dsmContext_t	*pcontext,
        dsmObject_t      tableNum,
	ULONG            area )
{
	bmBufHandle_t  bHandle;
	LONG		   rc;

	bHandle = bkfrrem(pcontext, area, IDXBLK, LOCKCHN);
	if( bHandle != 0 )
	{
		rc = cxDelLocks(pcontext, bHandle, tableNum, (int)0);
	if( rc == 0 )
	{
	   /* cxDelLocks removed all entries and in the process
		  freed the buffer lock on the block.  So we can just
		  return now											*/
	   return(0);
	}
	else if ( rc == 1 )
	{
	   /* Block is all index holds but for at least one active tx */
	   /* So put the block on the bottom of the index delete chain */
	   bkrmbot(pcontext, bHandle);
	}
	else
	{
	   /* Block has some real index entries in it.	So just remove
		  it from the index delete chain which has already been done */
	   ;
	}
	bmReleaseBuffer(pcontext, bHandle);
	}
	return ( 0 );
}

/***********************/
/*	PROGRAM: cxGetNewBlock - get a new index block off the free chain 
 *
 *	RETURNS: 0
 */
int
cxGetNewBlock(
	dsmContext_t	*pcontext,
	dsmTable_t       table,
	dsmIndex_t       index,
	ULONG            area,
	cxbap_t         *pbap)
{
  pbap->bufHandle = bkfrrem(pcontext, area, IDXBLK, FREECHN);
  pbap->pblkbuf = (cxblock_t *)bmGetBlockPointer(pcontext,pbap->bufHandle);
  pbap->blknum = pbap->pblkbuf->BKDBKEY;
  if(area == DSMAREA_TEMP)
  {
    omTempObject_t  *ptemp;
    bmBufHandle_t    bmHandle;
    bkbuf_t          *pbk;

    ptemp = omFetchTempPtr(pcontext,DSMOBJECT_MIXINDEX,
			   index,table);

    bmHandle = bmLockBuffer(pcontext,DSMAREA_TEMP, ptemp->lastBlock,
			    TOMODIFY,1);
    pbk = bmGetBlockPointer(pcontext,bmHandle);
    pbk->bk_hdr.bk_nextf = pbap->pblkbuf->bk_hdr.bk_dbkey;
    bmMarkModified(pcontext,bmHandle,0);  
    bmReleaseBuffer(pcontext,bmHandle); 
    ptemp->lastBlock = pbap->pblkbuf->bk_hdr.bk_dbkey;
  }
  return(0);
}

/***********************************************************************/
/* PROGRAM: cxFreeBlock - free a cx block and return it to the free chain
 *
 * RETURNS: 0
 */
int
cxFreeBlock(
	dsmContext_t  *pcontext,
	cxbap_t       *pbap)
{
	UCOUNT	mode;

	mode = RL_PHY;
	cxDeleteBlock (pcontext, pbap->bufHandle, mode, 0 );
	return(0);
}


/**********************************************************/
/* PROGRAM: cxProcessDeleteChain - process all  the 
 *          delete lock chain blocks ie, till as many
 *          blocks as are on the chain are processed
 *
 * RETURNS: DSMVOID
 * 
 */
DSMVOID
cxProcessDeleteChain(
        dsmContext_t    *pcontext, 
        dsmObject_t      tableNum,
        dsmArea_t        area)
{
    int           chainLength;
    bkAreaDesc_t *pbkAreaDesc;

    pbkAreaDesc = BK_GET_AREA_DESC(pcontext->pdbcontext, area);

    chainLength = pbkAreaDesc->bkNumBlocksOnChain[LOCKCHN];

    while( chainLength-- )
    {
        rlTXElock(pcontext,RL_TXE_UPDATE,RL_MISC_OP);
        cxRemoveFromDeleteChain(pcontext, tableNum,  area);
        rlTXEunlock(pcontext);
    }

}  /* end cxProcessDeleteChain */

