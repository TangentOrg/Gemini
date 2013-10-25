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

#include "pscret.h"
#include "dbconfig.h"

#include "bkpub.h"
#include "cxmsg.h"
#include "drmsg.h"
#include "cxpub.h"
#include "cxprv.h"
#include "dbcontext.h"
#include "dsmpub.h"
#include "ompub.h"
#include "scasa.h"
#include "usrctl.h"
#include "lkpub.h"
#include "utspub.h"    /* stcopy, stnclr */

#include <string.h>
#include <setjmp.h>

/* PROGRAM: dsmKeyCreate - Adds a key to an index.
 *          
 *
 * RETURNS: 
 */
dsmStatus_t
dsmKeyCreate(
        dsmContext_t  *pcontext,   /* IN database context */
        dsmKey_t      *pkey,       /* the key to be inserted in the index*/
        dsmTable_t   table,        /* the table the index is on.           */
        dsmRecid_t   recordId,     /* the recid of the record the 
			              index entry is for         */
        dsmText_t    *pname)	   /* refname for Rec in use msg    */
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    lockId_t     lockId;         /* lock identifier for indexed record */
    dsmStatus_t  returnCode;
    int          i;
    omDescriptor_t   objDesc;
    AUTOKEY(wordKey,KEYSIZE);

    if ( SCIDX_IS_VSI(pkey->index) )
        return DSM_S_SUCCESS;

    pdbcontext->inservice++;

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                                          (TEXT *)"dsmKeyCreate");
        goto done;
    }

    lockId.table = table;
    lockId.dbkey = recordId;
    
    returnCode = omFetch(pcontext, DSMOBJECT_MIXINDEX, pkey->index,
			 table, &objDesc);
    if( returnCode )
	goto done;

    pkey->area = objDesc.area;
    pkey->root = objDesc.objectRoot;
    pkey->unique_index = (dsmBoolean_t)
                         (objDesc.objectAttributes & DSMINDEX_UNIQUE);
    pkey->word_index =   (dsmBoolean_t)
                         (objDesc.objectAttributes & DSMINDEX_WORD);

    if (pkey->word_index)
    {
        /* For word indexed keys the pointer to the keystr is a list
           of key strings for each word in the string being indexed.     */
        /* We loop through the word list here rather than make it the
           responsibility of the caller to reduce the number of times
           the object cache (omFetch) has to be accessed.                */
        /* We must preserve the pkey->keystr because cxAddEntry extends
           the input key with the dbkey -- so can't just pass pkey->keystr. */
        bufcop(&wordKey.akey,pkey,sizeof(dsmKey_t));
        
        for(i = 0; i < (pkey->keyLen + 3); i += (int)pkey->keystr[i] + 3)
        {
            /* Copy one word into the key structure                 */
            bufcop(wordKey.akey.keystr, &pkey->keystr[i],
                   (int)pkey->keystr[i] + 3);
            wordKey.akey.keyLen = pkey->keystr[i];
            returnCode = cxAddEntry(pcontext, &wordKey.akey,
                                    &lockId, pname, table);

            if( returnCode )
                break;
        }
        goto done;
    }

    returnCode = cxAddEntry(pcontext, pkey, &lockId, pname, table);
    
done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;
    return (returnCode);

}  /* end dsmKeyCreate */


/* PROGRAM: dsmKeyDelete - Deletes a key from an index.
 *          
 *
 * RETURNS: 
 */
