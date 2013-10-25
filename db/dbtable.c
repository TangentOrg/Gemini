
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
#include "shmpub.h"
#include "dsmpub.h"   /* for DSM API */
#include "dbcontext.h"
#include "cxpub.h"    /* cxcursor_t  */
#include "cxprv.h"    /* notes for index actions */
#include "dbpub.h"
#include "dbprv.h"    
#include "lkpub.h"    /* needed for LKNOLOCK */
#include "ompub.h"    /* use of om structures */
#include "omprv.h"    /* objectNote_t */
#include "recpub.h"   /* prototypes for rec services calls */
#include "rmprv.h"    /* prototype for rmDeleteRec */
#include "rmpub.h"    /* prototypes for rm calls */
#include "scasa.h"    /* needed for field numbers in templates */
#include "stmpub.h"   /* stRentd and stVacate */
#include "stmprv.h"
#include "umlkwait.h" /* needed for LW_RECLK_MSG */
#include "utcpub.h"   /* needed for utpctlc */
#include "utspub.h"   /* needed for sct and stcopy -see Richb */
#include "usrctl.h"
#include "objblk.h"

/*************************************************************/
/* Local Function Prototypes for dbtable.c                   */
/*************************************************************/
LOCALF
dsmStatus_t
dbRMBLKscan(dsmContext_t *pcontext,rmBlock_t *pbk, 
            int  recnum, dsmRecord_t *precordDesc,
            dsmRecid_t  *pnextRecid);
LOCALF
dsmStatus_t
rowLockIt(dsmContext_t *pcontext, dsmArea_t areaNumber,
          dsmTable_t table, dsmRecid_t recid, int lockMode,
          bmBufHandle_t bufHandle, int *pretry);
LOCALF
dsmStatus_t
createIndexRoots(dsmContext_t *pcontext, 
                 dsmArea_t indexArea,int numIndexes,
                 dsmTable_t tableNumber,
                 dsmIndex_t  *pindexes);

LOCALF
dsmStatus_t
updateObjectRoot(dsmContext_t *pcontext, dsmIndex_t indexNumber,
                 dsmObject_t    tableNum, DBKEY   rootDbkey);

LOCALF
DSMVOID clearRepairStatus(dsmContext_t *pcontext, dsmArea_t areaNumber);

/*************************************************************/
/* End Local Function Prototypes                             */
/*************************************************************/

/* PROGRAM - To scan a storage area for records.
 *
 * RETURNS  - DSM_SUCCESS
 *          - DSM_S_RECORD_BUFFER_TOO_SMALL
 *          - DSM_S_LKTBFULL
 *          - DSM_S_CTRLC  for lock wait timeout
 */
