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
#include "cxprv.h"
#include "cxpub.h"
#include "shmpub.h"
#include "dsmpub.h"  /* for DSM API */
#include "dbcontext.h"
#include "dbpub.h"
#include "dbprv.h"   /* index array structure */
#include "keypub.h"  /* keyBracketBuild */
#include "lkpub.h"   /* needed for LKNOLOCK */
#include "mstrblk.h"
#include "scasa.h"   /* needed for field numbers in templates */
#include "svcspub.h" /* for svcDataType_t def */
#include "usrctl.h"
#include "utepub.h"
#include "utcpub.h"
#include "utfpub.h"
#include "utmsg.h"
#include "utspub.h"
#include "uttpub.h"
#include "ompub.h"
#include "bkaread.h"
#include "rmpub.h"
#include "objblk.h"
#include "geminikey.h"

#define NUM_INDEX_STATS_FIELDS  4  /* Table,index, component, rowsPerKey */

LOCALF
DSMVOID dbIndexStatsKey(TTINY *prow, dsmKey_t *pkey);

LOCALF
dsmStatus_t
dbIndexStatsDelete(dsmContext_t *pcontext, dsmCursid_t cursorId,
		   dsmKey_t  *pkey);

LOCALF
dsmStatus_t
dbIndexStatsCursorCreate(dsmContext_t *pcontext,dsmArea_t area,
			 DBKEY root, dsmCursid_t *pcursorId);

LOCALF
dsmStatus_t
dbIndexStatsCreate(
        dsmContext_t  *pcontext,           /* IN database context */
	LONG64         rowsPerKey,         /* IN                  */
        dsmKey_t      *pkey,               /* IN key to row       */
        dsmObject_t    tableNum);          /* IN table #          */

LOCALF
dsmStatus_t
dbIndexStatsGetData(dsmContext_t *pcontext,dsmCursid_t cursorId ,
		    LONG64 *prowsPerKey);


/* PROGRAM: dbIndexMove - Move a specified index to a new area
 *
 *
 * RETURNS: DSM_S_SUCCESS          - success
 *          DSM_S_FAILURE       - if call failed
 *
 * Assumption:  In order for other database clients to function correctly
 * during and after index move it has to be the case that:
 * 1. either the client accesses the root of the index only through 
 *    cxSearcgTree
 * 2. or all the work done with the index is under an index admin lock
 *    including the use of the storage object record of an index which
 *    will prevent index move from proceeding
 */
 
