
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
#include "dbpub.h"
#include "bmpub.h"  /* for bmReleaseBuffer call */
#include "lkpub.h"  
#include "ixmsg.h"
#include "dbcontext.h"
#include "scasa.h"   /* *** TODO: merge with scstat contants */
#include "cxpub.h"
#include "cxprv.h"

#include "utspub.h"
#include "ompub.h"
#include "usrctl.h"
#include "objblk.h"

/**** LOCAL FUNCTION PROTOTYPE DEFINITIIONS ****/


/**** END LOCAL FUNCTION PROTOTYPE DEFINITIONS ****/


/***************************************************************
*** CXFIL - the routines in this file all deal with accessing 
***         the master block for index use         
***
***
*** cxCreateIndex
***              create a new index file, assign an index number
***      and initialize the master block.
***
***********************************************************/


/*******************************************************/
/* PROGRAM: cxCreateIndex -- create a new index file
 *
 * RETURNS: the index number assigned to the new index.
 *          It will be > 0 except for virtual index creates.
 */

int
cxCreateIndex(
    dsmContext_t    *pcontext,
    int              ixunique _UNUSED_,  /* 1=no duplicates allowed. */
    int              ixnum,        /*  if INVALID_KEY_VALUE, use next
                                        available idx, else use this one.
                                        Must be INVALID_KEY_VALUE for
                                        ALL callers except scdbg because
                                        it isn't supported on the multi-
                                        user interprocess communication
                                        messages, nor in after-image
                                        roll forward */
    ULONG            area,
    dsmObject_t      tableNum,
    DBKEY           *pobjectRoot)
{
     DBKEY         ixdbkey;
     dsmStatus_t   rc;
     bmBufHandle_t  objHandle;
     objectBlock_t  *pobjectBlock;
     COUNT         newIndex;

    TRACE_CB(pcontext, "cxCreateIndex")

    if (pcontext->pdbcontext->resyncing || lkservcon(pcontext))
    FATAL_MSGN_CALLBACK(pcontext, ixFTL017); /* cxfilc during resync */

#ifdef VST_ENABLED
    /* siphon off VSI requests to create an index */
    if (SCIDX_IS_VSI(ixnum))
    {
       /* return the requested index number for "virtual" create */
       return ixnum;
    }
#endif  /* VST ENABLED */

    rlTXElock (pcontext,RL_TXE_UPDATE,RL_MISC_OP);

    if (ixnum == INVALID_KEY_VALUE)
    {   /* the caller wants the next available index, so scan   */
        /* the index table to find an available index slot      */
        rc = omGetNewObjectNumber(pcontext,DSMOBJECT_MIXINDEX,
				  tableNum, &newIndex);

        if (rc == DSM_S_MAX_INDICES_EXCEEDED)
        {
            /* end the microtransaction cause we are gonna fatal */
            rlTXEunlock (pcontext);
            /* attempt to define too many indices */
            MSGN_CALLBACK(pcontext, ixFTL006);
        }
        ixnum = newIndex;
    }

    /* get a new free block and stick it in the index slot  */
    /* initialize it as the lone master             */
    /* BUM - Assuming int ixnum < 2^15 */
    ixdbkey = cxCreateMaster(pcontext, area, (DBKEY) 0, (COUNT)ixnum, (GTBOOL) 1);
    *pobjectRoot = ixdbkey;  /* return pointer to the "root" block */


    rlTXEunlock (pcontext);
    /* See if we need to create an statistics index  */
    /* Get the object block of the index area to get the root
     of the index selectivity statistics index   */
    objHandle = bmLocateBlock(pcontext, area,
                            BLK2DBKEY(BKGET_AREARECBITS(area)), TOREAD);
    pobjectBlock = (objectBlock_t *)bmGetBlockPointer(pcontext,objHandle);

    if(!pobjectBlock->statisticsRoot)
    {
        DBKEY  statisticsRoot;

        bmReleaseBuffer(pcontext,objHandle);
        rlTXElock (pcontext,RL_TXE_SHARE,RL_MISC_OP);
        statisticsRoot = cxCreateMaster(pcontext,area,0,
					SCIDX_INDEX_STATS,(GTBOOL)1);
        objHandle = bmLocateBlock(pcontext, area,
                            BLK2DBKEY(BKGET_AREARECBITS(area)), TOREAD);
        pobjectBlock = (objectBlock_t *)bmGetBlockPointer(pcontext,objHandle);
        bkReplace(pcontext,objHandle,(TEXT *)&pobjectBlock->statisticsRoot,
              (TEXT *)&statisticsRoot, sizeof(statisticsRoot));
        bmReleaseBuffer(pcontext,objHandle);
        rlTXEunlock (pcontext);
    }
    else
          bmReleaseBuffer(pcontext,objHandle);


    /* return index file number */

    return ixnum;
}  /* end cxCreateIndex */


/********************************************************************/
/* PROGRAM: cxCreateMaster - create and init an index ROOT block (ixmgr.h)
 *
 * RETURNS: the dbkey of the new root block
 */
DBKEY
cxCreateMaster (
    dsmContext_t    *pcontext,
    ULONG            area,      /* create index root in this area */
    DBKEY            olddbk,    /* the dbkey of the old mstr block */
    COUNT            ixnum,     /* the index number        */
    GTBOOL           ixbottom)  /* 1 if the new mstr blk will be at */
                                /* the bottom of the index      */
{

     struct ixblk    *pixblk;
     IXNEWM           note;
     DBKEY            newdbkey;
     bmBufHandle_t    bufHandle;

    /* get a new free block */
    bufHandle = bkfrrem(pcontext, area,IDXBLK,FREECHN);
    pixblk = (struct ixblk *)bmGetBlockPointer(pcontext,bufHandle);
    newdbkey = pixblk->BKDBKEY;

    /* set up a note */

    INITNOTE( note, RL_IXMSTR, RL_PHY );
    INIT_S_IXNEWM_FILLERS( note );	/* bug 20000114-034 by dmeyer in v9.1b */
    note.ixbottom = ixbottom;
    note.ixnum = ixnum;
    note.ixdbk = olddbk;
    rlLogAndDo (pcontext, (RLNOTE *)&note, bufHandle, 0, PNULL );

    bmReleaseBuffer(pcontext, bufHandle); /* release the buffer */

    return(newdbkey);
}

/********************************************************************/
/* PROGRAM: cxLogIndexCreate - Write an index create note the bi
 *
 * RETURNS: 
 */
LONG cxLogIndexCreate(dsmContext_t *pcontext,
                      dsmArea_t    area,
                      dsmIndex_t indexNumber,
		      dsmTable_t tableNumber)
{
    IXFINOTE      note;
    /* write a logical note */

    INITNOTE( note, RL_IXFILC, RL_LOGBI );
    INIT_S_IXFINOTE_FILLERS( note );	/* bug 20000114-034 by dmeyer in v9.1b */

    note.ixnum = indexNumber;
    note.table = tableNumber;
    note.ixArea = area;
    /* Following not needed for v9    */
    note.ixunique = 0;      
    note.ixdbk = 0;

    rlLogAndDo(pcontext, (RLNOTE *)&note, 0, (COUNT)0, PNULL );

    return 0;
}

#if MSC
#pragma optimize("", on)
#endif


