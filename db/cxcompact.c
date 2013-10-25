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

#include <stdio.h>
#include "dbconfig.h"
#include "dbcontext.h"
#include "dsmpub.h"
#include "bmpub.h"
#include "cxpub.h"
#include "cxprv.h"
#include "stmpub.h"         /* for pdsmStoragePool */
#include "tmmgr.h"          /* for TMACC */
#include "usrctl.h"         /* for userctl status */
#include "utcpub.h"         /* for utpctclc */
#include "utspub.h"


/* Local constant definitions */
 
#define CX_S_COMPACT_CHECK_FAILED -2
#define CX_S_COMPACT_FAILED       -1
#define CX_S_SUCCESS               0
 
#ifdef CXDEBUG_COMPACT
#define NON_LEAF_IS 4
#endif /* CXDEBUG_COMPACT */
 
/* local definitions */
typedef struct cxBlockStats
       {
#ifdef DBKEY64
           psc_long64_t    blkNum;
           psc_long64_t    numEntries;
           psc_long64_t    lenOfEntries;
           psc_long64_t    delChainSize;
#else
           ULONG    blkNum;
           ULONG    numEntries;
           ULONG    lenOfEntries;
           ULONG    delChainSize;
#endif  /* DBKEY64 */
       } cxBlockStats_t;

/* Local function prototypes */
LOCALF dsmStatus_t
cxCompactProcessParent(
        dsmContext_t    *pcontext,
        cxcursor_t      *pcursor,
        LONG            percentUtilization, /* IN % required space util. */
#ifdef DBKEY64
        psc_long64_t    *pblockCounter,      /* OUT # of blocks processed */
        psc_long64_t    *ptotalBytes,        /* OUT # of bytes utilizeded */
#else
        LONG            *pblockCounter,      /* OUT # of blocks processed */
        LONG            *ptotalBytes,        /* OUT # of bytes utilizeded */
#endif  /* DBKEY64 */
        downlist_t      *pdownlst,
	dsmObject_t     tableNum,
        LONG            processRequest);     /* compact blks or blk report? */
 
LOCALF int
cxCompactFindNextParent(
        dsmContext_t  *pcontext,           /* IN database context */
        cxcursor_t    *pcursor,
        downlist_t    *pdownlst,
        DBKEY         *plastProcessedParent);
 
LOCALF dsmStatus_t
cxCompactBlocks(dsmContext_t *pcontext,
                cxcursor_t   *pcursor,
                cxbap_t      *pbapParent,
                cxbap_t      *pbapLeft,
                cxbap_t      *pbapRight,
                LONG          desiredUtil,
		dsmObject_t   tableNum,
                downlist_t   *pdownlst);
                
LOCALF dsmStatus_t
cxCompactBlockReport(dsmContext_t   *pcontext,
                     cxcursor_t     *pcursor,      /* cursor for index */
                     cxbap_t        *pbap,         /* blk access parms */
                     cxBlockStats_t *pblkStats);   /* block idx stats */
 
LOCALF int
cxKeyToCursor(
        dsmContext_t  *pcontext,           /* IN database context */
        cxcursor_t    *pcursor,
        TEXT          *pkeyPrefix,
        TEXT          *pos);

/* End of local function prototypes */