dsmStatus_t
dbIndexMove(
        dsmContext_t *pcontext,           /* IN database context */
        dsmIndex_t   indexNum,            /* inex to move */
        dsmArea_t    newArea,             /* area to move the new index to */
        dsmObject_t  tableNum,            /* table associate with the index */
        DBKEY        *newRootdbk)         /* new root of the index */
{
 
   omDescriptor_t     oldIndexDesc;
   dsmStatus_t        rc;
#if 0
   dsmStatus_t        retOmDelete;    /* return for omDelete() */
   dsmStatus_t        retOmCreate;    /* return for omCreate() */
#endif
   dsmText_t          *pname = (dsmText_t *)"sharedtablelock";
 
   /* steps:
      1.a. To prevent simultaneous execution of online utilities an index
           Admin lock needs to be acquired. This had to be changed and 
           placed in dsmObjectUpdate instead because of problems with
           index compact not using cxSearchTree for accessing root
      1.b. Get a shared lock on the table whose index needs to be moved.
      2. Make a copy of the index.
      3. Update the cache to have the new index area and root
      4. and call cxKill on the old index.
      5. release the shared lock on the table
   */

   /* start status collection */
 
   pcontext->pusrctl->uc_counter = 0;
   pcontext->pusrctl->uc_objectId = indexNum;
   pcontext->pusrctl->uc_objectType = DSMOBJECT_MIXINDEX;
   p_stcopy( pcontext->pusrctl->uc_operation, (TEXT*)"Index move");

 
   /* if transaction not already started  return failure*/
 
   if ( !pcontext->pusrctl->uc_task )
       return  DSM_S_FAILURE;         /* no transaction */
 
 
   rc = omFetch(pcontext,DSMOBJECT_MIXINDEX,indexNum,tableNum,&oldIndexDesc);
   if( rc )
   {
       return DSM_S_FAILURE;
   }

   if ( oldIndexDesc.area == newArea )
   {
       return DSM_S_FAILURE;
   }
 
 
   /* get shared lock on table to which the index belongs.
      set the state to waiting for shared table lock */ 

   pcontext->pusrctl->uc_state = DSMSTATE_LOCK_WAIT_TABLE;
   if ( dbLockRow( pcontext, oldIndexDesc.objectAssociate,
                       0, DSM_LK_SHARE, pname, 1 ) != DSM_S_SUCCESS)
        return DSM_S_FAILURE;
 
   /* clean delete lock chain */
    cxProcessDeleteChain( pcontext, tableNum, oldIndexDesc.area);

 
   /* make a copy of the index */
   pcontext->pusrctl->uc_state = DSMSTATE_INDEX_CREATE_NEW;
   if(( *newRootdbk = cxCopyIndex( pcontext, oldIndexDesc.objectRoot,
                             oldIndexDesc.area, newArea)) == -1 )
   {
        if ( dbUnlockRow( pcontext,oldIndexDesc.objectAssociate, 0) != DSM_S_SUCCESS)
             return DSM_S_FAILURE;
	return DSM_S_FAILURE;
   }
 
   /*update status information */
   pcontext->pusrctl->uc_state = DSMSTATE_INDEX_KILL_OLD;
   pcontext->pusrctl->uc_target =
       pcontext->pusrctl->uc_counter;
   pcontext->pusrctl->uc_counter = 0;
   

   /* update the object store to point to new index root  and
      invalidate omcache The following steps are needed:
      1. lock the storage object record in exclusive mode
      2. update the storage object record and 
      3. delete the om entry
      4. release the lock on the storage record
      - note that this function is invoked from dsmObjectUpdate
      - which already has a lock on the index Storage object
      - so the locking is not needed. step 5. need only be performed here.
      5.delete old omCache entry & create the new om cache entry for the index*/

#if 0
   this has been commented out because _StorageObject record could be 
   accessed in between omDelete and omCreate and this would cause the omCreate
   call to fail and its not clear at present if we need to watch out for 
   an omCreate failure of this kind if that is a problem.
   retOmDelete =omDelete( pcontext, DSMOBJECT_MIXINDEX, indexNum);

   if (retOmDelete != DSM_S_SUCCESS)
   {
        if ( dbUnlockRow( pcontext,oldIndexDesc.objectAssociate, 0) != DSM_S_SUCCESS)
             return DSM_S_FAILURE;
         return DSM_S_FAILURE;
   }



   newIndexDesc = oldIndexDesc;
   newIndexDesc.area = newArea;
   newIndexDesc.objectRoot = *newRootdbk;

   retOmCreate =omCreate(pcontext, DSMOBJECT_MIXINDEX, indexNum, &newIndexDesc);

   if (retOmCreate != DSM_S_SUCCESS)
   {
        if ( dbUnlockRow( pcontext,oldIndexDesc.objectAssociate, 0) != DSM_S_SUCCESS)
             return DSM_S_FAILURE;
         return DSM_S_FAILURE;
   }
#endif
 
   /*kill the old index */
   cxKill( pcontext, oldIndexDesc.area, oldIndexDesc.objectRoot, 
           indexNum, 0, tableNum, 0 );
 
   /* release table lock */
   if ( dbUnlockRow( pcontext,oldIndexDesc.objectAssociate, 0) != DSM_S_SUCCESS)
        return DSM_S_FAILURE;
 
   if ( utpctlc() )                    /* if user hit ctrl-C */
        return DSM_S_FAILURE;          /* return err the txn must backout*/

   return DSM_S_SUCCESS;
}

/* PROGRAM: dbValidArea - check to see that its valid to move the 
 * index to the area specified
 *
 *
 * RETURNS: 1 - if area valid
 *          0 - otherwise
 */

int
dbValidArea(
        dsmContext_t *pcontext,           /* IN database context   */
        dsmArea_t    area)                /* area to move index to */
{

        bkAreaDesc_t   *pbkAreaDesc = 
                        BK_GET_AREA_DESC(pcontext->pdbcontext, area);

        if (!pbkAreaDesc) 
           return 0;

        if( ((pbkAreaDesc != 0) &&
             (pbkAreaDesc->bkAreaType != DSMAREA_TYPE_DATA)) ||
             /* This check is required because BK_GET_AREA_DESC returns
                type 6 (data area) for a control area.
                Refer to BUG# 99-03-02-029 */
             (area == 1) )
           return 0;

        return 1;
}

