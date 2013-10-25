
 /* Copyright (C) 2000 by NuSphere Corporation
 *
 * This is an unpublished work.  No license is granted to use this software. 
 *
 */

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include "dbconfig.h"
#include "cxmsg.h"
#include "bkpub.h"
#include "dbpub.h"
#include "bmpub.h"
#include "rlpub.h"
 
#include "usrctl.h"
#include "lkpub.h"
#include "dbmgr.h"
#include "latpub.h"
#include "dbcontext.h"
#include "stmpub.h" /* storage manager subsystem */
#include "stmprv.h"
#include "dsmpub.h"
#include "cxpub.h"
#include "cxprv.h"
#include "utspub.h"   /* for sct, slng, etc */
#include "statpub.h"
#include "ompub.h"
#include "recpub.h"
#include "rmpub.h"
#include "scasa.h"
#include "umlkwait.h"
#include "utcpub.h"


/* PROGRAM: cxUndoBlockCopy - the redo, undo routine for copying a block
 *
 * RETURNS: DSMVOID
 */

DSMVOID
cxUndoBlockCopy( dsmContext_t *pcontext,
             RLNOTE       *pnote,
             COUNT        dlen,
             TEXT         *pdata)
{
    bmBufHandle_t    bufHandle;
    cxblock_t        *pixblk;

    rlTXElock (pcontext,RL_TXE_UPDATE,RL_MISC_OP);
    switch( pnote->rlcode )
    {
	case RL_IDXBLOCK_COPY:
              bufHandle = bmLocateBlock(pcontext,
                    xlng((unsigned char *)&(pnote->rlArea)), 
                    (DBKEY)xlng((unsigned char *)&(pnote->rldbk)), TOMODIFY);
              pixblk = (cxblock_t *)bmGetBlockPointer(pcontext,bufHandle);
              
              if ( pixblk->ix_hdr.ih_ixnum ==  ((IXCPYNOTE *)pnote)->ixNum &&
                   pixblk->bk_hdr.bk_type == IDXBLK )
              {

                   FASSERT_CB(pcontext, 
                              pixblk->bk_hdr.bk_frchn != LOCKCHN, 
                              "cxUndoBlockCopy:index block on lock chain" )

                  pnote->rlcode = RL_IDXBLOCK_UNCOPY;
                  sct((TEXT *)&pnote->rlflgtrn, (UCOUNT)RL_PHY );
                  rlLogAndDo( pcontext, pnote, bufHandle, dlen, pdata );
                  bkfradd(pcontext, bufHandle, FREECHN);
              }
              bmReleaseBuffer(pcontext, bufHandle);
              break;
        case RL_IDXBLOCK_UNCOPY:
              FASSERT_CB(pcontext, 0, "Physical note encountered in lgical backout" )
              break;
     }
     rlTXEunlock(pcontext);
}




/****************************************************************************/
/* PROGRAM: cxCopyIndex -  copy index block with given dbkey and return new
 * dbkey of corresponding block in the moved index.
 *
 *
 *      Input Parameters
 *      ----------------
 *          pcontext       - dsm context
 *          olddbk         - the index block and it's children to move.
 *          sourceArea     - the area from which to copy the index.
 *          targetArea     - the area into which to move the index.
 *      Output Parameters
 *      -----------------
 * RETURNS: 
 *      DBKEY of new index block
 *      -1 if unsuccessfull
 *
 * ALSO SEE Output Parameters (above).
 */