/*************************************************************************/
/* PROGRAM: cxCompactProcessParent - the requested parent blk by its dbkey
*           and area is located and all its sons are compacted, if needed.
*           The "sons" are the blocks pointed by the parent.  The sons
*           are processed from left to right two at a time.
*           Processing of a pair means moving entries from the right son
*           to the left son, if the left son has space and does not meet
*           the required compacting percentage.
*           Processing each pair of sons is done within a microtransaction.
*           Unfortunnately, block locks may not be held beyond a 
*           microtransaction. So, the lock on the parent must be released
*           after processing a pair of sons. This allow somone else to
*           modify the parent block, before the next pair is treated.
*           For this reason the parent is located and locked again
*           the information in the cursor allows to checkmif the parent
*           was modified. If not, the processing of the next pair is continued
*           and so on and so on for all the sons of the parent.
*           In the rare case that the parent was changed, this procedure
*           will terminate.  The caller will use the cursor to reposition
*           to a parent in the same level, eithr the same parent or another.
* RETURNS:
*/
LOCALF dsmStatus_t
cxCompactProcessParent(
        dsmContext_t    *pcontext,
        cxcursor_t      *pcursor,
        LONG             percentUtilization, /* IN % required space util. */
#ifdef DBKEY64
        psc_long64_t    *pblockCounter,      /* OUT # of blocks processed */
        psc_long64_t    *ptotalBytes,        /* OUT # of bytes utilizeded */
#else
        LONG            *pblockCounter,      /* OUT # of blocks processed */
        LONG            *ptotalBytes,        /* OUT # of bytes utilizeded */
#endif  /* DBKEY64 */

        downlist_t      *pdownlst,
	dsmObject_t      tableNum,
        LONG             processRequest)    /* compact blks or blk report? */
{
        usrctl_t    *pusr = pcontext->pusrctl;

        cxbap_t     bapParent,     /* parent block access parameters  */
                    bapLeft,       /* left block access parameters    */
                    bapRight,      /* right block access parameters   */
                    bap;           /* block access parameters         */
        cxblock_t  *pBlkParent,    /* pointer to the parent block     */
                   *pBlkLeft,      /* pointer to the left block       */
                   *pBlkRight,     /* pointer to the right block      */
                   *pixblk;        /* blk ptr for finding next parent */
        int	    blockSize,
                    blockCapacity,
                    entrySize,
                    transaction = 0,
                    cleanLeftBlock = 1,      /* 1 so first left is cleaned */
                    upgradeLock = 0,         /* 0 so no lock upgrade */
                    ret;
        ULONG       area = pcursor->area;
        DBKEY       parentDbk;
        findp_t    *pfndp,        /* pointer to the FIND P. B. */
                    fndp;         /* parameter block for the FIND */
        TEXT        keyPrefix[4096],
                   *pos,
                   *pLeftPos,
                   *plast;         /* last used position in the block */
        dsmStatus_t returnCode;
        LONG        retcxdel, 
                    leftUpdctr,        /* update ctr for left blk */
                    leftIndexNum, 
                    rightUpdctr,       /* update ctr for right blk */
                    rightIndexNum;
#ifdef DBKEY64
         psc_long64_t       deletedBlocks = 0;
#else
         LONG       deletedBlocks = 0;
#endif  /* DBKEY64 */

        cxBlockStats_t indexStats;
        int            checkFirstLeft = 1,
                       checkRootBlock = 1;

/*	int      cursorHeaderSize = 3, */
	int      cursorHeaderSize = FULLKEYHDRSZ,
/*	         cursorKsOffset = 1; */
	         cursorKsOffset = KS_OFFSET;
       
    indexStats.blkNum = 1;

    /* initialize the pointer to the block buffer
       so that blocks are counted correctly later */
    bapRight.pblkbuf = 0;

    /* find the capacity of a block */
    blockSize = pcontext->pdbcontext->pdbpub->dbblksize; 
    blockCapacity = blockSize - (sizeof(struct bkhdr) + sizeof(struct ixhdr));

    /* start a transaction */
    tmstrt(pcontext);
    transaction = 1;

    /* get the parent block  with TOREADINTENT lock */
    parentDbk = (&pdownlst->level[pdownlst->currlev])->dbk = pcursor->blknum;
    cxGetIndexBlock(pcontext, &bapParent, area, parentDbk, (UCOUNT)TOREADINTENT);
    pBlkParent = bapParent.pblkbuf;

    /* update status blk counter */
    pusr->uc_counter++;

    /* Check if the parent block was not changed since the cursor was set up */
    if ((ULONG)pBlkParent->BKUPDCTR != pcursor->updctr /* same updat counter? */
    || pBlkParent->IXNUM != pcursor->ixnum      /* same ixnum? */
    || (int)(pBlkParent->BKTYPE) != IDXBLK)     /* is it an index block? */
    {
        /* the block pointed by the cursor was changed, try to get it again */
        returnCode = DSM_S_SUCCESS;
        bmReleaseBuffer(pcontext, bapParent.bufHandle);
        goto end;
    }

    pos = &pBlkParent->ixchars[0];    /* start of index entries in block */
    plast = &pBlkParent->ixchars[pBlkParent->IXLENENT - 1]; /* last used byte */

    /* initialize the parameters of the find */
    pfndp = &fndp;  /* load pointer to the FIND Parameter structure */

    pfndp->pinpkey = pcursor->pkey + cursorHeaderSize;
/*    pfndp->inkeysz = pcursor->pkey[cursorKsOffset]; */
    pfndp->inkeysz = xct(&pcursor->pkey[cursorKsOffset]);
    pfndp->pblkbuf = pBlkParent;
    ret = cxFindEntry(pcontext, pfndp); /* search for the key in the block*/

    if (ret && pfndp->pprv != 0)
    {
        /* if exact key not found and there is a previous entry */
        /* no previous entry is possible if key was deleted without*/
        /* update of upper level                      */
        pos = pfndp->pprv;/* point to the previous entry to go down*/
    }
    else
    {
        /* point to the current entry to go down */
        pos = pfndp->position;
    }

    /* initialize size of entry variable */
    entrySize = 0; 
    while (pos < plast)
    {
        for (; pos < plast; pos += entrySize)
        {
            /* get blknum of left block */
            cxGetDBK(pcontext, pos, &bapLeft.blknum);
    
            /* get the left block  with TOREADINTENT lock */
            cxGetIndexBlock(pcontext, &bapLeft, area, bapLeft.blknum,
                                                        (UCOUNT)TOREADINTENT);
            pBlkLeft = bapLeft.pblkbuf;
            /* store the update counters and index #s */
            leftUpdctr = pBlkLeft->BKUPDCTR;
            leftIndexNum = pBlkLeft->IXNUM;

            if (bapRight.pblkbuf != 0)
            {
                /* count processed blocks, but don't count the left blk 
                   twice if the right blk was removed */
                *pblockCounter += 1;
                *ptotalBytes += pBlkLeft->IXLENENT;
                /* update status blk counter */
                pusr->uc_counter++;
            }
            
            /* get the prefix that is missing in the next key */
            entrySize = ENTRY_SIZE_COMPRESSED(pos);
            if (pos + entrySize < plast)
                cxSkipEntry(pcontext, pfndp, pos, pos + entrySize, &keyPrefix[0]);

            /* check if the block is compacted enough and */
            /* check to see if block is on the idx delete chain */
            if ((percentUtilization > 
                (int)(((DOUBLE)(pBlkLeft->IXLENENT) / (DOUBLE)blockCapacity) * 100))  && !(pBlkLeft->IXBOT && pBlkLeft->bk_hdr.bk_frchn == LOCKCHN))
            {
                /* left block is not up to the required percet or more */
                break;
            }
            else
nextLeft:
                /* release lock of left */
                bmReleaseBuffer(pcontext, bapLeft.bufHandle);
        }
        /* here we either at the end of the parent, or we found a left that
           needs compacting */
        if (pos > plast)
        {
           /* go to end, parent will be released there after the
             cursor has been updated */
            goto endOfParent;  /* we reached the end of the parent */
        }
        
        /* store the left block position in the parent in case the right block
           becomes empty and is removed */
        pLeftPos = pos;

        /* get the right block */
        pos += entrySize;
        if (pos > plast)
        {
            /* release lock of left and go to end, parent will be
               released there after the cursor has been updated */
            bmReleaseBuffer(pcontext, bapLeft.bufHandle);
            goto endOfParent;  /* we reached the end of the parent */
        }

        /* There is a right block, get it */
        /* get blknum of right block */
        cxGetDBK(pcontext, pos, &bapRight.blknum);

        /* get the right block  with TOREADINTENT lock */
        cxGetIndexBlock(pcontext, &bapRight, area, bapRight.blknum,
                                                        (UCOUNT)TOREADINTENT);
        pBlkRight = bapRight.pblkbuf;
        /* store the update counters and index #s */
        rightUpdctr = pBlkRight->BKUPDCTR;
        rightIndexNum = pBlkRight->IXNUM;
        *pblockCounter += 1;  /* count processed blocks */
        *ptotalBytes += pBlkRight->IXLENENT;
        /* update status blk counter */
        pusr->uc_counter++;

        /* update the cursor with the key pointing to the right */
        ret = cxKeyToCursor(pcontext, pcursor, &keyPrefix[0], pos);

        /* check to see if block is on the idx delete chain */
        if (pBlkRight->IXBOT && pBlkRight->bk_hdr.bk_frchn == LOCKCHN)
        {
            /* this block is on the idx delete chain, skip it */
            entrySize = ENTRY_SIZE_COMPRESSED(pos);
            /* release lock of right */
            bmReleaseBuffer(pcontext, bapRight.bufHandle);
            goto nextLeft;
        }
 
        /* release lock of left, right, and parent so we can
           start a mtx */
        bmReleaseBuffer(pcontext, bapRight.bufHandle);
        bmReleaseBuffer(pcontext, bapLeft.bufHandle);
        bmReleaseBuffer(pcontext, bapParent.bufHandle);

        /* start a Micro transaction */
        rlTXElock(pcontext,RL_TXE_UPDATE,RL_MISC_OP);

        /* get the left, right, and parent blk with TOMODIFY lock */
        cxGetIndexBlock(pcontext, &bapParent, area, bapParent.blknum,
                                                        (UCOUNT)TOMODIFY);
        cxGetIndexBlock(pcontext, &bapLeft, area, bapLeft.blknum,
                                                        (UCOUNT)TOMODIFY);
        cxGetIndexBlock(pcontext, &bapRight, area, bapRight.blknum,
                                                        (UCOUNT)TOMODIFY);
        
         /* Check that three blks were not changed since they were released.
            Make sure update counter, block type, and index number are teh
            same for all three */
         if (((pBlkParent->BKUPDCTR != pcursor->updctr
              || pBlkParent->IXNUM != pcursor->ixnum      
              || (int)(pBlkParent->BKTYPE) != IDXBLK)) || /* or the left */
            ((pBlkLeft->BKUPDCTR != leftUpdctr
              || pBlkLeft->IXNUM != leftIndexNum
              || pBlkLeft->BKTYPE != IDXBLK)) || /* or the right */
            ((pBlkRight->BKUPDCTR != rightUpdctr
                 || pBlkRight->IXNUM != rightIndexNum
                 || pBlkRight->BKTYPE != IDXBLK)))
         {
             /* a block was changed, release the left, right, and parent
                try to get parent again */
             bmReleaseBuffer(pcontext, bapRight.bufHandle);
             bmReleaseBuffer(pcontext, bapLeft.bufHandle);
             bmReleaseBuffer(pcontext, bapParent.bufHandle);
             rlTXEunlock(pcontext);  /* end micro transaction */
             returnCode = DSM_S_SUCCESS;
             goto end;
         }

        /* update the status for vst usage */
        if ((pusr->uc_state ==
                DSMSTATE_COMPACT_NONLEAF) && (pBlkLeft->IXBOT)) 
        {
            /* non-leaf blocks is done, now we are processing
               leaf blocks */
            pusr->uc_state = DSMSTATE_COMPACT_LEAF;
        }

        /* clean up delete locks in blks not on idx del chain */
        if (pBlkRight->IXBOT && (pcursor->unique_index & DSMINDEX_UNIQUE))
        {
           /* clean left blk if it is the first left */
           if (cleanLeftBlock)
           {
               /* blk should be locked exclusive, upgradeLock should
                  be zero */
               retcxdel = cxDelLocks(pcontext, bapLeft.bufHandle, 
				     tableNum, upgradeLock);
               cleanLeftBlock = 0;
           }
           /* blk should be locked exclusive, upgradeLock should
              be zero */
           retcxdel = cxDelLocks(pcontext, bapRight.bufHandle, 
				 tableNum, upgradeLock);
        }

        /******* here we do the compacting                         ****/
        /******* do not release any block, only the right if empty ****/
        /******* if the right was removed bapRight.pblkbuf = 0;    ****/ 
        pdownlst->currlev++;    /* to next level of the B-tree */ 
        /* right block */
        (&pdownlst->level[pdownlst->currlev])->dbk = bapRight.blknum;

        if (processRequest == CX_BLOCK_COMPACT)
        {
            returnCode = cxCompactBlocks(pcontext, pcursor, &bapParent,
                                         &bapLeft, &bapRight,
                                         percentUtilization,
					 tableNum, pdownlst);
        }
        else if (processRequest == CX_BLOCK_REPORT)
        {
           /* report on root block too! */
           if (pBlkParent->IXTOP && checkRootBlock)
           {
               returnCode = cxCompactBlockReport(pcontext, pcursor, &bapParent,
                                                 &indexStats);
               checkRootBlock = 0;
               indexStats.blkNum++;
           }
           if (checkFirstLeft)
           {
               /* report on first left block in level and flag
                  that this has been done */
               returnCode = cxCompactBlockReport(pcontext, pcursor, &bapLeft,
                                           &indexStats);
               checkFirstLeft = 0;
               indexStats.blkNum++;
           }
           returnCode = cxCompactBlockReport(pcontext, pcursor, &bapRight, 
                                             &indexStats);
           indexStats.blkNum++;
        }
        else
        {
            /* incorrect process parent request */
                returnCode = CX_S_COMPACT_FAILED;
                bmReleaseBuffer(pcontext, bapRight.bufHandle);
                bmReleaseBuffer(pcontext, bapLeft.bufHandle);
                bmReleaseBuffer(pcontext, bapParent.bufHandle);
                rlTXEunlock(pcontext);
                goto end;
        }
#if CXDEBUG_COMPACT
        /* turned on for debugging */
        {
            int retCheck;
            if ((retCheck = cxCheckBlock(pcontext, pBlkLeft)) != 0)
            {
                MSGD_CALLBACK(pcontext, "Left block failed check!");
                goto checkReturn;
            }
                
            if ((retCheck = cxCheckBlock(pcontext, pBlkRight)) != 0)
            {
                MSGD_CALLBACK(pcontext, "Right block failed check!");
                goto checkReturn;
            }

            if ((retCheck = cxCheckBlock(pcontext, pBlkParent)) != 0)
            {
                MSGD_CALLBACK(pcontext, "Parent block failed check!");
            }

checkReturn:
            if (retCheck)
            {
                /* a blk was damaged, release left, right, parent 
                   and end mtx.  */
                returnCode = CX_S_COMPACT_CHECK_FAILED;
                bmReleaseBuffer(pcontext, bapRight.bufHandle);
                bmReleaseBuffer(pcontext, bapLeft.bufHandle);
                bmReleaseBuffer(pcontext, bapParent.bufHandle);
                rlTXEunlock(pcontext);
                goto end;
            }
        }
#endif
        pdownlst->currlev--;    /* to parent level of the B-tree */ 

        if (returnCode != CX_S_SUCCESS)
        {
          /* an error occurred during compaction.  release the left,
             right, and parent. end mtx */
           bmReleaseBuffer(pcontext, bapRight.bufHandle);
           bmReleaseBuffer(pcontext, bapLeft.bufHandle);
           bmReleaseBuffer(pcontext, bapParent.bufHandle);
           rlTXEunlock(pcontext);
           goto end;
        }

        /* find new end of block */
        plast = &pBlkParent->ixchars[pBlkParent->IXLENENT - 1];
        if (bapRight.pblkbuf == 0)
        {
            /* right block was empty and removed.
               restore pos to to location of the left block */
            pos = pLeftPos;
            deletedBlocks++;
        }

        /* Update cursor, release all blks and end the transaction here */
        bmReleaseBuffer(pcontext, bapRight.bufHandle);
        bmReleaseBuffer(pcontext, bapLeft.bufHandle);
        pcursor->updctr = (ULONG)pBlkParent->BKUPDCTR;
        bmReleaseBuffer(pcontext, bapParent.bufHandle);
        rlTXEunlock (pcontext);

        /* get back the parent block and see if it was not changed */
        /* get the parent block  with TOREADINTENT lock */
        parentDbk = pcursor->blknum;
        cxGetIndexBlock(pcontext, &bapParent, area, parentDbk,
                                                (UCOUNT)TOREADINTENT);
        pBlkParent = bapParent.pblkbuf;
        
        if ((ULONG)pBlkParent->BKUPDCTR != pcursor->updctr
        || pBlkParent->IXNUM != pcursor->ixnum  /* same ixnum? */
        || (int)(pBlkParent->BKTYPE) != IDXBLK) /* is it an index block? */
        {
            /* the block pointed by the cursor was changed,
            try to get it again */
            bmReleaseBuffer(pcontext, bapParent.bufHandle); 
            returnCode = DSM_S_SUCCESS;
            goto end;
        }
    }

endOfParent:
    /* we reached the end of the parent. get the highest key at the leaf
       which is governed by this parent, save it in the cursor.
       This will help us find the next parent when called again */
    pos -= entrySize;  /* point back to last entry */
    /* update the cursor with the key pointing to the right */
    pixblk = bapParent.pblkbuf;
    if (pixblk->IXNUMENT == 1 && !pixblk->IXBOT)
    {
        bap.bufHandle=0;
        /* there is only one dummy key in the block, in this case
           we need to get all the way to the leaf level to get a key */
        while(!pixblk->IXBOT )
        {
            cxGetDBK(pcontext, &pixblk->ixchars[0], &bap.blknum);
            if (bap.bufHandle != 0 )
                bmReleaseBuffer(pcontext, bap.bufHandle);
            cxGetIndexBlock(pcontext, &bap, area, bap.blknum,
                                                (UCOUNT)TOREAD);
            pixblk = bap.pblkbuf;
        }
        cxKeyToCursor(pcontext, pcursor, &keyPrefix[0], &pixblk->ixchars[0]);
        bmReleaseBuffer(pcontext, bap.bufHandle);
    }
    else
    {
        cxKeyToCursor(pcontext, pcursor, &keyPrefix[0], pos);
    }

    bmReleaseBuffer(pcontext, bapParent.bufHandle);
    returnCode = DSM_S_SUCCESS;

end:
    /* if transaction is open, end it */
    if (transaction)
        tmend(pcontext, TMACC, NULL, (int)0);  /* ??? is this correct? */
    return returnCode;

}  /* cxCompactProcessParent */