/* PROGRAM: dbIndexStatsPut
 *
 */
dsmStatus_t
dbIndexStatsPut(
        dsmContext_t  *pcontext,           /* IN database context */
	dsmTable_t     tableNumber,        /* IN table index is on */
        dsmIndex_t     indexNumber,        /* IN                   */
	int            component,          /* component number     */
        LONG64        rowsPerKey)       /* IN/OUT rows per key value */
{
  return dbIndexStats(pcontext,tableNumber,indexNumber,component,&rowsPerKey,1);
}

/* PROGRAM: dbIndexStatsGet
 *
 */
dsmStatus_t
dbIndexStatsGet(
        dsmContext_t  *pcontext,           /* IN database context */
	dsmTable_t     tableNumber,        /* IN table index is on */
        dsmIndex_t     indexNumber,        /* IN                   */
	int            component,          /* component number     */
        LONG64        *prowsPerKey)       /* IN/OUT rows per key value */
{
  return dbIndexStats(pcontext,tableNumber,indexNumber,component,prowsPerKey,0);
}

/* PROGRAM: dbIndexStats
 *
 */
dsmStatus_t
dbIndexStats(
        dsmContext_t  *pcontext,           /* IN database context */
	dsmTable_t     tableNumber,        /* IN table index is on */
        dsmIndex_t     indexNumber,        /* IN                   */
	int            component,          /* component number     */
        LONG64        *prowsPerKey,       /* IN/OUT rows per key value */
	int             put)               /* 1 if put operation    */
{
  TTINY  row[64];
  TTINY  *prow;
  bmBufHandle_t  objHandle;
  objectBlock_t  *pobjectBlock;
  dsmRecid_t     rowid;
  int     componentLen;
  dsmStatus_t rc,dummy;
  dsmArea_t   area;
  dsmCursid_t cursorId;
  
  AUTOKEY(key,64)
  AUTOKEY(limitKey,64)

  if(SCTBL_IS_TEMP_OBJECT(tableNumber))
  {
    *prowsPerKey = 0;
    return 0;
  }

  rc = omIdToArea(pcontext,DSMOBJECT_MIXINDEX,indexNumber,tableNumber,&area);
  if (rc)
  {
      MSGD_CALLBACK(pcontext, "Failed to get index area for table %d, index %d",
                  tableNumber, indexNumber);
      return rc;
  }
  
  key.akey.index = SCIDX_INDEX_STATS;
  key.akey.area = area;
  key.akey.unknown_comp = 0;
  key.akey.unique_index = 0;
  key.akey.word_index = 0;
  key.akey.descending_key = 0;
  key.akey.ksubstr = 0;

  /* Get the object block of the index area to get the root
     of the index selectivity statistics index   */
  objHandle = bmLocateBlock(pcontext, area,
			    BLK2DBKEY(BKGET_AREARECBITS(area)), TOREAD);
  pobjectBlock = (objectBlock_t *)bmGetBlockPointer(pcontext,objHandle);

  key.akey.root = pobjectBlock->statisticsRoot;
  bmReleaseBuffer(pcontext,objHandle);

  prow = row;
  LONG_TO_STORE(prow,(LONG)tableNumber);
  prow += sizeof(LONG);
  LONG_TO_STORE(prow,(LONG)indexNumber);
  prow += sizeof(LONG);
  /* Store the component number   */
  LONG_TO_STORE(prow,(LONG)component);
  prow+=sizeof(LONG);

  /* Format a key string                        */
  dbIndexStatsKey(row, &key.akey);
  /* Add a limit range                          */
  bufcop(&limitKey.akey,&key.akey,sizeof(dsmKey_t) + key.akey.keyLen);
  rc = gemKeyAddHigh(&limitKey.akey.keystr[key.akey.keyLen], &componentLen);
  limitKey.akey.keyLen += componentLen - FULLKEYHDRSZ;
  key.akey.keyLen -= FULLKEYHDRSZ;
  /* See if we can find it                      */
  rc = dbIndexStatsCursorCreate(pcontext,area, key.akey.root,&cursorId);
#if 0
  rc = dbCursorCreate(pcontext,tableNumber,indexNumber,&cursorId,NULL);
  if(rc)
    return rc;
#endif

  rc = cxFind(pcontext,&key.akey, &limitKey.akey, &cursorId,
	      0, &rowid, DSMPARTIAL, DSMFINDFIRST, DSM_LK_NOLOCK);
  if(rc == DSM_S_ENDLOOP)
  {
    /* Create the row and index entry    */
    if(put)
      rc = dbIndexStatsCreate(pcontext, *prowsPerKey, &key.akey, 
                         tableNumber);
    else
    {
      *prowsPerKey = 0;
      rc = 0; /* Ok not to find them on a look up.  These rows
		 don't exist until the first table analyze  */
    }
  }
  else 
  {
    if(put)
    {
      /* Replace the entry                      */
      rc = dbIndexStatsDelete(pcontext,cursorId, &limitKey.akey);

      rc = dbIndexStatsCreate(pcontext,*prowsPerKey, &key.akey,
			 tableNumber);
    }
    else
    {
      /* Extract recs per key value from the index entry  */
      rc = dbIndexStatsGetData(pcontext,cursorId,prowsPerKey);
    }
  }
  dummy = dbCursorDelete(pcontext,&cursorId,0);
  return rc;
}