DBKEY 
cxCopyIndex(dsmContext_t *pcontext,
            DBKEY        olddbk,      /* dbkey of an index block in old index*/
            ULONG        sourceArea,  /* area of the old index */
            ULONG        targetArea)  /* area of the new index */
{

    /* steps:
     1. make a copy of the current block in the new area.
     2. traverse all the children of this block and call cxCopyIndex
        on each.
     3. modify the child dbkeys in the parent alongside in step 2.
       note:
     to reduce the duration of microtransaction, permitting others to
     commit during the copy opeartion locks on the buffers are released
     and microtxn ended as quickly as possible. This doesn't interfere
     with the copy since the table is under shared lock and no one is
     permitted to change the index being copied, with one exception:
     delete-lock chain processing algorithms may end up deleting a leaf
     blocks contents or removing it from the chain which at worst could mean
     that in the new area a block is on the delete lock chain which 
     shouldn't have been there and thats ok
    */

    cxblock_t        *pOldIndexBlock;    /* pointer to old index block */
    cxblock_t        *pNewIndexBlock;    /* pointer to new index block */
    bmBufHandle_t    oldIndexBufHandle;
    bmBufHandle_t    newIndexBufHandle;
    DBKEY            entdbk, childdbk, newdbk;
    int              entlen, entloc;
    TEXT*            pixchars;
    IXCPYNOTE        note;
    TEXT             textdbk[4];

    if ( sourceArea == targetArea )
	return -1;


    if ( utpctlc() )        /* if user hit ctrl-C */
	return -1;          /* return err the txn must backout*/

    rlTXElock (pcontext,RL_TXE_UPDATE,RL_MISC_OP);

    /* update status information in user control */
    pcontext->pusrctl->uc_counter += 1;
    
    oldIndexBufHandle = bmLocateBlock(pcontext,
                             sourceArea, olddbk, TOREAD);
    pOldIndexBlock = (cxblock_t *)bmGetBlockPointer(pcontext,oldIndexBufHandle);


    newIndexBufHandle = bkfrrem(pcontext, targetArea, IDXBLK, FREECHN);
    pNewIndexBlock = (cxblock_t *)bmGetBlockPointer(pcontext,newIndexBufHandle);
    newdbk   = pNewIndexBlock->bk_hdr.bk_dbkey;

    /* fill the note members */
    ((RLNOTE *)&note)->rllen = sizeof( IXCPYNOTE );
    ((RLNOTE *)&note)->rlcode = RL_IDXBLOCK_COPY;
    sct((TEXT *)&(((RLNOTE *)&note)->rlflgtrn),  RL_LOGBI|RL_PHY);
    note.ixNum = pOldIndexBlock->ix_hdr.ih_ixnum;

    rlLogAndDo( pcontext, (RLNOTE *)&note, 
                newIndexBufHandle, 
                (COUNT)(MAXIXCHRS + sizeof( struct ixhdr )),
                (TEXT *)&(pOldIndexBlock->ix_hdr) );
   
    FASSERT_CB(pcontext, 
               pOldIndexBlock->bk_hdr.bk_type == IDXBLK, 
               "cxCopyIndex:This block is not an index block" )

    /* if the block in the old area is on a delete lock chain then
       add the new block just obtained to the delete lock chain in 
       the new area */

    FASSERT_CB(pcontext, 
               pOldIndexBlock->bk_hdr.bk_frchn != LOCKCHN, 
               "cxCopyIndex:index block on lock chain" )

    bmReleaseBuffer(pcontext, oldIndexBufHandle); 

    /*  parent copied, now proceed to make copies of children */

    /* scan all its kids */
    if (!(pNewIndexBlock->IXBOT))
    for(entloc=0; entloc < pNewIndexBlock->IXLENENT; entloc+=entlen)
    {
            /* get the next kid's dbkey and read the block */
            pixchars = pNewIndexBlock->ixchars + entloc;
#ifdef LARGE_INDEX_ENTRIES_DISABLED
            entlen       = ENTHDRSZ + pixchars[1] + pixchars[2];
#else
            entlen       = ENTRY_SIZE_COMPRESSED(pixchars);
#endif  /* LARGE_INDEX_ENTRIES_DISABLED */
            entdbk       = (DBKEY)xlng(pixchars+entlen-4);
 
            bmReleaseBuffer(pcontext, newIndexBufHandle); 
            rlTXEunlock (pcontext);

            childdbk =   cxCopyIndex(pcontext, entdbk, 
                                    sourceArea, targetArea);

            if( childdbk == -1 ) return -1; /* user hit ctrl-c */

            /* now modify the parent's entry for the child's dbkey */
            rlTXElock (pcontext,RL_TXE_UPDATE,RL_MISC_OP);
            newIndexBufHandle = bmLocateBlock(pcontext,
                              targetArea, newdbk, TOMODIFY);
                               
            pNewIndexBlock = (cxblock_t *)bmGetBlockPointer(pcontext,
                                                            newIndexBufHandle);

            /* reset the values of pixchars, entlen 
               just defensive coding entlen has to be the same though*/

            pixchars = pNewIndexBlock->ixchars + entloc;
#ifdef LARGE_INDEX_ENTRIES_DISABLED
            entlen       = ENTHDRSZ + pixchars[1] + pixchars[2];
#else
            entlen       = ENTRY_SIZE_COMPRESSED(pixchars);
#endif  /* LARGE_INDEX_ENTRIES_DISABLED */

            slng(textdbk, childdbk);
            bkReplace(pcontext,
                      newIndexBufHandle,
                      (TEXT *)(pixchars+entlen-4),
                      (TEXT *)textdbk,
                      sizeof(DBKEY) );
    }


    /*This is redundand since it doesnt change the block at all
     purpose: this rlLogAndDo enforces that during undo the note
     RL_IDXBLOCK_COPY is encountered first for the parent and then for
     its children so that during undo the index is always removed top
     down, this takes care of readers which might be accessing that
     index. In the absence of this rlLogAndDo other readers that may
     be accessing the index may start pointing to freed up blocks since
     in the absence of this note the freeing up of the index blocks is
     bottom up (for a leaf node this is unnecessary) */
 
    if (!(pNewIndexBlock->IXBOT))
          rlLogAndDo( pcontext, (RLNOTE *)&note,
                      newIndexBufHandle,
                      (COUNT)(MAXIXCHRS + sizeof( struct ixhdr )),
                      (TEXT *)&(pNewIndexBlock->ix_hdr) );
 


    bmReleaseBuffer(pcontext, newIndexBufHandle); 
    rlTXEunlock (pcontext);

    return newdbk; 
}


