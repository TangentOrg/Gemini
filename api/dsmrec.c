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
#include "dbpub.h"            /* For RECTOOBIG definition */
#include "dsmpub.h"
#include "lkpub.h"
#include "ompub.h"
#include "recpub.h"
#include "rlpub.h"
#include "rmpub.h"
#include "rmprv.h"
#include "rmmsg.h"
#include "vstpub.h"
#include "usrctl.h"
#include "uttpub.h"
#include "statpub.h"
#include "objblk.h"

#include <setjmp.h>

/* PROGRAM: dsmRecordCreate - creates a new record
 *
 * RETURNS: status code received from rmCreateRec or other procedures
 *          which this procedure invokes.
 */
dsmStatus_t
dsmRecordCreate (
	dsmContext_t	*pcontext,
	dsmRecord_t	*pRecord)
{
    ULONG       area;
    dsmStatus_t returnCode = 0;
    dsmTable_t  table;
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    DBKEY       recordId;

    TRACE("dsmRecordCreate");

    pdbcontext->inservice++;

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmRecordCreate");
        goto done;
    }

    /* Otherwise it better be correct in the record descriptor */
    table = pRecord->table;
    
   /* record creation statistics */
 
    if ( pdbcontext->ptabstat  &&
         (pdbcontext->pdbpub->isbaseshifting ==0) &&
         table >= pdbcontext->pdbpub->argtablebase &&
         (table - pdbcontext->pdbpub->argtablebase
               < pdbcontext->pdbpub->argtablesize) )
         ((pdbcontext->ptabstat)[ table - pdbcontext->pdbpub->argtablebase ]).create++;

#ifdef VST_ENABLED
    /* siphon off VST requests */
    if (RM_IS_VST(table))
    {
	/* *** TODO: STATINC (rmvmkCt); */
    	returnCode = (dsmStatus_t)rmCreateVirtual(pcontext,
                                                  table, pRecord->pbuffer,
                                                  (COUNT)pRecord->recLength);
    }
    else
#endif  /* VST_ENABLED */
    {
	/* Find the area that contains the table       */
	returnCode = omIdToArea(pcontext, DSMOBJECT_MIXTABLE, table, table, &area );
	if( returnCode )
	    goto done;

	/* Hold off interrupts till we get the record created because
	   we must complete the micro-transaction before allowing a user
	   to exit.                                                      */
	recordId = rmCreate(pcontext, area, table, pRecord->pbuffer, 
                           (COUNT)pRecord->recLength,
                           (UCOUNT)RM_LOCK_NEEDED);
	/* rmCreate returns a recid upon success or negative number
	   for failure and should never return 0 */
	if( recordId > 0 )
	{
	    pRecord->recid = (dsmRecid_t)recordId;
	}
	else if( recordId == 0 )
	{
	    returnCode = DSM_S_FAILURE;
	}
	else
	{
	    returnCode = (dsmStatus_t)recordId;
	}
    }

done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;
    return returnCode;

}  /* dsmRecordCreate  */


/* PROGRAM: dsmRecordDelete - deletes a record
 *
 * RETURNS: status from rmDeleteRec:
 *		RMOK if successful
 *	        RMNOTFND if the record is not found
 *          status from lklock if all the locks are free
 *          error status from lkrglock if can't record-get lock fails
 *	    CTRLC if re-sync'ing or lkservcon
 */
dsmStatus_t
dsmRecordDelete (
	dsmContext_t *pcontext,
	dsmRecord_t  *pRecord,
	dsmText_t    *pname) /* refname for Rec in use message */
{
    dsmStatus_t  returnCode;
    ULONG        area;
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    int          tblno;
 
    
    TRACE("dsmRecordDelete");

    pdbcontext->inservice++;

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmRecordDelete");
        goto done;
    }

    /* statistics for deletion of records */
    tblno = pRecord->table;
    if ( pdbcontext->ptabstat  &&
         (pdbcontext->pdbpub->isbaseshifting ==0 ) &&
         tblno >= pdbcontext->pdbpub->argtablebase &&
         (tblno - pdbcontext->pdbpub->argtablebase
               < pdbcontext->pdbpub->argtablesize) )
        ((pdbcontext->ptabstat)[ tblno -
                               pdbcontext->pdbpub->argtablebase]).delete++;