/*************************************************************************/
/* PROGRAM: cxCompactIndex - increase space utilization of the index given
*           from the positiuon pointed by the given cursor. Interrupts the
*           process and returns to the caller when the number of processed
*           blocks exceeds the interuptBlockCount.
*
* RETURNS:  DSM_S_SUCCESS               - completed pass of index
*           DSM_S_CTLC                  - ctl-c was pressed
*/
dsmStatus_t
cxCompactIndex(
        dsmContext_t  *pcontext,           /* IN database context */
        dsmCursid_t   *pcursid,            /* IN  pointer to cursor */
        ULONG          percentUtilization, /* IN % required space util. */
#ifdef DBKEY64
        psc_long64_t  *pblockCounter,      /* OUT # of blocks processed */
        psc_long64_t  *ptotalBytes,        /* OUT # of bytes utilized */
#else
        LONG          *pblockCounter,      /* OUT # of blocks processed */
        LONG          *ptotalBytes,        /* OUT # of bytes utilized */
#endif  /* DBKEY64 */
	dsmObject_t    tableNum,
        LONG           processRequest)     /* compact blks or blk report ? */
{
        cxbap_t        bap;               /* blk access params */
        downlist_t     dlist;             /* list down */
        downlist_t    *pdownlst= &dlist;  /* to list down the B-tree*/       
        cxcursor_t    *pcursor;           /* cursor for idx */
        dsmStatus_t    retCode;           /* return code */ 
#ifdef DBKEY64
        psc_long64_t           processBlocks;     /* loop ctl for blk processing */
#else
        LONG           processBlocks;     /* loop ctl for blk processing */
#endif  /* DBKEY64 */
        DBKEY          lastProcessParent = 0; /* last blk used as parent */

    /* get the cursor using the id provided */
    pcursor = cxGetCursor(pcontext, pcursid, 0 );

    CXINITLV(pdownlst);                 /* initialize the list down level */
    pdownlst->area = pcursor->area;

    /* do we have storage for the key */
    if (pcursor->pkey == 0)
    {
        /* allocate enough room to get started, more will be allocated
           later when needed */
        pcursor->pkey = (TEXT *)stRentd(pcontext, pdsmStoragePool, 16);
        pcursor->kyareasz = 16;
    }

    /* initialize blk counter and processing loop control */
    *pblockCounter = 0;
    processBlocks = 1;
    while (processBlocks)
    {
       if (pcursor->level == 0)
       {
           /* it is a new cursor, need to start from the root block */
           pcursor->blknum = pcursor->root;
           cxGetIndexBlock(pcontext, &bap, pcursor->area,pcursor->blknum,
                           (UCOUNT)TOREAD);

           if (bap.pblkbuf->IXBOT)
           {
                /* release the block buffer */
                bmReleaseBuffer(pcontext, bap.bufHandle);
                /* no leaves, we reached the bottom level */
                retCode = DSM_S_REACHED_BOTTOM_LEVEL; 
                processBlocks = 0; /* break while loop */
                continue;
           }

           pcursor->updctr = (ULONG)bap.pblkbuf->BKUPDCTR;
           /* release the block buffer */
           bmReleaseBuffer(pcontext, bap.bufHandle);

           /* set low key in cursor */
/*           pcursor->pkey[0] = 4; */
/*           pcursor->pkey[1] = 1; */
/*           pcursor->pkey[2] = 0; */
           sct(&pcursor->pkey[TS_OFFSET] , FULLKEYHDRSZ + 1);
           sct(&pcursor->pkey[KS_OFFSET] , 1);
           sct(&pcursor->pkey[IS_OFFSET] , 0);
           pcursor->pkey[3] = 0;
       }
       else
       {
           /* it is a continuation, find next parent to continue compacting */
           /* use key in the cursor to traverse the tree to the given level */
           /* find the next parent to process */
           if (cxCompactFindNextParent(pcontext, pcursor, pdownlst,
                                       &lastProcessParent))
           {
              stVacate(pcontext, pdsmStoragePool, (TEXT *)pcursor->pkey);
              pcursor->pkey = NULL;
              /* Done all the way to bottom level */
              retCode = DSM_S_SUCCESS;
              processBlocks = 0; /* break while loop */
              continue;
           }
       }
    
       /* process the parent, finding children to be compacted */
       retCode = cxCompactProcessParent(pcontext, pcursor, percentUtilization,
                      pblockCounter, ptotalBytes, pdownlst, 
					tableNum, processRequest);

       /* check to see if ctrl-c has been pressed */
        if (utpctlc())
        {
             /* control c has been pressed */
			 processBlocks = 0; /* break while loop */
             retCode = DSM_S_USER_CTLC_TASK;
             continue;
        }

       if (pcursor->level == 0)
       {
            /* no more blocks in the level of the parent, go down one level */
            pcursor->level++;
            /* set low key in cursor */
/*            pcursor->pkey[0] = 4; */
/*            pcursor->pkey[1] = 1; */
/*            pcursor->pkey[2] = 0; */
            sct(&pcursor->pkey[TS_OFFSET] , FULLKEYHDRSZ + 1);
            sct(&pcursor->pkey[KS_OFFSET] , 1);
            sct(&pcursor->pkey[IS_OFFSET] , 0);
            pcursor->pkey[3] = 0;
       }

       if (retCode != DSM_S_SUCCESS)
       {
           /* an error has occurred, return the status back */
	   /* break while loop */
	   processBlocks = 0;
       }
    }

    return retCode;

}   /* cxIndexCompact */