LOCALF
dsmStatus_t
dbIndexStatsCursorCreate(dsmContext_t *pcontext,dsmArea_t area,
			 DBKEY root, dsmCursid_t *pcursorId)
{
  cxcursor_t  *pcursor;
  
  *pcursorId = NULLCURSID;

  pcursor = cxGetCursor(pcontext,pcursorId,SCIDX_INDEX_STATS);
  
  pcursor->root = root;
  pcursor->area = area;
  pcursor->unique_index = 0;
  pcursor->word_index = 0;
  pcursor->multi_index = 0;
  pcursor->table = 0;
  pcursor->tableArea = 0;
  
  return 0;
}

LOCALF
dsmStatus_t
dbIndexStatsDelete(dsmContext_t *pcontext, dsmCursid_t cursorId,
		   dsmKey_t  *pkey)
{
  cxcursor_t  *pcursor;
  COUNT        keySize;
  dsmStatus_t  rc;

  pcursor = cxGetCursor(pcontext,&cursorId,0);
  keySize =  xct(&pcursor->pkey[KS_OFFSET]);
  bufcop(pkey->keystr,pcursor->pkey, keySize + FULLKEYHDRSZ);

  keySize -= pkey->keystr[keySize-1+FULLKEYHDRSZ] & 0x0f;
  pkey->keyLen = keySize;

  rc = cxDeleteNL(pcontext,pkey,1,IXLOCK, pcursor->table, NULL);
  
  return rc;
}

LOCALF
dsmStatus_t
dbIndexStatsGetData(dsmContext_t *pcontext,dsmCursid_t cursorId ,
		LONG64 *prowsPerKey)
{
  cxcursor_t   *pcursor;
  TEXT         *pos;
  TEXT          buffer[sizeof(LONG64)];
  dsmStatus_t   rc;
  int           i;
  int           fieldIsNull;

  pcursor = cxGetCursor(pcontext,&cursorId,0);
  pos = pcursor->pkey + FULLKEYHDRSZ + 4;

  for ( i = 0; i < NUM_INDEX_STATS_FIELDS; i++)
  {
      rc = gemIdxComponentToField(pos, GEM_INT,
				  buffer,
				  sizeof(buffer),
				  0,
				  &fieldIsNull);    
      while(*pos++);
  }
  *prowsPerKey = STORE_TO_LONG64(buffer);
  return rc;
}

LOCALF
DSMVOID dbIndexStatsKey(TTINY *prow, dsmKey_t *pkey)
{
  int keyStringLen,componentLen,i;
  int rc;

  /* Create an index entry for this row   */
  rc = gemKeyInit(pkey->keystr,&keyStringLen, pkey->index);
  for(i = 0; i < NUM_INDEX_STATS_FIELDS - 1; i++, prow += sizeof(LONG))
  {
    rc = gemFieldToIdxComponent(prow,
				sizeof(LONG),GEM_INT,
				0, 0, &pkey->keystr[keyStringLen],
				64, &componentLen);
    keyStringLen += componentLen;
  }
  pkey->keyLen = (COUNT)keyStringLen;
  
  return;
}

