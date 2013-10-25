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
#include "dsmpub.h"
#include "cxpub.h"
#include "cxmsg.h"
#include "dbcontext.h"
#include "dbpub.h"
#include "lkpub.h"
#include "ompub.h"
#include "rmpub.h"
#include "utspub.h"
#include "scasa.h"  /* for VST def's */
#include "usrctl.h"
#include "umlkwait.h"        /* for LW_RECLK_MSG */

LOCALF DSMVOID dbCursorStepBack(dsmContext_t *pcontext, dsmMask_t findMode,
                              dsmCursid_t *pcursid);
/* PROGRAM: dbLockRow - Locks a row of the database.
 *          
 *
 * RETURNS: 
 */
int 
dbLockRow(
	dsmContext_t	*pcontext,
	COUNT tableNumber, /* Table the row is in     */
	DBKEY recid,          /* Record id of row.       */
	int   lockMode,       /* Share, exclusive etc.   */
	TEXT *pTableName,      /* table name for displaying wait message */
	int waitFlag _UNUSED_) /* 1 - if should wait to be granted lock
                                    in the event of a lock conflict    */
{
    lockId_t  lockId;
    int       returnCode = 0;

#ifdef VST_ENABLED
    if ( ((COUNT)tableNumber <= SCTBL_VST_FIRST) &&
         ((COUNT)tableNumber >= SCTBL_VST_LAST) )
    {
        return returnCode;  /* Ignore VST entries */
    }
#endif  /* VST ENABLED */

    lockId.table = (LONG)tableNumber;
    lockId.dbkey = recid;

    pcontext->pdbcontext->inservice++; /* keep signal handler out */
    returnCode = lklock(pcontext, &lockId, lockMode, pTableName);
    pcontext->pdbcontext->inservice--;
    
    return (returnCode);
}

/* PROGRAM: dbUnlockRow - Unlocks a row of the database.
 *          
 *
 * RETURNS: 
 */
int 
dbUnlockRow(
	dsmContext_t	*pcontext,
	COUNT tableNumber,    /* Table the row is in     */
	DBKEY recid       )   /* Record id of row.       */
{
    lockId_t  lockId;
    int       returnCode = 0;

#ifdef VST_ENABLED
    if ( ((COUNT)tableNumber <= SCTBL_VST_FIRST) &&
         ((COUNT)tableNumber >= SCTBL_VST_LAST) )
    {
        return returnCode;  /* Ignore VST entries */
    }
#endif  /* VST ENABLED */


    lockId.table = (LONG)tableNumber;
    lockId.dbkey = recid;

    pcontext->pdbcontext->inservice++; /* keep signal handler out */
    lkrels(pcontext, &lockId);
    pcontext->pdbcontext->inservice--;
    
    return (returnCode);
}

/* PROGRAM: dbCursorCreate - allocate a cursor for requested table and index
 *
 * dbCursorCreate() allocates a cursor for the requested table and index
 * and returns the cursid in pcursid.  If pcopy is not NULL, then it is
 * expected to identify an existing cursor which is copied into the newly
 * created cursor (ignoring table and index) duplicateing all the
 * positioning information associatd with the existing cursor.
 *
 * A cursor is used to maintain a position within an index or table between
 * dbCursorFind calls.  A cursor does not identify the range of values or
 * bracket associated with a find operation.
 *
 * NOTE: the initial implementation only supports index cursors
 * (position within an index) and requires a valid index value be provided.
 * Future implementations are expected to support passing zero as the
 * index number which would result in a table cursor (position within
 * a table object).
 *          
 * RETURNS: DSM_S_SUCCESS if success and cursorId will be set
 *          DSM_S_NOCURSOR if the cursorid to copy was not valid
 *          DSM_S_FAILURE if a new cursor cannot be created
 */