#ifdef VST_ENABLED
    /* siphon off VST requests */
    if (RM_IS_VST(pRecord->table))
    {
	/* TODO: *** STATINC (rmvdlCt); */
    	returnCode = rmDeleteVirtual(pcontext, pRecord->table, pRecord->recid);	
    }
    else
#endif  /* VST_ENABLED */
    {
	/* normal delete processing */
	returnCode = (dsmStatus_t)rmDelete (pcontext, (LONG)pRecord->table,
                                            pRecord->recid, pname);
    }
    if(!returnCode)
    {
        returnCode = omIdToArea(pcontext, DSMOBJECT_MIXTABLE, pRecord->table, 
                                pRecord->table, &area);
        rlTXElock (pcontext, RL_TXE_SHARE, RL_MISC_OP);
        bkRowCountUpdate(pcontext,area,pRecord->table, 0);
        rlTXEunlock (pcontext);
    }

done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;
    return returnCode;

}  /* end dsmRecordDelete */


/* PROGRAM: dsmRecordGet - retrieve a record from the database
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
dsmRecordGet(
	dsmContext_t	*pcontext,
	dsmRecord_t	*pRecord, 
	dsmMask_t	 getMode)
{
    dsmStatus_t  returnCode;
    dbcontext_t *pdbcontext = pcontext->pdbcontext;

    if(pRecord->recid <= 0 )
        return DSM_S_RMNOTFND;
            
    pdbcontext->inservice++;

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmRecordGet");
        goto done;
    }

    returnCode = dbRecordGet(pcontext, pRecord, getMode);

done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;
    return returnCode;

}  /* end dsmRecordGet */

/* PROGRAM: dsmRowCount - retrieves the row count for the specified table 
 *
 * RETURNS:
 *      0        - A-OK
 *
 */
dsmStatus_t
dsmRowCount(
        dsmContext_t    *pcontext,
        dsmTable_t       tableNumber,
        ULONG64          *prows)
{
    dsmStatus_t  returnCode;
    bmBufHandle_t  objHandle;
    objectBlock_t  *pobjectBlock;
    dbcontext_t    *pdbcontext = pcontext->pdbcontext;
    dsmArea_t       areaNumber;

    pdbcontext->inservice++;

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmRecordGet");
        goto done;
    }

#ifdef VST_ENABLED
    if (RM_IS_VST(tableNumber))
    {
      *prows = vstGetLastKey(pcontext, tableNumber);
    }
    else 
#endif  /* VST_ENABLED */
    if( SCTBL_IS_TEMP_OBJECT(tableNumber))
    {
      omTempObject_t *ptemp;

      ptemp = omFetchTempPtr(pcontext,DSMOBJECT_MIXTABLE,
			     tableNumber,tableNumber);
      *prows = ptemp->numRows;
    }
    else
    {
      returnCode = omIdToArea(pcontext, DSMOBJECT_MIXTABLE,
                            tableNumber, tableNumber, &areaNumber);
      if(returnCode)
          goto done;

      objHandle = bmLocateBlock(pcontext, areaNumber,
                         BLK2DBKEY(BKGET_AREARECBITS(areaNumber)), TOREAD);
      pobjectBlock = (objectBlock_t *)bmGetBlockPointer(pcontext,objHandle);
    
      *prows = pobjectBlock->totalRows;
      bmReleaseBuffer(pcontext,objHandle);
    }

done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;
    return returnCode;
}

/* PROGRAM: dsmRecordUpdate - Update a record 
 *          
 *
 * RETURNS: 
 *      negative - some error like RQSTQUED, RQSTREJ, CTRLC, LKTBFULL, etc.
 *      0        - A-OK
 */
