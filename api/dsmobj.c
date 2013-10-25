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

#include "bkpub.h"      /* for bkfilc() */
#include "cxpub.h"      /* for cxCreateIndex(), cxKill() */
#include "dbcode.h"
#include "dbcontext.h"
#include "dbpub.h"
#include "drmsg.h"
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

#include <setjmp.h>

/****   LOCAL  FUNCTION  PROTOTYPES   ****/
 

/****   END  LOCAL  FUNCTION  PROTOTYPES   ****/


/* PROGRAM: dsmObjectCreate - create a database storage object 
 *
 *
 * RETURNS: DSM_S_SUCCESS unqualified success
 *          DSM_S_DSMOBJECT_INVALID_AREA on failure
 *          DSM_S_DSMOBJECT_CREATE_FAILED on failure
 *          DSM_S_FAILURE on failure
 */
dsmStatus_t 
dsmObjectCreate(
            dsmContext_t       *pcontext,   /* IN database context */
	    dsmArea_t 	       area,        /* IN area */
	    dsmObject_t	       *pobject,    /* IN/OUT for object id */
	    dsmObjectType_t    objectType,  /* IN object type */
            dsmObjectAttr_t    objectAttr,  /* IN object attributes */
            dsmObject_t        associate,   /* IN object number to use
                                               for sequential scan of tables*/
            dsmObjectType_t    associateType, /* IN object type of associate
                                                 usually an index           */
            dsmText_t          *pname,      /* IN name of the object */
	    dsmDbkey_t         *pblock,     /* IN/OUT dbkey of object block */
	    dsmDbkey_t         *proot)      /* OUT dbkey of object root
                                             *  - for INDEX: index root block
                                             *  - 1st possible rec in the table.
                                             */
{
    dsmObject_t    objectNumber;   /* return for bkfilc()/cxCreateIndex()  */
    dsmStatus_t    returnCode;     /* return status for dsmObjectCreate() */
    dsmStatus_t    retOmCreate,rc;
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    omDescriptor_t omDescriptor;
    objectNote_t   objectNote;

    pdbcontext->inservice++; /* postpone signal handler processing */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmObjectCreate");
        goto done;
    }

 
    if ( objectType == DSMOBJECT_TEMP_TABLE ||
         objectType == DSMOBJECT_TEMP_INDEX )
    {
       /* in case we'r creating a sql temp result object we
          need only allocate object anchor for the object and
          hang it off the usercontrol of the user and return */
      returnCode = omGetNewTempObject(pcontext, pname, objectType, objectAttr,
					  associate,pobject);
      goto done;
    }
 

    if (!pobject)
    {
        returnCode = DSM_S_DSMOBJECT_CREATE_FAILED;
        goto done;
    }

    objectNumber = *pobject;

    /* do some area validation */
    if (area == DSMAREA_INVALID)
        area = DSMAREA_DEFAULT;
    else
    {
        bkAreaDesc_t      *pbkArea;
        bkAreaDescPtr_t   *pbkAreas;

        pbkAreas = XBKAREADP(pdbcontext, pdbcontext->pbkctl->bk_qbkAreaDescPtr);
        if (pbkAreas == NULL)
            returnCode = DSM_S_AREA_NOT_INIT;
        else if (area >= pbkAreas->bkNumAreaSlots )
            returnCode = DSM_S_AREA_NUMBER_TOO_LARGE;
        else
        {
            /* ensure that area exists, contains extents
             * and is a data area
             */
            pbkArea = XBKAREAD(pdbcontext, pbkAreas->bk_qAreaDesc[area]);
            if (!pbkArea || !pbkArea->bkNumFtbl ||
                 pbkArea->bkAreaType != DSMAREA_TYPE_DATA)
                returnCode = DSM_S_DSMOBJECT_INVALID_AREA;
            else
                returnCode = 0;
        }
        if (returnCode)
        {
            MSGN_CALLBACK(pcontext, drMSG691, area);
            goto done;
        }
    }

    /* Get an admin lock to cover getting of new object number with
       the creation of the storage object record.                   */
    returnCode = lkAdminLock(pcontext,1,1,DSM_LK_EXCL);
    if( returnCode )
    {
        MSGN_CALLBACK(pcontext, drMSG692, returnCode);
        return returnCode;
    }
                      
    /* Based on objectType, process the object */
    switch(objectType)
    {
        case DSMOBJECT_MIXTABLE:
        case DSMOBJECT_BLOB:
            if (objectNumber)
            {
#if 0
                if (objectType == DSMOBJECT_BLOB)
                    returnCode = DSM_S_DSMOBJECT_CREATE_FAILED; 
                else 
#endif
                    /* only table number can be preset */
                    returnCode = DSM_S_SUCCESS;
            }
            else
            {
                /* If no number specified, 
                 * call om to pick the next table or blob object number
                 */
                returnCode = omGetNewObjectNumber(pcontext, objectType, 
                                                   associate, &objectNumber);
            }
            associate = objectNumber;
            break;
        case DSMOBJECT_MIXINDEX:

	    objectNumber = cxCreateIndex(pcontext, objectAttr,
					 (int)objectNumber, area, 
                                         associate, proot);

            /* success > DSMINDEX_INVALID returned or
             * the index number returned was the one we requested
             * and not DSMINDEX_INVALID because this means cx should 
             * have picked a number > DSMINDEX_INVALID).
             */
	    if ((objectNumber > DSMINDEX_INVALID) ||
                ( (objectNumber == *pobject) &&
                  (*pobject != DSMINDEX_INVALID)) )
            {
		/* Index successfully created */
		returnCode = DSM_S_SUCCESS;
	    }
	    else
		returnCode =  DSM_S_DSMOBJECT_CREATE_FAILED;

	    break;

        case DSMOBJECT_MIXED:
#if 0
/* removing temporarily for release.  will reactivate when
   code is change to properly reflect definitions and spec */
        case DSMOBJECT_TABLE:
        case DSMOBJECT_INDEX:
#endif
            /* TODO: not supported yet : fail until this is flushed out   */
            objectNumber = 0;
            returnCode = DSM_S_DSMOBJECT_CREATE_FAILED;
            break;

        default:
            /* Incorrect object type requested */
            objectNumber = 0;
            returnCode = DSM_S_DSMOBJECT_CREATE_FAILED;
            break;
    } 

    if (returnCode == DSM_S_SUCCESS)
    {
        *pobject = objectNumber;  /* update the object # passed in 
                                   * (it may or may not have been changed.
                                   */

        /* Build object descriptor table */
        omDescriptor.area                = area;
        omDescriptor.objectBlock         = *pblock; /* dbkey of Object blk */
        omDescriptor.objectRoot          = *proot;
        omDescriptor.objectAttributes    = objectAttr;
        omDescriptor.objectAssociate     = associate;
        omDescriptor.objectAssociateType = associateType;
        
        /* write a logical note stating that we added a new
         * entry in the om cache - since the undo operation is an omDelete
         * which is allowed to fail and we are not writing the note and 
         * updating the cache atomically we will write the note before
         * we actually do the operation.
         */
        INITNOTE(objectNote, RL_OBJECT_CREATE, RL_LOGBI);
        /* fill in note information */
        objectNote.objectType   = objectType;
        objectNote.objectNumber = objectNumber;
        objectNote.tableNum = associate;
        objectNote.areaNumber   = area;
        rlLogAndDo(pcontext, (RLNOTE *)&objectNote, 0, (COUNT)0, PNULL );

        retOmCreate = omCreate(pcontext, objectType, objectNumber,
                               &omDescriptor);

        if (retOmCreate != DSM_S_SUCCESS)
        {
            /* object cache not successfully updated so fail */
            returnCode = DSM_S_DSMOBJECT_CREATE_FAILED;
        }
    }

    if ((returnCode == DSM_S_SUCCESS) &&
        !(pdbcontext->accessEnv & DSM_SCDBG) )
    {
        if (pdbcontext->dbcode == PROCODE)
        {
            /* create _Object Record and index entry */
            returnCode = dsmObjectRecordCreate(pcontext, area, *pobject,
                                               associateType,
                                               associate, pname,
                                               objectType, objectAttr,
                                               *pblock, *proot,
                                               (ULONG)0);
            if (returnCode != DSM_S_SUCCESS)
            {
                /* object record not successfully create so fail */
                returnCode = DSM_S_DSMOBJECT_CREATE_FAILED;
            }
        }
        if( objectType == DSMOBJECT_MIXINDEX )
        {
            rc = cxLogIndexCreate( pcontext, area, objectNumber,associate);

            /* create the index roots */
            rc = dbIndexAnchor(pcontext, area, pname, *proot, NULL, IX_ANCHOR_CREATE);
            if (rc != DSM_S_SUCCESS)
            {
                /* object record not successfully create so fail */
                returnCode = DSM_S_INDEX_ANCHOR;
            }
        }
    }
    rc = lkAdminUnlock(pcontext,1,1);
    