/****************************************************************************/
/* PROGRAM: -   cxLocateValidBlock 
 *          locate the correct root and as a side effect modify
 *          the peap, peap->pcursor, pdownlist to point to correct
 *          root and area.
 *
 *      Input Parameters
 *      ----------------
 *          pcontext       - dsmContext_t.
 *          peap           - the index Number.
 *          dbkey          - dbkey of the block to get.
 *          pdownlist      - to check whether or not root is needed.
 *          action         - what type of lock to acquire.
 *      Output Parameters
 *      -----------------
 * RETURNS:
*/

bmBufHandle_t
cxLocateValidBlock(
                   dsmContext_t    *pcontext,
                   cxeap_t         *peap,
                   DBKEY           *pdbkey,
                   downlist_t      *pdownlst,
                   dsmObject_t      tableNum,
                   int              action)
{
    bmBufHandle_t    bufHandle;
    cxblock_t        *pixblock;    /* pointer to new index block */


    /* algorithm, if it's not the root that is of interest
       to the caller just find the block and return it
       else get valid root and return that */


    if( pdownlst->currlev != 0)
    {
         return bmLocateBlock(pcontext, peap->pkykey->area, *pdbkey, action);
    }
    else
    {
         peap->pkykey->root = *pdbkey;
         peap->pkykey->area = pdownlst->area;
         while(1)
         {
              bufHandle = bmLocateBlock(pcontext, peap->pkykey->area, 
                              peap->pkykey->root, action);
              pixblock = (cxblock_t *)bmGetBlockPointer(pcontext,bufHandle);

              if ( pixblock->bk_hdr.bk_type != IDXBLK      ||
                   pixblock->ix_hdr.ih_ixnum != peap->pkykey->index ||
                   pixblock->ix_hdr.ih_top   != 1 )
              {
                 bmReleaseBuffer(pcontext, bufHandle);
                 cxGetRootAndArea( pcontext, tableNum, peap);
                 pdownlst->area = peap->pkykey->area;
                 (pdownlst->level[pdownlst->currlev]).dbk =
                                  peap->pkykey->root;
                 *pdbkey = peap->pkykey->root;
                 if ( peap->pcursor )
                 {
                    peap->pcursor->root = peap->pkykey->root;
                    peap->pcursor->area = peap->pkykey->area;
                 }
              }
              else
              {
                 return bufHandle;
              }
         }
    }
}


/****************************************************************************/
/* PROGRAM: cxGetRootAndArea -   given the indexnumber and the old root block
 *          pointer determines if the root is valid, else gets the valid root
 *
 *      Input Parameters
 *      ----------------
 *          pcontext       - dsmContext_t.
 *          peap           - to get new root and area of the moved index
 *      Output Parameters
 *      -----------------
 * RETURNS:
 *      DBKEY of valid index root block
 *      -1 if unsuccessfull
*/

DBKEY
cxGetRootAndArea(dsmContext_t *pcontext, dsmObject_t tableNum, cxeap_t *peap) 
{

    dsmObject_t      indexNum = peap->pkykey->index;
    ULONG            area = peap->pkykey->area;
    DBKEY            root = peap->pkykey->root; 
    cxblock_t        *pixblock;    /* pointer to new index block */
    bmBufHandle_t    bufHandle;
    omDescriptor_t   indexDesc;
    dsmStatus_t      rc;

	
    bufHandle = bmLocateBlock(pcontext, area, root, TOREAD);
    pixblock = (cxblock_t *)bmGetBlockPointer(pcontext,bufHandle);


    if ( pixblock->bk_hdr.bk_type != IDXBLK      ||
         pixblock->ix_hdr.ih_ixnum != indexNum   ||
         pixblock->ix_hdr.ih_top   != 1 )
    {

         latnap(pcontext, 100); 
         rc = omFetch(pcontext,DSMOBJECT_MIXINDEX,indexNum,tableNum,&indexDesc);
         if (rc )
         {
             bmReleaseBuffer(pcontext, bufHandle);
             return -1;
         }
         peap->pkykey->root = indexDesc.objectRoot;
         peap->pkykey->area = indexDesc.area;
    }

    bmReleaseBuffer(pcontext, bufHandle);
    return peap->pkykey->root;
}