dsmStatus_t
dsmKeyDelete(   
        dsmContext_t *pcontext,
        dsmKey_t     *pkey,    /* IN the key to be inserted in the index*/
        COUNT         table,   /* IN the table the index is on.         */
        dsmRecid_t    recordId,/* IN the recid of the record the 
        		          index entry is for                   */
        dsmMask_t     lockmode,/* Unused - should be set to zero       */
        dsmText_t    *pname)   /* IN refname for Rec in use msg         */
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    lockId_t    lockId;         /* Lock id for indexed record     */
    int         returnCode, i;
    omDescriptor_t indexDesc;
    AUTOKEY(wordKey,KEYSIZE);

    if ( SCIDX_IS_VSI(pkey->index) )
        return DSM_S_SUCCESS;

    pdbcontext->inservice++;

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                                          (TEXT *)"dsmKeyDelete");
        goto done;
    }

    lockmode = 0;

    lockId.table = table;
    lockId.dbkey = recordId;
    returnCode = omFetch(pcontext, DSMOBJECT_MIXINDEX, pkey->index, table, &indexDesc);
    if (returnCode)
        goto done;

    pkey->area = indexDesc.area;
    pkey->root = indexDesc.objectRoot;
    pkey->unique_index = (dsmBoolean_t)
                         (indexDesc.objectAttributes & DSMINDEX_UNIQUE);
    pkey->word_index =   (dsmBoolean_t)
                         (indexDesc.objectAttributes & DSMINDEX_WORD);
    
    if (pkey->word_index)
    {
        /* For word indexed keys the pointer to the keystr is a list
           of key strings for each word in the string being indexed.     */
        /* We loop through the word list here rather than make it the
           responsibility of the caller to reduce the number of times
           the object cache (omFetch) has to be accessed.                */
        /* We must preserve the pkey->keystr because cxAddEntry extends
           the input key with the dbkey -- so can't just pass pkey->keystr. */
        bufcop(&wordKey.akey,pkey,sizeof(dsmKey_t));
        
        for(i = 0; i < (pkey->keyLen + 3); i += (int)pkey->keystr[i] + 3)
        {
            /* Copy one word into the key structure                 */
            bufcop(wordKey.akey.keystr, &pkey->keystr[i],
                   (int)pkey->keystr[i] + 3);
            wordKey.akey.keyLen = pkey->keystr[i];
            returnCode = cxDeleteEntry(pcontext, &wordKey.akey, &lockId,
			 lockmode, table, pname);

            if( returnCode )
                break;
        }
        goto done;
    }

    returnCode = cxDeleteEntry(pcontext, pkey, &lockId, lockmode, table, pname);

done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;
    return (returnCode);

}  /* end dsmKeyDelete */

/* PROGRAM: dsmIndexCompact - increase space utilization for indexes
 *
 *
 * RETURNS: DSMSUCCESS          - success
 *          DSM_S_NOCURSOR      - the cursor id is invalid
 *          DSM_S_INVALID_INDEX - invalid index # provided
 *
 */