dsmStatus_t
dbTableScan(
            dsmContext_t    *pcontext,        /* IN database context */
            dsmRecord_t     *precordDesc,     /* IN/OUT row descriptor */
            int              direction,       /* IN next, previous...  */
            dsmMask_t        lockMode,        /* IN nolock share or excl */
            int              repair)          /* 1 if table scan is part
                                                 of a table repair 
                                                 operation               */ 
{
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    dsmStatus_t    rc;
    dsmArea_t      areaNumber;
    bkAreaDesc_t   *pbkarea;
    bmBufHandle_t   bufHandle;
    rmBlock_t       *pbk;
    int              areaRecbits;
    int              recmask;
    int              recnum;
    int              recsPerBlock;
    dsmRecid_t       nextRecid;
    dsmRecid_t       hiwaterRecid;
    int              bufferLockMode;


    /* Get the area the table is in         */
  
    rc = omIdToArea(pcontext,DSMOBJECT_MIXTABLE,precordDesc->table,
                    precordDesc->table, &areaNumber);
    if (rc)
      goto errorReturn;
    
    if(repair)
        bufferLockMode = TOMODIFY;
    else
        bufferLockMode = TOREAD;

    pbkarea = BK_GET_AREA_DESC(pdbcontext,areaNumber);
    areaRecbits = pbkarea->bkAreaRecbits;
    recsPerBlock = 1 << areaRecbits;
    recmask     = RECMASK(areaRecbits);
    hiwaterRecid = (pbkarea->bkHiWaterBlock + 1) << areaRecbits;
	  
    if ( direction == DSMFINDFIRST || precordDesc->recid == 0)
    {
#ifdef VST_ENABLED
      if (RM_IS_VST(precordDesc->table))
      {
        precordDesc->recid = 1;
      }
      else
#endif  /* VST ENABLED */
      {
	if(areaNumber == DSMAREA_TEMP)
	{
	  precordDesc->recid = omTempTableFirst(pcontext,precordDesc->table);
	  if(!precordDesc->recid)
	    return DSM_S_ENDLOOP;
	}
	else
	  /* Advance past the area and object blocks */
	  precordDesc->recid = (1 << areaRecbits) * 3;
      }
    }
    else
    {
      precordDesc->recid++;
    }

#ifdef VST_ENABLED
  if (RM_IS_VST(precordDesc->table))
  {
    int recSize;

    recSize = rmFetchVirtual(pcontext, precordDesc->table, precordDesc->recid,
                             precordDesc->pbuffer, precordDesc->recLength);
    if (recSize == RMNOTFND)
    {
      rc = DSM_S_ENDLOOP;
    }
    else if(recSize < 0)
    {
      rc = recSize;
    }
    else if(recSize > (int)precordDesc->recLength)
    {
      rc = DSM_S_RECORD_BUFFER_TOO_SMALL;
    }
  }
  else
#endif  /* VST ENABLED */
  {
    for(;precordDesc->recid < hiwaterRecid;)
    {
      recnum = precordDesc->recid & recmask;
      bufHandle = bmLockBuffer(pcontext, areaNumber, 
                                precordDesc->recid & (~ recmask), 
                                bufferLockMode, 1);
      pbk = (rmBlock_t *)bmGetBlockPointer(pcontext,bufHandle);
  
      if (pbk->rm_bkhdr.bk_type != RMBLK)
      {
        precordDesc->recid = pbk->rm_bkhdr.bk_dbkey + recsPerBlock;
        bmReleaseBuffer(pcontext,bufHandle);
        continue;
      }
      if(repair && recnum == 0)
      {
         /* If repairing the table see if the block belongs on the
            rm chain.                                               */
         COUNT dummy;
         pbk->rm_bkhdr.bk_frchn = NOCHN;
         rmFree(pcontext, areaNumber, bufHandle, pbk,&dummy);
      }
          
      if (recnum >= (int)pbk->numdir)
      {
	if(areaNumber == DSMAREA_TEMP)
	{
	  precordDesc->recid = pbk->rm_bkhdr.bk_nextf;
	  if(!precordDesc->recid)
	  {
	    bmReleaseBuffer(pcontext,bufHandle);
	    break;
	  }
	}
	else
	  precordDesc->recid = pbk->rm_bkhdr.bk_dbkey + recsPerBlock;

        bmReleaseBuffer(pcontext,bufHandle);
        continue;
      }
      rc = dbRMBLKscan(pcontext,pbk,recnum,precordDesc,&nextRecid);
      if( rc == 0 )
      {
        int retry;
        /* Got a record so lock it  */
        rc = rowLockIt(pcontext,areaNumber,precordDesc->table,
                        precordDesc->recid,lockMode,
                        bufHandle,&retry);
        if(retry)
          continue;
        
        STATINC (rmgtCt);
        if(repair && rc == 0)
            bkRowCountUpdate(pcontext, areaNumber, precordDesc->table, 1);

        if(rc == 0 && nextRecid )
        {
          int   bufferToGo = precordDesc->maxLength - precordDesc->recLength;
          int   recSize;

          /* Record spans blocks so get the rest of it */
          recSize = rmFetchRecord(pcontext,areaNumber,nextRecid,
                          precordDesc->pbuffer + precordDesc->recLength,
                          bufferToGo,
                          CONT);
          if(recSize > bufferToGo)
            rc = DSM_S_RECORD_BUFFER_TOO_SMALL;
          else if (recSize == RMNOTFND)
          {
            lockId_t  lockId; 
            lockId.table = precordDesc->table;
            lockId.dbkey = precordDesc->recid;
            lkrelsy(pcontext,&lockId,LK_RECLK);           
            continue;
          }
          precordDesc->recLength += recSize;
        }
        if(rc == 0 && lockMode == DSM_LK_NOLOCK)
        {
          /* rowLockIt got a record get lock so unlock it here */
          lockId_t   lockId;
          lockId.table = areaNumber;
          lockId.dbkey = precordDesc->recid;
          lkrgunlk(pcontext,&lockId);
        }       
        return rc;
      }
      else if (rc == 1)
      {
        /* Advance to the next block    */
	if(areaNumber == DSMAREA_TEMP)
	{
	  precordDesc->recid = pbk->rm_bkhdr.bk_nextf;
	  if(!precordDesc->recid)
	  {
	    bmReleaseBuffer(pcontext,bufHandle);
	    break;
	  }
	}
	else
	  precordDesc->recid = pbk->rm_bkhdr.bk_dbkey + recsPerBlock;
        bmReleaseBuffer(pcontext,bufHandle);
      }
      else
      {
        bmReleaseBuffer(pcontext,bufHandle);
        goto errorReturn;
      }
    }
    /* If we get here it means we ran out of blocks to read  */
    rc = DSM_S_ENDLOOP;
    if(repair)
    {
      /* Clear the repair status flag in the object block  */
      clearRepairStatus(pcontext,areaNumber);
    }
  }
 errorReturn:
  return rc;
}

