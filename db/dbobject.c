/* Copyright (C) 2001 NuSphere Corporation, All Rights Reserved.

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
 
#include "bkpub.h"      /* for bkfilc() */
#include "dbcode.h"
#include "dbcontext.h"
#include "dbpub.h"
#include "dbprv.h"
#include "dsmpub.h"
#include "kypub.h"
#include "keypub.h"
#include "lkpub.h"
#include "lkschma.h"
#include "mstrblk.h"    /* for mb_objectRoot */
#include "ompub.h"      /* for omcreate(), omdelete() */
#include "omprv.h"      /* for omcreate(), omdelete() */
#include "recpub.h"
#include "rmpub.h"
#include "scasa.h"
#include "stmpub.h"     /* storage management subsystem */
#include "stmprv.h"
#include "svcspub.h"
#include "umlkwait.h"
#include "utspub.h"
#include "usrctl.h"
#include "cxpub.h"

#define KEYBUFSIZE 100
#if 0

/* PROGRAM: dbObjectUpdate - Action routine for dsm layer to call to modify
 *                           the object by obtaining an exclusive record
 *                           lock and calling the appropriate object updater.  
 *
 *
 * RETURNS: DSM_S_SUCCESS unqualified success
 *          DSM_S_NO_TRANSACTION Caller must have started a tx.  
 *          DSM_S_INVALID_FIELD_VALUE update field value is invalid
 *          DSM_S_INVALID_TABLE_NUMBER invalid table number for move.
 *          DSM_S_INVALID_AREA_NUMBER invalid area number for move.
 *          DSM_S_INVALID_INDEX_NUMBER invalid index number for move.
 *          DSM_S_FAILURE on failure
 */
dsmStatus_t 
dbObjectUpdateRec(
            dsmContext_t       *pcontext,     /* IN database context */
            dsmObject_t	       objectNumber,  /* IN  for object id */
            dsmObjectType_t    objectType,    /* IN object type */
            omDescriptor_t     *pobjectDesc,  /* IN object descriptors */
            dsmMask_t          updateMask,     /* IN bit mask identifying which
                                                  storage object attributes are
                                                  to be modified. */
            dsmIndexList_t     *pindexList,    /* IN array of index numbers */
            COUNT              indexTotal,     /* IN num of indices in array */
            dsmArea_t          indexArea)      /* IN table move index area */
{
    dsmStatus_t    returnCode;
    dsmRecid_t     recordId;
    lockId_t       lockId;
    int            recordSize;
    TEXT           recordBuffer[1024];
    TEXT           *ptableName = (TEXT*)"_storageObject";
    objectNote_t   objectNote;
    DBKEY          newRoot;
    

    if( updateMask & DSM_OBJECT_UPDATE_INDEX_AREA )
    {
        pcontext->pusrctl->uc_state = 12;
        if ( lkAdminLock( pcontext, objectNumber,
                     0, DSM_LK_EXCL ) != DSM_S_SUCCESS)
        {
             returnCode = DSM_S_FAILURE;
             goto done;
        }
    }


    /* Read the storage object record from disk            */
    returnCode = omGetObjectRecordId(pcontext, objectType,
                                     objectNumber, SCTBL_OBJECT, &recordId);

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

    /* Modify the appropriate fields of the record.        */
    if( updateMask & DSM_OBJECT_UPDATE_ASSOCIATE )
    {
        /* set _Object-associate */
        if (recPutSMALL(recordBuffer, MAXRECSZ, (ULONG *)&recordSize,
                       SCFLD_OBJASSOC, (ULONG)0, 
                       pobjectDesc->objectAssociate, 0) )
        {
            returnCode = DSM_S_FAILURE;
            goto doneButUnlockRecord;
        }
    }
    if( updateMask & DSM_OBJECT_UPDATE_ASSOCIATE_TYPE)
    {
        if (recPutLONG(recordBuffer, MAXRECSZ, (ULONG *)&recordSize,
                       SCFLD_OBJATYPE, (ULONG)0, 
                       (ULONG)pobjectDesc->objectAssociateType, 0) )
        {
            returnCode = DSM_S_FAILURE;
            goto doneButUnlockRecord;
        }
    }
    
    if( updateMask & DSM_OBJECT_UPDATE_INDEX_AREA )
    {
        if( !dbValidArea(pcontext, pobjectDesc->area) )
        {
              returnCode = DSM_S_INVALID_FIELD_VALUE;
              goto doneButUnlockRecord;
        }
 
        if ((returnCode =
             dbIndexMove(pcontext, objectNumber, pobjectDesc->area, &newRoot)) 
              != DSM_S_SUCCESS)
              goto doneButUnlockRecord;

        if (recPutLONG(recordBuffer, MAXRECSZ, (ULONG *)&recordSize,
                       SCFLD_OBJAREA,(ULONG)0, pobjectDesc->area, 0))
              goto doneButUnlockRecord;

        if (recPutBIG(recordBuffer, MAXRECSZ, (ULONG *)&recordSize,
                       SCFLD_OBJROOT,(ULONG)0, newRoot, 0))
              goto doneButUnlockRecord;

    }

    /* write a logical note stating that we are updating an object.
     * The undo operation is an omDelete.
     */
    INITNOTE( objectNote, RL_OBJECT_UPDATE, RL_LOGBI );
    INIT_S_OBJECTNOTE_FILLERS( objectNote );  

    /* fill in note information */
    objectNote.objectType   = objectType;
    objectNote.objectNumber = objectNumber;
    objectNote.areaNumber   = pobjectDesc->area;
    rlLogAndDo(pcontext, (RLNOTE *)&objectNote, 0, (COUNT)0, PNULL );

    /* Write the record.                                    */
    returnCode = rmUpdateRecord(pcontext, SCTBL_OBJECT,
                                DSMAREA_SCHEMA, recordId,
                                recordBuffer, (COUNT)recordSize);
    if( returnCode )
        goto doneButUnlockRecord;
    
    /* Remove the old entry from the cache.                 */
    returnCode = omDelete(pcontext, objectType, objectNumber);
    
doneButUnlockRecord:
    lkrels(pcontext,&lockId);

done:


    if( updateMask & DSM_OBJECT_UPDATE_INDEX_AREA )
    {
        if ( lkAdminUnlock( pcontext, objectNumber, 0) != DSM_S_SUCCESS)
             returnCode=DSM_S_FAILURE;
    }

    return (returnCode);

}  /* end dbObjectUpdateRec */
#endif