/*************************************************************************/
/* PROGRAM: cxCompactFindNextParent - the cursor has the highest key of the current
*           parent and the level of the current parent.  This procedure finds
*           the next parent in the same level of the current parent or in the
*           next level if the current level is done with.
*           The found parent is not locked when this returns.  The cursor
*           is uptodate with the block update counter. It is the caller's
*           responsibility to get the parent block, lock it and check if
*           it was changed since found by this procedure. If not changed
*           it could be used, else the search for the parent should be 
*           repeated.
* RETURNS:
*          0 - Success - the dbkey of the next parent and its level
*                        are returned in pcursor->blknum and pcursor->level.
*                        Also the update counter of the block is saved in
*                        pcursor->updctr.
*          1 - Failed -  we reached the botom level, no more parent available
*/
LOCALF int
cxCompactFindNextParent(
        dsmContext_t  *pcontext,           /* IN database context */
        cxcursor_t    *pcursor,
        downlist_t    *pdownlst,
        DBKEY          *plastProcessedParent)
{
        findp_t     fndp;         /* parameter block for the FIND */
        findp_t    *pfndp=&fndp;  /* pointer to the FIND P. B. */
        inlevel_t  *plevel;
        cxbap_t     bap;          /* block access parameters */
        UCOUNT      lockmode;
        cxblock_t  *pixblk;
        TEXT       *pos;          /* address of the position in the block */
        TEXT       *pfreepos;
        DBKEY       nextDBK;      /* value to be pushed on stack */
        TEXT       *pnxtprv;      /* the position of nxt or prev entry */
        TEXT        lowkey = 0;
        int         nextEntryExists;
        int         ret;
/*        int      entryHeaderSize = ENTHDRSZ, */
/*	         ksOffset = 1; */

#ifdef CXDEBUG_NONLEAF
    /* turn this on to only process root and its children */
    return DSM_S_REACHED_BOTTOM_LEVEL;

#endif /* CXDEBUG_COMPACT */


fromtop:
    CXINITLV(pdownlst);                 /* initialize the list down level */
    plevel = &pdownlst->level[0];

    lockmode = TOREAD;
    plevel->dbk = pcursor->root; /* start with root block */
/*    pfndp->pinpkey = pcursor->pkey + entryHeaderSize; */
    pfndp->pinpkey = pcursor->pkey + FULLKEYHDRSZ;
/*    pfndp->inkeysz = pcursor->pkey[ksOffset]; */
    pfndp->inkeysz = xct(&pcursor->pkey[KS_OFFSET]);


traverse:
    bap.blknum = plevel->dbk;           /* blknum of starting level block */
    /* Each pass through this loop will move one level down the tree.
       This terminates when theparent level is reached                 */
    for(;;)
    {
        /* read the next block to be examined */
        bap.bufHandle = bmLocateBlock(pcontext, pcursor->area, bap.blknum,
                                      lockmode);
        bap.pblkbuf = (cxblock_t *)bmGetBlockPointer(pcontext,bap.bufHandle);
        pixblk = bap.pblkbuf;
        pfndp->bufHandle = bap.bufHandle; /* Handle to buffer in buffer pool */
        pfndp->pblkbuf = pixblk;        /* point to the block */
        pfndp->blknum = bap.blknum;     /* blknum of the block */
 
        /* search for the key in the block*/
        ret = cxFindEntry(pcontext, pfndp);
        pfreepos = pixblk->ixchars + pixblk->IXLENENT;
        if (pcursor->level == pdownlst->currlev)
            break; /* we reached the level of the current parent */

        if (ret && pfndp->pprv != 0)
        {
            /* if exact key not found and there is a previous entry */
            /* no previous entry is possible if key was deleted without*/
            /* update of upper level                      */
            pos = pfndp->pprv;/* point to the previous entry to go down*/
        }
        else
        {
            pos = pfndp->position; /* point to the current entry to go down */
        }
         /* get blknum of block to go down with */
        cxGetDBK(pcontext, pos, &bap.blknum);
        /* check if the block is "SAFE" */
        nextEntryExists = 1; /* assume safe */
        nextDBK = (DBKEY)-1;   /* assume no prev nor next */
        pnxtprv = pos + ENTRY_SIZE_COMPRESSED(pos);     /*to next entry */
        if (pnxtprv == pfreepos)      /* there is no next entry */
            nextEntryExists = 0;       /* not safe - no next entry in the block */
        else
           cxGetDBK(pcontext, pnxtprv, &nextDBK); /* nextDBK = next block */
        if (nextEntryExists)
            cxReleaseAbove(pcontext, pdownlst); /* release all predecessors */
        plevel++;       /* to next level of the B-tree */
        pdownlst->currlev++;    /* to next level of the B-tree */
        pdownlst->locklev++; /* count locked predecessor levels */
 
        plevel->nxtprvbk = nextDBK; /* next or previous dbk to list down */
        plevel->dbk = bap.blknum;   /* current block to list down */
    }

    /* check if botom level was reached.  Parent could not be at the bottom */
    if (pixblk->IXBOT)
    {
        cxReleaseAbove(pcontext, pdownlst);/* safe, release all predecessors */
        bmReleaseBuffer(pcontext, bap.bufHandle); /* release the block buffer */
        /* no more parents, we reached the bottom level */
        return DSM_S_REACHED_BOTTOM_LEVEL;
    }

    /* at this point we are in the level of the current parent
       looking for the next parent */
    pos = pfndp->position; /* point to the found entry  of current parent*/

    if  (ret == 0)     /* if key of current parent was found */
        pos += ENTRY_SIZE_COMPRESSED(pos); /* to next entry */

    /* check if there is a next parent in this block */
    if ((pos < pfreepos) 
    || (pixblk->IXNUMENT == 1 && *plastProcessedParent != bap.blknum))
    {
        /* we found a parent */
        cxReleaseAbove(pcontext, pdownlst);/* safe, release all predecessors */
         *plastProcessedParent =
        pcursor->blknum = bap.blknum;
        pcursor->updctr = (ULONG)pixblk->BKUPDCTR;
        bmReleaseBuffer(pcontext, bap.bufHandle); /* release the block buffer */
        return DSM_S_SUCCESS;
    }
    /* no next parent in this block need to find next block in parent level
       by using predecessor levels */
    bmReleaseBuffer(pcontext, bap.bufHandle); /* release the block buffer */
    if ((&pdownlst->level[pdownlst->currlev])->dbk != pcursor->root)
    {
        /* must start again from some predecessor */

        /* initialize the list down level */
        pdownlst->currlev -= pdownlst->locklev;/*top most locked lev*/
        plevel = &pdownlst->level[pdownlst->currlev];
        cxReleaseBelow(pcontext, pdownlst); /* released levels below top most */
 
        /* start with new top block, a level below the
        top most locked */
        plevel++; /* next level down */
        pdownlst->currlev++; /* next level down */
        plevel->dbk = plevel->nxtprvbk; /* the next bk is the new top */
        if (plevel->dbk != (DBKEY)-1)
        {
            /* the search key is set to very low to start the search at
               the beginning of the block */
/*            pcursor->pkey[entryHeaderSize] = lowkey; */
/*            pcursor->pkey[ksOffset] = 1; */
            pcursor->pkey[FULLKEYHDRSZ] = lowkey;
            sct(&pcursor->pkey[KS_OFFSET] , 1);
            goto traverse;
        }
        cxReleaseAbove(pcontext, pdownlst);/*release all predecessors */
    }
    /* no more blocks in the level of the parent, go down one level */
    pcursor->level++;
    /* the search key is set to very low to start the search at
       the beginning of the new level */
/*    pcursor->pkey[entryHeaderSize] = lowkey; */
/*    pcursor->pkey[ksOffset] = 1; */
    pcursor->pkey[FULLKEYHDRSZ] = lowkey;
    sct(&pcursor->pkey[KS_OFFSET] , 1);

    goto fromtop;
}   /* cxCompactFindNextParent */