done:
    
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;
    return (returnCode);

}  /* end dsmObjectCreate */

#if 0
/* PROGRAM: dsmObjectDelete - delete a database storage object 
 *
 *
 * RETURNS: DSM_S_SUCCESS unqualified success
 *          DSM_S_DSMOBJECT_DELETE_FAILED on failure
 */
dsmStatus_t 
dsmObjectDelete(
            dsmContext_t       *pcontext,  /* IN database context */
	    dsmObject_t	       object,     /* IN object id */
	    dsmObjectType_t    objectType, /* IN object type */
	    dsmMask_t          deleteMode, /* IN delete mode bit mask */
            dsmObject_t        associate,   /* IN object number to use
                                               for sequential scan of tables*/
	    dsmText_t          *pname)     /* IN reference name for messages */
{
    dsmStatus_t    returnCode;     /* return status for dsmObjectDelete() */
    dsmStatus_t    retOmDelete;    /* return for omDelete() */ 
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    omDescriptor_t indexDesc;
    omDescriptor_t tableDesc;
    
    pdbcontext->inservice++; /* postpone signal handler processing */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmObjectDelete");
        goto done;
    }

    /* Based on objectType, get the objectNumber */
    switch(objectType)
    {
        case DSMOBJECT_MIXINDEX:
            /* deleteMode: 1 - Delete the Btree
             *             0 - don't
             * (Only set to 1 for temp-tables)
             */
	    returnCode = omFetch(pcontext,DSMOBJECT_MIXINDEX,object,
                                 associate,&indexDesc);
            if (returnCode != DSM_S_SUCCESS)
            {
                 break;
            }

            /* need to get associate table's area to delete
               the records associated with this index.
            */
            returnCode = omFetch(pcontext,DSMOBJECT_MIXTABLE,
                                indexDesc.objectAssociate, 
                                indexDesc.objectAssociate, &tableDesc);
            if (returnCode != DSM_S_SUCCESS)
            {
                break;
            }

            if (deleteMode)
            {
                cxKill(pcontext, indexDesc.area,indexDesc.objectRoot,
		       (COUNT)object, deleteMode, associate, tableDesc.area);
            }

            returnCode = DSM_S_SUCCESS;
            break;

        case DSMOBJECT_MIXTABLE:
        case DSMOBJECT_BLOB:
            /* TODO: call xxObjectDelete  */
            /* xxObjectDelete() */

            returnCode = DSM_S_SUCCESS;
            break;

        case DSMOBJECT_MIXED:
#if 0
/* removing temporarily for release.  will reactivate when
   code is change to properly reflect definitions and spec */
        case DSMOBJECT_MIXTABLE:
        case DSMOBJECT_MIXINDEX:
#endif
            /* TODO: not supported yet : fail until this is flushed out   */
            returnCode = DSM_S_DSMOBJECT_DELETE_FAILED;
            break;

#ifdef TEMP_RESULT_TABLES
 
          case DSMOBJECT_TEMPRESULTS_TABLES:
          case DSMOBJECT_TEMPRESULTS_INDEX:
               if( omReleaseObjectAnchor( pcontext, object, objectType ) )
               {
                  /* unable to release temp result object */
                  return DSM_S_FAILURE;
               }
               break;
 
          case DSMOBJECT_TEMPRESULTS_SORTTABLE:
               /* this case is not yet implemented, will be available as
                  soon as something is done about temp result sort ,
                  there may be a need to release more than one object*/
               break;
#endif

        default:
            /* Incorrect object type requested */
            returnCode = DSM_S_DSMOBJECT_DELETE_FAILED;
            break;
    }

    if (returnCode == DSM_S_SUCCESS)
    {
#if 0
        if (pdbcontext->accessEnv & (DSM_JAVA_ENGINE + DSM_SQL_ENGINE))
#else
        if ( (pdbcontext->dbcode == PROCODE) &
             !(pdbcontext->accessEnv & DSM_SCDBG) )
#endif
        {

            /* delete _Object Record and index entry */
            returnCode = dbObjectRecordDelete(pcontext, object, objectType,
                                 associate);
            if (returnCode != DSM_S_SUCCESS)
            {
                returnCode = DSM_S_DSMOBJECT_DELETE_FAILED; 
                goto done;
            }
        }

        /* omDelete removes the object entry from the cache */
        retOmDelete = omDelete(pcontext, objectType, associate, object);

        if (retOmDelete != DSM_S_SUCCESS)
        {
            /* object cache not successfully updated so fail */
            returnCode = DSM_S_DSMOBJECT_DELETE_FAILED;
        }
    }
    