/* PROGRAM: dbObjectNameToNum  - Given a name return the number it maps to
 *
 *
 * RETURNS: DSM_S_SUCCESS success
 *          DSM_S_FAILURE on failure
 */
dsmStatus_t 
dbObjectNameToNum(
            dsmContext_t       *pcontext,     /* IN database context */
            dsmText_t          *psname,       /* IN object name */
            svcLong_t	       *pnum)         /* OUT object number */
{
    dsmStatus_t     rc = 0;
    dsmDbkey_t      objectBlock;
    TEXT            record[1024];
    dsmRecord_t     recordDesc;
    dsmCursid_t      cursorId = DSMCURSOR_INVALID;
    svcDataType_t   component[2];
    svcDataType_t   *pcomponent[2];
    ULONG           indicator;

    TEXT            namebuf[DSM_OBJECT_NAME_LEN];
    TEXT           *pname = namebuf;
    svcByteString_t fieldValue;

    dsmObjectType_t   objType;
#ifdef STAND_ALONE_TABLE
    DBKEY             root;
    dsmArea_t         objArea;
#endif

    COUNT            maxKeyLength = 64; /* SHOULD BE A CONSTANT! */
    AUTOKEY(objKeyLow, 64)
    AUTOKEY(objKeyHi, 64)
    
    objectBlock = 0;
    
    recordDesc.table = SCTBL_OBJECT;
    recordDesc.pbuffer = record;
    recordDesc.maxLength = sizeof(record);

    fieldValue.pbyte = pname;
    fieldValue.size = sizeof(namebuf);
    fieldValue.length = sizeof(namebuf);

    /* Read all the records */

 
 
    /* 1. look at all _storage objects and enable the flag in the areaTable if
     *    one exists.
     * 2. traverse the area descriptors.
     * 3. For each valid area Descriptor, if
     *    then areaTable[area] is not enabled then it is OK to update
     *    its high water mark.
     */
 
    /* set up keys to define bracket for scan of the area table */
    objKeyLow.akey.area         = DSMAREA_SCHEMA;
    objKeyHi.akey.area          = DSMAREA_SCHEMA;
    objKeyLow.akey.root       = pcontext->pdbcontext->pmstrblk->mb_objectRoot;
    objKeyHi.akey.root        = pcontext->pdbcontext->pmstrblk->mb_objectRoot;
    objKeyLow.akey.unique_index = 1;
    objKeyHi.akey.unique_index  = 1;
    objKeyLow.akey.word_index   = 0;
    objKeyHi.akey.word_index    = 0;
 
    /* create bracket */
    component[0].indicator = SVCLOWRANGE;
    pcomponent[0]          = &component[0];
 
    objKeyLow.akey.index    = SCIDX_OBJID;
    objKeyLow.akey.keycomps = 1;
    objKeyLow.akey.ksubstr  = 0;
 
    rc = keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength, &objKeyLow.akey);
    if (rc)
        goto badreturn;
 
    /* high range consists of table num followed by ANY filed position value */
    component[0].indicator = SVCHIGHRANGE;
    pcomponent[0]          = &component[0];
 
    objKeyHi.akey.index    = SCIDX_OBJID;
    objKeyHi.akey.keycomps = 1;
    objKeyHi.akey.ksubstr  = 0;
 
    rc = keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength, &objKeyHi.akey);
    if (rc)
        goto badreturn;
 
    /* find area number of each _storageObject record. */
    rc = (dsmStatus_t)cxFind(pcontext,
                               &objKeyLow.akey, &objKeyHi.akey,
                               &cursorId, SCTBL_OBJECT,
                               &recordDesc.recid,
                               DSMPARTIAL, DSMFINDFIRST,
                               (dsmMask_t)DSM_LK_NOLOCK);
    if (rc)
        goto badreturn;
 
    while (!rc)
    {
        /* Get the _StorageObject record */
        recordDesc.recLength = rmFetchRecord(pcontext, DSMAREA_SCHEMA,
                                    recordDesc.recid, recordDesc.pbuffer,
                                    MAXRECSZ, 0);
 
        /* get the name field */
        rc = recGetBYTES(recordDesc.pbuffer,SCFLD_OBJNAME,0,
                            &fieldValue,&indicator);
        if (rc != 0)
        {
            goto badreturn;
        }

        /* Check to see if this is the one we want */
        if (stpcmp(psname, fieldValue.pbyte) == 0)
        {
            /* get the object-number field */
            rc = recGetLONG(recordDesc.pbuffer,SCFLD_OBJNUM,0,
                                pnum,&indicator);
            if (rc != 0)
            {
                goto badreturn;
            }

            /* get the object type to see if it is an index */
            rc = recGetLONG(recordDesc.pbuffer,SCFLD_OBJTYPE,0,
                                (svcLong_t *)&objType,&indicator);
            if (rc != 0)
            {
                goto badreturn;
            }

#ifdef STAND_ALONE_TABLE
            if (objType == DSMOBJECT_MIXINDEX)
            {
                /* get the area, we only store index roots for
                   user areas */
                rc = recGetLONG(recordDesc.pbuffer,SCFLD_OBJAREA,0,
                                    (svcLong_t *)&objArea,&indicator);
                if (rc != 0)
                {
                    goto badreturn;
                }

                if (objArea < DSMAREA_DEFAULT)
                {
                    goto done;
                }

                rc = recGetBIG(recordDesc.pbuffer,SCFLD_OBJROOT,0,
                                    &root,&indicator);
                if (rc != 0)
                {
                    goto badreturn;
                }

                /* verify that the root and the table numbers have
                   not changed.  If they have changed, then update
                   the object record to reflect the change. */

                rc = cxVerifyIndexRoots(pcontext, pname, &root, pnum, objArea);

                if (rc < 0)
                {
                    goto badreturn;
                }

                if (rc > 0)
                {
                    /* The number or root have changed.  Update the
                       object record and put it back into the database */

                    rc = recPutBIG(recordDesc.pbuffer, recordDesc.maxLength, 
                              &recordDesc.recLength, SCFLD_OBJROOT, 
                              (ULONG)0, root, 0);
                    if (rc != 0)
                    {
                        goto badreturn;
                    }
                    rc = recPutLONG(recordDesc.pbuffer, recordDesc.maxLength, 
                              &recordDesc.recLength, SCFLD_OBJNUM, 
                              (ULONG)0, (LONG)*pnum, 0);
                    if (rc != 0)
                    {
                        goto badreturn;
                    }

                   rc = rmUpdateRecord (pcontext, SCTBL_OBJECT, DSMAREA_SCHEMA,
                          recordDesc.recid, recordDesc.pbuffer, recordDesc.recLength);
                    if (rc != 0)
                    {
                        goto badreturn;
                    }
                }
            }
#endif
            goto done;
        }
 
        /* find next entry */
         rc = cxNext(pcontext,
                             &cursorId, SCTBL_OBJECT,
                             &recordDesc.recid,
                             &objKeyLow.akey, &objKeyHi.akey,
                             DSMPARTIAL, DSMFINDNEXT,
                             (dsmMask_t)DSM_LK_NOLOCK);
    }