LOCALF
DSMVOID clearRepairStatus(dsmContext_t *pcontext, dsmArea_t areaNumber)
{
   objectBlock_t *ptableObjectBlock;
   bmBufHandle_t  tableObjectHandle;

   /* Issue a synchronous checkpoint             */
   rlSynchronousCP(pcontext);

   tableObjectHandle = bmLocateBlock(pcontext, areaNumber,
                         BLK2DBKEY(BKGET_AREARECBITS(areaNumber)), TOMODIFY);
   ptableObjectBlock = (objectBlock_t *)bmGetBlockPointer(pcontext,
                                                          tableObjectHandle);

   ptableObjectBlock->objStatus = DSM_OBJECT_OPEN;

   bmMarkModified(pcontext, tableObjectHandle,0);
   bmReleaseBuffer(pcontext,tableObjectHandle); 
   return;
}

/* PROGRAM: dbRMBLKscan - Scans the rm block looking for the
 *                        beginning of record starting with record
 *                        recnum.
 *                       
 *                       
 *
 * RETURNS: DSM_S_SUCCESS - success  Got a record 
 *          DSM_S_RECORD_BUFFER_TOO_SMALL
 *          1              No record found in the block
 */
LOCALF
dsmStatus_t
dbRMBLKscan(dsmContext_t *pcontext _UNUSED_,
            rmBlock_t *pbk, 
            int  recnum,
            dsmRecord_t *precordDesc,
            dsmRecid_t *pnextRecid)
{
    dsmStatus_t  rc = 0;
    TEXT             *prec;
    COUNT            reclen;
    
    *pnextRecid = 0;

    for(;recnum < (int)pbk->numdir;recnum++)
    {
      if(pbk->dir[recnum] > 0 &&
	 !(pbk->dir[recnum] & (CONT | HOLD | RMBLOB)))
      {
        /* Got a record                */
        prec = (TEXT *)pbk + 
          (pbk->dir[recnum] & RMOFFSET);
        /* Is it a split record        */
        if(pbk->dir[recnum] & SPLITREC)
        {
             *pnextRecid = xdbkey(prec);
             prec += sizeof(DBKEY);          /* skip past the next pointer */
        }
        reclen = xct(prec);
        if (reclen < 0)
        {
            MSGD_CALLBACK(pcontext, "record %D has an invalid length %d",
                pbk->rm_bkhdr.bk_dbkey + recnum, (int)reclen);
            continue;
        }
        prec += sizeof(COUNT);
        if( reclen <= (int)precordDesc->maxLength )
        {
          bufcop(precordDesc->pbuffer,prec,reclen);
          precordDesc->recid = pbk->rm_bkhdr.bk_dbkey + recnum;
        }
        else
        {
            rc = DSM_S_RECORD_BUFFER_TOO_SMALL;
        }
        precordDesc->recLength = reclen;
        return rc;  
      }
    }   /* End of rm block processing loop  */
    /* If we get here it means we didn't find a record in the block */
    return 1;
}