done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;
    return (returnCode);

}  /* end dsmObjectDelete */


/* PROGRAM: dsmObjectUpdate - Modify the object 
 *
 *
 * RETURNS: DSM_S_SUCCESS unqualified success
 *          DSM_S_NO_TRANSACTION Caller must have started a tx.  
 *          DSM_S_INVALID_FIELD_VALUE update field value is invalid
 *          DSM_S_FAILURE on failure
 */
dsmStatus_t 
dsmObjectUpdate(
            dsmContext_t       *pcontext,     /* IN database context */
            dsmArea_t 	       area,          /* IN area */
            dsmObject_t	       objectNumber,  /* IN  for object id */
	    dsmObjectType_t    objectType,    /* IN object type */
            dsmObjectAttr_t    objectAttr,    /* IN object attributes */
            dsmObject_t        associate,     /* IN object number to use
                                               for sequential scan of tables*/
            dsmObjectType_t    associateType, /* IN object type of associate
                                                 usually an index           */

	    dsmDbkey_t         obectBlock,  /* IN dbkey of object block */
	    dsmDbkey_t         rootBlock,   /* IN dbkey of object root
                                             * - for INDEX: index root block
                                             * - 1st possible rec in the table.
                                             */
            dsmMask_t          updateMask)  /* bit mask identifying which
                                               storage object attributes are
                                               to be modified. */
{
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    dsmStatus_t    returnCode;
    omDescriptor_t objectDescriptor;
    
    pdbcontext->inservice++; /* postpone signal handler processing */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmObjectUpdate");
        goto done;
    }

    /* Check to see if a transaction is active for this user. */
    if( pcontext->pusrctl->uc_task == 0 )
    {
        returnCode = DSM_S_NO_TRANSACTION;
        goto done;
    }
    
    /* Load an object descriptor struct with info passed in */
    objectDescriptor.area = area;
    objectDescriptor.objectBlock = obectBlock;
    objectDescriptor.objectRoot = rootBlock;
    objectDescriptor.objectAttributes = objectAttr;
    objectDescriptor.objectAssociateType = associateType;
    objectDescriptor.objectAssociate = associate;

    /* Update the object record according to the updateMask */  
    returnCode = dbObjectUpdateRec(pcontext, objectNumber, objectType,
                                   &objectDescriptor, updateMask, NULL, 0, 
                                   DSMAREA_TYPE_INVALID);  