dsmStatus_t
dbCursorCreate(
	dsmContext_t *pcontext,	/* IN/OUT database context */
	dsmTable_t    table,	/* IN table associated with cursor */
	dsmIndex_t    index,	/* IN index associated with cursor */
	dsmCursid_t  *pcursid,  /* OUT created cursor id */
	dsmCursid_t  *pcopy _UNUSED_) /* IN cursor id to copy */
{
    dsmStatus_t	 returnCode = DSM_S_FAILURE;
    CXCURSOR    *pCursor;
    omDescriptor_t omDesc;

    *pcursid = DSMCURSOR_INVALID;

    pCursor = cxGetCursor(pcontext, pcursid, index);

    
    if ( pCursor == NULL )
    {
        returnCode = DSM_S_FAILURE;
        goto done;
    }

    returnCode = omFetch(pcontext, DSMOBJECT_MIXINDEX, index, table, &omDesc);

    if ( returnCode )
        goto failedReturn;

    pCursor->area = omDesc.area;
    pCursor->root = omDesc.objectRoot;
    pCursor->unique_index = (dsmBoolean_t)omDesc.objectAttributes & 
                                          DSMINDEX_UNIQUE;
    pCursor->word_index   = (dsmBoolean_t)omDesc.objectAttributes &
                                          DSMINDEX_WORD;
    pCursor->multi_index  = (dsmBoolean_t)omDesc.objectAttributes &
                                          DSMINDEX_MULTI_OBJECT;

    if( !pCursor->multi_index )
    {
        returnCode = omIdToArea(pcontext,DSMOBJECT_MIXTABLE,
                            table, table, &pCursor->tableArea);
        if( returnCode )
            goto failedReturn;
        pCursor->table = table;
    }
    else
    {
        pCursor->table = 0;
        pCursor->tableArea = 0;
    }
    
    returnCode = DSM_S_SUCCESS;
    goto done;
    
failedReturn:

    cxDeactivateCursor(pcontext, pcursid);
    
done:

    return(returnCode);

}  /* end dbCursorCreate */


/* PROGRAM: dbCursorFind - find a recid using a cursor
 *
 * Positions a cursor based on the current position provided by pcursid.
 * The bracket indicated by the two index keys pkeybase and pkeylimit
 * and the mode bits in findMode modify the positioning semantics.
 * dbCursorFind() returns a recid into precid.  If the record is to be
 * locked, the lock information is provided in lockMode.  The key values
 * must be in the new index key format (V7+).  Subsequent calls to
 * dbCursorFind() for a traditional cursor must provide the same
 * bracket information on each call.  However, it is perfectly
 * acceptable for the caller to change the bracket at any time.
 *          
 * RETURNS: DSM_S_SUCCESS if success and precid will be set
 */
dsmStatus_t
dbCursorFind(
	dsmContext_t *pcontext,		/* IN database context */
	dsmCursid_t  *pcursid,		/* IN/OUT cursor */
	dsmKey_t     *pKeyBase,		/* IN base key of bracket for find */
	dsmKey_t     *pKeyLimit,	/* IN limit key of bracket for find */
        dsmMask_t     findType,         /* IN DSMUNIQUE, DSMPARTIAL ...     */
	dsmMask_t     findMode,		/* IN DSMFINDFIRST DSMFINDNEXT ... */
	dsmMask_t     lockMode,		/* IN lock mode bit mask */
        dsmObject_t  *pObjectNumber,    /* OUT the table/class the returned
                                         * recid is in                      */
	dsmRecid_t   *precid,		/* OUT recid found */
        dsmKey_t     *pKey)             /* OUT [optional] key found */
{
    dbcontext_t	*pdbcontext = pcontext->pdbcontext;
    dsmStatus_t	 returnCode = DSM_S_FAILURE;
    CXCURSOR    *pCursor;
    dsmKey_t    *pbase,*plimit;

    pCursor = cxGetCursor(pcontext, pcursid, 0 );

  tryAgain:
    
    if( findMode == DSMFINDLAST )
    {
        pbase = pKeyLimit;
        plimit = pKeyBase;
    }
    else
    {
        pbase = pKeyBase;
        plimit = pKeyLimit;
    }
    pbase->area = pCursor->area;
    pbase->root = pCursor->root;
    pbase->unique_index = pCursor->unique_index;
    pbase->word_index = pCursor->word_index;
    
    if( findType == DSMGETGE )
    {
        returnCode = cxFindGE(pcontext, pcursid, pCursor->table,
                              precid, pbase, plimit, lockMode);
    }
    else if( findMode == DSMFINDFIRST || findMode == DSMFINDLAST )
    {
        returnCode = cxFind(pcontext, pbase, plimit, pcursid,
                            pCursor->table, precid, findType, findMode,
                            lockMode);
    }
    else if (findMode == DSMFINDNEXT || findMode == DSMFINDPREV)
    {
        returnCode = cxNext(pcontext, pcursid, pCursor->table, precid,
                            pbase, plimit, findType, findMode,
                            lockMode);
    }
    else
    {
         MSGD_CALLBACK(pcontext, 
                      (TEXT *)"%GInvalid findMode %l",(LONG)findMode);
    }

    
    /* if error - perform cursor fixup */
    if ( pdbcontext->accessEnv & (DSM_JAVA_ENGINE + DSM_SQL_ENGINE) )
    {
        if ( (returnCode == DSM_S_RQSTQUED) ||
             (returnCode == DSM_S_RQSTREJ)  ||
             (returnCode == DSM_S_LKTBFULL) )
            dbCursorStepBack(pcontext, findMode, pcursid);
        if( returnCode == DSM_S_RQSTQUED)
        {
            if( findMode == DSMFINDFIRST )
                findMode = DSMFINDNEXT;
            else if(findMode == DSMFINDLAST)
                findMode = DSMFINDPREV;
 
            if (pcontext->pusrctl->uc_wait == TSKWAIT)
                if (lktskchain(pcontext)) /* chain the user to TSKWAIT chain */
		    goto tryAgain;	  /* the task to wait on is gone */

            returnCode = lkwait(pcontext, LW_RECLK_MSG, NULL);
            if (returnCode == 0)	/* record is free, try again to lock it */
            {
                goto tryAgain;
            }
        }
    }


    if( pObjectNumber )
        *pObjectNumber = (dsmObject_t)pCursor->table;
    
    if (pKey)
    {
        pKey->keyLen = xct(&pCursor->pkey[KS_OFFSET]);
        bufcop(pKey->keystr,pCursor->pkey,pKey->keyLen + FULLKEYHDRSZ);
    }

    return(returnCode);

}  /* end dbCursorFind */