/*************************************************************************/
/* PROGRAM: cxKeyToCursor - Copies a key from a current position in the
*           index block to the cursor.  The input is the position in the  
*           block and a pointer to where the compressed part of the key   
*           can be found.  
* RETURNS:
*/
LOCALF int
cxKeyToCursor(
        dsmContext_t  *pcontext,           /* IN database context */
        cxcursor_t    *pcursor,
        TEXT          *pkeyPrefix,
        TEXT          *pos)
{
        int      hs, cs, ks;
        TEXT     *poldkey;
/*        int      entryHeaderSize = ENTHDRSZ, */
/*                 csOffset = 0, */
/*                 ksOffset = 1, */
/*                 isOffset = 2; */

    hs = HS_GET(pos);  
    cs = CS_GET(pos);  
    ks = KS_GET(pos);

    /* the key needs more space */
/*    if(pcursor->kyareasz < (TEXT)(cs + ks + 8))     */
    if(pcursor->kyareasz < (TEXT)(cs + ks + FULLKEYHDRSZ + 9))    
    {
        poldkey = pcursor->pkey;   /* allocate a new area 16 */
/*        pcursor->kyareasz = hs + cs + ks + 16;   bytes longer than needed */ 
        pcursor->kyareasz = cs + ks + FULLKEYHDRSZ + 16;  /* bytes longer than needed */
        pcursor->pkey = (TEXT *)stRentd(pcontext, pdsmStoragePool, 
                                        pcursor->kyareasz);
        /* if an old key exists, copy its compressed part and free its area */
        if(poldkey)
            stVacate(pcontext, pdsmStoragePool, poldkey);
    }

/*        pcursor->pkey[csOffset] = (TEXT)(cs + ks + entryHeaderSize); */
/*        pcursor->pkey[ksOffset] = (TEXT)(cs + ks); */
/*        pcursor->pkey[isOffset] = (TEXT)0; */
        sct(&pcursor->pkey[TS_OFFSET] , cs + ks + FULLKEYHDRSZ);
        sct(&pcursor->pkey[KS_OFFSET] , cs + ks);
        sct(&pcursor->pkey[IS_OFFSET] , 0);
/*        bufcop(pcursor->pkey + entryHeaderSize, pkeyPrefix, cs ); */
/*        bufcop(pcursor->pkey + entryHeaderSize + cs, pos + hs, ks);  */
        bufcop(pcursor->pkey + FULLKEYHDRSZ, pkeyPrefix, cs );
        bufcop(pcursor->pkey + FULLKEYHDRSZ + cs, pos + hs, ks); 

        return 0;

}   /* cxKeyToCursor */


/* PROGRAM: cxCompactBlocks - take 3 blks (parent, left child, right child) 
 *          and move keys from right block to left block until:
 *           - the desired block space utilization percent is reached or
 *           - a key from the right block will not fit into the left block or
 *           - there are no more keys in the right block to be moved
 *
 *           The cursor must contain the first key of the right block for 
 *           non-leaf levels.
 *
 * Returns: CX_S_SUCCESS        - keys were moved
 *          CX_S_COMPACT_FAILED - no keys were moved
 *
 */