done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;

    return (returnCode);

}  /* end dsmObjectUpdate */

#endif

/* PROGRAM: dsmObjectInfo - Return contents of storageObject record.
 *
 *
 * RETURNS: 0 unqualified success
 *         
 */
dsmStatus_t 
dsmObjectInfo(
    dsmContext_t       *pcontext,          /* IN database context */
    dsmObject_t         objectNumber,      /* IN object number    */
    dsmObjectType_t     objectType,        /* IN object type      */
    dsmObject_t         associate,         /* IN Associate object number */
    dsmArea_t           *parea,            /* OUT area object is in */
    dsmObjectAttr_t     *pobjectAttr,      /* OUT Object attributes    */
    dsmObjectType_t     *passociateType,   /* OUT Associate object type  */
    dsmDbkey_t          *pblock,           /* OUT dbkey of object block  */
    dsmDbkey_t          *proot)            /* OUT dbkey of index root block */
{
    dsmStatus_t   returnCode;
    omDescriptor_t objectDesc;

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmObjectInfo");
        goto done;
    }
    
#ifdef VST_ENABLED
    /* siphon off VST requests */
    if ( ((objectType == DSMOBJECT_MIXINDEX) &&
          SCIDX_IS_VSI(objectNumber) ) ||
         ((objectType == DSMOBJECT_MIXTABLE) &&
          SCTBL_IS_VST(objectNumber)) )
          
    {
        objectDesc.area = 0;
        objectDesc.objectAssociate = objectNumber;
        objectDesc.objectBlock = 0;
        objectDesc.objectRoot = 0;

        if (objectType == DSMOBJECT_MIXINDEX)
        {
            objectDesc.objectAttributes = DSMINDEX_UNIQUE;
            objectDesc.objectAssociateType = DSMOBJECT_MIXTABLE;
        }
        else
        {
            objectDesc.objectAttributes = 0;
            objectDesc.objectAssociateType = DSMOBJECT_MIXINDEX;
        }

        returnCode = 0;
    }
    else