dsmStatus_t 
dsmRecordUpdate(dsmContext_t *pcontext,
                dsmRecord_t *pRecord,
		dsmText_t *pName)
{
    int       returnCode;
    lockId_t  lockId;
    ULONG     area;
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    int       tblno;

    pdbcontext->inservice++;

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmRecordUpdate");
        goto done;
    }


    if( pdbcontext->usertype & SELFSERVE )
    {
	if (pdbcontext->resyncing || lkservcon(pcontext))
        {
	    returnCode = DSM_S_CTRLC;             /* Self-service only */
            goto done;
        }
    }

    /* statistics for record update */
    tblno =  pRecord->table;
 
    if ( pdbcontext->ptabstat  &&
         (pdbcontext->pdbpub->isbaseshifting ==0 ) &&
         tblno >= pdbcontext->pdbpub->argtablebase &&
         (tblno - pdbcontext->pdbpub->argtablebase
               < pdbcontext->pdbpub->argtablesize) )
        ((pdbcontext->ptabstat)[ tblno -
                               pdbcontext->pdbpub->argtablebase ]).update++;

#ifdef VST_ENABLED
    /* siphon off VST requests */
    if (RM_IS_VST(pRecord->table))
    {
	/* *** TODO: STATINC (rmvrpCt); */
    	returnCode = ( (dsmStatus_t)rmUpdateVirtual(pcontext, pRecord->table, 
			         	 (DBKEY)pRecord->recid, 
				          pRecord->pbuffer, 
				          (COUNT)pRecord->recLength));
        goto done;
    }
#endif  /* VST_ENABLED */
  
    /* Find the area that contains the table       */
    returnCode = omIdToArea( pcontext, DSMOBJECT_MIXTABLE, 
			    pRecord->table, pRecord->table, &area);
    if( returnCode )
    {
	goto done;
    }
    lockId.table = pRecord->table;
    lockId.dbkey = pRecord->recid;

    /* Lock the record                         */

    returnCode = lkrclk(pcontext, &lockId, pName);    /* lock the record */
    if (returnCode)
    {
        goto done;
    }

    returnCode = (dsmStatus_t)rmUpdateRecord(pcontext, pRecord->table, area,
                                        lockId.dbkey, 
			                pRecord->pbuffer,
                                        (COUNT)pRecord->recLength);

done:

    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;
    return(returnCode);

}  /* end dsmRecordUpdate */


/* PROGRAM: dsmLockQueryInit - Query lock information
 *          
 *
 * RETURNS: 
 *      positive - no more locks
 *      0        - success
 *      negative - an error has occurred
 */
dsmStatus_t 
dsmLockQueryInit(dsmContext_t *pcontext,
                 int getLock)
{
    int          returnCode;
    dbcontext_t *pdbcontext = pcontext->pdbcontext;

    pdbcontext->inservice++;

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmRecordUpdate");
        goto done;
    }

    returnCode = (dsmStatus_t)dbLockQueryInit(pcontext, getLock);

done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;
    return(returnCode);
}  /* end dsmLockQueryInit */


/* PROGRAM: dsmLockQueryGetLock - Query lock information
 *          
 *
 * RETURNS: 
 *      positive - no more locks
 *      0        - success
 *      negative - an error has occurred
 */
dsmStatus_t 
dsmLockQueryGetLock(dsmContext_t *pcontext,
               dsmTable_t tableNum, 
               GEM_LOCKINFO *plockinfo)
{
    int          returnCode;
    dbcontext_t *pdbcontext = pcontext->pdbcontext;

    pdbcontext->inservice++;

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmRecordUpdate");
        goto done;
    }

    returnCode = (dsmStatus_t)dbLockQueryGetLock(pcontext, tableNum, plockinfo);

done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;
    return(returnCode);
}  /* end dsmLockQueryGetLock */