done:
    cxDeactivateCursor(pcontext, &cursorId);
    if( rc != 0 )
    {
        goto badreturn;
    }
    return 0;
 
badreturn:
    return rc;

}

/* PROGRAM: dbObjectDeleteAssociate - Remove all the storage objects that are
 *          associated with the passed object.  
 *
 * RETURNS: DSM_SUCCESS
            -1 on Failure
 */
dsmStatus_t
dbObjectDeleteAssociate(dsmContext_t    *pcontext,   /* IN database context */
	                dsmObject_t      tableNum,   /* IN table name */
                        dsmArea_t       *indexArea)  /* OUT index area */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    mstrblk_t       *pmstrblk = pdbcontext->pmstrblk;
    dsmObject_t      objectAssociate;
    dsmObject_t      objectNumber;
    dsmObjectType_t  objectType;
    CURSID           cursorId = NULLCURSID;
    dsmRecid_t       indexRecordId;
    TEXT            *precordBuffer;
    LONG             bufsize;
    LONG             recordSize;
    COUNT            maxKeyLength = DB_KEYSIZE;    
    ULONG            indicator;
    dsmStatus_t      rc;
    TEXT             namebuf[DSM_OBJECT_NAME_LEN];
    svcByteString_t  fieldValue;

    svcDataType_t component[2];
    svcDataType_t *pcomponent[2];

    AUTOKEY(objectKeyLow, DB_KEYSIZE)
    AUTOKEY(objectKeyHi, DB_KEYSIZE)

    if( SCTBL_IS_TEMP_OBJECT(tableNum) )
      return omDeleteTemp(pcontext, tableNum);

    fieldValue.pbyte = namebuf;
    fieldValue.size = sizeof(namebuf);
    fieldValue.length = sizeof(namebuf);

    /* Create a key bracket to retrieve all the storage object 
       records that belong to our table.                       */
    /* set up key  for the find */
    objectKeyLow.akey.area         = DSMAREA_SCHEMA;
    objectKeyHi.akey.area          = DSMAREA_SCHEMA;
    objectKeyLow.akey.root         = pmstrblk->mb_objectRoot;
    objectKeyHi.akey.root          = pmstrblk->mb_objectRoot;
    objectKeyLow.akey.unique_index = 1;
    objectKeyHi.akey.unique_index  = 1;
    objectKeyLow.akey.word_index   = 0;
    objectKeyHi.akey.word_index    = 0;

    objectKeyLow.akey.index       = SCIDX_OBJID;
    objectKeyLow.akey.keycomps    = 1;
    objectKeyLow.akey.ksubstr     = 0;

    /* First component - _Object._Db-recid */
    component[0].type            = SVCLONG;
    component[0].indicator       = SVCLOWRANGE;

    pcomponent[0] = &component[0];
 
    if (keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength, &objectKeyLow.akey))
        return DSM_S_FAILURE;
 
    objectKeyHi.akey.index           = SCIDX_OBJID;
    objectKeyHi.akey.ksubstr         = 0;
    objectKeyHi.akey.keycomps        = 1;

    /* Third component - */
    component[0].indicator       = SVCHIGHRANGE;
    pcomponent[0] = &component[0];
 
    if (keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength, &objectKeyHi.akey))
        return DSM_S_FAILURE;

    /* attempt to get first index entry within specified bracket */
    rc = (dsmStatus_t)cxFind (pcontext, &objectKeyLow.akey, &objectKeyHi.akey,
                              &cursorId, SCTBL_OBJECT, &indexRecordId,
                              DSMPARTIAL, DSMFINDFIRST, LKNOLOCK);
    if (rc)
    {
        /* Unable to find indices belonging to this table */
        return(rc);
    }

    /* allocate a record buffer to retrieve index storage object records */
    bufsize = (ULONG)1024;
    precordBuffer = (TEXT *)stRentd(pcontext, pdbcontext->privpool, bufsize);
 
    /* for each storage object find the one that matches our
       table name and delete it */
    while (rc == 0)
    {
        /* extract the object type from the storage object record */
        recordSize = rmFetchRecord(pcontext, DSMAREA_SCHEMA,
                                   indexRecordId, precordBuffer,
                                   (COUNT)bufsize,0);

        if (recordSize < 0) /* record fetch error */
        {
            rc = DSM_S_FAILURE;
            goto done;
        }

        if (recordSize > bufsize)
        {
            /* buffer was too small - Get a bigger one */
            bufsize = recordSize;
            stVacate(pcontext, pdbcontext->privpool, precordBuffer);
            precordBuffer = (TEXT *)stRentd(pcontext, pdbcontext->privpool,
                                           (unsigned)bufsize);
            continue;
        }
        
        /* extract the object associate object id               */
        if (recGetSMALL(precordBuffer, SCFLD_OBJASSOC,(ULONG)0,
                   &objectAssociate, &indicator))
            goto done;

        /* get the name field */
        rc = recGetBYTES(precordBuffer,SCFLD_OBJNAME,0,
                            &fieldValue,&indicator);
        if (rc != 0)
        {
            goto done;
        }

        if (objectAssociate == tableNum)
        {

            /* check to see if this record is for an index.  If it is
               then get the index area */
            /* get the object type */
            if (recGetLONG(precordBuffer, SCFLD_OBJTYPE,(ULONG)0,
                   &objectType, &indicator))
                goto done;

            /* get the object number */
            if (recGetSMALL(precordBuffer, SCFLD_OBJNUM,(ULONG)0,
                   &objectNumber, &indicator))
                goto done;

            if (objectType == DSMOBJECT_MIXINDEX)
            {
                rc = omIdToArea(pcontext, objectType, objectNumber, tableNum, indexArea);
            }

            /* Delete this record */
            rc = dbObjectRecordDelete(pcontext, objectNumber, objectType, 
                                     tableNum);
            if (rc)
            {
                MSGD_CALLBACK(pcontext, 
                    "Unable to delete object record num %d, type %d, ret %d",
                         objectNumber, objectType, rc);
                goto done;
            }
            rc = omDelete(pcontext, objectType, tableNum, objectNumber);
            if (rc)
            {
                MSGD_CALLBACK(pcontext, 
                "Unable to delete object record in object cache %d, ret %d",
                         indexRecordId, rc);
                goto done;
            }

           if (objectType == DSMOBJECT_MIXTABLE)
               MSGD_CALLBACK(pcontext, "Deleting Table %s id %d %L",
                    fieldValue.pbyte, tableNum);
           else if (objectType == DSMOBJECT_MIXINDEX)
               MSGD_CALLBACK(pcontext, "Deleting Index %s id %d on table %d %L",
                    fieldValue.pbyte, objectNumber, tableNum);
        }
 
        /* attempt to get next index entry within specified bracket */
        rc = (dsmStatus_t)cxNext(pcontext, &cursorId, DSMAREA_SCHEMA, 
                                  &indexRecordId, &objectKeyLow.akey, 
                                  &objectKeyHi.akey,
                                  DSMGETTAG, DSMFINDNEXT, LKNOLOCK);
    } 
 
    /* There are no more storage object records for this table */
    if( rc == DSM_S_ENDLOOP )
    {
          rc = 0;
    }
 