/* PROGRAM: dbIndexStatsCreate
 *
 */
LOCALF
dsmStatus_t
dbIndexStatsCreate(
        dsmContext_t  *pcontext,           /* IN database context */
	LONG64         rowsPerKey,         /* IN                  */
        dsmKey_t      *pkey,               /* IN key to row       */
        dsmObject_t    tableNum)           /* IN table #          */
{
  dsmStatus_t  rc = 0;
  int          componentLen;
  TEXT         buffer[sizeof(rowsPerKey)];

  /* Store a zero for the rows per key */
  LONG64_TO_STORE(buffer,rowsPerKey);
  
  rc = gemFieldToIdxComponent(buffer,
			      sizeof(LONG64),GEM_INT,
			      0, 0, &pkey->keystr[pkey->keyLen+FULLKEYHDRSZ],
			      64, &componentLen);
  if(rc)
    return rc;

  pkey->keyLen += componentLen;

  rc = cxAddNL(pcontext, pkey, 1, NULL, tableNum);

  return rc;
}

/* PROGRAM: dbIndexAnchor - Maintain the index anchor table
 *
 * RETURNS: DSM_S_SUCCESS
 *          DSM_S_FAILURE 
 */
dsmStatus_t
dbIndexAnchor(dsmContext_t *pcontext, ULONG area, dsmText_t *pname,
              DBKEY ixRoot, dsmText_t *pnewname, int mode)
{

     ixAnchorBlock_t  *pindexAnchor;
     bmBufHandle_t     objHandle;
     ixAnchors_t       indexAnchorEntry;
     COUNT             i;

    /* update the index anchor block with this root */
    objHandle = bmLocateBlock(pcontext, area,
                            BLK3DBKEY(BKGET_AREARECBITS(area)), TOREAD);
    pindexAnchor = (ixAnchorBlock_t *)bmGetBlockPointer(pcontext,objHandle);

    /* Find an empty entry for the new index */
    for (i = 0; i < IX_INDEX_MAX; i++)
    {
        if (mode == IX_ANCHOR_CREATE)
        {
            if (pindexAnchor->ixRoots[i].ixRoot == 0)
                break;
        }
        else
        {
            if (stpcmp(pname, pindexAnchor->ixRoots[i].ixName) == 0)
                break;
        }
    }

    if (i == IX_INDEX_MAX)
    {
        MSGD_CALLBACK(pcontext,"Index Anchor slot not found for index %s",
              pname);
        return DSM_S_FAILURE;
    }

    /* now that we have the slot in the block either add the entry
       or remove it */
    if (mode == IX_ANCHOR_CREATE)
    {
        indexAnchorEntry.ixRoot = ixRoot;
        stcopy(indexAnchorEntry.ixName, pname);
    }
    else if (mode == IX_ANCHOR_UPDATE)
    {
        if (ixRoot)
            indexAnchorEntry.ixRoot = ixRoot;
        else
            indexAnchorEntry.ixRoot = pindexAnchor->ixRoots[i].ixRoot;
        stcopy(indexAnchorEntry.ixName, pnewname);
    }
    else
    {
        indexAnchorEntry.ixRoot = 0;
        stnclr(indexAnchorEntry.ixName, IX_NAME_LEN);
    }

    bmReleaseBuffer(pcontext, objHandle);
    rlTXElock (pcontext, RL_TXE_SHARE, RL_MISC_OP);
    objHandle = bmLocateBlock(pcontext, area,
                            BLK3DBKEY(BKGET_AREARECBITS(area)), TOMODIFY);
    pindexAnchor = (ixAnchorBlock_t *)bmGetBlockPointer(pcontext,objHandle);
    bkReplace(pcontext, objHandle, (TEXT *)&pindexAnchor->ixRoots[i],
              (TEXT *)&indexAnchorEntry, sizeof(indexAnchorEntry));
    bmReleaseBuffer(pcontext, objHandle);
    rlTXEunlock (pcontext);

    return DSM_S_SUCCESS;
}
