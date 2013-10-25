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
#include "dbcontext.h"

#include "dbpub.h"
#include "bkpub.h"      /* for bkfilc() */
#include "bkaread.h"    /* area descriptor macros */
#include "cxpub.h"      /* for cxCreateIndex(), cxKill() */
#include "dbcode.h"
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
#include "statpub.h"
#include "usrctl.h"
#include "rmprv.h"
#include "tmmgr.h"


/****   LOCAL  FUNCTION  PROTOTYPES   ****/
 
/****   END  LOCAL  FUNCTION  PROTOTYPES   ****/


/* PROGRAM: dbObjectRecordDelete - remove a specific storage object record
 *
 *
 * RETURNS: DSM_S_SUCCESS
 *          DSM_S_FAILURE
 */
dsmStatus_t
dbObjectRecordDelete(
	dsmContext_t	*pcontext,
	dsmObject_t	 objectNumber,
	dsmObjectType_t  objectType,
        dsmObject_t      tableNum)
{
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    dsmStatus_t    returnCode;
    DBKEY          objectRecordId;
    lockId_t       lockId;  

    AUTOKEY(keyBase,  2*KEYSIZE )
    AUTOKEY(keyLimit, 2*KEYSIZE )

    svcDataType_t component[5];
    svcDataType_t *pcomponent[5];
    COUNT         maxKeyLength = 50; /* SHOULD BE A CONSTANT! */
 
    /* Build index of object record  */
    component[0].type            = SVCLONG;
    component[0].indicator       = 0;
    component[0].f.svcLong       = (LONG)pdbcontext->pmstrblk->mb_dbrecid;

    component[1].type            = SVCLONG;
    component[1].indicator       = 0;
    component[1].f.svcLong       = (LONG)objectType;

    component[2].type            = SVCSMALL;
    component[2].indicator       = 0;
    component[2].f.svcSmall      = tableNum;

    component[3].type            = SVCSMALL;
    component[3].indicator       = 0;
    component[3].f.svcSmall      = objectNumber;

    pcomponent[0] = &component[0];
    pcomponent[1] = &component[1];
    pcomponent[2] = &component[2];
    pcomponent[3] = &component[3];
    
    keyBase.akey.area         = DSMAREA_SCHEMA;
    keyBase.akey.root         = pdbcontext->pmstrblk->mb_objectRoot;
    keyBase.akey.unique_index = 1;
    keyBase.akey.word_index   = 0;
    keyBase.akey.index        = SCIDX_OBJID;
    keyBase.akey.keycomps     = 4;
    keyBase.akey.ksubstr      = 0;

    returnCode = keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength,
                           &keyBase.akey);
    if (returnCode)
    {
        returnCode = DSM_S_FAILURE;
	goto done;
    }

    component[4].indicator = SVCHIGHRANGE;

    keyLimit.akey.area          = DSMAREA_SCHEMA;
    keyLimit.akey.root          = pdbcontext->pmstrblk->mb_objectRoot;
    keyLimit.akey.unique_index  = 1;
    keyLimit.akey.word_index    = 0;
    keyLimit.akey.index         = SCIDX_OBJID;
    keyLimit.akey.keycomps      = 5;
    keyLimit.akey.ksubstr       = 0;

    pcomponent[4]          = &component[4];

    returnCode = keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength,
                           &keyLimit.akey);
    if (returnCode)
    {
        returnCode = DSM_S_FAILURE;
	goto done;
    }

    /* Now do the Find EXCLUSIVE */
    returnCode = cxFind(pcontext, &keyBase.akey, &keyLimit.akey,
                        NULL /* No cursor */,
                        SCTBL_OBJECT, (dsmRecid_t *)&objectRecordId,
                        DSMUNIQUE, DSMFINDFIRST, DSM_LK_EXCL);
    if (returnCode)
        goto done;

    lockId.table  = SCTBL_OBJECT;
    lockId.dbkey = (DBKEY)objectRecordId;
    returnCode = cxDeleteEntry(pcontext, &keyBase.akey, &lockId, 0, 
                               SCTBL_OBJECT, NULL);
    if (returnCode)
        goto done;

    /* Delete the associated Object Record */
    returnCode = (dsmStatus_t)rmDelete (pcontext, SCTBL_OBJECT,
                                        (DBKEY)objectRecordId, NULL);

done:
    return returnCode;

} /* end dbObjectRecordDelete */

/* PROGRAM: dbRecordGet - retrieve a record from the database
 *                         Acquires a record get lock if the
 *                         user control does not indicate a user
 *                         has a lock on the record.   
 *
 * RETURNS: 
 *      negative - Some error condition - usually RMNOTFND or
 *                                        DSM_S_RECORD_BUFFER_TOO_SMALL
 *      0        - A-OK
 *     
 */