LOCALF
dsmStatus_t
rowLockIt(dsmContext_t *pcontext, dsmArea_t areaNumber,
          dsmTable_t table, dsmRecid_t recid, int lockMode,
          bmBufHandle_t bufHandle, int *pretry)
{
  dsmStatus_t   rc;
  lockId_t      lockId;

  lockId.dbkey = recid;
  *pretry = 0;

  if( lockMode == DSM_LK_NOLOCK )
  {
    lockId.table = areaNumber;
    rc = lkrglock(pcontext,&lockId, DSM_LK_SHARE);
    bmReleaseBuffer(pcontext,bufHandle);
  }
  else
  {
    lockId.table = table;
    rc = lklocky(pcontext, &lockId, lockMode, LK_RECLK, NULL, NULL);
    bmReleaseBuffer(pcontext,bufHandle);
    if( rc == DSM_S_RQSTQUED)
    {
      /* Wait to get record or for lock timeout  */
      rc = lkwait(pcontext, LW_RECLK_MSG, NULL);
      if(!rc)
	*pretry = 1;
    }
  }

  return rc;
}

/* PROGRAM: dbTableAutoIncrement - Increments and return the table sequence value
 *
 */
dsmStatus_t
dbTableAutoIncrement(dsmContext_t *pcontext, dsmTable_t tableNumber,
                      ULONG64 *pvalue,int update)
{
    dsmArea_t   areaNumber;
    dsmStatus_t returnCode;
    bmBufHandle_t  objHandle;
    objectBlock_t  *pobjectBlock;
    dbAutoIncrementNote_t  note;
 
    if(SCTBL_IS_TEMP_OBJECT(tableNumber))
    {
      omTempObject_t  *ptemp;
      ptemp = omFetchTempPtr(pcontext,DSMOBJECT_MIXTABLE,
			     tableNumber,tableNumber);
      if(update)
	ptemp->autoIncrement++;

      *pvalue = ptemp->autoIncrement;
      return 0;
    }
    returnCode = omIdToArea(pcontext,DSMOBJECT_MIXTABLE,
                            tableNumber,tableNumber,&areaNumber);
    if(returnCode)
        goto done;

    rlTXElock (pcontext, RL_TXE_SHARE, RL_MISC_OP);

    objHandle = bmLocateBlock(pcontext, areaNumber,
                       BLK2DBKEY(BKGET_AREARECBITS(areaNumber)), TOMODIFY);
    pobjectBlock = (objectBlock_t *)bmGetBlockPointer(pcontext,objHandle);

    if(update)
    {
      INITNOTE(note,RL_AUTO_INCREMENT, RL_PHY);
      rlLogAndDo(pcontext,(RLNOTE *)&note,objHandle,0,PNULL);
    }
    *pvalue = pobjectBlock->autoIncrement;
    bmReleaseBuffer(pcontext,objHandle);
    rlTXEunlock (pcontext);
done:
    return returnCode;

}

/* PROGRAM: dbTableAutoIncrementDo - Do routine for incrementing the 
                                     table sequence number.
*/
DSMVOID
dbTableAutoIncrementDo(dsmContext_t *pcontext _UNUSED_,
                       RLNOTE *pnote _UNUSED_,
                       BKBUF *pblk,
                       COUNT  dlen _UNUSED_,
                       TEXT *pdata _UNUSED_)
{
  ((objectBlock_t *)pblk)->autoIncrement++;
  return;
}

/* PROGRAM: dbTableAutoIncrementUndo - Undo routine for incrementing the 
                                     table sequence number.
*/
DSMVOID
dbTableAutoIncrementUndo(dsmContext_t *pcontext _UNUSED_,
                         RLNOTE *pnote _UNUSED_,
                         BKBUF *pblk,
                         COUNT  dlen _UNUSED_,
                         TEXT *pdata _UNUSED_)
{
  ((objectBlock_t *)pblk)->autoIncrement--;
  return;
}
/* PROGRAM: dbTableAutoIncrementSet - Sets the table sequence value
 *
 */