done:    
    cxDeactivateCursor(pcontext, &cursorId);
    stVacate(pcontext, pdbcontext->privpool, precordBuffer);
    return(rc);

}

/* PROGRAM: dbObjectRename -  Rename object records for a table (including
 *                            indexes).
 *
 * RETURNS: DSM_SUCCESS
 *          -1 on Failure
 */
dsmStatus_t
dbObjectRename(dsmContext_t *pcontext,  /* IN database context */
               dsmText_t    *pNewName,  /* IN new table name*/
               dsmObject_t   tableNum,  /* IN table number */
               dsmArea_t    *indexArea) /* OUT index area */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    mstrblk_t       *pmstrblk = pdbcontext->pmstrblk;
    dsmObject_t      objectAssociate;
    dsmObject_t      objectNumber;
    dsmObjectType_t  objectType;
    svcByteString_t  fieldValue;
    TEXT             namebuf[DSM_OBJECT_NAME_LEN];
    TEXT             idxnamebuf[DSM_OBJECT_NAME_LEN];
    TEXT             oldname[DSM_OBJECT_NAME_LEN];
    TEXT            *pname = namebuf;
    CURSID           cursorId = NULLCURSID;
    dsmRecid_t       indexRecordId;
    TEXT             *precordBuffer;
    LONG            bufsize;
    LONG             recordSize;
    COUNT            maxKeyLength = DB_KEYSIZE;    
    ULONG            indicator;
    dsmStatus_t      rc;

    svcDataType_t component[2];
    svcDataType_t *pcomponent[2];

    AUTOKEY(objectKeyLow, DB_KEYSIZE)
    AUTOKEY(objectKeyHi, DB_KEYSIZE)

    /* Create a key bracket to retrieve all the storage object 
       records that belong to our table.                       */
    /* set up key  for the find */
    objectKeyLow.akey.area         = DSMAREA_SCHEMA;
    objectKeyHi.akey.area          = DSMAREA_SCHEMA;
    objectKeyLow.akey.root         = pmstrblk->mb_objectRoot;
    objectKeyHi.akey.root          = pmstrblk->mb_objectRoot;
    objectKeyLow.akey.unique_index = 1;
    objectKeyHi.akey.unique_index  = 1;
    objectKeyLow.akey.word_index   = 0;
    objectKeyHi.akey.word_index    = 0;

    objectKeyLow.akey.index       = SCIDX_OBJID;
    objectKeyLow.akey.keycomps    = 1;
    objectKeyLow.akey.ksubstr     = 0;

    /* First component - _Object._Db-recid */
    component[0].type            = SVCLONG;
    component[0].indicator       = SVCLOWRANGE;

    pcomponent[0] = &component[0];
 
    if (keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength, &objectKeyLow.akey))
        return DSM_S_FAILURE;
 
    objectKeyHi.akey.index           = SCIDX_OBJID;
    objectKeyHi.akey.ksubstr         = 0;
    objectKeyHi.akey.keycomps        = 1;

    /* Third component - */
    component[0].indicator       = SVCHIGHRANGE;
    pcomponent[0] = &component[0];
 
    if (keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength, &objectKeyHi.akey))
        return DSM_S_FAILURE;

    /* attempt to get first index entry within specified bracket */
    rc = (dsmStatus_t)cxFind (pcontext, &objectKeyLow.akey, &objectKeyHi.akey,
                              &cursorId, SCTBL_OBJECT, &indexRecordId,
                              DSMPARTIAL, DSMFINDFIRST, LKNOLOCK);
    if (rc)
    {
        /* Unable to find indices belonging to this table */
        return(rc);
    }

    /* allocate a record buffer to retrieve index storage object records */
    bufsize = (ULONG)1024;
    precordBuffer = (TEXT *)stRentd(pcontext, pdbcontext->privpool, bufsize);
 
    /* for each storage object find the one that matches our
       table name and delete it */
    while (rc == 0)
    {
        /* extract the object type from the storage object record */
        recordSize = rmFetchRecord(pcontext, DSMAREA_SCHEMA,
                                   indexRecordId, precordBuffer,
                                   (COUNT)bufsize,0);

        if (recordSize < 0) /* record fetch error */
        {
            rc = DSM_S_FAILURE;
            goto done;
        }

        if (recordSize > bufsize)
        {
            /* buffer was too small - Get a bigger one */
            bufsize = recordSize;
            stVacate(pcontext, pdbcontext->privpool, precordBuffer);
            precordBuffer = (TEXT *)stRentd(pcontext, pdbcontext->privpool,
                                           (unsigned)bufsize);
            continue;
        }
        
        /* extract the object associate object id               */
        if (recGetSMALL(precordBuffer, SCFLD_OBJASSOC,(ULONG)0,
                   &objectAssociate, &indicator))
            goto done;

        if (objectAssociate == tableNum)
        {
            /* check to see if this record is for an index.  If it is
               then get the index area */
            /* get the object type */
            if (recGetLONG(precordBuffer, SCFLD_OBJTYPE,(ULONG)0,
                   &objectType, &indicator))
                goto done;

            /* get the object number */
            if (recGetSMALL(precordBuffer, SCFLD_OBJNUM,(ULONG)0,
                   &objectNumber, &indicator))
                goto done;

            /* get the name field */
            fieldValue.pbyte = pname;
            fieldValue.size = sizeof(namebuf);
            fieldValue.length = sizeof(namebuf);

            rc = recGetBYTES(precordBuffer,SCFLD_OBJNAME,(ULONG)0,
                                &fieldValue,&indicator);
            if (rc != 0)
            {
                goto done;
            }

            stnclr(oldname, 80);
            stcopy(oldname, fieldValue.pbyte);

            if (objectType == DSMOBJECT_MIXINDEX)
            {
                TEXT *p;

                /* Index objects are named "database.table.indexname" -
                ** build the name from the new table name and the indexname
                */
                p = fieldValue.pbyte + chsidx((TEXT *)".", fieldValue.pbyte);
                p += chsidx((TEXT *)".", p);
                sprintf((char *)idxnamebuf, "%s.%s", pNewName, p);
                fieldValue.pbyte = idxnamebuf;
                fieldValue.size = sizeof(idxnamebuf);
                fieldValue.length = sizeof(idxnamebuf);
                rc = omIdToArea(pcontext, objectType, objectNumber, 
                                tableNum, indexArea);

                rc = dbIndexAnchor(pcontext, *indexArea, 
                                   (dsmText_t *)oldname, 0,
                                   (dsmText_t *)idxnamebuf,
                                   IX_ANCHOR_UPDATE);
            }
            else
            {
                /* Table object - just use the new table name */
                fieldValue.pbyte = pNewName;
                fieldValue.length = strlen((const char *)pNewName);
                fieldValue.size = fieldValue.length;
            }
            /* Store the new name field */
            rc = recPutBYTES(precordBuffer, MAXRECSZ, &recordSize,
                             SCFLD_OBJNAME,0, &fieldValue, 0);
            if (rc)
            {
                goto done;
            }
            /* Write the record. */
            rc = rmUpdateRecord(pcontext, SCTBL_OBJECT,
                                DSMAREA_SCHEMA, indexRecordId,
                                precordBuffer, recordSize);
            if (rc)
            {
                MSGD_CALLBACK(pcontext, 
                    "Unable to rename object record num %d, type %d, ret %d",
                         objectNumber, objectType, rc);
                goto done;
            }

            if (objectType == DSMOBJECT_MIXTABLE)
                MSGD_CALLBACK(pcontext, "Updated Table %s id %d with name %s%L",
                     oldname, tableNum, pNewName);
            else if (objectType == DSMOBJECT_MIXINDEX)
                MSGD_CALLBACK(pcontext, 
                    "Updated Index %s id %d on table id %d with name %s%L",
                     oldname, tableNum, objectAssociate, idxnamebuf);
        }
        /* attempt to get next index entry within specified bracket */
        rc = (dsmStatus_t)cxNext(pcontext, &cursorId, DSMAREA_SCHEMA, 
                                  &indexRecordId, &objectKeyLow.akey, 
                                  &objectKeyHi.akey,
                                  DSMGETTAG, DSMFINDNEXT, LKNOLOCK);
    } 
 
    /* There are no more storage object records for this table */
    if( rc == DSM_S_ENDLOOP )
    {
          rc = 0;
    }
 