dsmStatus_t
dbRecordGet(
	dsmContext_t	*pcontext,
	dsmRecord_t	*pRecord, 
	dsmMask_t	 getMode _UNUSED_)
{
    dsmStatus_t  returnCode;
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbpub = pdbcontext->pdbpub;
    struct lck  *pLock;
    usrctl_t    *pusr       = pcontext->pusrctl;
    int          needRgLock = 1;
    lockId_t     lockId;          /* identifier for locking the record */
    int          recordSize;
    dsmTable_t   table;
    ULONG        area;

    if(pRecord->recid <= 0 )
        return DSM_S_RMNOTFND;
            
    table = pRecord->table;
    if ( pdbcontext->ptabstat  &&
         (pdbpub->isbaseshifting == 0 ) &&
          (table >= pdbpub->argtablebase) &&
         (table - pdbpub->argtablebase < pdbpub->argtablesize))
    {
           ((pdbcontext->ptabstat)[table
                                - pdbpub->argtablebase ]).read++;
    }
 

#ifdef VST_ENABLED
    /* siphon off VST requests */
    if (RM_IS_VST(table))
    {
	/* *** TODO: STATINC (rmvgtCt); */
    	recordSize = rmFetchVirtual(pcontext, table, 
                                    (DBKEY)pRecord->recid, 
				    pRecord->pbuffer, 
				    (COUNT)pRecord->maxLength);
        if( recordSize > (int)pRecord->maxLength )
        {
            pRecord->recLength = recordSize;
            returnCode = DSM_S_RECORD_BUFFER_TOO_SMALL;
        }
        else if ( recordSize > 0 )
        {
            pRecord->recLength = recordSize;
            returnCode = DSM_S_SUCCESS;
        }
        else
        {
            returnCode = (dsmStatus_t)recordSize;
        }

        /* Verify that the record is actually for the correct table */
        if( returnCode == DSM_S_SUCCESS  &&
           pRecord->table != recGetTableNum(pRecord->pbuffer))
        {
            returnCode = DSM_S_RMNOTFND;
        }

	goto done;
    }
#endif  /* VST ENABLED */
    
    returnCode = omIdToArea(pcontext, DSMOBJECT_MIXTABLE, table, table, &area );
    if( returnCode )
        goto done;

    
    lockId.table = (LONG)table;
    lockId.dbkey = pRecord->recid;

    /* Acquire the record get lock if the user does not already
       have a lock on the table.                                   */
    if( pusr->uc_qwait2 )
    {
	pLock = XLCK(pdbcontext, pusr->uc_qwait2);
	if( pLock->lk_id.dbkey == lockId.dbkey &&
	   pLock->lk_id.table == lockId.table)
	{
	    /* We've got a full strength record lock so no need
	       for a record get lock.                           */
	    needRgLock = 0;
	}
    }
    if (needRgLock)
    {
        /* get locks use area plus recid for lock id                   */
        lockId.table = area;
	returnCode = (dsmStatus_t)lkrglock(pcontext, &lockId,LKSHARE);
	if( returnCode )
	{
	    /* CTRLC or LKTBFULL */
	    goto done;
	}
    }
    
    pRecord->recLength = 0;
    /* Now get the record                */
    /* returns size of record if record is found.  Returns
       a negative number if record is not found.                */
    recordSize = rmFetchRecord(pcontext, area,
                           pRecord->recid, pRecord->pbuffer,
	                   (COUNT)pRecord->maxLength, 0 /* not continuation */);
    if( recordSize > (int)pRecord->maxLength )
    {
	pRecord->recLength = recordSize;
	returnCode = DSM_S_RECORD_BUFFER_TOO_SMALL;
    }
    else if ( recordSize > 0 )
    {
	pRecord->recLength = recordSize;
	returnCode = DSM_S_SUCCESS;
    }
    else
    {
	returnCode = (dsmStatus_t)recordSize;
    }

    if( needRgLock )
    {
	lkrgunlk(pcontext, &lockId);
    }

done:
    return returnCode;

}  /* end dbRecordGet */

/* PROGRAM: dbDeleteAreaRecords - Delete all records associated with
 *                        the area including:
 *                        Area Records
 *                        Extent Records
 *                        Storage Object Records
 *
 * RETURNS: 0 on success, -1 on failure
 */
DSMVOID
dbDeleteAreaRecords(dsmContext_t *pcontext, dsmArea_t area)
{
    int rc;

    tmstrt(pcontext);
    rc = dbExtentRecordDelete(pcontext, 1, area);
    rc = dbAreaRecordDelete(pcontext, area);
    rc = dbObjectDeleteByArea(pcontext, area);
    rc = tmend(pcontext, TMACC, NULL, 1);

    return;
}

/* PROGRAM: dbLockQueryInit - Initialize for lock table query
 *          
 * RETURNS: 
 *      0        - success
 */
dsmStatus_t
dbLockQueryInit(
        dsmContext_t *pcontext,     /* IN database context */
        int           getLock)      /* IN lock/unlock lock table */
{
    if (getLock)
    {
      pcontext->pusrctl->uc_currChain = 0;
      pcontext->pusrctl->uc_plck = NULL;

      MU_LOCK_ALL_LOCK_CHAINS();
    }
    else
    {
      MU_UNLK_ALL_LOCK_CHAINS();
    }

    return DSM_S_SUCCESS;
}