#endif  /* VST_ENABLED */
        returnCode = omFetch(pcontext, objectType, objectNumber, associate, &objectDesc);

    if( returnCode == 0 )
    {
        if(parea)
            *parea = objectDesc.area;
        if(pobjectAttr)
            *pobjectAttr = objectDesc.objectAttributes;
        if(passociateType)
            *passociateType = objectDesc.objectAssociateType;
        if(pblock)
            *pblock = objectDesc.objectBlock;
        if(proot)
            *proot = objectDesc.objectRoot;
    }

done:
    dsmThreadSafeExit(pcontext);

    return returnCode;

}  /* end dsmObjectInfo */


/* PROGRAM: dsmObjectLock - lock a database object 
 *
 *
 * RETURNS: 0 unqualified success
 *          DSM_S_DSMOBJECT_LOCK_FAILED on failure
 */
dsmStatus_t 
dsmObjectLock(
            dsmContext_t       *pcontext,  /* IN database context */
	    dsmObject_t	       object,     /* IN object id */
	    dsmObjectType_t    objectType, /* IN object type */
	    dsmRecid_t         recid,      /* IN object type */
	    dsmMask_t          lockMode,   /* IN delete mode bit mask */
            COUNT              waitFlag,   /* IN client:1(wait), server:0 */
	    dsmText_t          *pname)     /* IN reference name for messages */
{
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    dsmStatus_t    returnCode;
    int            request;
    int            waitMsg;

    pdbcontext->inservice++; /* postpone signal handler processing */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmObjectLock");
        goto done;
    }

    /* Based on objectType, perform the lock */
    switch(objectType)
    {
        case DSMOBJECT_INDEX:
            returnCode = lkAdminLock(pcontext, object, 0, lockMode);
            break;
        case DSMOBJECT_TABLE:
            recid = 0;
        case DSMOBJECT_RECORD:
            returnCode = dbLockRow(pcontext, object, recid,
                                   lockMode, pname, waitFlag);
            break;
        case DSMOBJECT_SCHEMA:
            switch(lockMode & DSM_LK_TYPE)
            {
              /* NOTE: SLGUARSER Not supported */
              case DSM_LK_SHARE:
                request = SLSHARE;
                waitMsg = LW_SCHLK_MSG;
                break;
              case DSM_LK_EXCL:
                request = SLEXCL;
                waitMsg = LW_DBLK_MSG;
                break;
              default:
                returnCode = DSM_S_INVALID_LOCK_MODE;
                goto done;
            }
            /* A return code of 32 (SLCHG) means that the schema lock was
             * granted but that the schema has changed since the last time
             * this userctl looked at the schema.
             * TODO: We need to get rid of this magic number and 
             *       decide if this is useful information to the caller of
             *       this dsm entry point.
             */
retrylk:
            /* attempt/re-attempt the lock */
            returnCode = dblksch(pcontext, request);
            if (returnCode != DSM_S_SUCCESS)
            {
                if (returnCode == 32)
                    returnCode = DSM_S_SCHEMA_CHANGED;
                else if(returnCode == DSM_S_RQSTQUED)
                {
                    returnCode = lkwait(pcontext, waitMsg, PNULL);
                    if (!returnCode)
                        goto retrylk;
                }
            }
            break;
        default:
            /* ERROR - invalid type detected */
            returnCode = DSM_S_INVALID_LOCK_MODE;
    }

done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;
    return (returnCode);

}  /* end dsmObjectLock */


/* PROGRAM: dsmObjectUnlock - unlock a database object 
 *
 *
 * RETURNS: 0 unqualified success
 *          DSM_S_DSMOBJECT_UNLOCK_FAILED on failure
 */