LOCALF dsmStatus_t
cxCompactBlocks(dsmContext_t *pcontext,
                cxcursor_t   *pcursor,      /* cursor for index */
                cxbap_t      *pbapParent,   /* bap for parent blk */
                cxbap_t      *pbapLeft,     /* bap for left blk */
                cxbap_t      *pbapRight,    /* bap for right blk */
                LONG          desiredUtil,  /* desired % blk space used */
		dsmObject_t   tableNum,
                downlist_t   *pdownlst)     /* downlist, for blk remove */
{
    
    cxblock_t   *pblkRight  = pbapRight->pblkbuf,
                *pblkLeft   = pbapLeft->pblkbuf,
                *pblkParent = pbapParent->pblkbuf;

    cxCompactNote_t leftNote,  /* note for left blk modification */
                    rightNote; /* note for right blk modification */

    dsmStatus_t  returnCode;

    int          ret;

    TEXT     csBuffer[4096],      /* buffer for compressed part of key */
             rightDbkeyRaw[sizeof(DBKEY) + 2],    /* holder for raw dbky for remove/insert */
            *pSecondKey,          /* buffer for key in 2nd entry non-leaf blk */
            *pMoveBuffer,         /* buffer for added left blk keys */
            *pRightPosition,      /* current postion in right blk */
            *pSecondEntry,        /* pointer to second entry in right blk */
            *pRightEnd;        /* last position in right blk */ 

    LONG     blockSize,           /* block size for the db */
             blockCapacity,       /* amount blk will hold */
             leftSpaceAvail,      /* space available in left blk */
             numEntries = 0,      /* number of entries for the key */
             numMovedEntries = 0, /* # of entries being moved to left blk */
             sizeToMove = 0,      /* amount to move from right to left */
             firstSize = 0,       /* size of 1st entry in non-leaf blocks */
             entrySize = 0,       /* size of entry at position */
             deltaCS,             /* cs diff for 2nd entry in non-leaf blocks */
             insertKeySize,       /* size of key to be inserted into parent */
             moveBufferSize = 0,  /* size moving to left blk */
             entryNumber = 0;     /* counter for entries evaluated */

    cxeap_t  eapParent,                 /* entry access parms for parent blk */
            *peapParent = &eapParent;
    
    findp_t  fndpLeft,
             fndpRight,
             fndpParent,
            *pfndpParent = &fndpParent,
            *pfndpRight  = &fndpRight,
            *pfndpLeft   = &fndpLeft;
#ifdef VARIABLE_DBKEY
            int         dbkeySize;
#endif  /* VARIABLE_DBKEY */


/* structures to store entry information as we read through
   the right blk */
    typedef struct entryInfo
    {
        LONG    entryNumber;   /* physical entry number in blk */
        LONG    numEntries;    /* number of entries for key */
        LONG    entrySize;     /* size of entry */
    } entryInfo_t;

    typedef struct entryList
    {
        LONG         currEntry;        /* current physical entry number in blk */
        entryInfo_t  entryData[2000];  /* entry info struct, for 8k db it is */
    } entryList_t;                     /* only possible to have 1600 maximum */
                                       /* logical entries, 2000 should be */
                                       /* plenty */

    entryInfo_t *pEntryInfo;
    entryList_t entriesList;


    UCOUNT      maxKeySize      = pcontext->pdbcontext->pdbpub->maxkeysize;
/*int         entryHeaderSize = ENTHDRSZ, */
/*            csOffset        = 0, */
/*            ksOffset        = 1, */
/*            isOffset        = 2; */
    int         hs, cs, ks, is;  

    /* initialization */
    blockSize      = pcontext->pdbcontext->pdbpub->dbblksize;
    blockCapacity  = blockSize - (sizeof(struct bkhdr) + sizeof(struct ixhdr));
    leftSpaceAvail = blockCapacity - pblkLeft->IXLENENT;

    /* initialize the entry info structure which will hold entry number,
       number of entries, and size of entry */
    pEntryInfo = &entriesList.entryData[0];
    
    /* set up pfndpRight for cxSkipEntry() which will be used to get
       the number of entries for each physical entry in the right blk */
    pfndpRight->pblkbuf = pblkRight;
    pfndpRight->flags   = CXTAG;

    pRightEnd = pblkRight->ixchars + pblkRight->IXLENENT;
    /* loop through each entry in the right blk */
    for(pRightPosition = pblkRight->ixchars; pRightPosition < pRightEnd; 
        pRightPosition += entrySize)
    {
        /* count entries evaluated */
        entryNumber++;

        /* check and see if we have reached desired utilization */
        if ((int)(((DOUBLE)(pblkLeft->IXLENENT + moveBufferSize) / 
                      (DOUBLE)blockCapacity) * 100) > desiredUtil)
        {
            /* we've reached the desired utilitzation */
            break;
        }

        /* get the size of the entries at this position */
       entrySize = ENTRY_SIZE_COMPRESSED(pRightPosition);
 
        /* check for the first entry in a non-leaf blk */
        if ((entryNumber == 1) && (!pblkRight->IXBOT))
        {
            /* this is the first entry in non-leaf block. get size using
               the cursor's key size, which has been stored from parent blk */
/*            firstSize = sizeToMove = entryHeaderSize + pcursor->pkey[1] + */
/*                                     pRightPosition[isOffset]; */
            ks = xct(&pcursor->pkey[KS_OFFSET]);
            hs = (ks > MAXSMALLKEY) ? LRGENTHDRSZ : ENTHDRSZ;
            firstSize = sizeToMove = hs + ks + IS_GET(pRightPosition);
        }
        else
        {
            /* use the size of the entry in the blk */
            sizeToMove = entrySize;
        }

        if (sizeToMove > leftSpaceAvail)
        {
            /* it doesn't fit in the left block */
            break;
        }

        /* get the number of entries at this position */
        numEntries = cxSkipEntry(pcontext, pfndpRight, pRightPosition,
                                 pRightPosition + entrySize, &csBuffer[0]);

        /* load the physical entry info stuff */
        pEntryInfo->numEntries  = numEntries;
        pEntryInfo->entrySize   = entrySize;
        pEntryInfo->entryNumber = entryNumber;

        /* update running totals */
        leftSpaceAvail -= sizeToMove;
        moveBufferSize += sizeToMove;
        numMovedEntries += numEntries;
 
        /* make sure we don't overflow the number of
           pEntryInfo entries */
        if (entryNumber == 2000)
        {
            /* shouldn't have happened but did. leave now */
            returnCode = CX_S_COMPACT_FAILED;
            goto bottom;
        }

        pEntryInfo++;
    }

    if (numMovedEntries == 0)
    {
        /* nothing to move, leave and get two more child
           blks */
        returnCode = DSM_S_SUCCESS;
        goto bottom;
    }

    /* are we moving all the entries in the right blk ? */
    if (numMovedEntries != pblkRight->IXNUMENT)
    {
checkKeySize:
        /* make sure the new first entry's key will fit in the
           parent blk */
/*        insertKeySize = (pblkRight->IXBOT ? (pRightPosition[csOffset] + 1) : */
/*                                   pRightPosition[csOffset] + pRightPosition[ksOffset]); */
        insertKeySize = (pblkRight->IXBOT ? (CS_GET(pRightPosition) + 1) :
                                   (CS_GET(pRightPosition) + KS_GET(pRightPosition)) );
    
/*        if (((TEXT)insertKeySize > pcursor->pkey[1]) && */
/*            (((TEXT)insertKeySize - pcursor->pkey[1]) >  */
        if ((insertKeySize > xct(&pcursor->pkey[KS_OFFSET])) &&
            ((insertKeySize - xct(&pcursor->pkey[KS_OFFSET])) > 
            (blockCapacity - pblkParent->IXLENENT)))
        {
            /* key doesn't fit, move back an entry and then try again */
            pEntryInfo--;
            pRightPosition -= pEntryInfo->entrySize;
            if (pRightPosition == pblkRight->ixchars)
            {
                /* we're back to the beginning, can't move
                   anything */
                returnCode = CX_S_COMPACT_FAILED;
                goto bottom;
            }
            moveBufferSize -= pEntryInfo->entrySize;
            numMovedEntries -= pEntryInfo->numEntries;
            entryNumber--;
            goto checkKeySize;
        }
    }

    /* now put together the moveBuffer */
    /* allocate space for the buffers */
    pMoveBuffer = (TEXT *)stRentd(pcontext, pdsmStoragePool, (unsigned) blockCapacity);
    pSecondKey = (TEXT *)stRentd(pcontext, pdsmStoragePool, (unsigned) maxKeySize);

    /* set up fndp for parent blk. pfndpParent will be used to compression the
       2nd moved entry and for the removal from the parent blk of the original 
       first entry in the right blk or to remove the right blk if it has become
       empty */
    pfndpParent->pblkbuf   = pblkParent;
    pfndpParent->bufHandle = pbapParent->bufHandle;
    pfndpParent->flags     = CXNOLOG;

    if (pblkRight->IXBOT)
    {
        /* this is a leaf blk, move the whole chunk in one shot */
        bufcop(pMoveBuffer, pblkRight->ixchars, moveBufferSize);
    }
    else
    {
        /* this is a non-leaf block.  the key is not compressed, take 
           the key size from the cursor and the is from the entry */
/*        pMoveBuffer[csOffset] = 0; */
/*        pMoveBuffer[ksOffset] = pcursor->pkey[1]; */
/*        pMoveBuffer[isOffset] = pblkRight->ixchars[isOffset]; */
        ks = xct(&pcursor->pkey[KS_OFFSET]);
        is = IS_GET(pblkRight->ixchars);
        CS_KS_IS_SET(pMoveBuffer,(ks > MAXSMALLKEY),0,ks,is);
        hs = HS_GET(pMoveBuffer);

        /* take the entry from the cursor */
/*        bufcop(pMoveBuffer + entryHeaderSize,  */
/*               pcursor->pkey + entryHeaderSize,  */
/*               pcursor->pkey[1]); */
        bufcop(pMoveBuffer + hs, pcursor->pkey + FULLKEYHDRSZ, ks);

        /* move the info in from entry in blk */
/*        bufcop(pMoveBuffer + entryHeaderSize + pcursor->pkey[1], */
/*               pblkRight->ixchars + entryHeaderSize + pblkRight->ixchars[ksOffset], */
/*               pblkRight->ixchars[isOffset]); */
        bufcop(pMoveBuffer + hs + ks,
               (pblkRight->ixchars + HS_GET(&pblkRight->ixchars[0]) + 
		KS_GET(pblkRight->ixchars)),
               IS_GET(pblkRight->ixchars));

        /* is there more than one physical entry being moved?
           or logical in the case of the non-leaf blk */
        if (numMovedEntries > 1)
        {
            /* update pMoveBuffer to reflect the first 
               physical entry has been moved to the pMoveBuffer */
            pMoveBuffer += firstSize;

            /* reset the entry info list to the beginning and use the
               entry size of the first entry to position to the second
               entry */
            pEntryInfo = &entriesList.entryData[0];
            pSecondEntry = pblkRight->ixchars + pEntryInfo->entrySize;

            /* the second entry needs to be compressed based on the
               first entry. get an uncompressed copy of the key,
               and build a fndp structure cxFindEntry() */
/*            bufcop(pSecondKey,  */
/*                   pblkRight->ixchars + entryHeaderSize, */
/*                   pSecondEntry[csOffset]); */
/*            bufcop(pSecondKey + pSecondEntry[csOffset], */
/*                   pSecondEntry + entryHeaderSize, */
/*                   pSecondEntry[ksOffset]); */
            hs = HS_GET(pSecondEntry); /* HS of the second entry */
            cs = CS_GET(pSecondEntry); /* CS of the second entry */
            ks = KS_GET(pSecondEntry); /* KS of the second entry */
            bufcop(pSecondKey, 
                   pblkRight->ixchars + HS_GET(pblkRight->ixchars),
                   cs);
            bufcop(pSecondKey + cs, pSecondEntry + hs, ks);

            pfndpParent->pinpkey = pSecondKey;
/*            pfndpParent->inkeysz = pSecondEntry[csOffset] + pSecondEntry[ksOffset]; */
            pfndpParent->inkeysz = cs + ks;
            if ((ret = cxFindEntry(pcontext, pfndpParent)) != 1) 
            {
                /* an entry shouldn't have been found */
                /* this is bad, leave before it gets worse */
                returnCode = CX_S_COMPACT_FAILED;
                goto cleanUp;
            }
            else
            {
                /* move entry in reflecting the new compression */
/*                pMoveBuffer[csOffset] = pfndpParent->csofprev; */
/*                pMoveBuffer[ksOffset] = pfndpParent->inkeysz - pfndpParent->csofprev; */
/*                pMoveBuffer[isOffset] = pSecondEntry[isOffset]; */
                cs = pfndpParent->csofprev;
                ks = pfndpParent->inkeysz - pfndpParent->csofprev;
                is = IS_GET(pSecondEntry);
                CS_KS_IS_SET(pMoveBuffer,(pfndpParent->inkeysz > MAXSMALLKEY),cs,ks,is);
                hs = HS_GET(pMoveBuffer);
                
                /* update moveBufferSize amount to reflect the the new compresseion in 
                   the pMoveBuffer */
/*                deltaCS = pfndpParent->csofprev - pSecondEntry[csOffset];; */
                deltaCS = pfndpParent->csofprev - cs;
                moveBufferSize -= deltaCS;

                /* move the compressed key */
/*                bufcop(pMoveBuffer + entryHeaderSize, */
/*                       pSecondKey + pfndpParent->csofprev, */
/*                       pMoveBuffer[ksOffset]); */
                bufcop(pMoveBuffer + hs, pSecondKey + pfndpParent->csofprev, ks);

                /* now move the info, and the remaining entries. */
/*                bufcop(pMoveBuffer + entryHeaderSize + pMoveBuffer[ksOffset], */
/*                       pSecondEntry + entryHeaderSize + pSecondEntry[ksOffset], */
/*                       pRightPosition - pSecondEntry -  */
/*                                              entryHeaderSize - pSecondEntry[ksOffset]); */
                bufcop(pMoveBuffer + hs + ks,
                       pSecondEntry + HS_GET(pSecondEntry) + KS_GET(pSecondEntry),
                       pRightPosition - pSecondEntry - 
                                              HS_GET(pSecondEntry) - KS_GET(pSecondEntry));
                /* reset the start of pMoveBuffer back to the beginning */
                pMoveBuffer -= firstSize;
                pEntryInfo = &entriesList.entryData[entryNumber - 1];
            }
        }
    }


    /* check that there is something to move */
    if (moveBufferSize != 0)
    {
        /* build a fndp structure to get the compression of
           the first entry to be added to the left blk */
        pfndpLeft->pblkbuf = pblkLeft;
        pfndpLeft->flags = CXNOLOG;
/*        pfndpLeft->pinpkey = &pMoveBuffer[3]; */
/*        pfndpLeft->inkeysz = pMoveBuffer[1]; */
        pfndpLeft->pinpkey = &pMoveBuffer[HS_GET(pMoveBuffer)];
        pfndpLeft->inkeysz = KS_GET(pMoveBuffer);
        if ((ret = cxFindEntry(pcontext, pfndpLeft)) != 1) 
        {
           /* an entry shouldn't have been found */
           /* this is bad, leave before it gets worse */
            returnCode = CX_S_COMPACT_FAILED;
            goto cleanUp;
        }

#ifdef CXDEBUG_COMPACT
        if (pfndpLeft->position != (pblkLeft->ixchars + pblkLeft->IXLENENT))
        {
           /* the position to move entries to was not the end of the */
           /* block. this is bad, leave before it gets worse */
            returnCode = CX_S_COMPACT_FAILED;
            goto cleanUp;

        }
#endif /* ifdef CXDEBUG_COMPACT */

        /* Initialize and build note for left block, then do it */
        leftNote.rlnote.rllen = sizeof(leftNote);
        leftNote.rlnote.rlcode = RL_CXCOMPACT_LEFT;
        leftNote.offset = pblkLeft->IXLENENT;
        leftNote.cs = (int)pfndpLeft->csofprev;
        leftNote.numMovedEntries = numMovedEntries;
        leftNote.moveBufferSize = moveBufferSize;
        leftNote.lengthOfEntries = pblkLeft->IXLENENT;
        sct((TEXT *)&leftNote.rlnote.rlflgtrn, (COUNT)(RL_PHY));
        rlLogAndDo(pcontext, (RLNOTE *)&leftNote, pbapLeft->bufHandle, 
            (COUNT)moveBufferSize, pMoveBuffer);

        /* if all entries are not being moved, a key will need to
           be inserted into the parent */
        if (numMovedEntries != pblkRight->IXNUMENT)
        {
            /* put together the key that will be inserted into the
               parent for the new first physical entry in the blk.
               Do this now so that the right amount of space is
               allocated for pfndpParent->pkykey which is also use
               as part of removing an entry or blk */
               
            /* get the compressed part of the key from csBuffer.
               Use pSecondKey as buffer to hold key to be inserted */
            cs = CS_GET(pRightPosition);
            ks = KS_GET(pRightPosition);
            hs = HS_GET(pRightPosition);
            if (!pblkRight->IXBOT)
            {
                /* this is not a leaf, the whole key will need to be
                   inserted into the parent */
/*                insertKeySize = pRightPosition[csOffset] + */
/*                                pRightPosition[ksOffset]; */
                insertKeySize = cs+ ks;

/*                bufcop(pSecondKey, &csBuffer[0], pRightPosition[csOffset]); */
                bufcop(pSecondKey, &csBuffer[0], cs);

/*                bufcop(pSecondKey + pRightPosition[csOffset],  */
/*                       pRightPosition + entryHeaderSize,  */
/*                       pRightPosition[ksOffset]); */
                bufcop(pSecondKey + cs, pRightPosition + hs, ks);
            }
            else
            {
                /* this is a leaf, only the cs + 1 of the key will
                   need to be inserted into the parent */
/*                insertKeySize = pRightPosition[csOffset] + 1; */
                insertKeySize = cs + 1;
                bufcop(pSecondKey,
                       &csBuffer[0], 
/*                       pRightPosition[csOffset]); */
                       cs);
/*                bufcop(pSecondKey + pRightPosition[csOffset],  */
/*                       pRightPosition + entryHeaderSize, */
/*                       1); */
                bufcop(pSecondKey + cs, pRightPosition + hs, 1);
            }
        }
        else
        {
            /* use maximum key size for sizing pkykey */
            insertKeySize = maxKeySize;
        }

        /* add to the parent blk fndp structure to remove the right blk 
           or to remove the original first entry in the right blk from the 
           parent and the insertion of the new first entry in the right blk */

        /* use the entry in the cursor to get the position in parent blk
           for the key to be removed */
/*        pfndpParent->pinpkey = pcursor->pkey + entryHeaderSize; */
/*        pfndpParent->inkeysz = pcursor->pkey[ksOffset]; */
        pfndpParent->pinpkey = pcursor->pkey + FULLKEYHDRSZ;
        pfndpParent->inkeysz = xct(&pcursor->pkey[KS_OFFSET]);
        if ((ret = cxFindEntry(pcontext, pfndpParent)) != 0)
        {
            /* an entry should have been found */
            /* this is bad, leave before it gets worse */
            returnCode = CX_S_COMPACT_FAILED;
            goto cleanUp;
        }
        
        /* attention: we are storing the dbkey here as it is in the
           block. it is not usable as an integer! */
        is = IS_GET(pfndpParent->position);
        ks = KS_GET(pfndpParent->position);
        hs = HS_GET(pfndpParent->position);
#ifdef VARIABLE_DBKEY
/*        dbkeySize = pfndpParent->position[isOffset]; */
        dbkeySize = is;
        bufcop(&rightDbkeyRaw[0], 
/*               pfndpParent->position + entryHeaderSize + pfndpParent->position[ksOffset],  */
               pfndpParent->position + hs + ks, 
               dbkeySize);  
#else
        bufcop(&rightDbkeyRaw[0], 
/*               pfndpParent->position + entryHeaderSize + pfndpParent->position[ksOffset],  */
               pfndpParent->position + hs + ks, 
               4);  
#endif  /* VARIABLE_DBKEY */
        pfndpParent->pinpdata = &rightDbkeyRaw[0];

        /* allocate space for the key structure, including enough
        for what pfndpParent->pkykey->keystr will hold */
        pfndpParent->pkykey = 
            (dsmKey_t *)stRentd(pcontext, pdsmStoragePool, 
            sizeof(dsmKey_t) +
            ((FULLKEYHDRSZ + insertKeySize + sizeof(DBKEY) + 1) * 
	     sizeof(dsmText_t)));

        pfndpParent->pkykey->root = pcursor->root;
        pfndpParent->pkykey->unique_index = pcursor->unique_index;
        pfndpParent->pkykey->word_index = pcursor->word_index;

        
        if (numMovedEntries == pblkRight->IXNUMENT)
        {
            /* right block is empty, remove it amd make
               blkbuf null to tell caller its gone */
            cxRemoveBlock(pcontext, peapParent, pfndpParent, tableNum, pdownlst);
            pbapRight->pblkbuf = NULL;
        }
        else
        {
            /* remove first key reference from parent. note: cxRemoveEntry
               only returns zero */
            ret = cxRemoveEntry(pcontext, tableNum, pfndpParent);

            /* set up promotion of key into parent block */
            pfndpParent->pinpkey = pSecondKey;
            pfndpParent->inkeysz = insertKeySize;
            pfndpParent->pinpdata = &rightDbkeyRaw[0];

            /* get the csofprev, csofnext, and the position in parent blk
               as they are after the removal of the original first entry */
            if ((ret = cxFindEntry(pcontext, pfndpParent)) != 1)
            {
                /* an entry should not have been found */
                /* this is bad, leave before it gets worse */
                returnCode = CX_S_COMPACT_FAILED;
                /* free the pkykey */
                stVacate(pcontext, pdsmStoragePool,
                         (TEXT *)pfndpParent->pkykey);
                goto cleanUp;
            }

/*            pfndpParent->pkykey->keystr[1] = insertKeySize; */
            sct(&pfndpParent->pkykey->keystr[KS_OFFSET] , insertKeySize);
#ifdef VARIABLE_DBKEY
/*            pfndpParent->pkykey->keystr[2] = dbkeySize;  size of the variable size dbkey */ 
            sct(&pfndpParent->pkykey->keystr[IS_OFFSET] , dbkeySize); /* size of the variable size dbkey */
/*            pfndpParent->pkykey->keystr[0] = entryHeaderSize + insertKeySize + dbkeySize; */
	    
	    /* ????            sct(&pfndpParent->pkykey->keystr[TS_OFFSET] , entryHeaderSize + insertKeySize + dbkeySize); */
/*            bufcop(pfndpParent->pkykey->keystr + ENTHDRSZ, pSecondKey, insertKeySize); */
            bufcop(pfndpParent->pkykey->keystr + FULLKEYHDRSZ, pSecondKey, insertKeySize);
            /* take the dbkey from the position in the right blk */
/*            bufcop(pfndpParent->pkykey->keystr + ENTHDRSZ + insertKeySize,  */
            bufcop(pfndpParent->pkykey->keystr + FULLKEYHDRSZ + insertKeySize, 
                   &rightDbkeyRaw[0], dbkeySize);
#else
/*            pfndpParent->pkykey->keystr[2] = 4;  always 4 for a parent blk */ 
/*            pfndpParent->pkykey->keystr[0] = entryHeaderSize + insertKeySize + 4; */
            sct(&pfndpParent->pkykey->keystr[IS_OFFSET] , 4); /* size of the variable size dbkey */
            sct(&pfndpParent->pkykey->keystr[TS_OFFSET] , entryHeaderSize + insertKeySize + 4);
/*            bufcop(pfndpParent->pkykey->keystr + 3, pSecondKey, insertKeySize); */
            bufcop(pfndpParent->pkykey->keystr + FULLKEYHDRSZ, pSecondKey, insertKeySize);
            /* take the dbkey from the position in the right blk */
/*            bufcop(pfndpParent->pkykey->keystr + 3 + insertKeySize,  */
            bufcop(pfndpParent->pkykey->keystr + FULLKEYHDRSZ + insertKeySize, 
                   &rightDbkeyRaw[0], 4);
#endif  /* VARIABLE_DBKEY */

            cxInsertEntry(pcontext, pfndpParent, tableNum, 0);

            /* get the entry size  and the uncompressed key size for the position
               at the offset. will be used for the undo data in the note */
            entrySize = ENTRY_SIZE_COMPRESSED(pRightPosition);
/*            insertKeySize = pRightPosition[csOffset] + pRightPosition[ksOffset]; */
            insertKeySize = CS_GET(pRightPosition) + KS_GET(pRightPosition);
            
            /* initialize and build note for right block */
            cs = CS_GET(pRightPosition);
            ks = KS_GET(pRightPosition);
            hs = HS_GET(pRightPosition);
            rightNote.rlnote.rllen = sizeof(rightNote);
            rightNote.rlnote.rlcode = RL_CXCOMPACT_RIGHT;
            rightNote.offset = (COUNT)(pRightPosition - pblkRight->ixchars);
            rightNote.cs = cs;
/*            rightNote.csofnext = pRightPosition[entrySize]; */
            rightNote.csofnext = CS_GET(pRightPosition+entrySize);
            rightNote.numMovedEntries = numMovedEntries;
            rightNote.moveBufferSize = moveBufferSize;
            rightNote.lengthOfEntries = pblkRight->IXLENENT;
            
            /* insert from the csbuffer and the compressed key to moveBuffer */
            bufcop(pMoveBuffer + moveBufferSize,
                   &csBuffer[0], 
/*                   pRightPosition[csOffset]); */
                     cs);
/*            bufcop(pMoveBuffer + moveBufferSize + pRightPosition[csOffset], */
/*                   pRightPosition + entryHeaderSize,  */
/*                   pRightPosition[ksOffset]);  */
            bufcop(pMoveBuffer + moveBufferSize + cs, pRightPosition + hs, ks);
            sct((TEXT *)&rightNote.rlnote.rlflgtrn, (COUNT)(RL_PHY));
            rlLogAndDo(pcontext, (RLNOTE *)&rightNote, pbapRight->bufHandle,
                (COUNT)(moveBufferSize + insertKeySize), pMoveBuffer);
        }
        /* free the pkykey */
        stVacate(pcontext, pdsmStoragePool, (TEXT *)pfndpParent->pkykey);
    }

    returnCode = DSM_S_SUCCESS;

cleanUp:
    /* free the buffers, and pkykey */
    stVacate(pcontext, pdsmStoragePool, (TEXT *)pMoveBuffer);
    stVacate(pcontext, pdsmStoragePool, (TEXT *)pSecondKey);

bottom:
    return (returnCode);
}