/* PROGRAM: dbLockQueryGetLock - Query lock information
 *          
 *
 * RETURNS: 
 *      positive - no more locks
 *      0        - success
 *      negative - an error has occurred
 */
dsmStatus_t
dbLockQueryGetLock(
        dsmContext_t *pcontext,     /* IN database context */
        dsmTable_t    tableNum,     /* IN table number */
        GEM_LOCKINFO *plockinfo)    /* OUT lock information */
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    struct lkctl *plkctl = pdbcontext->plkctl;
    struct usrctl *pusrctl = pcontext->pusrctl;
    struct lck *plck = pusrctl->uc_plck;
    COUNT chainNum = pusrctl->uc_currChain;
    COUNT lastChain;
    recStatus_t rc = DSM_S_ENDLOOP;

    /* find Lock control structure associated with the requested Lock */
    if (!plkctl)
    {
      MU_UNLK_ALL_LOCK_CHAINS();
      return DSM_S_FAILURE;
    }

    if (!chainNum && !plck)
    {
       chainNum = 0;
       plck = XLCK(pdbcontext, pdbcontext->plkctl->lk_qanchor[chainNum]);
    }

    lastChain = plkctl->lk_chain[LK_PURLK] + plkctl->lk_hash - 1;

    /* traverse each lock hash chain */
    while (chainNum <= lastChain)
    {
      /* traverse each member of the current lock hash chain */
      while (plck != NULL)
      {
        if (plck->lk_id.table == tableNum)
        {
          ULONG  lkType;
          TEXT   asciiType[10];   /* for ascii lock type */

          /* lock found - fill in record */
          /* _Lock-Id - Lock id number is recid */
          plockinfo->lockid = plck->lk_id.dbkey;

          /* _Lock-Name - Name */
          if (plck->lk_qusr)
          {
            strcpy(plockinfo->lockname,
                   XTEXT(pdbcontext, XUSRCTL(pdbcontext,
                         plck->lk_qusr)->uc_qusrnam));
          }
          else
          {
            *plockinfo->lockname = 0;
          }
 
          /* _Lock-Type - Type */
          if (chainNum < plkctl->lk_chain[LK_PURLK]) 
              lkType = LK_RECLK;
          else if (chainNum < plkctl->lk_chain[LK_RGLK])
              lkType = LK_PURLK;
          else
              lkType = LK_RGLK;

          switch (lkType)
          {
             case LK_RECLK:
               if (plck->lk_id.dbkey == 0)
                 stcopy(asciiType, (TEXT *) "TAB ");
               else
                 stcopy (asciiType, (TEXT *)"REC ");
               break;
             case LK_PURLK:
               stcopy (asciiType, (TEXT *)"PURG");
               break;
             case LK_RGLK:
               stcopy (asciiType, (TEXT *)"RGET");
               break;
             default:
               utitoa((LONG)lkType, asciiType);
               break;
          }
 
          strcpy(plockinfo->locktype, asciiType);

          /* _Lock-Flags - Flags */
          lkType = plck->lk_curf;
          chrcop (asciiType, ' ', 10);
          switch (lkType & LKTYPE)
          {
              case LKIS:           stnmov(asciiType, (TEXT *)"IS ", 3); break;
              case LKIX:           stnmov(asciiType, (TEXT *)"IX ", 3); break;
              case LKSHARE:        stnmov(asciiType, (TEXT *)"S  ", 3); break;
              case LKSIX:          stnmov(asciiType, (TEXT *)"SIX", 3); break;
              case LKEXCL:         stnmov(asciiType, (TEXT *)"X  ", 3); break;
          }
          if ((lkType & LKUPGRD))  asciiType[3] = 'U';
          if ((lkType & LKQUEUED)) asciiType[4] = 'Q';
          if ((lkType & LKLIMBO))  asciiType[5] = 'L';
          if ((lkType & LKPURGED)) asciiType[6] = 'P';
          if ((lkType & LKNOHOLD)) asciiType[7] = 'H';
          asciiType[8] = '\0';
          strcpy(plockinfo->lockflags, asciiType);

          rc = DSM_S_SUCCESS;
        }
 
        plck = XLCK(pdbcontext, plck->lk_qnext);

        if (rc == DSM_S_SUCCESS)
        {
          goto done;
        }
      }  /* end lock hash chain member while loop */

      /* move on to the next chain */
      chainNum++;
      if (chainNum <= lastChain)
      {
        plck = XLCK(pdbcontext, pdbcontext->plkctl->lk_qanchor[chainNum]);
      }
    } /* end lock chain while loop */

    /* if we made it here, we have walked the entire lock table so unlock it */
    MU_UNLK_ALL_LOCK_CHAINS();

    rc = DSM_S_ENDLOOP;

done:
    pusrctl->uc_currChain = chainNum;
    pusrctl->uc_plck = plck;

    return rc;
}