dsmStatus_t 
dsmObjectUnlock(
            dsmContext_t       *pcontext,  /* IN database context */
	    dsmObject_t	       object,     /* IN object id */
	    dsmObjectType_t    objectType, /* IN object type */
	    dsmRecid_t         recid,      /* IN object type */
	    dsmMask_t          lockMode)   /* IN delete mode bit mask */
{
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    dsmStatus_t    returnCode;
    lockId_t       lockId;
    
    pdbcontext->inservice++; /* postpone signal handler processing */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmObjectUnlock");
        goto done;
    }

    /* Based on objectType, perform the unlock */
    switch(objectType)
    {
        case DSMOBJECT_INDEX:
            returnCode = lkAdminUnlock(pcontext, object, 0);
            break;
        case DSMOBJECT_TABLE:
            recid = 0;
        case DSMOBJECT_RECORD:
            if ( ((COUNT)object <= SCTBL_VST_FIRST) &&
                 ((COUNT)object >= SCTBL_VST_LAST) )
            {
                ; /* Ignore locks on VSTs */
            }
            else if( lockMode & DSM_UNLK_FREE )
            {
                lockId.table = object;
                lockId.dbkey = recid;

                lkrestore(pcontext,&lockId, lockMode);
            }
            else
            {
                dbUnlockRow(pcontext, object, recid);
            }
            returnCode = DSM_S_SUCCESS;
            break;
        case DSMOBJECT_SCHEMA:
            switch(lockMode & DSM_LK_TYPE)
            {
              /* NOTE: SLRELGS Not supported */
              case DSM_LK_SHARE:
                returnCode = dblksch(pcontext, SLRELS);
                break;
              case DSM_LK_EXCL:
                returnCode = dblksch(pcontext, SLRELX);
                break;
              default:
                returnCode = DSM_S_INVALID_LOCK_MODE;
                break;
            }
            break;
        default:
            /* ERROR - invalid type detected */
            returnCode = DSM_S_DSMOBJECT_LOCK_FAILED;
    }

done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;
    return (returnCode);

}  /* end dsmObjectUnlock */


/* PROGRAM: dsmObjectRecordCreate
 *
 *
 * RETURNS:
 */