dsmStatus_t
dbTableAutoIncrementSet(dsmContext_t *pcontext, dsmTable_t tableNumber,
                      ULONG64 value)
{
    dsmArea_t   areaNumber;
    dsmStatus_t returnCode;
    bmBufHandle_t  objHandle;
    dbAutoIncrementSetNote_t  note;

    if(SCTBL_IS_TEMP_OBJECT(tableNumber))
    {
      omTempObject_t  *ptemp;
      ptemp = omFetchTempPtr(pcontext,DSMOBJECT_MIXTABLE,
			     tableNumber,tableNumber);
      ptemp->autoIncrement = value;
      return 0;
    }
    returnCode = omIdToArea(pcontext,DSMOBJECT_MIXTABLE,
                            tableNumber,tableNumber,&areaNumber);
    if(returnCode)
        goto done;

    rlTXElock (pcontext, RL_TXE_SHARE, RL_MISC_OP);

    objHandle = bmLocateBlock(pcontext, areaNumber,
                       BLK2DBKEY(BKGET_AREARECBITS(areaNumber)), TOMODIFY);

    INITNOTE(note,RL_AUTO_INCREMENT_SET, RL_PHY);
    note.value = value;
    rlLogAndDo(pcontext,(RLNOTE *)&note,objHandle,0,PNULL);
    bmReleaseBuffer(pcontext,objHandle);
    rlTXEunlock(pcontext);
done:
    return returnCode;
}

/* PROGRAM: dbTableAutoIncrementDo - Do routine for incrementing the
                                     table sequence number.
*/
DSMVOID
dbTableAutoIncrementSetDo(dsmContext_t *pcontext _UNUSED_,
                          RLNOTE *pnote,
                          BKBUF *pblk,
                          COUNT  dlen _UNUSED_,
                          TEXT *pdata _UNUSED_)
{
  ((objectBlock_t *)pblk)->autoIncrement =  ((dbAutoIncrementSetNote_t *)pnote)->value;
  return;
}

/* PROGRAM: dbTableReset */