/* PROGRAM: dbCursorGet - get the cursor associated with a cursorid
 *
 * dbCursorGet() returns a pointer to a cursor structure in pcursor
 * given a cursor id in cursid.  The cursor structure is part of the
 * public interface to DSMAPI.
 *
 * RETURNS: DSM_S_SUCCESS if success and pcursor will be filled in
 *          DSM_S_NOCURSOR when pcursor is NULL or invalid
 *          DSM_S_FAILURE on failure
 */
dsmStatus_t
dbCursorGet(
	dsmContext_t *pcontext,	/* IN database context */
	dsmCursid_t   cursid,   /* IN cursor id to find */
	struct cxcursor   **ppCursor)/* OUT pointer to cursor structure */
{
    dsmStatus_t	 returnCode = DSM_S_FAILURE;

    *ppCursor = cxGetCursor(pcontext, &cursid, 0);

    if( *ppCursor != NULL )
        returnCode = DSM_S_SUCCESS;

    return(returnCode);

}  /* end dbCursorGet */


/* PROGRAM: dbCursorDelete - deallocate the cursor indicated by pcursid
 *
 * dbCursorDelete() deallocates the cursor indicated by cursorid.  
 * If deleteAll is true, then all cursors associated with the provided
 * context are deallocated.
 *
 * NOTE: much like the recid associated with a record, when a cursor is
 * deleted the resources associated with the cursor are returned to a
 * pool of cursors managed by the database storage manager.  This is why
 * the cursor id provided is cleared (so the caller "forgets" the cursor
 * id and does not attempt to reuse it).
 *
 * RETURNS: DSM_S_SUCCESS if success and pcursor will be filled in
 *          DSM_S_NOCURSOR when pcursor is NULL or invalid
 *          DSM_S_FAILURE on failure
 */
dsmStatus_t
dbCursorDelete(
	dsmContext_t  *pcontext,	/* IN database context */
	dsmCursid_t   *pcursid,		/* IN/OUT pointer to cursor to delete */
	dsmBoolean_t  deleteAll)	/* IN delete all cursors indicator */
{
    dsmStatus_t  returnCode;

    if ( deleteAll )
    {
        cxDeaCursors(pcontext);
    }
    else
    {
        cxDeactivateCursor(pcontext, pcursid);
    }

    returnCode = DSM_S_SUCCESS;
    
    return(returnCode);

}  /* end dsmCursorDelete */

/* PROGRAM: dbCursorStepBack - position the cursor to the previous record
 *      in the respective direction after a failed lock request.
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
dbCursorStepBack(
        dsmContext_t *pcontext,
        dsmMask_t     direction,
        dsmCursid_t  *pcursid)      /* the cursor to be advanced */
{

    /* What about DSMFINDGE ? */

    if ( (direction == DSMFINDNEXT) ||
         (direction == DSMFINDFIRST) )
        cxBackUp(pcontext, *pcursid);
    else
        cxForward(pcontext, *pcursid);

}  /* end dbCursorStepBack */