dsmStatus_t
dsmIndexCompact(
        dsmContext_t  *pcontext,           /* IN database context */
        dsmIndex_t    index,               /* IN index to be compressed */
        dsmTable_t    table,               /* IN table associated idx */
        ULONG         percentUtilization)  /* IN % required space util. */
{
    dbcontext_t    *pdbcontext = pcontext->pdbcontext;
    usrctl_t      *pusr = pcontext->pusrctl;
    dsmCursid_t     xcursor, 
                   *pCursid;
    cxcursor_t     *pCursor;
    dsmStatus_t     returnCode,
                    retCode;
    omDescriptor_t  omDesc;

    int         passNumber = -1;   /* # of passes */
    DBKEY       blockCounter,       /* # of blocks processed */
                totalBytes;         /* # of bytes utilizeded */

    pdbcontext->inservice++; /* postpone signal handler processing */
 
    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                                          (TEXT *)"dsmIndexCompact");
        goto done;
    }

    /* make sure desired utilization percent is within
       acceptable range */
    if ((percentUtilization < IDXBLK_MIN_UTILIZATION) ||
        (percentUtilization > IDXBLK_MAX_UTILIZATION))
    {
        MSGN_CALLBACK(pcontext, drMSG690,
                      percentUtilization, IDXBLK_DEFAULT_UTILIZATION);
        percentUtilization = IDXBLK_DEFAULT_UTILIZATION;
    }

    /* set the admin status area up */
    stcopy(pusr->uc_operation, (TEXT *)"Index Compact");
    pusr->uc_state = DSMSTATE_LOCK_WAIT_ADMIN;
    pusr->uc_objectId = index;
    pusr->uc_objectType = DSMOBJECT_MIXINDEX;
    pusr->uc_counter = (ULONG)0;
    pusr->uc_target = (ULONG)0;

    /* lock the index to prevent conflicts and to get a clean read
       of the root when creating the cursor */
    returnCode = lkAdminLock(pcontext, index, 0, DSM_LK_SHARE);

    if (returnCode)
        goto done;


    /* get a cursor, set the cursid to invalid so cxGetCursor
       does not try to reuse it */
    pCursid = &xcursor;
    *pCursid = DSMCURSOR_INVALID;
    pCursor = cxGetCursor(pcontext, pCursid, index);
    /* has a valid cursor id been provided? */
    if (pCursid  == NULL)
    {
        returnCode =  DSM_S_NOCURSOR;
        goto adminUnlock;
    }
 
    returnCode = omFetch(pcontext, DSMOBJECT_MIXINDEX, index, table, &omDesc);
    if ( returnCode )
    {
        /* was it an invalid index number? otherwise the cache
           was out of room, therefor return DSM_S_NOSTG */
        if ((returnCode == DSM_S_RMNOTFND) || (returnCode == DSM_S_ENDLOOP))
           returnCode = DSM_S_INVALID_INDEX_NUMBER; 
        goto removeCursor;
    }

    pCursor->area = omDesc.area;
    pCursor->root = omDesc.objectRoot;
    pCursor->unique_index =
             (dsmBoolean_t)omDesc.objectAttributes & DSMINDEX_UNIQUE;
    pCursor->word_index =
              (dsmBoolean_t)omDesc.objectAttributes & DSMINDEX_WORD;
    pCursor->multi_index =
              (dsmBoolean_t)omDesc.objectAttributes & DSMINDEX_MULTI_OBJECT;
 
    if( !pCursor->multi_index )
    {
        returnCode = omIdToArea(pcontext,DSMOBJECT_MIXTABLE,
                            table, table, &pCursor->tableArea);
        if( returnCode )
            goto removeCursor;
        pCursor->table = table;
    }
    else
    {
        pCursor->table = 0;
        pCursor->tableArea = 0;
    }

    /* initialize the bytes processed counter */
    totalBytes = 0;

    /*  the number of ixcompact passes will be the number of idx
        levels + 1 */
    while (passNumber != pCursor->level)
    {
        /* init the level in pCursor for every pass */
        pCursor->level = (LONG)0;

        /* we have the admin lock, now update the state since we are
           going to scan the idx delete chaing after determiing the area*/
        pusr->uc_state = DSMSTATE_SCAN_DELETE_CHAIN;

        /* now we have the area, scan the delete chain.
           Note: cxRemoveFromDeleteChain alwasy returns 0  */
        cxProcessDeleteChain(pcontext, table, pCursor->area);

        /* update the state info */
        pusr->uc_state = DSMSTATE_COMPACT_NONLEAF;
 
        /* do the work */
        returnCode = cxCompactIndex(pcontext, pCursid, percentUtilization,
                              &blockCounter, &totalBytes, table, CX_BLOCK_COMPACT);

        /* did we reach the bottom of the idx on this pass? */
        if (returnCode == DSM_S_REACHED_BOTTOM_LEVEL ||
            returnCode == DSM_S_SUCCESS)
        {
            /* set up for another pass */
            passNumber++;
            pusr->uc_counter = (ULONG)0;
            pusr->uc_target = (ULONG)0;
            pCursor->blknum = 0;
            pCursor->updctr = 0;
            returnCode = DSM_S_SUCCESS;
        }
        else
        {
            /* didn't reach bottom , better leave */
            break;
        }
    } /* while (passNumber != pCursor->level) */
 
    /* clean up admin status area */
    stnclr(pcontext->pusrctl->uc_operation,
           sizeof(pcontext->pusrctl->uc_operation));
    pusr->uc_state = (ULONG)0;
    pusr->uc_objectId = (COUNT)0;
    pusr->uc_objectType = (ULONG)0;
    pusr->uc_counter = (ULONG)0;
    pusr->uc_target = (ULONG)0;