done:    
    cxDeactivateCursor(pcontext, &cursorId);
    stVacate(pcontext, pdbcontext->privpool, precordBuffer);
    return(rc);
}

/* PROGRAM: dbObjectDeleteByArea - Remove all the storage objects that are
 *          from an specific area
 *
 * RETURNS: DSM_SUCCESS
            -1 on Failure
 */
dsmStatus_t
dbObjectDeleteByArea(dsmContext_t    *pcontext,   /* IN database context */
                        dsmArea_t    area)        /* IN area  to delete */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    mstrblk_t       *pmstrblk = pdbcontext->pmstrblk;
    dsmArea_t        objectArea;
    dsmObject_t      objectAssociate;
    dsmObject_t      objectNumber;
    dsmObjectType_t  objectType;
    CURSID           cursorId = NULLCURSID;
    dsmRecid_t       indexRecordId;
    TEXT            *precordBuffer;
    LONG             bufsize;
    LONG             recordSize;
    COUNT            maxKeyLength = DB_KEYSIZE;    
    ULONG            indicator;
    dsmStatus_t      rc;
    TEXT             namebuf[DSM_OBJECT_NAME_LEN];
    svcByteString_t  fieldValue;

    svcDataType_t component[2];
    svcDataType_t *pcomponent[2];

    AUTOKEY(objectKeyLow, DB_KEYSIZE)
    AUTOKEY(objectKeyHi, DB_KEYSIZE)

    fieldValue.pbyte = namebuf;
    fieldValue.size = sizeof(namebuf);
    fieldValue.length = sizeof(namebuf);

    /* Create a key bracket to retrieve all the storage object 
       records that belong to our table.                       */
    /* set up key  for the find */
    objectKeyLow.akey.area         = DSMAREA_SCHEMA;
    objectKeyHi.akey.area          = DSMAREA_SCHEMA;
    objectKeyLow.akey.root         = pmstrblk->mb_objectRoot;
    objectKeyHi.akey.root          = pmstrblk->mb_objectRoot;
    objectKeyLow.akey.unique_index = 1;
    objectKeyHi.akey.unique_index  = 1;
    objectKeyLow.akey.word_index   = 0;
    objectKeyHi.akey.word_index    = 0;

    objectKeyLow.akey.index       = SCIDX_OBJID;
    objectKeyLow.akey.keycomps    = 1;
    objectKeyLow.akey.ksubstr     = 0;

    /* First component - _Object._Db-recid */
    component[0].type            = SVCLONG;
    component[0].indicator       = SVCLOWRANGE;

    pcomponent[0] = &component[0];
 
    if (keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength, &objectKeyLow.akey))
        return DSM_S_FAILURE;
 
    objectKeyHi.akey.index           = SCIDX_OBJID;
    objectKeyHi.akey.ksubstr         = 0;
    objectKeyHi.akey.keycomps        = 1;

    /* Third component - */
    component[0].indicator       = SVCHIGHRANGE;
    pcomponent[0] = &component[0];
 
    if (keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength, &objectKeyHi.akey))
        return DSM_S_FAILURE;

    /* attempt to get first index entry within specified bracket */
    rc = (dsmStatus_t)cxFind (pcontext, &objectKeyLow.akey, &objectKeyHi.akey,
                              &cursorId, SCTBL_OBJECT, &indexRecordId,
                              DSMPARTIAL, DSMFINDFIRST, LKNOLOCK);
    if (rc)
    {
        /* Unable to find indices belonging to this table */
        return(rc);
    }

    /* allocate a record buffer to retrieve index storage object records */
    bufsize = (ULONG)1024;
    precordBuffer = (TEXT *)stRentd(pcontext, pdbcontext->privpool, bufsize);
 
    /* for each storage object find the one that matches our
       table name and delete it */
    while (rc == 0)
    {
        /* extract the object type from the storage object record */
        recordSize = rmFetchRecord(pcontext, DSMAREA_SCHEMA,
                                   indexRecordId, precordBuffer,
                                   (COUNT)bufsize,0);

        if (recordSize < 0) /* record fetch error */
        {
            rc = DSM_S_FAILURE;
            goto done;
        }

        if (recordSize > bufsize)
        {
            /* buffer was too small - Get a bigger one */
            bufsize = recordSize;
            stVacate(pcontext, pdbcontext->privpool, precordBuffer);
            precordBuffer = (TEXT *)stRentd(pcontext, pdbcontext->privpool,
                                           (unsigned)bufsize);
            continue;
        }
        
        /* extract the object associate object id               */
        if (recGetSMALL(precordBuffer, SCFLD_OBJASSOC,(ULONG)0,
                   &objectAssociate, &indicator))
            goto done;

        /* get the name field */
        rc = recGetBYTES(precordBuffer,SCFLD_OBJNAME,0,
                            &fieldValue,&indicator);
        if (rc != 0)
        {
            goto done;
        }

        if (recGetLONG(precordBuffer, SCFLD_OBJTYPE,(ULONG)0,
                   &objectType, &indicator))
            goto done;

        if (recGetLONG(precordBuffer, SCFLD_OBJAREA,(ULONG)0,
                   &objectArea, &indicator))
            goto done;

        /* get the object number */
        if (recGetSMALL(precordBuffer, SCFLD_OBJNUM,(ULONG)0,
                   &objectNumber, &indicator))
            goto done;

        if (objectArea == area)
        {
            /* Delete this record */
            rc = dbObjectRecordDelete(pcontext, objectNumber, objectType, 
                                     objectAssociate);
            if (rc)
            {
                MSGD_CALLBACK(pcontext, 
                    "Unable to delete object record num %d, type %d, ret %d",
                         objectNumber, objectType, rc);
                goto done;
            }
            rc = omDelete(pcontext, objectType, objectAssociate, objectNumber);
            if (rc)
            {
                MSGD_CALLBACK(pcontext, 
                "Unable to delete object record in object cache %d, ret %d",
                         indexRecordId, rc);
                goto done;
            }

           if (objectType == DSMOBJECT_MIXTABLE)
               MSGD_CALLBACK(pcontext, "Deleting Table %s id %d %L",
                    fieldValue.pbyte, objectNumber);
           else if (objectType == DSMOBJECT_MIXINDEX)
               MSGD_CALLBACK(pcontext, "Deleting Index %s id %d on table %d %L",
                    fieldValue.pbyte, objectNumber, objectAssociate);
        }
 
        /* attempt to get next index entry within specified bracket */
        rc = (dsmStatus_t)cxNext(pcontext, &cursorId, DSMAREA_SCHEMA, 
                                  &indexRecordId, &objectKeyLow.akey, 
                                  &objectKeyHi.akey,
                                  DSMGETTAG, DSMFINDNEXT, LKNOLOCK);
    } 
 
    /* There are no more storage object records for this table */
    if( rc == DSM_S_ENDLOOP )
    {
          rc = 0;
    }
 
done:    
    cxDeactivateCursor(pcontext, &cursorId);
    stVacate(pcontext, pdbcontext->privpool, precordBuffer);
    return(rc);

}