dsmStatus_t
dsmObjectRecordCreate(
	dsmContext_t	*pcontext,
	dsmArea_t	 areaNumber,
	dsmObject_t	 objectNumber,
        dsmObjectType_t  objectAssociateType,
	dsmObject_t	 objectAssociate,
	dsmText_t 	*pname,
	dsmObjectType_t  objectType,
        dsmObjectAttr_t  objectAttr,
	dsmDbkey_t       objectBlock,
	dsmDbkey_t       objectRoot,
	ULONG		 objectSystem)  /* not currently used */
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dsmStatus_t  returnCode = 0;
    lockId_t     lockId;        
    ULONG        recSize;
 
    TEXT        *pobjectRecord = 0;
 
    AUTOKEY(keyStruct, 2*KEYSIZE )

    svcDataType_t component[5];
    svcDataType_t *pcomponent[5];
    svcByteString_t fieldValue;

    COUNT         maxKeyLength = 50; /* SHOULD BE A CONSTANT! */
    dsmRecord_t  recordDesc;

    pobjectRecord = (TEXT *)stRent(pcontext, pdbcontext->privpool, MAXRECSZ);

    /* If there was a named passed in, then use it */
    if (pname)
    {
        fieldValue.pbyte = pname;
    }
    else
    {
        fieldValue.pbyte = (TEXT *)" ";
    }
    fieldValue.size = stlen(fieldValue.pbyte);
    fieldValue.length = stlen(fieldValue.pbyte);
 
    returnCode = DSM_S_FAILURE;
    /* materialize _Object template record */
    if (RECSUCCESS != recRecordNew(SCTBL_OBJECT, pobjectRecord, 
                                   MAXRECSZ, &recSize,
                                   SCFLD_OBJNAME, 0/*schemaversion*/))
        goto done;

    /* Format misc field */
    if (RECSUCCESS != recInitArray(pobjectRecord, MAXRECSZ, &recSize, 
                                   SCFLD_OBJMISC, 8/* num field extents */))
        goto done;

    /* set _Db-recid */
    if (recPutLONG(pobjectRecord, MAXRECSZ, &recSize,
                SCFLD_OBJDB,(ULONG)0, pdbcontext->pmstrblk->mb_dbrecid, 0) )
        goto done;

    /* set _Object-type */
    if (recPutLONG(pobjectRecord, MAXRECSZ, &recSize,
                   SCFLD_OBJTYPE,(ULONG)0, objectType, 0) )
        goto done;

    /* set _Object-number */
    if (recPutLONG(pobjectRecord, MAXRECSZ, &recSize,
                   SCFLD_OBJNUM,(ULONG)0, objectNumber, 0) )
        goto done;

    /* set _Object-associate-type    */
    if(recPutLONG(pobjectRecord,MAXRECSZ,&recSize,
                  SCFLD_OBJATYPE,(ULONG)0, objectAssociateType,0))
        goto done;
    
    /* set _Object-associate */
    if (recPutSMALL(pobjectRecord, MAXRECSZ, &recSize,
                   SCFLD_OBJASSOC,(ULONG)0, objectAssociate, 0) )
        goto done;


    /* set _Area-number */
    if (recPutLONG(pobjectRecord, MAXRECSZ, &recSize,
                   SCFLD_OBJAREA,(ULONG)0, areaNumber, 0) )
        goto done;

    /* set _Object-block */
    if (recPutBIG(pobjectRecord, MAXRECSZ, &recSize,
                   SCFLD_OBJBLOCK,(ULONG)0, objectBlock, 0) )
        goto done;

    /* set _Object-root */
    if (recPutBIG(pobjectRecord, MAXRECSZ, &recSize,
                   SCFLD_OBJROOT,(ULONG)0, objectRoot, 0) )
        goto done;

    /* set _Object-attrib */
    if (recPutLONG(pobjectRecord, MAXRECSZ, &recSize,
                   SCFLD_OBJATTR,(ULONG)0, (int)objectAttr, 0) )
        goto done;

    /* set _Object-system */
    if (recPutLONG(pobjectRecord, MAXRECSZ, &recSize,
                   SCFLD_OBJSYS,(ULONG)0, objectSystem, 0) )
        goto done;

    /* set _Object-name */
    if (recPutBYTES(pobjectRecord, MAXRECSZ, &recSize,
                   SCFLD_OBJNAME, 0, &fieldValue, 0) )
        goto done;

    /* set up the table descriptor */
    recordDesc.pbuffer   = pobjectRecord;
    /* NOTE: subtrct 1 from rec size because recRecordNew has added in ENDREC */
    recordDesc.recLength = recSize - 1;
    recordDesc.maxLength = MAXRECSZ;
    recordDesc.table     = SCTBL_OBJECT;
    recordDesc.recid = rmCreate(pcontext, DSMAREA_SCHEMA,
                                SCTBL_OBJECT,
                                recordDesc.pbuffer,
                                (COUNT)recordDesc.recLength,
                                (UCOUNT)RM_LOCK_NEEDED);
    if (recordDesc.recid == 0)
    {
        returnCode = DSM_S_FAILURE;
        goto done;
    }
    else if (recordDesc.recid <= 0)
    {
        returnCode = (dsmStatus_t)recordDesc.recid;
        goto done;
    }

    if (objectType == DSMOBJECT_MIXTABLE)
        MSGN_CALLBACK(pcontext, drMSG693, pname, objectNumber);
    else if (objectType == DSMOBJECT_MIXINDEX)
        MSGN_CALLBACK(pcontext, drMSG694, pname, objectNumber, objectAssociate);

    /* recid is returned in recordDesc.recid */
    lockId.dbkey = recordDesc.recid;
    lockId.table  = SCTBL_OBJECT;
    

    /* _Object record generation completed
     *
     * Now index the record just added.
     */
    component[0].type            = SVCLONG;
    component[0].indicator       = 0;
    component[0].f.svcLong       = (LONG)0;

    component[1].type            = SVCLONG;
    component[1].indicator       = 0;
    component[1].f.svcLong       = (LONG)objectType;

    component[2].type            = SVCSMALL;
    component[2].indicator       = 0;
    component[2].f.svcSmall      = objectAssociate;

    component[3].type            = SVCSMALL;
    component[3].indicator       = 0;
    component[3].f.svcSmall      = objectNumber;

    pcomponent[0] = &component[0];
    pcomponent[1] = &component[1];
    pcomponent[2] = &component[2];
    pcomponent[3] = &component[3];
    
    keyStruct.akey.area         = DSMAREA_SCHEMA;
    keyStruct.akey.root         = pdbcontext->pmstrblk->mb_objectRoot;
    keyStruct.akey.index        = SCIDX_OBJID;
    keyStruct.akey.keycomps     = 4;
    keyStruct.akey.ksubstr      = 0;
    keyStruct.akey.unique_index = 1;
    keyStruct.akey.word_index   = 0;

    returnCode = keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength,
                           &keyStruct.akey);
    if (returnCode)
    {
        returnCode = DSM_S_FAILURE;
	goto done;
    }

    returnCode = cxAddEntry(pcontext, &keyStruct.akey, &lockId, NULL, 
                            objectAssociate);