dsmStatus_t
dbTableReset(dsmContext_t *pcontext, dsmTable_t tableNumber, int numIndexes,
               dsmIndex_t   *pindexes)
{
   dsmStatus_t   rc = 0;
   objectBlock_t *ptableObjectBlock;
   bmBufHandle_t  tableObjectHandle;
   dsmArea_t      areaNumber;
   bkAreaDesc_t    *pbkAreaDesc;
   dsmArea_t  indexArea;

   /* Mark the table area as being in repair.    */
   rc = omIdToArea(pcontext,DSMOBJECT_MIXTABLE,tableNumber,
                   tableNumber,&areaNumber);
   tableObjectHandle = bmLocateBlock(pcontext, areaNumber,
                         BLK2DBKEY(BKGET_AREARECBITS(areaNumber)), TOMODIFY);
   ptableObjectBlock = (objectBlock_t *)bmGetBlockPointer(pcontext,
                                                          tableObjectHandle);

   ptableObjectBlock->objStatus = DSM_OBJECT_IN_REPAIR;

   bmMarkModified(pcontext, tableObjectHandle,0);

   /* Issue a synchronous checkpoint             */
   rlSynchronousCP(pcontext);
   /* Re-set the index area hi-water mark.       */
   if(numIndexes)
   {
       bmBufHandle_t  indexObjectHandle;
       objectBlock_t *pindexObjectBlock;

       rc = omIdToArea(pcontext,DSMOBJECT_MIXINDEX,pindexes[0],
                       tableNumber, &indexArea); 
       if (rc)
       {
           MSGD_CALLBACK(pcontext, "Unable to find area for index %d on table %d, error %d", pindexes[0], tableNumber, rc);
           bmReleaseBuffer(pcontext,tableObjectHandle);
           return (rc);
       }
       indexObjectHandle = bmLocateBlock(pcontext, indexArea,
                         BLK2DBKEY(BKGET_AREARECBITS(indexArea)), TOMODIFY);

          pindexObjectBlock = (objectBlock_t *)bmGetBlockPointer(pcontext,
                                                           indexObjectHandle);
       pindexObjectBlock->hiWaterBlock = 3;
       stnclr(pindexObjectBlock->chainFirst,
              (TEXT *)&pindexObjectBlock->numBlocksOnChain[LOCKCHN+1] - 
              (TEXT *)pindexObjectBlock->chainFirst);
       pbkAreaDesc = BK_GET_AREA_DESC(pcontext->pdbcontext, indexArea);
       pbkAreaDesc->bkHiWaterBlock = 3;
       pbkAreaDesc->bkChainFirst[0] = 0;
       pbkAreaDesc->bkChainFirst[1] = 0;
       pbkAreaDesc->bkChainFirst[2] = 0;
       pbkAreaDesc->bkNumBlocksOnChain[0] = 0;
       pbkAreaDesc->bkNumBlocksOnChain[1] = 0;
       pbkAreaDesc->bkNumBlocksOnChain[2] = 0;
       bmReleaseBuffer(pcontext,indexObjectHandle);
   }

   /* Unlink the rm chain in the table area and
      zero out the total rows count    */
   ptableObjectBlock->totalRows = 0;
   ptableObjectBlock->chainFirst[RMCHN] = 0;
   ptableObjectBlock->chainLast[RMCHN] = 0;
   ptableObjectBlock->numBlocksOnChain[RMCHN] = 0;

   pbkAreaDesc = BK_GET_AREA_DESC(pcontext->pdbcontext, areaNumber);
   pbkAreaDesc->bkChainFirst[RMCHN] = 0;
   pbkAreaDesc->bkNumBlocksOnChain[RMCHN] = 0;
   bmMarkModified(pcontext, tableObjectHandle, 0);
   bmReleaseBuffer(pcontext,tableObjectHandle);

   if (numIndexes)
   {
     /* For each index re-create the root block    */
     rc = createIndexRoots(pcontext, indexArea, numIndexes, tableNumber, pindexes);
   }

   return rc;
} 
/* PROGRAM: createIndexRoots - Re-create root blocks for the input 
                                 indexes.   
*/
LOCALF
dsmStatus_t
createIndexRoots(dsmContext_t *pcontext, dsmArea_t indexArea,
                               int numIndexes,
                               dsmTable_t    tableNum,
                               dsmIndex_t  *pindexes)
{
    dsmStatus_t   rc = 0;
    DBKEY         rootDbkey;
    int           i;

    for(i = 0; i < numIndexes; i++)
    {
        rc = omDelete(pcontext,DSMOBJECT_MIXINDEX,tableNum,pindexes[i]);
        rc = cxCreateIndex(pcontext,0,pindexes[i],indexArea,tableNum,
                           &rootDbkey);
        rc = updateObjectRoot(pcontext,pindexes[i],tableNum,rootDbkey);
    }
    return rc;
}

LOCALF
dsmStatus_t
updateObjectRoot(dsmContext_t *pcontext, dsmIndex_t indexNumber,
                 dsmObject_t    tableNum,
                 DBKEY   rootDbkey)
{
    dsmStatus_t   rc;
    DBKEY         rowid;
    int           recordSize;
    TEXT          recordBuffer[1024];
    dsmArea_t     objectArea;
    ULONG         indicator;
    TEXT          namebuf[DSM_OBJECT_NAME_LEN];
    svcByteString_t  fieldValue;

    fieldValue.pbyte = namebuf;
    fieldValue.size = sizeof(namebuf);
    fieldValue.length = sizeof(namebuf);

    rc = omGetObjectRecordId(pcontext, DSMOBJECT_MIXINDEX,
                            indexNumber, tableNum, &rowid);
  
    recordSize = rmFetchRecord(pcontext, DSMAREA_SCHEMA, rowid, 
                               recordBuffer, sizeof(recordBuffer),0);

    rc = recGetLONG(recordBuffer, SCFLD_OBJAREA,(ULONG)0, 
                     &objectArea, &indicator);

    rc = recGetBYTES(recordBuffer,SCFLD_OBJNAME,0,
                            &fieldValue,&indicator);

    rc = recPutBIG(recordBuffer, sizeof(recordBuffer),
               (ULONG *)&recordSize, SCFLD_OBJROOT, 0, rootDbkey,0);

    rc = rmUpdateRecord(pcontext,SCTBL_OBJECT,DSMAREA_SCHEMA,
                        rowid,recordBuffer, (COUNT)recordSize); 

    rc = dbIndexAnchor(pcontext, objectArea, namebuf, rootDbkey, namebuf, IX_ANCHOR_UPDATE);
    return rc;
}
 