/****
LOCALF dsmStatus_t
cxGetEntrySize(dsmContext_t *pcontext, TEXT *pPosition, LONG *pEntrySize)
{
    *pEntrySize += pPosition[1];
    *pEntrySize += pPosition[2];
    *pEntrySize += ENTHDRSZ;

    return DSM_S_SUCCESS;
}
****/

/* PROGRAM: cxCompactReportBlock - take an index blk and reports statistics
 *                                 about the block
 *
 *
 *
 * Returns: DSM_S_SUCCESS - successfully reported block statistics
 *
 */
LOCALF dsmStatus_t
cxCompactBlockReport(dsmContext_t   *pcontext,
                     cxcursor_t     *pcursor,      /* cursor for index */
                     cxbap_t        *pbap,
                     cxBlockStats_t *pblkStats)
{
    cxblock_t    *pBlock;            /* pointer to blk for collecting stats */
    int           blockSize,         /* blk size for db */
                  blockCapacity;     /* blk capacity for db */
    LONG          onDelChain,        /* is it on the idx del chain */ 
                  blockUtilization;  /* utilization level of blk */
    int           treeLevel;
    TEXT         *pnodeType;

   /* find the capacity of a block */
   blockSize = pcontext->pdbcontext->pdbpub->dbblksize;
   blockCapacity = blockSize - (sizeof(struct bkhdr) + sizeof(struct ixhdr));

   pBlock = pbap->pblkbuf;

   /* what is the utilization level for the block */
   blockUtilization =
      (LONG)((DOUBLE)pBlock->IXLENENT / (DOUBLE)blockCapacity * 100);

   /* keep running total for index */
   pblkStats->numEntries    += pBlock->IXNUMENT;
   pblkStats->lenOfEntries  += pBlock->IXLENENT;

   /* count and flag blks on index delete chain */
   pblkStats->delChainSize +=
               onDelChain = (pBlock->BKFRCHN == LOCKCHN ? 1 : 0);

   /* cursor treats the first level as 0, report it as 1 */
   treeLevel = pcursor->level + 1;
   if (pBlock->IXTOP)
   {
      /* root node */
      pnodeType = (TEXT *)"root";
   }
   else if (!pBlock->IXTOP && !pBlock->IXBOT)
   {
      /* non-leaf node */
      treeLevel += 1;
      pnodeType = (TEXT *)"nonLeaf";
   }
   else
   {
      /* leaf node */
      treeLevel += 1;
      pnodeType = (TEXT *)"leaf";
   }

  if (pblkStats->blkNum == 1)
  {
      printf("\nBlockSize = %d  Block Capacity = %d\n", blockSize, blockCapacity);
      printf("\t\tNumber\tLength\tOn\tLength\tDelete\n");
      printf("\t\tof\tof\tDelete\tof\tChain\t\tPercent\n");
      printf("DBKEY\tLevel\tEntries\tEntries\tChain\tSize\tType\t\tUtilized\n");

  }

  printf("%ld\t%d\t%d\t%d\t%ld\t%lu\t%s\t\t%ld\n",
                pBlock->bk_hdr.bk_dbkey,
                treeLevel,
                pBlock->IXNUMENT,
                pBlock->IXLENENT,
                onDelChain,
                pblkStats->delChainSize,
                pnodeType,
                blockUtilization);

  return DSM_S_SUCCESS;
}