done:
   if (pobjectRecord)
     stVacate(pcontext, pdbcontext->privpool, pobjectRecord);

    return returnCode;

}  /* end dsmObjectRecordCreate */

/* PROGRAM: dsmObjectNameToNum - Given a name, return a num
 *
 *    Given an object name, return the number associated with it
 *
 * RETURNS: DSM_S_SUCCESS success
 *          DSM_S_FAILURE on failure
 */
dsmStatus_t 
dsmObjectNameToNum(dsmContext_t *pcontext,   /* IN database context */
		   dsmBoolean_t  tempTable,  /* 1 if temporary object */
                 dsmText_t  *pname,          /* IN the name */
                 dsmObject_t         *pnum)  /* OUT number */
{
    dsmStatus_t  returnCode = 0;
    svcLong_t    num = 0;

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmObjectNameToNum");
        goto done;
    }

    if(tempTable)
      omNameToNum(pcontext,pname,pnum);
    else
    {
      returnCode = dbObjectNameToNum(pcontext, pname, &num);
      *pnum = (dsmObject_t)num;
    }

done:
    dsmThreadSafeExit(pcontext);
    return returnCode;

}

/* PROGRAM: dsmObjectDeleteAssociate - Removes all object records for the 
 *                 passed associate type.  This means all indexes and
 *                 tables will be removed
 *
 * RETURNS: DSM_S_SUCCESS unqualified success
 *          - -1 for FAILURE
 */
dsmStatus_t
dsmObjectDeleteAssociate(dsmContext_t *pcontext, 
                         dsmObject_t   num,
                         dsmArea_t    *indexArea)
{
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    dsmStatus_t    returnCode;

    pdbcontext->inservice++; /* postpone signal handler processing */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */
 
    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmObjectDeleteAssociate");
        goto done;
    }

    returnCode = dbObjectDeleteAssociate(pcontext,num,indexArea);

done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--; /* reset signal handler processing */

    return (returnCode);

}

/* PROGRAM: dsmObjectRename - Rename a table (storage objects and extents
 *                            for the table and indexes)
 *
 * RETURNS: DSM_S_SUCCESS unqualified success
 *          DSM_S_FAILURE for FAILURE
 */

dsmStatus_t
dsmObjectRename( 
            dsmContext_t   *pContext,     /* IN database context */
            dsmObject_t     tableNumber,  /* IN table number */
            dsmText_t      *pNewName,     /* IN new name of the object */
            dsmText_t      *pIdxExtName,  /* IN new name of the index extent */
            dsmText_t      *pNewExtName,  /* IN new name of the extent */
            dsmArea_t      *pIndexArea,   /* OUT index area */
            dsmArea_t      *pTableArea)   /* OUT table area */
{
    dbcontext_t   *pdbcontext = pContext->pdbcontext;
    dsmStatus_t    returnCode;

    pdbcontext->inservice++; /* postpone signal handler processing */

    SETJMP_ERROREXIT(pContext, returnCode) /* Ensure error exit address set */
 
    if ((returnCode = dsmThreadSafeEntry(pContext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pContext, returnCode,
                      (TEXT *)"dsmObjectDeleteAssociate");
        goto done;
    }

    /* rename the storage objects (table and indexes) */
    returnCode = dbObjectRename(pContext, pNewName, tableNumber, pIndexArea);
    if (returnCode)
        goto done;

    /* rename the table extent */
    returnCode = omIdToArea(pContext, DSMOBJECT_MIXTABLE, tableNumber, 
                            tableNumber, pTableArea );
    if (returnCode)
        goto done;

    returnCode = bkExtentRename(pContext, *pTableArea, pNewExtName);
    if (returnCode)
        goto done;

    /* rename the index extent */
    if (*pIndexArea)
    {
        returnCode = bkExtentRename(pContext, *pIndexArea, pIdxExtName);
    }

done:
    dsmThreadSafeExit(pContext);

    pdbcontext->inservice--; /* reset signal handler processing */

    return (returnCode);
}