/* PROGRAM: cxIndexAnchorUpdate - Modify the index anchor object
 *
 *
 * RETURNS: DSM_S_SUCCESS unqualified success
 *          DSM_S_NO_TRANSACTION Caller must have started a tx.
 *          DSM_S_FAILURE on failure
 */
dsmStatus_t
cxIndexAnchorUpdate(
            dsmContext_t       *pcontext,   /* IN database context */
            dsmArea_t          area,        /* IN area */
            dsmObject_t        object,      /* IN  for object id */
            dsmObjectType_t    objectType,  /* IN object type */
            dsmObject_t        associate,   /* IN object number to use
                                               for sequential scan of tables*/
            dsmObjectType_t    associateType, /* IN object type of associate
                                                 usually an index           */
 
            dsmDbkey_t         rootBlock,   /* IN dbkey of object root
                                             * - for INDEX: index root block
                                             * - 1st possible rec in the table.                                             */
            dsmMask_t           updateMask) /* bit mask identifying which
                                                 storage object attributes are
                                                 to be modified. */
{
    dsmStatus_t    returnCode;
    dsmRecid_t     recordId;
    lockId_t       lockId;
    int            recordSize;
    TEXT           recordBuffer[1024];
    TEXT           *ptableName = (TEXT*)"_storageObject";
 
 
    if( pcontext->pusrctl->uc_task == 0 )
    {
        returnCode = DSM_S_NO_TRANSACTION;
        goto done;
    }
 
    /* Read the storage object record from disk            */
    returnCode = omGetObjectRecordId(pcontext,objectType,object,associate,&recordId);

    /* Lock the record exclusive                       */
    if( returnCode )
        goto done;
 
    lockId.table = SCTBL_OBJECT;
    lockId.dbkey = recordId;
 
   
    for(returnCode = 1; returnCode != 0;)
    {
        returnCode = lklocky(pcontext,&lockId,DSM_LK_EXCL,LK_RECLK,
                         ptableName,NULL);
        if( returnCode == 0 )
            ;
        else if(returnCode == DSM_S_RQSTQUED)
        {
            if ((returnCode = lkwait(pcontext, LW_RECLK_MSG, ptableName)) != 0)
                goto done;
        }
        else
        {
            goto done;
        }
   }
 
    recordSize = rmFetchRecord(pcontext, DSMAREA_SCHEMA,
                               recordId, recordBuffer,
                               (COUNT)sizeof(recordBuffer),
                               0 /* not continuation */);

    if( recordSize > (int)sizeof(recordBuffer))
    {
        returnCode = DSM_S_RECORD_BUFFER_TOO_SMALL;
        goto doneButUnlockRecord;
    }

 
    if ( area != DSMAREA_INVALID )
    if (recPutLONG(recordBuffer, MAXRECSZ, (ULONG *)&recordSize,
                       SCFLD_OBJAREA,(ULONG)0, (ULONG)area, 0) )
    {
            returnCode = DSM_S_FAILURE;
            goto doneButUnlockRecord;
    }
 
    /* Modify the appropriate fields of the record.        */
    if( updateMask & DSM_OBJECT_UPDATE_ASSOCIATE )
    {
        /* set _Object-associate */
        if (recPutLONG(recordBuffer, MAXRECSZ, (ULONG *)&recordSize,
                       SCFLD_OBJASSOC,(ULONG)0, (ULONG)associate, 0) )
        {
            returnCode = DSM_S_FAILURE;
            goto doneButUnlockRecord;
        }
    }
    if( updateMask & DSM_OBJECT_UPDATE_ASSOCIATE_TYPE)
    {
        if (recPutLONG(recordBuffer, MAXRECSZ, (ULONG *)&recordSize,
                       SCFLD_OBJATYPE,(ULONG)0, (ULONG)associateType, 0) )
        {
            returnCode = DSM_S_FAILURE;
            goto doneButUnlockRecord;
        }
    }
 
 
    /* set _Object-rootBlock */
    if( updateMask & DSM_OBJECT_UPDATE_ROOT)
    {
        if (recPutBIG(recordBuffer, MAXRECSZ, (ULONG *)&recordSize,
                       SCFLD_OBJROOT,(ULONG)0, rootBlock, 0) )
        {
            returnCode = DSM_S_FAILURE;
            goto doneButUnlockRecord;
        }
    }
    /* Write the record.                                    */
    returnCode = rmUpdateRecord (pcontext, SCTBL_OBJECT,
                                 DSMAREA_SCHEMA,recordId,
                              recordBuffer, (COUNT)recordSize);
    if( returnCode )
        goto doneButUnlockRecord;
 
    /* Remove the old entry from the cache.                 */
    returnCode = omDelete(pcontext,objectType,associate,object);
 
doneButUnlockRecord:
    lkrels(pcontext,&lockId);
 
done:
 
 
    return (returnCode);
 
}  /* end cxIndexAnchorUpdate*/