removeCursor:
   cxDeactivateCursor(pcontext, pCursid);

adminUnlock:
   /* unlock the index */
   retCode = lkAdminUnlock(pcontext, index, 0);

done: 
    dsmThreadSafeExit(pcontext);
    pdbcontext->inservice--;

    return returnCode;

}  /* end dsmIndexCompact */

/* PROGRAM: dsmIndexRowsInRange - Returns percent of rows in range
 *                                between input base and limit 
 *
 *
 * RETURNS: DSMSUCCESS          - success
 *
 */
dsmStatus_t DLLEXPORT
dsmIndexRowsInRange(
        dsmContext_t  *pcontext,           /* IN database context */
        dsmKey_t      *pbase,              /* IN bracket base     */
        dsmKey_t      *plimit,             /* IN bracket limit    */
        dsmObject_t    tableNum,           /* IN table indexed on */
        float         *pctInrange)         /* Out percent of index in bracket */

{
    dsmStatus_t  returnCode = 0;
    dbcontext_t  *pdbcontext = pcontext->pdbcontext;
    omDescriptor_t  omDesc;

    pdbcontext->inservice++; /* postpone signal handler processing */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                                          (TEXT *)"dsmIndexRowsInRange");
        goto done;
    }

    returnCode = omFetch(pcontext,DSMOBJECT_MIXINDEX,pbase->index,
                         tableNum,&omDesc);
    if(returnCode)
      goto done;

    *pctInrange = cxEstimateTree(pcontext,
                                 &pbase->keystr[FULLKEYHDRSZ],pbase->keyLen,
                                 &plimit->keystr[FULLKEYHDRSZ],plimit->keyLen,
                                 omDesc.area,
                                 omDesc.objectRoot);
done:
    dsmThreadSafeExit(pcontext);
    pdbcontext->inservice--;

    return returnCode;
} 

/* PROGRAM: dsmIndexStatsPut - Update index stats with rows per key value
 *                             for the specified index and component.
 *                             
 *
 *
 * RETURNS: DSMSUCCESS          - success
 *
 */
dsmStatus_t DLLEXPORT
dsmIndexStatsPut(
        dsmContext_t  *pcontext,           /* IN database context */
	dsmTable_t     tableNumber,        /* IN table index is on */
        dsmIndex_t     indexNumber,        /* IN                   */
	int            component,          /* IN index component number */
        LONG64        rowsPerKey)         /* IN rows per key value */
{
    dsmStatus_t  returnCode = 0;
    dbcontext_t  *pdbcontext = pcontext->pdbcontext;

    pdbcontext->inservice++; /* postpone signal handler processing */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                                          (TEXT *)"dsmIndexStatsPut");
        goto done;
    }
    returnCode = dbIndexStatsPut(pcontext, tableNumber, indexNumber,
			 component, rowsPerKey);
done:
    dsmThreadSafeExit(pcontext);
    pdbcontext->inservice--;

    return returnCode;
}

/* PROGRAM: dsmIndexStatsGet - Get index stats with rows per key value
 *                             for the specified index and component.
 *                             
 *
 *
 * RETURNS: DSMSUCCESS          - success
 *
 */
dsmStatus_t DLLEXPORT
dsmIndexStatsGet(
        dsmContext_t  *pcontext,           /* IN database context */
	dsmTable_t     tableNumber,        /* IN table index is on */
        dsmIndex_t     indexNumber,        /* IN                   */
	int            component,          /* IN index component number */
        LONG64        *prowsPerKey)        /* IN rows per key value */
{
    dsmStatus_t  returnCode = 0;
    dbcontext_t  *pdbcontext = pcontext->pdbcontext;

    pdbcontext->inservice++; /* postpone signal handler processing */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                                          (TEXT *)"dsmIndexStatsGet");
        goto done;
    }
    returnCode = dbIndexStatsGet(pcontext, tableNumber, indexNumber,
			 component, prowsPerKey);
done:
    dsmThreadSafeExit(pcontext);
    pdbcontext->inservice--;

    return returnCode;
}
